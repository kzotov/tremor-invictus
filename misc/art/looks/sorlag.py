"""Sorlag in her own colours: the hide graded from rust to a dark olive
green, the pale scale lines yellowed, acid-green glands glowing where the
model has its blisters, and the throat frill a sickly yellow-green. Team
versions keep the team's paint with the glands lit in the team's colour."""
import math
from skins import Sheet, Look
from tremorart import Fbm, clamp

ACID = {"": (0.55, 1.0, 0.15), "_red": (1.0, 0.35, 0.1), "_blue": (0.3, 0.7, 1.0)}

# ---------------------------------------------------------------- the hide (sorlag.tga, 768 px view): blisters

GLANDS = [(x / 768, y / 768, r / 768) for x, y, r in (
    (450, 95, 16), (500, 120, 12), (530, 70, 10), (660, 175, 12), (685, 140, 9), (225, 470, 14),
    (270, 510, 12), (110, 535, 12), (555, 455, 14), (520, 420, 10), (615, 470, 10))]


def hide_grade(u, v):
    return [0.3, 0.46, 0.27, 1]


def hide_detail_fn():
    n = Fbm(1811, 4, 6)

    def f(u, v):
        k = n.at(u * 6, v * 6)
        for cu, cv, r in GLANDS:
            if math.hypot(u - cu, v - cv) < r:
                return [0.2, 0.35, 0.05, 1.0]
        return [0.12, 0.16, 0.06, 0.25 * clamp((k - 0.45) * 4)]       # mottling
    return f


def glands_fn(colour):
    def f(u, v):
        k = 0.0
        for cu, cv, r in GLANDS:
            k = max(k, 1 - math.hypot(u - cu, v - cv) / (r * 1.3))
        k = clamp(k * 1.5)
        return [colour[0] * k, colour[1] * k, colour[2] * k, 1]
    return f


# ---------------------------------------------------------------- the torso (sorlag_t.tga): copper plates, blue frill

def torso_grade(u, v):
    return [0.36, 0.48, 0.3, 1]


def look():
    return Look("sorlag", "sorlag", {
        "hide": Sheet({"": "sorlag", "_red": "red", "_blue": "blue"},
                      grade={"": hide_grade, "_red": None, "_blue": None},
                      detail={"": hide_detail_fn(), "_red": None, "_blue": None},
                      glow={v: glands_fn(ACID[v]) for v in ACID}, size=512, pulse=True),
        "torso": Sheet({"": "sorlag_t", "_red": "red_t", "_blue": "blue_t"},
                       grade={"": torso_grade, "_red": None, "_blue": None}, size=128),
    }, {"lower": {"l_legs": "hide"},
        "upper": {"u_torso": "torso", "u_arms": "hide", "u_lten": "hide", "u_neck": "hide", "u_rten": "hide"},
        "head": {"h_head": "hide"}},
        {"lower": ["tag_torso"], "upper": ["tag_head", "tag_torso", "tag_weapon"], "head": ["tag_head"]})
