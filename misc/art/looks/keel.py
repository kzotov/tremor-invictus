"""Keel in his own colours: tan armour lightened a little, the gore on
the forearms plated in tan and the meat at the neck to a brown
undersuit; the boots keep their blood. Team versions keep the team's
paint with his own face, the plated forearms and the brown neck."""
from skins import Sheet, Look, painted
from tremorart import Fbm

UNDERSUIT = (0.24, 0.16, 0.1)


def mean_colour(geo, u, v, n=128, r=3.0):
    """The model's own colour round a texel of an n grid, blurred over
    about r texels (black when the image is not one we can read)."""
    cs = [geo.colour(u + i * r / 2 / n, v + j * r / 2 / n) for i in (-1, 0, 1) for j in (-1, 0, 1)]
    if cs[0] is None:
        return (0.0, 0.0, 0.0)
    return tuple(sum(c[k] for c in cs) / 9 for k in range(3))


def gory(colour, k):
    r, g, b = colour
    return r > k * g and r > 0.12


def mask(geo, rule, n=128, grow=0):
    """An n x n grid of rule(texel, u, v), cleared of speckle (each texel
    goes with 5 or more of its 3 x 3 neighbours), then grown by grow
    texels; returns (u, v) -> bool."""
    rows = [[rule(geo.at((x + 0.5) / n, (y + 0.5) / n), (x + 0.5) / n, (y + 0.5) / n) for x in range(n)]
            for y in range(n)]

    def near(y, x):
        return [rows[y + j][x + i] for j in (-1, 0, 1) for i in (-1, 0, 1) if 0 <= x + i < n and 0 <= y + j < n]
    for _ in range(2):
        out = []
        for y in range(n):
            row = []
            for x in range(n):
                k = near(y, x)
                yes = k.count(True)
                row.append(True if yes >= 5 else False if len(k) - yes >= 5 else rows[y][x])
            out.append(row)
        rows = out
    for _ in range(grow):
        rows = [[any(near(y, x)) for x in range(n)] for y in range(n)]

    def at(u, v):
        return rows[min(n - 1, int(v * n))][min(n - 1, int(u * n))]
    return at


def body_grade(u, v):
    return [0.6, 0.55, 0.48, 1]


@painted
def body_detail(geo):
    """The gore on the torso's surface (it is all on the forearms), by its
    deep red, and a little round it; the legs keep theirs."""
    n = Fbm(2114, 4, 6)
    gore = mask(geo, lambda t, u, v: t.surf == "u_torso" and gory(mean_colour(geo, u, v), 2.0), grow=2)

    def f(u, v):
        if gore(u, v):
            k = 0.75 + 0.5 * n.at(u * 10, v * 10)
            return [0.5 * k, 0.4 * k, 0.3 * k, 0.8]
        return [0, 0, 0, 0]
    return f


@painted
def head_detail(geo):
    """The meat at the neck: the deep red at the back of the head and on
    the neck."""
    n = Fbm(2115, 4, 6)
    xs = [t.x for row in geo.grid for t in row if t is not None and t.covered and t.part == "head"]
    back = min(xs) + 0.3 * (max(xs) - min(xs))           # behind the ears
    meat = mask(geo, lambda t, u, v: t.x < back and gory(mean_colour(geo, u, v), 4.0), grow=1)

    def f(u, v):
        if meat(u, v):
            k = 0.8 + 0.4 * n.at(u * 8, v * 8)
            return [x * k for x in UNDERSUIT] + [0.85]
        return [0, 0, 0, 0]
    return f


def look():
    return Look("keel", "keel", {
        "body": Sheet({"": "keel", "_red": "red", "_blue": "blue"},
                      grade={"": body_grade, "_red": None, "_blue": None},
                      detail=body_detail, size=256),
        "head": Sheet({"": "keel_h", "_red": "keel_h", "_blue": "keel_h"}, detail=head_detail, size=256),
    }, {"lower": {"l_legs": "body"},
        "upper": {"u_torso": "body", "u_neck": "head"},
        "head": {"h_head": "head"}},
        {"lower": ["tag_torso"], "upper": ["tag_head", "tag_torso", "tag_weapon"], "head": ["tag_head"]})
