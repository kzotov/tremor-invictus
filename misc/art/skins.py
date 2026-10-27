"""Our own skins for player models the player already has: shader layers
over the model's own textures, which stay in the player's packs. Only these
layers ship. Called by make-models.py; by hand:

  python3 misc/art/skins.py OUT_DIR

  models/players/grunt/*_strogg[_red|_blue].skin, strogg/head_*.skin, strogg_*.tga
                       Strogg's look on grunt's "stripe" textures: the paint
                       graded to olive steel, the flesh to a sickly grey, an
                       implant plate, staples and veins on the bare arm, a
                       steel plate over the skull, a jaw grille, a glowing
                       implant eye and cable ports
  scripts/tremor-skins.shader

Every region is a polygon in the texture's 0..1 layout (u right, v down),
traced by hand from where the model's surfaces sit; nothing is read from
the model's own images.
"""
import math, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tremorart import Fbm, Noise, clamp, smooth, mix, write_tga


# ---------------------------------------------------------------- shapes in texture space

def poly(points, k=768.0):
    pts = [(x / k, y / k) for x, y in points]

    def inside(u, v):
        c = False
        for i in range(len(pts)):
            (x0, y0), (x1, y1) = pts[i], pts[i - 1]
            if (y0 > v) != (y1 > v) and u < (x1 - x0) * (v - y0) / (y1 - y0) + x0:
                c = not c
        return c

    def dist(u, v):
        best = 1e9
        for i in range(len(pts)):
            (x0, y0), (x1, y1) = pts[i], pts[i - 1]
            dx, dy = x1 - x0, y1 - y0
            t = clamp(((u - x0) * dx + (v - y0) * dy) / (dx * dx + dy * dy or 1))
            best = min(best, math.hypot(u - x0 - dx * t, v - y0 - dy * t))
        return -best if inside(u, v) else best
    return dist


def seg_dist(u, v, a, b):
    dx, dy = b[0] - a[0], b[1] - a[1]
    t = clamp(((u - a[0]) * dx + (v - a[1]) * dy) / (dx * dx + dy * dy))
    return math.hypot(u - a[0] - dx * t, v - a[1] - dy * t), t


def over(dst, src, a):
    return [d * (1 - a) + s * a for d, s in zip(dst, src)]


def metal(u, v, n, light=0.42):
    """Worn dark steel with a little rust."""
    k = light * (0.75 + 0.5 * n.at(u * 6, v * 6))
    r = smooth(0.62, 0.8, n.at(u * 3 + 7, v * 3))
    return mix((k, k * 1.0, k * 1.04), (k * 1.2, k * 0.7, k * 0.45), r * 0.6)


def rivets(u, v, centres, r):
    """0..1 for a lit dome, -1 outside."""
    for cu, cv in centres:
        d = math.hypot(u - cu, v - cv) / r
        if d < 1:
            return 1 - d
    return -1


def edge_rivets(points, k, step):
    """Rivet centres along a polygon's edge, pulled a little inside."""
    pts = [(x / k, y / k) for x, y in points]
    cx = sum(p[0] for p in pts) / len(pts)
    cy = sum(p[1] for p in pts) / len(pts)
    out = []
    for i in range(len(pts)):
        a, b = pts[i - 1], pts[i]
        n = max(1, int(math.hypot(b[0] - a[0], b[1] - a[1]) / step))
        for j in range(n):
            t = (j + 0.5) / n
            p = (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t)
            out.append((p[0] + (cx - p[0]) * 0.08, p[1] + (cy - p[1]) * 0.08))
    return out


# ---------------------------------------------------------------- grunt: the body sheet

# regions of the body sheet, traced in a 768 px view of it
FLESH = [poly([(368, 22), (562, 28), (604, 120), (594, 384), (470, 390), (368, 372), (358, 200)]),
         poly([(370, 388), (602, 388), (602, 482), (400, 492), (366, 460)])]
ARM_PLATE = [(468, 150), (566, 140), (580, 222), (548, 248), (474, 238)]
ARM_SEAM = ((0.545, 0.06), (0.66, 0.42))
PALLOR = (0.50, 0.53, 0.50)      # washed over bare skin
GRADE_STEEL = (0.70, 1.02, 1.25)     # x2 modulate factors: orange paint to olive steel
GRADE_FLESH = (0.80, 1.05, 1.10)     # pink to a sickly grey


def body_grade(u, v):
    f = max(clamp(0.5 - p(u, v) * 120) for p in FLESH)
    g = mix(GRADE_STEEL, GRADE_FLESH, f)
    return [g[0] / 2, g[1] / 2, g[2] / 2, 1]


def team_grade(u, v):
    """The team versions keep the team's paint: only the skin is graded."""
    f = max(clamp(0.5 - p(u, v) * 120) for p in FLESH)
    g = mix((1.0, 1.0, 1.0), GRADE_FLESH, f)
    return [g[0] / 2, g[1] / 2, g[2] / 2, 1]


def body_detail_fn():
    n, veins = Fbm(701, 4, 5), Fbm(709, 4, 3)
    plate = poly(ARM_PLATE)
    studs = edge_rivets(ARM_PLATE, 768.0, 0.03)

    def f(u, v):
        c, a = [0, 0, 0], 0.0
        in_flesh = min(p(u, v) for p in FLESH) < 0
        if in_flesh:
            c, a = list(PALLOR), 0.42                       # a dead, grey pallor over the skin
            # veins: dark lines where the noise crosses its middle
            w = abs(veins.at(u * 2, v * 2) - 0.5)
            if w < 0.018:
                c, a = [0.16, 0.12, 0.16], 0.55 * (1 - w / 0.018)
            # the stapled seam
            d, t = seg_dist(u, v, *ARM_SEAM)
            if d < 0.006:
                c, a = [0.18, 0.05, 0.05], 0.8
            phase = (t * 26) % 1
            if d < 0.016 and abs(phase - 0.5) < 0.12 and 0.04 < t < 0.96:
                k = 0.5 + 0.3 * (1 - d / 0.016)
                c, a = [k, k, k * 1.05], 1.0
        # the implant plate on the upper arm, bolted on
        pd = plate(u, v)
        if pd < 0:
            c, a = list(metal(u, v, n)), 1.0
            if pd > -0.006:
                c = [x * 0.55 for x in c]                      # shadowed rim
            r = rivets(u, v, studs, 0.009)
            if r >= 0:
                c = [0.25 + 0.5 * r] * 3
            hole = math.hypot(u - 0.69, v - 0.245)
            if hole < 0.02:
                c = [0.05, 0.05, 0.05] if hole < 0.012 else [0.55, 0.55, 0.56]
        elif pd < 0.008:
            c, a = [0.12, 0.05, 0.05], 0.7 * (1 - pd / 0.008)   # raw flesh round its edge
        # ports on the shoulder flesh
        for cu, cv in ((0.56, 0.56), (0.66, 0.58)):
            d = math.hypot(u - cu, v - cv)
            if d < 0.022:
                c, a = ([0.04, 0.04, 0.04] if d < 0.011 else [0.5, 0.5, 0.52]), 1.0
        return c + [a]
    return f


def body_glow(u, v):
    """The arm plate's status light and the chest grille's lamps."""
    k = 0.0
    for cu, cv, r in ((0.69, 0.245, 0.011), (0.215, 0.33, 0.012), (0.255, 0.33, 0.012)):
        k = max(k, 1 - math.hypot(u - cu, v - cv) / r)
    k = clamp(k) ** 1.2
    return [k, 0.12 * k, 0.05 * k, 1]


# ---------------------------------------------------------------- grunt: the head sheet

# traced in a 512 px view of the head sheet (the face on the left, the side of the head on the right)
SKULL_PLATE = [(262, 38), (472, 58), (502, 140), (470, 222), (332, 204), (250, 150)]
JAW = [(92, 312), (212, 312), (226, 352), (196, 400), (118, 400), (80, 352)]
EYE = (190 / 512, 232 / 512)
SCALP_STAPLES = ((120 / 512, 82 / 512), (252 / 512, 128 / 512))
PORTS = [(382 / 512, 300 / 512), (424 / 512, 352 / 512)]


def head_grade(u, v):
    g = GRADE_FLESH
    return [g[0] / 2, g[1] / 2, g[2] / 2, 1]


def head_detail_fn():
    n = Fbm(733, 4, 5)
    skull, jaw = poly(SKULL_PLATE, 512.0), poly(JAW, 512.0)
    studs = edge_rivets(SKULL_PLATE, 512.0, 0.05)
    jstuds = edge_rivets(JAW, 512.0, 0.06)

    def f(u, v):
        c, a = list(PALLOR), 0.42
        # dark, sunken skin round the eyes
        for ex in (75 / 512, 190 / 512):
            d = math.hypot((u - ex) * 0.8, v - 232 / 512)
            if d < 0.07:
                c, a = mix(PALLOR, (0.08, 0.06, 0.07), 1 - d / 0.07), 0.42 + 0.3 * (1 - d / 0.07)
                c = list(c)
        # staples across the scalp
        d, t = seg_dist(u, v, *SCALP_STAPLES)
        if d < 0.005:
            c, a = [0.2, 0.05, 0.05], 0.8
        if d < 0.014 and abs((t * 9) % 1 - 0.5) < 0.12:
            c, a = [0.6, 0.6, 0.62], 1.0
        # the steel plate over the skull
        sd = skull(u, v)
        if sd < 0:
            c, a = list(metal(u, v, n, 0.46)), 1.0
            if sd > -0.01:
                c = [x * 0.55 for x in c]
            r = rivets(u, v, studs, 0.014)
            if r >= 0:
                c = [0.25 + 0.5 * r] * 3
        elif sd < 0.012:
            c, a = [0.15, 0.05, 0.05], 0.6 * (1 - sd / 0.012)
        # the jaw grille: slats over the mouth and chin
        jd = jaw(u, v)
        if jd < 0:
            c, a = list(metal(u, v, n, 0.28)), 1.0
            slat = (v * 512 / 11) % 1
            if slat < 0.35:
                c = [0.02, 0.02, 0.02]
            if jd > -0.012:
                c = [x * 0.6 for x in c]
            r = rivets(u, v, jstuds, 0.016)
            if r >= 0:
                c = [0.25 + 0.5 * r] * 3
        # the implant eye's socket ring
        d = math.hypot(u - EYE[0], v - EYE[1])
        if d < 0.065:
            c, a = ([0.45, 0.45, 0.48] if d > 0.038 else [0.25, 0.02, 0.02]), 1.0
        # cable ports behind the ear
        for p in PORTS:
            d = math.hypot(u - p[0], v - p[1])
            if d < 0.035:
                c, a = ([0.04, 0.04, 0.04] if d < 0.018 else [0.52, 0.52, 0.55]), 1.0
        return c + [a]
    return f


def head_glow(u, v):
    k = clamp(1.3 - math.hypot(u - EYE[0], v - EYE[1]) / 0.04) ** 0.6
    return [k, 0.1 * k, 0.04 * k, 1]


# ---------------------------------------------------------------- files

def tex(path, w, h, fn, alpha):
    px = [fn((x + 0.5) / w, (y + 0.5) / h) for y in range(h) for x in range(w)]
    os.makedirs(os.path.dirname(path), exist_ok=True)
    write_tga(path, w, h, px, alpha)


SHADER = """
models/players/grunt/strogg_body%(suffix)s
{
	{
		map models/players/grunt/%(body)s.tga
		rgbGen lightingDiffuse
	}
	{
		map models/players/grunt/%(grade)s.tga
		blendFunc GL_DST_COLOR GL_SRC_COLOR
	}
	{
		map models/players/grunt/strogg_detail.tga
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen lightingDiffuse
	}
	{
		map models/players/grunt/strogg_glow.tga
		blendFunc GL_ONE GL_ONE
		rgbGen wave sin 0.85 0.15 0 1.5
	}
}

models/players/grunt/strogg_head%(suffix)s
{
	{
		map models/players/grunt/%(head)s.tga
		rgbGen lightingDiffuse
	}
	{
		map models/players/grunt/strogg_hgrade.tga
		blendFunc GL_DST_COLOR GL_SRC_COLOR
	}
	{
		map models/players/grunt/strogg_hdetail.tga
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen lightingDiffuse
	}
	{
		map models/players/grunt/strogg_hglow.tga
		blendFunc GL_ONE GL_ONE
		rgbGen wave sin 0.85 0.15 0 1.5
	}
}
"""
# Strogg on grunt: the model's own paint (stripe, or the team's), graded,
# then our details and lights
VARIANTS = (("", "stripe", "stripe_h", "strogg_grade"),
            ("_red", "red", "red_h", "strogg_tgrade"),
            ("_blue", "blue", "blue_h", "strogg_tgrade"))
SHADERS = "// our skins over models the player already has (misc/art/skins.py)\n" + "".join(
    SHADER % {"suffix": x, "body": b, "head": h, "grade": g} for x, b, h, g in VARIANTS)


def grunt_strogg(out):
    d = os.path.join(out, "models/players/grunt")
    tex(os.path.join(d, "strogg_grade.tga"), 128, 128, body_grade, False)
    tex(os.path.join(d, "strogg_detail.tga"), 512, 512, body_detail_fn(), True)
    tex(os.path.join(d, "strogg_glow.tga"), 128, 128, body_glow, False)
    tex(os.path.join(d, "strogg_hgrade.tga"), 16, 16, head_grade, False)
    tex(os.path.join(d, "strogg_hdetail.tga"), 256, 256, head_detail_fn(), True)
    tex(os.path.join(d, "strogg_hglow.tga"), 64, 64, head_glow, False)
    tex(os.path.join(d, "strogg_tgrade.tga"), 128, 128, team_grade, False)
    for suffix, _, _, _ in VARIANTS:
        body = "models/players/grunt/strogg_body" + suffix
        parts = {
            "lower": "tag_torso,\nl_legs,%s\n" % body,
            "upper": "tag_head,\ntag_weapon,\nu_torso,%s\ntag_torso,\nu_larm,%s\nu_rarm,%s\n" % (body, body, body),
            "head": "tag_head,\nh_head,models/players/grunt/strogg_head%s\n" % suffix,
        }
        for part, text in parts.items():
            if part == "head" and suffix:
                # team heads are looked up in a folder named after the skin
                path = os.path.join(d, "strogg", "head%s.skin" % suffix)
            else:
                path = os.path.join(d, "%s_strogg%s.skin" % (part, suffix))
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, "w") as f:
                f.write(text)


def build(out):
    grunt_strogg(out)
    os.makedirs(os.path.join(out, "scripts"), exist_ok=True)
    with open(os.path.join(out, "scripts/tremor-skins.shader"), "w") as f:
        f.write(SHADERS)


if __name__ == "__main__":
    build(sys.argv[1])
