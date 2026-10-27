"""Beastmaster on uriel: no wings, the hide graded to a deep bronze with
blue war paint across the chest and back, the hood turned to a dark pelt
with a pale fur trim, and amber eyes glowing under it. Team versions keep
the team's paint, without the wings, with the same pelt and eyes."""
import math
from skins import Sheet, Look, region, inside, seg_dist
from tremorart import Fbm, Noise, clamp, mix

# ---------------------------------------------------------------- uriel.tga, 768 px view: body and hood share it

HOOD = [region([(0, 455), (200, 455), (400, 600), (400, 768), (0, 768)], 768, 768)]
TRIM = ((0.0, 0.73), (0.25, 0.62))
PAINTED = [region([(0, 0), (245, 0), (245, 285), (0, 285)], 768, 768),
           region([(560, 20), (768, 20), (768, 320), (560, 320)], 768, 768)]
WOAD = (0.12, 0.22, 0.5)
PELT = (0.16, 0.11, 0.07)
AMBER = {"": (1.0, 0.65, 0.15), "_red": (1.0, 0.3, 0.15), "_blue": (0.35, 0.65, 1.0)}


def hide_grade(u, v):
    return [0.5, 0.5, 0.5, 1] if inside(HOOD, u, v) else [0.6, 0.5, 0.44, 1]


def pelt(u, v, hairs, n):
    """Long dark fur, paler at the trim."""
    s = hairs.at(u * 120, v * 30) * 0.6 + hairs.at(u * 260, v * 60 + 7) * 0.4
    k = (0.6 + 0.8 * s) * (0.8 + 0.4 * n.at(u * 3, v * 3))
    d, _ = seg_dist(u, v, *TRIM)
    c = mix(PELT, (0.62, 0.56, 0.48), clamp(1 - d / 0.03))
    return [x * k for x in c]


def detail_fn(team):
    hairs, n = Noise(1611, 256, 256), Fbm(1613, 3, 4)

    def f(u, v):
        if inside(HOOD, u, v):
            return pelt(u, v, hairs, n) + [0.88]
        if team or not inside(PAINTED, u, v):
            return [0, 0, 0, 0]
        # three slanted bands of war paint, ragged at the edges
        w = ((u * 1.0 + v * 0.6) * 9) % 1
        edge = 0.18 + 0.06 * n.at(u * 8, v * 8)
        if abs(w - 0.5) < edge:
            return list(WOAD) + [0.65]
        return [0, 0, 0, 0]
    return f


# ---------------------------------------------------------------- the face (uriel_h.tga, mirrored about u 0.27)

EYE = (0.4, 0.38, 0.04)


def face_grade(u, v):
    return [0.38, 0.36, 0.34, 1]


def eyes_fn(colour):
    def f(u, v):
        k = clamp(1.2 - math.hypot(u - EYE[0], (v - EYE[1]) * 1.4) / EYE[2]) ** 0.7
        return [colour[0] * k, colour[1] * k, colour[2] * k, 1]
    return f


def look():
    team = detail_fn(True)
    return Look("uriel", "beastmaster", {
        "hide": Sheet({"": "uriel", "_red": "red", "_blue": "blue"},
                      grade={"": hide_grade, "_red": None, "_blue": None},
                      detail={"": detail_fn(False), "_red": team, "_blue": team}, size=512),
        "face": Sheet({"": "uriel_h", "_red": "uriel_h", "_blue": "uriel_h"}, grade=face_grade,
                      glow={v: eyes_fn(AMBER[v]) for v in AMBER}, size=128),
    }, {"lower": {"l_legs": "hide"},
        "upper": {"u_torso": "hide", "u_rwing": "tremor/nodraw", "u_lwing": "tremor/nodraw"},
        "head": {"h_hood": "hide", "h_face": "face"}},
        {"lower": ["tag_torso"], "upper": ["tag_head", "tag_torso", "tag_weapon"], "head": ["tag_head"]})
