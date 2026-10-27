"""Raine on major: a survivor's suit, slate grey over the leathers and the
bare skin, with cold blue lights in its seams, a blue-lit badge over the
shoulder emblem, and the hair darkened. Team versions keep the team's
paint, with the skin covered.

The skin, the big pieces whose rims are lit, the badge and the hair are
worked out on the player's machine from the player's own model and its
paint."""
import colorsys, math
from skins import Sheet, Look, LabelMap, painted
from tremorart import Fbm, clamp

# ---------------------------------------------------------------- the body sheet (major.tga)

SLATE = (0.14, 0.16, 0.19)
LIGHT = {"": (0.3, 0.7, 1.0), "_red": (1.0, 0.35, 0.2), "_blue": (0.35, 0.7, 1.0)}
SEAM = 3.0                              # how far in from a big piece's rim its light runs, in 128ths

_places = {}


def body_places(geo, n=128):
    """(skin, seams, badge) on the body sheet. skin(t): the pieces the
    model paints light and pale on average, the bare skin. seams: a
    LabelMap, "a" on the big pieces of the suit (each a twentieth of the
    sheet or more), for lights just inside their rims. badge(t): how far a
    texel is from the badge's middle, in parts of its radius: on the
    right of the chest, a little above halfway up the torso."""
    if id(geo) in _places:
        return _places[id(geo)]
    sums, total = {}, 0
    for y in range(n):
        for x in range(n):
            u, v = (x + 0.5) / n, (y + 0.5) / n
            t = geo.at(u, v)
            if t.covered:
                h, s, val = colorsys.rgb_to_hsv(*(geo.colour(u, v) or (0, 0, 0)))
                e = sums.setdefault(t.island, [0.0, 0.0, 0])
                e[0] += s
                e[1] += val
                e[2] += 1
                total += 1
    pale = {k for k, (s, val, c) in sums.items() if val / c > 0.38 and s / c < 0.6}
    big = {k for k, e in sums.items() if e[2] > 0.035 * total}
    seams = LabelMap.painted(lambda t, u, v: "a" if t.covered and t.island in big and t.island not in pale
                             else "z", n, cap=8)(geo)
    torso = [t for row in geo.grid for t in row if t and t.covered and t.surf == "u_torso"]
    y0, y1 = min(t.y for t in torso), max(t.y for t in torso)
    z0, z1 = min(t.z for t in torso), max(t.z for t in torso)
    gy, gz = y0 + 0.22 * (y1 - y0), z0 + 0.58 * (z1 - z0)
    c = min((t for t in torso if t.nrm[0] > 0.3), key=lambda t: math.hypot(t.y - gy, t.z - gz)).pos
    r = 0.17 * (y1 - y0)

    def badge(t):
        if t.surf != "u_torso":
            return 1e9
        return math.sqrt(sum((t.pos[i] - c[i]) ** 2 for i in range(3))) / r
    _places[id(geo)] = out = ((lambda t: t.island in pale), seams, badge)
    return out


def seam(m, u, v):
    """Distance in 128ths from the light just inside a big piece's rim."""
    return abs(m.edge(u, v) - SEAM) if m.label(u, v) == "a" else 1e9


def body_grade(u, v):
    return [0.4, 0.42, 0.48, 1]


def body_detail_fn(team):
    n = Fbm(1511, 4, 6)

    @painted
    def make(geo):
        skin, seams, badge = body_places(geo)

        def f(u, v):
            t = geo.at(u, v)
            k = 0.8 + 0.4 * n.at(u * 5, v * 5)
            if skin(t):
                return [x * k for x in SLATE] + [0.93]
            if team:
                return [0, 0, 0, 0]
            if badge(t) < 1:
                return [0.06, 0.07, 0.09, 1.0]
            if seam(seams, u, v) < 1.0:
                return [0.03, 0.04, 0.05, 0.9]
            return [x * k for x in SLATE] + [0.6]
        return f
    return make


def body_glow_fn(colour, team):
    @painted
    def make(geo):
        skin, seams, badge = body_places(geo)

        def f(u, v):
            k = 0.0
            if not team:
                k = 1 - seam(seams, u, v) / 0.5
                if 0.45 < badge(geo.at(u, v)) < 0.7:
                    k = max(k, 0.9)
            k = clamp(k)
            return [colour[0] * k, colour[1] * k, colour[2] * k, 1]
        return f
    return make


# ---------------------------------------------------------------- the head (major_h.tga): darker hair

@painted
def head_grade(geo):
    ts = [t for row in geo.grid for t in row if t and t.covered]
    z0, z1 = min(t.z for t in ts), max(t.z for t in ts)
    x0, x1 = min(t.x for t in ts), max(t.x for t in ts)
    # the face: the front of the head below the brow; the rest is hair
    brow, cheek = z0 + 0.72 * (z1 - z0), x1 - 0.29 * (x1 - x0)

    def f(u, v):
        t = geo.at(u, v)
        if t.z > brow or t.x < cheek:
            return [0.32, 0.32, 0.34, 1]
        return [0.5, 0.5, 0.5, 1]
    return f


def look():
    team = body_detail_fn(True)
    return Look("major", "raine", {
        "body": Sheet({"": "major", "_red": "red", "_blue": "blue"},
                      grade={"": body_grade, "_red": None, "_blue": None},
                      detail={"": body_detail_fn(False), "_red": team, "_blue": team},
                      glow={"": body_glow_fn(LIGHT[""], False), "_red": None, "_blue": None}, size=512),
        "head": Sheet({"": "major_h", "_red": "red_h", "_blue": "blue_h"}, grade=head_grade, size=128),
    }, {"lower": {"l_legs": "body", "l_lfoot": "body", "l_rfoot": "body"},
        "upper": {"u_torso": "body", "u_larm": "body", "u_rarm": "body"},
        "head": {"h_head": "head"}},
        {"lower": ["tag_torso"], "upper": ["tag_head", "tag_weapon", "tag_torso"], "head": ["tag_head"]})
