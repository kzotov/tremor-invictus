"""Nyx on mynx: blue hair painted over the model's own curls, a violet-black
bodysuit with glowing seams over the leather and the bare skin, a veil over
the lower face, a dark visor, and a crescent sigil where the belt skull was.
Team versions keep the team's suit paint and light the seams in its colour."""
import math, random
from skins import Sheet, Look, region, seg_dist
from tremorart import Fbm, clamp, smooth, mix


# ---------------------------------------------------------------- the suit sheet (mynx_shiny, 768 x 384 view)

SKULL = region([(278, 22), (342, 22), (346, 100), (276, 100)], 768, 384)
SIGIL = (310 / 768, 60 / 384)
VISOR = region([(384, 208), (480, 208), (480, 288), (384, 288)], 768, 384)
# seams over the big suit piece and the boot, as segments in 0..1
SUIT_SEAMS = [((40 / 768, 0.02), (60 / 768, 0.98)), ((220 / 768, 0.02), (205 / 768, 0.98)),
              ((300 / 768, 190 / 384), (300 / 768, 370 / 384))]
VIOLET = (0.15, 0.08, 0.24)
GLOW = {"": (0.35, 0.75, 1.0), "_red": (1.0, 0.4, 0.25), "_blue": (0.4, 0.8, 1.0)}


def suit_grade(u, v):
    return [0.32, 0.27, 0.42, 1]


def team_suit_grade(u, v):
    return [0.36, 0.34, 0.38, 1]


def crescent(u, v, c, r):
    """< 0 inside a crescent moon of radius r round c."""
    du, dv = (u - c[0]) * 2, v - c[1]          # the sheet is twice as wide as tall
    outer = math.hypot(du, dv) - r
    inner = math.hypot(du - r * 0.45, dv - r * 0.2) - r * 0.85
    return max(outer, -inner)


def suit_detail_fn(wash):
    n = Fbm(811, 4, 6)

    def f(u, v):
        c, a = list(VIOLET), wash
        k = 0.8 + 0.4 * n.at(u * 3, v * 3)
        c = [x * k for x in c]
        for s in SUIT_SEAMS:
            d, _ = seg_dist(u, v, *s)
            if d < 0.006:
                c, a = [0.04, 0.03, 0.06], 0.9
        if SKULL(u, v) < 0:
            c, a = [0.07, 0.05, 0.1], 1.0
            if crescent(u, v, SIGIL, 0.07) < 0.004:
                c = [0.55, 0.6, 0.7]
        if VISOR(u, v) < 0:
            c, a = [0.06, 0.04, 0.12], 0.95
        return c + [a]
    return f


def suit_glow_fn(colour):
    def f(u, v):
        k = 0.0
        for s in SUIT_SEAMS:
            d, _ = seg_dist(u, v, *s)
            k = max(k, 1 - d / 0.004)
        k = max(k, 1 - max(0, crescent(u, v, SIGIL, 0.07)) / 0.006 if SKULL(u, v) < 0 else 0)
        if VISOR(u, v) < 0:
            k = max(k, 0.35 * (1 - abs(v - 248 / 384) / 0.03))
        k = clamp(k)
        return [colour[0] * k, colour[1] * k, colour[2] * k, 1]
    return f


# ---------------------------------------------------------------- the arms sheet (mynx.tga, 768 x 768 view): chest, gloves, back

NECK = 0.17          # bare skin above this on the chest piece
GLOVES = region([(430, 0), (768, 0), (768, 310), (430, 310)], 768, 768)
ARM_SEAMS = [((0.30, NECK), (0.40, 0.95)), ((0.70, 0.47), (0.70, 0.97))]
SKIN = (0.86, 0.80, 0.84)


def arms_detail_fn(fabric):
    n = Fbm(823, 4, 5)

    def f(u, v):
        neck = u < 0.56 and v < NECK
        if neck:
            return list(SKIN) + [0.45]
        k = 0.75 + 0.5 * n.at(u * 2.5, v * 2.5)
        c, a = [x * k for x in fabric], 0.93
        if GLOVES(u, v) < 0:
            c, a = [0.07, 0.05, 0.1], 0.9
        if u < 0.56 and v < NECK + 0.012:
            c, a = [0.03, 0.02, 0.05], 1.0                   # the collar
        for s in ARM_SEAMS:
            d, _ = seg_dist(u, v, *s)
            if d < 0.006:
                c, a = [0.04, 0.03, 0.06], 1.0
        return c + [a]
    return f


def arms_glow_fn(colour):
    def f(u, v):
        k = 0.0
        for s in ARM_SEAMS:
            d, _ = seg_dist(u, v, *s)
            k = max(k, 1 - d / 0.004)
        k = clamp(k)
        return [colour[0] * k, colour[1] * k, colour[2] * k, 1]
    return f


# ---------------------------------------------------------------- the head sheet (mynx_h, 768 x 384 view): face left, hair right

HAIR_U = 278 / 768
VEIL_V = 212 / 384
HAIR_DARK = (0.03, 0.06, 0.16)
HAIR = (0.10, 0.24, 0.62)
HAIR_LIGHT = (0.45, 0.70, 1.0)


def head_grade(u, v):
    if u < HAIR_U:
        return [0.48, 0.47, 0.52, 1]          # paler skin
    return [0.5, 0.5, 0.5, 1]


class Curls:
    """Loose curls: arcs of rings jittered over a few lattices of cells."""

    def __init__(self, seed, scales=((26, 13), (17, 9), (37, 19))):
        rng = random.Random(seed)
        self.layers = []
        for nx, ny in scales:
            cells = {(i, j): (rng.random(), rng.random(), 0.26 + 0.14 * rng.random(), rng.random() * 6.283,
                              3.0 + 1.6 * rng.random())
                     for i in range(nx) for j in range(ny)}
            self.layers.append((nx, ny, cells))

    def at(self, u, v):
        """(strand 0..1, highlight 0..1)"""
        best, light = 0.0, 0.0
        for nx, ny, cells in self.layers:
            x, y = u * nx, v * ny
            for i in range(int(x) - 1, int(x) + 2):
                for j in range(int(y) - 1, int(y) + 2):
                    cx, cy, r, a0, span = cells[(i % nx, j % ny)]
                    dx, dy = x - i - cx, y - j - cy
                    ring = abs(math.hypot(dx, dy) - r)
                    a = (math.atan2(dy, dx) - a0) % 6.283
                    if ring < 0.1 and a < span:
                        k = (1 - ring / 0.1) * min(1, a / 0.6, (span - a) / 0.6)
                        if k > best:
                            best, light = k, clamp(0.5 - 0.5 * math.sin(math.atan2(dy, dx) + 0.8))
        return best, light


def head_detail_fn():
    n, warp, veil = Fbm(831, 4, 4), Fbm(837, 3, 3), Fbm(839, 3, 4)
    curls = Curls(841)

    def f(u, v):
        if u >= HAIR_U - 0.005:
            wu = u + 0.02 * (warp.at(u * 2, v * 2) - 0.5)
            wv = v + 0.04 * (warp.at(u * 2 + 5, v * 2) - 0.5)
            strand, light = curls.at(wu, wv)
            depth = n.at(u * 3, v * 3)
            c = mix(HAIR_DARK, HAIR, clamp(depth * 1.4 - 0.15))
            c = mix(c, HAIR, strand * 0.8)
            c = mix(c, HAIR_LIGHT, strand * light * (0.4 + 0.6 * depth))
            gap = math.hypot((u - 0.72) * 2, v - 0.42)        # the dark parting under the crown
            c = [x * (0.35 + 0.65 * smooth(0.06, 0.2, gap)) for x in c]
            edge = clamp((u - HAIR_U + 0.005) / 0.01)
            return list(c) + [edge]
        if v > VEIL_V:
            k = 0.8 + 0.4 * veil.at(u * 2, v * 6)              # folds of the wrap
            fold = 0.85 + 0.15 * math.sin(v * 70 + 4 * veil.at(u * 3, v * 3))
            c = [x * k * fold for x in (0.12, 0.07, 0.18)]
            if v < VEIL_V + 0.012:
                c = [0.05, 0.03, 0.08]                          # the hem
            return c + [clamp((v - VEIL_V) / 0.006)]
        return [0, 0, 0, 0]
    return f


def look():
    sheets = {
        "suit": Sheet({"": "mynx_shiny", "_red": "red_s", "_blue": "blue_s"},
                      grade={"": suit_grade, "_red": team_suit_grade},
                      detail={"": suit_detail_fn(0.55), "_red": suit_detail_fn(0.3)},
                      glow={v: suit_glow_fn(GLOW[v]) for v in GLOW}, size=512),
        "arms": Sheet({"": "mynx", "_red": "mynx", "_blue": "mynx"},
                      detail={"": arms_detail_fn(VIOLET), "_red": arms_detail_fn((0.36, 0.06, 0.06)),
                              "_blue": arms_detail_fn((0.06, 0.1, 0.4))},
                      glow={v: arms_glow_fn(GLOW[v]) for v in GLOW}, size=256),
        "head": Sheet({"": "mynx_h", "_red": "red_h", "_blue": "red_h"},
                      grade=head_grade, detail=head_detail_fn(), size=512),
    }
    sheets["suit"].layers["grade"]["_blue"] = team_suit_grade
    sheets["suit"].layers["detail"]["_blue"] = sheets["suit"].layers["detail"]["_red"]
    return Look("mynx", "nyx", sheets,
                {"lower": {"l_legs": "suit"},
                 "upper": {"u_torso": "suit", "u_arms": "arms"},
                 "head": {"h_head": "head", "h_glasses": "suit"}},
                {"lower": ["tag_torso"], "upper": ["tag_head", "tag_torso", "tag_weapon"], "head": ["tag_head"]})
