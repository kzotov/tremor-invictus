"""Scalebearer: razor's "id" armour under visor's helm. Red scale armour
with gold rims painted over the chest, dark bronze scales over the bare
arms, dark scaled greaves, our own gold shield over the chest emblem, and
the helm graded to a red bronze. Team versions keep the team's paint with
the scales over the skin."""
import math
from skins import Sheet, Look, region, inside
from tremorart import Fbm, clamp

# ---------------------------------------------------------------- the body sheet (razor's id.tga, 768 px view)

CHEST = [region([(0, 0), (445, 0), (445, 400), (0, 400)], 768, 768),
         region([(600, 40), (768, 40), (768, 360), (600, 360)], 768, 768)]       # the back
ARMS = [region(p, 768, 768) for p in (
    [(280, 350), (500, 350), (500, 500), (280, 500)],
    [(480, 490), (768, 490), (768, 768), (480, 768)],
    [(500, 280), (640, 280), (640, 380), (500, 380)],
)]
LEGS = [region([(0, 400), (285, 400), (285, 768), (0, 768)], 768, 768)]
CREST = (0.2, 0.2)             # over the chest emblem
RED = (0.5, 0.06, 0.04)
BRONZE = (0.3, 0.2, 0.1)
DARK = (0.1, 0.08, 0.07)
GOLD = (0.85, 0.62, 0.22)


def scales(u, v, nu, nv):
    """(depth 0..1 down a scale, on its rim) for overlapping rows of scales."""
    y = v * nv
    row = math.floor(y)
    x = u * nu + 0.5 * (row % 2)
    fx, fy = x - math.floor(x) - 0.5, y - row
    shape = fy + 1.6 * fx * fx
    return clamp(shape), shape > 0.82


def crest(u, v):
    """< 0 inside a gold shield with a bar across it."""
    du, dv = (u - CREST[0]) / 0.04, (v - CREST[1]) / 0.055
    shield = max(abs(du) * 0.9 + max(dv, 0) * 0.8 - 1, -dv - 0.8)
    if abs(dv + 0.1) < 0.12 and shield < -0.12:
        return 0.05                                   # the bar, dark
    return shield


def body_detail_fn(team):
    n = Fbm(1411, 4, 6)

    def paint(u, v, colour, nu, nv, alpha, rim, rim_alpha):
        """Scales: each shaded darker down its length, rimmed at its edge."""
        depth, on_rim = scales(u, v, nu, nv)
        if on_rim:
            return list(rim) + [rim_alpha]
        k = (0.8 + 0.4 * n.at(u * 4, v * 4)) * (1.2 - 0.6 * depth)
        return [x * k for x in colour] + [alpha]

    def f(u, v):
        if not team:
            c = crest(u, v)
            if c < 0:
                k = 0.7 + 0.5 * clamp(-c * 3)
                return [x * k for x in GOLD] + [1.0]
            if c < 0.12:
                return [0.06, 0.03, 0.02, 1.0]
        if inside(ARMS, u, v):
            return paint(u, v, BRONZE, 40, 52, 0.85, (0.5, 0.36, 0.15), 0.9)
        if team:
            return [0, 0, 0, 0]
        if inside(CHEST, u, v):
            return paint(u, v, (0.35, 0.03, 0.02), 36, 46, 0.3, GOLD, 0.55)
        if inside(LEGS, u, v):
            return paint(u, v, DARK, 30, 40, 0.5, (0.32, 0.24, 0.12), 0.6)
        return [0, 0, 0, 0]
    return f


# ---------------------------------------------------------------- the helm (visor.tga; the head is all this look wears of it)

def helm_grade(u, v):
    return [0.62, 0.45, 0.38, 1]           # grey steel to a red bronze


def look():
    team = body_detail_fn(True)
    return Look("razor", "scalebearer", {
        "body": Sheet({"": "id", "_red": "red", "_blue": "blue"},
                      detail={"": body_detail_fn(False), "_red": team, "_blue": team}, size=512),
        "helm": Sheet({"": "visor", "_red": "red", "_blue": "blue"}, model="visor",
                      grade={"": helm_grade, "_red": None, "_blue": None},
                      size=256),
    }, {"lower": {"l_legs": "body"}, "upper": {"u_body": "body"}, "head": {"h_head": "helm"}},
        {"lower": ["tag_torso"], "upper": ["tag_head", "tag_weapon", "tag_torso"], "head": ["tag_head"]},
        head_model="visor")
