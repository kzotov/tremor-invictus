"""Generate the medal icons.

Usage: python3 make-medals.py OUT_DIR [key ...]

Writes OUT_DIR/gfx/tremor/medals/<key>.tga (256 x 256, with alpha) for every
medal: the match medals in bg_medalInfo, the classic awards and the
end-of-match ones. Each is a worn metal ring like the logo's emblem around a
dark face, with a pictogram in silver and glowing red. Weapon masters get a
ring that runs from silver into red. All drawn here from shapes and noise.
"""
import math, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tremorart import *

N = 256


# ---------------------------------------------------------------- pictograms
# each returns (silver, red): shapes in the unit square, either may be None

def p_firstblood():
    return None, circle(0.5, 0.57, 0.14) | polygon([(0.5, 0.24), (0.38, 0.52), (0.62, 0.52)])


def p_midair():
    return (ring(0.5, 0.42, 0.12, 0.05) | line(0.27, 0.73, 0.4, 0.73, 0.045) | line(0.6, 0.73, 0.73, 0.73, 0.045),
            circle(0.5, 0.42, 0.045) | line(0.5, 0.6, 0.5, 0.68, 0.03))


def p_airborne():
    return chevron(0.5, 0.64, 0.15) | chevron(0.5, 0.51, 0.15), chevron(0.5, 0.38, 0.15)


def p_combokill():
    return (circle(0.33, 0.5, 0.085) | box(0.5, 0.5, 0.055, 0.016) | box(0.5, 0.5, 0.016, 0.055),
            regular(0.67, 0.5, 0.1, 4, 0))


def p_collateral():
    return ring(0.43, 0.5, 0.08, 0.045) | ring(0.63, 0.5, 0.08, 0.045), arrow(0.2, 0.5, 0.8, 0.5, 0.045)


def p_doublewhammy():
    return burst(0.42, 0.45, 0.15, 0.065, 8).outline(0.03), burst(0.6, 0.58, 0.15, 0.065, 8)


def p_denied():
    return (ring(0.5, 0.5, 0.17, 0.05) | box(0.5, 0.5, 0.07, 0.022) | box(0.5, 0.5, 0.022, 0.07),
            line(0.37, 0.63, 0.63, 0.37, 0.055))


def p_revenge():
    return arc(0.5, 0.5, 0.16, W, 150, 400), arrow(0.5 + 0.16 * math.cos(math.radians(380)),
                                                   0.5 + 0.16 * math.sin(math.radians(380)),
                                                   0.5 + 0.16 * math.cos(math.radians(415)),
                                                   0.5 + 0.16 * math.sin(math.radians(415)), W, 0.08)


def p_partybreaker():
    dots = circle(0.5, 0.33, 0.07) | circle(0.33, 0.62, 0.07) | circle(0.67, 0.62, 0.07)
    crack = zigzag([(0.5, 0.2), (0.45, 0.42), (0.55, 0.5), (0.47, 0.62), (0.5, 0.8)], 0.035)
    return dots - crack.grow(0.02), crack


def p_perforated():
    holes = [(0.43, 0.42), (0.58, 0.4), (0.5, 0.56), (0.38, 0.6), (0.63, 0.6)]
    disc = circle(0.5, 0.5, 0.2)
    h = union(*[circle(x, y, 0.035) for x, y in holes])
    return disc - h.grow(0.015), h


def p_pointblank():
    return box(0.32, 0.5, 0.13, 0.05, 0.01) | box(0.22, 0.58, 0.03, 0.06, 0.01), burst(0.6, 0.5, 0.15, 0.06, 10)


def p_precise():
    return reticle(0.16), regular(0.5, 0.5, 0.045, 4, 0)


def p_ringout():
    r = arc(0.5, 0.52, 0.15, 0.05, -10, 290)
    return r, arrow(0.5, 0.52, 0.76, 0.27, 0.05)


def p_stopwatch():
    return (ring(0.5, 0.54, 0.16, 0.055) | box(0.5, 0.31, 0.04, 0.025) | line(0.62, 0.38, 0.66, 0.34, 0.035),
            line(0.5, 0.54, 0.58, 0.44, 0.04) | circle(0.5, 0.54, 0.03))


def p_tank():
    s = shield(0.5, 0.5, 0.19)
    bars = box(0.5, 0.45, 0.11, 0.025) | box(0.5, 0.56, 0.08, 0.025)
    return s - bars.grow(0.015), bars


def p_diehard():
    crack = zigzag([(0.5, 0.3), (0.45, 0.43), (0.55, 0.5), (0.48, 0.62)], 0.03)
    return crack, heart(0.5, 0.5, 0.15) - crack.grow(0.015)


def p_fromthegrave():
    t = tombstone(0.5, 0.52, 0.2)
    a = arrow(0.5, 0.66, 0.5, 0.36, 0.05)
    return t - a.grow(0.025), a


def p_kamikaze():
    return circle(0.5, 0.5, 0.06), burst(0.5, 0.5, 0.25, 0.1, 12) - circle(0.5, 0.5, 0.09)


def p_showstopper():
    return regular(0.5, 0.5, 0.2, 8, 22.5).outline(0.05), box(0.5, 0.5, 0.12, 0.035)


def p_triplekill():
    return None, union(*[line(0.37 + i * 0.13, 0.66, 0.43 + i * 0.13, 0.34, 0.06) for i in range(3)])


def p_overkill():
    return None, union(*[line(0.31 + i * 0.12, 0.66, 0.37 + i * 0.12, 0.34, 0.05) for i in range(4)])


def p_crusher():
    head = box(0.5, 0.36, 0.17, 0.08, 0.015)
    handle = box(0.5, 0.58, 0.035, 0.16)
    return handle, head


def p_exterminator():
    sight = ring(0.5, 0.5, 0.17, 0.035)
    ticks = box(0.5, 0.27, 0.02, 0.07) | box(0.5, 0.73, 0.02, 0.07) | box(0.27, 0.5, 0.07, 0.02) | box(0.73, 0.5, 0.07, 0.02)
    return sight | ticks, circle(0.5, 0.5, 0.045)


def p_demolisher():
    return burst(0.5, 0.5, 0.25, 0.11, 8) - box(0.5, 0.5, 0.09, 0.09).grow(0.02), box(0.5, 0.5, 0.09, 0.09, 0.01)


def p_grimreaper():
    staff = line(0.38, 0.76, 0.58, 0.26, 0.035)
    blade = polygon([(0.56, 0.24), (0.3, 0.3), (0.2, 0.42), (0.36, 0.34), (0.58, 0.31)])
    return staff, blade


def p_fighter():
    b = line(0.3, 0.7, 0.7, 0.3, 0.05) | line(0.3, 0.3, 0.7, 0.7, 0.05)
    guards = line(0.33, 0.58, 0.42, 0.67, 0.045) | line(0.58, 0.67, 0.67, 0.58, 0.045)
    return b - guards.grow(0.015), guards


def p_avatarofdeath():
    s, eyes = skull(0.5, 0.5, 0.15)
    horns = polygon([(0.38, 0.4), (0.27, 0.22), (0.43, 0.33)]) | polygon([(0.62, 0.4), (0.73, 0.22), (0.57, 0.33)])
    return s | horns, eyes


def p_killingspree():
    return chevron_right(0.34, 0.5, 0.12) | chevron_right(0.48, 0.5, 0.12), chevron_right(0.62, 0.5, 0.12)


def tier(n, r1):
    return circle(0.5, 0.5, 0.075), burst(0.5, 0.5, r1, 0.11, n) - circle(0.5, 0.5, 0.09)


def p_rage():
    return tier(6, 0.2)


def p_frenzy():
    return tier(9, 0.23)


def p_rampage():
    return tier(12, 0.26)


def p_dominating():
    c = crown(0.5, 0.5, 0.2)
    gems = circle(0.4, 0.55, 0.03) | circle(0.5, 0.55, 0.03) | circle(0.6, 0.55, 0.03)
    return c - gems.grow(0.015), gems


def p_powerupmassacre():
    d = regular(0.5, 0.5, 0.22, 4, 0).outline(0.045)
    return d, burst(0.5, 0.5, 0.12, 0.05, 4, -90)


def p_gauntletgod():
    blade = burst(0.5, 0.5, 0.21, 0.15, 10) - circle(0.5, 0.5, 0.075)
    return blade, circle(0.5, 0.5, 0.045)


def p_machinegungod():
    return bullet(0.38, 0.52, 0.32) | bullet(0.62, 0.52, 0.32), bullet(0.5, 0.48, 0.32)


def p_shotgungod():
    shell = box(0.5, 0.62, 0.06, 0.09, 0.01) | box(0.5, 0.72, 0.075, 0.02)
    return shell, union(*[circle(0.5 + 0.13 * math.cos(math.radians(a)), 0.45 + 0.13 * math.sin(math.radians(a)), 0.03)
                          for a in (-150, -120, -90, -60, -30)])


def p_nailgungod():
    nail = polygon([(0.66, 0.34), (0.62, 0.32), (0.32, 0.62), (0.3, 0.7), (0.38, 0.68), (0.68, 0.38)])
    head = line(0.6, 0.27, 0.73, 0.4, 0.05)
    return nail | head, line(0.22, 0.58, 0.32, 0.48, 0.025) | line(0.28, 0.72, 0.42, 0.58, 0.025)


def p_triboltgod():
    def bolt(a):
        c, s = math.cos(math.radians(a)), math.sin(math.radians(a))
        cx, cy = 0.5 + c * 0.1, 0.55 + s * 0.1
        return polygon([(cx + c * 0.13, cy + s * 0.13), (cx - s * 0.035, cy + c * 0.035),
                        (cx - c * 0.08, cy - s * 0.08), (cx + s * 0.035, cy - c * 0.035)])
    return line(0.38, 0.72, 0.62, 0.72, 0.04), bolt(-120) | bolt(-90) | bolt(-60)


def p_rocketgod():
    body = polygon([(0.5, 0.22), (0.57, 0.33), (0.57, 0.6), (0.64, 0.68), (0.36, 0.68), (0.43, 0.6), (0.43, 0.33)])
    return body, polygon([(0.45, 0.7), (0.55, 0.7), (0.5, 0.84)])


def p_lightninggod():
    bolt = polygon([(0.56, 0.2), (0.36, 0.53), (0.49, 0.53), (0.42, 0.8), (0.65, 0.44), (0.52, 0.44), (0.6, 0.2)])
    return bolt.outline(0.025), bolt


def p_railgod():
    slug = line(0.25, 0.5, 0.75, 0.5, 0.04)
    pts = [(0.25 + 0.5 * i / 40, 0.5 + 0.08 * math.sin(i / 40 * 4 * math.pi)) for i in range(41)]
    return slug, polyline(pts, 0.03)


def p_damagedealer():
    return burst(0.5, 0.5, 0.25, 0.13, 8, -67.5).outline(0.04), burst(0.5, 0.5, 0.14, 0.07, 8)


def p_netmaster():
    net = None
    for a in (0, 60, 120):
        c, s = math.cos(math.radians(a)), math.sin(math.radians(a))
        for off in (-0.08, 0.08):
            l = line(0.5 - c * 0.2 - s * off, 0.5 - s * 0.2 + c * off, 0.5 + c * 0.2 - s * off, 0.5 + s * 0.2 + c * off, 0.03)
            net = l if net is None else net | l
    return net & circle(0.5, 0.5, 0.22), circle(0.5, 0.5, 0.05)


def p_assistant():
    return arrow(0.24, 0.32, 0.43, 0.46, 0.05) | arrow(0.24, 0.68, 0.43, 0.54, 0.05), circle(0.62, 0.5, 0.08)


# the classic awards and the end-of-match ones

def p_impressive():
    return reticle(0.17), circle(0.5, 0.43, 0.04) | circle(0.5, 0.57, 0.04)


def p_excellent():
    return star(0.4, 0.52, 0.13, 0.055, 5), star(0.62, 0.45, 0.13, 0.055, 5)


def p_gauntlet():
    return p_gauntletgod()


def p_defend():
    s = shield(0.5, 0.5, 0.19)
    c = shield(0.5, 0.5, 0.09)
    return s - c.grow(0.03), c


def p_capture():
    pole = line(0.37, 0.25, 0.37, 0.77, 0.04) | line(0.3, 0.77, 0.44, 0.77, 0.04)
    return pole, polygon([(0.39, 0.27), (0.7, 0.36), (0.39, 0.5)])


def p_assist():
    return p_assistant()


def p_accuracy():
    return reticle(0.17), circle(0.5, 0.5, 0.05)


def p_frags():
    s, eyes = skull(0.5, 0.5, 0.17)
    return s, eyes


def p_victory():
    cup = polygon([(0.33, 0.27), (0.67, 0.27), (0.63, 0.45), (0.55, 0.53), (0.45, 0.53), (0.37, 0.45)])
    stem = box(0.5, 0.6, 0.025, 0.08) | box(0.5, 0.7, 0.11, 0.03, 0.01)
    handles = arc(0.33, 0.36, 0.06, 0.03, 90, 270) | arc(0.67, 0.36, 0.06, 0.03, -90, 90)
    return (cup | stem | handles) - circle(0.5, 0.38, 0.06), circle(0.5, 0.38, 0.04)


MEDALS = [
    "firstblood", "midair", "airborne", "combokill", "collateral", "doublewhammy", "denied", "revenge",
    "partybreaker", "perforated", "pointblank", "precise", "ringout", "stopwatch", "tank", "diehard",
    "fromthegrave", "kamikaze", "showstopper", "triplekill", "overkill", "fighter", "crusher", "exterminator", "demolisher",
    "grimreaper", "avatarofdeath", "killingspree",
    "rage", "frenzy", "rampage", "dominating", "powerupmassacre", "gauntletgod", "machinegungod",
    "shotgungod", "nailgungod", "triboltgod", "rocketgod", "lightninggod", "railgod", "damagedealer",
    "netmaster", "assistant",
    "impressive", "excellent", "gauntlet", "defend", "capture", "assist", "accuracy", "frags", "victory",
]
MASTERS = {"gauntletgod", "machinegungod", "shotgungod", "nailgungod", "triboltgod", "rocketgod",
           "lightninggod", "railgod"}


def make_medal(key):
    c = Canvas(N, N)
    master = key in MASTERS
    seed = sum(map(ord, key))

    # the face, dark with a faint lighter middle
    c.fill(circle(0.5, 0.5, 0.41), lambda x, y: mix((0.13, 0.13, 0.145), INK, smooth(0, 0.4, math.hypot(x - 0.5, y - 0.45))))
    c.fill(ring(0.5, 0.5, 0.392, 0.012), RED_DEEP if not master else RED)

    # the ring: worn metal lit from above; the weapon masters' runs into red
    rim = ring(0.5, 0.5, 0.435, 0.075)
    c.fill(rim.grow(0.006), INK)
    c.fill(rim, worn_metal(seed, SILVER, RED_DEEP if master else GUNMETAL, 0.05, 0.95))
    if master:
        c.glow(ring(0.5, 0.5, 0.392, 0.004), RED, 0.015, 0.6)

    silver, red = globals()["p_" + key]()
    if silver is not None:
        c.fill(silver.grow(0.014), INK)
        c.fill(silver, worn_metal(seed + 1, SILVER, PEWTER, 0.2, 0.8, 0.15))
    if red is not None:
        c.glow(red, RED, 0.03, 0.55)
        c.fill(red.grow(0.01), (0.25, 0.02, 0.03))
        c.fill(red, lambda x, y: mix((1.0, 0.32, 0.3), RED, smooth(0.25, 0.7, y)))
    return c


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    out = os.path.join(sys.argv[1], "gfx", "tremor", "medals")
    os.makedirs(out, exist_ok=True)
    for key in sys.argv[2:] or MEDALS:
        make_medal(key).write(os.path.join(out, key + ".tga"))


if __name__ == "__main__":
    main()
