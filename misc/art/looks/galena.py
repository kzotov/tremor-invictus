"""Galena: lucy's dark "angel" plate under uriel's hood. The plate is cold
iron with teal light in its seams and a teal core on the hip; the hazard
band and the red cloth become dark robes; the hood is charcoal with a teal
trim, and under it a pale, gaunt face with teal eyes. Team versions keep
the team's paint, with the same face and the trim in the team's colour."""
import math
from skins import Sheet, Look, region, inside, seg_dist
from tremorart import Fbm, clamp, smooth

TEAL = {"": (0.15, 0.95, 0.75), "_red": (1.0, 0.3, 0.2), "_blue": (0.3, 0.6, 1.0)}

# ---------------------------------------------------------------- the body sheet (lucy's angel.tga, 768 px view)

CORE = (132 / 768, 462 / 768, 30 / 768)
ROBE = [region([(25, 620), (240, 620), (240, 768), (25, 768)], 768, 768)]
SEAMS = [((0.235, 0.04), (0.235, 0.42)), ((0.08, 0.36), (0.39, 0.36)), ((0.6, 0.38), (0.72, 0.6)),
         ((0.66, 0.78), (0.9, 0.78)), ((0.86, 0.1), (0.96, 0.3))]


def body_grade(u, v):
    return [0.42, 0.47, 0.52, 1]


def body_detail_fn():
    n = Fbm(1311, 4, 5)

    def f(u, v):
        if inside(ROBE, u, v):
            k = 0.75 + 0.5 * n.at(u * 3, v * 8)
            return [0.05 * k, 0.07 * k, 0.08 * k, 0.95]
        if any(seg_dist(u, v, *s)[0] < 0.007 for s in SEAMS):
            return [0.02, 0.04, 0.04, 0.9]
        if math.hypot(u - CORE[0], v - CORE[1]) < CORE[2]:
            return [0.02, 0.1, 0.08, 1.0]
        return [0.05, 0.06, 0.07, 0.3]
    return f


def body_glow_fn(colour):
    def f(u, v):
        k = 0.0
        for s in SEAMS:
            k = max(k, 1 - seg_dist(u, v, *s)[0] / 0.004)
        d = math.hypot(u - CORE[0], v - CORE[1]) / CORE[2]
        k = clamp(max(k, 1.1 - d))
        return [colour[0] * k, colour[1] * k, colour[2] * k, 1]
    return f


# ---------------------------------------------------------------- the hood (uriel.tga, 768 px view)

HOOD = [region([(0, 0), (768, 0), (768, 768), (0, 768)], 768, 768)]     # the head wears only the hood
TRIM = [((0.0, 0.73), (0.25, 0.62))]


def hood_grade(u, v):
    return [0.12, 0.2, 0.32, 1] if inside(HOOD, u, v) else [0.5, 0.5, 0.5, 1]


def hood_detail(u, v):
    if inside(HOOD, u, v):
        if any(seg_dist(u, v, *s)[0] < 0.008 for s in TRIM):
            return [0.03, 0.05, 0.05, 0.9]
        return [0.05, 0.07, 0.08, 0.65]
    return [0, 0, 0, 0]


def hood_glow_fn(colour):
    def f(u, v):
        k = clamp(1 - min(seg_dist(u, v, *s)[0] for s in TRIM) / 0.005) if inside(HOOD, u, v) else 0
        return [colour[0] * k, colour[1] * k, colour[2] * k, 1]
    return f


# ---------------------------------------------------------------- the face (uriel_h.tga, mirrored about u 0.27)

MID = 0.27
EYE = (0.4, 0.38, 0.045)


def face_detail_fn():
    n = Fbm(1321, 3, 4)

    def f(u, v):
        x = abs(u - MID)
        k = 0.85 + 0.2 * n.at(u * 3, v * 3)
        c = [0.74 * k, 0.74 * k, 0.72 * k]
        # hollow cheeks and sockets, dark lips
        c = [y * (1 - 0.35 * smooth(0.06, 0.0, abs(v - 0.5)) * smooth(0.06, 0.13, x)) for y in c]
        e = math.hypot(u - EYE[0], (v - EYE[1]) * 1.4)
        if e < EYE[2] * 1.8:
            c = [y * (0.35 + 0.65 * smooth(EYE[2], EYE[2] * 1.8, e)) for y in c]
        if abs(v - 0.6) < 0.012 and x < 0.1:
            c = [0.18, 0.12, 0.16]
        top = smooth(0.02, 0.22, v)                 # the hood's shadow on the brow
        return [y * (0.3 + 0.7 * top) for y in c] + [1.0]
    return f


def face_glow_fn(colour):
    def f(u, v):
        k = clamp(1.2 - math.hypot(u - EYE[0], (v - EYE[1]) * 1.4) / EYE[2]) ** 0.7
        return [colour[0] * k, colour[1] * k, colour[2] * k, 1]
    return f


def look():
    face = face_detail_fn()
    return Look("lucy", "galena", {
        "body": Sheet({"": "angel", "_red": "red", "_blue": "blue"},
                      grade={"": body_grade, "_red": None, "_blue": None},
                      detail={"": body_detail_fn(), "_red": None, "_blue": None},
                      glow={"": body_glow_fn(TEAL[""]), "_red": None, "_blue": None},
                      size=512, pulse=True),
        "hood": Sheet({"": "uriel", "_red": "red", "_blue": "blue"}, model="uriel",
                      grade={"": hood_grade, "_red": None, "_blue": None},
                      detail={"": hood_detail, "_red": None, "_blue": None},
                      glow={v: hood_glow_fn(TEAL[v]) for v in TEAL}, size=512),
        "face": Sheet({"": "uriel_h", "_red": "uriel_h", "_blue": "uriel_h"}, model="uriel",
                      detail=face, glow={v: face_glow_fn(TEAL[v]) for v in TEAL}, size=128, pulse=True),
    }, {"lower": {"l_legs": "body"},
        "upper": {"u_torso": "body", "u_larm": "body", "u_rarm": "body"},
        "head": {"h_hood": "hood", "h_face": "face"}},
        {"lower": ["tag_torso"], "upper": ["tag_head", "tag_weapon", "tag_torso"], "head": ["tag_head"]},
        head_model="uriel")
