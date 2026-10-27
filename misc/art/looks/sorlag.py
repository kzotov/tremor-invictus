"""Sorlag in her own colours: the hide graded from rust to a dark olive
green, the pale scale lines yellowed, acid-green glands glowing where the
model's own texture has its blisters, and the throat frill a salmon pink. Team
versions keep the team's paint with the glands lit in the team's colour."""
import math
from skins import Sheet, Look, painted
from tremorart import Fbm, clamp

ACID = {"": (0.55, 1.0, 0.15), "_red": (1.0, 0.35, 0.1), "_blue": (0.3, 0.7, 1.0)}

# ---------------------------------------------------------------- the hide (sorlag.tga): blisters

def find_glands(geo, n=256):
    """The glands, found on the player's texture: its blisters are flecked
    with vivid blue (bluer than the grey-cyan scale lines); a gland sits
    where the flecks crowd, one to a neighbourhood. [(u, v, r)]"""
    if geo.colour(0.5, 0.5) is None:
        return []
    fleck = [[0] * n for _ in range(n)]
    for y in range(n):
        for x in range(n):
            u, v = (x + 0.5) / n, (y + 0.5) / n
            r, g, b = geo.colour(u, v)
            fleck[y][x] = 1 if geo.at(u, v).covered and b > r + 0.15 and b > g + 0.08 else 0
    # flecks within 3 texels, then the crowded spots that lead their neighbourhood
    rows = [[sum(fleck[y][max(0, x - 3):x + 4]) for x in range(n)] for y in range(n)]
    crowd = [[sum(rows[j][x] for j in range(max(0, y - 3), min(n, y + 4))) for x in range(n)] for y in range(n)]
    out = []
    for y in range(n):
        for x in range(n):
            c = crowd[y][x]
            if c >= 2 and all(c > crowd[j][i] or (c == crowd[j][i] and (j, i) <= (y, x))
                              for j in range(max(0, y - 8), min(n, y + 9)) for i in range(max(0, x - 8), min(n, x + 9))):
                out.append(((x + 0.5) / n, (y + 0.5) / n, (8 + min(c, 8)) / 3.0 / n))     # 3 to 5 texels
    return out


def glands(geo):
    if not hasattr(geo, "sorlag_glands"):
        geo.sorlag_glands = find_glands(geo)
    return geo.sorlag_glands


def hide_grade(u, v):
    return [0.3, 0.46, 0.27, 1]


@painted
def hide_detail_fn(geo):
    n, spots = Fbm(1811, 4, 6), glands(geo)

    def f(u, v):
        k = n.at(u * 6, v * 6)
        for cu, cv, r in spots:
            if math.hypot(u - cu, v - cv) < r:
                return [0.2, 0.35, 0.05, 1.0]
        return [0.12, 0.16, 0.06, 0.25 * clamp((k - 0.45) * 4)]       # mottling
    return f


def glands_fn(colour):
    @painted
    def make(geo):
        spots = glands(geo)

        def f(u, v):
            k = 0.0
            for cu, cv, r in spots:
                k = max(k, 1 - math.hypot(u - cu, v - cv) / (r * 1.3))
            k = clamp(k * 1.5)
            return [colour[0] * k, colour[1] * k, colour[2] * k, 1]
        return f
    return make


# ---------------------------------------------------------------- the torso (sorlag_t.tga): copper plates, blue frill

def torso_grade(u, v):
    return [0.6, 0.46, 0.44, 1]              # a salmon frill


def look():
    return Look("sorlag", "sorlag", {
        "hide": Sheet({"": "sorlag", "_red": "red", "_blue": "blue"},
                      grade={"": hide_grade, "_red": None, "_blue": None},
                      detail={"": hide_detail_fn, "_red": None, "_blue": None},
                      glow={v: glands_fn(ACID[v]) for v in ACID}, size=512, pulse=True),
        "torso": Sheet({"": "sorlag_t", "_red": "red_t", "_blue": "blue_t"},
                       grade={"": torso_grade, "_red": None, "_blue": None}, size=128),
    }, {"lower": {"l_legs": "hide"},
        "upper": {"u_torso": "torso", "u_arms": "hide", "u_lten": "hide", "u_neck": "hide", "u_rten": "hide"},
        "head": {"h_head": "hide"}},
        {"lower": ["tag_torso"], "upper": ["tag_head", "tag_torso", "tag_weapon"], "head": ["tag_head"]})
