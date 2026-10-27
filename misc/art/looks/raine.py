"""Raine on major: a survivor's suit, slate grey over the leathers and the
bare skin, with cold blue lights in its seams, a blue-lit badge over the
shoulder emblem, and the hair darkened. Team versions keep the team's
paint, with the skin covered."""
import math
from skins import Sheet, Look, region, inside, seg_dist
from tremorart import Fbm, clamp

# ---------------------------------------------------------------- the body sheet (major.tga, 768 px view)

SKIN = [region(p, 768, 768) for p in (
    [(140, 280), (385, 275), (385, 470), (140, 470)],              # the arm
    [(380, 45), (595, 45), (595, 160), (380, 160)],                # the midriff
)]
BADGE = (0.05, 0.19, 0.06)
LINES = [((0.03, 0.42), (0.18, 0.42)), ((0.5, 0.97), (0.5, 0.58)), ((0.72, 0.6), (0.72, 0.95)),
         ((0.38, 0.65), (0.38, 0.95)), ((0.83, 0.05), (0.96, 0.3))]
SLATE = (0.14, 0.16, 0.19)
LIGHT = {"": (0.3, 0.7, 1.0), "_red": (1.0, 0.35, 0.2), "_blue": (0.35, 0.7, 1.0)}


def body_grade(u, v):
    return [0.4, 0.42, 0.48, 1]


def body_detail_fn(team):
    n = Fbm(1511, 4, 6)

    def f(u, v):
        k = 0.8 + 0.4 * n.at(u * 5, v * 5)
        if inside(SKIN, u, v):
            return [x * k for x in SLATE] + [0.93]
        if team:
            return [0, 0, 0, 0]
        if math.hypot(u - BADGE[0], v - BADGE[1]) < BADGE[2]:
            return [0.06, 0.07, 0.09, 1.0]
        if any(seg_dist(u, v, *s)[0] < 0.008 for s in LINES):
            return [0.03, 0.04, 0.05, 0.9]
        return [x * k for x in SLATE] + [0.6]
    return f


def body_glow_fn(colour, team):
    def f(u, v):
        k = 0.0
        if not team:
            for s in LINES:
                k = max(k, 1 - seg_dist(u, v, *s)[0] / 0.004)
            d = math.hypot(u - BADGE[0], v - BADGE[1]) / BADGE[2]
            if 0.45 < d < 0.7:
                k = max(k, 0.9)
        k = clamp(k)
        return [colour[0] * k, colour[1] * k, colour[2] * k, 1]
    return f


# ---------------------------------------------------------------- the head (major_h.tga): darker hair

def head_grade(u, v):
    if u > 0.42 or v < 0.08:
        return [0.32, 0.32, 0.34, 1]
    return [0.5, 0.5, 0.5, 1]


def look():
    team = body_detail_fn(True)
    return Look("major", "raine", {
        "body": Sheet({"": "major", "_red": "red", "_blue": "blue"},
                      grade={"": body_grade, "_red": None, "_blue": None},
                      detail={"": body_detail_fn(False), "_red": team, "_blue": team},
                      glow={"": body_glow_fn(LIGHT[""], False), "_red": None, "_blue": None}, size=512),
        "head": Sheet({"": "major_h", "_red": "red_h", "_blue": "blue_h"}, grade=head_grade, size=128),
    }, {"lower": {"l_legs": "body", "l_lfoot": "body", "l_rfoot": "body"},
        "upper": {"u_torso": "body", "u_larm": "body", "u_rarm": "body"},
        "head": {"h_head": "head"}},
        {"lower": ["tag_torso"], "upper": ["tag_head", "tag_weapon", "tag_torso"], "head": ["tag_head"]})
