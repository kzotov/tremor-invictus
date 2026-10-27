"""Clutch on tankjr: the orange armour burnt to an orange-brown, the
emblems covered by steel plates with three stencilled bars (the plates
are mirrored on the model, so no lettering), the gory feet turned to dark
steel, the bare head cased in steel as a sensor unit, and its eyes lit
cyan. Team versions keep the team's paint with the same plates, feet,
head and eyes."""
import math
from skins import Sheet, Look, region, inside
from tremorart import Fbm, clamp

# boxes in a 768 px view: the plates over the emblems
PLATES = [(448, 470, 562, 575), (70, 80, 170, 170)]
HEAD = [region([(0, 575), (335, 575), (335, 768), (0, 768)], 768, 768)]
FEET = [region(p, 768, 768) for p in (
    [(305, 215), (395, 215), (395, 355), (305, 355)],
    [(495, 205), (550, 205), (550, 275), (495, 275)],
    [(300, 700), (410, 700), (410, 768), (300, 768)],
    [(660, 690), (768, 690), (768, 768), (660, 768)],
    [(580, 340), (640, 340), (640, 410), (580, 410)],
)]
EYES = [(85 / 768, 680 / 768, 16 / 768), (128 / 768, 680 / 768, 16 / 768)]
CYAN = {"": (0.3, 0.9, 1.0), "_red": (1.0, 0.35, 0.2), "_blue": (0.3, 0.7, 1.0)}


def plate(u, v):
    """None off the plates, else whether (u, v) is on a stencilled bar."""
    for x0, y0, x1, y1 in PLATES:
        x, y = (u * 768 - x0) / (x1 - x0), (v * 768 - y0) / (y1 - y0)
        if 0 <= x <= 1 and 0 <= y <= 1:
            return 0.22 < y < 0.78 and any(abs(x - c) < 0.07 for c in (0.3, 0.5, 0.7))
    return None


def body_grade(u, v):
    return [0.42, 0.4, 0.38, 1]


def detail_fn():
    n = Fbm(2011, 4, 6)

    def f(u, v):
        p = plate(u, v)
        if p is not None:
            if p:
                return [0.85, 0.82, 0.7, 1.0]
            k = 0.3 * (0.8 + 0.4 * n.at(u * 8, v * 8))
            return [k, k, k * 1.05, 1.0]
        if inside(FEET, u, v):
            k = 0.2 * (0.75 + 0.5 * n.at(u * 8, v * 8))
            return [k, k, k * 1.05, 0.95]
        if inside(HEAD, u, v):
            k = 0.36 * (0.75 + 0.5 * n.at(u * 6, v * 6))
            return [k, k * 1.02, k * 1.08, 0.88]
        return [0, 0, 0, 0]
    return f


def eyes_fn(colour):
    def f(u, v):
        k = 0.0
        for cu, cv, r in EYES:
            k = max(k, 1 - math.hypot(u - cu, v - cv) / r)
        k = clamp(k * 1.4)
        return [colour[0] * k, colour[1] * k, colour[2] * k, 1]
    return f


def look():
    return Look("tankjr", "clutch", {
        "body": Sheet({"": "tankjr", "_red": "red", "_blue": "blue"},
                      grade={"": body_grade, "_red": None, "_blue": None},
                      detail=detail_fn(),
                      glow={v: eyes_fn(CYAN[v]) for v in CYAN}, size=512),
    }, {"lower": {s: "body" for s in ("l_rcalf", "l_rfoot", "l_lfoot", "l_hips", "l_rthigh", "l_lthigh",
                                      "l_rtoe", "l_ltoe", "l_rtoes", "l_ltoes", "l_lcalf")},
        "upper": {"u_abdomen": "body", "u_arms": "body", "u_torso": "body"},
        "head": {"h_head": "body"}},
        {"lower": ["tag_torso"], "upper": ["tag_head", "tag_weapon", "tag_torso"], "head": ["tag_head"]})
