"""Generate the HUD icon set: weapons, ammo, abilities, passives, items,
kill-feed causes and small HUD glyphs.

Usage: python3 make-icons.py OUT_DIR [group[:key,key...] ...]

Writes, under OUT_DIR/gfx/tremor/icons/:
  weapons/<key>.tga     256 x 128 side views facing right (column, kill feed,
                        pickups, best weapon)
  ammo/<key>.tga        128 x 128 ammo glyphs, drawn to sit inside hud/ammo_ring
  abilities/<key>.tga   128 x 128 glyphs for the ability disc and charge pips
  passives/<key>.tga    128 x 128 glyphs for the passive ring
  items/<key>.tga       128 x 128 pickups, power-ups and objectives
  causes/<key>.tga      128 x 128 kill-feed causes that are not weapons
  glyphs/<key>.tga      128 x 128 HUD marks (tick, lock, network, status, stats)

Every icon is one flat white tone with a lighter rim facing the upper left
and a darker one facing the lower right, with darker panel lines, so the
code tints it (weapon hue, ammo hue, ability state, team colour). Shapes are
laid out on the style grid: 64 x 64 for square icons (keyline circle 56),
128 x 64 for weapons; a 45 degree chamfer of 4 is the family motif.
All drawn here from shapes; nothing traced.
"""
import math, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tremorart import *

G = 64.0            # grid cells per unit
LINE = 1.4 / G      # interior panel line
DETAIL = 0.62       # grey of panel lines, against the 0.86 fill


def g(v):
    return v / G


def P(*pts):
    """A polygon given in grid cells."""
    return polygon([(x / G, y / G) for x, y in pts])


def R(x0, y0, x1, y1, cut=0.0, corners="tl"):
    """A rectangle in grid cells, chamfered at the named corners."""
    return chamfered(x0 / G, y0 / G, x1 / G, y1 / G, cut / G, corners)


def C(cx, cy, r):
    return circle(cx / G, cy / G, r / G)


def RG(cx, cy, r, w):
    return ring(cx / G, cy / G, r / G, w / G)


def L(ax, ay, bx, by, w):
    return line(ax / G, ay / G, bx / G, by / G, w / G)


def PL(pts, w):
    return polyline([(x / G, y / G) for x, y in pts], w / G)


def A(cx, cy, r, w, a0, a1):
    return arc(cx / G, cy / G, r / G, w / G, a0, a1)


def E(cx, cy, rx, ry):
    return ellipse(cx / G, cy / G, rx / G, ry / G)


def RB(cx, cy, hw, hh, r=0.0):
    return box(cx / G, cy / G, hw / G, hh / G, r / G)


def ARROW(ax, ay, bx, by, w=4, head=9):
    return arrow(ax / G, ay / G, bx / G, by / G, w / G, head / G)


def lines(*segs, w=1.4):
    """Panel lines: segments of (ax, ay, bx, by) in grid cells."""
    return union(*[L(*s, w) for s in segs])


class Icon:
    """A body shape plus darker panel lines and lighter highlights."""

    def __init__(self, body, detail=None, light=None):
        self.body, self.detail, self.light = body, detail, light


def render(icon, w, h, span):
    c = Canvas(w, h, span=span)
    c.fill_flat(icon.body, base=0.84, light=1.0, dark=0.56, rim=0.035)
    clip = icon.body.grow(-0.6 / G)
    if icon.detail is not None:
        c.fill(icon.detail & clip, (DETAIL,) * 3)
    if icon.light is not None:
        c.fill(icon.light & clip, (1.0, 1.0, 1.0))
    return c


# ================================================================ weapons
# 128 x 64 grid, muzzle to the right, 2 cells of padding


def w_melee():
    cuff = R(6, 22, 42, 46, 4, "tl bl")
    housing = C(46, 34, 13)
    hand = R(52, 18, 84, 50, 4, "tr br")
    blades = (P((82, 19), (122, 25), (82, 29)) | P((82, 30), (124, 34), (82, 38))
              | P((82, 39), (118, 44), (82, 49)))
    body = (cuff | housing | hand | blades) - C(46, 34, 4.5)
    detail = (lines((16, 23, 16, 45), (26, 23, 26, 45), (60, 26, 80, 26), (60, 34, 80, 34), (60, 42, 80, 42))
              | RG(46, 34, 8.5, 1.4))
    return Icon(body, detail)


def w_machinegun():
    stock = R(4, 20, 34, 40, 4, "tl bl") - R(12, 25, 27, 34, 2, "tl br")
    receiver = R(32, 18, 86, 33, 4, "tr")
    guard = R(80, 21, 106, 31, 3, "tr br")
    barrel = R(104, 24, 124, 28)
    sight = R(44, 13, 56, 18, 2, "tl tr") | R(92, 15, 96, 21)
    mag = P((60, 32), (72, 32), (74, 54), (62, 54))
    grip = P((40, 32), (50, 32), (46, 50), (36, 50))
    trigger = A(54, 33, 5, 2, 20, 160)
    body = stock | receiver | guard | barrel | sight | mag | grip | trigger
    detail = lines((40, 25, 78, 25), (84, 26, 102, 26), (63, 41, 73, 41))
    return Icon(body, detail)


def w_hmg():
    stock = P((4, 22), (30, 20), (30, 38), (4, 42))
    receiver = R(28, 17, 72, 35, 4, "tr")
    shroud = R(68, 18, 116, 34, 4, "tr br") - union(*[RB(78 + i * 9, 26, 2.5, 3.5, 1.2) for i in range(4)])
    barrels = R(114, 20, 124, 24) | R(114, 28, 124, 32)
    rail = R(36, 13, 66, 17)
    scope = R(40, 6, 62, 12, 3, "tl tr bl br")
    drum = C(56, 42, 11)
    grip = P((32, 34), (42, 34), (38, 50), (28, 50))
    body = stock | receiver | shroud | barrels | rail | scope | drum | grip
    detail = lines((10, 26, 26, 25), (40, 24, 66, 24)) | RG(56, 42, 5.5, 1.4)
    return Icon(body, detail)


def w_shotgun():
    stock = P((4, 24), (36, 20), (38, 32), (10, 42), (4, 42))
    receiver = R(34, 18, 62, 33, 4, "tr")
    barrel = R(58, 21, 124, 27)
    tube = R(58, 28, 112, 32)
    pump = R(74, 26, 100, 37, 3, "tl tr bl br")
    grip = P((40, 32), (50, 32), (46, 46), (36, 46))
    sight = R(120, 18, 123, 21)
    trigger = A(54, 33, 4.5, 2, 20, 160)
    body = stock | receiver | barrel | tube | pump | grip | sight | trigger
    detail = lines((80, 28, 80, 35), (85, 28, 85, 35), (90, 28, 90, 35), (95, 28, 95, 35), (40, 25, 58, 25))
    return Icon(body, detail)


def w_ssg():
    stock = P((12, 34), (40, 22), (44, 38), (34, 42), (30, 54), (18, 54), (20, 44), (12, 46))
    receiver = R(38, 18, 62, 40, 4, "tl bl")
    barrels = R(58, 17, 108, 28) | R(58, 29, 108, 40)
    muzzle = R(104, 15, 114, 42, 3, "tr br")
    hinge = C(62, 40, 6)
    grip = R(70, 38, 92, 45, 3, "bl br")
    body = stock | receiver | barrels | muzzle | hinge | grip
    detail = lines((64, 28.5, 102, 28.5), (46, 24, 46, 36), (76, 40, 76, 44), (82, 40, 82, 44)) | C(62, 40, 2)
    return Icon(body, detail)


def w_nailgun():
    body_ = R(26, 18, 74, 44, 6, "tl tr bl br")
    handle = PL([(34, 18), (38, 9), (62, 9), (66, 18)], 5)
    tubes = R(72, 19, 120, 24) | R(72, 27, 122, 32) | R(72, 35, 120, 40)
    bands = R(84, 16, 90, 43, 2, "tl tr bl br") | R(104, 17, 109, 42, 2, "tl tr bl br")
    grip = P((40, 42), (52, 42), (48, 56), (36, 56))
    stock = R(8, 24, 28, 38, 4, "tl bl")
    body = body_ | handle | tubes | bands | grip | stock
    detail = lines((34, 26, 66, 26), (34, 36, 66, 36), (90, 25.5, 104, 25.5), (90, 33.5, 104, 33.5))
    return Icon(body, detail)


def w_sng():
    hull = R(16, 16, 70, 50, 6, "tl bl")
    head = R(66, 12, 112, 52, 5, "tr br")
    collar = R(108, 16, 124, 48, 4, "tr br")
    handle = PL([(26, 16), (30, 6), (56, 6), (60, 16)], 5)
    grip = P((30, 48), (44, 48), (40, 60), (26, 60))
    body = hull | head | collar | handle | grip
    detail = (lines((72, 20, 106, 20), (72, 28, 106, 28), (72, 36, 106, 36), (72, 44, 106, 44),
                    (24, 26, 60, 26), (24, 38, 60, 38))
              | C(116, 24, 2.2) | C(116, 32, 2.2) | C(116, 40, 2.2))
    return Icon(body, detail)


def w_tribolt():
    frame = P((20, 26), (80, 10), (92, 10), (92, 52), (80, 52), (20, 38))
    stock = R(6, 26, 22, 38, 3, "tl bl")
    bolts = union(*[R(90, y - 2, 110, y + 2) | P((108, y - 5), (124, y), (108, y + 5)) for y in (16, 31, 46)])
    grip = P((38, 36), (50, 38), (46, 54), (34, 54))
    body = frame | stock | bolts | grip
    detail = lines((30, 31, 84, 18), (30, 33, 84, 44), (84, 14, 84, 48))
    return Icon(body, detail)


def w_rocket():
    tube = R(22, 18, 112, 36)
    muzzle = P((108, 16), (124, 11), (124, 43), (108, 38))
    exhaust = P((4, 14), (26, 18), (26, 36), (4, 40))
    sight = R(52, 10, 72, 18, 3, "tl tr")
    grips = P((44, 34), (54, 34), (50, 52), (40, 52)) | R(76, 34, 84, 46, 2, "bl br")
    body = (tube | muzzle | exhaust | sight | grips) - R(10, 24, 18, 30)
    detail = lines((32, 19, 32, 35), (96, 19, 96, 35), (36, 27, 92, 27))
    return Icon(body, detail)


def w_lightning():
    body_ = R(18, 18, 66, 42, 5, "tl tr bl br")
    rod = R(62, 27, 104, 33)
    coil = union(*[RB(70 + i * 7, 30, 2.2, 10, 1.5) for i in range(5)])
    prongs = (PL([(100, 26), (112, 22), (124, 24)], 4) | PL([(100, 34), (112, 38), (124, 36)], 4))
    grip = P((30, 40), (42, 40), (38, 56), (26, 56))
    stock = R(6, 22, 20, 36, 3, "tl bl")
    body = body_ | rod | coil | prongs | grip | stock
    detail = lines((24, 30, 60, 30)) | C(48, 30, 3.5)
    return Icon(body, detail)


def w_railgun():
    stock = P((4, 26), (22, 22), (22, 38), (4, 40))
    body_ = R(20, 20, 62, 38, 4, "tr")
    rails = R(58, 20, 124, 24) | R(58, 32, 122, 36)
    spacer = R(58, 24, 70, 32) | R(96, 24, 99, 32)
    coil = RB(34, 29, 5, 13, 3)
    grip = P((40, 36), (50, 36), (46, 52), (36, 52))
    body = (stock | body_ | rails | spacer | coil | grip) - R(72, 26, 94, 30)
    detail = lines((31, 18, 31, 40), (37, 18, 37, 40), (44, 26, 60, 26))
    return Icon(body, detail)


WEAPONS = {
    "melee": w_melee, "machinegun": w_machinegun, "hmg": w_hmg, "shotgun": w_shotgun, "ssg": w_ssg,
    "nailgun": w_nailgun, "sng": w_sng, "tribolt": w_tribolt, "rocket": w_rocket,
    "lightning": w_lightning, "railgun": w_railgun,
}


# ================================================================ shared parts
# 64 x 64 grid, keyline circle of radius 28 around (32, 32)


def cross(cx, cy, r, arm):
    """Our health cross: four arms that flare towards their ends, with
    concave sides; r is the half span, arm the half width at the ends."""
    w = arm * 0.92                                      # the half waist where the arms meet
    d = (r * r + arm * arm - 2 * w * w) / (2 * (r + arm) - 4 * w)
    rho = math.sqrt(2) * (d - w)
    bites = union(*[C(cx + sx * d, cy + sy * d, rho) | RB(cx + sx * (d + r) / 2 + sx, cy + sy * (d + r) / 2 + sy, (r - d) / 2 + 1, (r - d) / 2 + 1)
                    for sx in (-1, 1) for sy in (-1, 1)])
    return RB(cx, cy, r, r) - bites


def armour_shield(cx, cy, w, h, notches=3):
    """Flat top with notches, pointed base."""
    x0, x1, y0 = cx - w / 2, cx + w / 2, cy - h / 2
    s = P((x0, y0), (x1, y0), (x1, cy + h * 0.08), (cx, cy + h / 2), (x0, cy + h * 0.08))
    if notches:
        step = w / (notches + 1)
        nw = w * 0.07
        for i in range(notches):
            nx = x0 + step * (i + 1)
            s = s - P((nx - nw, y0 - 1), (nx + nw, y0 - 1), (nx, y0 + h * 0.14))
    return s


def drop(cx, cy, r):
    """A teardrop, point up."""
    return C(cx, cy, r) | P((cx - r * 0.86, cy - r * 0.5), (cx, cy - r * 2.1), (cx + r * 0.86, cy - r * 0.5))


TONGUE = [(0, 1), (-0.5, 0.82), (-0.68, 0.45), (-0.6, 0.05), (-0.36, -0.36), (-0.06, -0.72), (0.08, -1.0),
          (0.16, -0.66), (0.4, -0.36), (0.6, 0.02), (0.66, 0.45), (0.46, 0.84)]


def tongue(cx, cy, w, h, lean=0.0):
    """One flame tongue in a w x h box centred on (cx, cy), tip up, leaning right by `lean`."""
    return P(*[(cx + (x + lean * (1 - y) * 0.5 * (y < 0)) * w / 1.36, cy + y * h / 2) for x, y in TONGUE])


def flame(cx, cy, s, tongues=3):
    """A flame of 1-4 tongues: (body, inner core). s is about the half height."""
    layout = {1: [(0, 1.0, 0.1)], 2: [(-0.3, 0.8, -0.1), (0.25, 1.0, 0.15)],
              3: [(-0.42, 0.7, -0.25), (0.02, 1.0, 0.05), (0.42, 0.75, 0.25)],
              4: [(-0.52, 0.62, -0.3), (-0.17, 0.92, -0.1), (0.2, 1.0, 0.1), (0.52, 0.7, 0.3)]}[tongues]
    sh = tongue(cx, cy + s * 0.35, s * 1.2, s * 1.3)
    for dx, hgt, lean in layout:
        h = s * 2 * hgt
        sh = sh | tongue(cx + dx * s, cy + s - h / 2, s * (1.0 if tongues > 1 else 1.35), h, lean)
    inner = tongue(cx, cy + s * 0.45, s * 0.6, s * 1.0, 0.1)
    return sh, inner


def skullg(cx, cy, s):
    head = C(cx, cy - s * 0.12, s) | RB(cx, cy + s * 0.68, s * 0.56, s * 0.36, s * 0.12)
    eyes = P((cx - s * 0.62, cy - s * 0.05), (cx - s * 0.14, cy - s * 0.05), (cx - s * 0.2, cy + s * 0.32), (cx - s * 0.5, cy + s * 0.32)) \
        | P((cx + s * 0.62, cy - s * 0.05), (cx + s * 0.14, cy - s * 0.05), (cx + s * 0.2, cy + s * 0.32), (cx + s * 0.5, cy + s * 0.32))
    nose = P((cx, cy + s * 0.42), (cx - s * 0.1, cy + s * 0.6), (cx + s * 0.1, cy + s * 0.6))
    teeth = union(*[RB(cx + dx * s, cy + s * 0.95, s * 0.04, s * 0.14) for dx in (-0.25, 0, 0.25)])
    return head - eyes - nose - teeth


def speed_lines(x0, x1, ys, w=3):
    return union(*[L(x0 + i * 3, y, x1, y, w) for i, y in enumerate(ys)])


def fist(cx, cy, s):
    """A clenched fist from the back, knuckles up."""
    k = union(*[RB(cx - s * 0.6 + i * s * 0.4, cy - s * 0.55, s * 0.19, s * 0.3, s * 0.15) for i in range(4)])
    palm = RB(cx, cy + s * 0.15, s * 0.8, s * 0.6, s * 0.18)
    thumb = RB(cx - s * 0.35, cy + s * 0.02, s * 0.6, s * 0.17, s * 0.16).rotated(-12, g(cx), g(cy))
    cuff = RB(cx + s * 0.05, cy + s * 0.95, s * 0.6, s * 0.22, 0.05 * s)
    body = k | palm | cuff
    detail = thumb.outline(1.4 / G) | lines((cx - s * 0.2, cy - s * 0.3, cx - s * 0.2, cy - s * 0.12),
                                         (cx + s * 0.2, cy - s * 0.3, cx + s * 0.2, cy - s * 0.12))
    return body, detail


def crate(cx, cy, s):
    face = R(cx - s, cy - s, cx + s, cy + s, s * 0.22, "tl tr bl br")
    detail = (R(cx - s * 0.72, cy - s * 0.72, cx + s * 0.72, cy + s * 0.72, s * 0.15, "tl tr bl br").outline(1.4 / G)
              | L(cx - s * 0.62, cy - s * 0.62, cx + s * 0.62, cy + s * 0.62, 1.4))
    return face, detail


def hexagon(cx, cy, r, rot=30):
    return regular(cx / G, cy / G, r / G, 6, rot)


# ================================================================ ammo
# glyphs filling the 64 grid; the code draws them at 40 inside the ring


def am_bullets():
    def cart(x):
        return R(x - 4.5, 26, x + 4.5, 54, 1.5, "bl br") | P((x - 4.5, 27), (x - 4, 18), (x, 10), (x + 4, 18), (x + 4.5, 27))
    body = cart(18) | cart(32) | cart(46)
    return Icon(body, lines((13.5, 26, 22.5, 26), (27.5, 26, 36.5, 26), (41.5, 26, 50.5, 26),
                            (13.5, 48, 22.5, 48), (27.5, 48, 36.5, 48), (41.5, 48, 50.5, 48)))


def am_shells():
    def shell(x):
        return R(x - 8, 10, x + 8, 44, 3, "tl tr") | R(x - 9.5, 44, x + 9.5, 54, 1.5, "bl br")
    body = shell(21) | shell(43)
    return Icon(body, lines((13, 44, 29, 44), (35, 44, 51, 44), (13, 20, 29, 20), (35, 20, 51, 20)))


def am_nails():
    def nail(x):
        n = R(x - 2.5, 14, x + 2.5, 46) | P((x - 2.5, 46), (x, 55), (x + 2.5, 46)) | R(x - 7, 9, x + 7, 14, 1.5, "tl tr bl br")
        return n.rotated(18, g(x), g(32))
    return Icon(nail(18) | nail(32) | nail(46))


def am_bolts():
    tip = P((32, 6), (42, 22), (36, 22), (36, 46), (28, 46), (28, 22), (22, 22))
    fins = P((28, 40), (18, 56), (28, 52)) | P((36, 40), (46, 56), (36, 52)) | R(30, 44, 34, 58)
    body = tip | fins
    side = (P((14, 18), (20, 28), (17, 28), (17, 44), (11, 44), (11, 28), (8, 28))
            | P((50, 18), (56, 28), (53, 28), (53, 44), (47, 44), (47, 28), (44, 28)))
    return Icon(body | side, lines((30, 26, 34, 26)))


def am_rockets():
    body = (R(25, 18, 39, 46) | P((25, 18), (32, 4), (39, 18)) | P((25, 36), (15, 52), (15, 58), (25, 52))
            | P((39, 36), (49, 52), (49, 58), (39, 52)) | R(28, 46, 36, 56))
    return Icon(body, lines((25.5, 20, 38.5, 20), (25.5, 30, 38.5, 30)) | C(32, 25, 2))


def am_cells():
    batt = R(16, 12, 48, 58, 4, "tl tr bl br") | R(25, 6, 39, 12)
    bolt = P((35, 16), (23, 37), (31, 37), (27, 54), (41, 31), (33, 31), (38, 16))
    return Icon(batt - bolt.grow(1.8 / G) | bolt)


def am_slugs():
    rod = R(28, 4, 36, 60, 2, "tl tr bl br")
    coil = union(*[(E(32, 12 + i * 10, 13, 4.5) - E(32, 12 + i * 10, 10, 2.2)).rotated(-14, g(32), g(12 + i * 10))
                    for i in range(5)])
    return Icon(rod | coil, lines((32, 6, 32, 58)))


AMMO = {"bullets": am_bullets, "shells": am_shells, "nails": am_nails, "bolts": am_bolts,
        "rockets": am_rockets, "cells": am_cells, "slugs": am_slugs}


# ================================================================ abilities


def ab_hook():
    shank = R(29, 14, 35, 40)
    bend = A(22, 40, 10, 6, -10, 180)
    barb = P((8, 40), (12, 30), (17, 40))
    eye = RG(32, 10, 5, 3.4)
    rope = A(43, 10, 6, 3, 180, 360) | A(51, 16, 6, 3, 0, 180) | L(57, 16, 58, 30, 3)
    return Icon(shank | bend | barb | eye | rope, lines((30, 22, 34, 22)))


def ab_hookpip():
    return Icon(R(28, 10, 36, 38) | A(22, 38, 12, 8, -10, 180) | P((5, 38), (10, 24), (16, 38)))


def ab_bullrush():
    pad = P((26, 18), (44, 16), (54, 26), (54, 44), (22, 48), (18, 32))
    horn = P((46, 18), (58, 8), (62, 12), (54, 26))
    body = pad | horn
    detail = lines((26, 30, 50, 28), (26, 39, 50, 37))
    return Icon(body | speed_lines(2, 14, (24, 33, 42)), detail)


def ab_injection():
    syringe = (R(20, 14, 36, 42, 2, "tl tr bl br") | R(25, 6, 31, 14) | R(19, 3, 37, 7, 1.5, "tl tr bl br")
               | R(26, 42, 30, 58)).rotated(40, g(28), g(30))
    fluid = R(23, 26, 33, 39).rotated(40, g(28), g(30))
    plus = RB(48, 46, 10, 3.2) | RB(48, 46, 3.2, 10)
    return Icon(syringe | plus, fluid)


def ab_dualwield():
    def gun(flip):
        sh = R(0, -9, 44, 0, 3, "br") | P((2, -8), (13, -8), (10, -24), (-1, -24)) | A(17, -9, 4, 2, 200, 340)
        ln = L(4, -4, 40, -4, 1.4)
        sh, ln = sh.rotated(-60, 0, 0), ln.rotated(-60, 0, 0)
        if flip:
            sh, ln = sh.mirrored(0), ln.mirrored(0)
        dx, dy = (g(20), g(52)) if not flip else (g(44), g(52))
        return sh.moved(dx, dy), ln.moved(dx, dy)
    a, la = gun(False)
    b, lb = gun(True)
    return Icon(a | b, la | lb)


def ab_barrier():
    hexs = hexagon(38, 32, 22, 0) - hexagon(38, 32, 14, 0)
    dot = C(14, 32, 8)
    return Icon(hexs | dot, hexagon(38, 32, 18, 0).outline(1.4 / G))


def ab_flamestrike():
    out = None
    inner = None
    for ang in (-36, 0, 36):
        f, i = flame(32, 25, 13, 1)
        f, i = f.rotated(ang, g(32), g(58)), i.rotated(ang, g(32), g(58))
        out = f if out is None else out | f
        inner = i if inner is None else inner | i
    return Icon(out, inner)


def ab_berserk():
    body, detail = fist(36, 30, 18)
    return Icon(body | speed_lines(2, 13, (20, 31, 42)), detail)


def ab_turret():
    head = R(20, 12, 50, 30, 4, "tl tr")
    barrel = R(48, 17, 60, 23)
    legs = PL([(35, 30), (35, 40)], 5) | PL([(35, 38), (18, 58)], 4.5) | PL([(35, 38), (52, 58)], 4.5) | L(35, 38, 35, 58, 4.5)
    return Icon(head | barrel | legs, lines((24, 22, 44, 22)) | C(30, 21, 2.5))


def ab_totem():
    stake = R(29, 22, 35, 50) | P((29, 50), (32, 60), (35, 50))
    prongs = (P((26, 24), (32, 4), (38, 24)) | PL([(30, 26), (18, 18), (16, 6)], 4.5) | PL([(34, 26), (46, 18), (48, 6)], 4.5))
    return Icon(stake | prongs | R(24, 24, 40, 30, 2, "tl tr bl br"), lines((30, 38, 34, 38)))


def ab_totempip():
    return Icon(P((24, 52), (32, 6), (40, 52)))


def ab_grenades():
    ball = C(30, 38, 18)
    cap = R(24, 14, 38, 22, 2, "tl tr")
    spoon = PL([(38, 17), (48, 14), (52, 24), (48, 38)], 4.5)
    pin = RG(20, 16, 5, 2.6)
    return Icon(ball | cap | spoon | pin, lines((14, 38, 46, 38), (30, 22, 30, 54)))


def ab_grenadepip():
    return Icon(C(32, 34, 20) | R(25, 8, 39, 16, 2, "tl tr"))


def ab_ghostwalk():
    hood = C(32, 21, 14) | P((18, 21), (46, 21), (50, 36), (60, 58), (4, 58), (14, 36))
    rim = hood - hood.grow(-4.5 / G)
    cuts = union(*[RB(32, 6, 1.8, 40).rotated(a, 0.5, g(34)) for a in range(15, 360, 30)]) - halfplane(0, -1, -g(55))
    eyes = P((22, 22), (30, 25), (29, 28), (22, 26)) | P((42, 22), (34, 25), (35, 28), (42, 26))
    return Icon((rim - cuts) | eyes)


def ab_orb():
    sphere = C(32, 32, 14)
    orbit = (E(32, 32, 28, 9) - E(32, 32, 24, 5.5)).rotated(-25, 0.5, 0.5)
    gap = (E(32, 32, 30, 11) - E(32, 32, 22, 4)).rotated(-25, 0.5, 0.5) & halfplane(0, 1, g(32))
    body = (sphere - gap.grow(1.0 / G)) | (orbit - (sphere.grow(2.0 / G) & halfplane(0, -1, -g(32))))
    return Icon(body, None, C(26, 26, 3.5))


def ab_plasmatrail():
    blade = P((28, 40), (58, 40), (60, 44), (28, 44))
    boot = P((32, 16), (46, 16), (48, 30), (56, 34), (56, 40), (30, 40))
    trail = PL([(4, 30), (10, 26), (16, 32), (22, 28), (28, 34)], 3.5) | PL([(4, 44), (10, 40), (16, 46), (24, 42)], 3.5)
    return Icon(blade | boot | trail, lines((36, 26, 46, 26), (32, 34, 52, 34)))


def ab_acidspit():
    arc_ = A(32, 32, 24, 5, 200, 340)
    drops = drop(18, 34, 5.5) | drop(32, 44, 6.5) | drop(46, 34, 5.5)
    return Icon(arc_ | drops)


def ab_drone():
    hull = E(32, 36, 15, 10)
    arms = L(10, 24, 54, 24, 4) | L(32, 26, 32, 28, 4)
    rotors = E(10, 20, 9, 2.6) | E(54, 20, 9, 2.6) | R(8, 20, 12, 26) | R(52, 20, 56, 26)
    lens = C(32, 38, 6)
    return Icon((hull | arms | rotors) - lens.grow(1.8 / G) | lens, None, C(30, 36, 1.8))


def ab_piercingsight():
    eye = (A(32, 56, 34, 5, 225, 315) | A(32, 8, 34, 5, 45, 135))
    iris = RG(32, 32, 9, 4.5)
    spiral = A(32, 32, 4, 2.5, 0, 270) | C(32, 32, 1.4)
    lens = RG(32, 32, 28, 3)
    return Icon(eye | iris | spiral | lens)


def ab_shellshock():
    shell = C(32, 34, 14)
    crack = PL([(24, 22), (30, 32), (26, 38), (34, 46)], 2.2)
    waves = A(32, 34, 21, 3.5, 200, 340) | A(32, 34, 27, 3, 210, 330)
    return Icon((shell - crack) | waves)


def ab_spinevolley():
    out = None
    for ang in (-36, -12, 12, 36):
        sp = P((28.5, 52), (32, 4), (35.5, 52)).rotated(ang, g(32), g(58))
        out = sp if out is None else out | sp
    return Icon(out | C(32, 55, 7), C(32, 55, 3))


def ab_masterofdeath():
    sk = skullg(32, 36, 13)
    halo = A(32, 34, 25, 4, 150, 390)
    head = P((48, 46), (56, 46), (52, 54))
    return Icon(sk | halo | head)


def ab_supplydrop():
    canopy = A(32, 30, 22, 0.1, 180, 360).grow(0) | (C(32, 30, 22) & halfplane(0, 1, g(30)))
    canopy = canopy - union(*[E(x, 30, 5.5, 4) for x in (16, 32, 48)])
    cords = L(12, 30, 26, 44, 2) | L(52, 30, 38, 44, 2) | L(32, 30, 32, 44, 2)
    box_, detail = crate(32, 52, 9)
    return Icon(canopy | cords | box_, detail)


def ab_markprey():
    shaft = L(8, 56, 38, 26, 4)
    head = P((34, 22), (48, 16), (42, 30))
    fl = P((7, 50), (4, 42), (14, 48)) | P((14, 57), (22, 60), (16, 50))
    target = RG(45, 19, 13, 3)
    return Icon(shaft | head | fl | target)


def ab_downpour():
    cloud = C(22, 24, 10) | C(36, 18, 13) | C(48, 26, 9) | R(14, 24, 54, 35, 4, "bl br")
    rain = union(*[L(x, 42, x - 4, 56, 3.5) for x in (20, 32, 44)])
    return Icon(cloud | rain)


def ab_hound():
    pad = E(32, 42, 13, 11)
    toes = E(14, 26, 5, 6.5).rotated(-20, g(14), g(26)) | E(26, 16, 5.5, 7) | E(39, 16, 5.5, 7) \
        | E(51, 26, 5, 6.5).rotated(20, g(51), g(26))
    return Icon(pad | toes)


def ab_riftanchor():
    shank = R(29, 14, 35, 52)
    ring_ = RG(32, 10, 5.5, 3.6)
    stock = R(20, 20, 44, 25, 2, "tl tr bl br")
    arms = A(32, 34, 20, 5, 30, 150)
    flukes = P((10, 38), (18, 44), (8, 48)) | P((54, 38), (46, 44), (56, 48))
    return Icon(shank | ring_ | stock | arms | flukes)


ABILITIES = {
    "hook": ab_hook, "bullrush": ab_bullrush, "injection": ab_injection, "dualwield": ab_dualwield,
    "barrier": ab_barrier, "flamestrike": ab_flamestrike, "berserk": ab_berserk, "turret": ab_turret,
    "totem": ab_totem, "grenades": ab_grenades, "ghostwalk": ab_ghostwalk, "orb": ab_orb,
    "plasmatrail": ab_plasmatrail, "acidspit": ab_acidspit, "drone": ab_drone,
    "piercingsight": ab_piercingsight, "shellshock": ab_shellshock, "spinevolley": ab_spinevolley,
    "masterofdeath": ab_masterofdeath, "supplydrop": ab_supplydrop, "markprey": ab_markprey, "downpour": ab_downpour,
    "hound": ab_hound, "riftanchor": ab_riftanchor,
    "pip_hook": ab_hookpip, "pip_totem": ab_totempip, "pip_grenade": ab_grenadepip,
}


# ================================================================ passives
# glyphs for the passive ring (and the champion pages); keys are the
# PASSIVE_* and MOVEABIL_* names in lower case, plus the two move styles


def heart_(cx, cy, s):
    """Our heart: a closed curve, about 2s wide."""
    pts = []
    for i in range(64):
        t = i * math.tau / 64
        x = 16 * math.sin(t) ** 3
        y = 13 * math.cos(t) - 5 * math.cos(2 * t) - 2 * math.cos(3 * t) - math.cos(4 * t)
        pts.append((cx + x * s / 16, cy - (y + 2) * s / 16))
    return P(*pts)


def circ_arrow(cx, cy, r, w, a0, a1, head=8):
    a = A(cx, cy, r, w, a0, a1)
    t = math.radians(a1)
    ex, ey = cx + r * math.cos(t), cy + r * math.sin(t)
    tx, ty = -math.sin(t), math.cos(t)          # clockwise tangent
    nx, ny = math.cos(t), math.sin(t)
    tip = P((ex + tx * head, ey + ty * head), (ex + nx * head * 0.75, ey + ny * head * 0.75),
            (ex - nx * head * 0.75, ey - ny * head * 0.75))
    return a | tip


def pa_regeneration():
    h = heart_(26, 34, 15)
    plus = RB(49, 16, 9, 3) | RB(49, 16, 3, 9)
    return Icon((h - h.grow(-4 / G)) | plus)


def pa_haste():
    sh = shield(0.5, 0.5, g(22))
    b = burst(0.5, g(30), g(14), g(7), 8)
    minus = RB(32, 30, 7, 2.2)
    return Icon(sh - b.grow(1.8 / G) | (b - minus.grow(1.6 / G)))


def pa_splashresist():
    plate = R(10, 12, 54, 52, 6, "tl tr bl br")
    rivets = union(*[C(x, y, 3) for x in (17, 47) for y in (19, 45)])
    return Icon(plate - rivets.grow(1.4 / G) | rivets, R(22, 24, 42, 40, 3, "tl br").outline(1.4 / G))


def pa_fireproof():
    f, inner = flame(32, 32, 15, 3)
    return Icon(RG(32, 32, 26, 4.5) | f, inner)


def pa_acidproof():
    return Icon(RG(32, 32, 26, 4.5) | drop(32, 38, 10), None, C(28, 38, 3))


def pa_armorcooldown():
    sh = armour_shield(32, 34, 26, 30, 0)
    return Icon(sh | circ_arrow(32, 32, 26, 4, 200, 470, 8))


def pa_healthcooldown():
    clock = A(32, 32, 25, 4.5, 140, 400)
    return Icon(clock | drop(32, 38, 10.5))


def pa_ammocooldown():
    a, da = crate(20, 46, 10)
    b, db = crate(44, 46, 10)
    c, dc = crate(32, 20, 10)
    return Icon(a | b | c, da | db | dc)


def pa_fullsprint():
    chev = P((22, 12), (44, 32), (22, 52), (14, 52), (34, 32), (14, 12))
    trail = L(4, 26, 18, 26, 3.5) | L(4, 38, 18, 38, 3.5)
    chev2 = P((40, 12), (60, 32), (40, 52), (34, 52), (52, 32), (34, 12))
    return Icon(chev | chev2 | trail)


def pa_charring():
    body, detail = fist(26, 34, 15)
    ember, inner = flame(50, 22, 11, 2)
    return Icon(body | ember, detail | inner)


def pa_stroyent():
    cap = RB(32, 32, 11, 25, 11).rotated(35, 0.5, 0.5)
    inner = RB(32, 32, 8, 22, 8).rotated(35, 0.5, 0.5) & RB(32, 18, 12, 14).rotated(35, 0.5, 0.5)
    hatch = union(*[L(x, 0, x + 40, 40, 1.4) for x in (-6, 2, 10, 18, 26)] + [L(x, 40, x + 40, 0, 1.4) for x in (-6, 2, 10, 18, 26)])
    band = RB(32, 32, 11.5, 2.2).rotated(35, 0.5, 0.5)
    return Icon(cap, (hatch & inner) | band)


def pa_decoratedairman():
    return Icon(RG(32, 32, 22, 6) | C(32, 32, 7))


def pa_hiveregen():
    cells = union(*[hexagon(x, y, 10, 30) - hexagon(x, y, 6, 30) for x, y in ((22, 22), (40, 22), (31, 38))])
    plus = RB(48, 48, 9, 3) | RB(48, 48, 3, 9)
    return Icon(cells | plus)


def pa_leapingkick():
    boot = P((8, 34), (20, 34), (20, 46), (36, 48), (42, 54), (42, 58), (8, 58))
    arc_ = circ_arrow(36, 44, 20, 4.5, 200, 340, 9)
    return Icon(boot | arc_, lines((12, 50, 20, 50)))


def pa_drillsergeant():
    return Icon(union(*[chevron(0.5, g(y), g(18), g(6)) for y in (20, 34, 48)]))


def pa_lightfeet():
    vane = P((50, 8), (56, 14), (40, 40), (24, 50), (14, 50), (20, 38), (40, 16))
    quill = L(8, 58, 30, 38, 3)
    cuts = lines((30, 30, 24, 26), (38, 34, 34, 28), (36, 22, 32, 18))
    return Icon(vane | quill, cuts)


def pa_slipstream():
    w1 = PL([(6, 20), (40, 20)], 4) | A(40, 14, 6, 4, -90, 180)
    w2 = PL([(6, 32), (52, 32)], 4) | A(52, 26, 6, 4, -90, 180) if False else PL([(6, 32), (50, 32)], 4) | A(50, 40, 8, 4, 270, 450 - 180)
    w3 = PL([(12, 44), (36, 44)], 4) | A(36, 50, 6, 4, -90, 90)
    return Icon(w1 | w2 | w3)


def pa_thickhide():
    rows = [((14, 32, 50), 14), ((23, 41), 28), ((14, 32, 50), 42)]
    body = None
    detail = None
    for xs, y in rows:
        for x in xs:
            sc = C(x, y + 4, 10) | R(x - 10, y - 6, x + 10, y + 4)
            body = sc if body is None else body | sc
            a = A(x, y + 4, 10, 1.6, 10, 170)
            detail = a if detail is None else detail | a
    return Icon(body & R(4, 8, 60, 62), detail)


def pa_echo():
    return Icon(C(16, 32, 6) | A(16, 32, 14, 4, -50, 50) | A(16, 32, 25, 4, -45, 45) | A(16, 32, 36, 4, -40, 40))


def pa_armorregen():
    wrench = (R(28, 22, 36, 52, 3, "tl tr bl br") | (C(32, 16, 10) - R(28, 4, 36, 16))).rotated(-40, 0.5, 0.5)
    return Icon(circ_arrow(32, 32, 26, 4, 120, 400, 8) | wrench)


def pa_slowdecay():
    h = heart_(32, 30, 17)
    hand = L(32, 30, 32, 18, 3.4) | L(32, 30, 41, 34, 3.4) | C(32, 30, 3)
    return Icon((h - h.grow(-4.5 / G)) | hand)


def pa_walljump():
    wall = R(8, 4, 16, 60)
    path = PL([(54, 56), (20, 36)], 4) | ARROW(20, 36, 50, 10, 4, 10)
    return Icon(wall | path)


def pa_doublejump():
    return Icon(chevron(0.5, g(24), g(18), g(7)) | chevron(0.5, g(42), g(18), g(7)))


def pa_airdodge():
    c1 = chevron_right(g(32), 0.5, g(14), g(6.5))
    c2 = chevron_right(g(46), 0.5, g(14), g(6.5))
    dash = L(4, 32, 10, 32, 4) | L(16, 32, 22, 32, 4)
    return Icon(c1 | c2 | dash)


def pa_acceleration():
    bars = R(6, 40, 14, 52) | R(18, 32, 26, 52) | R(30, 24, 38, 52)
    return Icon(bars | ARROW(40, 52, 58, 14, 4.5, 10))


def pa_heavyweight():
    anvil = P((8, 18), (56, 18), (56, 24), (44, 30), (42, 40), (50, 48), (50, 54), (14, 54), (14, 48), (22, 40), (20, 30), (8, 26))
    return Icon(anvil, lines((20, 24, 46, 24))) if False else Icon(anvil.moved(0, g(4)) | ARROW(32, 0, 32, 14, 4, 7).moved(0, g(1)))


def pa_rampjump():
    slope = P((4, 58), (40, 30), (40, 58))
    return Icon(slope | ARROW(42, 26, 58, 6, 4, 9))


def pa_bunnyhop():
    return Icon(A(14, 46, 9, 4, 180, 360) | A(32, 46, 9, 4, 180, 360) | A(50, 46, 9, 4, 180, 360)
                | L(2, 52, 62, 52, 3) & halfplane(0, 1, 2))


def pa_easystrafe():
    return Icon(circ_arrow(32, 40, 22, 5, 190, 330, 9) | C(32, 40, 4))


def pa_isoair():
    return Icon(C(32, 32, 5) | ARROW(32, 32, 32, 6, 4, 8) | ARROW(32, 32, 32, 58, 4, 8)
                | ARROW(32, 32, 6, 32, 4, 8) | ARROW(32, 32, 58, 32, 4, 8))


def pa_aircontrol():
    board = R(8, 36, 56, 42, 3, "tl tr bl br") | A(10, 33, 4, 3, 90, 200) | A(54, 33, 4, 3, -20, 90)
    jets = C(20, 46, 3.5) | C(44, 46, 3.5)
    motion = L(14, 24, 36, 24, 3) | L(24, 16, 50, 16, 3)
    return Icon(board | jets | motion)


def pa_slide():
    roof = L(6, 18, 58, 18, 4)
    return Icon(roof | ARROW(6, 44, 56, 44, 5, 10) | L(8, 54, 40, 54, 2.5))


PASSIVES = {
    "regeneration": pa_regeneration, "haste": pa_haste, "splashresist": pa_splashresist,
    "fireproof": pa_fireproof, "acidproof": pa_acidproof, "armorcooldown": pa_armorcooldown,
    "healthcooldown": pa_healthcooldown, "ammocooldown": pa_ammocooldown, "fullsprint": pa_fullsprint,
    "charring": pa_charring, "stroyent": pa_stroyent, "decoratedairman": pa_decoratedairman,
    "hiveregen": pa_hiveregen, "leapingkick": pa_leapingkick, "drillsergeant": pa_drillsergeant,
    "lightfeet": pa_lightfeet, "slipstream": pa_slipstream, "thickhide": pa_thickhide, "echo": pa_echo,
    "armorregen": pa_armorregen, "slowdecay": pa_slowdecay,
    "walljump": pa_walljump, "doublejump": pa_doublejump, "airdodge": pa_airdodge,
    "acceleration": pa_acceleration, "heavyweight": pa_heavyweight, "rampjump": pa_rampjump,
    "bunnyhop": pa_bunnyhop, "easystrafe": pa_easystrafe, "isoair": pa_isoair,
    "aircontrol": pa_aircontrol, "slide": pa_slide,
}


# ================================================================ items


def it_health_small():
    c = cross(32, 32, 25, 11)
    return Icon(c - c.grow(-4.5 / G))


def it_health_large():
    return Icon(cross(32, 32, 25, 11), None, None)


def it_mega():
    frame = hexagon(32, 32, 29, 30) - hexagon(32, 32, 22, 30)
    return Icon(frame | cross(32, 32, 16, 7), hexagon(32, 32, 25.5, 30).outline(1.4 / G))


def it_shard():
    sh = P((24, 8), (44, 14), (48, 30), (36, 56), (18, 36), (16, 16))
    return Icon(sh, lines((24, 8, 30, 32), (30, 32, 48, 30), (30, 32, 36, 56)), P((24, 10), (29, 30), (18, 34), (17, 17)))


def it_armor_light():
    sh = armour_shield(32, 34, 44, 48, 1)
    return Icon(sh, lines((10, 32, 54, 32)))


def it_armor_heavy():
    sh = armour_shield(32, 33, 48, 52, 3)
    return Icon(sh, lines((32, 13, 32, 56), (10, 26, 54, 26)))


def it_quad():
    d = regular(0.5, 0.5, g(9), 4, 0)
    heads = None
    for a in range(0, 360, 90):
        h = P((32, 4), (44, 16), (36, 16), (36, 20), (28, 20), (28, 16), (20, 16)).rotated(a, 0.5, 0.5)
        heads = h if heads is None else heads | h
    return Icon(d | heads)


def it_protection():
    dome = (C(32, 40, 22) - C(32, 40, 16)) & halfplane(0, 1, g(40))
    base = R(6, 40, 58, 45)
    dot = C(32, 34, 5)
    rays = L(32, 4, 32, 11, 3.5) | L(14, 10, 18, 16, 3.5) | L(50, 10, 46, 16, 3.5)
    return Icon(dome | base | dot | rays)


def it_hourglass():
    caps = R(12, 4, 52, 11, 3, "tl tr bl br") | R(12, 53, 52, 60, 3, "tl tr bl br")
    glass = P((16, 11), (48, 11), (48, 16), (36, 30), (36, 34), (48, 48), (48, 53), (16, 53), (16, 48), (28, 34), (28, 30), (16, 16))
    sand = P((22, 16), (42, 16), (32, 28)) | P((32, 40), (44, 50), (20, 50))
    return Icon(caps | (glass - glass.grow(-3.5 / G)) | sand)


def ammo_box(glyph):
    def f():
        face, detail = crate(32, 32, 28)
        inner = glyph()
        return Icon(face - inner.body.rotated(0).moved(0, 0).grow(0), detail - inner.body.grow(2 / G))
    return f


def it_flag():
    pole = R(12, 4, 17, 60)
    pennant = P((17, 8), (54, 18), (17, 32))
    return Icon(pole | pennant | C(14.5, 5, 4), lines((17, 20, 40, 20)))


def it_flag_taken():
    pole = R(12, 4, 17, 60)
    edge = polyline([(g(17), g(8)), (g(54), g(18)), (g(17), g(32))], g(3.5))
    dashes = union(*[RB(36, 18, 1.8, 30).rotated(a, g(36), g(18)) for a in (0, 90)]) if False else None
    cuts = union(*[R(x, 0, x + 3, 40) for x in (24, 34, 44)])
    return Icon(pole | (edge - cuts) | C(14.5, 5, 4))


def it_flag_dropped():
    ground = R(4, 52, 60, 56)
    pole = L(8, 48, 50, 36, 4)
    pennant = P((40, 39), (30, 20), (22, 44))
    timer = A(48, 18, 10, 3.5, -90, 180)
    return Icon(ground | pole | pennant | timer)


def it_soul():
    f, inner = flame(32, 32, 14, 1)
    return Icon(RG(32, 32, 26, 4.5) | f, inner)


def it_obelisk():
    body = P((24, 14), (32, 4), (40, 14), (44, 52), (20, 52))
    base = R(14, 52, 50, 60, 3, "tl tr")
    return Icon(body | base, lines((32, 8, 32, 50)), P((26, 15), (31, 8), (31, 50), (22, 50)))


ITEMS = {
    "health_small": it_health_small, "health_large": it_health_large, "mega": it_mega, "shard": it_shard,
    "armor_light": it_armor_light, "armor_heavy": it_armor_heavy, "quad": it_quad, "protection": it_protection,
    "hourglass": it_hourglass, "flag": it_flag, "flag_taken": it_flag_taken, "flag_dropped": it_flag_dropped,
    "soul": it_soul, "obelisk": it_obelisk,
}


def _ammo_crate(fn):
    def f():
        face, detail = crate(32, 32, 28)
        glyph = fn().body
        small = Shape(lambda x, y, sdf=glyph.sdf: sdf((x - 0.5) / 0.62 + 0.5, (y - 0.5) / 0.62 + 0.5) * 0.62,
                      (0.5 - 0.31, 0.5 - 0.31, 0.5 + 0.31, 0.5 + 0.31))
        return Icon(face - small.grow(1.4 / G), None) if False else Icon(face - small.grow(2.2 / G) | small,
                                                                         R(8, 8, 56, 56, 5, "tl tr bl br").outline(1.4 / G) - small.grow(4 / G))
    return f


for _k, _fn in AMMO.items():
    ITEMS["ammo_" + _k] = _ammo_crate(_fn)


# ================================================================ kill-feed causes


def ca_telefrag():
    spikes = None
    for a in range(0, 360, 45):
        sp = P((28.5, 6), (35.5, 6), (32, 19)).rotated(a, 0.5, 0.5)
        spikes = sp if spikes is None else spikes | sp
    return Icon(RG(32, 32, 26, 5) | spikes | C(32, 32, 6))


def figure(cx, cy, s, ang=0):
    head = C(cx, cy - s * 0.95, s * 0.28)
    body = L(cx, cy - s * 0.6, cx, cy + s * 0.2, s * 0.26)
    arms = PL([(cx - s * 0.55, cy - s * 0.65), (cx, cy - s * 0.4), (cx + s * 0.55, cy - s * 0.65)], s * 0.2)
    legs = PL([(cx - s * 0.4, cy + s * 0.95), (cx, cy + s * 0.2), (cx + s * 0.4, cy + s * 0.95)], s * 0.22)
    f = head | body | arms | legs
    return f.rotated(ang, g(cx), g(cy)) if ang else f


def ca_fall():
    ledge = R(4, 14, 28, 20)
    return Icon(ledge | figure(42, 36, 18, 160) | L(42, 4, 42, 10, 3) | L(52, 4, 52, 12, 3))


def ca_lava():
    waves = union(*[PL([(6 + i * 4, y), (16, y - 4), (26, y), (36, y - 4), (46, y), (56, y - 4)], 3.5)
                    for i, y in enumerate((44, 52, 60))]) & R(2, 30, 62, 64)
    f, inner = flame(32, 20, 12, 3)
    return Icon(waves | f, inner)


def ca_acid():
    edge = R(6, 8, 58, 14)
    drips = R(14, 14, 20, 22, 3, "bl br") | R(42, 14, 48, 26, 3, "bl br")
    return Icon(edge | drips | drop(31, 44, 10))


def ca_crushed():
    top = R(8, 4, 56, 22, 4, "bl br")
    bottom = R(8, 42, 56, 60, 4, "tl tr")
    return Icon(top | bottom | E(32, 32, 9, 5.5), lines((14, 10, 50, 10), (14, 54, 50, 54)))


def ca_drowning():
    return Icon(RG(24, 46, 9, 3.5) | RG(40, 28, 7, 3.2) | RG(28, 13, 5, 3))


def ca_suicide():
    return Icon(skullg(32, 30, 18))


def ca_quad_marker():
    return Icon(regular(0.5, 0.5, g(26), 4, 0), regular(0.5, 0.5, g(18), 4, 0).outline(1.4 / G))


CAUSES = {"telefrag": ca_telefrag, "fall": ca_fall, "lava": ca_lava, "acid": ca_acid, "crushed": ca_crushed,
          "drowning": ca_drowning, "suicide": ca_suicide, "quad_marker": ca_quad_marker}


# ================================================================ HUD glyphs


def gl_tick():
    return Icon(P((6, 34), (16, 24), (26, 34), (50, 10), (60, 20), (26, 54)))


def gl_lock():
    body = R(12, 28, 52, 58, 4, "tl tr bl br")
    shackle = (C(32, 26, 14) - C(32, 26, 8.5) - halfplane(0, -1, -g(26))) | R(18, 26, 23.5, 30) | R(40.5, 26, 46, 30)
    hole = C(32, 40, 4) | R(30.5, 40, 33.5, 50)
    return Icon((body - hole) | shackle)


def gl_net_server():
    units = union(*[R(10, y, 54, y + 13, 3, "tl tr bl br") for y in (6, 24, 42)])
    leds = union(*[C(46, y + 6.5, 2.5) for y in (6, 24, 42)])
    return Icon(units - leds, lines((16, 12.5, 32, 12.5), (16, 30.5, 32, 30.5), (16, 48.5, 32, 48.5)))


def gl_net_packet():
    sq = R(8, 8, 56, 56, 6, "tl br")
    crack = PL([(36, 4), (30, 22), (38, 34), (28, 60)], 5)
    return Icon((sq - sq.grow(-6 / G)) - crack)


def gl_net_ping():
    bars = union(*[R(8 + i * 13, 52 - (i + 1) * 11, 17 + i * 13, 56) for i in range(4)])
    return Icon(bars)


def gl_net_client():
    screen = R(6, 8, 58, 44, 4, "tl tr bl br")
    stand = R(28, 44, 36, 52) | R(18, 52, 46, 57, 2, "tl tr")
    return Icon((screen - R(11, 13, 53, 39, 2, "tl tr bl br")) | stand)


def gl_burning():
    f, inner = flame(32, 32, 20, 3)
    return Icon(f, inner)


def gl_acid():
    return Icon(drop(32, 40, 15), None, C(27, 41, 4))


def gl_regen():
    h = heart_(32, 32, 20)
    plus = RB(32, 32, 9, 3) | RB(32, 32, 3, 9)
    return Icon(h - plus.grow(1 / G))


def gl_slowed():
    clock = RG(32, 32, 24, 4.5)
    hand = L(32, 32, 32, 16, 3.5)
    ch = chevron(0.5, g(42), g(10), g(5)).mirrored(0.5)
    ch = Shape(lambda x, y, f=ch.sdf: f(x, 2 * g(42) - y), (g(20), g(34), g(44), g(50)))
    return Icon(clock | hand | ch | C(32, 32, 3))


def gl_deathmarker():
    d = regular(0.5, 0.5, g(28), 4, 0) - regular(0.5, 0.5, g(22), 4, 0)
    x = L(22, 22, 42, 42, 5) | L(42, 22, 22, 42, 5)
    return Icon(d | x)


def gl_stat_health():
    return Icon(RG(32, 32, 28, 3.6) | cross(32, 32, 17, 7.5))


def gl_stat_armor():
    return Icon(RG(32, 32, 28, 3.6) | armour_shield(32, 33, 30, 34, 3))


def gl_stat_speed():
    a = P((14, 16), (26, 16), (40, 32), (26, 48), (14, 48), (28, 32))
    b = a.moved(g(14), 0)
    return Icon(RG(32, 32, 28, 3.6) | a | b)


def gl_star():
    st = star(0.5, g(34), g(30), g(12.5), 5)
    return Icon(st - union(*[C(32 + 30 * math.cos(math.radians(-90 + i * 72)), 34 + 30 * math.sin(math.radians(-90 + i * 72)), 3.2)
                             for i in range(5)]).grow(0))


GLYPHS = {
    "tick": gl_tick, "lock": gl_lock, "net_server": gl_net_server, "net_packet": gl_net_packet,
    "net_ping": gl_net_ping, "net_client": gl_net_client, "status_burning": gl_burning, "status_acid": gl_acid,
    "status_regen": gl_regen, "status_slowed": gl_slowed, "deathmarker": gl_deathmarker,
    "stat_health": gl_stat_health, "stat_armor": gl_stat_armor, "stat_speed": gl_stat_speed, "star": gl_star,
}


# ================================================================ medals
# glyphs drawn white (tinted gold in the HUD ring), and full-colour badges
# for the end screen whose frame shape is set by the tier


def scaled(shape, k, ox, oy):
    """shape scaled by k about the origin, then moved by (ox, oy) grid cells."""
    f = shape.sdf
    x0, y0, x1, y1 = shape.box
    dx, dy = g(ox), g(oy)
    return Shape(lambda x, y: f((x - dx) / k, (y - dy) / k) * k, (x0 * k + dx, y0 * k + dy, x1 * k + dx, y1 * k + dy))


def notches(n, r0=24, r1=30):
    out = None
    for i in range(n):
        a = math.radians(-90 + 360 * i / n)
        t = L(32 + r0 * math.cos(a), 32 + r0 * math.sin(a), 32 + r1 * math.cos(a), 32 + r1 * math.sin(a), 3.6)
        out = t if out is None else out | t
    return out


def spark(cx, cy, r):
    return star(g(cx), g(cy), g(r), g(r * 0.3), 4, -90)


def diamond(cx, cy, r):
    return regular(g(cx), g(cy), g(r), 4, 0)


def laurel():
    stem = A(32, 30, 26, 2.6, 30, 150)
    leaves = None
    for side in (-1, 1):
        for i in range(5):
            a = math.radians(90 - side * (70 - i * 13))
            x, y = 32 + 26 * math.cos(a), 30 + 26 * math.sin(a)
            lf = E(x, y, 5, 2.2).rotated(math.degrees(a) + 90 + side * 40, g(x), g(y))
            leaves = lf if leaves is None else leaves | lf
    return stem | leaves


def md_excellent():
    return Icon(chevron(0.5, g(24), g(20), g(8)) | chevron(0.5, g(44), g(20), g(8)))


def md_impressive():
    rod = R(4, 29, 60, 35, 3, "tr br")
    rings = (E(24, 32, 5, 15) - E(24, 32, 2.2, 12)) | (E(42, 32, 5, 15) - E(42, 32, 2.2, 12))
    return Icon((rod - rings.grow(1.4 / G)) | rings)


def md_gauntlet():
    palm = R(16, 30, 46, 58, 6, "bl br")
    fingers = union(*[RB(20 + i * 8, 22 - (i % 3 == 1) * 4 - (i == 2) * 2, 3.2, 12, 3.2) for i in range(4)])
    thumb = RB(10, 38, 3.4, 10, 3.4).rotated(-35, g(10), g(42))
    return Icon(palm | fingers | thumb | spark(52, 14, 10), lines((19, 34, 43, 34)))


def md_firstblood():
    d = drop(32, 40, 16)
    one = P((28, 30), (34, 26), (36, 26), (36, 52), (31, 52), (31, 34), (28, 35))
    return Icon(d - one)


def md_midair():
    arc_ = A(32, 46, 26, 4, 200, 340)
    rocket = am_rockets().body
    rocket = scaled(rocket, 0.62, 32 - 32 * 0.62, 40 - 32 * 0.62).rotated(40, g(32), g(40))
    return Icon(arc_ | rocket)


def md_airborne():
    path = union(*[A(32, 54, 24, 3.5, a, a + 16) for a in range(190, 350, 28)])
    return Icon(path | figure(32, 40, 10) | ARROW(32, 22, 32, 4, 4, 8))


def md_combokill():
    b = burst(0.5, 0.5, g(24), g(10), 8)
    slug = R(4, 29, 60, 35, 3, "tl tr bl br").rotated(-35, 0.5, 0.5)
    return Icon((b - slug.grow(2 / G)) | slug)


def md_collateral():
    b = burst(0.5, g(26), g(20), g(9), 8)
    return Icon(b | skullg(14, 48, 7.5) | skullg(50, 48, 7.5))


def md_doublewhammy():
    return Icon(C(18, 40, 8) | C(46, 24, 8) | L(4, 48, 60, 16, 4))


def md_denied():
    return Icon(RG(32, 32, 22, 6) | L(12, 52, 52, 12, 6))


def md_revenge():
    return Icon(circ_arrow(32, 32, 20, 5.5, 120, 400, 11) | C(32 + 20 * math.cos(math.radians(120)), 32 + 20 * math.sin(math.radians(120)), 5))


def md_partybreaker():
    d = diamond(32, 32, 27)
    crack = PL([(30, 4), (36, 20), (26, 32), (38, 44), (32, 60)], 3.2)
    return Icon(d - crack, diamond(32, 32, 19).outline(1.4 / G) - crack.grow(1 / G))


def md_perforated():
    return Icon(RG(32, 32, 26, 5) | burst(0.5, 0.5, g(17), g(6), 8))


def md_pointblank():
    left = R(4, 23, 10, 41, 1.5, "tl bl") | R(10, 24, 25, 40) | P((25, 24), (30, 28), (30, 36), (25, 40))
    return Icon(left | left.mirrored(0.5), lines((10, 24, 10, 40), (54, 24, 54, 40)))


def md_precise():
    ret = RG(24, 40, 14, 4) | L(24, 22, 24, 30, 3.5) | L(24, 50, 24, 58, 3.5) | L(6, 40, 14, 40, 3.5) | L(34, 40, 42, 40, 3.5)
    return Icon(ret | C(24, 40, 2.5) | C(52, 10, 5))


def md_ringout():
    edge = R(4, 22, 30, 28)
    wall = R(24, 28, 30, 60)
    return Icon(edge | wall | figure(46, 40, 13, 25))


def md_stopwatch():
    body = RG(32, 36, 21, 5)
    crown_ = R(28, 4, 36, 10) | R(30, 10, 34, 15) | L(48, 16, 52, 12, 4)
    hand = L(32, 36, 32, 20, 4) | C(32, 36, 4)
    return Icon(body | crown_ | hand)


def md_tank():
    sh = armour_shield(32, 32, 48, 54, 3)
    c = RB(32, 29, 13, 4.5) | RB(32, 29, 4.5, 13)
    return Icon((sh - c.grow(2.4 / G)) | c)


def md_diehard():
    h = heart_(32, 30, 22)
    crack = PL([(30, 12), (36, 24), (28, 34), (34, 48)], 3)
    return Icon((h - crack) | spark(52, 10, 9))


def md_fromthegrave():
    return Icon(skullg(32, 40, 14) | ARROW(32, 18, 32, 0, 5, 9).moved(0, g(4)))


def md_kamikaze():
    b = burst(0.5, 0.5, g(30), g(20), 12)
    sk = skullg(32, 31, 12)
    return Icon((b - sk.grow(2 / G)) | sk)


def md_showstopper():
    link1 = (RB(20, 32, 13, 7.5, 7.5) - RB(20, 32, 8.5, 3, 3))
    link2 = (RB(44, 32, 13, 7.5, 7.5) - RB(44, 32, 8.5, 3, 3))
    gap = R(30, 20, 34, 44)
    broken = (link1 | link2) - gap
    return Icon(broken.rotated(-30, 0.5, 0.5) | spark(32, 32, 7).rotated(15, 0.5, 0.5))


def multi(n):
    def f():
        return Icon(skullg(32, 32, 14) | notches(n))
    return f


def streak(tongues, n):
    def f():
        fl, inner = flame(32, 32 if not n else 34, 16 if not n else 13, tongues)
        sh = fl | notches(n, 25, 30) if n else fl
        return Icon(sh, inner)
    return f


def md_executioner():
    return Icon(skullg(32, 32, 13) | notches(15, 25, 30))


def md_dominating():
    return Icon(crown(0.5, g(22), g(20)) | diamond(32, 48, 12), None, None)


def md_powerupmassacre():
    d = diamond(32, 26, 18)
    tallies = union(*[L(14 + i * 7, 46, 14 + i * 7, 60, 3.6) for i in range(4)]) | L(10, 58, 46, 46, 3.6)
    return Icon(d | tallies, diamond(32, 26, 11).outline(1.4 / G))


def god(fn):
    def f():
        w = fn()
        k = 0.4
        body = scaled(w.body, k, 32 - 64 * k, 34 - 32 * k)
        det = scaled(w.detail, k, 32 - 64 * k, 34 - 32 * k) if w.detail is not None else None
        return Icon(laurel() | body, det)
    return f


def md_damagedealer():
    bars = R(8, 40, 18, 58) | R(22, 30, 32, 58) | R(36, 18, 46, 58)
    return Icon(bars | ARROW(8, 32, 56, 4, 4, 9) & halfplane(0, 1, 1))


def md_netmaster():
    beam = R(8, 14, 56, 18)
    post = R(30, 14, 34, 54) | R(20, 54, 44, 59, 2, "tl tr")
    pans = (C(14, 38, 10) & halfplane(0, -1, -g(38))) | (C(50, 38, 10) & halfplane(0, -1, -g(38)))
    cords = L(10, 16, 6, 38, 2) | L(18, 16, 22, 38, 2) | L(46, 16, 42, 38, 2) | L(54, 16, 58, 38, 2)
    return Icon(beam | post | pans | cords | C(32, 12, 4))


def md_assistant():
    arms = L(6, 58, 24, 36, 10) | L(58, 58, 40, 36, 10)
    clasp = C(32, 28, 11)
    return Icon(arms | clasp, lines((24, 22, 40, 22), (22, 28, 42, 28), (24, 34, 40, 34)))


def md_offense():
    fl = R(36, 6, 40, 58) | P((40, 8), (60, 16), (40, 24))
    return Icon(fl | ARROW(4, 46, 30, 32, 5, 10))


def md_capture():
    return it_flag()


def md_defend():
    sh = armour_shield(32, 34, 46, 50, 0)
    fl = R(26, 16, 29, 48) | P((29, 18), (44, 24), (29, 30))
    return Icon(sh - fl.grow(2 / G) | fl)


def md_assist():
    return Icon(chevron_right(g(16), 0.5, g(14), g(6)) | chevron_right(g(30), 0.5, g(14), g(6)) | C(50, 32, 8))


def md_accuracy():
    return Icon(RG(32, 32, 18, 4.5) | RG(32, 32, 8, 3.5) | L(32, 2, 32, 12, 4) | L(32, 52, 32, 62, 4)
                | L(2, 32, 12, 32, 4) | L(52, 32, 62, 32, 4))


def md_frags():
    bones = L(10, 50, 54, 62, 5) | L(54, 50, 10, 62, 5)
    return Icon(skullg(32, 26, 15) | bones)


def md_victory():
    cup = P((16, 8), (48, 8), (46, 26), (38, 36), (26, 36), (18, 26))
    stem = R(29, 36, 35, 48) | R(18, 48, 46, 56, 2, "tl tr")
    handles = A(16, 18, 7, 4, 90, 270) | A(48, 18, 7, 4, -90, 90)
    return Icon(cup | stem | handles, None, P((20, 11), (28, 11), (27, 30), (22, 26)))


MEDAL_GLYPHS = {
    "firstblood": md_firstblood, "midair": md_midair, "airborne": md_airborne, "combokill": md_combokill,
    "collateral": md_collateral, "doublewhammy": md_doublewhammy, "denied": md_denied, "revenge": md_revenge,
    "partybreaker": md_partybreaker, "perforated": md_perforated, "pointblank": md_pointblank,
    "precise": md_precise, "ringout": md_ringout, "stopwatch": md_stopwatch, "tank": md_tank,
    "diehard": md_diehard, "fromthegrave": md_fromthegrave, "kamikaze": md_kamikaze,
    "showstopper": md_showstopper, "triplekill": multi(3), "overkill": multi(4), "fighter": multi(5),
    "crusher": multi(6), "exterminator": multi(7), "demolisher": multi(8), "grimreaper": multi(9),
    "avatarofdeath": multi(10), "killingspree": streak(1, 0), "rage": streak(2, 0), "fury": streak(2, 3),
    "frenzy": streak(3, 0), "riot": streak(3, 5), "rampage": streak(4, 0), "merciless": streak(4, 8),
    "fanaticism": streak(4, 10), "sadism": streak(4, 12), "executioner": md_executioner,
    "dominating": md_dominating, "powerupmassacre": md_powerupmassacre,
    "gauntletgod": god(w_melee), "machinegungod": god(w_machinegun), "shotgungod": god(w_shotgun),
    "nailgungod": god(w_nailgun), "triboltgod": god(w_tribolt), "rocketgod": god(w_rocket),
    "lightninggod": god(w_lightning), "railgod": god(w_railgun),
    "damagedealer": md_damagedealer, "netmaster": md_netmaster, "assistant": md_assistant,
    "offense": md_offense, "impressive": md_impressive, "excellent": md_excellent, "gauntlet": md_gauntlet,
    "defend": md_defend, "capture": md_capture, "assist": md_assist, "accuracy": md_accuracy,
    "frags": md_frags, "victory": md_victory,
}

COMMON, RARE, EPIC, LEGENDARY = "common", "rare", "epic", "legendary"
TIER_COLOR = {COMMON: (0.784, 0.816, 0.847), RARE: (0.235, 0.549, 0.941),
              EPIC: (0.784, 0.063, 0.925), LEGENDARY: (0.941, 0.690, 0.125)}
MEDAL_TIER = {k: RARE for k in MEDAL_GLYPHS}
MEDAL_TIER.update({k: COMMON for k in ("firstblood", "airborne", "pointblank", "stopwatch", "capture", "defend",
                                       "offense", "assist", "accuracy", "frags", "victory")})
MEDAL_TIER.update({k: EPIC for k in ("gauntlet", "collateral", "doublewhammy", "partybreaker", "perforated",
                                     "fromthegrave", "kamikaze", "showstopper", "fighter", "crusher",
                                     "exterminator", "demolisher", "grimreaper", "rage", "fury", "frenzy",
                                     "riot", "dominating")})
MEDAL_TIER.update({k: LEGENDARY for k in ("avatarofdeath", "rampage", "merciless", "fanaticism", "sadism",
                                          "executioner", "powerupmassacre", "gauntletgod", "machinegungod",
                                          "shotgungod", "nailgungod", "triboltgod", "rocketgod", "lightninggod",
                                          "railgod")})
GOLD = (0.925, 0.753, 0.031)


def frame_shape(tier):
    if tier == COMMON:
        return C(32, 32, 29)
    if tier == RARE:
        return star(0.5, 0.5, g(31), g(22), 6)
    if tier == EPIC:
        return P((8, 6), (56, 6), (58, 8), (58, 36), (32, 60), (6, 36), (6, 8)) - P((4, 2), (14, 2), (4, 12)) - P((60, 2), (50, 2), (60, 12))
    return star(0.5, 0.5, g(31), g(24), 10)


def make_badge(key):
    """The end-screen badge: tier frame, dark face with a thin ring, gold glyph."""
    tier = MEDAL_TIER[key]
    col = TIER_COLOR[tier]
    c = Canvas(256, 256)
    frame = frame_shape(tier)
    c.glow(frame, scale(col, 0.35), 0.03, 0.5)
    c.fill_flat(frame, base=0.84, light=1.0, dark=0.55, rim=0.03, color=tuple(min(1, v / 0.84) for v in col))
    c.fill(frame.outline(1.0 / G), scale(col, 0.45))
    face = C(32, 32, 21) if tier != EPIC else C(32, 28, 19)
    c.fill(face.grow(1.5 / G), scale(col, 0.4))
    c.fill(face, lambda x, y: mix((0.14, 0.14, 0.15), (0.06, 0.06, 0.07), smooth(0.25, 0.75, y)))
    glyph = MEDAL_GLYPHS[key]()
    k = 0.52
    cy = 32 if tier != EPIC else 28
    body = scaled(glyph.body, k, 32 - 32 * k, cy - 32 * k)
    c.glow(body, scale(GOLD, 0.5), 0.02, 0.45)
    c.fill_flat(body, base=0.84, light=1.0, dark=0.6, rim=0.02, color=tuple(min(1, v / 0.84) for v in GOLD))
    if glyph.detail is not None:
        c.fill(scaled(glyph.detail, k, 32 - 32 * k, cy - 32 * k) & body.grow(-0.4 / G), scale(GOLD, 0.62))
    return c

# ================================================================ the set

GROUPS = {
    "weapons": (WEAPONS, 256, 128, 2.0),
    "ammo": (AMMO, 128, 128, 1.0),
    "abilities": (ABILITIES, 128, 128, 1.0),
    "passives": (PASSIVES, 128, 128, 1.0),
    "items": (ITEMS, 128, 128, 1.0),
    "causes": (CAUSES, 128, 128, 1.0),
    "glyphs": (GLYPHS, 128, 128, 1.0),
    "medals": (MEDAL_GLYPHS, 128, 128, 1.0),
    "badges": (MEDAL_GLYPHS, 256, 256, 1.0),
}


def job(args):
    group, key, out = args
    table, w, h, span = GROUPS[group]
    if group == "badges":
        make_badge(key).write(os.path.join(out, "..", "ui", "post_medal_" + key + ".tga"))
        return
    render(table[key](), w, h, span).write(os.path.join(out, group, key + ".tga"))


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    out = os.path.join(sys.argv[1], "gfx", "tremor", "icons")
    jobs = []
    for arg in sys.argv[2:] or GROUPS:
        group, _, keys = arg.partition(":")
        os.makedirs(os.path.join(out, group if group != "badges" else "../ui"), exist_ok=True)
        jobs += [(group, key, out) for key in (keys.split(",") if keys else GROUPS[group][0])]
    try:
        from multiprocessing import Pool
        with Pool() as pool:
            pool.map(job, jobs)
    except (ImportError, OSError):
        for j in jobs:
            job(j)


if __name__ == "__main__":
    main()
