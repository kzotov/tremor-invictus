"""B.J. on bitterman: the green hide graded to a weathered human skin, an
olive vest over the chest, olive fatigues over the legs, a brown leather
belt, and a blond buzz cut painted over the tentacles. Team versions keep
the team's paint and the same bare head."""
import math
from skins import Sheet, Look, region, inside, seg_dist
from tremorart import Fbm, Noise, clamp, smooth, mix

# ---------------------------------------------------------------- the body sheet (bitterman.tga, 768 px view)

VEST = [region([(0, 0), (445, 0), (445, 330), (0, 330)], 768, 768),
        region([(590, 0), (768, 0), (768, 330), (590, 330)], 768, 768)]       # the back
BELT = [region([(0, 300), (445, 300), (445, 410), (0, 410)], 768, 768)]
LEGS = [region([(0, 420), (285, 420), (285, 768), (0, 768)], 768, 768)]
GEAR = [region(p, 768, 768) for p in (
    [(305, 60), (615, 60), (615, 390), (305, 390)],       # the shoulder plate
    [(290, 390), (560, 390), (560, 545), (290, 545)],     # pouches
    [(300, 575), (380, 575), (380, 768), (300, 768)],
)]
FLESH_GRADE = (0.64, 0.5, 0.64)         # olive hide to skin
VEST_COLOUR = (0.17, 0.19, 0.11)
FATIGUES = (0.2, 0.21, 0.13)
LEATHER = (0.3, 0.17, 0.09)


def body_grade(u, v):
    if inside(GEAR, u, v) or inside(BELT, u, v):
        return [0.5, 0.5, 0.5, 1]
    return list(FLESH_GRADE) + [1]


def body_detail_fn():
    n, camo = Fbm(1111, 4, 6), Fbm(1117, 3, 3)

    def f(u, v):
        k = 0.8 + 0.4 * n.at(u * 5, v * 5)
        if inside(VEST, u, v):
            c = [x * k for x in VEST_COLOUR]
            # quilted rows and a zip down the middle
            if abs((v * 768 / 46) % 1 - 0.5) < 0.05 or abs(u - 0.29) < 0.004:
                c = [x * 0.5 for x in c]
            return c + [0.88]
        if inside(LEGS, u, v):
            c = [x * k for x in FATIGUES]
            if camo.at(u * 3, v * 3) > 0.56:
                c = [x * 0.6 for x in c]
            return c + [0.82]
        if inside(BELT, u, v):
            return [x * k for x in LEATHER] + [0.7]
        return [0, 0, 0, 0]
    return f


# ---------------------------------------------------------------- the head (h_bitterman.tga, 768 px view)

FACE = region([(0, 210), (300, 200), (420, 300), (440, 560), (330, 700), (160, 740), (0, 700)], 768, 768)
SCAR = ((0.27, 0.46), (0.33, 0.56))
HEAD_GRADE = (0.68, 0.5, 0.72)
HAIR = (0.82, 0.68, 0.38)


def head_grade(u, v):
    return list(HEAD_GRADE) + [1]


def head_detail_fn():
    stubble, n = Noise(1123, 256, 256), Fbm(1129, 3, 6)

    def f(u, v):
        d = FACE(u, v)
        if d > -0.01:
            # buzz cut: fine light flecks over a darker scalp, fading in at the hairline
            fleck = stubble.at(u * 220, v * 220)
            c = mix(tuple(x * 0.45 for x in HAIR), HAIR, smooth(0.4, 0.8, fleck))
            k = 0.85 + 0.3 * n.at(u * 3, v * 3)
            return [x * k for x in c] + [clamp((d + 0.01) / 0.03) * 0.95]
        if v > 0.66:
            # stubble on the jaw
            return [0.32, 0.25, 0.16, 0.35 * smooth(0.5, 0.7, stubble.at(u * 260, v * 260))]
        sd, _ = seg_dist(u, v, *SCAR)
        if sd < 0.006:
            return [0.62, 0.38, 0.36, 0.6]
        return [0, 0, 0, 0]
    return f


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
