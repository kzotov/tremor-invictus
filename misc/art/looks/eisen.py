"""Eisen on sarge's body, painted from scratch: a red quilted work vest
over a yellow work shirt and a white undershirt, a brown leather belt,
khaki work trousers with leather chaps over the thighs, grey boots,
yellow work gloves, and ginger hair and a full beard. All of it is our own
opaque paint except the face, which keeps the model's features under a
skin wash; shadows gather where one material meets another, the vest is
quilted in puffed diamonds, the shirt and trousers fold, the chaps and
vest are stitched, and the leather and boots are scuffed. The shoulder pad is hidden. Team versions paint the vest in
the team's colour."""
import math
from skins import Sheet, Look, LabelMap, painted
from tremorart import Fbm, Noise, clamp, smooth, mix

# what each texel of sarge's band.tga is, worked out on the player's machine
# from where it sits on the standing model: h hair, b beard, f face, v vest,
# w undershirt, y shirt and sleeves, l gloves, s bare skin, g belt, c chaps,
# k trousers, o boots
TURN = math.radians(40)         # the torso is turned this far to the right in its stand frame


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


def spans(geo, n=128):
    """The lowest and highest point of each UV island: {island: (z0, z1)}."""
    out = {}
    for y in range(n):
        for x in range(n):
            t = geo.at((x + 0.5) / n, (y + 0.5) / n)
            if t and t.covered:
                z0, z1 = out.get(t.island, (t.z, t.z))
                out[t.island] = (min(z0, t.z), max(z1, t.z))
    return out


def label_map(geo):
    if hasattr(geo, "eisen_map"):
        return geo.eisen_map
    span = spans(geo)
    c, s = math.cos(TURN), math.sin(TURN)

    def rule(t):
        x, y, z = t.pos
        z0, z1 = span[t.island]
        if t.surf == "h_head":
            if z > 31 or x < -0.5:
                return "h"                                   # the crown and the back of the head
            if (x > 1.5 and z < 27.5) or z < 27.1:
                return "b"                                   # the beard round the jaw
            return "f" if x > 2.1 else "h"                   # the face, framed by sideburns
        if t.part == "upper":
            if z1 > 26:                                      # pieces up to the neck: the vest
                side, ahead = abs(x * c + y * s + 0.75), x * s - y * c
                if ahead > 2 and side < 1.4:                 # open down the chest's middle
                    return "w" if side < 0.6 else "y"
                return "v"
            if z > 22.6:
                return "y"                                   # the sleeves
            return "l" if x > 11.8 else "s"                  # the hand held forward is gloved
        if z > 7.2:
            return "g"
        if z < -17:
            return "o"
        if z > -11 and z0 < -12 and z1 > 5:                  # long pieces from the knee to the hips
            return "c"
        return "k"
    geo.eisen_map = sort_texels(geo, rule, "v")              # the hidden shoulder pad's texels: vest
    return geo.eisen_map


VEST = {"": (0.62, 0.13, 0.1), "_red": (0.66, 0.08, 0.06), "_blue": (0.12, 0.24, 0.66)}
COLOURS = {
    "w": (0.86, 0.84, 0.78), "y": (0.86, 0.66, 0.14), "l": (0.88, 0.7, 0.16),
    "s": (0.8, 0.56, 0.42), "g": (0.3, 0.17, 0.08), "c": (0.46, 0.26, 0.12),
    "k": (0.52, 0.44, 0.22), "o": (0.44, 0.44, 0.42),
}
SKIN = (0.8, 0.56, 0.42)
GINGER = (0.8, 0.38, 0.13)


def quilt(x, y):
    """Light on a puffed diamond of the vest: 1 on its crown, low in the seams."""
    a, b = (x + y) / 26.0, (x - y) / 26.0
    fa, fb = a - math.floor(a), b - math.floor(b)
    seam = min(fa, 1 - fa, fb, 1 - fb)
    return smooth(0.0, 0.22, seam) * (0.85 + 0.3 * (0.5 - fa))   # lit from above


def detail_fn(variant):
    @painted
    def make(geo):
        return paint(variant, label_map(geo))
    return make


def paint(variant, MAP):
    n, fine, strands = Fbm(2311, 4, 6), Noise(2313, 256, 256), Noise(2317, 128, 128)
    folds = Noise(2319, 32, 32)

    def f(u, v):
        x, y = u * 512, v * 512
        k = 0.82 + 0.36 * n.at(u * 6, v * 6)
        c, e = MAP.label(u, v), MAP.edge(u, v)
        if c == "f":
            return list(SKIN) + [0.35]
        k *= MAP.shade(u, v, 0.5, 3.5)                       # shadow where materials meet
        if c in "hb":
            s = strands.at(u * 60, v * 240) * 0.6 + strands.at(u * 140, v * 420 + 5) * 0.4
            hair = mix(tuple(z * 0.5 for z in GINGER), GINGER, smooth(0.3, 0.8, s))
            return [clamp(z * k) for z in hair] + [1]
        if c == "v":
            k *= 0.55 + 0.55 * quilt(x, y)
            if 1.0 < e < 1.8 and (x + 2 * y) % 7 < 3.5:
                return [0.85, 0.75, 0.6, 1]                  # stitched edge
            return [clamp(z * k) for z in VEST[variant]] + [1]
        if c in "ywk":
            fold = folds.at(u * 18, v * 5)                  # long folds down the cloth
            k *= 0.78 + 0.4 * smooth(0.2, 0.8, fold)
            if c == "k":
                k *= 0.92 + 0.16 * fine.at(x * 2, y * 2)     # twill
        elif c == "c":
            k *= (0.85 + 0.25 * fine.at(x, y)) * (0.6 if (x % 40) < 1.5 else 1)
            if 1.2 < e < 2.0 and (x + y) % 8 < 4:
                return [0.8, 0.66, 0.45, 1]                  # stitched hem
            if fine.at(x * 0.5 + 7, y * 0.5) > 0.7:
                k *= 1.25                                    # scuffed leather
        elif c == "o":
            k *= 0.85 + 0.3 * fine.at(x, y)
            if e < 1.5:
                k *= 0.55                                    # the sole and welt
            elif fine.at(x * 0.7, y * 0.7 + 9) > 0.72:
                k *= 1.3                                     # scuffs
        elif c == "g":
            k *= 0.8 + 0.4 * smooth(0.4, 0.6, fine.at(x * 0.5, y * 3))
            if 1.0 < e < 1.8 and x % 6 < 3:
                return [0.7, 0.55, 0.3, 1]                   # stitched belt edge
        elif c == "l":
            k *= 0.8 + 0.35 * folds.at(u * 40, v * 40)       # creased gloves
        return [clamp(z * k) for z in COLOURS[c]] + [1]
    return f


def look():
    return Look("sarge", "eisen", {
        "body": Sheet({"": "band", "_red": "band", "_blue": "band"},
                      detail={v: detail_fn(v) for v in VEST}, size=512),
    }, {"lower": {"l_legs": "body"},
        "upper": {"u_torso": "body", "u_rshoulder": "tremor/nodraw"},
        "head": {"h_head": "body", "h_cigar": "tremor/nodraw"}},
        {"lower": ["tag_torso"], "upper": ["tag_head", "tag_torso", "tag_weapon"], "head": ["tag_head"]})
