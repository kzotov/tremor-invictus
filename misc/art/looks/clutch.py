"""Clutch on tankjr: the orange armour tinted a glossy dark red with the
model's own shading kept, the steel trim left bright, shadows in the
creases, worn edges and scratches, the emblems covered by steel plates with three stencilled bars (the plates
are mirrored on the model, so no lettering), the gory feet turned to
dark steel, the bare head cased in steel as a sensor unit, and its eyes
lit amber. Team versions keep the
team's paint with the same plates, feet, head and eyes."""
import math
from skins import Sheet, Look, LabelMap, painted
from tremorart import Fbm, Noise, clamp

# the plates, in the standing model's units: on the shoulder guards' outer
# faces (|y| past SHOULDER), from x0, w wide, the top at z top (at x = -1)
# rising by rise per unit forwards, h high; on the chest, an arc from a0
# degrees round from the front, w degrees wide, the top at z top, h high
SHOULDER, SHOULDER_PLATE = 17, (-5.1, 8.2, 29.3, 0.25, 6.2)
CHEST_PLATE = (40, 49, 27.2, 6.8)
EYES = (0.4, 2.0)                      # the eyes, out to the left of the face's middle
EYE_HEIGHT, EYE_SIZE = 0.45, 0.75       # their height as a share of the head's, and radius
PAINT = (0.3, 0.03, 0.03)
LIGHTS = {"": (1.0, 0.75, 0.2), "_red": (1.0, 0.35, 0.2), "_blue": (0.3, 0.7, 1.0)}
TINT = (0.44, 0.11, 0.1)                        # the orange paint to dark red
STEEL = (0.72, 0.72, 0.74)


def mean_colour(geo, u, v, n=128):
    """The model's own colour over a texel of an n grid (a mid orange when
    the image is not one we can read)."""
    cs = [geo.colour(u + i * 0.25 / n, v + j * 0.25 / n) for i in (-1, 1) for j in (-1, 1)]
    if cs[0] is None:
        return (0.6, 0.35, 0.1)
    return tuple(sum(c[k] for c in cs) / 4 for k in range(3))


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


def material(geo, u, v):
    """What a texel of tankjr.tga is, by the model's own colour there: p
    paint (a saturated orange to yellow), g gore (a saturated red), s
    steel (the rest), k shadow."""
    r, g, b = mean_colour(geo, u, v)
    hi, lo = max(r, g, b), min(r, g, b)
    d = hi - lo + 1e-6
    sat, lum = d / (hi + 1e-6), (r + g + b) / 3
    if hi == r:
        hue = 60 * (((g - b) / d) % 6)
    elif hi == g:
        hue = 60 * ((b - r) / d + 2)
    else:
        hue = 60 * ((r - g) / d + 4)
    if lum < 0.06:
        return "k"
    if sat > 0.6 and (hue < 12 or hue > 330) and r > 0.3:
        return "g"
    if sat > 0.45 and 12 <= hue < 70 and lum > 0.12:
        return "p"
    return "s"


MAPS = {}


def label_map(geo):
    """The sheet's LabelMap, made once per sheet."""
    if id(geo) not in MAPS:
        n = 128
        rows = [[material(geo, (x + 0.5) / n, (y + 0.5) / n) for x in range(n)] for y in range(n)]
        MAPS[id(geo)] = LabelMap(["".join(row) for row in majority(rows)])
    return MAPS[id(geo)]


def plate_at(t):
    """Where a texel sits on one of the steel plates over the emblems, as
    (across, down) in 0..1, or None: the outer faces of the shoulder
    guards (both sides, the guards share their paint) and the left of the
    chest, round from the front towards the side."""
    if t.surf == "u_arms" and abs(t.y) > SHOULDER and abs(t.nrm[1]) > 0.6:
        x0, w, top, rise, h = SHOULDER_PLATE
        across, down = (t.x - x0) / w, (top + rise * (t.x + 1) - t.z) / h
    elif t.surf == "u_torso" and t.x * t.nrm[0] + t.y * t.nrm[1] > 0 and math.hypot(t.x, t.y) > 6:
        a0, w, top, h = CHEST_PLATE
        across, down = (math.degrees(math.atan2(t.y, t.x)) - a0) / w, (top - t.z) / h
    else:
        return None
    return (across, down) if 0 <= across <= 1 and 0 <= down <= 1 else None


def plate_fn(geo):
    """(u, v) -> None off the plates, else True on a stencilled bar."""
    def plate(u, v):
        p = plate_at(geo.at(u, v))
        if p is None:
            return None
        x, y = p
        return 0.22 < y < 0.78 and any(abs(x - c) < 0.07 for c in (0.3, 0.5, 0.7))
    return plate


def eyes_at(geo):
    """The eyes on the head's face, at about half its height, both on its
    left: one beside the middle and one further round; returns the glow
    (0..1) at a texel."""
    zs = [t.z for row in geo.grid for t in row if t is not None and t.covered and t.part == "head"]
    mid = min(zs) + EYE_HEIGHT * (max(zs) - min(zs))

    def at(u, v):
        t = geo.at(u, v)
        if t.part != "head" or t.nrm[0] < 0.3:
            return 0.0
        return max(1 - math.hypot(t.y - y, t.z - mid) / EYE_SIZE for y in EYES)
    return at


def body_grade_fn(team):
    n = Fbm(2013, 4, 6)

    @painted
    def make(geo):
        labels = label_map(geo)

        def f(u, v):
            c, k = labels.label(u, v), labels.shade(u, v, 0.5)
            k *= 0.88 + 0.24 * n.at(u * 5, v * 5)               # grime
            if c == "p" and not team:
                return [clamp(z * k) for z in TINT] + [1]
            if c == "s":
                return [0.56 * k, 0.56 * k, 0.6 * k, 1]          # the steel a little brighter and cooler
            return [0.5 * k, 0.5 * k, 0.5 * k, 1]
        return f
    return make


def detail_fn(team):
    n, fine, scratch = Fbm(2011, 4, 6), Noise(2015, 256, 256), Noise(2017, 64, 512)

    @painted
    def make(geo):
        labels, plate = label_map(geo), plate_fn(geo)

        def f(u, v):
            p = plate(u, v)
            if p is not None:
                if p:
                    return [0.85, 0.82, 0.7, 1.0]
                k = 0.3 * (0.8 + 0.4 * n.at(u * 8, v * 8))
                return [k, k, k * 1.05, 1.0]
            t = geo.at(u, v)
            if "foot" in t.surf or "toe" in t.surf:                # the gory feet
                k = 0.2 * (0.75 + 0.5 * n.at(u * 8, v * 8))
                return [k, k, k * 1.05, 0.95]
            if t.part == "head":                                 # the bare head
                k = 0.36 * (0.75 + 0.5 * n.at(u * 6, v * 6))
                return [k, k * 1.02, k * 1.08, 0.88]
            c = labels.label(u, v)
            if c == "p":
                if labels.edge(u, v) < 0.9 and fine.at(u * 400, v * 400) > 0.68:
                    return list(STEEL) + [0.75]                  # paint worn off the edge
                if abs(scratch.at(u * 12, v * 90) - 0.5) < 0.006:
                    return list(STEEL) + [0.45]                  # a scratch
                if not team:
                    k = 0.85 + 0.3 * n.at(u * 6, v * 6)
                    return [PAINT[0] * k, PAINT[1] * k, PAINT[2] * k, 0.45]
            return [0, 0, 0, 0]
        return f
    return make


def eyes_fn(colour):
    @painted
    def make(geo):
        eyes = eyes_at(geo)

        def f(u, v):
            k = clamp(eyes(u, v) * 1.4)
            return [colour[0] * k, colour[1] * k, colour[2] * k, 1]
        return f
    return make


def look():
    return Look("tankjr", "clutch", {
        "body": Sheet({"": "tankjr", "_red": "red", "_blue": "blue"},
                      grade={"": body_grade_fn(False), "_red": body_grade_fn(True), "_blue": body_grade_fn(True)},
                      detail={"": detail_fn(False), "_red": detail_fn(True), "_blue": detail_fn(True)},
                      glow={v: eyes_fn(LIGHTS[v]) for v in LIGHTS}, size=512),
    }, {"lower": {s: "body" for s in ("l_rcalf", "l_rfoot", "l_lfoot", "l_hips", "l_rthigh", "l_lthigh",
                                      "l_rtoe", "l_ltoe", "l_rtoes", "l_ltoes", "l_lcalf")},
        "upper": {"u_abdomen": "body", "u_arms": "body", "u_torso": "body"},
        "head": {"h_head": "body"}},
        {"lower": ["tag_torso"], "upper": ["tag_head", "tag_weapon", "tag_torso"], "head": ["tag_head"]})
