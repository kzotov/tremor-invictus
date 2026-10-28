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
  gfx/champions/<champion>.tga, _small.tga   real portraits (256 and 64 px) for the
                                            champions that have none elsewhere:
                                            Athena, Beastmaster, Galena,
                                            Scalebearer, Unumrey, Wrack and Xaero
                                            (portraits.py)
  sound/tremor/hit_armor.wav, hit_health.wav, armor_break.wav
  sound/tremor/slide_start.wav, slide_loop.wav, slide_stop.wav   the crouch slide
  sound/tremor/dodge.wav                    Clutch's dodge
  sound/tremor/hook_attach.wav              Athena's hook biting
  sound/tremor/hitbeep1.wav .. hitbeep4.wav the hit beeps by damage (<= 25, 26-49,
                                            50-74, >= 75), pitch falling as it rises
  sound/tremor/killbeep.wav                 the kill beep: a short bell
  sound/tremor/tribolt_exp.wav              a Tri-bolt bolt bursting: a short
                                            crack over a low thump, lighter
                                            than a rocket
                                            hit tones: a bright ping on armour, a
                                            dull knock on health, a crack when the
                                            armour breaks (synthesised)
  sound/tremor/match_found.wav              the ready check's call: three rising
                                            bell tones
  models/abilities/totem/totem.md3          the Unholy Totem: an iron "m" on a
                                            stake (two outer blades and a centre
                                            stake under a top bar); its groove,
                                            core and cracks in the entity colour
  gfx/abilities/totem_aura.tga, totem_beam.tga
                                            its floor aura and the light over it
  models/abilities/turret/turret.md3        the sentry: a tripod, a turning head
                                            and a barrel along +x
  models/abilities/drone/drone.md3          Strogg's Peeker: a small sphere with a red eye
                                            and four metal rods trailing behind it
  sound/abilities/turret_*.wav, drone_shot.wav
                                            the turret's deploy, online, target,
                                            shot, idle hum and destroyed; the
                                            drone gun's zap
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
The champions' looks over models the player has (skins.py, looks/) are
not made here: bake-looks.py paints them on the player's machine.
All shapes are built here from lathes and boxes; textures from noise.
"""
import math, os, random, shutil, struct, sys, zipfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tremorart import *
import tremorart as art
import portraits as champion_portraits


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
    for m in meshes:
        assert len(m.xyz) <= 1000, "%s: %i vertices, the engine takes 1000 a surface" % (m.shader, len(m.xyz))
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


def thrust(u, v, n=Fbm(61, 4, 6)):
    """Slash's skate thrust (the jet under each skate, drawn facing the
    viewer): a white-hot core fading through green to nothing, its edge
    ragged with noise, black outside for an additive blend."""
    x, y = u * 2 - 1, v * 2 - 1
    r = math.hypot(x, y)
    d = r * (0.85 + 0.3 * n.at(u * 4, v * 4))
    core = math.exp(-(d / 0.16) ** 2)
    glow = math.exp(-(d / 0.4) ** 2) * (1 - smooth(0.55, 0.85, r))       # nothing at the edges
    return [clamp(0.15 * glow + 0.9 * core), clamp(0.85 * glow + core), clamp(0.3 * glow + 0.8 * core), 1]


def thrust_flicker(u, v, n=Fbm(67, 4, 8)):
    """Streaks round the jet's middle, turned and flickered by the shader so
    the thrust boils."""
    x, y = u * 2 - 1, v * 2 - 1
    d, a = math.hypot(x, y), math.atan2(y, x)
    streak = smooth(0.35, 0.9, n.at(a * 1.6 + 7, d * 3)) * math.exp(-(d / 0.5) ** 2) * (1 - smooth(0.6, 0.85, d))
    return [0.1 * streak, 0.75 * streak, 0.3 * streak, 1]


def blood_edge(u, v, n=Fbm(47, 5, 5), m=Fbm(53, 3, 14)):
    """hud.md 9.7, the low-health overlay: dark red at the screen edges in
    blotches, clear in the middle (ours, drawn for the purpose)."""
    edge = min(u, 1 - u, v, 1 - v)                     # 0 at an edge, 0.5 in the middle
    reach = 0.1 + 0.16 * n.at(u, v)                    # how far in the stain reaches here
    k = max(0.0, 1.0 - edge / reach)
    k = k * k * (3 - 2 * k)
    k *= 0.75 + 0.25 * m.at(u, v)                      # small mottling
    shade = 0.32 + 0.25 * m.at(v, u)
    return [shade, 0.01, 0.02, min(1.0, k * 1.15)]


def drone_blast(u, v, n=Fbm(61, 4, 6)):
    """The drone's charge blast: a blue-white core in a ragged electric ring."""
    dx, dy = u - 0.5, v - 0.5
    r = math.sqrt(dx * dx + dy * dy) * 2
    a = math.atan2(dy, dx)
    rag = 0.12 * n.at(math.cos(a) * 3 + 3, math.sin(a) * 3 + 3)
    core = max(0.0, 1 - r / 0.45) ** 1.5
    ring = max(0.0, 1 - abs(r - 0.7 - rag) / 0.12)
    k = min(1.0, core + 0.8 * ring)
    return [0.55 + 0.45 * core, 0.75 + 0.25 * core, 1.0, k]


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


def prism(mesh, outline, y0, y1):
    """A convex outline of (x, z) points, clockwise seen from -y, pushed from
    y0 to y1: two faces and the sides between them."""
    n = len(outline)
    for y, sign in ((y0, -1), (y1, 1)):
        idx = [mesh.vert((x, y, z), (0, sign, 0), (0.5 + x / 64, 0.5 - z / 64)) for x, z in outline]
        for k in range(1, n - 1):
            if sign < 0:
                mesh.tri(idx[0], idx[k], idx[k + 1])
            else:
                mesh.tri(idx[0], idx[k + 1], idx[k])
    for k in range(n):
        (ax, az), (bx, bz) = outline[k], outline[(k + 1) % n]
        nx, nz = bz - az, -(bx - ax)
        length = math.hypot(nx, nz) or 1.0
        nrm = (-nx / length, 0, -nz / length)
        q = [mesh.vert((ax, y0, az), nrm, (0, 0)), mesh.vert((bx, y0, bz), nrm, (1, 0)),
             mesh.vert((bx, y1, bz), nrm, (1, 1)), mesh.vert((ax, y1, az), nrm, (0, 1))]
        mesh.quad(*q)


# the totem's skin: the front view in units, x from TOTEM_SKIN_X0, z down
# from TOTEM_SKIN_Z0, TOTEM_SKIN_SPAN units across both ways; the trim strip
# for the edges at x TOTEM_SKIN_EDGE .. + 2.6
TOTEM_SKIN_X0, TOTEM_SKIN_Z0, TOTEM_SKIN_SPAN, TOTEM_SKIN_EDGE = -20.0, 60.0, 72.0, 20.5
TOTEM_LIGHT = (-0.55, -0.75)            # towards the light, in the picture (x right, y down)
IRON = (0.17, 0.165, 0.17)
BRONZE = (0.55, 0.36, 0.17)


def totem_skin():
    """Paint the totem's skin (512 x 512) and its glow map in the same layout.
    Blackened iron plates with aged bronze trims: every part bevelled, lit
    from the upper left with a bright catch on its rim and a dark lower right
    edge, an inset line round each plate, rivets, engraved runes down the
    blades, a groove down the stake, rings round the hub, scratches, grime
    towards the floor and the shadows the parts in front cast on those
    behind."""
    W = 512
    span = TOTEM_SKIN_SPAN
    c = Canvas(W, W, (0.05, 0.05, 0.05, 1), span=span)
    glow = Canvas(W, W, (0, 0, 0, 1), span=span)
    grain = Fbm(101, 4, 10)
    fine = Noise(103, 256, 256)
    rng = random.Random(107)

    def pt(x, z):
        return (x - TOTEM_SKIN_X0, TOTEM_SKIN_Z0 - z)

    def poly(pts):
        return polygon([pt(x, z) for x, z in pts])

    def mirror(pts):
        return [(-x, z) for x, z in reversed(pts)]

    def material(base, x, y, wear=1.0):
        n = grain.at(x / span * 6, y / span * 6)
        f = fine.at(x * 9, y * 9) - 0.5
        k = 0.82 + 0.36 * n + 0.10 * f
        z = TOTEM_SKIN_Z0 - y
        k *= 1 - 0.35 * wear * smooth(30, 0, z) * (0.6 + 0.4 * grain.at(x * 0.3, y * 0.3))   # grime low down
        return tuple(ch * k for ch in base)

    def bevelled(shape, base, bevel=0.7, wear=1.0):
        """Fill with a bevel: the rim facing the light bright, the far rim
        dark, a thin catch of light on the very edge."""
        sdf = shape.sdf
        e = 0.05
        lx, ly = TOTEM_LIGHT
        ln = math.hypot(lx, ly)
        lx, ly = lx / ln, ly / ln

        def f(x, y):
            d = -sdf(x, y)
            col = material(base, x, y, wear)
            if d < bevel:
                gx = sdf(x + e, y) - sdf(x - e, y)
                gy = sdf(x, y + e) - sdf(x, y - e)
                gl = math.hypot(gx, gy) or 1.0
                facing = -(gx * lx + gy * ly) / gl        # +1: the rim faces the light
                t = 1 - max(0.0, d) / bevel
                k = 1 + t * (0.9 * facing if facing > 0 else 0.75 * facing)
                if d < 0.14:
                    k += 0.5 * max(0.0, facing) + 0.15        # the catch on the edge
                col = tuple(ch * max(0.15, k) for ch in col)
            return col
        c.fill(shape, f)

    def shadow(shape, depth=0.9):
        """The shade a part casts on what lies behind it, away from the light."""
        lx, ly = TOTEM_LIGHT
        c.fill(shape.moved(-lx * depth, -ly * depth).grow(0.2), (0, 0, 0), 0.55, soft=0.9)

    def inset(shape, k=0.9):
        line_ = shape.grow(-k).outline(0.16)
        c.fill(line_, (0.03, 0.03, 0.03), 0.8)
        c.fill(line_.moved(0.12, 0.12), (1, 1, 1), 0.12)

    def rivet(x, z, r=0.45):
        cx, cy = pt(x, z)
        c.fill(circle(cx + 0.15, cy + 0.2, r * 1.1), (0, 0, 0), 0.6, soft=0.2)
        bevelled(circle(cx, cy, r), scale(BRONZE, 1.3), bevel=r)
        c.fill(circle(cx - r * 0.35, cy - r * 0.35, r * 0.28), (1, 0.9, 0.7), 0.55)

    def scratches(shape, n):
        x0, y0, x1, y1 = shape.box
        for _ in range(n):
            x, y = rng.uniform(x0, x1), rng.uniform(y0, y1)
            a = rng.uniform(0, math.pi)
            l = rng.uniform(0.4, 1.6)
            st = line(x, y, x + math.cos(a) * l, y + math.sin(a) * l, 0.06) & shape.grow(-0.4)
            c.fill(st, (0.9, 0.85, 0.8), 0.35)

    def rune_column(x, z0, z1, seed):
        """Engraved signs down a plate: two to four strokes each on a 2 x 3 grid."""
        r = random.Random(seed)
        out = None
        z = z1
        while z - 3.2 > z0:
            g = [(x + gx * 0.9, z - gz * 1.1) for gz in range(3) for gx in (-1, 0, 1)]
            pts = r.sample(g, 4)
            sign = None
            for k in range(r.randint(2, 3)):
                (ax, az), (bx, bz) = pts[k], pts[k + 1]
                st = line(*pt(ax, az), *pt(bx, bz), 0.3)
                sign = st if sign is None else sign | st
            out = sign if out is None else out | sign
            z -= 3.6
        return out

    # the parts, from the back to the front
    brace = [(-13, 46), (-11, 46), (-4.5, 39), (-4.5, 37)]
    blade = [(-18.5, 48), (-11.5, 46), (-11.5, 18), (-17.5, 11)]
    bar = [(-17, 50), (17, 50), (16, 46), (-16, 46)]
    crest1 = [(-6, 53), (6, 53), (6, 50), (-6, 50)]
    crest2 = [(-3.5, 56), (3.5, 56), (3.5, 53), (-3.5, 53)]
    stake = [(-2.6, 46), (2.6, 46), (2.6, 2), (-2.6, 2)]
    tip = [(-2.6, 2), (2.6, 2), (2.6, -3), (0.4, -10), (-2.6, -4)]
    hub = [(-4.5, 44), (4.5, 44), (4.5, 36), (-4.5, 36)]
    bands = [[(-3.4, z1), (3.4, z1), (3.4, z0), (-3.4, z0)] for z0, z1 in ((30, 32), (17, 18.5), (2, 4.5))]

    for b in (brace, mirror(brace)):
        bevelled(poly(b), scale(BRONZE, 0.75), 0.5)
    bevelled(poly(crest2), BRONZE, 0.6)
    for x in (-1.6, 0, 1.6):
        c.fill(line(*pt(x, 55.4), *pt(x, 53.6), 0.22), (0.05, 0.03, 0.02), 0.8)
    for b, seed in ((blade, 1), (mirror(blade), 2)):
        sh = poly(b)
        bevelled(sh, IRON, 0.9)
        inset(sh, 1.0)
        bx = sum(p[0] for p in b) / 4
        runes = rune_column(bx, 20, 42, seed)
        c.fill(runes.grow(0.08), (0.02, 0.02, 0.02), 0.9)
        c.fill(runes.moved(0.1, 0.1), (1, 1, 1), 0.08)
        glow.fill(runes.grow(-0.02), (1, 1, 1))
        glow.glow(runes, (0.35, 0.35, 0.35), 0.4, 0.6)
        scratches(sh, 26)
        rivet(bx, 44.2)
        rivet(bx + (1.3 if bx < 0 else -1.3), 16.5 if bx < 0 else 16.5, 0.4)
    shadow(poly(crest1))
    bevelled(poly(crest1), BRONZE, 0.7)
    inset(poly(crest1), 0.6)
    shadow(poly(bar))
    bar_s = poly(bar)
    bevelled(bar_s, IRON, 0.9)
    inset(bar_s, 0.8)
    scratches(bar_s, 18)
    for x in (-14.5, -9, 9, 14.5):
        rivet(x, 48)
    shadow(poly(stake))
    st_s = poly(stake)
    bevelled(st_s, IRON, 0.8)
    groove = art.box(*pt(0, 17.5), 0.75, 11.5)
    c.fill(groove.grow(0.15), (0.01, 0.01, 0.01), 0.95)
    glow.fill(groove.grow(-0.15), (1, 1, 1))
    glow.glow(groove, (0.4, 0.4, 0.4), 0.6, 0.7)
    scratches(st_s, 10)
    shadow(poly(tip), 0.6)
    bevelled(poly(tip), scale(IRON, 0.9), 0.7)
    c.fill(poly([(-1.4, 1), (1.4, 1), (0.2, -7)]).outline(0.12), (0.02, 0.02, 0.02), 0.7)
    for b in bands:
        shadow(poly(b), 0.6)
        bevelled(poly(b), BRONZE, 0.5)
        z = (b[0][1] + b[2][1]) / 2
        rivet(-2.4, z, 0.38)
        rivet(2.4, z, 0.38)
    shadow(poly(hub))
    hub_s = poly(hub)
    bevelled(hub_s, BRONZE, 0.8)
    hx, hy = pt(0, 40)
    for r, w in ((3.4, 0.35), (2.9, 0.14)):
        c.fill(ring(hx, hy, r, w), (0.03, 0.02, 0.01), 0.85)
    glow.fill(ring(hx, hy, 3.4, 0.18), (0.8, 0.8, 0.8))
    for a in range(0, 360, 45):
        ax, ay = hx + 3.9 * math.cos(math.radians(a)), hy + 3.9 * math.sin(math.radians(a))
        if abs(ax - hx) < 4.1 and abs(ay - hy) < 3.6:
            c.fill(circle(ax, ay, 0.22), (1.0, 0.8, 0.55), 0.6)

    # the trim strip the side faces use: bronze, a bright ridge along it
    sx0, _ = pt(TOTEM_SKIN_EDGE, 0)

    def edge(x, y):
        t = (x - sx0) / 2.6
        k = 0.55 + 0.75 * math.exp(-((t - 0.5) / 0.22) ** 2) - 0.25 * abs(t - 0.5)
        return material(scale(IRON, 1.6), x, y, 0.6) if 0 <= t <= 1 else (0, 0, 0)
    c.fill(art.box(sx0 + 1.3, span / 2, 1.3, span / 2), lambda x, y: tuple(
        ch * (0.55 + 0.75 * math.exp(-(((x - sx0) / 2.6 - 0.5) / 0.22) ** 2)) for ch in edge(x, y)))
    return c, glow


TOTEM_SCALE = 0.45


def totem(out):
    """Galena's Unholy Totem: a tall iron sign like a letter "m" set on a post.
    A banded centre stake, its blade point driven into the floor, hangs from a
    top bar; two broad flat blades hang from the bar's ends, cut at a slant
    at the bottom; braces run from the bar to a hub on the stake, and a
    stepped crest sits on top. A burning groove runs down the stake and a core
    burns in the hub. Built 56 units tall from the floor (z = -2: the game
    places the entity 2 units off the surface it hit) and written at
    TOTEM_SCALE, about 25 units. The groove, the core and the
    iron's cracks take the entity colour: the code paints them in the owner's
    side's colour."""
    iron, core = Mesh("ability/totem"), Mesh("ability/totemcore")
    # the top bar and its stepped crest
    prism(iron, [(-17, 50), (17, 50), (16, 46), (-16, 46)], -2.4, 2.4)
    prism(iron, [(-6, 53), (6, 53), (6, 50), (-6, 50)], -2.0, 2.0)
    prism(iron, [(-3.5, 56), (3.5, 56), (3.5, 53), (-3.5, 53)], -1.6, 1.6)
    # the two outer blades, flaring at the top and cut at a slant at the bottom,
    # the outer edge the longer
    for side in (-1, 1):
        start = len(iron.xyz)
        prism(iron, [(-18.5, 48), (-11.5, 46), (-11.5, 18), (-17.5, 11)], -1.8, 1.8)
        if side > 0:
            transform(iron, start, lambda p: (-p[0], -p[1], p[2]))
    # the centre stake, its bands and its blade point in the floor
    prism(iron, [(-2.6, 46), (2.6, 46), (2.6, 2), (-2.6, 2)], -2.6, 2.6)
    for z0, z1 in ((30, 32), (17, 18.5), (2, 4.5)):
        prism(iron, [(-3.4, z1), (3.4, z1), (3.4, z0), (-3.4, z0)], -3.4, 3.4)
    prism(iron, [(-2.6, 2), (2.6, 2), (2.6, -3), (0.4, -10), (-2.6, -4)], -2.2, 2.2)
    # the hub under the bar and the braces from the bar's ends down to it
    prism(iron, [(-4.5, 44), (4.5, 44), (4.5, 36), (-4.5, 36)], -3.2, 3.2)
    for side in (-1, 1):
        start = len(iron.xyz)
        prism(iron, [(-13, 46), (-11, 46), (-4.5, 39), (-4.5, 37)], -1.2, 1.2)
        if side > 0:
            transform(iron, start, lambda p: (-p[0], -p[1], p[2]))
    # the burning groove on both faces of the stake, and the core in the hub
    for y in (-2.75, 2.75):
        prism(core, [(-0.7, 29), (0.7, 29), (0.7, 6), (-0.7, 6)], y - 0.2, y + 0.2)
    lathe(core, [(2.6 * math.sin(math.pi * k / 8), 40 - 2.6 * math.cos(math.pi * k / 8)) for k in range(9)], 12,
          (0, 0, 0))
    start = len(core.xyz)
    lathe(core, [(0.05, 0), (2.4, 0.4), (2.4, 7.2), (0.05, 7.6)], 12)
    transform(core, start, lambda p: (p[0], p[2] - 3.8, p[1] + 40))     # a lens through the hub, face to face
    # the skin is painted as the totem seen from the front (totem_skin below):
    # faces looking along y take their place in it, the edges a strip of trim
    # at its right
    for i, (p, n) in enumerate(zip(iron.xyz, iron.normal)):
        if abs(n[1]) > 0.5:
            iron.st[i] = ((p[0] - TOTEM_SKIN_X0) / TOTEM_SKIN_SPAN, (TOTEM_SKIN_Z0 - p[2]) / TOTEM_SKIN_SPAN)
        else:
            t = min(1.0, max(0.0, (p[1] + 3.4) / 6.8))
            iron.st[i] = ((TOTEM_SKIN_EDGE + 2.6 * t - TOTEM_SKIN_X0) / TOTEM_SKIN_SPAN, (TOTEM_SKIN_Z0 - p[2]) / TOTEM_SKIN_SPAN)
    # built 56 units tall, made TOTEM_SCALE of that: up to Galena's hip
    # (Konstantin), standing on the same floor
    for m in (iron, core):
        m.xyz = [(x * TOTEM_SCALE, y * TOTEM_SCALE, -2 + (z + 2) * TOTEM_SCALE) for x, y, z in m.xyz]
    write_md3(os.path.join(out, "models/abilities/totem/totem.md3"), "totem", [iron, core])

    def ember(u, v, n=Fbm(71, 4, 4)):
        g = 0.45 + 0.55 * n.at(u * 3, v * 3)
        return [g, g, g, 1]

    skin, glow = totem_skin()
    skin.write(os.path.join(out, "models/abilities/totem/totem.tga"), alpha=False)
    glow.write(os.path.join(out, "models/abilities/totem/totem_cracks.tga"), alpha=False)
    texture(os.path.join(out, "models/abilities/totem/totem_core.tga"), 64, 64, ember, False)


def totem_aura(out):
    """The ground aura under a standing totem (gfx/abilities/totem_aura.tga): a
    disc of faint light out to its heal radius with a brighter rim and a ring
    of small marks, white for the code to tint; and a soft column of light
    (totem_beam.tga) that rises from the core."""
    def aura(u, v, n=Fbm(73, 3, 6)):
        x, y = u * 2 - 1, v * 2 - 1
        d = math.hypot(x, y)
        a = math.atan2(y, x)
        rim = math.exp(-((d - 0.94) / 0.025) ** 2)
        inner = 0.10 * smooth(0.2, 0.93, d) * (d < 0.95)
        marks = 0.0
        if 0.78 < d < 0.86:
            t = (a / (2 * math.pi) * 18) % 1
            marks = 0.55 * (0.3 < t < 0.7) * math.exp(-((d - 0.82) / 0.025) ** 2)
        g = min(1.0, rim + inner * (0.7 + 0.6 * n.at(u * 4, v * 4)) + marks)
        return [g, g, g, 1]

    def beam(u, v):
        g = math.exp(-((u - 0.5) / 0.18) ** 2) * (1 - v) ** 1.5 * smooth(0.0, 0.08, v)
        return [g, g, g, 1]
    texture(os.path.join(out, "gfx/abilities/totem_aura.tga"), 256, 256, aura, False)
    texture(os.path.join(out, "gfx/abilities/totem_beam.tga"), 32, 128, beam, False)


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


def rod(mesh, root, direction, length, r0, r1, sweep, segments=8, rings=12):
    """A thin tapered pole from root along direction, its radius falling from
    r0 to r1, bending by sweep (a vector) times the square of the way along:
    u runs round it, v from the root (0) to the tip (1)."""
    d = direction
    k = math.sqrt(sum(c * c for c in d))
    d = tuple(c / k for c in d)
    # two axes across the pole
    a = (0.0, 0.0, 1.0) if abs(d[2]) < 0.9 else (1.0, 0.0, 0.0)
    e1 = (a[1] * d[2] - a[2] * d[1], a[2] * d[0] - a[0] * d[2], a[0] * d[1] - a[1] * d[0])
    k = math.sqrt(sum(c * c for c in e1))
    e1 = tuple(c / k for c in e1)
    e2 = (d[1] * e1[2] - d[2] * e1[1], d[2] * e1[0] - d[0] * e1[2], d[0] * e1[1] - d[1] * e1[0])
    first = len(mesh.xyz)
    for i in range(rings + 1):
        t = i / rings
        r = r0 + (r1 - r0) * t
        c = tuple(root[j] + d[j] * length * t + sweep[j] * t * t for j in range(3))
        for s in range(segments + 1):
            ang = 2 * math.pi * s / segments
            n = tuple(e1[j] * math.cos(ang) + e2[j] * math.sin(ang) for j in range(3))
            mesh.vert(tuple(c[j] + n[j] * r for j in range(3)), n, (s / segments, t))
    for i in range(rings):
        for s in range(segments):
            p = first + i * (segments + 1) + s
            q = p + segments + 1
            mesh.quad(p, p + 1, q + 1, q)
    # a small bead closing the tip
    tip = tuple(root[j] + d[j] * length + sweep[j] for j in range(3))
    bead = len(mesh.xyz)
    lat = 4
    for i in range(lat + 1):
        th = math.pi * i / lat
        for s in range(segments + 1):
            ang = 2 * math.pi * s / segments
            n = tuple(d[j] * math.cos(th) + (e1[j] * math.cos(ang) + e2[j] * math.sin(ang)) * math.sin(th)
                      for j in range(3))
            mesh.vert(tuple(tip[j] + n[j] * r1 * 1.9 for j in range(3)), n, (s / segments, 0.97))
    for i in range(lat):
        for s in range(segments):
            p = bead + i * (segments + 1) + s
            q = p + segments + 1
            mesh.quad(p, q, q + 1, p + 1)


def drone(out):
    """Strogg's Peeker: a small sphere (radius 10, the drone's box) of dark
    metal with a lit band and a red eye facing forward (+x), and four thin
    rods streaming out behind it from a collar on its back, splayed a little
    and bending upward like a comet's tail."""
    m = Mesh("ability/drone")
    lathe(m, [(10 * math.sin(math.pi * k / 12), -10 * math.cos(math.pi * k / 12)) for k in range(13)], 20)
    rods = Mesh("ability/drone_rods")
    # the collar, a short sleeve round the back (-x)
    start = len(rods.xyz)
    lathe(rods, [(0.1, 0), (3.6, 0), (4.2, 0.8), (4.2, 3.2), (3.2, 4.2), (2.4, 4.2), (0.1, 4.2)], 16)
    for i in range(start, len(rods.xyz)):
        x, y, z = rods.xyz[i]
        nx, ny, nz = rods.normal[i]
        rods.xyz[i] = (-8.6 - z, y, x)
        rods.normal[i] = (-nz, ny, nx)
    # the rods, long to short, seen from behind as an X
    for az, length, lift in ((45, 40, 7), (135, 34, 6), (225, 30, 4), (315, 26, 4)):
        a = math.radians(az)
        cy, cz = math.cos(a), math.sin(a)
        root = (-12.2, 2.3 * cy, 2.3 * cz)
        rod(rods, root, (-1, 0.14 * cy, 0.14 * cz), length, 0.75, 0.28, (0, 0, lift))
    write_md3(os.path.join(out, "models/abilities/drone/drone.md3"), "drone", [m, rods])

    def body(u, v, n=Fbm(83, 4, 8)):
        k = 0.32 * (0.8 + 0.4 * n.at(u * 4, v * 4))
        if abs(v - 0.5) < 0.04:
            k = 0.6                                   # the band round its middle
        return [k, k, k * 1.08, 1]

    def eye(u, v):
        d = math.hypot(min(u, 1 - u) * 2.2, (v - 0.5) * 1.4)   # u 0 and 1 are both the front
        g = max(0.0, 1 - d / 0.16) ** 1.5
        return [g, 0.12 * g, 0.08 * g, 1]

    def rods_metal(u, v, grain=Fbm(89, 3, 8), streak=Noise(97, 64, 4)):
        """Brushed gunmetal along the rod, a polished steel ferrule at the
        root, two blackened joints and a black tip."""
        k = 0.26 + 0.06 * grain.at(u * 2, v * 2) + 0.10 * (streak.at(u * 64, v * 2) - 0.5)
        tint = 1.04
        if v < 0.07:
            k, tint = 0.62 + 0.08 * streak.at(u * 64, 0), 1.0      # the ferrule
        elif min(abs(v - 0.36), abs(v - 0.66)) < 0.025:
            k, tint = 0.07, 1.0                                    # the joints
        elif v > 0.9:
            k, tint = 0.06 + 0.04 * (1 - (v - 0.9) / 0.1), 1.0      # the tip
        return [k, k, k * tint, 1]

    def sheen(u, v):
        """A soft studio sky for the rods' reflection: bright overhead,
        dark below."""
        y = 1 - v
        k = 0.05 + 0.5 * max(0.0, y - 0.45) ** 2 * 4 + 0.25 * math.exp(-((u - 0.3) ** 2 + (y - 0.75) ** 2) * 60)
        return [k, k, k * 1.05, 1]

    texture(os.path.join(out, "models/abilities/drone/drone.tga"), 64, 64, body, False)
    texture(os.path.join(out, "models/abilities/drone/drone_eye.tga"), 64, 64, eye, False)
    texture(os.path.join(out, "models/abilities/drone/drone_rods.tga"), 32, 128, rods_metal, False)
    texture(os.path.join(out, "models/abilities/drone/drone_sheen.tga"), 64, 64, sheen, False)


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


def hud_icons(out):
    """The vitals' icons (hud.md 9.7): a health cross and an armour shield,
    white with a dark outline so the HUD can tint them."""
    white = (0.95, 0.95, 0.93)
    cross = union(art.box(0.5, 0.5, 0.36, 0.13, 0.03), art.box(0.5, 0.5, 0.13, 0.36, 0.03))
    shield = polygon([(0.5, 0.08), (0.88, 0.2), (0.84, 0.56), (0.5, 0.94), (0.16, 0.56), (0.12, 0.2)])
    for name, shape in (("health", cross), ("armor", shield)):
        c = Canvas(64, 64)
        c.fill(shape.outline(0.06), INK)
        c.fill(shape, white)
        write_tga(os.path.join(out, "icons/tremor_%s.tga" % name), 64, 64, c.px)


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
    "F": ["11111", "10000", "10000", "11110", "10000", "10000", "10000"],
    "G": ["01111", "10000", "10000", "10011", "10001", "10001", "01111"],
    "H": ["10001", "10001", "10001", "11111", "10001", "10001", "10001"],
    "I": ["11111", "00100", "00100", "00100", "00100", "00100", "11111"],
    "J": ["00111", "00010", "00010", "00010", "10010", "10010", "01100"],
    "K": ["10001", "10010", "10100", "11000", "10100", "10010", "10001"],
    "L": ["10000", "10000", "10000", "10000", "10000", "10000", "11111"],
    "M": ["10001", "11011", "10101", "10101", "10001", "10001", "10001"],
    "N": ["10001", "11001", "10101", "10011", "10001", "10001", "10001"],
    "O": ["01110", "10001", "10001", "10001", "10001", "10001", "01110"],
    "P": ["11110", "10001", "10001", "11110", "10000", "10000", "10000"],
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


def hud_badges(out):
    """hud.md 9.7: the mode icon (top left) and the network warning icons,
    white letters on a dark hex or square, tinted by the HUD."""
    for tag, text in (("ffa", "FFA"), ("duel", "DU"), ("rduel", "RD"), ("tdm", "TDM"), ("ca", "CA"),
                      ("ctf", "CTF"), ("sac", "SAC")):
        c = Canvas(64, 64)
        c.fill(regular(0.5, 0.5, 0.48, 6, 0), (0.55, 0.55, 0.58))
        c.fill(regular(0.5, 0.5, 0.42, 6, 0), INK)
        monogram(c, text, 0.5, 0.5, 0.045 if len(text) > 2 else 0.06, SILVER)
        path = os.path.join(out, "gfx/tremor/modes/%s.tga" % tag)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        write_tga(path, 64, 64, c.px)
    for tag, text in (("loss", "PL"), ("ping", "MS"), ("spike", "SP"), ("fps", "FPS")):
        c = Canvas(64, 64)
        c.fill(art.box(0.5, 0.5, 0.46, 0.46, 0.1), (1, 1, 1))
        c.fill(art.box(0.5, 0.5, 0.4, 0.4, 0.07), INK)
        monogram(c, text, 0.5, 0.5, 0.045 if len(text) > 2 else 0.06, (1, 1, 1))
        path = os.path.join(out, "gfx/tremor/net/%s.tga" % tag)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        write_tga(path, 64, 64, c.px)


# ---------------------------------------------------------------- sounds

def write_wav(path, samples, rate=22050):
    """16-bit mono PCM; samples in -1..1."""
    data = b"".join(struct.pack("<h", int(max(-1.0, min(1.0, v)) * 32000)) for v in samples)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(b"RIFF" + struct.pack("<I", 36 + len(data)) + b"WAVEfmt ")
        f.write(struct.pack("<IHHIIHH", 16, 1, 1, rate, rate * 2, 2, 16))
        f.write(b"data" + struct.pack("<I", len(data)) + data)


def level(samples, db=-1.0):
    """Scaled so the peak sits at db dBFS (sound-clarity.md 4: never clipped)."""
    peak = max(abs(v) for v in samples) or 1.0
    gain = 10 ** (db / 20) * 32767 / 32000 / peak
    return [v * gain for v in samples]


def tone(freqs, seconds, decay, noise=0.0, rate=22050, seed=5):
    rnd = random.Random(seed)
    out = []
    n = int(seconds * rate)
    for i in range(n):
        t = i / rate
        env = math.exp(-t * decay) * min(1.0, t * 400) * min(1.0, (n - i) / (0.04 * rate))   # a 40 ms faded tail
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
            env = math.exp(-t * decay) * min(1.0, t * 200) * min(1.0, (n - i) / (0.04 * rate))   # faded tail
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


# ---------------------------------------------------------------- ability sounds

RATE = 22050


def _f(v, t):
    return v(t) if callable(v) else v


def osc(freq, seconds, wave="sine", amp=1.0):
    """A tone; freq a number or a function of time. sine, saw, square or tri."""
    out, phase = [], 0.0
    for i in range(int(seconds * RATE)):
        t = i / RATE
        phase += 2 * math.pi * _f(freq, t) / RATE
        x = (phase / (2 * math.pi)) % 1.0
        if wave == "saw":
            v = 2 * x - 1
        elif wave == "square":
            v = 1.0 if x < 0.5 else -1.0
        elif wave == "tri":
            v = 4 * abs(x - 0.5) - 1
        else:
            v = math.sin(phase)
        out.append(v * _f(amp, t))
    return out


def noise(seconds, cutoff, seed=1, amp=1.0):
    """Noise through a one-pole low-pass; cutoff a number or a function of time."""
    rnd = random.Random(seed)
    out, lp = [], 0.0
    for i in range(int(seconds * RATE)):
        t = i / RATE
        k = 1.0 - math.exp(-2 * math.pi * _f(cutoff, t) / RATE)
        lp += k * ((rnd.random() * 2 - 1) - lp)
        out.append(lp * _f(amp, t))
    return out


def env(samples, attack=0.005, decay=6.0, release=0.04):
    """A fast attack, an exponential fall, and a faded tail (never cut)."""
    n = len(samples)
    out = []
    for i, v in enumerate(samples):
        t = i / RATE
        g = min(1.0, t / attack) * math.exp(-t * decay)
        tail = (n - i) / RATE
        if tail < release:
            g *= tail / release
        out.append(v * g)
    return out


def mix(*parts):
    """(samples, gain, start seconds) parts summed."""
    n = max(int(start * RATE) + len(s) for s, g, start in parts)
    out = [0.0] * n
    for s, g, start in parts:
        o = int(start * RATE)
        for i, v in enumerate(s):
            out[o + i] += v * g
    return out


def lfo(rate, depth, base):
    return lambda t: base * (1 + depth * math.sin(2 * math.pi * rate * t))


def glide(f0, f1, seconds):
    return lambda t: f0 + (f1 - f0) * min(1.0, t / seconds)


# sound-clarity.md 4 / SND-13: every ability its own recognisable cast (at
# most 1.5 s) and end (at most 1.0 s), mono, peak -1 dBFS, tails faded
ABILITY_SOUNDS = {
    # Athena's hook: a metallic twang and the chain whipping out
    "hook": (lambda: mix((env(osc(glide(900, 1400, 0.1), 0.5, "tri"), decay=7), 0.6, 0),
                         (env(osc(2650, 0.5), decay=9), 0.25, 0),
                         (env(noise(0.35, glide(3000, 800, 0.35), 3), decay=6), 0.5, 0.02)),
             lambda: mix(*[(env(osc(2300 + k * 170, 0.05, "tri"), decay=60), 0.45, k * 0.045) for k in range(5)]
                         + [(env(noise(0.25, 2500, 4), decay=14), 0.3, 0)])),		# the chain rattling back
    # Scalebearer's bull rush: a low rumble surging forward
    "bullrush": (lambda: mix((env(noise(1.0, glide(200, 900, 0.8), 5), attack=0.08, decay=1.6), 1.0, 0),
                             (env(osc(glide(55, 90, 0.8), 1.0, "saw"), attack=0.06, decay=1.8), 0.35, 0)),
                 lambda: mix((env(noise(0.5, 400, 6), decay=7), 0.9, 0), (env(osc(70, 0.5, "sine"), decay=8), 0.6, 0))),
    # the injection: a syringe hiss and a rising blip
    "injection": (lambda: mix((env(noise(0.45, 6000, 7), attack=0.02, decay=5), 0.45, 0),
                              (env(osc(glide(600, 1500, 0.25), 0.3), decay=8), 0.6, 0.12)),
                  lambda: mix((env(osc(glide(1400, 700, 0.2), 0.25), decay=12), 0.6, 0))),
    # B.J.'s dual wield: two clicks and a cocking slide
    "dualwield": (lambda: mix((env(noise(0.06, 5000, 8), decay=60), 0.9, 0), (env(noise(0.06, 5000, 9), decay=60), 0.9, 0.12),
                              (env(noise(0.25, glide(1200, 3500, 0.2), 10), attack=0.02, decay=10), 0.6, 0.22),
                              (env(osc(1900, 0.2, "square"), decay=25), 0.12, 0.24)),
                  lambda: mix((env(noise(0.08, 4000, 11), decay=50), 0.9, 0), (env(noise(0.08, 3000, 12), decay=50), 0.8, 0.1))),
    # the barrier: a shimmering chord settling into place
    "barrier": (lambda: mix((env(osc(lfo(6, 0.01, 440), 1.2), attack=0.1, decay=2.2), 0.4, 0),
                            (env(osc(lfo(6.5, 0.01, 554), 1.2), attack=0.12, decay=2.2), 0.35, 0),
                            (env(osc(lfo(7, 0.01, 659), 1.2), attack=0.14, decay=2.2), 0.3, 0),
                            (env(noise(1.0, 7000, 13), attack=0.05, decay=4), 0.12, 0)),
                lambda: mix((env(osc(glide(659, 330, 0.5), 0.6), decay=6), 0.5, 0), (env(noise(0.5, 5000, 14), decay=8), 0.2, 0))),
    # Death Knight's flame strike: a crackling roar
    "flamestrike": (lambda: mix((env(noise(1.2, lfo(13, 0.6, 1500), 15), attack=0.04, decay=2.2), 1.0, 0),
                                (env(noise(1.2, 300, 16), attack=0.08, decay=2.0), 0.9, 0)),
                    lambda: mix((env(osc(glide(90, 45, 0.3), 0.4), decay=9), 0.8, 0),
                                *[(env(noise(0.03, 3000, 170 + k), decay=80), 0.6, 0.05 + k * 0.07) for k in range(6)])),
    # Doom Slayer's berserk: a vibrating growl
    "berserk": (lambda: mix((env(osc(lfo(18, 0.12, 82), 1.1, "saw"), attack=0.05, decay=2.2), 0.6, 0),
                            (env(noise(1.1, lfo(18, 0.5, 600), 18), attack=0.05, decay=2.4), 0.6, 0)),
                lambda: mix((env(osc(glide(110, 55, 0.6), 0.7, "saw"), decay=5), 0.5, 0))),
    # the turret: a mechanical clack and a servo whine
    "turret": (lambda: mix((env(noise(0.08, 3000, 19), decay=45), 1.0, 0),
                           (env(osc(glide(300, 900, 0.5), 0.7, "square"), attack=0.04, decay=3.5), 0.18, 0.08),
                           (env(noise(0.08, 2500, 20), decay=45), 0.9, 0.6)),
               lambda: mix((env(osc(glide(800, 250, 0.4), 0.5, "square"), decay=6), 0.18, 0), (env(noise(0.1, 2000, 21), decay=40), 0.8, 0.35))),
    # Galena's totem: a hollow knock and an eerie tone
    "totem": (lambda: mix((env(osc(180, 0.25), decay=25), 1.0, 0), (env(noise(0.06, 1200, 22), decay=50), 0.6, 0),
                          (env(osc(lfo(5, 0.03, 740), 1.0, "tri"), attack=0.15, decay=3), 0.25, 0.05)),
              lambda: mix((env(osc(lfo(5, 0.03, 520), 0.6, "tri"), decay=6), 0.3, 0), (env(osc(150, 0.2), decay=30), 0.8, 0))),
    # Keel's grenades: the pins and a throw
    "grenades": (lambda: mix((env(osc(3200, 0.05), decay=60), 0.6, 0), (env(osc(3500, 0.05), decay=60), 0.6, 0.06),
                             (env(noise(0.4, glide(600, 2500, 0.3), 23), attack=0.05, decay=6), 0.8, 0.12)),
                 lambda: mix((env(osc(2900, 0.06), decay=60), 0.6, 0), (env(noise(0.1, 1500, 24), decay=40), 0.5, 0))),
    # Nyx's ghost walk: an airy reversed breath and a high shimmer
    "ghostwalk": (lambda: mix((env(noise(0.9, glide(500, 4000, 0.8), 25), attack=0.6, decay=0.3, release=0.2), 0.8, 0),
                              (env(osc(lfo(9, 0.02, 1760), 0.9), attack=0.5, decay=0.5, release=0.2), 0.12, 0)),
                  lambda: mix((env(osc(1320, 0.6), decay=6), 0.45, 0), (env(osc(1980, 0.6), decay=8), 0.2, 0),
                              (env(noise(0.5, glide(3000, 600, 0.4), 26), decay=8), 0.3, 0))),
    # Ranger's dire orb: a deep wobbling hum
    "orb": (lambda: mix((env(osc(lfo(7, 0.25, 110), 1.2), attack=0.05, decay=1.5), 0.8, 0),
                        (env(osc(lfo(7, 0.25, 220), 1.2, "tri"), attack=0.05, decay=1.8), 0.3, 0),
                        (env(noise(0.3, 2000, 27), decay=10), 0.3, 0)),
            lambda: mix((env(osc(glide(220, 60, 0.3), 0.4), decay=8), 0.8, 0), (env(noise(0.4, 900, 28), decay=8), 0.5, 0))),
    # Slash's plasma trail: an electric buzz rising
    "plasmatrail": (lambda: mix((env(osc(glide(120, 240, 0.8), 1.0, "saw"), attack=0.03, decay=1.5), 0.4, 0),
                                (env(noise(1.0, lfo(60, 0.7, 3000), 29), attack=0.03, decay=1.8), 0.4, 0)),
                    lambda: mix((env(osc(glide(240, 90, 0.4), 0.5, "saw"), decay=6), 0.4, 0))),
    # Sorlag's acid spit: a wet splat and a bubble
    "acidspit": (lambda: mix((env(noise(0.25, 900, 30), decay=14), 1.0, 0),
                             (env(osc(glide(300, 900, 0.1), 0.15), decay=20), 0.5, 0.05),
                             (env(osc(glide(400, 1100, 0.08), 0.12), decay=25), 0.4, 0.2)),
                 lambda: mix((env(noise(0.3, 700, 31), decay=12), 0.9, 0), (env(osc(glide(500, 900, 0.1), 0.12), decay=25), 0.4, 0.1))),
    # Strogg's drone: a servo spinning up
    "drone": (lambda: mix((env(osc(glide(200, 1200, 0.9), 1.1, "square"), attack=0.05, decay=1.5), 0.15, 0),
                          (env(noise(1.1, glide(400, 3000, 0.9), 32), attack=0.05, decay=1.8), 0.6, 0)),
              lambda: mix((env(osc(1000, 0.09, "square"), decay=20), 0.2, 0), (env(osc(700, 0.09, "square"), decay=20), 0.2, 0.13),
                          (env(osc(450, 0.14, "square"), decay=14), 0.2, 0.26))),
    # Visor's piercing sight: a high ping and a sonar sweep
    "piercingsight": (lambda: mix((env(osc(2093, 0.4), decay=10), 0.6, 0), (env(osc(glide(500, 2500, 0.8), 1.0), attack=0.2, decay=2), 0.3, 0.1)),
                      lambda: mix((env(osc(1568, 0.3), decay=14), 0.5, 0))),
    # Wrack's shell shock: a deep thud and a ringing
    "shellshock": (lambda: mix((env(osc(glide(90, 40, 0.3), 0.6), decay=6), 1.0, 0), (env(noise(0.2, 600, 34), decay=18), 0.6, 0),
                               (env(osc(3100, 1.2), attack=0.05, decay=2.5), 0.12, 0.05)),
                   lambda: mix((env(osc(2800, 0.6), decay=6), 0.15, 0), (env(noise(0.3, 500, 35), decay=10), 0.5, 0))),
    # Klesk's spine volley: three quick whistles
    "spinevolley": (lambda: mix(*[(env(osc(glide(2200 - k * 200, 1200 - k * 100, 0.15), 0.2), decay=10), 0.5, k * 0.09) for k in range(3)]),
                    lambda: mix((env(noise(0.06, 1500, 36), decay=60), 0.9, 0), (env(osc(glide(1400, 600, 0.12), 0.15, "tri"), decay=18), 0.4, 0.03))),
    # master of death: an ominous low chord
    "masterofdeath": (lambda: mix((env(osc(lfo(4, 0.01, 65), 1.4, "saw"), attack=0.2, decay=1.3), 0.35, 0),
                                  (env(osc(lfo(4.5, 0.01, 98), 1.4, "saw"), attack=0.25, decay=1.3), 0.25, 0),
                                  (env(osc(lfo(5, 0.01, 155), 1.4, "tri"), attack=0.3, decay=1.3), 0.3, 0),
                                  (env(noise(1.4, 400, 37), attack=0.3, decay=1.5), 0.4, 0)),
                      lambda: mix((env(osc(glide(155, 65, 0.6), 0.8, "tri"), decay=4), 0.4, 0))),
    # Sarge's supply drop: two radio beeps and a whoosh from above
    "supplydrop": (lambda: mix((env(osc(1200, 0.1, "square"), decay=8), 0.2, 0), (env(osc(1600, 0.1, "square"), decay=8), 0.2, 0.15),
                               (env(noise(0.7, glide(3000, 500, 0.6), 38), attack=0.1, decay=3), 0.7, 0.3)),
                   lambda: mix((env(osc(1600, 0.1, "square"), decay=8), 0.2, 0), (env(noise(0.25, 300, 39), decay=12), 0.9, 0.08))),
    # Hunter's mark prey: a sharp rising whistle, a hunting cry
    "markprey": (lambda: mix((env(osc(glide(900, 2600, 0.35), 0.6, "tri"), attack=0.02, decay=4), 0.5, 0),
                             (env(osc(glide(1800, 3400, 0.35), 0.5), attack=0.02, decay=5), 0.2, 0)),
                 lambda: mix((env(osc(glide(2600, 900, 0.3), 0.4, "tri"), decay=8), 0.4, 0))),
    # Raine's downpour: a rain swell
    "downpour": (lambda: mix((env(noise(1.4, lfo(25, 0.4, 5000), 40), attack=0.35, decay=1.0), 0.8, 0),
                             (env(noise(1.4, 400, 41), attack=0.35, decay=1.0), 0.5, 0)),
                 lambda: mix((env(noise(0.8, 4500, 42), decay=4), 0.5, 0),
                             *[(env(osc(1900 + k * 230, 0.06), decay=45), 0.35, 0.1 + k * 0.13) for k in range(5)])),
    # Beastmaster's hound: a snarl
    "hound": (lambda: mix((env(noise(0.8, lfo(22, 0.6, 700), 43), attack=0.05, decay=3), 1.0, 0),
                          (env(osc(lfo(22, 0.15, 140), 0.8, "saw"), attack=0.05, decay=3.2), 0.3, 0)),
              lambda: mix((env(osc(lambda t: (900 - 1100 * t) * (1 + 0.08 * math.sin(2 * math.pi * 9 * t)), 0.38, "tri"), decay=5), 0.6, 0))),
    # Unumrey's rift anchor: a warp falling into place
    "riftanchor": (lambda: mix((env(osc(glide(1600, 120, 1.0), 1.2, "tri"), attack=0.03, decay=1.8), 0.5, 0),
                               (env(noise(1.2, glide(5000, 300, 1.0), 45), attack=0.03, decay=2.0), 0.5, 0)),
                   lambda: mix((env(osc(88, 0.8), attack=0.01, decay=4), 0.8, 0), (env(osc(88 * 2.76, 0.8), decay=5), 0.3, 0),
                               (env(noise(0.5, 6000, 46), decay=10), 0.2, 0))),
}


def sounds(out):
    def put(path, samples):
        # sound-clarity.md 4: every one peaks at -1 dBFS, never clipped
        write_wav(path, level(samples))

    put(os.path.join(out, "sound/tremor/hit_armor.wav"), tone([(2400, 0.6), (3600, 0.3), (5100, 0.15)], 0.12, 38))
    put(os.path.join(out, "sound/tremor/hit_health.wav"), tone([(520, 0.7), (780, 0.25)], 0.12, 45, noise=0.15))
    put(os.path.join(out, "sound/tremor/armor_break.wav"), tone([(1800, 0.4), (2700, 0.3), (900, 0.3)], 0.35, 14, noise=0.5))
    # a bell (fundamental and inharmonic partials) on A, C sharp and E, 0.16 s apart
    call = [0.0] * int(1.2 * 22050)
    for n, f in enumerate((880, 1109, 1319)):
        bell = tone([(f, 0.6), (f * 2.76, 0.2), (f * 5.4, 0.08)], 0.8, 6)
        start = int(n * 0.16 * 22050)
        for i, v in enumerate(bell):
            call[start + i] += v * 0.6
    put(os.path.join(out, "sound/tremor/match_found.wav"), call)
    # the crouch slide: a scrape starting, a steady loop, a short stop
    put(os.path.join(out, "sound/tremor/slide_start.wav"), scrape(0.25, 600, 9, seed=41))
    put(os.path.join(out, "sound/tremor/slide_loop.wav"), scrape(1.0, 420, 0, seed=43, loop=True))
    put(os.path.join(out, "sound/tremor/slide_stop.wav"), scrape(0.18, 300, 22, seed=47))
    # Clutch's dodge: a short airy whoosh
    put(os.path.join(out, "sound/tremor/dodge.wav"), level(whoosh(0.28, seed=53)))
    # Athena's hook biting: a short metallic clank
    put(os.path.join(out, "sound/tremor/hook_attach.wav"), tone([(1400, 0.5), (2300, 0.35), (3700, 0.2)], 0.22, 22, noise=0.35, seed=59))
    # the hit beeps, 0.12 s each, falling in pitch with the damage tier
    for n, f in enumerate((1560, 1240, 960, 740)):
        put(os.path.join(out, "sound/tremor/hitbeep%i.wav" % (n + 1)),
                  tone([(f, 0.8), (f * 2, 0.15)], 0.12, 30, seed=71 + n))
    # the kill beep: a short bell
    put(os.path.join(out, "sound/tremor/killbeep.wav"),
              tone([(1760, 0.6), (1760 * 2.76, 0.18), (1760 * 5.4, 0.06)], 0.35, 11, seed=79))
    # a Tri-bolt bolt bursting: a short crack over a low thump
    crack = scrape(0.55, 1800, 9, seed=61)
    thump = tone([(95, 0.9), (62, 0.6)], 0.55, 8, seed=67)
    put(os.path.join(out, "sound/tremor/tribolt_exp.wav"),
              level([0.75 * a + 0.9 * b for a, b in zip(crack, thump)]))
    # Strogg's drone charging (the second press): a servo screaming up and a rush
    put(os.path.join(out, "sound/abilities/drone_charge.wav"),
        mix((env(osc(glide(400, 1800, 0.5), 0.7, "square"), attack=0.02, decay=2.5), 0.15, 0),
            (env(noise(0.7, glide(800, 5000, 0.5), 47), attack=0.05, decay=2.2), 0.8, 0)))
    # a Berserk punch that misses: a heavy swing through the air
    put(os.path.join(out, "sound/abilities/berserk_swing.wav"),
        mix((env(noise(0.3, glide(600, 2200, 0.2), 89), attack=0.03, decay=9), 1.0, 0),
            (env(osc(glide(140, 90, 0.25), 0.3), attack=0.02, decay=10), 0.3, 0)))
    # Clutch's Mining Laser: a searing hum (a loop: whole cycles in 1 s)
    laser = mix((osc(110, 1.0, "saw"), 0.3, 0), (osc(220, 1.0, "square"), 0.12, 0),
                (osc(lfo(6, 0.02, 1760), 1.0, "tri"), 0.12, 0), (noise(1.0, 4000, 101, amp=0.3), 1.0, 0))
    n = len(laser)
    laser = [v * min(1.0, i / (0.02 * RATE), (n - i) / (0.02 * RATE)) for i, v in enumerate(laser)]
    write_wav(os.path.join(out, "sound/abilities/laser_loop.wav"), level(laser, db=-6))
    # Galena's totem: a burst (a cracking thud) and a heal (a rising chime)
    put(os.path.join(out, "sound/abilities/totem_burst.wav"),
        mix((env(noise(0.5, glide(5000, 600, 0.4), 102), decay=7), 1.0, 0),
            (env(osc(glide(150, 55, 0.35), 0.5), decay=7), 0.9, 0),
            (env(osc(lfo(7, 0.05, 520), 0.5, "tri"), decay=6), 0.25, 0.02)))
    put(os.path.join(out, "sound/abilities/totem_heal.wav"),
        mix((env(osc(glide(660, 990, 0.25), 0.4, "tri"), attack=0.02, decay=6), 0.5, 0),
            (env(osc(1320, 0.3), attack=0.03, decay=8), 0.25, 0.08)))
    # Sorlag's acid: a wet spit per blob, a splash where it lands, a pool's
    # sizzle (a loop); shell shock: the ears ringing (a loop)
    put(os.path.join(out, "sound/abilities/acid_spit.wav"),
        mix((env(noise(0.25, glide(2500, 900, 0.2), 103), attack=0.01, decay=12), 1.0, 0),
            (env(osc(glide(300, 120, 0.15), 0.2), decay=15), 0.5, 0)))
    put(os.path.join(out, "sound/abilities/acid_splash.wav"),
        mix((env(noise(0.35, glide(4000, 700, 0.3), 104), decay=9), 1.0, 0),
            (env(noise(0.2, 1200, 105), decay=14), 0.6, 0.04)))
    sizzle = noise(1.0, lfo(3, 0.3, 3500), 106)
    n = len(sizzle)
    sizzle = [v * min(1.0, i / (0.02 * RATE), (n - i) / (0.02 * RATE)) for i, v in enumerate(sizzle)]
    write_wav(os.path.join(out, "sound/abilities/acid_pool.wav"), level(sizzle, db=-14))
    ring = mix((osc(3000, 1.0), 0.5, 0), (osc(3010, 1.0), 0.5, 0), (osc(4500, 1.0, amp=0.15), 1.0, 0))
    write_wav(os.path.join(out, "sound/abilities/shock_ring.wav"), level(ring, db=-16))
    # loops while an ability runs (whole cycles in their second): Visor's
    # sight (a high shimmer), the Barrier's hum, the hook's rope pulling
    def loop_fade(x):
        n = len(x)
        return [v * min(1.0, i / (0.02 * RATE), (n - i) / (0.02 * RATE)) for i, v in enumerate(x)]
    write_wav(os.path.join(out, "sound/abilities/piercingsight_loop.wav"), level(loop_fade(
        mix((osc(lfo(4, 0.01, 2400), 1.0, "tri"), 0.3, 0), (osc(1800, 1.0), 0.2, 0), (noise(1.0, 6000, 107, amp=0.15), 1.0, 0))), db=-14))
    write_wav(os.path.join(out, "sound/abilities/barrier_loop.wav"), level(loop_fade(
        mix((osc(100, 1.0, "saw"), 0.3, 0), (osc(200, 1.0), 0.3, 0), (osc(lfo(5, 0.03, 800), 1.0, "tri"), 0.08, 0))), db=-13))
    write_wav(os.path.join(out, "sound/abilities/hook_loop.wav"), level(loop_fade(
        mix((noise(1.0, lfo(8, 0.4, 2200), 108), 0.8, 0), (osc(lfo(8, 0.05, 320), 1.0, "saw"), 0.15, 0))), db=-14))
    # the drone's gun: a light, quick zap
    put(os.path.join(out, "sound/abilities/drone_shot.wav"),
        mix((env(osc(glide(2400, 900, 0.07), 0.1, "saw"), decay=35), 0.3, 0),
            (env(osc(glide(1200, 450, 0.07), 0.1), decay=35), 0.4, 0),
            (env(noise(0.04, 6000, 88), decay=90), 0.5, 0)))
    # the Peeker drone, a machine (abilities.md "Peeker drone: HUD and
    # sounds"): metal clanks on a hit (three takes), the pilot's glitch, a
    # hovering hum and a motion whirr (loops: whole cycles in one second),
    # a bolt pinging off it, a detection blip and its own blast
    for n in range(3):
        f = (1900, 2350, 2700)[n]
        put(os.path.join(out, "sound/abilities/drone_hit%i.wav" % (n + 1)),
            mix((env(osc(f, 0.25, "tri"), decay=22), 0.4, 0),
                (env(osc(f * 1.51, 0.2), decay=28), 0.25, 0),
                (env(noise(0.08, 5000, 90 + n), decay=50), 0.8, 0),
                (env(noise(0.12, 2500, 93 + n), decay=30), 0.25, 0.03)))
    put(os.path.join(out, "sound/abilities/drone_glitch.wav"),
        mix(*[(env(osc(400 + 900 * (k % 3), 0.04, "square"), decay=40), 0.25, k * 0.035) for k in range(6)]
            + [(env(noise(0.22, 7000, 97), decay=10), 0.5, 0)]))
    hum = mix((osc(90, 1.0, "saw"), 0.25, 0), (osc(180, 1.0), 0.35, 0),
              (osc(1260, 1.0, "tri", amp=lambda t: 0.06 * (1 + math.sin(2 * math.pi * 4 * t))), 1.0, 0))
    write_wav(os.path.join(out, "sound/abilities/drone_hum.wav"), level(hum, db=-12))
    whirr = mix((osc(lfo(3, 0.05, 340), 1.0, "saw"), 0.2, 0), (noise(1.0, 1500, 98, amp=0.4), 1.0, 0))
    n = len(whirr)
    whirr = [v * min(1.0, i / (0.02 * RATE), (n - i) / (0.02 * RATE)) for i, v in enumerate(whirr)]
    write_wav(os.path.join(out, "sound/abilities/drone_move.wav"), level(whirr, db=-14))
    put(os.path.join(out, "sound/abilities/drone_bolt_hit.wav"),
        mix((env(osc(3100, 0.12, "tri"), decay=40), 0.35, 0), (env(noise(0.05, 6000, 99), decay=70), 0.7, 0)))
    write_wav(os.path.join(out, "sound/abilities/drone_detect.wav"), level(
        mix((env(osc(1568, 0.06, "square"), decay=35), 0.3, 0), (env(osc(2093, 0.08, "square"), decay=30), 0.3, 0.07)), db=-8))
    put(os.path.join(out, "sound/abilities/drone_explode.wav"),
        mix((env(noise(0.9, glide(6000, 500, 0.7), 100), decay=4.5), 1.0, 0),
            (env(osc(glide(120, 45, 0.5), 0.8), decay=5), 0.9, 0),
            (env(osc(glide(2400, 600, 0.3), 0.4, "square"), decay=9), 0.15, 0)))
    # Eisen's turret at work (konstantin-list-audit 2: deploy, coming online,
    # target acquired, each shot, an idle hum, destroyed)
    put(os.path.join(out, "sound/abilities/turret_deploy.wav"),
        mix((env(osc(glide(120, 60, 0.15), 0.3), decay=14), 1.0, 0),
            (env(noise(0.1, 2500, 81), decay=40), 0.8, 0),
            (env(osc(1900, 0.2, "tri"), decay=25), 0.25, 0.02),
            (env(osc(glide(220, 660, 0.45), 0.55, "saw"), attack=0.05, decay=3), 0.2, 0.12),
            (env(noise(0.06, 3500, 82), decay=60), 0.7, 0.62),
            (env(noise(0.06, 3000, 83), decay=60), 0.6, 0.74)))
    put(os.path.join(out, "sound/abilities/turret_online.wav"),
        mix((env(osc(880, 0.12, "square"), decay=18), 0.25, 0),
            (env(osc(1320, 0.18, "square"), decay=14), 0.25, 0.1),
            (env(noise(0.05, 4000, 84), decay=70), 0.6, 0)))
    # (a short square beep: quieter than the rest, -9 dBFS peak)
    write_wav(os.path.join(out, "sound/abilities/turret_target.wav"), level(
        mix((env(osc(2093, 0.07, "square"), decay=30), 0.3, 0),
            (env(osc(2093, 0.07, "square"), decay=30), 0.3, 0.1),
            (env(osc(2637, 0.1, "square"), decay=25), 0.3, 0.2)), db=-9))
    put(os.path.join(out, "sound/abilities/turret_shot.wav"),
        mix((env(osc(glide(1700, 500, 0.09), 0.12, "square"), decay=28), 0.35, 0),
            (env(noise(0.06, 5000, 85), decay=70), 0.7, 0),
            (env(osc(140, 0.1), decay=40), 0.6, 0)))
    # the hum loops: whole cycles in its one second, so it repeats cleanly
    hum = mix((osc(60, 1.0, "saw"), 0.3, 0), (osc(120, 1.0), 0.4, 0),
              (osc(1500, 1.0, "tri", amp=lambda t: 0.05 * (1 + math.sin(2 * math.pi * 2 * t))), 1.0, 0))
    write_wav(os.path.join(out, "sound/abilities/turret_idle.wav"), level(hum, db=-14))
    put(os.path.join(out, "sound/abilities/turret_destroyed.wav"),
        mix((env(noise(0.7, glide(4000, 400, 0.6), 86), decay=5), 1.0, 0),
            (env(osc(glide(90, 40, 0.4), 0.6), decay=6), 0.9, 0),
            (env(osc(glide(1200, 300, 0.5), 0.6, "tri"), decay=6), 0.3, 0.03),
            (env(noise(0.05, 3000, 87), decay=60), 0.6, 0.35)))
    # each ability: its own cast and end (ABILITY_SOUNDS), levelled to -1 dBFS
    for name in ABILITIES:
        cast, end = ABILITY_SOUNDS[name]
        put(os.path.join(out, "sound/abilities/%s_cast.wav" % name), level(cast()))
        put(os.path.join(out, "sound/abilities/%s_end.wav" % name), level(end()))


SHADERS = """// tremor-invictus generated assets (misc/art/make-models.py)
// Replaceable: a pack loaded later may define these names again.

// the world item timers' pie slices (hud.md 9.4): the client's colour and
// alpha per vertex, hidden by walls
gfx/tremor/pie
{
	nopicmip
	cull none
	{
		map $whiteimage
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen vertex
		alphaGen vertex
	}
}

// the drone's charge blast, faded by the client (additive)
gfx/abilities/drone_blast
{
	nopicmip
	cull none
	{
		clampmap gfx/abilities/drone_blast.tga
		blendFunc GL_ONE GL_ONE
		rgbGen vertex
	}
}

// Slash's skate thrust: a jet turned to the viewer, lifted off the floor
// and bobbing as if it held her up, pulsing, with boiling streaks round it
// (the skates' own surfaces wear it; looks/slash.py)
tremor/slash_thrust
{
	sort additive
	cull disable
	deformVertexes move 0 0 1 sin 5 2 0 4
	deformVertexes autoSprite
	{
		clampmap gfx/tremor/thrust.tga
		blendFunc GL_ONE GL_ONE
		tcMod stretch sin 0.92 0.12 0 7
	}
	{
		clampmap gfx/tremor/thrust_flicker.tga
		blendFunc GL_ONE GL_ONE
		tcMod rotate 240
		rgbGen wave noise 0.7 0.5 0 9
	}
}

// the low-health overlay (hud.md 9.7), coloured and faded by the HUD
gfx/tremor/blood_edge
{
	nopicmip
	{
		clampmap gfx/tremor/blood_edge.tga
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen vertex
		alphaGen vertex
	}
}

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
	{
		map models/abilities/totem/totem_cracks.tga
		blendFunc GL_ONE GL_ONE
		rgbGen entity
	}
}

// the groove and the core in the entity colour (the owner's side), pulsing
ability/totemcore
{
	sort additive
	{
		map models/abilities/totem/totem_core.tga
		blendFunc GL_ONE GL_ONE
		tcMod rotate 40
		rgbGen entity
	}
	{
		map models/abilities/totem/totem_core.tga
		blendFunc GL_ONE GL_ONE
		tcMod scroll 0 0.6
		rgbGen wave sin 0.35 0.25 0 1.4
	}
}

// the totem's aura on the floor and the light over its core (cg_ents.c), tinted by vertex colour
ability/totem_aura
{
	nopicmip
	polygonOffset
	cull none
	sort additive
	{
		clampmap gfx/abilities/totem_aura.tga
		blendFunc GL_ONE GL_ONE
		rgbGen vertex
	}
}

ability/totem_beam
{
	nopicmip
	cull none
	sort additive
	{
		clampmap gfx/abilities/totem_beam.tga
		blendFunc GL_ONE GL_ONE
		rgbGen vertex
	}
}

ability/drone
{
	{
		map models/abilities/drone/drone.tga
		rgbGen lightingDiffuse
	}
	{
		map models/abilities/drone/drone_eye.tga
		blendFunc GL_ONE GL_ONE
		rgbGen identity
	}
}

// the Peeker's trailing rods: brushed gunmetal with a soft metal sheen
ability/drone_rods
{
	{
		map models/abilities/drone/drone_rods.tga
		rgbGen lightingDiffuse
	}
	{
		map models/abilities/drone/drone_sheen.tga
		blendFunc GL_ONE GL_ONE
		tcGen environment
		rgbGen identity
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


FONTS = ["Roboto-Regular.ttf", "Roboto-Bold.ttf", "Roboto-Black.ttf", "RobotoCondensed-Regular.ttf",
         "RobotoCondensed-Bold.ttf", "RobotoMono-Regular.ttf",
         # hud.md 10.13: Tektur for the HUD (OFL), Play for the chat (OFL),
         # Ubuntu Mono for the console (Ubuntu Font Licence); Roboto stays as
         # the fallback
         "Tektur-Medium.ttf", "Tektur-SemiBold.ttf", "Tektur-Bold.ttf", "Play-Regular.ttf", "Play-Bold.ttf",
         "UbuntuMono-Regular.ttf"]
LICENCES = ["LICENSE-Roboto.txt", "LICENSE-Tektur.txt", "LICENSE-Play.txt", "LICENSE-UbuntuMono.txt"]


def fonts(out):
    """The Roboto fonts (Apache-2.0) from $TREMOR_FONTS, packed unmodified with
    their licence; without them the menus and HUD fall back to the bitmap
    fonts. Font files stay out of the repository (assets/baseq3/ui/fonts)."""
    here = os.path.dirname(os.path.abspath(__file__))
    folder = os.environ.get("TREMOR_FONTS", os.path.join(os.path.expanduser("~"), "projects/quake/fonts"))
    dest = os.path.join(out, "ui/fonts")
    found = [f for f in FONTS if os.path.isfile(os.path.join(folder, f))]
    if not found:
        print("make-models: no Roboto fonts in %s; menus and HUD use the fallback fonts" % folder)
        return
    os.makedirs(dest, exist_ok=True)
    for f in found:
        with open(os.path.join(folder, f), "rb") as i, open(os.path.join(dest, f), "wb") as o:
            o.write(i.read())
    for name in LICENCES:
        licence = os.path.join(here, "../../assets/baseq3/ui/fonts", name)
        with open(licence, "rb") as i, open(os.path.join(dest, name), "wb") as o:
            o.write(i.read())


def main():
    out = sys.argv[1]
    src = os.path.join(out, "assets-src")
    hourglass(src)
    orb(src)
    claw(src)
    totem(src)
    totem_aura(src)
    turret(src)
    drone(src)
    barrier(src)
    acid(src)
    # the champions' looks are painted on the player's machine (bake-looks.py),
    # never packed here: clear any left from older builds
    shutil.rmtree(os.path.join(src, "models/players"), ignore_errors=True)
    if os.path.isfile(os.path.join(src, "scripts/tremor-skins.shader")):
        os.remove(os.path.join(src, "scripts/tremor-skins.shader"))
    os.makedirs(os.path.join(src, "icons"), exist_ok=True)
    weapon_icons(src)
    hud_icons(src)
    hud_badges(src)
    portraits(src)
    champion_portraits.write_portraits(src)
    sounds(src)
    texture(os.path.join(src, "gfx/abilities/plasmatrail.tga"), 128, 64, trail, False)
    texture(os.path.join(src, "gfx/tremor/blood_edge.tga"), 256, 256, blood_edge)
    texture(os.path.join(src, "gfx/tremor/thrust.tga"), 128, 128, thrust, False)
    texture(os.path.join(src, "gfx/tremor/thrust_flicker.tga"), 128, 128, thrust_flicker, False)
    texture(os.path.join(src, "gfx/abilities/drone_blast.tga"), 128, 128, drone_blast)
    os.makedirs(os.path.join(src, "scripts"), exist_ok=True)
    with open(os.path.join(src, "scripts/tremor-assets.shader"), "w") as f:
        f.write(SHADERS)
    fonts(src)
    with zipfile.ZipFile(os.path.join(out, "tremor-assets.pk3"), "w", zipfile.ZIP_DEFLATED) as z:
        for root, _, files in os.walk(src):
            for name in sorted(files):
                full = os.path.join(root, name)
                z.write(full, os.path.relpath(full, src))


if __name__ == "__main__":
    main()
