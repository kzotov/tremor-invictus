"""Generate the HUD's own images: vitals icons, bar segments, the ability
disc and its ring, glows, the score box, world pins, the damage arc and
the rest of the pieces the HUD layout draws.

Usage: python3 make-hud.py OUT_DIR [name ...]

Writes OUT_DIR/gfx/tremor/hud/<name>.tga (and gfx/tremor/ui/progress_bar.tga)
at twice the 1080p size so they stay sharp at 4K. Pieces the code tints are
white; glows are white or coloured light on transparent black, meant for
additive or alpha blending as noted per image. All drawn here from shapes.
"""
import math, os, random, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tremorart import *


def hexc(h):
    return tuple(int(h[i:i + 2], 16) / 255 for i in (1, 3, 5))


WHITE = (1.0, 1.0, 1.0)


def px(c, v):
    """v pixels in canvas units."""
    return v / c.k


def radial(c, cx, cy, r, color, peak, power=2.0):
    """A radial light: alpha peak at the centre falling to 0 at r (pixels)."""
    def f(x, y, p):
        d = math.hypot(x * c.k - cx, y * c.k - cy) / r
        a = peak * max(0.0, 1 - d) ** power
        return (color[0], color[1], color[2], a)
    c.shade(f)


# ---------------------------------------------------------------- vitals


def concave_cross(cx, cy, r, arm):
    w = arm * 0.92
    d = (r * r + arm * arm - 2 * w * w) / (2 * (r + arm) - 4 * w)
    rho = math.sqrt(2) * (d - w)
    s = box(cx, cy, r, r)
    for sx in (-1, 1):
        for sy in (-1, 1):
            s = s - circle(cx + sx * d, cy + sy * d, rho) \
                - box(cx + sx * ((d + r) / 2 + 0.01), cy + sy * ((d + r) / 2 + 0.01), (r - d) / 2 + 0.01, (r - d) / 2 + 0.01)
    return s


def vital_health_icon():
    c = Canvas(80, 80, span=80)
    col = hexc("#6C90C0")
    c.fill(ring(40, 40, 34, 4), col)
    cr = concave_cross(40, 40, 20, 8.5)
    c.fill_flat(cr, base=0.86, light=1.0, dark=0.7, rim=2.5, color=tuple(v / 0.86 for v in col))
    return c


def vital_armour_icon():
    c = Canvas(80, 80, span=80)
    col = hexc("#4EBA54")
    c.fill(ring(40, 40, 34, 4), col)
    x0, x1, y0, h = 22, 58, 20, 40
    sh = polygon([(x0, y0), (x1, y0), (x1, y0 + h * 0.55), (40, y0 + h), (x0, y0 + h * 0.55)])
    for nx in (31, 40, 49):
        sh = sh - polygon([(nx - 2.6, y0 - 1), (nx + 2.6, y0 - 1), (nx, y0 + 5.5)])
    c.fill_flat(sh, base=0.86, light=1.0, dark=0.7, rim=2.5, color=tuple(v / 0.86 for v in col))
    return c


def segment():
    c = Canvas(52, 34, span=52)
    c.fill(box(26, 17, 25, 16), WHITE)
    return c


def seg_glow():
    c = Canvas(68, 50, span=68)
    r = box(34, 25, 25, 16)
    c.glow(r, WHITE, 6, 1.0)
    for p in c.px:
        p[0] = p[1] = p[2] = 1.0
    return c


# ---------------------------------------------------------------- ability


def ability_disc():
    c = Canvas(208, 208, span=208)
    n = Noise(41, 256, 256)
    c.fill(circle(104, 104, 94.5), lambda x, y: (lambda k: (k, k, k))(0.94 + 0.12 * (n.at(x * 0.9, y * 0.9) - 0.5)))
    return c


def ability_ring():
    c = Canvas(208, 208, span=208)
    c.fill(ring(104, 104, 98, 10), WHITE)
    return c


def ability_glow():
    c = Canvas(320, 320)
    radial(c, 160, 160, 160, hexc("#F7B245"), 0.6, 1.6)
    return c


def lens_streak():
    c = Canvas(512, 32)

    def f(x, y, p):
        u = abs(x * c.k - 256) / 256
        v = abs(y * c.k - 16) / 16
        a = (1 - u) ** 2.2 * max(0.0, 1 - v) ** 1.5
        return (1, 1, 1, a)
    c.shade(f)
    return c


def ability_cradle():
    c = Canvas(480, 160, span=480)
    w = 3
    u = arc(240, 4, 124, w, 0, 180)
    arms = line(38, 4, 116, 4, w) | line(364, 4, 442, 4, w)
    c.fill(u | arms, WHITE)
    return c


def keycap():
    c = Canvas(36, 44, span=36)
    c.fill_flat(box(18, 22, 17, 21, 6), base=0.86, light=1.0, dark=0.7, rim=2, color=tuple(v / 0.86 for v in hexc("#C0BCB0")))
    return c


def charge_pip_glow():
    c = Canvas(80, 80)
    radial(c, 40, 40, 40, hexc("#F0A030"), 0.7, 1.8)
    return c


# ---------------------------------------------------------------- ammo and weapon column


def ammo_ring():
    c = Canvas(128, 128, span=128)
    c.fill(ring(64, 64, 60 - 2.5, 5), WHITE)
    return c


def low_ammo_smear():
    w, h = 720, 192
    c = Canvas(w, h, span=w)
    rng = random.Random(7)
    dark, bright = hexc("#600000"), hexc("#C01818")
    fine = Noise(3, 256, 64)
    for i in range(6):
        y = 50 + i * 18 + rng.uniform(-6, 6)
        x0 = rng.uniform(10, 120)
        x1 = rng.uniform(600, 700)
        th = rng.uniform(16, 30)
        seed = rng.random() * 100

        def f(x, y_, y=y, x0=x0, x1=x1, th=th, seed=seed):
            return mix(dark, bright, smooth(0, w, x))
        stroke = line(x0, y, x1, y + rng.uniform(-10, 10), th)
        c.fill(stroke, f, alpha=0.55)

    def bristle(x, y, p):
        u = x / w
        a = p[3]
        if a <= 0:
            return p
        streak = 0.55 + 0.45 * fine.at(x * 0.02, y * 0.45)        # dry-brush streaks along the stroke
        ragged = smooth(0.0, 0.35, u + 0.25 * (fine.at(y * 0.12, 3.0) - 0.5))
        a = min(1.0, a * 1.4) * streak * (0.85 * ragged) * smooth(1.0, 0.9, u + 0.06 * fine.at(y * 0.1, 9.0))
        return (p[0], p[1], p[2], a)
    c.shade(bristle)
    return c


def rack_gauge():
    c = Canvas(20, 104, span=20)
    outer = chamfered(3, 2, 17, 102, 6, "tl")
    c.fill(outer - chamfered(5, 4, 15, 100, 5, "tl"), WHITE)
    return c


def rack_gauge_fill():
    c = Canvas(20, 104, span=20)
    c.fill(chamfered(5, 4, 15, 100, 5, "tl"), WHITE)
    return c


def rack_bracket():
    c = Canvas(44, 124, span=44)
    c.fill(box(22, 62, 20, 60, 6).outline(3), WHITE)
    return c


# ---------------------------------------------------------------- score bar


def score_box_shape():
    # the left side's box; its inner (right) top corner is cut
    return chamfered(0, 0, 444, 72, 33, "tr")


def score_box():
    c = Canvas(444, 72, span=444)
    c.fill(score_box_shape(), WHITE)
    return c


def score_box_line():
    c = Canvas(444, 72, span=444)
    o = polyline([(0, 1.5), (444 - 33, 1.5), (442.5, 33), (442.5, 72)], 3)
    c.fill(o, WHITE)
    return c


def name_strip():
    c = Canvas(444, 44, span=444)
    c.fill(box(222, 22, 222, 22), (0, 0, 0), 0.7)
    return c


def portrait_frame():
    c = Canvas(124, 124, span=124)
    c.fill(box(62, 62, 60.5, 60.5).outline(3), hexc("#8A8A86"))
    c.fill(box(62, 62, 58, 58).outline(2), (0, 0, 0))
    return c


def dead_slash():
    c = Canvas(180, 180, span=180)
    rng = random.Random(11)
    n = Noise(5, 64, 64)
    col = hexc("#FF1010")
    for off in (-24, 0, 24):
        reach = 62 - abs(off) * 0.6 + rng.uniform(-4, 4)
        o = off * 0.7071
        a = (90 + o - reach * 0.7071, 90 + o + reach * 0.7071)
        b = (90 + o + reach * 0.7071, 90 + o - reach * 0.7071)
        th = 13 - abs(off) * 0.12
        st = line(a[0], a[1], b[0], b[1], th)
        c.fill(st, col, soft=3)

    def rough(x, y, p):
        if p[3] <= 0:
            return p
        k = 0.6 + 0.6 * n.at((x + y) * 0.08, (x - y) * 0.5)
        return (p[0], p[1], p[2], clamp(p[3] * k))
    c.shade(rough)
    return c


def badge():
    c = Canvas(60, 60, span=60)
    c.fill(circle(30, 30, 26), hexc("#693027"))
    c.fill(ring(30, 30, 26, 4), hexc("#C8A060"))
    tick = polygon([(17, 31), (21.5, 26.5), (26, 31), (38.5, 18.5), (43, 23), (26, 40)])
    c.fill(tick, WHITE)
    return c


def medal_ring(tier):
    col = {"common": "#C8D0D8", "rare": "#3C8CF0", "epic": "#C810EC", "legendary": "#F0B020"}[tier]

    def make():
        c = Canvas(192, 192, span=192)
        ro = star(96, 96, 70, 56, 8, -90)
        c.fill(ro.outline(2), WHITE, 0.4)
        c.glow(ring(96, 96, 85, 6), scale(hexc(col), 0.6), 6, 0.6)
        c.fill(ring(96, 96, 85, 6), hexc(col))
        return c
    return make


def medal_streak():
    c = Canvas(1600, 80)
    col = hexc("#B4AEFC")
    rows = [(40, 1.0, 1.0), (28, 0.55, 0.7), (52, 0.55, 0.75), (18, 0.3, 0.45), (62, 0.3, 0.5)]

    def f(x, y, p):
        X, Y = x * c.k, y * c.k
        a = 0.0
        for ry, width, reach in rows:
            u = abs(X - 800) / (800 * reach)
            if u >= 1:
                continue
            a += math.exp(-((Y - ry) / (1.6 * width + 0.6)) ** 2) * (1 - u) ** 1.8 * width
        a = min(1.0, a)
        return (col[0], col[1], col[2], a)
    c.shade(f)
    return c


# ---------------------------------------------------------------- world markers


def world_pin():
    c = Canvas(128, 144, span=128)
    c.fill(circle(64, 62, 58), (0, 0, 0), 0.25)
    c.fill(ring(64, 62, 57, 2), WHITE, 0.35)
    c.fill(polyline([(50, 126), (64, 140), (78, 126)], 4), WHITE)
    return c


def world_bar():
    c = Canvas(28, 16, span=28)
    c.fill(box(14, 8, 14, 8), (0.1, 0.1, 0.1))
    c.fill(box(14, 8, 12, 6), WHITE)
    return c


def world_arrow():
    c = Canvas(32, 36, span=32)
    tri = polygon([(2, 2), (30, 2), (16, 34)]) - polygon([(12, 0), (20, 0), (16, 8)])
    c.fill(tri, WHITE)
    return c


def respawn_arc(i):
    def make():
        c = Canvas(128, 128, span=128)
        seg = 180 / 3
        gap = 3 / 56 * 180 / math.pi / 2         # 3 px of gap on the stroke's middle radius
        a0 = 180 + i * seg + gap
        a1 = 180 + (i + 1) * seg - gap
        c.fill(arc_band(64, 64, 56, 16, a0, a1), (0.62, 0.62, 0.62))
        return c
    return make


def arc_band(cx, cy, r, w, a0, a1):
    """A band of the circle with square ends, from a0 to a1 degrees."""
    band = ring(cx, cy, r, w)
    lo, hi = math.radians(a0), math.radians(a1)

    def f(x, y):
        a = math.atan2(y - cy, x - cx)
        while a < lo:
            a += math.tau
        inside = a <= hi
        d = band.sdf(x, y)
        if inside:
            return d
        rr = math.hypot(x - cx, y - cy)
        da = min(abs(a - hi), abs(lo + math.tau - a))
        return max(d, da * rr)
    return Shape(f, (cx - r - w, cy - r - w, cx + r + w, cy + r + w))


def crosshair_plus():
    c = Canvas(32, 32, span=32)
    plus = box(16, 16, 13, 1.5) | box(16, 16, 1.5, 13)
    c.fill(plus.grow(2), (0.05, 0.05, 0.05), 0.8)
    c.fill(plus, WHITE)
    return c


def damage_arc_shape(thick, k):
    """Arc of radius 590 spanning 64 degrees, `thick` px at its middle tapering to 0, with
    a diamond of 28 px at the middle; centred at the top of a 640 x 128 canvas."""
    R = 590
    cx, cy = 320, 16 + R

    def f(x, y):
        a = math.degrees(math.atan2(x - cx, cy - y))           # 0 at the top
        t = clamp(1 - abs(a) / 32)
        half = thick / 2 * t ** 0.8
        return abs(math.hypot(x - cx, y - cy) - R) - half if t > 0 else 99
    band = Shape(f, (0, 0, 640, 128))
    dia = regular(320, 16 + 2, 14, 4, 0)
    return band | dia


def damage_arc(thick, blurred):
    def make():
        c = Canvas(640, 128, span=640)
        sh = damage_arc_shape(thick, 1)
        if blurred:
            c.glow(sh, WHITE, 20, 0.9)
            for p in c.px:
                p[0] = p[1] = p[2] = 1.0
        else:
            c.fill(sh, WHITE)
        return c
    return make


def hit_streak():
    c = Canvas(512, 16)

    def f(x, y, p):
        X, Y = x * c.k, y * c.k
        u = X / 512
        a = u ** 2.4 * max(0.0, 1 - abs(Y - 8) / 2.6)
        head = math.exp(-((X - 500) / 10) ** 2 - ((Y - 8) / 3) ** 2)
        return (1, 1, 1, min(1.0, a + head))
    c.shade(f)
    return c


def progress_bar():
    c = Canvas(576, 28, span=576)
    c.fill(box(288, 14, 288, 14), (0.4, 0.4, 0.42))
    c.fill(box(288, 14, 286, 12), hexc("#202020"))
    return c


IMAGES = {
    "vital_health_icon": vital_health_icon, "vital_armour_icon": vital_armour_icon, "segment": segment,
    "seg_glow": seg_glow, "ability_disc": ability_disc, "ability_ring": ability_ring,
    "ability_glow": ability_glow, "lens_streak": lens_streak, "ability_cradle": ability_cradle,
    "keycap": keycap, "charge_pip_glow": charge_pip_glow, "ammo_ring": ammo_ring,
    "low_ammo_smear": low_ammo_smear, "rack_gauge": rack_gauge, "rack_gauge_fill": rack_gauge_fill,
    "rack_bracket": rack_bracket, "score_box": score_box, "score_box_line": score_box_line,
    "name_strip": name_strip, "portrait_frame": portrait_frame, "dead_slash": dead_slash, "badge": badge,
    "medal_ring_common": medal_ring("common"), "medal_ring_rare": medal_ring("rare"),
    "medal_ring_epic": medal_ring("epic"), "medal_ring_legendary": medal_ring("legendary"),
    "medal_streak": medal_streak, "world_pin": world_pin, "world_bar": world_bar, "world_arrow": world_arrow,
    "respawn_arc_1": respawn_arc(0), "respawn_arc_2": respawn_arc(1), "respawn_arc_3": respawn_arc(2),
    "crosshair_plus": crosshair_plus, "damage_arc": damage_arc(12, False),
    "damage_arc_glow": damage_arc(12, True), "damage_arc_thick": damage_arc(20, False),
    "hit_streak": hit_streak, "progress_bar": progress_bar,
}


def job(args):
    name, root = args
    sub = "ui" if name == "progress_bar" else "hud"
    IMAGES[name]().write(os.path.join(root, sub, name + ".tga"))


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    root = os.path.join(sys.argv[1], "gfx", "tremor")
    os.makedirs(os.path.join(root, "hud"), exist_ok=True)
    os.makedirs(os.path.join(root, "ui"), exist_ok=True)
    jobs = [(name, root) for name in (sys.argv[2:] or IMAGES)]
    try:
        from multiprocessing import Pool
        with Pool() as pool:
            pool.map(job, jobs)
    except (ImportError, OSError):
        for j in jobs:
            job(j)


if __name__ == "__main__":
    main()
