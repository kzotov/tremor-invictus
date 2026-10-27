"""Athena on crash: a military uniform, with olive armour panels and
small sage lights round their rims, the bare skin and the leathers under
a dark olive suit, and major's head instead of the helmet, with dark
hair. Team versions keep the team's paint and only cover the skin, lit in
the team's colour.

Where the armour, the skin and the hair are is worked out on the player's
machine: the armour is the pieces the model paints as green-grey metal,
the skin the smooth tan of the torso and arms, the chest plate the front
of the chest with a light across it, the hair all of the head but the
front of the face."""
import colorsys
from skins import Sheet, Look, LabelMap, painted
from tremorart import Fbm, Noise, clamp, smooth

SUIT = (0.12, 0.15, 0.12)
OLIVE = (0.33, 0.36, 0.12)
LIGHT = {"": (0.45, 0.85, 0.7), "_red": (1.0, 0.35, 0.2), "_blue": (0.3, 0.7, 1.0)}
DARK = [0.05, 0.07, 0.12, 0.9]


def light(colour, k):
    k = clamp(k)
    return [colour[0] * k, colour[1] * k, colour[2] * k, 1]


# ---------------------------------------------------------------- the body sheet (crash.tga)

def metal(c):
    """The model's green-grey metal."""
    h, s, val = colorsys.rgb_to_hsv(*c)
    return (0.2 < h < 0.6 or s < 0.12) and val > 0.08


def tan(geo, u, v, d=1.0 / 256):
    """The model's bare skin: smooth, light tan."""
    h, s, val = colorsys.rgb_to_hsv(*(geo.colour(u, v) or (0, 0, 0)))
    if not (0.03 < h < 0.13 and 0.25 < s < 0.6 and val > 0.3):
        return False
    ls = [sum(geo.colour(u + i * d, v + j * d) or (0, 0, 0)) / 3 for i in (-1, 0, 1) for j in (-1, 0, 1)]
    m = sum(ls) / 9
    return sum((q - m) ** 2 for q in ls) / 9 < 0.03 ** 2


_maps = {}


def body_map(geo, n=128):
    """The body sheet's LabelMap: a armour, s skin, o the rest. A piece is
    armour when a tenth of its paint is metal; skin is where most texels
    round about are tan, on the torso and arms."""
    if id(geo) in _maps:
        return _maps[id(geo)]
    count = {}
    for y in range(n):
        for x in range(n):
            u, v = (x + 0.5) / n, (y + 0.5) / n
            t = geo.at(u, v)
            if t.covered:
                e = count.setdefault(t.island, [0, 0])
                e[0] += metal(geo.colour(u, v) or (0, 0, 0))
                e[1] += 1
    armour = {k for k, (m, all_) in count.items() if m > 0.1 * all_}
    hit = [[geo.at((x + 0.5) / n, (y + 0.5) / n).part == "upper" and tan(geo, (x + 0.5) / n, (y + 0.5) / n)
            for x in range(n)] for y in range(n)]
    r = 4

    def skin(x, y):
        ys, xs = range(max(0, y - r), min(n, y + r + 1)), range(max(0, x - r), min(n, x + r + 1))
        return sum(hit[j][i] for j in ys for i in xs) > 0.3 * len(ys) * len(xs)

    def rule(t, u, v):
        x, y = min(n - 1, int(u * n)), min(n - 1, int(v * n))
        if skin(x, y):
            return "s"
        return "a" if t.covered and t.island in armour else "o"
    _maps[id(geo)] = m = LabelMap.painted(rule, n)(geo)
    return m


def rim(m, u, v, width=0.6):
    """0..1 on a thin line just inside an armour piece's rim."""
    if m.label(u, v) != "a":
        return 0.0
    return 1 - abs(m.edge(u, v) - 2.0) / width


@painted
def body_grade(geo):
    m = body_map(geo)
    return lambda u, v: [0.55, 0.56, 0.42, 1] if m.label(u, v) == "a" else [0.42, 0.42, 0.44, 1]


def body_detail_fn(team):
    n = Fbm(1011, 4, 6)

    @painted
    def make(geo):
        m = body_map(geo)

        def f(u, v):
            what = m.label(u, v)
            if what == "s":
                k = 0.8 + 0.4 * n.at(u * 4, v * 4)
                return [x * k for x in SUIT] + [0.92]
            if team:
                return [0, 0, 0, 0]
            if rim(m, u, v, 1.2) > 0:
                return DARK
            if what == "a":
                return list(OLIVE) + [0.65]
            return list(SUIT) + [0.6]
        return f
    return make


def body_glow_fn(colour):
    @painted
    def make(geo):
        m = body_map(geo)
        return lambda u, v: light(colour, rim(m, u, v))
    return make


# ---------------------------------------------------------------- the chest (crash_t.tga): the plate, and a light across it

def chest_frame(geo):
    """(lowest, highest) height of the chest."""
    zs = [t.z for row in geo.grid for t in row if t and t.covered]
    return min(zs), max(zs)


def chest_at(geo, z0, z1, u, v):
    """(on the plate, height of the light above or below, in parts of the chest's height)"""
    t = geo.at(u, v)
    h = (t.z - z0) / (z1 - z0)
    front = t.nrm[0] > 0.25 and t.nrm[1] < 0.6
    return front and h > 0.15, (h - 0.45 if front else 1.0)


@painted
def chest_grade(geo):
    z0, z1 = chest_frame(geo)
    return lambda u, v: [0.55, 0.56, 0.42, 1] if chest_at(geo, z0, z1, u, v)[0] else [0.42, 0.42, 0.44, 1]


@painted
def chest_detail(geo):
    z0, z1 = chest_frame(geo)

    def f(u, v):
        plate, d = chest_at(geo, z0, z1, u, v)
        if abs(d) < 0.016:
            return DARK
        if plate:
            return list(OLIVE) + [0.65]
        return list(SUIT) + [0.6]
    return f


def chest_glow_fn(colour):
    @painted
    def make(geo):
        z0, z1 = chest_frame(geo)
        return lambda u, v: light(colour, 1 - abs(chest_at(geo, z0, z1, u, v)[1]) / 0.009)
    return make


# ---------------------------------------------------------------- the head (major's): dark hair

HAIR = (0.11, 0.08, 0.06)


def head_detail_fn():
    strands, n = Noise(1031, 128, 128), Fbm(1033, 3, 4)

    @painted
    def make(geo):
        ts = [t for row in geo.grid for t in row if t and t.covered]
        z0, z1 = min(t.z for t in ts), max(t.z for t in ts)
        x0, x1 = min(t.x for t in ts), max(t.x for t in ts)
        # the face: the front of the head below the brow
        brow, cheek = z0 + 0.72 * (z1 - z0), x1 - 0.29 * (x1 - x0)

        def f(u, v):
            t = geo.at(u, v)
            out = max(cheek - t.x, t.z - brow)          # > 0 off the face, in model units
            if out < -0.4:
                return [0, 0, 0, 0]
            s = strands.at(u * 40, v * 160) * 0.6 + strands.at(u * 90, v * 300 + 7) * 0.4
            k = (0.55 + 0.6 * smooth(0.3, 0.8, s)) * (0.85 + 0.3 * n.at(u * 3, v * 3))
            return [HAIR[0] * k, HAIR[1] * k, HAIR[2] * k, 0.85 * clamp(out / 0.4 + 1)]
        return f
    return make


def look():
    team = body_detail_fn(True)
    return Look("crash", "athena", {
        "body": Sheet({"": "crash", "_red": "red", "_blue": "blue"},
                      grade={"": body_grade, "_red": None, "_blue": None},
                      detail={"": body_detail_fn(False), "_red": team, "_blue": team},
                      glow={v: body_glow_fn(LIGHT[v]) for v in LIGHT}, size=512),
        "chest": Sheet({"": "crash_t", "_red": "red_t", "_blue": "blue_t"},
                       grade={"": chest_grade, "_red": None, "_blue": None},
                       detail={"": chest_detail, "_red": None, "_blue": None},
                       glow={v: chest_glow_fn(LIGHT[v]) for v in LIGHT}, size=256),
        "head": Sheet({"": "major_h", "_red": "major_h", "_blue": "major_h"}, model="major",
                      detail=head_detail_fn(), size=256),
    }, {"lower": {"l_legs": "body"},
        "upper": {"u_arms": "body", "u_chest": "chest"},
        "head": {"h_head": "head"}},
        {"lower": ["tag_torso"], "upper": ["tag_head", "tag_weapon", "tag_torso"], "head": ["tag_head"]},
        head_model="major")
