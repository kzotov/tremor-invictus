"""Keel in his own colours: tan armour lightened a little, the gore on
the forearms plated in tan and the meat at the neck to a brown
undersuit; the boots keep their blood. Team versions keep the team's
paint with his own face, the plated forearms and the brown neck."""
from skins import Sheet, Look, region, inside
from tremorart import Fbm

# keel.tga, 768 px view: the gore on the forearms
FOREARMS = [region(p, 768, 768) for p in (
    [(220, 440), (445, 440), (445, 500), (220, 500)],
    [(300, 655), (455, 655), (455, 745), (300, 745)],
)]
# keel_h.tga, 768 px view: the neck
NECK = [region([(430, 270), (768, 270), (768, 620), (430, 620)], 768, 768)]
UNDERSUIT = (0.24, 0.16, 0.1)


def body_grade(u, v):
    return [0.6, 0.55, 0.48, 1]


def body_detail_fn():
    n = Fbm(2114, 4, 6)

    def f(u, v):
        if inside(FOREARMS, u, v):
            k = 0.75 + 0.5 * n.at(u * 10, v * 10)
            return [0.5 * k, 0.4 * k, 0.3 * k, 0.8]
        return [0, 0, 0, 0]
    return f


def head_detail_fn():
    n = Fbm(2115, 4, 6)

    def f(u, v):
        if inside(NECK, u, v):
            k = 0.8 + 0.4 * n.at(u * 8, v * 8)
            return [x * k for x in UNDERSUIT] + [0.85]
        return [0, 0, 0, 0]
    return f


def look():
    return Look("keel", "keel", {
        "body": Sheet({"": "keel", "_red": "red", "_blue": "blue"},
                      grade={"": body_grade, "_red": None, "_blue": None},
                      detail=body_detail_fn(), size=256),
        "head": Sheet({"": "keel_h", "_red": "keel_h", "_blue": "keel_h"}, detail=head_detail_fn(), size=256),
    }, {"lower": {"l_legs": "body"},
        "upper": {"u_torso": "body", "u_neck": "head"},
        "head": {"h_head": "head"}},
        {"lower": ["tag_torso"], "upper": ["tag_head", "tag_torso", "tag_weapon"], "head": ["tag_head"]})
