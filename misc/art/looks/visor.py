"""Visor in his own colours: the steel and the old plates tinted orange
with the model's own shading kept, shadows in the creases, chipped and
scratched edges, the hide turned to pale skin, the dark joints kept dark
gunmetal, and the visor an orange face mask with a dark slit. Team
versions tint the armour in the team's colour."""
import math
from skins import Sheet, Look, LabelMap, painted
from tremorart import Fbm, Noise, clamp


def mean_colour(geo, u, v, n=128):
    """The model's own colour round a texel of an n grid, a little blurred
    (a mid brown when the image is not one we can read)."""
    cs = [geo.colour(u + i * 0.75 / n, v + j * 0.75 / n) for i in (-1, 0, 1) for j in (-1, 0, 1)]
    if cs[0] is None:
        return (0.3, 0.18, 0.08)
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


def heights(geo, part):
    """The lowest and highest z of a part's texels on the sheet."""
    zs = [t.z for row in geo.grid for t in row if t is not None and t.covered and t.part == part]
    return min(zs), max(zs)


def material(geo, shoulder):
    """What each texel of visor.tga is, from where it sits on the model
    and the model's own colour there: o armour, s skin, d joints, k shadow
    (left alone). The legs are all armour. On the torso: out on the arms
    (8 units or more from the body's axis) the hide is where the colour
    is a saturated brown, short of the right shoulder plate that rises
    above shoulder (a height); nearer the axis, where the colour turns a
    deep red brown. On the head the hide is all but the front and the
    crown, which are the mask."""
    def rule(u, v):
        t, (r, g, b) = geo.at(u, v), mean_colour(geo, u, v)
        hi, lo = max(r, g, b), min(r, g, b)
        lum, sat = (r + g + b) / 3, (hi - lo) / (hi + 1e-6)
        if lum < 0.05:
            return "k"
        skin = False
        if t.part == "upper":
            if math.hypot(t.x, t.y) >= 8:
                skin = sat >= 0.46 and (t.z < shoulder or t.y > -1.25)
            else:
                skin = g < 0.35 * r
        elif t.part == "head":
            skin = t.nrm[0] < 0.5 and t.nrm[2] < 0.74
        if skin:
            return "k" if lum < 0.06 else "s"
        if lum < 0.17 and sat < 0.35:
            return "d"
        return "o"
    return rule


MAPS = {}


def label_map(geo):
    """The sheet's LabelMap, made once per sheet."""
    if id(geo) not in MAPS:
        n, (lo, hi) = 128, heights(geo, "upper")
        rule = material(geo, lo + 0.8 * (hi - lo))
        rows = [[rule((x + 0.5) / n, (y + 0.5) / n) for x in range(n)] for y in range(n)]
        MAPS[id(geo)] = LabelMap(["".join(row) for row in majority(rows)])
    return MAPS[id(geo)]


SLIT = (0.677, 0.781)                            # the slit's band, as a share of the head's height
ARMOUR = {"": (0.86, 0.4, 0.1), "_red": (0.78, 0.12, 0.08), "_blue": (0.14, 0.3, 0.78)}
TINT = {"": (1.0, 0.56, 0.2), "_red": (0.95, 0.24, 0.17), "_blue": (0.3, 0.42, 0.95)}
SKIN = (0.8, 0.62, 0.5)
STEEL = (0.7, 0.68, 0.64)


def grade_fn(variant):
    n = Fbm(2119, 4, 6)

    @painted
    def make(geo):
        labels = label_map(geo)

        def f(u, v):
            c, k = labels.label(u, v), labels.shade(u, v)
            k *= 0.88 + 0.24 * n.at(u * 5, v * 5)               # grime
            if c == "o":
                return [clamp(z * k) for z in TINT[variant]] + [1]
            if c == "s":
                return [0.9 * k, 0.8 * k, 0.84 * k, 1]
            if c == "d":
                return [0.3 * k, 0.3 * k, 0.32 * k, 1]
            return [0.45 * k, 0.45 * k, 0.45 * k, 1]
        return f
    return make


def slit(geo):
    """The mask's slit: the front of the head across a band a little above
    its middle (as a share of the head's height)."""
    lo, hi = heights(geo, "head")

    def at(u, v):
        t = geo.at(u, v)
        if t.part != "head" or t.nrm[0] < 0.5 or max(abs(t.nrm[1]), t.nrm[2]) > 0.5:
            return False                                    # not the mask's front
        return SLIT[0] <= (t.z - lo) / (hi - lo) <= SLIT[1]
    return at


def detail_fn(variant):
    n, fine, scratch = Fbm(2117, 4, 6), Noise(2121, 256, 256), Noise(2123, 64, 512)

    @painted
    def make(geo):
        labels, in_slit = label_map(geo), slit(geo)

        def f(u, v):
            if in_slit(u, v):
                return [0.03, 0.03, 0.03, 1]
            x, y = u * 768, v * 768
            c = labels.label(u, v)
            k = 0.85 + 0.3 * n.at(u * 8, v * 8)
            if c == "o":
                e = labels.edge(u, v)
                if e < 1.6 and fine.at(x * 0.6, y * 0.6) > 0.5:
                    return list(STEEL) + [0.8]                   # chipped paint on the edge
                s = scratch.at(u * 12, v * 90)
                if abs(s - 0.5) < 0.012:
                    return list(STEEL) + [0.5]                   # a scratch
                if n.at(u * 14 + 3, v * 14) > 0.7:
                    return [0.08, 0.06, 0.05, 0.45]              # a grime spot
                return [clamp(z * k) for z in ARMOUR[variant]] + [0.3]
            if c == "s":
                return [clamp(z * k) for z in SKIN] + [0.62]
            return [0, 0, 0, 0]
        return f
    return make


def look():
    return Look("visor", "visor", {
        "body": Sheet({"": "visor", "_red": "visor", "_blue": "visor"},
                      grade={v: grade_fn(v) for v in TINT},
                      detail={v: detail_fn(v) for v in TINT}, size=512),
    }, {"lower": {"l_legs": "body"},
        "upper": {"u_torso": "body"},
        "head": {"h_head": "body"}},
        {"lower": ["tag_torso"], "upper": ["tag_head", "tag_torso", "tag_weapon"], "head": ["tag_head"]})
