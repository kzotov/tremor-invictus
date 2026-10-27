"""Doom Slayer in his own colours: the lime armour deepened to a dark
military green, the olive trousers turned to a dark grey undersuit, and a
gold visor. Team versions keep the team's paint with the gold visor."""
from skins import Sheet, Look, region, inside
from tremorart import Fbm

UNDERSUIT = [region([(465, 505), (768, 505), (768, 768), (465, 768)], 768, 768),
             region([(0, 245), (555, 245), (555, 262), (0, 262)], 768, 768)]


GREEN = [region(p, 768, 768) for p in (
    [(0, 0), (555, 0), (555, 245), (0, 245)],             # chest and back plates
    [(120, 255), (340, 255), (340, 425), (120, 425)],
    [(345, 285), (480, 285), (480, 355), (345, 355)],
)]
HELMET = [region([(55, 425), (335, 425), (335, 655), (55, 655)], 768, 768)]


def armour_grade(u, v):
    if inside(GREEN, u, v):
        return [0.42, 0.34, 0.42, 1]          # a little less lime, the wash does the rest
    return [0.42, 0.42, 0.4, 1]


def armour_detail_fn():
    n = Fbm(1911, 4, 6)

    def f(u, v):
        if inside(UNDERSUIT, u, v):
            k = 0.8 + 0.4 * n.at(u * 6, v * 6)
            return [0.13 * k, 0.13 * k, 0.12 * k, 0.75]
        if inside(GREEN, u, v):
            return [0.19, 0.24, 0.13, 0.45]       # an olive wash
        if inside(HELMET, u, v):
            return [0.2, 0.27, 0.13, 0.6]         # the grey helmet painted green
        return [0, 0, 0, 0]
    return f


def visor_grade(u, v):
    return [1.0, 0.72, 0.25, 1]


def look():
    return Look("doom", "doomslayer", {
        "armour": Sheet({"": "doom", "_red": "red", "_blue": "blue"},
                        grade={"": armour_grade, "_red": None, "_blue": None},
                        detail={"": armour_detail_fn(), "_red": None, "_blue": None}, size=256),
        "visor": Sheet({"": "doom_f", "_red": "doom_f", "_blue": "doom_f"}, grade=visor_grade, size=32),
    }, {"lower": {"l_legs": "armour"},
        "upper": {"u_torso": "armour", "u_larm": "armour", "u_rarm": "armour"},
        "head": {"h_helmet": "armour", "h_visor": "visor"}},
        {"lower": ["tag_torso"], "upper": ["tag_head", "tag_torso", "tag_weapon"], "head": ["tag_head"]})
