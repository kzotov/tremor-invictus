"""Champion portraits drawn from shapes: a bust in worn dark metal, lit from
the upper right, with glowing red eyes or visor, on a dark backdrop tinted in
the champion's class colour. Called by make-models.py, which packs them.

Only the champions that have no portrait anywhere else are drawn here; each
design is our own.
"""
import math, os
from tremorart import *

SIZE, SMALL = 256, 64
CLASS_TINT = {"light": (0.30, 0.75, 0.95), "medium": (0.95, 0.75, 0.25), "heavy": (0.90, 0.25, 0.20)}
ARMOUR = (0.30, 0.30, 0.32)
DARK = (0.12, 0.12, 0.13)
CLOTH = (0.20, 0.17, 0.16)
SKIN = (0.42, 0.33, 0.29)
RIM = (1.0, 0.97, 0.9)


def shoulders(w, top=0.72, slope=0.08):
    """A torso across the bottom edge, `w` half-wide at the shoulders."""
    return polygon([(0.5 - w - 0.06, 1.05), (0.5 - w, top + slope), (0.5 - w * 0.55, top),
                    (0.5 + w * 0.55, top), (0.5 + w, top + slope), (0.5 + w + 0.06, 1.05)])


# each design returns a list of (shape, kind): kind is a colour for a lit
# solid, or "glow" for red light

def athena():
    hair = ellipse(0.5, 0.36, 0.15, 0.16) | polygon([(0.36, 0.3), (0.62, 0.22), (0.66, 0.38), (0.6, 0.3)])
    face = ellipse(0.5, 0.42, 0.11, 0.14)
    neck = box(0.5, 0.58, 0.05, 0.06)
    collar = polygon([(0.38, 0.72), (0.43, 0.6), (0.57, 0.6), (0.62, 0.72)])
    visor = box(0.5, 0.41, 0.115, 0.022, 0.01)
    return [(shoulders(0.2, 0.74, 0.05), CLOTH), (neck, SKIN), (collar, ARMOUR), (hair, DARK), (face, SKIN),
            (visor, "glow"), (line(0.4, 0.8, 0.6, 0.8, 0.012), "glow")]


def beastmaster():
    skull = ellipse(0.5, 0.36, 0.12, 0.19) | ellipse(0.5, 0.24, 0.08, 0.12)
    jaw = polygon([(0.4, 0.46), (0.6, 0.46), (0.55, 0.6), (0.45, 0.6)])
    tusks = (polygon([(0.43, 0.5), (0.37, 0.66), (0.33, 0.62), (0.4, 0.49)])
             | polygon([(0.57, 0.5), (0.63, 0.66), (0.67, 0.62), (0.6, 0.49)]))
    hump = polygon([(0.14, 1.05), (0.2, 0.74), (0.36, 0.62), (0.64, 0.62), (0.8, 0.74), (0.86, 1.05)])
    eyes = circle(0.45, 0.37, 0.022) | circle(0.55, 0.37, 0.022) | circle(0.5, 0.3, 0.018)
    return [(hump, (0.24, 0.22, 0.18)), (skull | jaw, (0.33, 0.30, 0.24)),
            (tusks, (0.62, 0.58, 0.5)), (eyes, "glow")]


def galena():
    hood = polygon([(0.5, 0.12), (0.7, 0.42), (0.68, 0.66), (0.32, 0.66), (0.3, 0.42)])
    opening = ellipse(0.5, 0.45, 0.1, 0.14)
    robe = shoulders(0.24, 0.66, 0.1)
    pendant = ring(0.5, 0.8, 0.045, 0.014)
    eyes = ellipse(0.46, 0.44, 0.02, 0.012) | ellipse(0.54, 0.44, 0.02, 0.012)
    return [(robe, CLOTH), (hood - opening, (0.24, 0.2, 0.22)), (opening, (0.04, 0.03, 0.04)),
            (pendant, "glow"), (eyes, "glow")]


def scalebearer():
    helm = ellipse(0.5, 0.4, 0.14, 0.15) | box(0.5, 0.5, 0.12, 0.08, 0.03)
    horns = (polygon([(0.38, 0.33), (0.22, 0.22), (0.2, 0.32), (0.36, 0.4)])
             | polygon([(0.62, 0.33), (0.78, 0.22), (0.8, 0.32), (0.64, 0.4)]))
    pauldrons = ellipse(0.24, 0.72, 0.17, 0.12) | ellipse(0.76, 0.72, 0.17, 0.12)
    slit = box(0.5, 0.45, 0.08, 0.012)
    return [(shoulders(0.3, 0.68, 0.06), DARK), (pauldrons, ARMOUR), (horns, (0.55, 0.5, 0.42)),
            (helm, ARMOUR), (slit, "glow")]


def unumrey():
    halo = arc(0.5, 0.36, 0.25, 0.03, 200, 340)
    head = ellipse(0.5, 0.4, 0.09, 0.17)
    collar = polygon([(0.34, 0.74), (0.4, 0.55), (0.6, 0.55), (0.66, 0.74)])
    rift = polygon([(0.5, 0.3), (0.515, 0.42), (0.5, 0.5), (0.485, 0.42)])
    return [(halo, "glow"), (shoulders(0.2, 0.72, 0.08), CLOTH), (collar, ARMOUR), (head, (0.26, 0.25, 0.28)),
            (rift, "glow")]


def wrack():
    helm = ellipse(0.5, 0.4, 0.14, 0.16)
    brow = box(0.5, 0.36, 0.14, 0.03, 0.01)
    visor = box(0.5, 0.42, 0.1, 0.018) | box(0.5, 0.47, 0.016, 0.05)
    plates = union(*[polygon([(0.5 + s * 0.12, 0.64 + k * 0.05), (0.5 + s * 0.36, 0.68 + k * 0.05),
                              (0.5 + s * 0.38, 0.76 + k * 0.05), (0.5 + s * 0.13, 0.72 + k * 0.05)])
                     for s in (-1, 1) for k in (0, 1, 2)])
    gorget = box(0.5, 0.6, 0.1, 0.05, 0.02)
    return [(shoulders(0.27, 0.7, 0.07), DARK), (plates, ARMOUR), (gorget, DARK), (helm, ARMOUR),
            (brow, (0.38, 0.38, 0.4)), (visor, "glow")]


def xaero():
    crest = polygon([(0.5, 0.06), (0.56, 0.2), (0.53, 0.32), (0.47, 0.32), (0.44, 0.2)])
    helm = ellipse(0.5, 0.38, 0.13, 0.15)
    cheeks = polygon([(0.38, 0.4), (0.62, 0.4), (0.57, 0.56), (0.43, 0.56)])
    band = box(0.5, 0.345, 0.128, 0.012)
    collar = polygon([(0.3, 0.74), (0.38, 0.58), (0.62, 0.58), (0.7, 0.74)])
    sash = polygon([(0.36, 0.76), (0.44, 0.76), (0.66, 1.05), (0.58, 1.05)])
    eyes = (polygon([(0.43, 0.4), (0.48, 0.42), (0.43, 0.425)]) | polygon([(0.57, 0.4), (0.52, 0.42), (0.57, 0.425)]))
    return [(shoulders(0.26, 0.72, 0.06), (0.16, 0.15, 0.16)), (sash, (0.4, 0.08, 0.08)), (collar, ARMOUR),
            (crest, (0.45, 0.42, 0.38)), (helm | cheeks, (0.22, 0.22, 0.24)), (band, (0.6, 0.48, 0.25)),
            (eyes, "glow")]


DESIGNS = {
    "athena": ("light", athena), "beastmaster": ("heavy", beastmaster), "galena": ("medium", galena),
    "scalebearer": ("heavy", scalebearer), "unumrey": ("medium", unumrey), "wrack": ("medium", wrack),
    "xaero": ("heavy", xaero),
}


def portrait(name):
    cls, design = DESIGNS[name]
    tint = CLASS_TINT[cls]
    seed = sum(map(ord, name))
    c = Canvas(SIZE, SIZE, (0, 0, 0, 1))
    grain = Fbm(seed, 3, 6)

    def back(x, y, p):
        d = math.hypot(x - 0.62, y - 0.35)
        k = 0.06 + 0.22 * (1 - smooth(0.0, 0.75, d)) + 0.03 * (grain.at(x, y) - 0.5)
        v = 1 - 0.5 * smooth(0.35, 0.75, max(abs(x - 0.5), abs(y - 0.5)))
        return (BACK[0] + tint[0] * k * v, BACK[1] + tint[1] * k * v, BACK[2] + tint[2] * k * v, 1)
    c.shade(back)
    c.fill(regular(0.5, 0.42, 0.36, 6, 0).outline(0.008), scale(tint, 0.35))

    for i, (shape, kind) in enumerate(design()):
        # the designs are drawn small; close in on the head
        shape = shape.scaled(1.3, 0.5, 0.12)
        if kind == "glow":
            c.glow(shape, RED, 0.025, 0.7)
            c.fill(shape, (1.0, 0.42, 0.36))
        else:
            c.fill(shape.grow(0.006), (0.02, 0.02, 0.025))
            c.fill_lit(shape, kind, mix(RIM, tint, 0.3), rim_width=0.03, seed=seed + i)

    # a frame like the menus': a thin rule, the class colour along the bottom
    c.fill(box(0.5, 0.5, 0.5, 0.5).outline(0.012), (0.08, 0.08, 0.09))
    c.fill(box(0.5, 0.985, 0.5, 0.015), tint)
    return c


def downsample(px, n, f):
    """Box filter an n x n picture by f."""
    m = n // f
    out = []
    for j in range(m):
        for i in range(m):
            acc = [0.0, 0.0, 0.0, 0.0]
            for y in range(j * f, j * f + f):
                for x in range(i * f, i * f + f):
                    p = px[y * n + x]
                    for k in range(4):
                        acc[k] += p[k]
            out.append([a / (f * f) for a in acc])
    return out


def write_portraits(root):
    """gfx/champions/<name>.tga (256) and <name>_small.tga (64) under root."""
    d = os.path.join(root, "gfx", "champions")
    os.makedirs(d, exist_ok=True)
    for name in DESIGNS:
        c = portrait(name)
        write_tga(os.path.join(d, name + ".tga"), SIZE, SIZE, c.px, alpha=False)
        write_tga(os.path.join(d, name + "_small.tga"), SMALL, SMALL, downsample(c.px, SIZE, SIZE // SMALL), alpha=False)
