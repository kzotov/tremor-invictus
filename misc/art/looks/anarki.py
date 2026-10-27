"""Anarki in his own colours: a tanned, scarred junk-rat instead of the
grey skin, a near-black mohawk, the green shorts and olive trousers turned
to worn brown leather, rusted scrap armour, and brass-rimmed goggles. Team
versions keep the team's paint with the tan, the dark hair and the brass
goggles.

Where the skin, the cloth and the hair are is worked out on the player's
machine: the skin is the torso's pale pieces, the cloth is where the
model's own paint is green to olive, the hair is the top and back of the
head."""
import colorsys
from skins import Sheet, Look, painted
from tremorart import Fbm

TAN = [0.62, 0.48, 0.37, 1]
SCRAP = [0.58, 0.46, 0.4, 1]            # scrap with a rust cast


def cloth(c):
    """The model's own cloth: green to olive, not grey or rust."""
    h, s, val = colorsys.rgb_to_hsv(*c)
    return 0.11 < h < 0.45 and s > 0.35 and val > 0.06


def island_means(geo, n=128):
    """The mean hue, saturation and value of each island's own paint."""
    sums = {}
    for y in range(n):
        for x in range(n):
            u, v = (x + 0.5) / n, (y + 0.5) / n
            t = geo.at(u, v)
            if t.covered:
                s = sums.setdefault(t.island, [0.0, 0.0, 0.0, 0])
                for i, q in enumerate(colorsys.rgb_to_hsv(*(geo.colour(u, v) or (0, 0, 0)))):
                    s[i] += q
                s[3] += 1
    return {k: (s[0] / s[3], s[1] / s[3], s[2] / s[3]) for k, s in sums.items()}


def cloth_map(geo, n=128, r=3):
    """How much of the texels round each one are cloth, 0..1 on an n grid;
    a whole island is cloth when its paint is strong olive on average."""
    means = island_means(geo)
    olive = {k for k, (h, s, val) in means.items() if 0.09 < h < 0.45 and s > 0.6}
    hit = [[geo.at((x + 0.5) / n, (y + 0.5) / n).island in olive or
            cloth(geo.colour((x + 0.5) / n, (y + 0.5) / n) or (0, 0, 0)) for x in range(n)] for y in range(n)]
    out = [[0.0] * n for _ in range(n)]
    for y in range(n):
        for x in range(n):
            ys, xs = range(max(0, y - r), min(n, y + r + 1)), range(max(0, x - r), min(n, x + r + 1))
            out[y][x] = sum(hit[j][i] for j in ys for i in xs) / (len(ys) * len(xs))
    return lambda u, v: out[min(n - 1, int(v * n))][min(n - 1, int(u * n))]


def pale_islands(geo, level=0.36):
    """The islands whose own paint is light on average: bare skin, not metal."""
    return {k for k, (h, s, val) in island_means(geo).items() if val > level}


@painted
def body_grade(geo):
    pale, wear = pale_islands(geo), cloth_map(geo)

    def f(u, v):
        t = geo.at(u, v)
        if t.part == "upper" and t.island in pale and wear(u, v) < 0.5:
            return TAN
        return SCRAP
    return f


def body_detail_fn(team):
    n = Fbm(2113, 4, 6)

    @painted
    def make(geo):
        wear = cloth_map(geo)

        def f(u, v):
            if not team and wear(u, v) > 0.5:
                k = 0.8 + 0.4 * n.at(u * 8, v * 8)
                return [0.28 * k, 0.17 * k, 0.1 * k, 0.7]
            return [0, 0, 0, 0]
        return f
    return make


@painted
def head_grade(geo):
    zs = [t.z for row in geo.grid for t in row if t and t.covered]
    z0, z1 = min(zs), max(zs)

    def f(u, v):
        t = geo.at(u, v)
        h = (t.z - z0) / (z1 - z0)
        # the crown, and above the ears what does not face forward
        if h > 0.62 or (h > 0.4 and t.nrm[0] < 0.45):
            return [0.3, 0.27, 0.27, 1]
        return [0.62, 0.48, 0.37, 1]
    return f


def goggles_grade(u, v):
    return [0.72, 0.55, 0.3, 1]


def look():
    return Look("anarki", "anarki", {
        "body": Sheet({"": "anarki", "_red": "red", "_blue": "blue"},
                      grade=body_grade,
                      detail={"": body_detail_fn(False), "_red": None, "_blue": None}, size=256),
        "head": Sheet({"": "anarki_h", "_red": "red_h", "_blue": "anarki_h"}, grade=head_grade, size=128),
        "goggles": Sheet({"": "anarki_g", "_red": "red_g", "_blue": "blue_g"},
                         grade={"": goggles_grade, "_red": None, "_blue": None}, size=64),
    }, {"lower": {"l_legs": "body", "l_board": "models/players/anarki/anarki_b.tga"},
        "upper": {"u_torso": "body"},
        "head": {"h_head": "head", "h_glasses": "goggles"}},
        {"lower": ["tag_torso"], "upper": ["tag_head", "tag_torso", "tag_weapon"], "head": ["tag_head"]})
