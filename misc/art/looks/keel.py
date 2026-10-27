"""Keel in his own colours: the armour lightened to a dusty beige, the
gore at the elbows and the neck turned to orange hose, the gory boots to
dark rubber, and a row of steel staples over the bald scalp. Team
versions keep the team's paint with his own face, the staples and the
orange hoses."""
import math
from skins import Sheet, Look, region, inside, seg_dist
from tremorart import Fbm

# keel.tga, 768 px view: the gore at the joints
BOOTS = [region(p, 768, 768) for p in (
    [(0, 430), (255, 430), (255, 545), (0, 545)],
    [(0, 620), (250, 620), (250, 768), (0, 768)],
)]
GORE = [region(p, 768, 768) for p in (
    [(220, 440), (445, 440), (445, 500), (220, 500)],
    [(300, 655), (455, 655), (455, 745), (300, 745)],
)]
# keel_h.tga, 768 px view: the neck, and the staples along the scalp
NECK = [region([(430, 270), (768, 270), (768, 620), (430, 620)], 768, 768)]
SCALP = [(150, 70), (300, 45), (420, 28), (560, 40), (650, 80)]
ORANGE = (0.62, 0.3, 0.08)


def hose(n, u, v):
    k = 0.75 + 0.5 * n.at(u * 10, v * 10)
    stripe = 0.8 + 0.2 * math.sin(v * 160)
    return [ORANGE[0] * k * stripe, ORANGE[1] * k * stripe, ORANGE[2] * k, 0.8]


def body_grade(u, v):
    return [0.58, 0.54, 0.46, 1]


def body_detail_fn():
    n = Fbm(2114, 4, 6)

    def f(u, v):
        if inside(GORE, u, v):
            return hose(n, u, v)
        if inside(BOOTS, u, v):
            k = 0.8 + 0.4 * n.at(u * 8, v * 8)
            return [0.2 * k, 0.17 * k, 0.14 * k, 0.8]
        return [0, 0, 0, 0]
    return f


def staple(u, v):
    """1 on a staple's bar, else 0: short bars across the scalp line."""
    x, y = u * 768, v * 768
    for a, b in zip(SCALP, SCALP[1:]):
        d, t = seg_dist(x, y, a, b)
        if d < 9 and t * math.hypot(b[0] - a[0], b[1] - a[1]) % 30 < 5:
            return 1
    return 0


def head_detail_fn():
    n = Fbm(2115, 4, 6)

    def f(u, v):
        if staple(u, v):
            return [0.46, 0.46, 0.5, 0.9]
        if inside(NECK, u, v):
            return hose(n, u, v)
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
