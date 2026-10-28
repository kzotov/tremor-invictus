"""Generate the ability icons and effect textures.

Usage: python3 make-abilities.py OUT_DIR [name ...]

Writes, under OUT_DIR/gfx/tremor/:
  abilities/<name>.tga   256 x 256 icons with alpha, one per active ability in
                         bg_abilityInfo: a worn metal hexagon (like the
                         pillars) around a dark face, the pictogram in silver
                         and glowing red
  fx/hookline.tga        128 x 32, a chain of links for the hook's line
  fx/glowball.tga        128 x 128, a soft ball of light in grey, tinted by
                         the entity colour (fireballs, acid, trail, spines)
  fx/orb.tga             128 x 128, a dark core inside a red corona
  fx/barrier.tga         256 x 256, a tiling grid of red hexagonal cells
  fx/ring.tga            256 x 256, a thin ring of light for ground effects
All drawn here from shapes and noise.
"""
import math, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tremorart import *

N = 256


# ---------------------------------------------------------------- pictograms
# each returns (silver, red) shapes in the unit square; either may be None

def a_hook():
    hook = arc(0.5, 0.6, 0.12, W, -10, 190) | line(0.38, 0.6, 0.31, 0.53, W)
    links = union(*[box(0.5, 0.13 + i * 0.07, 0.022, 0.03, 0.02).outline(0.02) for i in range(2)])
    return links | hook | line(0.62, 0.6, 0.62, 0.3, W) | line(0.5, 0.3, 0.62, 0.3, W), polygon([(0.31, 0.48), (0.25, 0.6), (0.37, 0.56)])


def a_bullrush():
    horns = arc(0.37, 0.4, 0.13, W, 100, 220) | arc(0.63, 0.4, 0.13, W, -40, 80)
    head = box(0.5, 0.55, 0.1, 0.11, 0.05)
    return horns | head - circle(0.5, 0.6, 0.03), arrow(0.28, 0.79, 0.72, 0.79, 0.04, 0.06)


def a_injection():
    body = box(0.5, 0.5, 0.07, 0.17, 0.015)
    plunger = line(0.5, 0.2, 0.5, 0.33, 0.035) | line(0.42, 0.2, 0.58, 0.2, 0.04)
    needle = line(0.5, 0.67, 0.5, 0.83, 0.02)
    fluid = box(0.5, 0.56, 0.045, 0.1)
    return (body - fluid.grow(0.012)) | plunger | needle, fluid


def pistol(cx, cy, flip=False):
    pts = [(cx - 0.17, cy - 0.06), (cx + 0.13, cy - 0.06), (cx + 0.13, cy + 0.01), (cx - 0.02, cy + 0.01),
           (cx - 0.05, cy + 0.15), (cx - 0.15, cy + 0.15), (cx - 0.11, cy + 0.01), (cx - 0.17, cy + 0.01)]
    if flip:
        pts = [(2 * cx - x, y) for x, y in pts]
    return polygon(pts)


def a_dualwield():
    return pistol(0.44, 0.38) | pistol(0.56, 0.6, True), circle(0.6, 0.355, 0.025) | circle(0.4, 0.575, 0.025)


def a_barrier():
    s = shield(0.5, 0.5, 0.2)
    cells = union(*[regular(0.5 + dx, 0.5 + dy, 0.045, 6, 30) for dx, dy in
                    ((0, 0), (0.08, -0.045), (-0.08, -0.045), (0, -0.09), (0.08, 0.045), (-0.08, 0.045), (0, 0.09))])
    return s - cells.grow(0.012), cells


def tongue(cx, cy, r, h, lean=0.0):
    """A flame tongue: a round bottom drawn up to a point."""
    return circle(cx, cy, r) | polygon([(cx - r * 0.98, cy - r * 0.2), (cx + lean, cy - h), (cx + r * 0.98, cy - r * 0.2)])


def a_flamestrike():
    f = tongue(0.5, 0.6, 0.15, 0.4) | tongue(0.38, 0.65, 0.09, 0.28, -0.05) | tongue(0.62, 0.65, 0.09, 0.28, 0.05)
    inner = tongue(0.5, 0.65, 0.065, 0.2)
    return line(0.3, 0.8, 0.7, 0.8, 0.04) | inner, f - inner.grow(0.02)


def a_berserk():
    claws = union(*[line(0.36 + i * 0.1, 0.33, 0.44 + i * 0.1, 0.67, 0.035) for i in range(3)])
    return circle(0.5, 0.5, 0.22).outline(0.045) - claws.grow(0.025), claws


def a_turret():
    legs = line(0.5, 0.58, 0.32, 0.8, 0.045) | line(0.5, 0.58, 0.68, 0.8, 0.045) | line(0.5, 0.58, 0.5, 0.8, 0.045)
    head = box(0.47, 0.45, 0.12, 0.08, 0.03)
    barrel = box(0.67, 0.43, 0.1, 0.025)
    return legs | head | barrel, circle(0.43, 0.45, 0.035)


def a_totem():
    body = polygon([(0.42, 0.8), (0.44, 0.3), (0.5, 0.2), (0.56, 0.3), (0.58, 0.8)])
    eye = polygon([(0.5, 0.36), (0.535, 0.42), (0.5, 0.48), (0.465, 0.42)])
    base = box(0.5, 0.8, 0.14, 0.025)
    return body - eye.grow(0.015) | base, eye


def a_grenades():
    g = circle(0.47, 0.56, 0.16)
    bands = box(0.47, 0.56, 0.16, 0.014)
    cap = box(0.47, 0.38, 0.05, 0.035)
    return (g - bands) | cap, ring(0.6, 0.3, 0.05, 0.03)


def a_ghostwalk():
    pts = [(0.32, 0.78), (0.32, 0.45)]
    for i in range(21):
        a = math.radians(180 + i * 9)
        pts.append((0.5 + 0.18 * math.cos(a), 0.45 + 0.2 * math.sin(a)))
    pts += [(0.68, 0.78), (0.62, 0.72), (0.56, 0.78), (0.5, 0.72), (0.44, 0.78), (0.38, 0.72)]
    body = polygon(pts).outline(0.04)
    return body, circle(0.44, 0.45, 0.03) | circle(0.56, 0.45, 0.03)


def a_orb():
    orbit = arc(0.5, 0.5, 0.21, 0.035, 200, 520)
    return orbit & halfplane(0.3, -1, 0.2) | arc(0.5, 0.5, 0.21, 0.035, -40, 40), circle(0.5, 0.5, 0.12)


def a_plasmatrail():
    pts = [(0.2 + 0.4 * i / 30, 0.62 + 0.06 * math.sin(i / 30 * 3 * math.pi)) for i in range(31)]
    return chevron_right(0.7, 0.55, 0.13), polyline(pts, 0.05)


def a_acidspit():
    drops = circle(0.62, 0.46, 0.06) | circle(0.72, 0.62, 0.04) | circle(0.52, 0.64, 0.035)
    mouth = arc(0.33, 0.5, 0.14, W, -50, 50)
    return mouth, drops


def a_drone():
    body = box(0.5, 0.52, 0.12, 0.07, 0.04)
    rotors = line(0.22, 0.38, 0.42, 0.38, 0.03) | line(0.58, 0.38, 0.78, 0.38, 0.03) \
        | line(0.38, 0.38, 0.42, 0.48, 0.035) | line(0.62, 0.38, 0.58, 0.48, 0.035)
    return (body | rotors) - circle(0.5, 0.52, 0.05), circle(0.5, 0.52, 0.035)


def a_piercingsight():
    eye = polygon([(0.2 + 0.6 * i / 20, 0.5 - 0.17 * math.sin(math.pi * i / 20)) for i in range(21)]
                  + [(0.8 - 0.6 * i / 20, 0.5 + 0.17 * math.sin(math.pi * i / 20)) for i in range(1, 20)])
    return eye.outline(0.045), circle(0.5, 0.5, 0.08)


def a_shellshock():
    return ring(0.5, 0.5, 0.13, 0.035) | ring(0.5, 0.5, 0.21, 0.03), circle(0.5, 0.5, 0.06)


def a_spinevolley():
    def spine(x, y, l):
        # pointing up and to the right
        return polygon([(x + l * 0.7, y - l * 0.7), (x - 0.04, y - 0.04), (x + 0.04, y + 0.04)])
    return spine(0.24, 0.62, 0.32) | spine(0.42, 0.8, 0.32), spine(0.3, 0.73, 0.4)


def a_masterofdeath():
    pts = []
    for i in range(61):
        t = i / 60 * 2 * math.pi
        d = 1 + math.sin(t) ** 2
        pts.append((0.5 + 0.24 * math.cos(t) / d, 0.5 + 0.24 * math.sin(t) * math.cos(t) / d))
    return polyline(pts, 0.05), circle(0.5, 0.5, 0.035)


def a_supplydrop():
    crate = box(0.5, 0.6, 0.15, 0.13, 0.015)
    cross = box(0.5, 0.6, 0.07, 0.02) | box(0.5, 0.6, 0.02, 0.07)
    return crate - cross.grow(0.015), cross | arrow(0.5, 0.18, 0.5, 0.4, 0.04, 0.06)


def a_markprey():
    return reticle(0.17), polygon([(0.5, 0.42), (0.57, 0.55), (0.43, 0.55)])


def a_downpour():
    cloud = circle(0.42, 0.42, 0.1) | circle(0.56, 0.38, 0.12) | box(0.5, 0.47, 0.17, 0.05, 0.05)
    rain = union(*[line(0.38 + i * 0.08, 0.6, 0.35 + i * 0.08, 0.72, 0.03) for i in range(4)])
    return cloud, rain


def a_hound():
    pad = circle(0.5, 0.6, 0.11)
    toes = union(*[circle(0.5 + 0.13 * math.cos(math.radians(a)), 0.47 + 0.13 * math.sin(math.radians(a)), 0.045)
                   for a in (-150, -110, -70, -30)])
    return toes, pad


def a_riftanchor():
    shank = line(0.5, 0.3, 0.5, 0.75, W)
    bar = line(0.4, 0.37, 0.6, 0.37, 0.045)
    flukes = arc(0.5, 0.6, 0.17, W, 20, 160)
    return shank | bar | flukes, ring(0.5, 0.24, 0.05, 0.035)


ABILITIES = ["hook", "bullrush", "injection", "dualwield", "barrier", "flamestrike", "berserk", "turret",
             "totem", "grenades", "ghostwalk", "orb", "plasmatrail", "acidspit", "drone", "piercingsight",
             "shellshock", "spinevolley", "masterofdeath", "supplydrop", "markprey", "downpour", "hound", "riftanchor"]


def make_icon(name):
    c = Canvas(N, N)
    seed = sum(map(ord, name)) + 1000
    face = regular(0.5, 0.5, 0.43, 6, 30)
    c.fill(face, lambda x, y: mix((0.13, 0.13, 0.145), INK, smooth(0, 0.4, math.hypot(x - 0.5, y - 0.45))))
    c.fill(regular(0.5, 0.5, 0.405, 6, 30).outline(0.01), RED_DEEP)
    rim = regular(0.5, 0.5, 0.47, 6, 30) - regular(0.5, 0.5, 0.415, 6, 30)
    c.fill(rim.grow(0.006), INK)
    c.fill(rim, worn_metal(seed, SILVER, GUNMETAL, 0.05, 0.95))
    silver, red = globals()["a_" + name]()
    if silver is not None:
        c.fill(silver.grow(0.014), INK)
        c.fill(silver, worn_metal(seed + 1, SILVER, PEWTER, 0.2, 0.8, 0.15))
    if red is not None:
        c.glow(red, RED, 0.03, 0.55)
        c.fill(red.grow(0.01), (0.25, 0.02, 0.03))
        c.fill(red, lambda x, y: mix((1.0, 0.32, 0.3), RED, smooth(0.25, 0.7, y)))
    return c


# ---------------------------------------------------------------- effects


def make_hookline():
    """Links along the length (s runs along the beam); grey on black, added."""
    w, h = 128, 32
    c = Canvas(w, h, (0, 0, 0, 1), span=4.0)        # units: the height is 1
    for i in range(3):
        x = 0.4 + i * 1.33
        if i % 2 == 0:
            link = box(x + 0.27, 0.5, 0.42, 0.22, 0.2).outline(0.1)
        else:
            link = box(x + 0.27, 0.5, 0.42, 0.06, 0.05)
        c.glow(link, scale(RED, 0.6), 0.12, 0.6)
        c.fill(link, worn_metal(70 + i, (0.85, 0.84, 0.82), (0.45, 0.45, 0.46), 0.25, 0.75, 0.1))
    return c


def make_glowball():
    c = Canvas(128, 128, (0, 0, 0, 1))

    def f(x, y, p):
        d = math.hypot(x - 0.5, y - 0.5) / 0.5
        g = math.exp(-d * d * 5) + 0.7 * math.exp(-d * d * 30)
        g *= 1 - smooth(0.85, 1.0, d)
        return (g, g, g, 1)
    c.shade(f)
    return c


def make_orb():
    c = Canvas(128, 128)
    n = Fbm(80, 4, 6)

    def f(x, y, p):
        d = math.hypot(x - 0.5, y - 0.5) / 0.5
        a = math.atan2(y - 0.5, x - 0.5)
        wob = 0.08 * (n.at(0.5 + math.cos(a) * 0.3, 0.5 + math.sin(a) * 0.3) - 0.5)
        edge = 0.5 + wob
        corona = math.exp(-((d - edge) / 0.12) ** 2) + 0.5 * math.exp(-max(0, d - edge) / 0.18) * (d > edge)
        core = 1 - smooth(edge - 0.06, edge, d)
        r = clamp(RED[0] * corona * 1.4 + 0.3 * corona ** 3)
        g = clamp(RED[1] * corona + 0.25 * corona ** 3)
        b = clamp(RED[2] * corona + 0.2 * corona ** 3)
        alpha = clamp(max(core, corona * 0.9)) * (1 - smooth(0.9, 1.0, d))
        k = 0.06 + 0.1 * n.at(x * 2, y * 2)
        return (r * (1 - core) + k * core, g * (1 - core) + k * 0.5 * core, b * (1 - core) + k * 0.6 * core, alpha)
    c.shade(f)
    return c


def make_barrier():
    """Hexagonal cells tiling a square: red edges on black, added."""
    n = 256
    c = Canvas(n, n, (0, 0, 0, 1))
    # 4 cells across; a hex lattice that wraps needs rows of sqrt(3)/2 spacing,
    # so the tile holds 4 columns by 4 rows squeezed slightly to fit
    cols, rows = 4, 4
    sx, sy = 1.0 / cols, 1.0 / rows

    def f(x, y, p):
        best, second = 9, 9
        for j in range(-1, rows + 1):
            for i in range(-1, cols + 1):
                cx = (i + 0.5 * (j % 2) + 0.25) * sx
                cy = (j + 0.5) * sy
                d = math.hypot((x - cx) / sx, (y - cy) / sy * 0.87)
                if d < best:
                    best, second = d, best
                elif d < second:
                    second = d
        e = second - best                    # 0 on the cell walls
        g = math.exp(-(e / 0.05) ** 2) + 0.15 * math.exp(-(e / 0.25) ** 2)
        hot = math.exp(-(e / 0.02) ** 2) * 0.35
        return (RED[0] * g + hot, RED[1] * g + hot, RED[2] * g + hot, 1)
    c.shade(f)
    return c


def make_ring():
    c = Canvas(256, 256, (0, 0, 0, 1))

    def f(x, y, p):
        d = math.hypot(x - 0.5, y - 0.5)
        g = math.exp(-((d - 0.47) / 0.012) ** 2) + 0.35 * math.exp(-((d - 0.47) / 0.05) ** 2)
        return (g, g, g, 1)
    c.shade(f)
    return c


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    root = os.path.join(sys.argv[1], "gfx", "tremor")
    icons = os.path.join(root, "abilities")
    fx = os.path.join(root, "fx")
    os.makedirs(icons, exist_ok=True)
    os.makedirs(fx, exist_ok=True)
    for name in sys.argv[2:] or ABILITIES:
        make_icon(name).write(os.path.join(icons, name + ".tga"))
    if len(sys.argv) == 2:
        make_hookline().write(os.path.join(fx, "hookline.tga"), alpha=False)
        make_glowball().write(os.path.join(fx, "glowball.tga"), alpha=False)
        make_orb().write(os.path.join(fx, "orb.tga"))
        make_barrier().write(os.path.join(fx, "barrier.tga"), alpha=False)
        make_ring().write(os.path.join(fx, "ring.tga"), alpha=False)


if __name__ == "__main__":
    main()
