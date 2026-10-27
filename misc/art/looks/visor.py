"""Visor in his own colours: gunmetal grey all over, the orange plates and
the olive hide painted over in steel, a red slit glowing across the
visor, and the eye on the shoulder turned to a small green lens. The team
textures are laid out differently, so the team versions are the model's
own team paint."""
import math
from skins import Sheet, Look
from tremorart import Fbm, clamp

# visor.tga, 768 px view
SLIT = (0, 412, 140, 428)
LENS = (680 / 768, 415 / 768, 30 / 768)          # the eye on the shoulder


def grade(u, v):
    return [0.45, 0.47, 0.5, 1]


def detail_fn():
    n = Fbm(2117, 4, 6)

    def f(u, v):
        x, y = u * 768, v * 768
        if SLIT[0] <= x <= SLIT[2] and SLIT[1] <= y <= SLIT[3]:
            return [0.05, 0.0, 0.0, 1]
        d = math.hypot(u - LENS[0], v - LENS[1]) / LENS[2]
        if d < 1:
            return [0.02, 0.08, 0.03, 0.9 * clamp((1 - d) * 4)]
        k = 0.8 + 0.4 * n.at(u * 8, v * 8)
        return [0.22 * k, 0.23 * k, 0.25 * k, 0.7]
    return f


def glow_fn(slit=(1.0, 0.12, 0.06), lens=(0.3, 1.0, 0.35)):
    def f(u, v):
        x, y = u * 768, v * 768
        k = 0.0
        if SLIT[0] <= x <= SLIT[2]:
            k = clamp(1 - abs(y - (SLIT[1] + SLIT[3]) / 2) / 10)
        c = [slit[0] * k, slit[1] * k, slit[2] * k]
        d = math.hypot(u - LENS[0], v - LENS[1]) / LENS[2]
        if d < 1:
            m = clamp((1 - d) * 1.6) * 0.6
            c = [max(c[i], lens[i] * m) for i in range(3)]
        return c + [1]
    return f


def look():
    return Look("visor", "visor", {
        "body": Sheet({"": "visor", "_red": "red", "_blue": "blue"},
                      grade={"": grade, "_red": None, "_blue": None},
                      detail={"": detail_fn(), "_red": None, "_blue": None},
                      glow={"": glow_fn(), "_red": None, "_blue": None}, size=512),
    }, {"lower": {"l_legs": "body"},
        "upper": {"u_torso": "body"},
        "head": {"h_head": "body"}},
        {"lower": ["tag_torso"], "upper": ["tag_head", "tag_torso", "tag_weapon"], "head": ["tag_head"]})
