"""Scalebearer: keel's heavy armour under biker's head, the body painted
from scratch: a coat of big overlapping iron scales rimmed in copper over
the back, sides and pauldrons, a chest plate of banded lames with a
copper trim and a glowing gem, gauntlets in small scales, a leather
waist and a wide belt with hanging tassets, worn brown leather trousers
crossed by olive straps, red enamel knee pads, steel greaves and dark
boots with enamel toe caps. Shadows gather where one part meets another.
The head is a pale grey-blue brute with glowing blue eyes under a dark
helm rimmed in copper. Team versions enamel the pads and toe caps and
light the gem and the eyes in the team's colour."""
import math
from skins import Sheet, Look, LabelMap, painted
from tremorart import Fbm, Noise, clamp, smooth, mix

# what each texel of keel.tga is, worked out on the player's machine from
# where it sits on the standing model: s scale coat, p pauldrons, c chest
# plate, g gauntlets, w waist, t belt and tassets, u trousers, n knee pads,
# r greaves, o boots, k nothing


def sort_texels(geo, rule, rest, n=128, reach=4):
    """A LabelMap of the sheet: rule(texel) -> letter for the texels the
    model uses, spread reach texels into the unused ones round them (each
    pass from the left, the right, above, then below), rest beyond."""
    grid = [[None] * n for _ in range(n)]
    for y in range(n):
        for x in range(n):
            t = geo.at((x + 0.5) / n, (y + 0.5) / n)
            if t and t.covered:
                grid[y][x] = rule(t)
    for _ in range(reach):
        for i, j in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            grid = [[grid[y][x] or (grid[y - j][x - i] if 0 <= x - i < n and 0 <= y - j < n else None)
                     for x in range(n)] for y in range(n)]
    return LabelMap.painted(lambda t, u, v: grid[int(v * n)][int(u * n)] or rest, n)(geo)


def body_rule(t):
    x, y, z = t.pos
    if t.part == "lower":
        if z > 9:
            return "t"                                       # belt and tassets
        if z < -17:
            return "o"
        if -11 < z < -3 and t.nrm[0] > 0.5:
            return "n"                                       # the front of the knee
        return "r" if z < -11 else "u"
    if abs(y) > 9:                                           # the arms
        return "p" if z > 25.5 else "g"
    if x > 4.5 and z > 22.5:
        return "c"
    return "w" if z < 19.5 else "s"


def label_map(geo):
    if not hasattr(geo, "scalebearer_map"):
        geo.scalebearer_map = sort_texels(geo, body_rule, "k")
    return geo.scalebearer_map


@painted
def body_label(geo):
    return label_map(geo).label


IRON = (0.17, 0.18, 0.21)
STEEL = (0.32, 0.34, 0.38)
COPPER = (0.56, 0.3, 0.17)
LEATHER = (0.3, 0.19, 0.12)
DARK_LEATHER = (0.14, 0.09, 0.06)
OLIVE = (0.36, 0.36, 0.2)
GUNMETAL = IRON
ENAMEL = {"": (0.6, 0.08, 0.06), "_red": (0.62, 0.06, 0.05), "_blue": (0.08, 0.2, 0.62)}
LIGHT = {"": (0.3, 0.6, 1.0), "_red": (1.0, 0.3, 0.2), "_blue": (0.3, 0.6, 1.0)}
# how glossy each part is
SHINE = {"s": 0.35, "p": 0.4, "g": 0.25, "c": 0.3, "t": 0.15, "w": 0.05, "u": 0.06, "n": 0.4, "r": 0.4, "o": 0.1}
# the form shading over the body, worked out on the player's machine from
# the model they have (bake-form.py): (a + b x how much a texel faces the
# light) x (c + d x how open it is), and a highlight per material
FORM = {"body": {"model": ("keel", "default", "keel.tga"), "light": (0.45, 0.85), "open": (0.5, 0.5), "shine": SHINE, "label": body_label}}
GEM = (1.2, 26.95, 1.05)        # on the chest plate, a little left of its middle: y, z, radius


def gem(geo, u, v):
    """How far a point of the sheet is from the gem's centre, in its radii
    (9 off the front of the chest); placed between texels, for a round gem."""
    n = geo.n
    x, y = u * n - 0.5, v * n - 0.5
    x0, y0 = int(math.floor(x)), int(math.floor(y))
    fx, fy = x - x0, y - y0
    near = [(geo.at((x0 + i + 0.5) / n, (y0 + j + 0.5) / n), (fx if i else 1 - fx) * (fy if j else 1 - fy))
            for i in (0, 1) for j in (0, 1)]
    if any(t is None or t.x < 4.5 or t.nrm[0] < 0.5 for t, w in near):
        return 9
    py, pz = sum(t.y * w for t, w in near), sum(t.z * w for t, w in near)
    return math.hypot(py - GEM[0], pz - GEM[1]) / GEM[2]


def scales(u, v, nu, nv):
    """(depth 0..1 down a scale, distance to its rim 0..) for rows of
    overlapping scales, each row offset by half a scale."""
    y = v * nv
    row = math.floor(y)
    x = u * nu + 0.5 * (row % 2)
    fx, fy = x - math.floor(x) - 0.5, y - row
    shape = fy + 1.8 * fx * fx
    return clamp(shape), 1 - shape


def scale_paint(u, v, nu, nv, k, n):
    depth, rim = scales(u, v, nu, nv)
    if rim < 0.08:
        return [clamp(z * k * (0.8 + 0.4 * n)) for z in COPPER]
    if rim < 0.14:
        return [z * 0.3 for z in IRON]                       # the rim's shadow
    lit = 1.25 - 0.75 * depth                                # lit on top, dark at the foot
    if depth < 0.18:
        lit += 0.5 * (1 - depth / 0.18)                      # a glint along the scale's crown
    ridge = 1.15 if abs(((u * nu + 0.5 * (math.floor(v * nv) % 2)) % 1) - 0.5) < 0.04 else 1
    return [clamp(z * k * lit * ridge) for z in IRON]


def detail_fn(variant):
    @painted
    def make(geo):
        return paint(variant, label_map(geo), geo)
    return make


def paint(variant, MAP, geo):
    n, fine, grain = Fbm(1411, 4, 6), Noise(1415, 256, 256), Noise(1421, 64, 512)

    def f(u, v):
        c = MAP.label(u, v)
        if c == "k":
            return [0, 0, 0, 0]
        x, y, e = u * 512, v * 512, MAP.edge(u, v)
        k = (0.85 + 0.3 * n.at(u * 6, v * 6)) * MAP.shade(u, v, 0.55, 3.5)
        nf = fine.at(x, y)
        if c == "s":
            col = scale_paint(u, v, 22, 30, k, nf)
        elif c == "p":
            col = scale_paint(u, v, 14, 18, k, nf)
        elif c == "g":
            col = scale_paint(u, v, 36, 48, k * 0.9, nf)
        elif c == "c":
            d = gem(geo, u, v)
            if d < 1:
                col = [z * (0.3 + 0.7 * (1 - d)) for z in LIGHT[variant]]
            elif d < 1.5:
                col = list(COPPER)                            # the gem's setting
            elif e < 1.6:
                col = [clamp(z * k * (0.8 + 0.4 * nf)) for z in COPPER]   # trim
            else:
                lame = (y % 26) / 26.0
                col = [clamp(z * k * (1.2 - 0.6 * lame) * (0.92 + 0.16 * grain.at(u * 16, v * 128)))
                       for z in IRON]
                if lame < 0.07:
                    col = [z * 0.25 for z in IRON]           # the gap between lames
                elif (x % 26) < 3 and 0.12 < lame < 0.24:
                    col = list(COPPER)                        # rivets
        elif c == "w":
            col = [clamp(z * k * (0.85 + 0.3 * nf)) for z in LEATHER]
            if (y % 18) < 2:
                col = [z * 0.5 for z in col]                  # stitched bands
        elif c == "t":
            band = (y % 40) / 40.0
            if band < 0.4:
                col = [clamp(z * k * (0.85 + 0.3 * nf)) for z in DARK_LEATHER]   # the belt
                if band < 0.06 or band > 0.34:
                    col = list(COPPER)
            else:
                col = scale_paint(u, v, 18, 12.8, k, nf)       # tassets hanging below
        elif c == "u":
            crease = abs(grain.at(u * 10, v * 70) - 0.5) < 0.02
            col = [clamp(z * k * (0.8 + 0.4 * nf) * (0.6 if crease else 1)) for z in LEATHER]
            if nf > 0.75:
                col = [clamp(z * 1.3) for z in col]           # scuffed
            strap = min(abs((x + y) % 150 - 75), abs((x - y) % 170 - 85))
            if strap < 12:
                col = [clamp(z * k * (0.85 + 0.3 * nf)) for z in OLIVE]
                if strap > 10.5:
                    col = [z * 0.5 for z in col]
                elif (x % 30) < 3:
                    col = list(STEEL)                         # rivets on the straps
        elif c == "n":
            if e < 0.8:
                col = list(STEEL)                             # the pad's steel rim
            elif nf > 0.86:
                col = [0.7, 0.68, 0.66]                       # chipped enamel
            else:
                col = [clamp(z * k * (1.1 - 0.3 * v % 1)) for z in ENAMEL[variant]]
        elif c == "r":
            col = [clamp(z * k * (0.9 + 0.2 * grain.at(u * 16, v * 128))) for z in STEEL]
            if (y % 32) < 2:
                col = [z * 0.4 for z in col]                  # plate seams
            elif (y % 32) < 5 and (x % 22) < 3:
                col = list(COPPER)
        else:   # boots
            if (y % 64) > 44:
                col = [clamp(z * k) for z in ENAMEL[variant]]  # the toe cap
                if nf > 0.8:
                    col = [0.7, 0.68, 0.66]
            else:
                col = [clamp(z * k * (0.8 + 0.4 * nf)) for z in DARK_LEATHER]
                if (y % 10) < 2 and abs(x % 40 - 20) < 6:
                    col = [0.5, 0.45, 0.4]                    # laces
        return [clamp(z) for z in col] + [1]
    return f


def glow_fn(colour):
    @painted
    def make(geo):
        def f(u, v):
            k = clamp(1.2 * (1 - gem(geo, u, v)))
            return [colour[0] * k, colour[1] * k, colour[2] * k, 1]
        return f
    return make


def neck_detail(u, v):
    return list(IRON) + [0.9]


# ---------------------------------------------------------------- the head (biker_h.tga)

# placed on the head itself: s is how far a texel is from the face's middle
# line, z its height, x how far forward
MID = -0.2                      # the face's middle line (y)
EYES = (1.16, 35.07, 5.0)       # either side of it, at this height, at least this far forward
SKIN = (0.6, 0.66, 0.74)


def helm(t):
    """How deep a texel is in the helm, in model units (< 0 on the face):
    it comes down to the brow and over the sides and back of the head,
    behind the cheeks."""
    return max(t.z - 36.4, 2.8 - t.x if t.z > 32 else -9)


def eye(t, rs, rz):
    """Distance from the nearer eye, in radii (rs across, rz up)."""
    if t.x < EYES[2] - 1:
        return 9
    return math.hypot((abs(t.y - MID) - EYES[0]) / rs, (t.z - EYES[1]) / rz)


@painted
def head_grade(geo):
    def f(u, v):
        t = geo.at(u, v)
        if t is None or helm(t) > 0:
            return [0.4, 0.4, 0.45, 1]
        return [0.62, 0.68, 0.8, 1]                   # warm skin to a cold grey-blue
    return f


@painted
def head_detail_fn(geo):
    n = Fbm(1419, 4, 6)

    def f(u, v):
        t = geo.at(u, v)
        if t is None:
            return list(GUNMETAL) + [1]
        h, s = helm(t), abs(t.y - MID)
        k = 0.8 + 0.4 * n.at(u * 8, v * 8)
        if h > 0.28:
            fall = 0.0463 * (47.7 - t.z) - 0.029 * t.x + 0.09 * s     # darker down the helm and round its sides
            k *= (0.7 + 0.3 * smooth(0.28, 1.2, h)) * (1.5 - 0.9 * smooth(0, 1, fall))
            front = t.x > 2.5 and t.z > 36.8              # the brow and the front of the dome
            if front and abs(t.y + 0.8) < 0.75:
                k *= 1 + 0.8 * (1 - abs(t.y + 0.8) / 0.75)  # light along the dome
            ridge = front and abs(t.y + 0.15) < 0.2        # a ridge down the brow
            return [clamp(z * k * (2.2 if ridge else 1)) for z in GUNMETAL] + [1]
        if h > 0:
            return list(COPPER) + [1]                      # the helm's rim
        d = eye(t, 0.55, 0.235)
        if d < 1:
            return [0.02, 0.03, 0.06, 1 - 0.5 * d]         # a dark socket round the light
        return [clamp(z * k) for z in SKIN] + [0.45]
    return f


def head_glow_fn(colour):
    @painted
    def make(geo):
        def f(u, v):
            t = geo.at(u, v)
            k = clamp((1 - eye(t, 0.36, 0.147)) * 1.6) if t else 0
            return [colour[0] * k, colour[1] * k, colour[2] * k, 1]
        return f
    return make


def look():
    return Look("keel", "scalebearer", {
        "body": Sheet({"": "keel", "_red": "keel", "_blue": "keel"},
                      detail={v: detail_fn(v) for v in ENAMEL},
                      glow={v: glow_fn(LIGHT[v]) for v in LIGHT}, size=1024, form=FORM["body"]),
        "neck": Sheet({"": "keel_h", "_red": "keel_h", "_blue": "keel_h"}, detail=neck_detail, size=64),
        "head": Sheet({"": "biker_h", "_red": "biker_h", "_blue": "biker_h"}, model="biker",
                      grade=head_grade, detail=head_detail_fn,
                      glow={v: head_glow_fn(LIGHT[v]) for v in LIGHT}, size=256),
    }, {"lower": {"l_legs": "body"},
        "upper": {"u_torso": "body", "u_neck": "neck"},
        "head": {"h_head": "head"}},
        {"lower": ["tag_torso"], "upper": ["tag_head", "tag_torso", "tag_weapon"], "head": ["tag_head"]},
        head_model="biker")
