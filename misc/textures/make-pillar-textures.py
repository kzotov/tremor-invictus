"""Generate the pillar and podium textures for the lineup and podium scenes.

Usage: python3 make-pillar-textures.py OUT_DIR

Writes TGA files to OUT_DIR/gfx/tremor/pillar/. Everything is drawn here from
noise and shapes; no source images are used. Plain Python 3, no packages.

Each surface has a base image (dark metal, lighting baked in, since the
pillars are drawn unlit) and a glow mask in grey. The shaders add the glow
over the base tinted by the vertex colour, so one set of images serves the
neutral (logo red), red team and blue team pillars.

  side.tga / side_glow.tga   256 x 1024, one hexagon face (24 units wide)
                             from the top edge down 128 units
  cap.tga / cap_glow.tga     256 x 256, the top face, mapped flat across
                             48 x 48 units centred on the column
  rank1..3_glow.tga          128 x 128, numerals I, II, III for the podium,
                             added over the face that looks at the camera
"""
import math, os, random, struct, sys

# ---------------------------------------------------------------- helpers


def clamp(x, lo=0.0, hi=1.0):
    return lo if x < lo else hi if x > hi else x


def smooth(e0, e1, x):
    t = clamp((x - e0) / (e1 - e0))
    return t * t * (3 - 2 * t)


def band(x, lo, hi, soft):
    """1 inside [lo, hi], fading to 0 over `soft` outside it."""
    return smooth(lo - soft, lo, x) * (1 - smooth(hi, hi + soft, x))


def mix(a, b, t):
    return a + (b - a) * t


class Noise:
    """Value noise on a lattice that wraps at px x py cells."""

    def __init__(self, seed, px, py):
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
    """Octaves of value noise over the unit square, tiling in both axes."""

    def __init__(self, seed, px, py, octaves):
        self.layers = [Noise(seed * 31 + i, px << i, py << i) for i in range(octaves)]

    def at(self, u, v):
        total, amp, norm = 0.0, 1.0, 0.0
        for n in self.layers:
            total += n.at(u * n.px, v * n.py) * amp
            norm += amp
            amp *= 0.5
        return total / norm


def seg_dist(px, py, ax, ay, bx, by):
    dx, dy = bx - ax, by - ay
    t = clamp(((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy))
    ex, ey = px - ax - dx * t, py - ay - dy * t
    return math.sqrt(ex * ex + ey * ey)


def write_tga(path, w, h, pixels):
    """24-bit uncompressed TGA; `pixels` is rows top to bottom of (r, g, b) in 0..1."""
    out = bytearray(struct.pack("<BBBHHBHHHHBB", 0, 0, 2, 0, 0, 0, 0, 0, w, h, 24, 0x20))
    for row in pixels:
        for r, g, b in row:
            out += bytes((int(clamp(b) * 255 + 0.5), int(clamp(g) * 255 + 0.5), int(clamp(r) * 255 + 0.5)))
    with open(path, "wb") as f:
        f.write(out)


# Metal tones, a little warm like the logo's ring
METAL = (0.155, 0.152, 0.158)
METAL_LIGHT = (0.47, 0.46, 0.44)
METAL_DARK = (0.045, 0.045, 0.05)


def shade(k):
    """Metal at brightness k: 0 is deep shadow, 1 base, 2 highlight."""
    if k < 1:
        return tuple(mix(d, m, k) for d, m in zip(METAL_DARK, METAL))
    return tuple(mix(m, l, k - 1) for m, l in zip(METAL, METAL_LIGHT))


# ---------------------------------------------------------------- side

SIDE_W, SIDE_H = 256, 1024
FACE_W, FACE_H = 24.0, 128.0        # units the side image spans

# The trace across the collar: (x, height) in units, the same at both ends
# so it runs on round the column from face to face
TRACE = [(0, 0), (4.5, 0), (5.3, -0.7), (6.1, 0.5), (6.8, 0), (9.2, 0), (10.1, -2.6),
         (11.2, 3.2), (12.3, -3.6), (13.2, 1.4), (13.9, -0.5), (14.6, 0), (17.4, 0),
         (18.1, 0.6), (18.8, -0.4), (19.4, 0), (24, 0)]
TRACE_Y = 9.0                       # centre line of the trace
SEAMS = [14.0, 50.0, 86.0, 122.0]   # panel joints below the collar
CONDUIT = (10.6, 13.4)              # recessed channel down the middle of each face
CONDUIT_TOP = 36.0                  # where it starts, below the plate
PLATE = (4.0, 20.0, 16.5, 33.5)     # inset plate under the collar: x0, x1, y0, y1


def make_side():
    brush = Fbm(1, 96, 2, 3)        # long vertical streaks
    mottle = Fbm(2, 3, 12, 4)       # patchy wear
    grain = Noise(3, 256, 1024)
    rng = random.Random(4)
    scratches = []
    for _ in range(70):
        x, y = rng.uniform(0, FACE_W), rng.uniform(4, FACE_H)
        a = rng.uniform(-0.5, 0.5) + (math.pi / 2 if rng.random() < 0.3 else 0)
        l = rng.uniform(0.8, 3.5)
        scratches.append((x, y, x + math.cos(a) * l, y + math.sin(a) * l, rng.uniform(0.2, 0.6)))

    base, glow = [], []
    for j in range(SIDE_H):
        y = (j + 0.5) / SIDE_H * FACE_H
        v = (j + 0.5) / SIDE_H
        rb, rg = [], []
        for i in range(SIDE_W):
            x = (i + 0.5) / SIDE_W * FACE_W
            u = (i + 0.5) / SIDE_W

            k = 0.82 + 0.3 * brush.at(u, v) + 0.25 * (mottle.at(u, v) - 0.5) + 0.06 * (grain.at(i, j) - 0.5)
            g = 0.0

            # the top lip: a bright chamfer, then a groove holding the glow strip
            if y < 1.3:
                k = mix(2.0, 1.4, y / 1.3)
            elif y < 4.2:
                k = 0.35 + 0.25 * band(y, 1.6, 3.9, 0.25)
                d = abs(y - 2.75)
                g = max(g, 1 - smooth(0.55, 0.8, d))
                g = max(g, 0.45 * math.exp(-d * d / 1.8))
            elif y < SEAMS[0]:
                # the collar: a raised plate with the trace cut into it
                k += 0.18
                k += 0.5 * (1 - smooth(0, 0.6, y - 4.2))   # lit upper edge
                k -= 0.5 * smooth(SEAMS[0] - 0.8, SEAMS[0], y)
                if 5 < y < 13.5:
                    d = min(seg_dist(x, y, TRACE[n][0], TRACE_Y + TRACE[n][1],
                                     TRACE[n + 1][0], TRACE_Y + TRACE[n + 1][1])
                            for n in range(len(TRACE) - 1))
                    k *= 0.35 + 0.65 * smooth(0.25, 0.55, d)
                    g = max(g, 1 - smooth(0.12, 0.32, d))
                    g = max(g, 0.5 * math.exp(-d * d / 0.9))
            else:
                # panels: joints with a lit lower lip and rivets below them
                for s in SEAMS:
                    dy = y - s
                    if -0.5 < dy < 0.4:
                        k *= 0.2
                    elif 0.4 <= dy < 1.0:
                        k += 0.7 * (1 - (dy - 0.4) / 0.6)
                    for rx in (2.6, FACE_W - 2.6):
                        dx, dz = x - rx, dy - 2.6
                        r = math.sqrt(dx * dx + dz * dz)
                        if r < 0.85:
                            k = 1.1 + 0.9 * clamp(-(dx + dz) / 0.85) - 0.4 * clamp((dx + dz) / 0.85)
                        elif r < 1.15:
                            k *= 0.45
                # the inset plate that holds the podium numerals
                x0, x1, y0, y1 = PLATE
                if x0 < x < x1 and y0 < y < y1:
                    k *= 0.55
                    k += 0.6 * (1 - smooth(0, 0.5, x1 - x)) + 0.6 * (1 - smooth(0, 0.5, y1 - y))
                    k *= 0.6 + 0.4 * smooth(0, 0.5, min(x - x0, y - y0))
                # the conduit: a dark channel with a broken glowing line
                if y > CONDUIT_TOP:
                    c0, c1 = CONDUIT
                    if c0 < x < c1:
                        k *= 0.38
                        if x < c0 + 0.35:
                            k = 1.6
                    elif c0 - 0.4 < x <= c0 or c1 <= x < c1 + 0.4:
                        k *= 0.6
                    d = abs(x - (c0 + c1) / 2)
                    fade = math.exp(-(y - CONDUIT_TOP) / 30)
                    gap = band((y - CONDUIT_TOP) % 9.0, 1.4, 8.4, 0.3)
                    g = max(g, fade * gap * (1 - smooth(0.18, 0.4, d)))
                    g = max(g, fade * 0.35 * math.exp(-d * d / 1.2))
                    # a bleed of light onto the channel floor
                    if c0 < x < c1:
                        k += 0.5 * fade * gap * math.exp(-d * d / 0.5)

            # scratches catching the light
            for sx0, sy0, sx1, sy1, a in scratches:
                if min(sy0, sy1) - 0.2 < y < max(sy0, sy1) + 0.2 and min(sx0, sx1) - 0.2 < x < max(sx0, sx1) + 0.2:
                    if seg_dist(x, y, sx0, sy0, sx1, sy1) < 0.06 + 0.03 * a:
                        k += a

            # hexagon edges: the corner between faces catches the light
            ex = min(x, FACE_W - x)
            if y > 1.3:
                k += 0.55 * (1 - smooth(0.0, 0.7, ex))
                k *= 0.75 + 0.25 * smooth(0.7, 2.0, ex)

            # darker down the column, into the floor's shadow
            k *= mix(1.0, 0.35, smooth(16, FACE_H, y))
            rb.append(shade(k))
            rg.append((g, g, g))
        base.append(rb)
        glow.append(rg)
    return base, glow


# ---------------------------------------------------------------- cap

CAP_N = 256
CAP_SPAN = 48.0
APOTHEM = 24 * math.cos(math.radians(30))
HEX_NORMALS = [(math.cos(math.radians(a)), math.sin(math.radians(a))) for a in (0, 60, 120)]


def hexdist(x, y):
    """Distance to the centre in the hexagon's own metric, 1 on the edges."""
    return max(abs(x * nx + y * ny) for nx, ny in HEX_NORMALS) / APOTHEM


def make_cap():
    mottle = Fbm(5, 4, 4, 4)
    grain = Noise(6, 256, 256)
    corners = [(math.cos(math.radians(30 + 60 * c)), math.sin(math.radians(30 + 60 * c))) for c in range(6)]
    base, glow = [], []
    for j in range(CAP_N):
        y = ((j + 0.5) / CAP_N - 0.5) * CAP_SPAN
        rb, rg = [], []
        for i in range(CAP_N):
            x = ((i + 0.5) / CAP_N - 0.5) * CAP_SPAN
            u, v = (i + 0.5) / CAP_N, (j + 0.5) / CAP_N
            r = hexdist(x, y)
            k = 0.95 + 0.3 * (mottle.at(u, v) - 0.5) + 0.06 * (grain.at(i, j) - 0.5)
            g = 0.0
            if r > 1.0:
                k = 0.3                         # outside the column, never seen
            elif r > 0.93:
                k = mix(1.9, 1.3, (r - 0.93) / 0.07)
            elif r > 0.905:
                k = 0.3
            elif r > 0.845:
                # the glow ring sits in a groove
                k = 0.3
                d = abs(r - 0.875) * APOTHEM
                g = max(g, 1 - smooth(0.3, 0.5, d))
            elif r > 0.82:
                k = 0.3
            else:
                # tread plate: raised diamonds in a lattice turned 30 degrees
                a, b = x * 0.866 + y * 0.5, -x * 0.5 + y * 0.866
                fa, fb = (a / 3.0) % 1.0 - 0.5, (b / 3.0) % 1.0 - 0.5
                if abs(fa) + abs(fb) * 2.6 < 0.42:
                    k += 0.25 + 0.5 * clamp(-(fa + fb) * 3)
                else:
                    k *= 0.85
                # an engraved inner hexagon
                if abs(r - 0.48) * APOTHEM < 0.3:
                    k *= 0.35
                elif 0.3 <= (r - 0.48) * APOTHEM < 0.6:
                    k += 0.4
                k += 0.4 * (1 - smooth(0.0, 0.05, 0.82 - r))   # lit inner edge
            # short ticks from the ring out to each corner
            if 0.6 < r < 0.905:
                for cx, cy in corners:
                    t = x * cx + y * cy
                    d = abs(x * cy - y * cx)
                    if t > 0 and d < 0.9 and r < 0.845:
                        g = max(g, (1 - smooth(0.25, 0.45, d)) * smooth(0.7, 0.8, r))
            # a halo round all the glow
            if 0.78 < r < 0.97:
                d = abs(r - 0.875) * APOTHEM
                g = max(g, 0.4 * math.exp(-d * d / 1.5))
            rb.append(shade(k))
            rg.append((g, g, g))
        base.append(rb)
        glow.append(rg)
    return base, glow


# ---------------------------------------------------------------- numerals

RANK_N = 128


def make_rank(n):
    """Roman numeral n: upright strokes with flared serifs, centred, glowing."""
    stroke, gap, height = 0.15, 0.08, 0.66
    width = n * stroke + (n - 1) * gap
    top, bottom = 0.5 - height / 2, 0.5 + height / 2
    xs = [0.5 - width / 2 + s * (stroke + gap) + stroke / 2 for s in range(n)]
    serif_w, serif_h = stroke * 1.0, 0.06
    bar_l, bar_r = xs[0] - stroke / 2 - serif_w * 0.4, xs[-1] + stroke / 2 + serif_w * 0.4
    rows = []
    for j in range(RANK_N):
        y = (j + 0.5) / RANK_N
        row = []
        for i in range(RANK_N):
            x = (i + 0.5) / RANK_N
            d = 1.0
            for cx in xs:
                # stroke, wider at the serifs: the half-width grows near the ends
                e = min(y - top, bottom - y)
                hw = stroke / 2 + serif_w * 0.5 * (1 - smooth(0, serif_h, e))
                dx = abs(x - cx) - hw
                dy = max(top - y, y - bottom)
                d = min(d, max(dx, dy))
            # joined serif bars top and bottom across all strokes
            for ey in (top, bottom):
                dy = abs(y - ey) - 0.012
                dx = max(bar_l - x, x - bar_r)
                d = min(d, max(dx, dy))
            core = 1 - smooth(-0.004, 0.006, d)
            halo = 0.45 * math.exp(-max(d, 0) ** 2 / 0.0012)
            g = max(core, halo)
            row.append((g, g, g))
        rows.append(row)
    return rows


# ---------------------------------------------------------------- main


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    out = os.path.join(sys.argv[1], "gfx", "tremor", "pillar")
    os.makedirs(out, exist_ok=True)
    base, glow = make_side()
    write_tga(os.path.join(out, "side.tga"), SIDE_W, SIDE_H, base)
    write_tga(os.path.join(out, "side_glow.tga"), SIDE_W, SIDE_H, glow)
    base, glow = make_cap()
    write_tga(os.path.join(out, "cap.tga"), CAP_N, CAP_N, base)
    write_tga(os.path.join(out, "cap_glow.tga"), CAP_N, CAP_N, glow)
    for n in (1, 2, 3):
        write_tga(os.path.join(out, "rank%d_glow.tga" % n), RANK_N, RANK_N, make_rank(n))


if __name__ == "__main__":
    main()
