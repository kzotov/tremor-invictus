"""Unumrey on orbb: the flesh graded to a bruised violet-grey, the metal
blackened, and an eye of our own, violet with a glowing ring round the
pupil. Team versions keep the team's paint, the eye lit in the team's
colour."""
import math
from skins import Sheet, Look, painted
from tremorart import Fbm, clamp, smooth, mix

VIOLET = {"": (0.65, 0.3, 1.0), "_red": (1.0, 0.3, 0.2), "_blue": (0.3, 0.6, 1.0)}


def body_grade(u, v):
    return [0.4, 0.36, 0.48, 1]


def body_detail_fn():
    n = Fbm(1711, 4, 6)

    def f(u, v):
        k = n.at(u * 4, v * 4)
        return [0.08, 0.04, 0.12, 0.25 + 0.3 * smooth(0.45, 0.7, k)]      # bruising
    return f


# ---------------------------------------------------------------- the eye (orbb_h2.tga): iris to the front

IRIS = 0.25
PUPIL = 0.09
IRIS_ANGLE = 38.0          # degrees off the eye's axis that IRIS stands for


def eye_frame(geo):
    """Where each texel sits on the eyeball, as (d, a): d grows with the
    angle off the eye's axis (IRIS at IRIS_ANGLE), a goes round it. The
    ball's middle is the middle of its box, its axis the way its surface
    faces on average."""
    if hasattr(geo, "eye_frame"):
        return geo.eye_frame
    ts = [t for row in geo.grid for t in row if t is not None and t.covered]
    mid = [(min(t.pos[i] for t in ts) + max(t.pos[i] for t in ts)) / 2 for i in range(3)]
    ax = [sum(t.nrm[i] for t in ts) for i in range(3)]
    m = math.sqrt(sum(q * q for q in ax)) or 1
    ax = [q / m for q in ax]
    # across the axis: left (y) and up, square to it
    up = [-ax[2] * ax[0], -ax[2] * ax[1], 1 - ax[2] * ax[2]]
    m = math.sqrt(sum(q * q for q in up)) or 1
    up = [q / m for q in up]
    left = [up[1] * ax[2] - up[2] * ax[1], up[2] * ax[0] - up[0] * ax[2], up[0] * ax[1] - up[1] * ax[0]]

    def at(u, v):
        t = geo.at(u, v)
        if t is None:
            return 1.0, 0.0
        p = [t.pos[i] - mid[i] for i in range(3)]
        along = sum(p[i] * ax[i] for i in range(3))
        x, y = sum(p[i] * left[i] for i in range(3)), sum(p[i] * up[i] for i in range(3))
        return math.degrees(math.atan2(math.hypot(x, y), along)) * IRIS / IRIS_ANGLE, math.atan2(-y, x)
    geo.eye_frame = at
    return at


def eye_detail_fn(colour):
    @painted
    def make(geo):
        n, veins = Fbm(1721, 3, 8), Fbm(1723, 4, 3)
        frame = eye_frame(geo)

        def f(u, v):
            d, a = frame(u, v)
            if d > IRIS:
                # our own sclera over the whole sheet (the model's has holes its own shader hides),
                # grey-violet with dark veins
                k = 0.75 + 0.25 * n.at(u * 2, v * 2)
                c = [0.62 * k, 0.58 * k, 0.66 * k]
                if abs(veins.at(u * 1.5, v * 1.5) - 0.5) < 0.015:
                    c = [0.3, 0.1, 0.25]
                return c + [1.0]
            if d < PUPIL:
                return [0.01, 0.0, 0.02, 1.0]
            fib = 0.75 + 0.5 * n.at(math.cos(a) * 3 + 5, math.sin(a) * 3 + d * 20)
            c = mix(tuple(x * 0.25 for x in colour), tuple(x * 0.7 for x in colour), (d - PUPIL) / (IRIS - PUPIL))
            return [x * fib for x in c] + [1.0]
        return f
    return make


def eye_glow_fn(colour):
    @painted
    def make(geo):
        frame = eye_frame(geo)

        def f(u, v):
            d = frame(u, v)[0]
            k = clamp(1 - abs(d - PUPIL - 0.02) / 0.025)
            return [colour[0] * k, colour[1] * k, colour[2] * k, 1]
        return f
    return make


def look():
    return Look("orbb", "unumrey", {
        "body": Sheet({"": "orbb", "_red": "red", "_blue": "blue"},
                      grade={"": body_grade, "_red": None, "_blue": None},
                      detail={"": body_detail_fn(), "_red": None, "_blue": None}, size=256),
        "eye": Sheet({"": "orbb_h2", "_red": "red_h", "_blue": "blue_h"},
                     detail={v: eye_detail_fn(VIOLET[v]) for v in VIOLET},
                     glow={v: eye_glow_fn(VIOLET[v]) for v in VIOLET}, size=256, pulse=True),
    }, {"lower": {"l_legs": "body"},
        "upper": {"u_torso": "body", "u_ratchet": "body", "u_antenna": "body",
                  "u_antennaball": {"": "models/players/orbb/orbb_light.tga",
                                    "_blue": "models/players/orbb/orbb_light_blue.tga"},
                  "u_tailight": {"": "models/players/orbb/orbb_tail.tga",
                                 "_blue": "models/players/orbb/orbb_tail_blue.tga"}},
        "head": {"h_head": "eye"}},
        {"lower": ["tag_torso"], "upper": ["tag_head", "tag_weapon", "tag_torso"], "head": ["tag_head"]})
