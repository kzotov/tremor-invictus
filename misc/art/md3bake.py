"""Light and shade from the player's own copy of a model: for each texel
of one of the model's textures, how much it faces a light from above and
in front, and how open it is to the sky. Run on the player's machine by
bake-form.py only; nothing it makes is kept in the repository or packed
into tremor-assets.pk3. Stdlib only."""
import math, os, struct, zipfile

LIGHT = (0.55, 0.25, 0.8)


class Files:
    """The player's files: loose ones and those in .pk3s, later packs winning."""

    def __init__(self, folder):
        self.folder, self.packed = folder, {}
        if os.path.isdir(folder):
            for name in sorted(os.listdir(folder)):
                if name.lower().endswith(".pk3"):
                    z = zipfile.ZipFile(os.path.join(folder, name))
                    for n in z.namelist():
                        self.packed[n.lower()] = (z, n)

    def read(self, path):
        loose = os.path.join(self.folder, path)
        if os.path.isfile(loose):
            return open(loose, "rb").read()
        hit = self.packed.get(path.lower())
        return hit[0].read(hit[1]) if hit else None


def read_md3(data):
    """(tags per frame {name: (origin, axes)}, [(surface, tris, st, xyz per frame, normals per frame)])"""
    nf, nt, ns = struct.unpack_from("<3i", data, 76)
    otg, osf = struct.unpack_from("<2i", data, 96)
    tags = []
    for f in range(nf):
        fr = {}
        for t in range(nt):
            o = otg + (f * nt + t) * 112
            v = struct.unpack_from("<12f", data, o + 64)
            fr[data[o:o + 64].split(b"\0")[0].decode()] = (v[0:3], (v[3:6], v[6:9], v[9:12]))
        tags.append(fr)
    surfs, o = [], osf
    for _ in range(ns):
        name = data[o + 4:o + 68].split(b"\0")[0].decode()
        snf, _, nv, ntr, otr, _, ost, oxyz, end = struct.unpack_from("<9i", data, o + 72)
        tris = [struct.unpack_from("<3i", data, o + otr + 12 * i) for i in range(ntr)]
        st = [struct.unpack_from("<2f", data, o + ost + 8 * i) for i in range(nv)]
        xyz, nor = [], []
        for f in range(snf):
            fx, fn = [], []
            for i in range(nv):
                x, y, z, n = struct.unpack_from("<3hH", data, o + oxyz + (f * nv + i) * 8)
                fx.append((x / 64.0, y / 64.0, z / 64.0))
                lat, lng = (n >> 8) * 2 * math.pi / 255, (n & 255) * 2 * math.pi / 255
                fn.append((math.cos(lat) * math.sin(lng), math.sin(lat) * math.sin(lng), math.cos(lng)))
            xyz.append(fx)
            nor.append(fn)
        surfs.append((name, tris, st, xyz, nor))
        o += end
    return tags, surfs


def standing_frames(cfg):
    """The torso's stand frame and the legs' idle frame, from animation.cfg."""
    first = {}
    for line in cfg.splitlines():
        parts = line.split("//")
        if len(parts) > 1 and parts[0].split() and parts[0].split()[0].isdigit():
            first[parts[1].strip().split()[0]] = int(parts[0].split()[0])
    skip = first["LEGS_WALKCR"] - first["TORSO_GESTURE"]
    return first["TORSO_STAND"], first["LEGS_IDLE"] - skip


def bake(files, model, skin, texture, n=128, open_n=48):
    """(lit, open), each an n x n list of rows of 0..1, or None when the
    model is not among the player's files (a Files)."""
    base = "models/players/%s/" % model
    parts = {p: files.read(base + p + ".md3") for p in ("lower", "upper")}
    cfg = files.read(base + "animation.cfg")
    if not all(parts.values()) or not cfg:
        return None
    torso, legs = standing_frames(cfg.decode("latin-1"))
    lt, ls = read_md3(parts["lower"])
    ut, us = read_md3(parts["upper"])
    org, axes = lt[min(legs, len(lt) - 1)]["tag_torso"]

    def world(p):
        return tuple(org[i] + sum(p[j] * axes[j][i] for j in range(3)) for i in range(3))

    def turn(v):
        return tuple(sum(v[j] * axes[j][i] for j in range(3)) for i in range(3))

    wears = {}
    for p in ("lower", "upper"):
        text = files.read(base + "%s_%s.skin" % (p, skin)) or b""
        for line in text.decode("latin-1").splitlines():
            if "," in line:
                s, t = line.split(",", 1)
                wears[(p, s.strip().lower())] = t.strip().replace("\\", "/").split("/")[-1].lower() == texture.lower()
    light = [x / math.sqrt(sum(y * y for y in LIGHT)) for x in LIGHT]
    lit = [[None] * n for _ in range(n)]
    pos = [[None] * n for _ in range(n)]
    nrm = [[None] * n for _ in range(n)]
    elements = []
    for p, surfs, fr, xf, tf in (("lower", ls, legs, lambda q: q, lambda q: q), ("upper", us, torso, world, turn)):
        for name, tris, st, xyz, nor in surfs:
            f = min(fr, len(xyz) - 1)
            P, N = [xf(q) for q in xyz[f]], [tf(q) for q in nor[f]]
            for a, b, c in tris:
                e1 = [P[b][i] - P[a][i] for i in range(3)]
                e2 = [P[c][i] - P[a][i] for i in range(3)]
                cr = (e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0])
                area = math.sqrt(sum(x * x for x in cr)) / 2
                if area > 1e-6:
                    elements.append((tuple((P[a][i] + P[b][i] + P[c][i]) / 3 for i in range(3)),
                                     tuple(x / (2 * area) for x in cr), area))
            if not wears.get((p, name.lower())):
                continue
            for a, b, c in tris:
                uv = [(st[i][0] * n, st[i][1] * n) for i in (a, b, c)]
                den = (uv[1][1] - uv[2][1]) * (uv[0][0] - uv[2][0]) + (uv[2][0] - uv[1][0]) * (uv[0][1] - uv[2][1])
                if abs(den) < 1e-9:
                    continue
                x0, x1 = max(0, int(min(q[0] for q in uv))), min(n, int(math.ceil(max(q[0] for q in uv))))
                y0, y1 = max(0, int(min(q[1] for q in uv))), min(n, int(math.ceil(max(q[1] for q in uv))))
                for y in range(y0, y1):
                    for x in range(x0, x1):
                        px, py = x + 0.5, y + 0.5
                        l1 = ((uv[1][1] - uv[2][1]) * (px - uv[2][0]) + (uv[2][0] - uv[1][0]) * (py - uv[2][1])) / den
                        l2 = ((uv[2][1] - uv[0][1]) * (px - uv[2][0]) + (uv[0][0] - uv[2][0]) * (py - uv[2][1])) / den
                        l3 = 1 - l1 - l2
                        if min(l1, l2, l3) < -0.15 or (p == "upper" and lit[y][x] is not None and pos[y][x][3] == "lower"):
                            continue
                        w = (l1, l2, l3)
                        v = [sum(w[k] * N[i][j] for k, i in enumerate((a, b, c))) for j in range(3)]
                        m = math.sqrt(sum(q * q for q in v)) or 1
                        v = [q / m for q in v]
                        lit[y][x] = max(0.0, min(1.0, sum(v[j] * light[j] for j in range(3)) * 0.5 + 0.5))
                        pos[y][x] = tuple(sum(w[k] * P[i][j] for k, i in enumerate((a, b, c))) for j in range(3)) + (p,)
                        nrm[y][x] = v
    # how open to the sky, on a coarser grid: the elements facing a texel and in front of it shut it in
    shut = [[None] * open_n for _ in range(open_n)]
    for gy in range(open_n):
        for gx in range(open_n):
            y, x = int((gy + 0.5) * n / open_n), int((gx + 0.5) * n / open_n)
            if pos[y][x] is None:
                continue
            q = [pos[y][x][i] + nrm[y][x][i] * 0.3 for i in range(3)]
            v, occ = nrm[y][x], 0.0
            for c, en, area in elements:
                d = (c[0] - q[0], c[1] - q[1], c[2] - q[2])
                r2 = d[0] * d[0] + d[1] * d[1] + d[2] * d[2] + 1e-6
                facing = (d[0] * v[0] + d[1] * v[1] + d[2] * v[2])
                if facing <= 0:
                    continue
                back = -(d[0] * en[0] + d[1] * en[1] + d[2] * en[2])
                if back <= 0:
                    continue
                r = math.sqrt(r2)
                occ += area * (facing / r) * (back / r) / (math.pi * r2 + area)
            shut[gy][gx] = max(0.0, min(1.0, 1 - occ * 0.9))
    return fill(lit, 0.6), fill(shut, 1.0)


def fill(grid, empty):
    """Spread the known texels into the gaps around them, then soften."""
    n = len(grid)
    for _ in range(8):
        out = [row[:] for row in grid]
        for y in range(n):
            for x in range(n):
                if grid[y][x] is None:
                    near = [grid[j][i] for i, j in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1))
                            if 0 <= i < n and 0 <= j < n and grid[j][i] is not None]
                    if near:
                        out[y][x] = sum(near) / len(near)
        grid = out
    grid = [[empty if v is None else v for v in row] for row in grid]
    return [[sum(grid[j][i] for i in range(max(0, x - 1), min(n, x + 2)) for j in range(max(0, y - 1), min(n, y + 2)))
             / ((min(n, x + 2) - max(0, x - 1)) * (min(n, y + 2) - max(0, y - 1))) for x in range(n)] for y in range(n)]


def sample(grid, u, v):
    n = len(grid)
    x, y = u * n - 0.5, v * n - 0.5
    x0, y0 = max(0, min(n - 1, int(math.floor(x)))), max(0, min(n - 1, int(math.floor(y))))
    x1, y1 = min(n - 1, x0 + 1), min(n - 1, y0 + 1)
    fx, fy = min(1.0, max(0.0, x - x0)), min(1.0, max(0.0, y - y0))
    top = grid[y0][x0] + (grid[y0][x1] - grid[y0][x0]) * fx
    bottom = grid[y1][x0] + (grid[y1][x1] - grid[y1][x0]) * fx
    return top + (bottom - top) * fy
