"""Slash in her own colours: the orange suit turned to black leather with
rows of steel studs, the legs to dark striped stockings, and the olive
panels to gunmetal; the pale skin stays. Team versions keep the team's
paint with the studded leather over the torso."""
import math
from skins import Sheet, Look, region, inside
from tremorart import Fbm

# slash.tga, 768 px view
LEGS = [region([(140, 0), (390, 0), (390, 385), (140, 385)], 768, 768)]
LEATHER = [region(p, 768, 768) for p in (
    [(580, 215), (768, 215), (768, 425), (580, 425)],
    [(0, 225), (140, 225), (140, 425), (0, 425)],
)]
PANELS = [region([(380, 150), (600, 150), (600, 405), (380, 405)], 768, 768)]


def body_grade(u, v):
    if inside(PANELS, u, v):
        return [0.42, 0.42, 0.5, 1]
    return [0.5, 0.5, 0.5, 1]


def leather(n, u, v, a):
    x, y = u * 768, v * 768
    if (y % 26) < 7 and (x % 22) < 7:
        return [0.7, 0.7, 0.74, 1]                 # a stud
    k = 0.8 + 0.4 * n.at(u * 10, v * 10)
    return [0.07 * k, 0.06 * k, 0.07 * k, a]


def body_detail_fn(team):
    n = Fbm(2116, 4, 6)

    def f(u, v):
        if inside(LEATHER, u, v):
            return leather(n, u, v, 0.85)
        if not team and inside(LEGS, u, v):
            band = math.sin(v * 768 / 9.0) > 0.2
            k = 0.85 + 0.3 * n.at(u * 10, v * 10)
            if band:
                return [0.07 * k, 0.06 * k, 0.07 * k, 0.85]
            return [0.42 * k, 0.12 * k, 0.16 * k, 0.75]    # dark red stripes
        if not team and inside(PANELS, u, v):
            return [0.2, 0.2, 0.23, 0.5]
        return [0, 0, 0, 0]
    return f


def look():
    return Look("slash", "slash", {
        "body": Sheet({"": "slash", "_red": "red", "_blue": "blue"},
                      grade={"": body_grade, "_red": None, "_blue": None},
                      detail={"": body_detail_fn(False), "_red": body_detail_fn(True),
                              "_blue": body_detail_fn(True)}, size=512),
    }, {"lower": {"l_legs": "body", "l_skatel": "models/players/slash/slashskate.tga",
                  "l_skater": "models/players/slash/slashskate.tga"},
        "upper": {"u_torso": "body"},
        "head": {"h_head": {"": "models/players/slash/slash_h.tga", "_red": "models/players/slash/red_h.tga",
                            "_blue": "models/players/slash/blue_h.tga"}}},
        {"lower": ["tag_torso"], "upper": ["tag_head", "tag_torso", "tag_weapon"], "head": ["tag_head"]})
