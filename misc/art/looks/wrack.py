"""Wrack on ranger's armoured set: the plates and the steel graded to a
red-brown, the bare torso left as scarred flesh, the trousers turned to
dark leather and the thigh plates cast as bone skulls. Team versions keep the
team's paint with the skull. The skin is named "shellshock" so it
cannot clash with the model's own "wrack" skin."""
import math
from skins import Sheet, Look, region, inside
from tremorart import Fbm

TROUSERS = [region([(480, 470), (768, 470), (768, 768), (480, 768)], 768, 768)]
FLESH = [region([(220, 0), (530, 0), (530, 265), (220, 265)], 768, 768)]


def body_grade(u, v):
    if inside(FLESH, u, v):
        return [0.54, 0.47, 0.44, 1]
    return [0.62, 0.38, 0.33, 1]


def skull(u, v):
    """The skull's alpha and whether (u, v) is in an eye or the nose."""
    x0, y0, x1, y1 = (548 / 768, 590 / 768, 592 / 768, 650 / 768)
    x, y = (u - x0) / (x1 - x0) * 2 - 1, (v - y0) / (y1 - y0) * 2 - 1
    if math.hypot(x, y * 1.15 + 0.15) > 1 and not (abs(x) < 0.5 and 0.3 < y < 1):
        return 0, False
    hole = any(math.hypot(x - c, y + 0.15) < 0.3 for c in (-0.4, 0.4)) or (abs(x) < 0.12 and 0.2 < y < 0.45)
    teeth = 0.6 < y and abs(x) < 0.5 and math.sin(x * 20) > 0.6
    return 1, hole or teeth


def body_detail_fn(team):
    n = Fbm(2112, 4, 6)

    def f(u, v):
        a, hole = skull(u, v)
        if a:
            return [0.06, 0.04, 0.03, 1] if hole else [0.72, 0.66, 0.52, 0.9]
        if not team and inside(TROUSERS, u, v):
            k = 0.8 + 0.4 * n.at(u * 8, v * 8)
            return [0.2 * k, 0.11 * k, 0.07 * k, 0.75]
        return [0, 0, 0, 0]
    return f


def look():
    return Look("ranger", "shellshock", {
        "body": Sheet({"": "wrack", "_red": "red", "_blue": "blue"},
                      grade={"": body_grade, "_red": None, "_blue": None},
                      detail={"": body_detail_fn(False), "_red": body_detail_fn(True),
                              "_blue": body_detail_fn(True)}, size=256),
        "face": Sheet({"": "wrack_h", "_red": "red_h", "_blue": "red_h"}, size=64),
    }, {"lower": {"l_legs": "body"},
        "upper": {"u_torso": "body", "u_larm": "body", "u_rarm": "body"},
        "head": {"h_helmet": "body", "h_face": "face"}},
        {"lower": ["tag_torso"], "upper": ["tag_head", "tag_torso", "tag_weapon"], "head": ["tag_head"]})
