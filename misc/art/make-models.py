"""Generate the replaceable game assets: models, their textures and shaders,
packed into OUT_DIR/tremor-assets.pk3.

Usage: python3 make-models.py OUT_DIR

Every asset sits at the stable path the game asks for, so a local pack whose
name sorts after tremor-assets.pk3 replaces any of them without code changes:

  models/powerups/hourglass/hourglass.md3   the Hourglass pickup: a brass frame,
                                            glass bulbs and glowing sand
  icons/hourglass.tga                       its HUD icon
  models/abilities/orb/orb.md3              Dire Orb: a sphere for the swirl shader
  models/abilities/hook/claw.md3            the hook's three-pronged claw
  gfx/abilities/orb_swirl.tga               the orb's swirling surface
  gfx/abilities/plasmatrail.tga             Plasma Trail ribbon
  gfx/abilities/hookrope.tga                the hook's twisted cable
  gfx/champions-generated/<champion>.tga, _small.tga
                                            portraits (128 and 32 px): a hex badge
                                            in the class colour, a bust in the
                                            champion's hue, the monogram; the game
                                            prefers gfx/champions/<champion> (a
                                            portrait pack), then the model's icon
  scripts/tremor-assets.shader              the shaders above
All shapes are built here from lathes and boxes; textures from noise.
"""
import math, os, struct, sys, zipfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tremorart import *


# ---------------------------------------------------------------- meshes

class Mesh:
    """Triangles with per-vertex normals and texture coordinates."""

    def __init__(self, shader):
        self.shader = shader
        self.xyz, self.normal, self.st, self.tris = [], [], [], []

    def vert(self, p, n, st):
        self.xyz.append(p)
        length = math.sqrt(sum(c * c for c in n)) or 1.0
        self.normal.append(tuple(c / length for c in n))
        self.st.append(st)
        return len(self.xyz) - 1

    def tri(self, a, b, c):
        self.tris.append((a, b, c))

    def quad(self, a, b, c, d):
        self.tri(a, b, c)
        self.tri(a, c, d)


def lathe(mesh, profile, segments=24, centre=(0.0, 0.0, 0.0)):
    """Turn a profile of (radius, z) points around the z axis."""
    rings = []
    for i, (r, z) in enumerate(profile):
        # the normal from the profile's slope
        r0, z0 = profile[max(i - 1, 0)]
        r1, z1 = profile[min(i + 1, len(profile) - 1)]
        dr, dz = r1 - r0, z1 - z0
        nr, nz = dz, -dr
        ring = []
        for s in range(segments + 1):
            a = 2 * math.pi * s / segments
            ca, sa = math.cos(a), math.sin(a)
            p = (centre[0] + r * ca, centre[1] + r * sa, centre[2] + z)
            ring.append(mesh.vert(p, (nr * ca, nr * sa, nz), (s / segments, i / max(len(profile) - 1, 1))))
        rings.append(ring)
    for i in range(len(rings) - 1):
        for s in range(segments):
            mesh.quad(rings[i][s], rings[i][s + 1], rings[i + 1][s + 1], rings[i + 1][s])


def box(mesh, lo, hi):
    """An axis-aligned box from corner lo to corner hi."""
    x0, y0, z0 = lo
    x1, y1, z1 = hi
    faces = [
        ((1, 0, 0), [(x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1)]),
        ((-1, 0, 0), [(x0, y1, z0), (x0, y0, z0), (x0, y0, z1), (x0, y1, z1)]),
        ((0, 1, 0), [(x1, y1, z0), (x0, y1, z0), (x0, y1, z1), (x1, y1, z1)]),
        ((0, -1, 0), [(x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1)]),
        ((0, 0, 1), [(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)]),
        ((0, 0, -1), [(x0, y1, z0), (x1, y1, z0), (x1, y0, z0), (x0, y0, z0)]),
    ]
    for n, corners in faces:
        idx = [mesh.vert(c, n, st) for c, st in zip(corners, [(0, 1), (1, 1), (1, 0), (0, 0)])]
        mesh.quad(*idx)


def transform(mesh, start, fn):
    """Move the vertices added since index start through fn(point) -> point."""
    for i in range(start, len(mesh.xyz)):
        mesh.xyz[i] = fn(mesh.xyz[i])


def rotate_z(p, a):
    c, s = math.cos(a), math.sin(a)
    return (p[0] * c - p[1] * s, p[0] * s + p[1] * c, p[2])


# ---------------------------------------------------------------- MD3 writer

def encode_normal(n):
    x, y, z = n
    lat = int(round(math.atan2(y, x) * 256 / (2 * math.pi))) & 255
    lng = int(round(math.acos(max(-1.0, min(1.0, z))) * 256 / (2 * math.pi))) & 255
    return (lat << 8) | lng


def name_bytes(s, n):
    return s.encode("ascii")[:n - 1].ljust(n, b"\0")


def write_md3(path, name, meshes):
    """One frame, no tags, a surface per mesh."""
    pts = [p for m in meshes for p in m.xyz]
    lo = [min(p[i] for p in pts) for i in range(3)]
    hi = [max(p[i] for p in pts) for i in range(3)]
    radius = max(math.sqrt(sum(c * c for c in p)) for p in pts)

    surfaces = b""
    for k, m in enumerate(meshes):
        head = 108
        ofs_shaders = head
        ofs_tris = ofs_shaders + 68
        ofs_st = ofs_tris + 12 * len(m.tris)
        ofs_xyz = ofs_st + 8 * len(m.xyz)
        ofs_end = ofs_xyz + 8 * len(m.xyz)
        s = b"IDP3" + name_bytes("s%i" % k, 64)
        s += struct.pack("<9i", 0, 1, 1, len(m.xyz), len(m.tris), ofs_tris, ofs_shaders, ofs_st, ofs_xyz)
        s += struct.pack("<i", ofs_end)
        s += name_bytes(m.shader, 64) + struct.pack("<i", 0)
        for t in m.tris:
            s += struct.pack("<3i", *t)
        for st in m.st:
            s += struct.pack("<2f", *st)
        for p, n in zip(m.xyz, m.normal):
            s += struct.pack("<3hH", *(int(round(c * 64)) for c in p), encode_normal(n))
        surfaces += s

    ofs_frames = 108
    ofs_tags = ofs_frames + 56
    ofs_surfaces = ofs_tags
    ofs_eof = ofs_surfaces + len(surfaces)
    out = b"IDP3" + struct.pack("<i", 15) + name_bytes(name, 64)
    out += struct.pack("<9i", 0, 1, 0, len(meshes), 0, ofs_frames, ofs_tags, ofs_surfaces, ofs_eof)
    out += struct.pack("<10f", *lo, *hi, 0, 0, 0, radius) + name_bytes("frame0", 16)
    out += surfaces
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(out)


# ---------------------------------------------------------------- textures

def texture(path, w, h, fn, alpha=True):
    px = [fn(x / w, y / h) for y in range(h) for x in range(w)]
    os.makedirs(os.path.dirname(path), exist_ok=True)
    write_tga(path, w, h, px, alpha)


def brass(u, v, n=Fbm(7, 4, 6)):
    k = 0.75 + 0.35 * n.at(u * 4, v * 4)
    return [0.72 * k, 0.55 * k, 0.24 * k, 1]


def sand(u, v, n=Fbm(11, 3, 16)):
    k = 0.8 + 0.3 * n.at(u * 8, v * 8)
    return [1.0 * k, 0.78 * k, 0.35 * k, 1]


def orb_swirl(u, v, n=Fbm(23, 5, 4)):
    x, y = u - 0.5, v - 0.5
    r = math.sqrt(x * x + y * y)
    a = math.atan2(y, x)
    swirl = 0.5 + 0.5 * math.sin(a * 3 + r * 18 + 3 * n.at(u * 3, v * 3))
    k = swirl * (0.6 + 0.4 * n.at(u * 6, v * 6))
    return [0.45 * k + 0.08, 0.12 * k + 0.02, 0.75 * k + 0.12, 1]


def trail(u, v, n=Fbm(31, 4, 8)):
    # across the ribbon (v): a hot core with a cyan edge; along it (u): noise
    d = abs(v - 0.5) * 2
    core = max(0.0, 1 - d * 1.6) ** 1.5
    edge = max(0.0, 1 - abs(d - 0.8) * 6)
    k = 0.7 + 0.5 * n.at(u * 6, v * 2)
    return [(0.95 * core + 0.1 * edge) * k, (0.35 * core + 0.85 * edge) * k, (0.85 * core + 0.95 * edge) * k, 1]


def rope(u, v):
    # a twisted cable: diagonal strands with dark grooves
    t = (u * 4 + v * 2) % 1.0
    k = 0.45 + 0.45 * math.sin(t * math.pi) ** 0.6
    a = max(0.0, 1 - abs(v - 0.5) * 2.2)
    return [0.62 * k, 0.6 * k, 0.56 * k, min(1.0, a * 3)]


def hourglass_icon(u, v):
    x, y = (u - 0.5) * 2, (v - 0.5) * 2
    inside = abs(x) < 0.18 + 0.55 * abs(y) and abs(y) < 0.8
    frame = abs(y) > 0.8 and abs(y) < 0.95 and abs(x) < 0.75
    lower = inside and y > 0.25
    if frame:
        return [0.85, 0.66, 0.3, 1]
    if lower:
        return [1.0, 0.8, 0.35, 1]
    if inside:
        return [0.55, 0.8, 0.95, 0.55]
    return [0, 0, 0, 0]


# ---------------------------------------------------------------- the models

def hourglass(out):
    frame, glass, grain = Mesh("models/powerups/hourglass/frame"), Mesh("models/powerups/hourglass/glass"), Mesh("models/powerups/hourglass/sand")
    # top and bottom plates
    lathe(frame, [(0, 21), (12, 21), (12, 18), (0, 18)], 6)
    lathe(frame, [(0, -18), (12, -18), (12, -21), (0, -21)], 6)
    # three posts
    for k in range(3):
        start = len(frame.xyz)
        box(frame, (9, -1.2, -18), (11.4, 1.2, 18))
        transform(frame, start, lambda p, a=k * 2 * math.pi / 3 + math.pi / 6: rotate_z(p, a))
    # the glass, waisted in the middle
    lathe(glass, [(0.5, 18), (7.5, 17), (8.5, 11), (5, 4), (1.2, 0), (5, -4), (8.5, -11), (7.5, -17), (0.5, -18)], 20)
    # sand: a heap in the bottom bulb and a thin stream
    lathe(grain, [(0.1, -6), (2.5, -8), (6.5, -12), (7.6, -16.5), (0.1, -16.5)], 16)
    lathe(grain, [(0.4, 1), (0.4, -7)], 6)
    write_md3(os.path.join(out, "models/powerups/hourglass/hourglass.md3"), "hourglass", [frame, grain, glass])
    texture(os.path.join(out, "models/powerups/hourglass/frame.tga"), 64, 64, brass, False)
    texture(os.path.join(out, "models/powerups/hourglass/sand.tga"), 64, 64, sand, False)
    texture(os.path.join(out, "icons/hourglass.tga"), 64, 64, hourglass_icon)


def orb(out):
    m = Mesh("ability/orb")
    lathe(m, [(12 * math.sin(math.pi * i / 12), 12 * math.cos(math.pi * i / 12)) for i in range(13)], 24)
    write_md3(os.path.join(out, "models/abilities/orb/orb.md3"), "orb", [m])
    texture(os.path.join(out, "gfx/abilities/orb_swirl.tga"), 128, 128, orb_swirl, False)


def claw(out):
    m = Mesh("ability/hookclaw")
    # a hub along +x (the direction of travel) and three prongs bending back
    lathe(m, [(0, 0), (3, 1), (3.5, 6), (2, 9), (0, 10)], 10)
    transform(m, 0, lambda p: (p[2], p[1], -p[0]))      # lathe axis z to x
    for k in range(3):
        start = len(m.xyz)
        box(m, (2, -1, 2), (9, 1, 4))                     # out from the hub
        box(m, (7, -1, 3), (9, 1, 9))                     # and curling back
        box(m, (4, -1, 7.5), (8, 1, 9.5))                 # the tip
        transform(m, start, lambda p, a=k * 2 * math.pi / 3: (p[0], p[1] * math.cos(a) - p[2] * math.sin(a), p[1] * math.sin(a) + p[2] * math.cos(a)))
    write_md3(os.path.join(out, "models/abilities/hook/claw.md3"), "claw", [m])
    texture(os.path.join(out, "models/abilities/hook/claw.tga"), 64, 64,
            lambda u, v, n=Fbm(41, 4, 8): [0.55 * (0.7 + 0.4 * n.at(u * 4, v * 4))] * 3 + [1], False)
    texture(os.path.join(out, "gfx/abilities/hookrope.tga"), 64, 32, rope)


# ---------------------------------------------------------------- portraits

CHAMPIONS = [
    ("ranger", "RG", "medium"), ("anarki", "AN", "light"), ("athena", "AT", "light"), ("nyx", "NX", "light"),
    ("slash", "SL", "light"), ("bj", "BJ", "medium"), ("deathknight", "DK", "medium"), ("doomslayer", "DS", "medium"),
    ("eisen", "EI", "medium"), ("galena", "GA", "medium"), ("strogg", "ST", "medium"), ("visor", "VI", "medium"),
    ("clutch", "CL", "heavy"), ("keel", "KE", "heavy"), ("scalebearer", "SB", "heavy"), ("sorlag", "SO", "heavy"),
    ("wrack", "WR", "medium"), ("sarge", "SA", "medium"), ("klesk", "KL", "light"), ("hunter", "HU", "light"),
    ("xaero", "XA", "heavy"), ("raine", "RA", "light"), ("beastmaster", "BM", "heavy"), ("unumrey", "UN", "medium"),
]

CLASS_COLOURS = {"light": (0.30, 0.75, 0.95), "medium": (0.95, 0.75, 0.25), "heavy": (0.90, 0.25, 0.20)}

# a 5 x 7 block font, rows top to bottom, for the monograms
GLYPHS = {
    "A": ["01110", "10001", "10001", "11111", "10001", "10001", "10001"],
    "B": ["11110", "10001", "10001", "11110", "10001", "10001", "11110"],
    "C": ["01111", "10000", "10000", "10000", "10000", "10000", "01111"],
    "D": ["11110", "10001", "10001", "10001", "10001", "10001", "11110"],
    "E": ["11111", "10000", "10000", "11110", "10000", "10000", "11111"],
    "G": ["01111", "10000", "10000", "10011", "10001", "10001", "01111"],
    "H": ["10001", "10001", "10001", "11111", "10001", "10001", "10001"],
    "I": ["11111", "00100", "00100", "00100", "00100", "00100", "11111"],
    "J": ["00111", "00010", "00010", "00010", "10010", "10010", "01100"],
    "K": ["10001", "10010", "10100", "11000", "10100", "10010", "10001"],
    "L": ["10000", "10000", "10000", "10000", "10000", "10000", "11111"],
    "M": ["10001", "11011", "10101", "10101", "10001", "10001", "10001"],
    "N": ["10001", "11001", "10101", "10011", "10001", "10001", "10001"],
    "O": ["01110", "10001", "10001", "10001", "10001", "10001", "01110"],
    "R": ["11110", "10001", "10001", "11110", "10100", "10010", "10001"],
    "S": ["01111", "10000", "10000", "01110", "00001", "00001", "11110"],
    "T": ["11111", "00100", "00100", "00100", "00100", "00100", "00100"],
    "U": ["10001", "10001", "10001", "10001", "10001", "10001", "01110"],
    "V": ["10001", "10001", "10001", "10001", "10001", "01010", "00100"],
    "W": ["10001", "10001", "10001", "10101", "10101", "11011", "10001"],
    "X": ["10001", "10001", "01010", "00100", "01010", "10001", "10001"],
}


def monogram(c, text, cx, cy, cell, colour):
    """Block letters centred at (cx, cy), cell units per font pixel."""
    w = len(text) * 6 * cell - cell
    x0 = cx - w / 2
    for k, ch in enumerate(text):
        rows = GLYPHS.get(ch)
        if not rows:
            continue
        for j, row in enumerate(rows):
            for i, bit in enumerate(row):
                if bit == "1":
                    x = x0 + (k * 6 + i) * cell
                    y = cy - 3.5 * cell + j * cell
                    c.fill(polygon([(x, y), (x + cell, y), (x + cell, y + cell), (x, y + cell)]), colour)


def hue(name):
    h = sum(ord(ch) * (i + 7) for i, ch in enumerate(name)) % 360 / 360.0
    i = int(h * 6)
    f = h * 6 - i
    q, t = 1 - f, f
    r, g, b = [(1, t, 0), (q, 1, 0), (0, 1, t), (0, q, 1), (t, 0, 1), (1, 0, q)][i % 6]
    return (0.35 + 0.5 * r, 0.35 + 0.5 * g, 0.35 + 0.5 * b)


def portraits(out):
    for name, mono, cls in CHAMPIONS:
        frame = CLASS_COLOURS[cls]
        for size, suffix in ((128, ""), (32, "_small")):
            c = Canvas(size, size)
            hexagon = regular(0.5, 0.5, 0.48, 6, 0)
            c.fill(hexagon, frame)
            c.fill(regular(0.5, 0.5, 0.42, 6, 0), INK)
            if size >= 64:
                # a bust: head and shoulders in the champion's hue
                body = hue(name)
                c.fill(circle(0.5, 0.3, 0.12), body)
                c.fill(polygon([(0.26, 0.64), (0.32, 0.46), (0.68, 0.46), (0.74, 0.64)]), scale(body, 0.75))
                monogram(c, mono, 0.5, 0.76, 0.018, SILVER)
            else:
                monogram(c, mono, 0.5, 0.5, 0.05, SILVER)
            path = os.path.join(out, "gfx/champions-generated/%s%s.tga" % (name, suffix))
            os.makedirs(os.path.dirname(path), exist_ok=True)
            write_tga(path, size, size, c.px)


SHADERS = """// tremor-invictus generated assets (misc/art/make-models.py)
// Replaceable: a pack loaded later may define these names again.

models/powerups/hourglass/frame
{
	{
		map models/powerups/hourglass/frame.tga
		rgbGen lightingDiffuse
	}
	{
		map $whiteimage
		blendFunc GL_DST_COLOR GL_ONE
		rgbGen wave sin 0.15 0.1 0 0.5
	}
}

models/powerups/hourglass/sand
{
	{
		map models/powerups/hourglass/sand.tga
		rgbGen wave sin 0.9 0.15 0 0.7
	}
}

models/powerups/hourglass/glass
{
	cull none
	surfaceparm trans
	sort additive
	{
		map $whiteimage
		blendFunc GL_ONE GL_ONE
		rgbGen const ( 0.08 0.18 0.26 )
	}
}

ability/orb
{
	cull none
	{
		map gfx/abilities/orb_swirl.tga
		tcMod rotate 30
		tcMod turb 0 0.05 0 0.2
		rgbGen entity
	}
	{
		map gfx/abilities/orb_swirl.tga
		blendFunc GL_ONE GL_ONE
		tcMod rotate -55
		tcMod scale 1.5 1.5
		rgbGen entity
	}
}

ability/hookclaw
{
	{
		map models/abilities/hook/claw.tga
		rgbGen lightingDiffuse
	}
}

ability/hookrope
{
	cull none
	{
		map gfx/abilities/hookrope.tga
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen vertex
		alphaGen vertex
	}
}

ability/plasmatrail
{
	cull none
	sort additive
	{
		map gfx/abilities/plasmatrail.tga
		blendFunc GL_ONE GL_ONE
		tcMod scroll 0.25 0
		rgbGen vertex
	}
	{
		map gfx/abilities/plasmatrail.tga
		blendFunc GL_ONE GL_ONE
		tcMod scroll -0.4 0.05
		tcMod scale 2 1
		rgbGen wave noise 0.5 0.3 0 3
	}
}
"""


def main():
    out = sys.argv[1]
    src = os.path.join(out, "assets-src")
    hourglass(src)
    orb(src)
    claw(src)
    portraits(src)
    texture(os.path.join(src, "gfx/abilities/plasmatrail.tga"), 128, 64, trail, False)
    os.makedirs(os.path.join(src, "scripts"), exist_ok=True)
    with open(os.path.join(src, "scripts/tremor-assets.shader"), "w") as f:
        f.write(SHADERS)
    with zipfile.ZipFile(os.path.join(out, "tremor-assets.pk3"), "w", zipfile.ZIP_DEFLATED) as z:
        for root, _, files in os.walk(src):
            for name in sorted(files):
                full = os.path.join(root, name)
                z.write(full, os.path.relpath(full, src))


if __name__ == "__main__":
    main()
