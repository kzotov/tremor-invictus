"""Galena: lucy's armour under uriel's hood, the body painted from scratch
as a priestess's: ivory plate with gold trim and engraved lines, a cream
breastplate with a gold cross, layered pauldrons banded in gold, a gold
belt, a cream tabard bordered in gold over near-black robes, and ivory
boots; lit from the model's own form. The hood is near-black with a gold
edge; under it a pale face framed by long black hair, with a red mark on
the brow. Team versions dye the robes and the hood in the team's colour."""
import math
from skins import Sheet, Look, LabelMap, painted
from tremorart import Fbm, Noise, clamp, smooth


# ---------------------------------------------------------------- the body sheet (lucy's angel.tga)

# what each texel is, worked out on the player's machine from where it sits
# on the standing model: w ivory plate, c breastplate, p pauldrons,
# g gauntlets, b belt, t tabard, d robes, o boots, k nothing


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


def label_map(geo):
    if hasattr(geo, "galena_map"):
        return geo.galena_map

    def rule(t):
        x, y, z = t.pos
        if t.part == "lower":
            if z > 11:
                return "b"                                   # the belt round the hips
            if z < -19:
                return "o"
            if z < -6:
                return "w"                                   # greaves
            return "t" if x > 2.5 and abs(y) < 3.2 else "d"  # the tabard hangs down the front
        if t.surf in ("u_larm", "u_rarm"):
            return "p" if z > 24 else "g" if z < 16 else "d"
        if z < 18:
            return "d"                                       # robes below the breastplate
        return "c" if x > 3.5 else "w"
    geo.galena_map = sort_texels(geo, rule, "k")
    return geo.galena_map


@painted
def body_label(geo):
    return label_map(geo).label


IVORY = (0.84, 0.82, 0.77)
CREAM = (0.86, 0.79, 0.62)
GOLD = (0.78, 0.6, 0.24)
ROBE = {"": (0.1, 0.095, 0.1), "_red": (0.32, 0.04, 0.04), "_blue": (0.05, 0.1, 0.36)}
SHINE = {"w": 0.35, "c": 0.35, "p": 0.4, "g": 0.35, "b": 0.5, "t": 0.1, "d": 0.04, "o": 0.3}
# the form shading over the body, worked out on the player's machine from
# the model they have (bake-form.py): (a + b x how much a texel faces the
# light) x (c + d x how open it is), and a highlight per material
FORM = {"body": {"model": ("lucy", "angel", "angel.tga"), "light": (0.45, 0.85), "open": (0.55, 0.45), "shine": SHINE, "label": body_label}}
# the gold cross high on the breastplate, on the body's middle line: (y, z)
# bounds of its upright and of its arms
CROSS = (((-0.3, 0.7), (23.7, 26.1)), ((-1.4, 1.8), (24.7, 25.4)))


def gold(k, nf):
    return [clamp(z * k * (0.85 + 0.3 * nf)) for z in GOLD]


def detail_fn(variant):
    @painted
    def make(geo):
        return paint(variant, label_map(geo), geo)
    return make


def paint(variant, MAP, geo):
    n, fine, weave = Fbm(1311, 4, 5), Noise(1323, 256, 256), Noise(1325, 64, 512)

    def f(u, v):
        c = MAP.label(u, v)
        if c == "k":
            return [0, 0, 0, 0]
        x, y, e = u * 512, v * 512, MAP.edge(u, v)
        k = (0.88 + 0.24 * n.at(u * 6, v * 6)) * MAP.shade(u, v, 0.45, 3.0)
        nf = fine.at(x, y)
        if c in "wpgo":
            col = [clamp(z * k) for z in IVORY]
            if e < 1.3:
                col = gold(k, nf)                                # gold trim on every plate
            elif e < 1.7:
                col = [z * 0.55 for z in IVORY]                  # the trim's groove
            elif c == "p" and (y % 30) < 5:
                col = gold(k, nf)                                # the pauldron's banded lames
            elif c == "p" and (y % 30) < 7:
                col = [z * 0.5 for z in IVORY]
            elif c == "g" and (y % 48) < 6:
                col = gold(k, nf)                                # cuffs
            elif c == "o" and (y % 40) > 32:
                col = gold(k, nf)                                # toe caps
            elif abs(math.sin(x * 0.11) * 6 + math.sin(y * 0.07) * 6 - (x - y) % 24 + 12) < 0.9:
                col = [z * 0.78 for z in col]                    # engraved scrollwork
        elif c == "c":
            t = geo.at(u, v)
            if any(y0 < t.y < y1 and z0 < t.z < z1 for (y0, y1), (z0, z1) in CROSS):
                col = gold(k * 1.1, nf)                          # the cross
            elif e < 1.3:
                col = gold(k, nf)
            else:
                col = [clamp(z * k) for z in CREAM]
        elif c == "b":
            col = gold(k, nf)
            if (x % 40) < 10 and 8 < (y % 24) < 16:
                col = [z * k for z in ROBE[variant]]              # set stones
            elif (y % 24) < 2:
                col = [z * 0.5 for z in GOLD]
        elif c == "t":
            col = [clamp(z * k * (0.9 + 0.12 * weave.at(u * 64, v * 16))) for z in CREAM]
            if e < 2.0:
                col = gold(k, nf)                                # the tabard's border
        else:   # robes
            fold = weave.at(u * 14, v * 3)
            col = [clamp(z * k * (0.65 + 0.7 * smooth(0.2, 0.8, fold)) * (0.92 + 0.16 * weave.at(u * 128, v * 512)))
                   for z in ROBE[variant]]
            if e < 1.2:
                col = gold(k * 0.8, nf)                          # gold-hemmed
        return [clamp(z) for z in col] + [1]
    return f


# ---------------------------------------------------------------- the hood (uriel.tga)

def hood_rim(geo, n=256, reach=3):
    """The texels where the hood's cloth ends: within reach of the edge
    between texels the model uses and texels it leaves unused."""
    used = [[bool(geo.at((x + 0.5) / n, (y + 0.5) / n) and geo.at((x + 0.5) / n, (y + 0.5) / n).covered)
             for x in range(n)] for y in range(n)]
    ring = [(i, j) for i in range(-reach, reach + 1) for j in range(-reach, reach + 1) if i * i + j * j <= reach * reach]
    return {(x, y) for y in range(n) for x in range(n)
            if any(0 <= x + i < n and 0 <= y + j < n and used[y + j][x + i] != used[y][x] for i, j in ring)}


def hood_detail_fn(variant):
    @painted
    def make(geo):
        n, weave = Fbm(1327, 4, 5), Noise(1329, 64, 512)
        if not hasattr(geo, "galena_rim"):
            geo.galena_rim = hood_rim(geo)
        rim = geo.galena_rim

        def f(u, v):
            t = geo.at(u, v)
            if t and (t.x > 1 or t.nrm[0] > 0.45) and (int(u * 256), int(v * 256)) in rim:
                return gold(0.9, 0.5) + [1]                      # the gold edge round the face
            fold = weave.at(u * 16, v * 4)
            k = (0.45 + 1.0 * smooth(0.25, 0.75, fold)) * (0.85 + 0.3 * n.at(u * 4, v * 4))
            k *= 0.7 + 0.5 * smooth(35, 8, t.z if t else 40)     # darker toward the crown
            return [clamp(z * k) for z in ROBE[variant]] + [1]
        return f
    return make


# ---------------------------------------------------------------- the face (uriel_h.tga)

EYE = (0.69, 30.7, 0.44, 0.29)          # off the face's middle line, height; half width, half height


@painted
def face_detail_fn(geo):
    """Placed on the face itself: s is how far a texel is from the face's
    middle line (half way across its width), z its height."""
    n, strands = Fbm(1321, 3, 4), Noise(1331, 64, 256)
    ys = [t.y for t in (geo.at((x + 0.5) / 64, (y + 0.5) / 64) for y in range(64) for x in range(64)) if t and t.covered]
    mid = (min(ys) + max(ys)) / 2 if ys else 0

    def f(u, v):
        t = geo.at(u, v)
        s, z, ahead = (abs(t.y - mid), t.z, t.nrm[0]) if t else (9, 0, 0)
        if s > 1.06 or z > 32.4 or (ahead < 0 and s > 0.2):     # long black hair framing the face
            h = strands.at(u * 40, v * 140)
            k = 0.5 + 0.8 * smooth(0.3, 0.8, h)
            return [0.05 * k, 0.045 * k, 0.05 * k, 1]
        k = 0.88 + 0.16 * n.at(u * 3, v * 3)
        c = [0.86 * k, 0.8 * k, 0.78 * k]                         # pale skin
        c = [y * (1 - 0.25 * smooth(0.34, 0.0, abs(z - 30.0)) * smooth(0.25, 0.69, s)) for y in c]   # cheekbones
        e = math.hypot((s - EYE[0]) / EYE[2], (z - EYE[1]) / EYE[3])
        if e < 1:
            c = [y * (0.3 + 0.7 * smooth(0.375, 1, e)) for y in c]   # dark-lined eyes
        if abs(z - 29.43) < 0.07 and s < 0.45:
            c = [0.42, 0.14, 0.16]                               # lips
        if s < 0.07 and 31.3 < z < 31.93:
            c = [0.6, 0.06, 0.06]                                # the red mark on the brow
        top = smooth(32.72, 31.6, z)                             # the hood's shadow on the brow
        return [y * (0.4 + 0.6 * top) for y in c] + [1.0]
    return f


def look():
    return Look("lucy", "galena", {
        "body": Sheet({"": "angel", "_red": "angel", "_blue": "angel"},
                      detail={v: detail_fn(v) for v in ROBE}, size=1024, form=FORM["body"]),
        "hood": Sheet({"": "uriel", "_red": "uriel", "_blue": "uriel"}, model="uriel",
                      detail={v: hood_detail_fn(v) for v in ROBE}, size=256),
        "face": Sheet({"": "uriel_h", "_red": "uriel_h", "_blue": "uriel_h"}, model="uriel",
                      detail=face_detail_fn, size=128),
    }, {"lower": {"l_legs": "body"},
        "upper": {"u_torso": "body", "u_larm": "body", "u_rarm": "body"},
        "head": {"h_hood": "hood", "h_face": "face"}},
        {"lower": ["tag_torso"], "upper": ["tag_head", "tag_weapon", "tag_torso"], "head": ["tag_head"]},
        head_model="uriel")
