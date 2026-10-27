"""Death Knight: visor's plate body with bones' skull. The plate is blackened
iron with embers glowing through its cracks and a furnace grate where the
chest eye was; the skull is charred, with fire in the sockets. Team versions
keep the team's paint and burn in its colour."""
import math
from skins import Sheet, Look, painted
from tremorart import Fbm, clamp, smooth

# ---------------------------------------------------------------- the body sheet (visor.tga)

IRON = (0.07, 0.065, 0.07)
FIRE = {"": (1.0, 0.42, 0.08), "_red": (1.0, 0.3, 0.06), "_blue": (0.3, 0.6, 1.0)}


def find_furnace(geo, n=256):
    """The furnace sits on the shoulder pad: of the sheet's fair-sized UV
    islands, the one riding highest on the body. (u, v, r): the disc that
    fits its box."""
    islands = {}
    for y in range(n):
        for x in range(n):
            t = geo.at((x + 0.5) / n, (y + 0.5) / n)
            if t is not None and t.covered:
                s = islands.setdefault(t.island, [0, 0.0, n, n, 0, 0])
                s[0] += 1
                s[1] += t.z
                s[2], s[3], s[4], s[5] = min(s[2], x), min(s[3], y), max(s[4], x + 1), max(s[5], y + 1)
    big = [s for s in islands.values() if s[0] >= n * n // 100]
    if not big:
        return (0.5, 0.5, 0.0)
    _, _, x0, y0, x1, y1 = max(big, key=lambda s: s[1] / s[0])
    return ((x0 + x1) / 2.0 / n, (y0 + y1) / 2.0 / n, min(x1 - x0, y1 - y0) / 2.0 / n)


def furnace(geo):
    if not hasattr(geo, "dk_furnace"):
        geo.dk_furnace = find_furnace(geo)
    return geo.dk_furnace


def body_grade(u, v):
    return [0.36, 0.33, 0.33, 1]


def team_grade(u, v):
    return [0.42, 0.4, 0.4, 1]


def grate(u, v, disc):
    """For the furnace disc (u, v, r): (inside the disc, on a bar, on the rim)."""
    cu, cv, r = disc
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
    @painted
    def make(geo):
        return body_detail(wash, furnace(geo))
    return make


def body_detail(wash, disc):
    n = Fbm(907, 4, 6)

    def f(u, v):
        k = 0.7 + 0.6 * n.at(u * 4, v * 4)
        c, a = [x * k for x in IRON], wash
        crack = CRACKS(u, v)
        if crack > 0:
            c, a = [0.02, 0.01, 0.01], max(a, 0.9 * crack)
        inside, bar, rim = grate(u, v, disc)
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
    @painted
    def make(geo):
        disc = furnace(geo)

        def f(u, v):
            k = 0.8 * CRACKS(u, v)
            inside, bar, rim = grate(u, v, disc)
            if inside and not rim and not bar:
                cu, cv, r = disc
                k = max(k, 0.8 - 0.5 * abs(u - cu) / r)
            return [colour[0] * k, colour[1] * k, colour[2] * k, 1]
        return f
    return make


# ---------------------------------------------------------------- the skull (bones' stiff.tga)

def find_sockets(geo, n=256):
    """The sockets: patches of the face (facing forward) that the model's
    own texture paints dark, the eyes and the nose. [(u, v, r)], each a
    disc of its patch's size."""
    if geo.colour(0.5, 0.5) is None:
        return []
    dark = set()
    for y in range(n):
        for x in range(n):
            u, v = (x + 0.5) / n, (y + 0.5) / n
            t = geo.at(u, v)
            if t is not None and t.covered and t.part == "head" and t.nrm[0] > 0.5 and sum(geo.colour(u, v)) / 3 < 0.12:
                dark.add((x, y))
    out, seen = [], set()
    for p in sorted(dark):
        if p in seen:
            continue
        todo, patch = [p], []
        seen.add(p)
        while todo:
            x, y = todo.pop()
            patch.append((x, y))
            for q in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                if q in dark and q not in seen:
                    seen.add(q)
                    todo.append(q)
        if len(patch) * 1000 >= n * n:
            out.append(((sum(x for x, _ in patch) / len(patch) + 0.5) / n, (sum(y for _, y in patch) / len(patch) + 0.5) / n,
                        1.3 * math.sqrt(len(patch) / math.pi) / n))
    return out


def skull_grade(u, v):
    return [0.24, 0.215, 0.2, 1]


def skull_detail_fn():
    soot = Fbm(919, 4, 6)

    def f(u, v):
        k = soot.at(u * 3, v * 3)
        return [0.03, 0.025, 0.02, 0.25 + smooth(0.45, 0.7, k) * 0.6]
    return f


def skull_glow_fn(colour):
    @painted
    def make(geo):
        if not hasattr(geo, "dk_sockets"):
            geo.dk_sockets = find_sockets(geo)
        sockets = geo.dk_sockets

        def f(u, v):
            k = 0.0
            for cu, cv, r in sockets:
                k = max(k, 1 - math.hypot(u - cu, v - cv) / r)
            k = clamp(k * 1.4) ** 0.8
            return [colour[0] * k, colour[1] * k, colour[2] * k, 1]
        return f
    return make


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
