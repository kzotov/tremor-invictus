"""Unumrey on orbb: the flesh graded to a bruised violet-grey, the metal
blackened, and an eye of our own, violet with a glowing ring round the
pupil. Team versions keep the team's paint, the eye lit in the team's
colour."""
import math
from skins import Sheet, Look
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


# ---------------------------------------------------------------- the eye (orbb_h2.tga): iris at the middle

IRIS = 0.25
PUPIL = 0.09


def eye_detail_fn(colour):
    n, veins = Fbm(1721, 3, 8), Fbm(1723, 4, 3)

    def f(u, v):
        d = math.hypot(u - 0.5, v - 0.5)
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
        a = math.atan2(v - 0.5, u - 0.5)
        fib = 0.75 + 0.5 * n.at(math.cos(a) * 3 + 5, math.sin(a) * 3 + d * 20)
        c = mix(tuple(x * 0.25 for x in colour), tuple(x * 0.7 for x in colour), (d - PUPIL) / (IRIS - PUPIL))
        return [x * fib for x in c] + [1.0]
    return f


def eye_glow_fn(colour):
    def f(u, v):
        d = math.hypot(u - 0.5, v - 0.5)
        k = clamp(1 - abs(d - PUPIL - 0.02) / 0.025)
        return [colour[0] * k, colour[1] * k, colour[2] * k, 1]
    return f


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
