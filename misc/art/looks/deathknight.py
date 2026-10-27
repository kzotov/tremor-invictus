"""Death Knight: visor's plate body with bones' skull. The plate is blackened
iron with embers glowing through its cracks and a furnace grate where the
chest eye was; the skull is charred, with fire in the sockets. Team versions
keep the team's paint and burn in its colour."""
import math
from skins import Sheet, Look
from tremorart import Fbm, clamp, smooth

# ---------------------------------------------------------------- the body sheet (visor.tga, 768 px view)

FURNACE = (680 / 768, 415 / 768, 84 / 768)          # over the chest eye
IRON = (0.07, 0.065, 0.07)
FIRE = {"": (1.0, 0.42, 0.08), "_red": (1.0, 0.3, 0.06), "_blue": (0.3, 0.6, 1.0)}


def body_grade(u, v):
    return [0.36, 0.33, 0.33, 1]


def team_grade(u, v):
    return [0.42, 0.4, 0.4, 1]


def grate(u, v):
    """For the furnace: (inside the disc, on a bar, on the rim)."""
    cu, cv, r = FURNACE
    d = math.hypot(u - cu, v - cv)
    slot = abs(((v - cv) / r * 2.5 + 0.5) % 1 - 0.5) < 0.09 and abs(u - cu) < r * 0.55
    return d < r, not slot, d > r * 0.85


def cracks_fn(seed):
    lines, mask = Fbm(seed, 4, 4), Fbm(seed + 3, 3, 3)

    def f(u, v):
        """0..1 on a crack."""
        w = abs(lines.at(u * 2, v * 2) - 0.5)
        return clamp(1 - w / 0.012) * smooth(0.5, 0.6, mask.at(u * 2, v * 2))
    return f


CRACKS = cracks_fn(911)


def body_detail_fn(wash):
    n = Fbm(907, 4, 6)

    def f(u, v):
        k = 0.7 + 0.6 * n.at(u * 4, v * 4)
        c, a = [x * k for x in IRON], wash
        crack = CRACKS(u, v)
        if crack > 0:
            c, a = [0.02, 0.01, 0.01], max(a, 0.9 * crack)
        inside, bar, rim = grate(u, v)
        if inside:
            if rim:
                c, a = [0.16, 0.15, 0.15], 1.0
            elif bar:
                c, a = [0.09, 0.085, 0.09], 1.0
            else:
                c, a = [0.25, 0.06, 0.01], 1.0
        return c + [a]
    return f


def body_glow_fn(colour):
    def f(u, v):
        k = 0.8 * CRACKS(u, v)
        inside, bar, rim = grate(u, v)
        if inside and not rim and not bar:
            cu, cv, r = FURNACE
            k = max(k, 0.8 - 0.5 * abs(u - cu) / r)
        return [colour[0] * k, colour[1] * k, colour[2] * k, 1]
    return f


# ---------------------------------------------------------------- the skull (bones' stiff.tga, 768 px view)

SOCKETS = [(38 / 768, 636 / 768, 20 / 768), (118 / 768, 630 / 768, 28 / 768), (72 / 768, 692 / 768, 11 / 768)]


def skull_grade(u, v):
    return [0.24, 0.215, 0.2, 1]


def skull_detail_fn():
    soot = Fbm(919, 4, 6)

    def f(u, v):
        k = soot.at(u * 3, v * 3)
        return [0.03, 0.025, 0.02, 0.25 + smooth(0.45, 0.7, k) * 0.6]
    return f


def skull_glow_fn(colour):
    def f(u, v):
        k = 0.0
        for cu, cv, r in SOCKETS:
            k = max(k, 1 - math.hypot(u - cu, v - cv) / r)
        k = clamp(k * 1.4) ** 0.8
        return [colour[0] * k, colour[1] * k, colour[2] * k, 1]
    return f


def look():
    team_detail = body_detail_fn(0.35)
    return Look("visor", "deathknight", {
        "body": Sheet({"": "visor", "_red": "red", "_blue": "blue"},
                      grade={"": body_grade, "_red": team_grade, "_blue": team_grade},
                      detail={"": body_detail_fn(0.6), "_red": team_detail, "_blue": team_detail},
                      glow={v: body_glow_fn(FIRE[v]) for v in FIRE}, size=512, pulse=True),
        "skull": Sheet({"": "stiff", "_red": "stiff", "_blue": "stiff"}, model="bones",
                       grade=skull_grade, detail=skull_detail_fn(),
                       glow={v: skull_glow_fn(FIRE[v]) for v in FIRE}, size=256, pulse=True),
    }, {"lower": {"l_legs": "body"}, "upper": {"u_torso": "body"}, "head": {"h_head": "skull"}},
        {"lower": ["tag_torso"], "upper": ["tag_head", "tag_torso", "tag_weapon"], "head": ["tag_head"]},
        head_model="bones")
