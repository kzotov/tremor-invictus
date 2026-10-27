"""Eisen on biker: ginger mutton-chops and moustache, blue-glass brass
goggles pushed up on the brow, a leather work apron over the belly,
canvas work trousers over the jeans, and a copper prosthetic for the left arm (its own surface, so only
that arm wears it). Team versions keep the team's paint, the beard, the
goggles and the arm."""
import math
from skins import Sheet, Look, region, inside, rivets
from tremorart import Fbm, Noise, clamp, smooth, mix

# ---------------------------------------------------------------- the body sheet (biker.tga, 768 px view)

APRON = [region([(0, 0), (500, 0), (500, 415), (0, 415)], 768, 768)]
TROUSERS = [region([(0, 415), (460, 415), (460, 768), (0, 768)], 768, 768)]
ARM = [region([(445, 310), (768, 310), (768, 768), (445, 768)], 768, 768)]
LEATHER = (0.26, 0.15, 0.08)
CANVAS = (0.2, 0.19, 0.15)


def body_detail_fn():
    n, weave = Fbm(1211, 4, 6), Noise(1213, 256, 256)

    def f(u, v):
        k = 0.8 + 0.4 * n.at(u * 4, v * 4)
        if inside(APRON, u, v):
            c = [x * k for x in LEATHER]
            if abs(u - 0.32) < 0.006 or abs(v - 0.27) < 0.005:      # stitched seams
                c = [x * 0.55 for x in c]
            return c + [0.86]
        if inside(TROUSERS, u, v):
            w = 0.9 + 0.2 * weave.at(u * 200, v * 200)
            return [x * k * w for x in CANVAS] + [0.8]
        return [0, 0, 0, 0]
    return f


def arm_detail_fn():
    n = Fbm(1217, 4, 5)
    bands = [i / 6 for i in range(1, 6)]

    def f(u, v):
        if not inside(ARM, u, v):
            return [0, 0, 0, 0]
        # copper plates in rings down the arm, with dark joints and brass rivets
        k = 0.7 + 0.6 * n.at(u * 6, v * 6)
        c = [0.36 * k, 0.17 * k, 0.09 * k]
        t = (v - 310 / 768) / (458 / 768)
        if min(abs(t - b) for b in bands) < 0.012:
            c = [0.06, 0.06, 0.07]
        r = rivets(u, v, [(0.62 + 0.08 * i, 310 / 768 + (b - 0.03) * 458 / 768) for b in bands for i in range(4)], 0.009)
        if r >= 0:
            c = [0.4 + 0.45 * r, 0.3 + 0.35 * r, 0.12 + 0.12 * r]
        return c + [1.0]
    return f


# ---------------------------------------------------------------- the head (biker_h.tga, 768 px view)

FACE_CX = 185 / 768
BEARD = region([(0, 470), (90, 520), (150, 560), (230, 560), (300, 520), (360, 470), (380, 560),
                (560, 640), (600, 768), (0, 768)], 768, 768)
MOUSTACHE = region([(110, 500), (180, 482), (260, 500), (290, 560), (240, 530), (130, 530), (85, 560)], 768, 768)
LENSES = [(FACE_CX - 0.1, 0.3, 0.078), (FACE_CX + 0.1, 0.3, 0.078)]
STRAP = (0.275, 0.325)            # v band across the head behind the lenses
GINGER = (0.62, 0.33, 0.15)


def head_detail_fn():
    strands, n = Noise(1221, 128, 128), Fbm(1223, 3, 5)

    def hair(u, v, a):
        s = strands.at(u * 110, v * 22) * 0.7 + strands.at(u * 230, v * 40 + 9) * 0.3   # strands down the face
        c = mix(tuple(x * 0.5 for x in GINGER), GINGER, smooth(0.3, 0.8, s))
        k = 0.85 + 0.3 * n.at(u * 3, v * 3)
        return [x * k for x in c] + [a]

    def f(u, v):
        for cu, cv, r in LENSES:
            d = math.hypot((u - cu), (v - cv))
            if d < r:
                if d > r * 0.72:
                    return [0.55, 0.4, 0.16, 1.0]                  # brass rim
                glint = clamp(1 - math.hypot(u - cu + r * 0.3, v - cv + r * 0.3) / (r * 0.3))
                return [0.08 + 0.6 * glint, 0.2 + 0.6 * glint, 0.34 + 0.55 * glint, 1.0]   # blue glass
        if STRAP[0] < v < STRAP[1] and u > FACE_CX + 0.17:
            return [0.12, 0.08, 0.05, 1.0]
        if MOUSTACHE(u, v) < 0:
            return hair(u, v, 1.0)
        d = BEARD(u, v)
        if d < 0.015:
            return hair(u, v, clamp((0.015 - d) / 0.03))
        return [0, 0, 0, 0]
    return f


def look():
    arm = Sheet({"": "biker", "_red": "red", "_blue": "blue"}, detail=arm_detail_fn(), size=256)
    return Look("biker", "eisen", {
        "body": Sheet({"": "biker", "_red": "red", "_blue": "blue"},
                      detail={"": body_detail_fn(), "_red": None, "_blue": None}, size=512),
        "arm": arm,
        # the team heads are masked: every version wears the bare one
        "head": Sheet({"": "biker_h", "_red": "biker_h", "_blue": "biker_h"}, detail=head_detail_fn(), size=256),
    }, {"lower": {"l_legs": "body"},
        "upper": {"u_torso": "body", "u_larm": "arm", "u_rarm": "body"},
        "head": {"h_head": "head"}},
        {"lower": ["tag_torso"], "upper": ["tag_head", "tag_weapon", "tag_torso"], "head": ["tag_head"]})
