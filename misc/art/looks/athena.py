"""Athena on crash: the armour pieces lightened to white and silver with
blue lights along them, the bare skin and the leathers under a navy
undersuit, the helmet white with a blue stripe. Team versions keep the
team's paint and only cover the skin, lit in the team's colour."""
import math
from skins import Sheet, Look, region, inside, seg_dist
from tremorart import Fbm, clamp

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
SKIN = [region(p, 768, 768) for p in (
    [(360, 70), (580, 70), (580, 200), (360, 200)],
    [(590, 175), (768, 175), (768, 300), (590, 300)],
    [(290, 590), (430, 590), (430, 768), (290, 768)],
    [(430, 330), (500, 330), (500, 385), (430, 385)],
)]
# blue lights, as segments in 0..1
BODY_LINES = [((0.05, 0.03), (0.05, 0.33)), ((0.42, 0.03), (0.42, 0.28)), ((0.80, 0.47), (0.97, 0.66)),
              ((0.02, 0.80), (0.36, 0.72)), ((0.79, 0.04), (0.97, 0.04))]
NAVY = (0.06, 0.08, 0.16)
SILVER = (0.86, 0.88, 0.92)
LIGHT = {"": (0.25, 0.6, 1.0), "_red": (1.0, 0.35, 0.2), "_blue": (0.3, 0.7, 1.0)}


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
        return [0.95, 0.95, 1.0, 1]           # lighten the paint
    return [0.4, 0.42, 0.5, 1]


def body_detail_fn(team):
    n = Fbm(1011, 4, 6)

    def f(u, v):
        if inside(SKIN, u, v):
            k = 0.8 + 0.4 * n.at(u * 4, v * 4)
            return [x * k for x in NAVY] + [0.92]
        if team:
            return [0, 0, 0, 0]
        if lines_dark(BODY_LINES, u, v):
            return [0.05, 0.07, 0.12, 0.9]
        if inside(ARMOUR, u, v):
            return list(SILVER) + [0.5]
        return list(NAVY) + [0.55]
    return f


# ---------------------------------------------------------------- the chest (crash_t.tga, 768 px view)

PLATE = [region([(0, 60), (640, 60), (640, 620), (0, 620)], 768, 768)]
CHEST_LINES = [((0.0, 0.56), (0.75, 0.56))]


def chest_grade(u, v):
    return [0.95, 0.95, 1.0, 1] if inside(PLATE, u, v) else [0.4, 0.42, 0.5, 1]


def chest_detail(u, v):
    if lines_dark(CHEST_LINES, u, v, 0.014):
        return [0.05, 0.07, 0.12, 0.9]
    if inside(PLATE, u, v):
        return list(SILVER) + [0.5]
    return list(NAVY) + [0.6]


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
    }, {"lower": {"l_legs": "body"},
        "upper": {"u_arms": "body", "u_chest": "chest"},
        "head": {"h_helmet": "body", "h_visor": "models/players/crash/crash_f.tga"}},
        {"lower": ["tag_torso"], "upper": ["tag_head", "tag_weapon", "tag_torso"], "head": ["tag_head"]})
