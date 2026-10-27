"""Where each texel of a model's texture sits on the model, worked out on
the player's machine from the player's own copy of the model, for the
looks to paint by: which part and surface it is on, where it is on the
standing body and which way it faces, and the colour the model's own
texture has there. Used by the local looks bake only (bake-looks.py);
nothing it reads or makes is kept in the repository or packed into
tremor-assets.pk3. Stdlib only.

Positions are in the standing model's own units: x forward, y to the
model's left, z up, the origin at the legs' origin (about the hips);
"height" is z.
"""
import math, os, struct, zlib
from md3bake import Files, read_md3, standing_frames


class Texel:
    """One texel: part ("lower", "upper" or "head"), surface name, island
    (a connected piece of the surface's UVs), position (x, y, z), normal,
    and covered (False for texels no surface uses; they take the
    nearest used texel's values)."""
    __slots__ = ("part", "surf", "island", "pos", "nrm", "covered")

    def __init__(self, part, surf, island, pos, nrm, covered=True):
        self.part, self.surf, self.island, self.pos, self.nrm, self.covered = part, surf, island, pos, nrm, covered

    x = property(lambda self: self.pos[0])
    y = property(lambda self: self.pos[1])
    z = property(lambda self: self.pos[2])


def _turn(axes, v):
    return tuple(sum(v[j] * axes[j][i] for j in range(3)) for i in range(3))


def _move(origin, axes, p):
    return tuple(origin[i] + sum(p[j] * axes[j][i] for j in range(3)) for i in range(3))


class Body:
    """A player model put together standing: the legs in their idle frame,
    the torso in its stand frame at the legs' tag_torso, the head (of
    head_model) at the torso's tag_head. parts: {part: [(surface, tris,
    st, positions, normals)]} in body space."""

    def __init__(self, files, model, head_model=None):
        base = "models/players/%s/" % model
        cfg = files.read(base + "animation.cfg")
        lower, upper = files.read(base + "lower.md3"), files.read(base + "upper.md3")
        head = files.read("models/players/%s/head.md3" % (head_model or model))
        if not (cfg and lower and upper and head):
            raise FileNotFoundError(model)
        torso, legs = standing_frames(cfg.decode("latin-1"))
        lt, ls = read_md3(lower)
        ut, us = read_md3(upper)
        _, hs = read_md3(head)
        legs, torso = min(legs, len(lt) - 1), min(torso, len(ut) - 1)
        to, ta = lt[legs]["tag_torso"]
        ho, ha = ut[torso]["tag_head"]
        ho = _move(to, ta, ho)
        ha = tuple(_turn(ta, a) for a in ha)
        self.parts = {}
        for part, surfs, frame, origin, axes in (("lower", ls, legs, (0, 0, 0), ((1, 0, 0), (0, 1, 0), (0, 0, 1))),
                                                  ("upper", us, torso, to, ta), ("head", hs, 0, ho, ha)):
            self.parts[part] = []
            for name, tris, st, xyz, nor in surfs:
                f = min(frame, len(xyz) - 1)
                self.parts[part].append((name.lower(), tris, st, [_move(origin, axes, p) for p in xyz[f]],
                                         [_turn(axes, n) for n in nor[f]]))


def _islands(tris, st):
    """For each vertex, the id of its UV island (vertices joined by
    triangles or by sharing a UV)."""
    parent = list(range(len(st)))

    def find(i):
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i
    seen = {}
    for i, uv in enumerate(st):
        key = (round(uv[0], 4), round(uv[1], 4))
        if key in seen:
            parent[find(i)] = find(seen[key])
        else:
            seen[key] = i
    for a, b, c in tris:
        parent[find(b)] = find(a)
        parent[find(c)] = find(a)
    return [find(i) for i in range(len(st))]


class SheetGeo:
    """The texels of one texture sheet of a body: wears is a set of (part,
    surface) that use it. at(u, v) gives the Texel there (n x n grid);
    colour(u, v) the model's own texture's rgb there (0..1), or None when
    the image is not one we can read."""

    def __init__(self, body, wears, n=256, image=None):
        self.n, self.image = n, image
        grid = [[None] * n for _ in range(n)]
        island_base = 0
        for part, surfs in body.parts.items():
            for name, tris, st, pos, nor in surfs:
                if (part, name) not in wears:
                    continue
                isl = _islands(tris, st)
                for a, b, c in tris:
                    uv = [(st[i][0] * n, st[i][1] * n) for i in (a, b, c)]
                    den = (uv[1][1] - uv[2][1]) * (uv[0][0] - uv[2][0]) + (uv[2][0] - uv[1][0]) * (uv[0][1] - uv[2][1])
                    if abs(den) < 1e-9:
                        continue
                    for y in range(max(0, int(min(q[1] for q in uv))), min(n, int(math.ceil(max(q[1] for q in uv))))):
                        for x in range(max(0, int(min(q[0] for q in uv))), min(n, int(math.ceil(max(q[0] for q in uv))))):
                            px, py = x + 0.5, y + 0.5
                            l1 = ((uv[1][1] - uv[2][1]) * (px - uv[2][0]) + (uv[2][0] - uv[1][0]) * (py - uv[2][1])) / den
                            l2 = ((uv[2][1] - uv[0][1]) * (px - uv[2][0]) + (uv[0][0] - uv[2][0]) * (py - uv[2][1])) / den
                            w = (l1, l2, 1 - l1 - l2)
                            if min(w) < -0.15:
                                continue
                            old = grid[y][x]
                            if old is not None and (min(w) < 0 or (old.part == "lower" and part != "lower")):
                                continue        # the legs win where parts share texels
                            p = tuple(sum(w[k] * pos[i][j] for k, i in enumerate((a, b, c))) for j in range(3))
                            v = [sum(w[k] * nor[i][j] for k, i in enumerate((a, b, c))) for j in range(3)]
                            m = math.sqrt(sum(q * q for q in v)) or 1
                            grid[y][x] = Texel(part, name, island_base + isl[a], p, tuple(q / m for q in v))
                island_base += len(st)
        # unused texels take their nearest used neighbour, marked uncovered
        for _ in range(n):
            todo = [(x, y) for y in range(n) for x in range(n) if grid[y][x] is None]
            if not todo or len(todo) == n * n:
                break
            fresh = []
            for x, y in todo:
                for i, j in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    t = grid[y + j][x + i] if 0 <= x + i < n and 0 <= y + j < n else None
                    if t is not None:
                        fresh.append((x, y, Texel(t.part, t.surf, t.island, t.pos, t.nrm, False)))
                        break
            for x, y, t in fresh:
                grid[y][x] = t
        self.grid = grid

    def at(self, u, v):
        n = self.n
        return self.grid[min(n - 1, max(0, int(v * n)))][min(n - 1, max(0, int(u * n)))]

    def colour(self, u, v):
        if self.image is None:
            return None
        w, h, px = self.image
        return px[min(h - 1, max(0, int(v * h)))][min(w - 1, max(0, int(u * w)))]


def read_tga(data):
    """(w, h, rows of (r, g, b) 0..1, top row first) of an uncompressed or
    run-length TGA, true colour or grey; None for anything else."""
    idlen, cmtype, kind = data[0], data[1], data[2]
    w, h, bits, desc = struct.unpack_from("<HHBB", data, 12)
    if cmtype or kind not in (2, 3, 10, 11) or bits not in (8, 24, 32):
        return None
    size, o, flat = bits // 8, 18 + idlen, []
    count = w * h
    if kind in (2, 3):
        raw = data[o:o + count * size]
        flat = [raw[i:i + size] for i in range(0, len(raw), size)]
    else:
        while len(flat) < count:
            head = data[o]
            o += 1
            if head & 0x80:
                flat += [data[o:o + size]] * ((head & 0x7f) + 1)
                o += size
            else:
                k = (head & 0x7f) + 1
                flat += [data[o + i * size:o + (i + 1) * size] for i in range(k)]
                o += k * size
    rows = []
    for y in range(h):
        row = []
        for x in range(w):
            p = flat[y * w + x]
            row.append((p[0] / 255.0,) * 3 if size == 1 else (p[2] / 255.0, p[1] / 255.0, p[0] / 255.0))
        rows.append(row)
    if not desc & 0x20:
        rows.reverse()                      # stored bottom row first
    return w, h, rows


class Context:
    """What the looks paint by on this machine: the player's files and the
    bodies and sheets made from them (kept, as several looks share them)."""

    def __init__(self, folder):
        self.files, self.bodies, self.sheets = Files(folder), {}, {}

    def body(self, model, head_model=None):
        key = (model, head_model)
        if key not in self.bodies:
            self.bodies[key] = Body(self.files, model, head_model)
        return self.bodies[key]

    def image(self, model, name):
        for ext in (".tga", ".jpg"):
            data = self.files.read("models/players/%s/%s%s" % (model, name, ext))
            if data:
                return read_tga(data) if ext == ".tga" else None
        return None

    def sheet(self, look, name, n=256):
        """The SheetGeo of a look's sheet."""
        key = (look.model, look.skin, name, n)
        if key not in self.sheets:
            sheet = look.sheets[name]
            wears = set()
            for part, surfs in look.surfaces.items():
                for sf, s in surfs.items():
                    if (s.get("", "") if isinstance(s, dict) else s) == name:
                        wears.add((part, sf.lower()))
            body = self.body(look.model, look.head_model if look.head_model != look.model else None)
            self.sheets[key] = SheetGeo(body, wears, n, self.image(sheet.model or look.model, sheet.base[""]))
        return self.sheets[key]
