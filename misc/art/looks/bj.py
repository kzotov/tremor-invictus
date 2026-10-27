"""B.J. on bitterman: the green hide graded to a weathered human skin, an
olive vest over the chest, olive fatigues over the legs, a brown leather
belt, and a blond buzz cut painted over the tentacles. Team versions keep
the team's paint and the same bare head.

Where the vest, the belt, the fatigues and the hair are is worked out on
the player's machine from where each texel sits on the model."""
from skins import Sheet, Look, seg_dist, painted
from tremorart import Fbm, Noise, clamp, smooth, mix

# ---------------------------------------------------------------- the body sheet (bitterman.tga)

FLESH_GRADE = (0.64, 0.5, 0.64)         # olive hide to skin
VEST_COLOUR = (0.17, 0.19, 0.11)
FATIGUES = (0.2, 0.21, 0.13)
LEATHER = (0.3, 0.17, 0.09)

_parts = {}


def body_parts(geo):
    """(what, ...) for the body sheet: what(t) is "vest", "belt", "legs",
    "gear" or "" (bare skin). The vest is the torso pieces that reach up
    to the shoulders (not the arms) above the belt, the bottom quarter of
    the torso; the fatigues are the leg pieces that reach the hips; the
    gear is the boots and the straps. Also the vest's middle line and its
    height, for the zip and the quilting."""
    if id(geo) in _parts:
        return _parts[id(geo)]
    tops, ups, lows = {}, [], {}
    for row in geo.grid:
        for t in row:
            if not t.covered:
                continue
            if t.surf == "u_body":
                tops[t.island] = max(tops.get(t.island, -1e9), t.z)
                ups.append(t)
            elif t.part == "lower":
                lows[t.island] = max(lows.get(t.island, -1e9), t.z)
    z0, z1 = min(t.z for t in ups), max(t.z for t in ups)
    trunk = {k for k, z in tops.items() if z > z0 + 0.7 * (z1 - z0)}
    belt = z0 + 0.25 * (z1 - z0)
    front = [t.y for t in ups if t.island in trunk and t.nrm[0] > 0.5]
    mid = (min(front) + max(front)) / 2

    def what(t):
        if t.part == "lower":
            return "legs" if lows.get(t.island, 0) > 0 else "gear"
        if t.surf != "u_body":
            return "gear"
        if t.island in trunk:
            return "vest" if t.z > belt else "belt"
        return ""
    _parts[id(geo)] = r = (what, mid)
    return r


@painted
def body_grade(geo):
    what, _ = body_parts(geo)

    def f(u, v):
        if what(geo.at(u, v)) in ("gear", "belt"):
            return [0.5, 0.5, 0.5, 1]
        return list(FLESH_GRADE) + [1]
    return f


def body_detail_fn():
    n, camo = Fbm(1111, 4, 6), Fbm(1117, 3, 3)

    @painted
    def make(geo):
        what, mid = body_parts(geo)

        def f(u, v):
            t = geo.at(u, v)
            w = what(t)
            k = 0.8 + 0.4 * n.at(u * 5, v * 5)
            if w == "vest":
                c = [x * k for x in VEST_COLOUR]
                # quilted rows and a zip down the middle of the front
                if abs((t.z / 2.5) % 1 - 0.5) < 0.05 or (t.nrm[0] > 0.5 and abs(t.y - mid) < 0.16):
                    c = [x * 0.5 for x in c]
                return c + [0.88]
            if w == "legs":
                c = [x * k for x in FATIGUES]
                if camo.at(u * 3, v * 3) > 0.56:
                    c = [x * 0.6 for x in c]
                return c + [0.82]
            if w == "belt":
                return [x * k for x in LEATHER] + [0.7]
            return [0, 0, 0, 0]
        return f
    return make


# ---------------------------------------------------------------- the head (h_bitterman.tga)

HEAD_GRADE = (0.68, 0.5, 0.72)
HAIR = (0.82, 0.68, 0.38)


def head_grade(u, v):
    return list(HEAD_GRADE) + [1]


def head_detail_fn():
    stubble, n = Noise(1123, 256, 256), Fbm(1129, 3, 6)

    @painted
    def make(geo):
        ts = [t for row in geo.grid for t in row if t and t.covered]
        z0, z1 = min(t.z for t in ts), max(t.z for t in ts)
        x0, x1 = min(t.x for t in ts), max(t.x for t in ts)
        front = [t.y for t in ts if t.nrm[0] > 0.95]
        mid = sum(front) / len(front)                   # the face's middle line
        brow, cheek = z0 + 0.8 * (z1 - z0), x1 - 0.4 * (x1 - x0)
        jaw = z0 + 0.28 * (z1 - z0)
        # a scar down the left cheek, (across, height) from the middle line
        h = z1 - z0
        scar = ((0.7, z0 + 0.54 * h), (1.3, z0 + 0.43 * h))

        def f(u, v):
            t = geo.at(u, v)
            d = max(t.z - brow, cheek - t.x)            # > 0 off the face, in model units
            if d > -0.1:
                # buzz cut: fine light flecks over a darker scalp, fading in at the hairline
                fleck = stubble.at(u * 220, v * 220)
                c = mix(tuple(x * 0.45 for x in HAIR), HAIR, smooth(0.4, 0.8, fleck))
                k = 0.85 + 0.3 * n.at(u * 3, v * 3)
                return [x * k for x in c] + [clamp((d + 0.1) / 0.3) * 0.95]
            if t.z < jaw:
                # stubble on the jaw
                return [0.32, 0.25, 0.16, 0.35 * smooth(0.5, 0.7, stubble.at(u * 260, v * 260))]
            sd, _ = seg_dist(t.y - mid, t.z, *scar)
            if sd < 0.07 and t.nrm[0] > 0.5:
                return [0.62, 0.38, 0.36, 0.6]
            return [0, 0, 0, 0]
        return f
    return make


def look():
    # the team heads are hooded: every version wears the bare one
    head = Sheet({"": "h_bitterman", "_red": "h_bitterman", "_blue": "h_bitterman"},
                 grade=head_grade, detail=head_detail_fn(), size=256)
    return Look("bitterman", "bj", {
        "body": Sheet({"": "bitterman", "_red": "red", "_blue": "blue"},
                      grade={"": body_grade, "_red": None, "_blue": None},
                      detail={"": body_detail_fn(), "_red": None, "_blue": None}, size=512),
        "head": head,
    }, {"lower": {"l_legs": "body"},
        "upper": {"u_body": "body", "u_vestl": "body", "u_vestr": "body"},
        "head": {"h_head": "head"}},
        {"lower": ["tag_torso"], "upper": ["tag_head", "tag_weapon", "tag_torso"], "head": ["tag_head"]})
