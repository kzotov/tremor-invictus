"""Generate the HUD's own images (hud.md 10.14), white where the code tints.

Usage: python3 make-hud.py OUT_DIR

Writes, under OUT_DIR/gfx/tremor/hud/:
  disc.tga        128 x 128, a filled circle (the ability disc, status slots)
  health.tga      128 x 128, a ring with a four-pointed cross inside (the
                  health icon; tinted blue by the code)
  armor.tga       128 x 128, a ring with a crenellated shield inside
  streak.tga      256 x 16, a hit streak fading out to the right
  smear.tga       256 x 64, a soft brush smear (the low-ammo glow)
  bloom.tga       128 x 128, a soft round glow for ready / full states
All drawn here from shapes.
"""
import math, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tremorart import *

WHITE = (1.0, 1.0, 1.0)


def disc(out):
    c = Canvas(128, 128)
    c.fill(circle(0.5, 0.5, 0.48), WHITE)
    c.write(os.path.join(out, "disc.tga"))


def health(out):
    # ring Ø 36 in a 42 box (hud.md 10.3): radius 0.43, 2 dp of 42
    c = Canvas(128, 128)
    c.fill(ring(0.5, 0.5, 0.41, 0.05), WHITE)
    cross = polygon([(0.5, 0.2), (0.56, 0.44), (0.8, 0.5), (0.56, 0.56), (0.5, 0.8), (0.44, 0.56), (0.2, 0.5), (0.44, 0.44)])
    c.fill(cross, WHITE)
    c.write(os.path.join(out, "health.tga"))


def armor(out):
    c = Canvas(128, 128)
    c.fill(ring(0.5, 0.5, 0.41, 0.05), WHITE)
    body = polygon([(0.3, 0.3), (0.38, 0.3), (0.38, 0.26), (0.46, 0.26), (0.46, 0.3), (0.54, 0.3), (0.54, 0.26),
                    (0.62, 0.26), (0.62, 0.3), (0.7, 0.3), (0.7, 0.52), (0.5, 0.76), (0.3, 0.52)])
    c.fill(body, WHITE)
    c.write(os.path.join(out, "armor.tga"))


def streak(out):
    w, h = 256, 16
    px = []
    for j in range(h):
        y = (j + 0.5) / h
        k = max(0.0, 1 - abs(y - 0.5) / 0.5) ** 2
        for i in range(w):
            x = (i + 0.5) / w
            a = k * (1 - x) ** 1.5
            px.append([1, 1, 1, a])
    write_tga(os.path.join(out, "streak.tga"), w, h, px)


def smear(out):
    w, h = 256, 64
    n = Fbm(131, 4, 6)
    px = []
    for j in range(h):
        y = (j + 0.5) / h
        for i in range(w):
            x = (i + 0.5) / w
            edge = smooth(0.0, 0.25, x) * (1 - smooth(0.85, 1.0, x))
            band = max(0.0, 1 - abs(y - 0.5) / (0.38 + 0.08 * n.at(x * 3, 0.5)))
            a = clamp(edge * band ** 0.7 * (0.75 + 0.35 * n.at(x * 6, y * 2)))
            px.append([1, 1, 1, a])
    write_tga(os.path.join(out, "smear.tga"), w, h, px)


def bloom(out):
    n = 128
    px = []
    for j in range(n):
        for i in range(n):
            d = math.hypot((i + 0.5) / n - 0.5, (j + 0.5) / n - 0.5) / 0.5
            a = clamp(math.exp(-d * d * 4) - 0.02)
            px.append([1, 1, 1, a])
    write_tga(os.path.join(out, "bloom.tga"), n, n, px)


def main():
    out = os.path.join(sys.argv[1], "gfx", "tremor", "hud")
    os.makedirs(out, exist_ok=True)
    for f in (disc, health, armor, streak, smear, bloom):
        f(out)


if __name__ == "__main__":
    main()
