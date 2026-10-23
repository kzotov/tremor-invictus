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
  sound/tremor/hit_armor.wav, hit_health.wav, armor_break.wav
  sound/tremor/slide_start.wav, slide_loop.wav, slide_stop.wav   the crouch slide
  sound/tremor/dodge.wav                    Clutch's dodge
  sound/tremor/hook_attach.wav              Athena's hook biting
                                            hit tones: a bright ping on armour, a
                                            dull knock on health, a crack when the
                                            armour breaks (synthesised)
  sound/tremor/match_found.wav              the ready check's call: three rising
                                            bell tones
  models/abilities/totem/totem.md3          the Unholy Totem: a bone post with a
                                            skull knob and a glowing ring
  models/abilities/turret/turret.md3        the sentry: a tripod, a turning head
                                            and a barrel along +x
  models/abilities/barrier/barrier.md3      the barrier: a faceted shell around a
                                            standing player
  gfx/abilities/acid_ball.tga, acid_mark.tga
                                            the acid blob's sprite and its splash
  icons/iconw_hmg.tga, iconw_ssg.tga, iconw_sng.tga, iconw_tribolt.tga
                                            the heavy weapons' own icons: gold
                                            silhouettes, three barrels, two
                                            barrels, a drum, three bolts, so
                                            none reuses another weapon's icon
  sound/abilities/<ability>_cast.wav, _end.wav
                                            a placeholder call per ability: a
                                            swept tone pitched by its name
  scripts/tremor-assets.shader              the shaders above
All shapes are built here from lathes and boxes; textures from noise.
"""
import math, os, random, struct, sys, zipfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tremorart import *
import tremorart as art


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


def totem(out):
    post, ring = Mesh("ability/totem"), Mesh("ability/totemring")
    # a foot, a tapering post and a round knob with two eye sockets
    lathe(post, [(0, 0), (7, 0), (7, 2), (3, 4), (2.2, 18), (2.6, 19), (0.1, 19)], 10)
    lathe(post, [(0.1, 18.5), (3.5, 19.5), (5, 23), (4.5, 26.5), (2.5, 28.5), (0.1, 29)], 12)
    for side in (-1, 1):
        box(post, (3.6, side * 2.2 - 0.9, 23.5), (4.9, side * 2.2 + 0.9, 25))
    # a flat band of light around the post
    lathe(ring, [(9, 11), (9, 13), (10.5, 13), (10.5, 11)], 24)
    write_md3(os.path.join(out, "models/abilities/totem/totem.md3"), "totem", [post, ring])
    texture(os.path.join(out, "models/abilities/totem/totem.tga"), 64, 64,
            lambda u, v, n=Fbm(53, 4, 6): [c * (0.75 + 0.35 * n.at(u * 3, v * 6)) for c in (0.78, 0.72, 0.6)] + [1], False)


def turret(out):
    m = Mesh("ability/turret")
    # three legs
    for k in range(3):
        start = len(m.xyz)
        box(m, (0, -1, 0), (12, 1, 2))
        transform(m, start, lambda p, a=k * 2 * math.pi / 3 + math.pi / 3:
                  rotate_z((p[0], p[1], p[2] + (12 - p[0]) * 0.9), a))
    lathe(m, [(0, 10), (3, 10), (3, 14), (0, 14)], 8)        # the mast
    box(m, (-6, -5, 14), (6, 5, 22))                          # the head
    start = len(m.xyz)                                        # the barrel, laid along +x
    lathe(m, [(0, 0), (1.6, 0), (1.6, 18), (2.2, 18), (2.2, 20), (0, 20)], 8)
    transform(m, start, lambda p: (p[2] + 4, p[1], 18 - p[0]))
    write_md3(os.path.join(out, "models/abilities/turret/turret.md3"), "turret", [m])
    texture(os.path.join(out, "models/abilities/turret/turret.tga"), 64, 64,
            lambda u, v, n=Fbm(61, 4, 8): [0.42 * (0.75 + 0.4 * n.at(u * 4, v * 4))] * 3 + [1], False)


def barrier(out):
    m = Mesh("ability/barrier")
    # a capsule of eight facets around a standing player (z from the player's
    # origin: feet at -24, head near 32)
    lathe(m, [(0.1, -27), (20, -23), (26, -10), (27, 8), (24, 24), (14, 33), (0.1, 36)], 8)
    write_md3(os.path.join(out, "models/abilities/barrier/barrier.md3"), "barrier", [m])
    texture(os.path.join(out, "gfx/abilities/barrier.tga"), 64, 64,
            lambda u, v: [0.25, 0.6, 1.0, 1] if min(u % 0.125, v % 0.166) < 0.012 else [0.05, 0.15, 0.3, 1], False)


def acid(out):
    def ball(u, v, n=Fbm(71, 4, 6)):
        d = math.hypot(u - 0.5, v - 0.5) * 2
        a = max(0.0, 1 - d) ** 1.5 * (0.8 + 0.4 * n.at(u * 4, v * 4))
        return [0.45 * a, 1.0 * a, 0.2 * a, min(1.0, a)]

    def mark(u, v, n=Fbm(73, 5, 5)):
        d = math.hypot(u - 0.5, v - 0.5) * 2 + 0.35 * (n.at(u * 3, v * 3) - 0.5)
        a = 1.0 if d < 0.7 else max(0.0, 1 - (d - 0.7) / 0.25)
        return [0.25, 0.6, 0.12, a * 0.85]

    texture(os.path.join(out, "gfx/abilities/acid_ball.tga"), 64, 64, ball)
    texture(os.path.join(out, "gfx/abilities/acid_mark.tga"), 128, 128, mark)


def weapon_icons(out):
    # this file's box() builds meshes; the 2D one comes from tremorart
    gold = (0.95, 0.72, 0.2)
    dark = (0.35, 0.22, 0.05)
    # heavy machinegun: a body, three stacked barrels, a grip
    hmg = union(art.box(0.38, 0.5, 0.2, 0.12, 0.02), art.box(0.72, 0.42, 0.22, 0.025), art.box(0.72, 0.5, 0.22, 0.025),
                art.box(0.72, 0.58, 0.22, 0.025), polygon([(0.24, 0.6), (0.34, 0.6), (0.3, 0.82), (0.2, 0.82)]))
    # super shotgun: two fat barrels side by side and a stock
    ssg = union(art.box(0.6, 0.44, 0.32, 0.05, 0.02), art.box(0.6, 0.56, 0.32, 0.05, 0.02),
                polygon([(0.06, 0.5), (0.3, 0.42), (0.3, 0.64), (0.1, 0.72)]))
    # super nailgun: a round drum, a short body and a spiked muzzle
    sng = union(circle(0.4, 0.5, 0.2), art.box(0.68, 0.5, 0.18, 0.08, 0.02),
                polygon([(0.86, 0.38), (0.96, 0.5), (0.86, 0.62)]), art.box(0.3, 0.76, 0.04, 0.1))
    # tri-bolt: a launcher body and three bolts fanning out
    tribolt = union(art.box(0.3, 0.55, 0.2, 0.1, 0.02), polygon([(0.12, 0.62), (0.22, 0.62), (0.2, 0.84), (0.1, 0.84)]),
                    *[polygon([(0.52, 0.55 + d * 0.06), (0.82, 0.5 + d * 0.2), (0.92, 0.5 + d * 0.22), (0.82, 0.56 + d * 0.2)])
                      for d in (-1, 0, 1)])
    for name, shape in (("hmg", hmg), ("ssg", ssg), ("sng", sng), ("tribolt", tribolt)):
        c = Canvas(64, 64)
        c.fill(shape.outline(0.06), INK)
        c.fill(shape, gold)
        if name == "sng":
            c.fill(circle(0.4, 0.5, 0.08), dark)
        write_tga(os.path.join(out, "icons/iconw_%s.tga" % name), 64, 64, c.px)


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


# ---------------------------------------------------------------- sounds

def write_wav(path, samples, rate=22050):
    """16-bit mono PCM; samples in -1..1."""
    data = b"".join(struct.pack("<h", int(max(-1.0, min(1.0, v)) * 32000)) for v in samples)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(b"RIFF" + struct.pack("<I", 36 + len(data)) + b"WAVEfmt ")
        f.write(struct.pack("<IHHIIHH", 16, 1, 1, rate, rate * 2, 2, 16))
        f.write(b"data" + struct.pack("<I", len(data)) + data)


def tone(freqs, seconds, decay, noise=0.0, rate=22050, seed=5):
    rnd = random.Random(seed)
    out = []
    for i in range(int(seconds * rate)):
        t = i / rate
        env = math.exp(-t * decay) * min(1.0, t * 400)
        v = sum(a * math.sin(2 * math.pi * f * t) for f, a in freqs)
        v += noise * (rnd.random() * 2 - 1)
        out.append(v * env * 0.6)
    return out


def sweep(f0, f1, seconds, decay, noise=0.12, rate=22050, seed=5):
    """A tone gliding from f0 to f1 with a breath of noise."""
    rnd = random.Random(seed)
    out, phase = [], 0.0
    for i in range(int(seconds * rate)):
        t = i / rate
        f = f0 + (f1 - f0) * t / seconds
        phase += 2 * math.pi * f / rate
        env = math.exp(-t * decay) * min(1.0, t * 300)
        v = 0.7 * math.sin(phase) + 0.25 * math.sin(phase * 2.01) + noise * (rnd.random() * 2 - 1)
        out.append(v * env * 0.55)
    return out


def scrape(seconds, cutoff, decay, rate=22050, seed=5, loop=False):
    """Low-passed noise: a body sliding on the floor. A loop fades in and
    out over its ends so it repeats without a click."""
    rnd = random.Random(seed)
    out, lp = [], 0.0
    k = 1.0 - math.exp(-2 * math.pi * cutoff / rate)
    n = int(seconds * rate)
    for i in range(n):
        t = i / rate
        lp += k * ((rnd.random() * 2 - 1) - lp)
        if loop:
            env = min(1.0, i / (0.05 * rate), (n - i) / (0.05 * rate))
        else:
            env = math.exp(-t * decay) * min(1.0, t * 200)
        out.append(lp * env * 1.6)
    return out


def whoosh(seconds, rate=22050, seed=5):
    """Noise through a band that sweeps up and back down, swelling and fading."""
    rnd = random.Random(seed)
    out, lp = [], 0.0
    n = int(seconds * rate)
    for i in range(n):
        t = i / n
        cutoff = 500 + 2500 * math.sin(math.pi * t)
        k = 1.0 - math.exp(-2 * math.pi * cutoff / rate)
        lp += k * ((rnd.random() * 2 - 1) - lp)
        env = math.sin(math.pi * t) ** 1.5
        out.append(lp * env * 1.8)
    return out


# the active abilities' names (bg_abilityInfo's cvarName)
ABILITIES = ["hook", "bullrush", "injection", "dualwield", "barrier", "flamestrike", "berserk", "turret",
             "totem", "grenades", "ghostwalk", "orb", "plasmatrail", "acidspit", "drone", "piercingsight",
             "shellshock", "spinevolley", "masterofdeath", "supplydrop", "markprey", "downpour", "hound",
             "riftanchor"]


def sounds(out):
    write_wav(os.path.join(out, "sound/tremor/hit_armor.wav"), tone([(2400, 0.6), (3600, 0.3), (5100, 0.15)], 0.12, 38))
    write_wav(os.path.join(out, "sound/tremor/hit_health.wav"), tone([(520, 0.7), (780, 0.25)], 0.12, 45, noise=0.15))
    write_wav(os.path.join(out, "sound/tremor/armor_break.wav"), tone([(1800, 0.4), (2700, 0.3), (900, 0.3)], 0.35, 14, noise=0.5))
    # a bell (fundamental and inharmonic partials) on A, C sharp and E, 0.16 s apart
    call = [0.0] * int(1.2 * 22050)
    for n, f in enumerate((880, 1109, 1319)):
        bell = tone([(f, 0.6), (f * 2.76, 0.2), (f * 5.4, 0.08)], 0.8, 6)
        start = int(n * 0.16 * 22050)
        for i, v in enumerate(bell):
            call[start + i] += v * 0.6
    write_wav(os.path.join(out, "sound/tremor/match_found.wav"), call)
    # the crouch slide: a scrape starting, a steady loop, a short stop
    write_wav(os.path.join(out, "sound/tremor/slide_start.wav"), scrape(0.25, 600, 9, seed=41))
    write_wav(os.path.join(out, "sound/tremor/slide_loop.wav"), scrape(1.0, 420, 0, seed=43, loop=True))
    write_wav(os.path.join(out, "sound/tremor/slide_stop.wav"), scrape(0.18, 300, 22, seed=47))
    # Clutch's dodge: a short airy whoosh
    write_wav(os.path.join(out, "sound/tremor/dodge.wav"), whoosh(0.28, seed=53))
    # Athena's hook biting: a short metallic clank
    write_wav(os.path.join(out, "sound/tremor/hook_attach.wav"), tone([(1400, 0.5), (2300, 0.35), (3700, 0.2)], 0.22, 22, noise=0.35, seed=59))
    # each ability: a rising sweep on casting, a falling blip at the end,
    # pitched by the name so they differ
    for name in ABILITIES:
        base = 300 + (sum(ord(c) * (i + 1) for i, c in enumerate(name)) % 500)
        write_wav(os.path.join(out, "sound/abilities/%s_cast.wav" % name), sweep(base, base * 2.2, 0.32, 9, seed=len(name)))
        write_wav(os.path.join(out, "sound/abilities/%s_end.wav" % name), sweep(base * 1.8, base * 0.9, 0.18, 18, seed=len(name) + 1))


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

ability/totem
{
	{
		map models/abilities/totem/totem.tga
		rgbGen lightingDiffuse
	}
}

ability/totemring
{
	cull none
	sort additive
	{
		map $whiteimage
		blendFunc GL_ONE GL_ONE
		rgbGen wave sin 0.5 0.25 0 1.2
	}
}

ability/turret
{
	{
		map models/abilities/turret/turret.tga
		rgbGen lightingDiffuse
	}
}

ability/barrier
{
	cull none
	sort additive
	{
		map gfx/abilities/barrier.tga
		blendFunc GL_ONE GL_ONE
		tcMod scroll 0 0.2
		rgbGen wave sin 0.6 0.15 0 1.5
	}
}

ability/acid_ball
{
	nopicmip
	cull none
	{
		clampmap gfx/abilities/acid_ball.tga
		blendFunc GL_ONE GL_ONE
		rgbGen entity
	}
}

ability/acid_mark
{
	polygonOffset
	{
		clampmap gfx/abilities/acid_mark.tga
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen exactVertex
		alphaGen vertex
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

// Strogg's body while he flies the drone: a faint shimmer, its strength
// from the entity's alpha (cgame)
tremor/droneshimmer
{
	deformVertexes wave 30 sin 0 0.6 0 0.4
	{
		map $whiteimage
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen entity
		alphaGen entity
	}
	{
		map gfx/abilities/plasmatrail.tga
		blendFunc GL_SRC_ALPHA GL_ONE
		tcGen environment
		tcMod scroll 0.05 0.1
		rgbGen entity
		alphaGen entity
	}
}
"""


def main():
    out = sys.argv[1]
    src = os.path.join(out, "assets-src")
    hourglass(src)
    orb(src)
    claw(src)
    totem(src)
    turret(src)
    barrier(src)
    acid(src)
    os.makedirs(os.path.join(src, "icons"), exist_ok=True)
    weapon_icons(src)
    portraits(src)
    sounds(src)
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
