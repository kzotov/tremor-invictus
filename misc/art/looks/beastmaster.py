"""Beastmaster on uriel: no wings, the hide graded to a deep bronze with
blue war paint across the chest and back, the hood turned to a dark pelt
with a pale fur trim, and amber eyes glowing under it. Team versions keep
the team's paint, without the wings, with the same pelt and eyes.

The hood, the painted chest and back, the brim's fold and the eyes are
worked out on the player's machine from where each texel sits on the
model."""
import math
from skins import Sheet, Look, painted
from tremorart import Fbm, Noise, clamp, mix

# ---------------------------------------------------------------- uriel.tga: body and hood share it

WOAD = (0.12, 0.22, 0.5)
PELT = (0.16, 0.11, 0.07)
AMBER = {"": (1.0, 0.65, 0.15), "_red": (1.0, 0.3, 0.15), "_blue": (0.35, 0.65, 1.0)}


def hood(t):
    return t.part == "head"


def chest_and_back(geo, n=128):
    """The torso's pieces that face most forward and most back, as island
    ids, and the height above which they are painted (the waist down is
    left bare)."""
    sums, zs = {}, []
    for y in range(n):
        for x in range(n):
            t = geo.at((x + 0.5) / n, (y + 0.5) / n)
            if t.covered and t.part == "upper":
                e = sums.setdefault(t.island, [0.0, 0])
                e[0] += t.nrm[0]
                e[1] += 1
                zs.append(t.z)
    big = {k: e[0] / e[1] for k, e in sums.items() if e[1] > 0.1 * len(zs)}
    return {max(big, key=big.get), min(big, key=big.get)}, min(zs) + 0.35 * (max(zs) - min(zs))


def trim(geo):
    """0..1 on the fold of the hood's brim, where the hood turns from
    facing down to facing out, on its front half."""
    xs = [t.x for row in geo.grid for t in row if t and t.covered and hood(t)]
    mid = (min(xs) + max(xs)) / 2

    def f(t):
        if t.x < mid:
            return 0.0
        return clamp(1.5 - abs(t.nrm[2] + 0.1) / 0.12)
    return f


@painted
def hide_grade(geo):
    return lambda u, v: [0.5, 0.5, 0.5, 1] if hood(geo.at(u, v)) else [0.6, 0.5, 0.44, 1]


def pelt(u, v, hairs, n, k_trim):
    """Long dark fur, paler at the trim."""
    s = hairs.at(u * 120, v * 30) * 0.6 + hairs.at(u * 260, v * 60 + 7) * 0.4
    k = (0.6 + 0.8 * s) * (0.8 + 0.4 * n.at(u * 3, v * 3))
    c = mix(PELT, (0.62, 0.56, 0.48), k_trim)
    return [x * k for x in c]


def detail_fn(team):
    hairs, n = Noise(1611, 256, 256), Fbm(1613, 3, 4)

    @painted
    def make(geo):
        painted_on, waist = chest_and_back(geo)
        fur = trim(geo)

        def f(u, v):
            t = geo.at(u, v)
            if hood(t):
                return pelt(u, v, hairs, n, fur(t)) + [0.88]
            if team or t.island not in painted_on or t.z < waist:
                return [0, 0, 0, 0]
            # three slanted bands of war paint, ragged at the edges
            w = ((u * 1.0 + v * 0.6) * 9) % 1
            edge = 0.18 + 0.06 * n.at(u * 8, v * 8)
            if abs(w - 0.5) < edge:
                return list(WOAD) + [0.65]
            return [0, 0, 0, 0]
        return f
    return make


# ---------------------------------------------------------------- the face (uriel_h.tga)

def face_grade(u, v):
    return [0.38, 0.36, 0.34, 1]


def eyes_fn(colour):
    @painted
    def make(geo):
        ts = [t for row in geo.grid for t in row if t and t.covered]
        z0, z1 = min(t.z for t in ts), max(t.z for t in ts)
        front = [t for t in ts if t.nrm[0] > 0.9]
        mid = sum(t.y for t in front) / len(front)               # the face's middle line
        half = max(abs(t.y - mid) for t in ts)
        # the eyes: either side of the middle, three fifths of the way up
        ez, ey, r = z0 + 0.6 * (z1 - z0), 0.19 * half, 0.05 * half

        def f(u, v):
            t = geo.at(u, v)
            d = math.hypot(abs(t.y - mid) - ey, t.z - ez) if t.nrm[0] > 0.3 else 1e9
            k = clamp(1.2 - d / r) ** 0.7
            return [colour[0] * k, colour[1] * k, colour[2] * k, 1]
        return f
    return make


def look():
    team = detail_fn(True)
    return Look("uriel", "beastmaster", {
        "hide": Sheet({"": "uriel", "_red": "red", "_blue": "blue"},
                      grade={"": hide_grade, "_red": None, "_blue": None},
                      detail={"": detail_fn(False), "_red": team, "_blue": team}, size=512),
        "face": Sheet({"": "uriel_h", "_red": "uriel_h", "_blue": "uriel_h"}, grade=face_grade,
                      glow={v: eyes_fn(AMBER[v]) for v in AMBER}, size=128),
    }, {"lower": {"l_legs": "hide"},
        "upper": {"u_torso": "hide", "u_rwing": "tremor/nodraw", "u_lwing": "tremor/nodraw"},
        "head": {"h_hood": "hide", "h_face": "face"}},
        {"lower": ["tag_torso"], "upper": ["tag_head", "tag_torso", "tag_weapon"], "head": ["tag_head"]})
