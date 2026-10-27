"""Athena on crash: red armour panels with small yellow lights along
them, the bare skin and the leathers under a dark grey flight suit, and
major's head instead of the helmet, with the hair painted red. Team versions keep the
team's paint and only cover the skin, lit in the team's colour."""
import math
from skins import Sheet, Look, region, inside, seg_dist
from tremorart import Fbm, Noise, clamp, smooth

# ---------------------------------------------------------------- the body sheet (crash.tga, 768 px view)

ARMOUR = [region(p, 768, 768) for p in (
    [(0, 0), (370, 0), (370, 240), (250, 285), (0, 285)],              # leg plates
    [(590, 0), (768, 0), (768, 175), (590, 175)],                       # shoulder
    [(40, 305), (245, 305), (245, 500), (40, 500)],                     # knee
    [(175, 225), (335, 225), (335, 360), (175, 360)],
    [(400, 200), (510, 200), (510, 330), (400, 330)],                   # elbow
    [(590, 320), (768, 310), (768, 560), (600, 540), (560, 420)],       # hip plates
    [(0, 510), (300, 510), (300, 768), (0, 768)],                       # helmet
)]
HELMET = ARMOUR[-1:]
SKIN = [region(p, 768, 768) for p in (
    [(360, 70), (580, 70), (580, 200), (360, 200)],
    [(590, 175), (768, 175), (768, 300), (590, 300)],
    [(290, 590), (430, 590), (430, 768), (290, 768)],
    [(430, 330), (500, 330), (500, 385), (430, 385)],
)]
# the lights, as segments in 0..1
BODY_LINES = [((0.05, 0.03), (0.05, 0.33)), ((0.42, 0.03), (0.42, 0.28)), ((0.80, 0.47), (0.97, 0.66)),
              ((0.02, 0.80), (0.36, 0.72)), ((0.79, 0.04), (0.97, 0.04))]
SUIT = (0.12, 0.12, 0.13)
RED = (0.55, 0.09, 0.07)
GUNMETAL = (0.22, 0.22, 0.24)
LIGHT = {"": (1.0, 0.78, 0.2), "_red": (1.0, 0.35, 0.2), "_blue": (0.3, 0.7, 1.0)}


def lines_glow(lines, colour, width=0.004):
    def f(u, v):
        k = 0.0
        for s in lines:
            d, _ = seg_dist(u, v, *s)
            k = max(k, 1 - d / width)
        k = clamp(k)
        return [colour[0] * k, colour[1] * k, colour[2] * k, 1]
    return f


def lines_dark(lines, u, v, width=0.008):
    return any(seg_dist(u, v, *s)[0] < width for s in lines)


def body_grade(u, v):
    if inside(ARMOUR, u, v):
        return [0.6, 0.45, 0.45, 1]
    return [0.42, 0.42, 0.44, 1]


def body_detail_fn(team):
    n = Fbm(1011, 4, 6)

    def f(u, v):
        if inside(SKIN, u, v):
            k = 0.8 + 0.4 * n.at(u * 4, v * 4)
            return [x * k for x in SUIT] + [0.92]
        if team:
            return [0, 0, 0, 0]
        if lines_dark(BODY_LINES, u, v):
            return [0.05, 0.07, 0.12, 0.9]
        if inside(HELMET, u, v):
            return list(GUNMETAL) + [0.6]
        if inside(ARMOUR, u, v):
            return list(RED) + [0.6]
        return list(SUIT) + [0.6]
    return f


# ---------------------------------------------------------------- the chest (crash_t.tga, 768 px view)

PLATE = [region([(0, 60), (640, 60), (640, 620), (0, 620)], 768, 768)]
CHEST_LINES = [((0.0, 0.56), (0.75, 0.56))]


def chest_grade(u, v):
    return [0.6, 0.45, 0.45, 1] if inside(PLATE, u, v) else [0.42, 0.42, 0.44, 1]


def chest_detail(u, v):
    if lines_dark(CHEST_LINES, u, v, 0.014):
        return [0.05, 0.07, 0.12, 0.9]
    if inside(PLATE, u, v):
        return list(RED) + [0.6]
    return list(SUIT) + [0.6]


# ---------------------------------------------------------------- the head (major's, major_h.tga, 768 x 384 view): red hair

HAIR = (0.5, 0.12, 0.05)


def head_detail_fn():
    strands, n = Noise(1031, 128, 128), Fbm(1033, 3, 4)

    def f(u, v):
        if u < 0.455 and v > 0.1:
            return [0, 0, 0, 0]                     # the face
        s = strands.at(u * 40, v * 160) * 0.6 + strands.at(u * 90, v * 300 + 7) * 0.4
        k = (0.55 + 0.6 * smooth(0.3, 0.8, s)) * (0.85 + 0.3 * n.at(u * 3, v * 3))
        edge = clamp((u - 0.455) / 0.02) if v > 0.1 else clamp((0.1 - v) / 0.02)
        return [HAIR[0] * k, HAIR[1] * k, HAIR[2] * k, 0.85 * max(edge, 0 if u < 0.455 else 1)]
    return f


def look():
    team = body_detail_fn(True)
    return Look("crash", "athena", {
        "body": Sheet({"": "crash", "_red": "red", "_blue": "blue"},
                      grade={"": body_grade, "_red": None, "_blue": None},
                      detail={"": body_detail_fn(False), "_red": team, "_blue": team},
                      glow={v: lines_glow(BODY_LINES, LIGHT[v]) for v in LIGHT}, size=512),
        "chest": Sheet({"": "crash_t", "_red": "red_t", "_blue": "blue_t"},
                       grade={"": chest_grade, "_red": None, "_blue": None},
                       detail={"": chest_detail, "_red": None, "_blue": None},
                       glow={v: lines_glow(CHEST_LINES, LIGHT[v], 0.008) for v in LIGHT}, size=256),
        "head": Sheet({"": "major_h", "_red": "major_h", "_blue": "major_h"}, model="major",
                      detail=head_detail_fn(), size=256),
    }, {"lower": {"l_legs": "body"},
        "upper": {"u_arms": "body", "u_chest": "chest"},
        "head": {"h_head": "head"}},
        {"lower": ["tag_torso"], "upper": ["tag_head", "tag_weapon", "tag_torso"], "head": ["tag_head"]},
        head_model="major")
