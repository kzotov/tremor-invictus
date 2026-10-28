"""Slash in her own colours: an olive-gold corset with boning, orange
shorts with stitched hems, olive legs, dark knee pads, bracers and
shoulder pad, tan skate boots, pale skin, dark hair and a green jet
under each skate; the colours are tinted over the model's own shading, with
shadows in the creases and worn edges. Team versions paint the shorts
in the team's colour."""
import math
from skins import Sheet, Look, LabelMap, painted
from tremorart import Fbm, Noise, clamp


def mean_colour(geo, u, v, n=128):
    """The model's own colour round a texel of an n grid, a little blurred
    (a mid olive when the image is not one we can read)."""
    cs = [geo.colour(u + i * 0.75 / n, v + j * 0.75 / n) for i in (-1, 0, 1) for j in (-1, 0, 1)]
    if cs[0] is None:
        return (0.3, 0.25, 0.1)
    return tuple(sum(c[k] for c in cs) / 9 for k in range(3))


def majority(rows, passes=2):
    """Each letter takes the one most of its 3 x 3 neighbours have, when
    at least 5 of them have it: clears the speckle."""
    n = len(rows)
    for _ in range(passes):
        out = [row[:] for row in rows]
        for y in range(n):
            for x in range(n):
                count = {}
                for j in (-1, 0, 1):
                    for i in (-1, 0, 1):
                        if 0 <= x + i < n and 0 <= y + j < n:
                            c = rows[y + j][x + i]
                            count[c] = count.get(c, 0) + 1
                c = max(count, key=count.get)
                if count[c] >= 5:
                    out[y][x] = c
        rows = out
    return rows


def pieces(geo):
    """For each UV island of the sheet: (mean x, mean y, lowest z,
    highest z) of its texels on the model."""
    acc = {}
    for row in geo.grid:
        for t in row:
            if t is not None and t.covered:
                a = acc.setdefault(t.island, [0, 0.0, 0.0, 1e9, -1e9])
                a[0] += 1
                a[1] += t.x
                a[2] += t.y
                a[3], a[4] = min(a[3], t.z), max(a[4], t.z)
    return {i: (a[1] / a[0], a[2] / a[0], a[3], a[4]) for i, a in acc.items()}


def material(geo):
    """What each texel of slash.tga is, from where it and its piece sit on
    the model: c corset, p shorts, l legs, b boots, d pads, s skin, k
    shadow (left alone). On the legs: pieces wholly below the ankles are
    the boots, pieces wholly about the knee the knee pads, the rest olive
    legs with the shorts above the top of the thighs (heights as shares of
    the legs'). On the torso: pieces centred within 4 units of the body's
    axis are the corset, pieces out from it are the arms, bare behind the
    shoulders and padded (bracers, gloves, shoulder pad) in front. Pale,
    unsaturated texels are skin anywhere."""
    isl = pieces(geo)
    legs = [t.z for row in geo.grid for t in row if t is not None and t.covered and t.part == "lower"]
    lo, h = min(legs), max(legs) - min(legs)

    def rule(u, v):
        t, (r, g, b) = geo.at(u, v), mean_colour(geo, u, v)
        most = max(r, g, b)
        lum, sat = (r + g + b) / 3, (most - min(r, g, b)) / (most + 1e-6)
        if lum < 0.05:
            return "k"
        if sat < 0.25 and lum > 0.5:
            return "s"
        cx, cy, bottom, top = isl.get(t.island, (t.x, t.y, t.z, t.z))
        if t.part == "lower":
            if top < lo + 0.16 * h:
                return "b"
            if lo + 0.42 * h < bottom and top < lo + 0.61 * h:
                return "d"
            return "p" if t.z > lo + 0.71 * h else "l"
        if math.hypot(cx, cy) < 4:
            return "c"
        return "s" if cx < -1.8 else "d"
    return rule


MAPS = {}


def label_map(geo):
    """The sheet's LabelMap, made once per sheet."""
    if id(geo) not in MAPS:
        n, rule = 128, material(geo)
        rows = [[rule((x + 0.5) / n, (y + 0.5) / n) for x in range(n)] for y in range(n)]
        MAPS[id(geo)] = LabelMap(["".join(row) for row in majority(rows)])
    return MAPS[id(geo)]


SHORTS = {"": (0.9, 0.42, 0.08), "_red": (0.75, 0.1, 0.07), "_blue": (0.12, 0.28, 0.75)}
COLOURS = {"c": (0.55, 0.47, 0.12), "l": (0.36, 0.35, 0.12), "b": (0.66, 0.46, 0.22),
           "d": (0.1, 0.1, 0.1), "s": (0.86, 0.82, 0.8)}
ALPHA = {"c": 0.5, "p": 0.45, "l": 0.5, "b": 0.45, "d": 0.45, "s": 0.35}
# the grade under the paint: the same colours, as a x2 multiply
TINT = {"c": (0.75, 0.62, 0.22), "l": (0.55, 0.53, 0.25), "b": (0.85, 0.6, 0.32),
        "d": (0.25, 0.25, 0.26), "s": (0.62, 0.6, 0.6)}
TEAM_TINT = {"": (1.0, 0.55, 0.18), "_red": (0.95, 0.2, 0.15), "_blue": (0.25, 0.4, 0.95)}
HAIR = 0.39                     # the hairline, as a share of the way up the head
HAIR_COLOUR = (0.07, 0.05, 0.05)


def body_grade_fn(variant):
    n = Fbm(2120, 4, 6)

    @painted
    def make(geo):
        labels = label_map(geo)

        def f(u, v):
            c, k = labels.label(u, v), labels.shade(u, v, 0.5)
            k *= 0.88 + 0.24 * n.at(u * 5, v * 5)
            if c == "k":
                return [0.5 * k, 0.5 * k, 0.5 * k, 1]
            tint = TEAM_TINT[variant] if c == "p" else TINT[c]
            return [clamp(z * k) for z in tint] + [1]
        return f
    return make


def body_detail_fn(variant):
    n, fine = Fbm(2116, 4, 6), Noise(2122, 256, 256)

    @painted
    def make(geo):
        labels = label_map(geo)

        def f(u, v):
            c = labels.label(u, v)
            if c == "k":
                return [0, 0, 0, 0]
            x, y, e = u * 512, v * 512, labels.edge(u, v)
            k = 0.85 + 0.3 * n.at(u * 8, v * 8)
            colour = SHORTS[variant] if c == "p" else COLOURS[c]
            if c == "c" and (x % 22) < 1.6:
                return [0.12, 0.1, 0.03, 0.7]                    # the corset's boning
            if c == "p" and 1.2 < e < 2.2 and (x + y) % 9 < 4.5:
                return [0.95, 0.85, 0.6, 0.7]                    # stitched hem
            if c in "pbd" and e < 1.2 and fine.at(x, y) > 0.55:
                return [0.8, 0.75, 0.65, 0.5]                    # worn edge
            if c == "d" and n.at(u * 20, v * 20) > 0.62:
                return [0.35, 0.35, 0.36, 0.5]                   # a scuff on the pads
            return [clamp(z * k) for z in colour] + [ALPHA[c]]
        return f
    return make


def hair(geo):
    """The hair on the head sheet, clear of the face and the goggles: the
    head above a hairline over the goggles that slopes down towards the
    back, and the locks hanging at the sides behind the face."""
    zs = [t.z for row in geo.grid for t in row if t is not None and t.covered]
    xs = [t.x for row in geo.grid for t in row if t is not None and t.covered]
    line, cheek = min(zs) + HAIR * (max(zs) - min(zs)), max(xs) - 2.8

    def at(u, v):
        t = geo.at(u, v)
        if t.z - 0.2 * t.x >= line:
            return t.nrm[2] < 0.97
        return abs(t.nrm[1]) > 0.93 and t.x < cheek and t.nrm[2] < -0.03
    return at


@painted
def head_detail(geo):
    n, on_hair = Fbm(2118, 4, 6), hair(geo)

    def f(u, v):
        if on_hair(u, v):
            k = 0.8 + 0.4 * n.at(u * 30, v * 6)
            return [z * k for z in HAIR_COLOUR] + [0.8]
        return [0, 0, 0, 0]
    return f


@painted
def skate_jet(geo):
    """The jet under each skate, painted where the card sits on the model:
    from a white-hot nozzle at the top of the card down to the sole, a
    tapering green flame, ragged with noise, and nothing below the sole
    (the card's lower half is under the floor)."""
    pts = [t.pos for row in geo.grid for t in row if t is not None and t.covered]
    if not pts:
        return lambda u, v: [0, 0, 0, 1]
    top, bottom = max(p[2] for p in pts), min(p[2] for p in pts)
    sole = (top + bottom) * 0.5
    ys = [p[1] for p in pts]
    mid, half = (max(ys) + min(ys)) * 0.5, max(1.0, (max(ys) - min(ys)) * 0.5)
    n = Noise(1907, 16, 16)

    def f(u, v):
        t = geo.at(u, v)
        if t is None or not t.covered:
            return [0, 0, 0, 1]
        down = (top - t.z) / max(1.0, top - sole)          # 0 at the nozzle, 1 at the sole
        if down > 1.05:
            return [0, 0, 0, 1]
        across = abs(t.y - mid) / half
        width = 0.45 * (1 - 0.75 * min(1.0, down)) * (0.85 + 0.3 * n.at(u * 8, v * 3))
        k = max(0.0, 1 - across / max(0.05, width)) * (1 - max(0.0, down - 0.8) / 0.25)
        core = math.exp(-(down / 0.18) ** 2) * math.exp(-(across / 0.18) ** 2)
        return [clamp(0.25 * k + core), clamp(0.95 * k + core), clamp(0.45 * k + core), 1]
    return f


def look():
    return Look("slash", "slash", {
        "body": Sheet({"": "slash", "_red": "slash", "_blue": "slash"},
                      grade={v: body_grade_fn(v) for v in SHORTS},
                      detail={v: body_detail_fn(v) for v in SHORTS}, size=512),
        "head": Sheet({"": "slash_h", "_red": "slash_h", "_blue": "slash_h"}, detail=head_detail, size=256),
        "skate": Sheet({"": "slashskate", "_red": "slashskate", "_blue": "slashskate"}, detail=skate_jet, size=64, fx=True),
    }, {"lower": {"l_legs": "body", "l_skatel": "skate", "l_skater": "skate"},
        "upper": {"u_torso": "body"},
        "head": {"h_head": "head"}},
        {"lower": ["tag_torso"], "upper": ["tag_head", "tag_torso", "tag_weapon"], "head": ["tag_head"]})
