"""Shared drawing for the art generators: shapes as signed distances, noise,
the logo's palette and a TGA writer. Plain Python 3, no packages.

Shapes are functions from a point to a signed distance (negative inside), in
whatever units the caller works in, with a bounding box so a fill only visits
the pixels it can touch. Edges are antialiased from the distance.
"""
import math, random, struct

# ---------------------------------------------------------------- palette

RED = (0.827, 0.098, 0.122)         # the logo's red on dark, #d3191f
RED_DEEP = (0.647, 0.075, 0.102)    # on light, #a5131a
SILVER = (0.847, 0.843, 0.820)      # #d8d7d1
STEEL = (0.702, 0.698, 0.678)       # #b3b2ad
PEWTER = (0.557, 0.557, 0.545)      # #8e8e8b
GUNMETAL = (0.353, 0.357, 0.369)    # #5a5b5e
INK = (0.071, 0.071, 0.078)         # #121214
BACK = (0.051, 0.051, 0.063)        # the menus' background


def clamp(x, lo=0.0, hi=1.0):
    return lo if x < lo else hi if x > hi else x


def smooth(e0, e1, x):
    t = clamp((x - e0) / (e1 - e0))
    return t * t * (3 - 2 * t)


def mix(a, b, t):
    if isinstance(a, tuple):
        return tuple(x + (y - x) * t for x, y in zip(a, b))
    return a + (b - a) * t


def scale(c, k):
    return tuple(x * k for x in c)


# ---------------------------------------------------------------- noise


class Noise:
    """Value noise on a lattice that wraps at px x py cells."""

    def __init__(self, seed, px=64, py=64):
        rng = random.Random(seed)
        self.px, self.py = px, py
        self.g = [rng.random() for _ in range(px * py)]

    def at(self, x, y):
        xi, yi = math.floor(x), math.floor(y)
        fx, fy = x - xi, y - yi
        fx = fx * fx * (3 - 2 * fx)
        fy = fy * fy * (3 - 2 * fy)
        px, py, g = self.px, self.py, self.g
        x0, x1 = xi % px, (xi + 1) % px
        y0, y1 = (yi % py) * px, ((yi + 1) % py) * px
        a = g[y0 + x0] + (g[y0 + x1] - g[y0 + x0]) * fx
        b = g[y1 + x0] + (g[y1 + x1] - g[y1 + x0]) * fx
        return a + (b - a) * fy


class Fbm:
    def __init__(self, seed, octaves=4, base=4):
        self.layers = [Noise(seed * 31 + i, 256, 256) for i in range(octaves)]
        self.base = base

    def at(self, u, v):
        total, amp, norm, f = 0.0, 1.0, 0.0, self.base
        for n in self.layers:
            total += n.at(u * f, v * f) * amp
            norm += amp
            amp *= 0.5
            f *= 2
        return total / norm


# ---------------------------------------------------------------- shapes


class Shape:
    def __init__(self, sdf, box):
        self.sdf, self.box = sdf, box     # box: x0, y0, x1, y1

    def __or__(self, o):
        a, b = self.sdf, o.sdf
        return Shape(lambda x, y: min(a(x, y), b(x, y)), _union(self.box, o.box))

    def __sub__(self, o):
        a, b = self.sdf, o.sdf
        return Shape(lambda x, y: max(a(x, y), -b(x, y)), self.box)

    def __and__(self, o):
        a, b = self.sdf, o.sdf
        return Shape(lambda x, y: max(a(x, y), b(x, y)), self.box)

    def grow(self, r):
        a = self.sdf
        x0, y0, x1, y1 = self.box
        return Shape(lambda x, y: a(x, y) - r, (x0 - r, y0 - r, x1 + r, y1 + r))

    def outline(self, w):
        a = self.sdf
        x0, y0, x1, y1 = self.box
        return Shape(lambda x, y: abs(a(x, y)) - w / 2, (x0 - w, y0 - w, x1 + w, y1 + w))

    def moved(self, dx, dy):
        a = self.sdf
        x0, y0, x1, y1 = self.box
        return Shape(lambda x, y: a(x - dx, y - dy), (x0 + dx, y0 + dy, x1 + dx, y1 + dy))

    def rotated(self, deg, cx=0.5, cy=0.5):
        a, c, s = self.sdf, math.cos(math.radians(deg)), math.sin(math.radians(deg))
        x0, y0, x1, y1 = self.box
        r = max(math.hypot(px - cx, py - cy) for px in (x0, x1) for py in (y0, y1))
        return Shape(lambda x, y: a(cx + (x - cx) * c + (y - cy) * s, cy - (x - cx) * s + (y - cy) * c),
                     (cx - r, cy - r, cx + r, cy + r))

    def mirrored(self, cx=0.5):
        a = self.sdf
        x0, y0, x1, y1 = self.box
        return Shape(lambda x, y: a(2 * cx - x, y), (2 * cx - x1, y0, 2 * cx - x0, y1))


def _union(a, b):
    return (min(a[0], b[0]), min(a[1], b[1]), max(a[2], b[2]), max(a[3], b[3]))


def union(*shapes):
    s = shapes[0]
    for o in shapes[1:]:
        s = s | o
    return s


def circle(cx, cy, r):
    return Shape(lambda x, y: math.hypot(x - cx, y - cy) - r, (cx - r, cy - r, cx + r, cy + r))


def ring(cx, cy, r, w):
    return Shape(lambda x, y: abs(math.hypot(x - cx, y - cy) - r) - w / 2,
                 (cx - r - w, cy - r - w, cx + r + w, cy + r + w))


def line(ax, ay, bx, by, w):
    """A stroke of width w with round ends."""
    r = w / 2
    dx, dy = bx - ax, by - ay
    ll = dx * dx + dy * dy or 1e-9

    def f(x, y):
        t = clamp(((x - ax) * dx + (y - ay) * dy) / ll)
        return math.hypot(x - ax - dx * t, y - ay - dy * t) - r
    return Shape(f, (min(ax, bx) - r, min(ay, by) - r, max(ax, bx) + r, max(ay, by) + r))


def polyline(pts, w):
    return union(*[line(*pts[i], *pts[i + 1], w) for i in range(len(pts) - 1)])


def box(cx, cy, hw, hh, round_=0.0):
    def f(x, y):
        qx, qy = abs(x - cx) - hw + round_, abs(y - cy) - hh + round_
        return math.hypot(max(qx, 0), max(qy, 0)) + min(max(qx, qy), 0) - round_
    return Shape(f, (cx - hw, cy - hh, cx + hw, cy + hh))


def polygon(pts):
    pts = [p for i, p in enumerate(pts) if p != pts[i - 1]]
    n = len(pts)

    def f(x, y):
        d = float("inf")
        s = 1.0
        for i in range(n):
            ax, ay = pts[i]
            bx, by = pts[i - 1]
            ex, ey = bx - ax, by - ay
            wx, wy = x - ax, y - ay
            t = clamp((wx * ex + wy * ey) / (ex * ex + ey * ey))
            d = min(d, math.hypot(wx - ex * t, wy - ey * t))
            c1, c2, c3 = y >= ay, y < by, ex * wy > ey * wx
            if (c1 and c2 and c3) or (not c1 and not c2 and not c3):
                s = -s
        return s * d
    xs, ys = [p[0] for p in pts], [p[1] for p in pts]
    return Shape(f, (min(xs), min(ys), max(xs), max(ys)))


def regular(cx, cy, r, n, rot=0.0):
    return polygon([(cx + r * math.cos(math.radians(rot + i * 360 / n)),
                     cy + r * math.sin(math.radians(rot + i * 360 / n))) for i in range(n)])


def star(cx, cy, r0, r1, n, rot=-90.0):
    pts = []
    for i in range(n * 2):
        r = r0 if i % 2 == 0 else r1
        a = math.radians(rot + i * 180 / n)
        pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return polygon(pts)


def arc(cx, cy, r, w, a0, a1):
    """A stroke along a circle from angle a0 to a1 (degrees, clockwise on screen)."""
    pts = []
    steps = max(4, int(abs(a1 - a0) / 6))
    for i in range(steps + 1):
        a = math.radians(a0 + (a1 - a0) * i / steps)
        pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return polyline(pts, w)


def halfplane(nx, ny, d):
    """Inside where x*nx + y*ny < d."""
    return Shape(lambda x, y: x * nx + y * ny - d, (-10, -10, 10, 10))


# ---------------------------------------------------------------- pictogram parts

W = 0.065       # the usual stroke in icons drawn on the unit square


def bullet(cx, cy, h):
    return box(cx, cy + h * 0.15, 0.04, h * 0.35, 0.01) | circle(cx, cy - h * 0.2, 0.04)


def heart(cx, cy, s):
    return (circle(cx - s * 0.5, cy - s * 0.25, s * 0.55) | circle(cx + s * 0.5, cy - s * 0.25, s * 0.55)
            | polygon([(cx - s * 1.0, cy - s * 0.05), (cx + s * 1.0, cy - s * 0.05), (cx, cy + s * 1.1)]))


def skull(cx, cy, s):
    head = circle(cx, cy - s * 0.15, s) | box(cx, cy + s * 0.65, s * 0.55, s * 0.35, s * 0.1)
    eyes = circle(cx - s * 0.4, cy, s * 0.26) | circle(cx + s * 0.4, cy, s * 0.26)
    nose = polygon([(cx, cy + s * 0.25), (cx - s * 0.12, cy + s * 0.5), (cx + s * 0.12, cy + s * 0.5)])
    teeth = box(cx - s * 0.2, cy + s * 0.9, s * 0.05, s * 0.15) | box(cx + s * 0.2, cy + s * 0.9, s * 0.05, s * 0.15) \
        | box(cx, cy + s * 0.9, s * 0.05, s * 0.15)
    return head - eyes - nose - teeth, eyes.grow(-s * 0.06)


def chevron(cx, cy, s, w=W):
    """Pointing up."""
    return polyline([(cx - s, cy + s * 0.5), (cx, cy - s * 0.5), (cx + s, cy + s * 0.5)], w)


def chevron_right(cx, cy, s, w=W):
    return polyline([(cx - s * 0.5, cy - s), (cx + s * 0.5, cy), (cx - s * 0.5, cy + s)], w)


def reticle(r=0.17):
    s = ring(0.5, 0.5, r, 0.045)
    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        s = s | line(0.5 + dx * (r - 0.06), 0.5 + dy * (r - 0.06), 0.5 + dx * (r + 0.07), 0.5 + dy * (r + 0.07), 0.04)
    return s


def shield(cx, cy, s):
    return polygon([(cx - s, cy - s * 0.9), (cx, cy - s * 1.15), (cx + s, cy - s * 0.9),
                    (cx + s * 0.9, cy + s * 0.2), (cx, cy + s * 1.15), (cx - s * 0.9, cy + s * 0.2)])


def burst(cx, cy, r0, r1, n, rot=-90):
    return star(cx, cy, r0, r1, n, rot)


def arrow(ax, ay, bx, by, w=W, head=0.07):
    dx, dy = bx - ax, by - ay
    l = math.hypot(dx, dy)
    ux, uy = dx / l, dy / l
    px_, py_ = -uy, ux
    tip = polygon([(bx + ux * head * 0.6, by + uy * head * 0.6),
                   (bx - ux * head + px_ * head * 0.8, by - uy * head + py_ * head * 0.8),
                   (bx - ux * head - px_ * head * 0.8, by - uy * head - py_ * head * 0.8)])
    return line(ax, ay, bx - ux * head * 0.5, by - uy * head * 0.5, w) | tip


def zigzag(pts, w=W):
    return polyline(pts, w)


def tombstone(cx, cy, s):
    return box(cx, cy + s * 0.25, s * 0.6, s * 0.75) | circle(cx, cy - s * 0.5, s * 0.6)


def crown(cx, cy, s):
    return polygon([(cx - s, cy + s * 0.6), (cx - s, cy - s * 0.5), (cx - s * 0.5, cy), (cx, cy - s * 0.75),
                    (cx + s * 0.5, cy), (cx + s, cy - s * 0.5), (cx + s, cy + s * 0.6)])


def fan(cx, cy, n, spread, r0, r1, w):
    s = None
    for i in range(n):
        a = math.radians(-90 - spread / 2 + spread * i / max(1, n - 1))
        l = line(cx + math.cos(a) * r0, cy + math.sin(a) * r0, cx + math.cos(a) * r1, cy + math.sin(a) * r1, w)
        s = l if s is None else s | l
    return s


# ---------------------------------------------------------------- canvas


class Canvas:
    """RGBA in 0..1, rows top to bottom. Shapes are given in units where
    the canvas spans `span` (default: 0..1 across the width)."""

    def __init__(self, w, h, bg=(0, 0, 0, 0), span=1.0):
        self.w, self.h = w, h
        self.k = w / span                  # pixels per unit
        self.px = [list(bg) for _ in range(w * h)]

    def _range(self, box, pad):
        x0 = max(0, int(box[0] * self.k - pad))
        y0 = max(0, int(box[1] * self.k - pad))
        x1 = min(self.w, int(box[2] * self.k + pad) + 1)
        y1 = min(self.h, int(box[3] * self.k + pad) + 1)
        return x0, y0, x1, y1

    def fill(self, shape, color, alpha=1.0, soft=0.0):
        """color: (r, g, b) or a function (x, y) -> (r, g, b). soft widens the edge (units)."""
        aa = 1.0 / self.k
        width = aa + soft
        x0, y0, x1, y1 = self._range(shape.box, 2 + soft * self.k)
        fn = color if callable(color) else None
        for j in range(y0, y1):
            y = (j + 0.5) / self.k
            row = j * self.w
            for i in range(x0, x1):
                x = (i + 0.5) / self.k
                d = shape.sdf(x, y)
                if d > width:
                    continue
                cov = clamp(0.5 - d / width) * alpha
                if cov <= 0:
                    continue
                c = fn(x, y) if fn else color
                p = self.px[row + i]
                ia = 1 - cov
                p[0] = c[0] * cov + p[0] * ia
                p[1] = c[1] * cov + p[1] * ia
                p[2] = c[2] * cov + p[2] * ia
                p[3] = cov + p[3] * ia

    def glow(self, shape, color, radius, strength=1.0):
        """Light added around (and over) a shape, falling off over `radius` units."""
        x0, y0, x1, y1 = self._range(shape.box, radius * self.k * 2.5)
        r2 = radius * radius
        for j in range(y0, y1):
            y = (j + 0.5) / self.k
            row = j * self.w
            for i in range(x0, x1):
                x = (i + 0.5) / self.k
                d = max(shape.sdf(x, y), 0)
                g = strength * math.exp(-d * d / r2)
                if g < 0.004:
                    continue
                p = self.px[row + i]
                p[0] += color[0] * g
                p[1] += color[1] * g
                p[2] += color[2] * g
                p[3] = max(p[3], min(1.0, g))

    def shade(self, fn):
        """Apply fn(x, y, rgba) -> rgba to every pixel."""
        for j in range(self.h):
            y = (j + 0.5) / self.k
            for i in range(self.w):
                x = (i + 0.5) / self.k
                self.px[j * self.w + i] = list(fn(x, y, self.px[j * self.w + i]))

    def write(self, path, alpha=True):
        write_tga(path, self.w, self.h, self.px, alpha)


def write_tga(path, w, h, px, alpha=True):
    """Uncompressed TGA, top-left origin; px is a flat list of [r, g, b, a] in 0..1."""
    bpp = 32 if alpha else 24
    out = bytearray(struct.pack("<BBBHHBHHHHBB", 0, 0, 2, 0, 0, 0, 0, 0, w, h, bpp, 0x28 if alpha else 0x20))
    for p in px:
        b, g, r = (int(clamp(c) * 255 + 0.5) for c in (p[2], p[1], p[0]))
        out += bytes((b, g, r, int(clamp(p[3]) * 255 + 0.5))) if alpha else bytes((b, g, r))
    with open(path, "wb") as f:
        f.write(out)


# ---------------------------------------------------------------- materials


def worn_metal(seed, light, dark, top=0.0, bottom=1.0, grain=0.22):
    """A grainy metal fill, lit from above: a function for Canvas.fill."""
    n = Fbm(seed, 4, 24)
    fine = Noise(seed + 7, 256, 256)

    def f(x, y):
        t = smooth(top, bottom, y)
        k = 1 + grain * (n.at(x, y) - 0.5) * 2 + 0.12 * (fine.at(x * 300, y * 300) - 0.5)
        return tuple(clamp(c * k) for c in mix(light, dark, t))
    return f
