"""Galena: lucy's armour under uriel's hood, the body painted from scratch
as a priestess's: ivory plate with gold trim and engraved lines, a cream
breastplate with a gold cross, layered pauldrons banded in gold, a gold
belt, a cream tabard bordered in gold over near-black robes, and ivory
boots; lit from the model's own form. The hood is near-black with a gold
edge; under it a pale face framed by long black hair, with a red mark on
the brow. Team versions dye the robes and the hood in the team's colour."""
import math
from skins import Sheet, Look, LabelMap, region, inside, seg_dist
from tremorart import Fbm, Noise, clamp, smooth


# ---------------------------------------------------------------- the body sheet (lucy's angel.tga)

# what each texel is, 128 x 128, run-length coded rows: w ivory plate,
# c breastplate, p pauldrons, g gauntlets, b belt, t tabard, d robes,
# o boots, k nothing
MAP = LabelMap((
    "w3k4w5gw13g2w27c12o35w26",
    "w3k4w46c15o29w31",
    "w3k4w45c18o26w32",
    "w3k4w43c22o23w33",
    "w3k4w42c23k23w33",
    "w3k4w39cwc25k22w33",
    "w3k4w35c31k22w33",
    "w3k4w35c32k21w33",
    "w3k4w34c33k21w33",
    "w3k4w33c34k21w33",
    "w3k4w33c34k22w32",
    "w3k4w32c35k22w32",
    "w3k4w32c35k23w31",
    "w3k4w31c36k23w31",
    "w3k4w31c36k23c11w20",
    "w3k4w31c36k23c11w20",
    "w3k3w31c37k22c13w19",
    "w3k2w31c38k22c13w19",
    "w3kw31c39k21c14w19",
    "w34c40k21c14w19",
    "w33c41k21c14w19",
    "w32c42k20c15w19",
    "w31c43k20c15w19",
    "w31c43k20c15w19",
    "w30c44k18c17w19",
    "w30c44k16c20w18",
    "w29c45k15c21w18",
    "w29c45k13c23w18",
    "w29c45k11c25w18",
    "w29c45k10c26w18",
    "w28c46k10c26w18",
    "w28c46k10c26w18",
    "w27c46k11c26w18",
    "w27c46k11c11d14cw18",
    "w27c46k11d44",
    "w26c47b11d44",
    "w26c46b12d44",
    "w26c45b14d43",
    "w26c45b17d39w",
    "w26c44b21d35w2",
    "w4dw21c42b30d19b8dw2",
    "w3d5w18c42b58w2",
    "w2d10w14c42b58w2",
    "w2d16w9c41b58w2",
    "wd23w3c41b59w",
    "wd28c39b59w",
    "wd36c31b59w",
    "d44c23b60d",
    "d48c4dc13b61d",
    "d50c16b62",
    "d46c20b62",
    "bd42c23b62",
    "b2d24bd27c12b23d3b36",
    "b2d21b5d6bd26c5b4d11b3td9b34",
    "b3d20b12d27c3d16t3d11b33",
    "d22b3d9bd28c2d17t3d16b6d5b11d5",
    "d21b3d10bd93",
    "d3k18d13td93",
    "d3k18d12t2d93",
    "d3k18d10t4d26kt2d64",
    "d3k18d7t7d24k3t3d4t2d57",
    "d3p13k5d36k4t5d2t3d57",
    "d2p19d34k5dt10d57",
    "dp21d31k7d2t10d56",
    "dp26d24k3b6d3t9d56",
    "dp34d16k3b7d3t8d56",
    "dp37d12k4b8d3t8d55",
    "dp42d6k5b9d2t8d55",
    "dp43d5k5b10t9d55",
    "dp44k9b10td3t5d55",
    "dp44k9b11d3t4d56",
    "dp44k9b12d2t5d55",
    "dp10dp33k9b12d2t5d55",
    "dp9d4p32k8b13dt5d55",
    "p9d7p30k8b13dt5d51t4",
    "p8d10p28k8b13t6d50t5",
    "p6d14p26k8b14t6d49t5",
    "p5d17p24dk7b14t6d48t6",
    "pd22p22d2k7b14t6d47t7",
    "d25p13d9k7b15t5d47t7",
    "d26p3dp5d12k7b15t5d46t8",
    "d30p3d14k8b15t5d44t9",
    "d31pd15k8b15t5d44t9",
    "d47k8b11t9d43t9d",
    "d47k8b2t18d42t10d",
    "d47k8bt15d3td17w3d22t10d",
    "d47k9t15d3t2d9w20d12t9kd",
    "d47k9t15d3w34d8t10d2",
    "d47k9t15d2w38d5t7w3d2",
    "d47k9t16w54d2",
    "d47k9t16w54d2",
    "d47k9t16w54d2",
    "d11gd35k10t16w53d2",
    "d11g2d33k11t15w54d2",
    "d11gd34k11t15w55d",
    "d11gd33k12t15w55d",
    "d10g5d30k12t15w55d",
    "d10g8d26k13t15w55d",
    "d10g9d25k14t14w55d",
    "d9g11d23k15t13w56d",
    "d9g14d20k15t13w56d",
    "d9g14d20k15t12w57d",
    "d8g16d19k15t11w58d",
    "d8g16d19k16t9w59d",
    "d8g17d18k25w59d",
    "d7g18d18k24w60d",
    "d8g18d17k24w61",
    "d8g18d17k23w62",
    "d8g19d16k22w63",
    "d8g19d16k21w64",
    "wd7g20d15k21w64",
    "wd8g19d15k14o7w64",
    "wd8g19d15k13o9w63",
    "wd8g19d15k13o10w27ow22o3w9",
    "wd8g20d14k12o11w14o26w8o11w3",
    "wkd8g18dg3d11k11o53w3o16w2",
    "wkd8g19d14k11o73w",
    "wk2d8g18d14k11o74",
    "k3d9g18d12k12o74",
    "k4d8g18d12k12o73k",
    "k4d9g17d11k13o72k2",
    "k4d9g19d9k13o71k3",
    "k4d9g21d7k13o70k4",
    "k4d9g20d8k13o69k5",
    "k4d9g19d9w13o69k5",
    "w3kd7g22d6w16o67w6",
    "w3k2d5g24d3w18c4o60w9",
    "w3k3dw3g24w21c8o41wo11w12",
))
IVORY = (0.84, 0.82, 0.77)
CREAM = (0.86, 0.79, 0.62)
GOLD = (0.78, 0.6, 0.24)
ROBE = {"": (0.1, 0.095, 0.1), "_red": (0.32, 0.04, 0.04), "_blue": (0.05, 0.1, 0.36)}
SHINE = {"w": 0.35, "c": 0.35, "p": 0.4, "g": 0.35, "b": 0.5, "t": 0.1, "d": 0.04, "o": 0.3}
# the form shading over the body, worked out on the player's machine from
# the model they have (bake-form.py): (a + b x how much a texel faces the
# light) x (c + d x how open it is), and a highlight per material
FORM = {"body": {"model": ("lucy", "angel", "angel.tga"), "light": (0.45, 0.85), "open": (0.55, 0.45), "shine": SHINE, "label": MAP.label}}
CROSS = (351 / 768, 150 / 768)


def gold(k, nf):
    return [clamp(z * k * (0.85 + 0.3 * nf)) for z in GOLD]


def detail_fn(variant):
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
            dx, dy = abs(u - CROSS[0]) * 768, abs(v - CROSS[1]) * 768
            if (dx < 5 and dy < 26) or (dy < 5 and dx < 17 and v * 768 < CROSS[1] * 768 + 4 and v * 768 > CROSS[1] * 768 - 10):
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


# ---------------------------------------------------------------- the hood (uriel.tga, 768 px view)

TRIM = [((0.0, 0.73), (0.25, 0.62))]


def hood_detail_fn(variant):
    n, weave = Fbm(1327, 4, 5), Noise(1329, 64, 512)

    def f(u, v):
        if min(seg_dist(u, v, *s)[0] for s in TRIM) < 0.012:
            return gold(0.9, 0.5) + [1]                          # the hood's gold edge
        fold = weave.at(u * 16, v * 4)
        k = (0.45 + 1.0 * smooth(0.25, 0.75, fold)) * (0.85 + 0.3 * n.at(u * 4, v * 4))
        k *= 0.7 + 0.5 * smooth(0.9, 0.1, v)                     # lit from above
        return [clamp(z * k) for z in ROBE[variant]] + [1]
    return f


# ---------------------------------------------------------------- the face (uriel_h.tga, mirrored about u 0.27)

MID = 0.27
EYE = (0.4, 0.38, 0.045)


def face_detail_fn():
    n, strands = Fbm(1321, 3, 4), Noise(1331, 64, 256)

    def f(u, v):
        x = abs(u - MID)
        if x > 0.19 or v < 0.08:                                 # long black hair framing the face
            s = strands.at(u * 40, v * 140)
            k = 0.5 + 0.8 * smooth(0.3, 0.8, s)
            return [0.05 * k, 0.045 * k, 0.05 * k, 1]
        k = 0.88 + 0.16 * n.at(u * 3, v * 3)
        c = [0.86 * k, 0.8 * k, 0.78 * k]                         # pale skin
        c = [y * (1 - 0.25 * smooth(0.06, 0.0, abs(v - 0.5)) * smooth(0.06, 0.13, x)) for y in c]
        e = math.hypot(u - EYE[0], (v - EYE[1]) * 1.4)
        if e < EYE[2] * 1.6:
            c = [y * (0.3 + 0.7 * smooth(EYE[2] * 0.6, EYE[2] * 1.6, e)) for y in c]   # dark-lined eyes
        if abs(v - 0.6) < 0.012 and x < 0.08:
            c = [0.42, 0.14, 0.16]                               # lips
        if x < 0.012 and 0.16 < v < 0.27:
            c = [0.6, 0.06, 0.06]                                # the red mark on the brow
        top = smooth(0.02, 0.22, v)                              # the hood's shadow on the brow
        return [y * (0.4 + 0.6 * top) for y in c] + [1.0]
    return f


def look():
    return Look("lucy", "galena", {
        "body": Sheet({"": "angel", "_red": "angel", "_blue": "angel"},
                      detail={v: detail_fn(v) for v in ROBE}, size=1024, form=True),
        "hood": Sheet({"": "uriel", "_red": "uriel", "_blue": "uriel"}, model="uriel",
                      detail={v: hood_detail_fn(v) for v in ROBE}, size=256),
        "face": Sheet({"": "uriel_h", "_red": "uriel_h", "_blue": "uriel_h"}, model="uriel",
                      detail=face_detail_fn(), size=128),
    }, {"lower": {"l_legs": "body"},
        "upper": {"u_torso": "body", "u_larm": "body", "u_rarm": "body"},
        "head": {"h_hood": "hood", "h_face": "face"}},
        {"lower": ["tag_torso"], "upper": ["tag_head", "tag_weapon", "tag_torso"], "head": ["tag_head"]},
        head_model="uriel")
