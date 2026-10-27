"""Nyx on mynx: bright cyan-blue hair painted over the model's own curls,
a dark navy-teal bodysuit with glowing cyan seams over the leather and
the bare skin, her face bare, dark glasses with a cyan glint, and a
crescent sigil where the belt skull was. Team versions keep the team's suit paint and light the
seams in its colour.

The seams, the belt skull, the glasses, the neck, the gloves and the
hair are worked out on the player's machine from the player's own model
and its paint."""
import colorsys, math, random
from skins import Sheet, Look, LabelMap, painted
from tremorart import Fbm, clamp, smooth, mix


def extent(ts, axis):
    vals = [t.pos[axis] for t in ts]
    return min(vals), max(vals)


def covered(geo):
    return [t for row in geo.grid for t in row if t and t.covered]


def rims(geo, keep, n=128):
    """A LabelMap of the texels keep(texel) picks, "a", and the rest, "z";
    its edge() is how far in from a piece's rim a texel is."""
    return LabelMap.painted(lambda t, u, v: "a" if keep(t) else "z", n, cap=12)(geo)


# ---------------------------------------------------------------- the suit sheet (mynx_shiny)

SUIT = (0.07, 0.11, 0.15)
GLOW = {"": (0.2, 0.9, 1.0), "_red": (1.0, 0.4, 0.25), "_blue": (0.4, 0.8, 1.0)}
SEAM = 7.0                              # how far in from a leg piece's rim its seam runs, in 128ths


def suit_grade(u, v):
    return [0.24, 0.32, 0.46, 1]


def team_suit_grade(u, v):
    return [0.36, 0.34, 0.38, 1]


def crescent(u, v, c, r, wide=1.0):
    """< 0 inside a crescent moon of radius r round c, on a sheet wide
    times as wide as it is tall."""
    du, dv = (u - c[0]) * wide, v - c[1]
    outer = math.hypot(du, dv) - r
    inner = math.hypot(du - r * 0.45, dv - r * 0.2) - r * 0.85
    return max(outer, -inner)


_suits = {}


def suit_places(geo, n=128):
    """(seam, skull, sigil, wide, glasses) on the suit sheet: seam(t) the
    distance in degrees to a seam down the legs; skull(u, v) whether a
    texel is on the belt skull, found as the biggest patch of pale bone
    paint on the torso, with the sigil's place at its middle; glasses(t)
    the height of a glasses texel above their middle, or None."""
    if id(geo) in _suits:
        return _suits[id(geo)]

    def bone(x, y):
        u, v = (x + 0.5) / n, (y + 0.5) / n
        h, s, val = colorsys.rgb_to_hsv(*(geo.colour(u, v) or (0, 0, 0)))
        return geo.at(u, v).surf == "u_torso" and s < 0.55 and val > 0.35
    hit = [[bone(x, y) for x in range(n)] for y in range(n)]
    # the biggest patch of bone, joining texels up to two apart
    seen, best = set(), []
    for y in range(n):
        for x in range(n):
            if hit[y][x] and (x, y) not in seen:
                patch, todo = [], [(x, y)]
                seen.add((x, y))
                while todo:
                    i, j = todo.pop()
                    patch.append((i, j))
                    for a in range(-2, 3):
                        for b in range(-2, 3):
                            q = (i + a, j + b)
                            if 0 <= q[0] < n and 0 <= q[1] < n and hit[q[1]][q[0]] and q not in seen:
                                seen.add(q)
                                todo.append(q)
                if len(patch) > len(best):
                    best = patch
    xs, ys = [p[0] for p in best] or [-9], [p[1] for p in best] or [-9]       # none: no skull
    box = ((min(xs) - 1.5) / n, (min(ys) - 1.5) / n, (max(xs) + 2.5) / n, (max(ys) + 2.5) / n)
    sigil = ((box[0] + box[2]) / 2, (box[1] + box[3]) / 2)

    def skull(u, v):
        return box[0] < u < box[2] and box[1] < v < box[3]
    lens = [t for t in covered(geo) if t.surf == "h_glasses"]
    gz = sum(extent(lens, 2)) / 2

    m = rims(geo, lambda t: t.covered and t.part == "lower")

    def seam(u, v):
        if m.label(u, v) != "a":
            return 1e9
        return abs(m.edge(u, v) - SEAM)

    def glasses(t):
        return t.z - gz if t.surf == "h_glasses" else None
    wide = geo.image[0] / geo.image[1] if geo.image else 1.0
    _suits[id(geo)] = r = (seam, skull, sigil, wide, glasses)
    return r


def suit_detail_fn(wash):
    n = Fbm(811, 4, 6)

    @painted
    def make(geo):
        seam, skull, sigil, wide, glasses = suit_places(geo)

        def f(u, v):
            t = geo.at(u, v)
            c, a = list(SUIT), wash
            k = 0.8 + 0.4 * n.at(u * 3, v * 3)
            c = [x * k for x in c]
            if seam(u, v) < 0.8:
                c, a = [0.04, 0.03, 0.06], 0.9
            if skull(u, v):
                c, a = [0.07, 0.05, 0.1], 1.0
                if crescent(u, v, sigil, 0.07, wide) < 0.004:
                    c = [0.55, 0.6, 0.7]
            if glasses(t) is not None:
                c, a = [0.04, 0.06, 0.1], 0.95
            return c + [a]
        return f
    return make


def suit_glow_fn(colour):
    @painted
    def make(geo):
        seam, skull, sigil, wide, glasses = suit_places(geo)

        def f(u, v):
            t = geo.at(u, v)
            k = 1 - seam(u, v) / 0.5
            if skull(u, v):
                k = max(k, 1 - max(0, crescent(u, v, sigil, 0.07, wide)) / 0.006)
            g = glasses(t)
            if g is not None:
                k = max(k, 0.35 * (1 - abs(g) / 0.42))      # a glint across the glasses
            k = clamp(k)
            return [colour[0] * k, colour[1] * k, colour[2] * k, 1]
        return f
    return make


# ---------------------------------------------------------------- the arms sheet (mynx.tga): chest, gloves, back

SKIN = (0.86, 0.80, 0.84)
_arms = {}


def arms_places(geo):
    """what(t) on the arms sheet: "neck" (the top quarter of the chest
    piece, the biggest one), "collar" (a thin band under it), "gloves"
    (the pieces that stay in the bottom third: the hands), or "". And
    seam(t, u, v): the distance in model units to a seam down the middle
    of the chest's front, or just inside the rim of the arms' pieces."""
    if id(geo) in _arms:
        return _arms[id(geo)]
    ts = covered(geo)
    z0, z1 = extent(ts, 2)
    pieces = {}
    for t in ts:
        pieces.setdefault(t.island, []).append(t)
    chest = max(pieces, key=lambda k: len(pieces[k]))
    c0, c1 = extent(pieces[chest], 2)
    neck = c0 + 0.74 * (c1 - c0)
    hands = {k for k, p in pieces.items() if extent(p, 2)[1] < z0 + 0.33 * (z1 - z0)}

    def what(t):
        if t.island == chest:
            if t.z > neck:
                return "neck"
            if t.z > neck - 0.1:
                return "collar"
            return ""
        return "gloves" if t.island in hands else ""

    # down the middle of the chest's front
    cp = pieces[chest]
    fx, fy = sum(t.nrm[0] for t in cp), sum(t.nrm[1] for t in cp)
    k = math.hypot(fx, fy) or 1
    fx, fy = fx / k, fy / k
    cx, cy = sum(t.x for t in cp) / len(cp), sum(t.y for t in cp) / len(cp)
    # just inside the rim of the arms' pieces
    m = rims(geo, lambda t: t.covered and t.island != chest and t.island not in hands)

    def seam(t, u, v):
        if t.island == chest:
            if t.z > neck or t.nrm[0] * fx + t.nrm[1] * fy < 0.2:
                return 1e9
            return abs((t.x - cx) * -fy + (t.y - cy) * fx)
        if m.label(u, v) != "a":
            return 1e9
        return abs(m.edge(u, v) - 4) * 0.06
    _arms[id(geo)] = r = (what, seam)
    return r


def arms_detail_fn(fabric):
    n = Fbm(823, 4, 5)

    @painted
    def make(geo):
        what, seam = arms_places(geo)

        def f(u, v):
            t = geo.at(u, v)
            w = what(t)
            if w == "neck":
                return list(SKIN) + [0.45]
            k = 0.75 + 0.5 * n.at(u * 2.5, v * 2.5)
            c, a = [x * k for x in fabric], 0.93
            if w == "gloves":
                c, a = [0.07, 0.05, 0.1], 0.9
            if w == "collar":
                c, a = [0.03, 0.02, 0.05], 1.0
            if seam(t, u, v) < 0.11:
                c, a = [0.04, 0.03, 0.06], 1.0
            return c + [a]
        return f
    return make


def arms_glow_fn(colour):
    @painted
    def make(geo):
        what, seam = arms_places(geo)

        def f(u, v):
            k = clamp(1 - seam(geo.at(u, v), u, v) / 0.07)
            return [colour[0] * k, colour[1] * k, colour[2] * k, 1]
        return f
    return make


# ---------------------------------------------------------------- the head sheet (mynx_h): the face and the hair

HAIR_DARK = (0.03, 0.12, 0.3)
HAIR = (0.12, 0.45, 0.85)
HAIR_LIGHT = (0.5, 0.85, 1.0)


def hair_places(geo):
    """(hair(t), inward(t)): the hair is the pieces that hang down a good
    part of the head's height (the face's pieces are short); inward is
    how much a texel faces in towards the middle of the head."""
    ts = covered(geo)
    z0, z1 = extent(ts, 2)
    mid = [sum(extent(ts, a)) / 2 for a in range(3)]
    span = {}
    for t in ts:
        lo, hi = span.get(t.island, (1e9, -1e9))
        span[t.island] = (min(lo, t.z), max(hi, t.z))
    hair = {k for k, (lo, hi) in span.items() if hi - lo > 0.4 * (z1 - z0)}

    def inward(t):
        d = [t.pos[i] - mid[i] for i in range(3)]
        m = math.sqrt(sum(q * q for q in d)) or 1
        return -sum(t.nrm[i] * d[i] for i in range(3)) / m
    return (lambda t: t.island in hair), inward


@painted
def head_grade(geo):
    hair, _ = hair_places(geo)
    return lambda u, v: [0.5, 0.5, 0.5, 1] if hair(geo.at(u, v)) else [0.48, 0.47, 0.52, 1]       # paler skin


class Curls:
    """Loose curls: arcs of rings jittered over a few lattices of cells."""

    def __init__(self, seed, scales=((26, 13), (17, 9), (37, 19))):
        rng = random.Random(seed)
        self.layers = []
        for nx, ny in scales:
            cells = {(i, j): (rng.random(), rng.random(), 0.26 + 0.14 * rng.random(), rng.random() * 6.283,
                              3.0 + 1.6 * rng.random())
                     for i in range(nx) for j in range(ny)}
            self.layers.append((nx, ny, cells))

    def at(self, u, v):
        """(strand 0..1, highlight 0..1)"""
        best, light = 0.0, 0.0
        for nx, ny, cells in self.layers:
            x, y = u * nx, v * ny
            for i in range(int(x) - 1, int(x) + 2):
                for j in range(int(y) - 1, int(y) + 2):
                    cx, cy, r, a0, span = cells[(i % nx, j % ny)]
                    dx, dy = x - i - cx, y - j - cy
                    ring = abs(math.hypot(dx, dy) - r)
                    a = (math.atan2(dy, dx) - a0) % 6.283
                    if ring < 0.1 and a < span:
                        k = (1 - ring / 0.1) * min(1, a / 0.6, (span - a) / 0.6)
                        if k > best:
                            best, light = k, clamp(0.5 - 0.5 * math.sin(math.atan2(dy, dx) + 0.8))
        return best, light


def head_detail_fn():
    n, warp = Fbm(831, 4, 4), Fbm(837, 3, 3)
    curls = Curls(841)

    @painted
    def make(geo):
        hair, inward = hair_places(geo)

        def f(u, v):
            t = geo.at(u, v)
            if not hair(t):
                return [0, 0, 0, 0]
            wu = u + 0.02 * (warp.at(u * 2, v * 2) - 0.5)
            wv = v + 0.04 * (warp.at(u * 2 + 5, v * 2) - 0.5)
            strand, light = curls.at(wu, wv)
            depth = n.at(u * 3, v * 3)
            c = mix(HAIR_DARK, HAIR, clamp(depth * 1.4 - 0.15))
            c = mix(c, HAIR, strand * 0.8)
            c = mix(c, HAIR_LIGHT, strand * light * (0.4 + 0.6 * depth))
            # dark where the hair faces in towards the head: the parting under the crown
            c = [x * (0.35 + 0.65 * smooth(0.5, -0.3, inward(t))) for x in c]
            return list(c) + [1]
        return f
    return make


def look():
    sheets = {
        "suit": Sheet({"": "mynx_shiny", "_red": "red_s", "_blue": "blue_s"},
                      grade={"": suit_grade, "_red": team_suit_grade},
                      detail={"": suit_detail_fn(0.8), "_red": suit_detail_fn(0.3)},
                      glow={v: suit_glow_fn(GLOW[v]) for v in GLOW}, size=512),
        "arms": Sheet({"": "mynx", "_red": "mynx", "_blue": "mynx"},
                      detail={"": arms_detail_fn(SUIT), "_red": arms_detail_fn((0.36, 0.06, 0.06)),
                              "_blue": arms_detail_fn((0.06, 0.1, 0.4))},
                      glow={v: arms_glow_fn(GLOW[v]) for v in GLOW}, size=256),
        "head": Sheet({"": "mynx_h", "_red": "red_h", "_blue": "red_h"},
                      grade=head_grade, detail=head_detail_fn(), size=512),
    }
    sheets["suit"].layers["grade"]["_blue"] = team_suit_grade
    sheets["suit"].layers["detail"]["_blue"] = sheets["suit"].layers["detail"]["_red"]
    return Look("mynx", "nyx", sheets,
                {"lower": {"l_legs": "suit"},
                 "upper": {"u_torso": "suit", "u_arms": "arms"},
                 "head": {"h_head": "head", "h_glasses": "suit"}},
                {"lower": ["tag_torso"], "upper": ["tag_head", "tag_torso", "tag_weapon"], "head": ["tag_head"]})
