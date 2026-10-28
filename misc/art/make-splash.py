"""Generate the splash pictures: the backdrop behind the connect and loading
screens, and the picture for maps that have no levelshot.

Usage: python3 make-splash.py OUT_DIR

Writes to OUT_DIR/gfx/tremor/splash/:
  back.tga    1024 x 512 holding a 16:9 picture, shown cropped to the
              screen's shape (UI_DrawBackdrop): near-black metal and a large
              faint ring
  nomap.tga   256 x 256 holding a 4:3 picture (shown like a levelshot)
All drawn here from shapes and noise. These are placeholders: the approved
branding pack replaces both (misc/art/import-brand-art.py).
"""
import math, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tremorart import *

def backdrop(w, h, ring_at, seed):
    """Shared look; coordinates in units of the height (x runs 0..w/h)."""
    c = Canvas(w, h, (0, 0, 0, 1), span=w / h)
    aspect = w / h
    grain = Fbm(seed, 4, 3)
    fine = Noise(seed + 1, 256, 256)
    cx, cy = aspect * 0.5, 0.48

    def base(x, y, p):
        d = math.hypot((x - cx) / aspect * 1.6, y - cy)
        k = mix(1.9, 0.55, smooth(0.0, 0.75, d))
        k *= 1 + 0.25 * (grain.at(x, y) - 0.5) + 0.08 * (fine.at(x * 400, y * 400) - 0.5)
        # faint panel joints
        if abs((y * 6) % 1 - 0.5) > 0.495 or abs((x * 3) % 1 - 0.5) > 0.4985:
            k *= 0.75
        return (BACK[0] * k, BACK[1] * k, BACK[2] * k * 1.05, 1)
    c.shade(base)

    # the large ring, barely lit
    rx, ry, rr = ring_at
    rim = ring(rx, ry, rr, rr * 0.16)
    c.fill(rim, worn_metal(seed + 2, scale(STEEL, 0.26), scale(GUNMETAL, 0.09), ry - rr, ry + rr, 0.3))
    c.fill(ring(rx, ry, rr * 0.9, rr * 0.012), scale(RED_DEEP, 0.35))
    return c


def make_back():
    # drawn at 16:9, then squeezed into 1024 x 512 like the branding pack's
    return squeeze(backdrop(1024, 576, (1024 / 576 * 0.5, 0.48, 0.36), 11), 1024, 576, 512)


def squeeze(c, w, h, to):
    """The canvas's rows resampled from h down or up to `to` (the width stays)."""
    out = []
    for j in range(to):
        y = (j + 0.5) * h / to - 0.5
        j0 = max(0, min(h - 1, int(math.floor(y))))
        j1 = min(h - 1, j0 + 1)
        t = clamp(y - j0)
        for i in range(w):
            a, b = c.px[j0 * w + i], c.px[j1 * w + i]
            out.append([a[k] + (b[k] - a[k]) * t for k in range(4)])
    return out


def make_nomap():
    # drawn at 4:3, then stretched into the square texture
    c = backdrop(256, 192, (256 / 192 * 0.5, 0.5, 0.3), 12)
    # a mark in the middle: an empty hexagon, nothing to show here
    hexa = regular(256 / 192 * 0.5, 0.5, 0.12, 6, 30)
    c.fill(hexa.outline(0.018), worn_metal(5, STEEL, GUNMETAL, 0.38, 0.62, 0.2))
    return squeeze(c, 256, 192, 256)


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    out = os.path.join(sys.argv[1], "gfx", "tremor", "splash")
    os.makedirs(out, exist_ok=True)
    write_tga(os.path.join(out, "back.tga"), 1024, 512, make_back(), alpha=False)
    write_tga(os.path.join(out, "nomap.tga"), 256, 256, make_nomap(), alpha=False)


if __name__ == "__main__":
    main()
