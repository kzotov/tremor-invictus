"""Generate the console background.

Usage: python3 make-console.py OUT_DIR

Writes OUT_DIR/gfx/tremor/console.tga: 512 x 256, opaque. Dark brushed
gunmetal with soft noise, fine horizontal grain, a faint grid of panel
seams and the logo red heartbeat line along the bottom edge; the console
shader (tremor/console) stretches it over the dropped console.
"""
import math, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tremorart import *


def heartbeat(u):
    """The trace's height (0 flat, +1 up) along u in 0..1: flat with three beats."""
    y = 0.0
    for c in (0.22, 0.55, 0.83):
        d = u - c
        if -0.02 < d < 0.0:
            y += (d + 0.02) / 0.02 * 0.4
        elif 0.0 <= d < 0.015:
            y += 0.4 - d / 0.015 * 1.4
        elif 0.015 <= d < 0.035:
            y += -1.0 + (d - 0.015) / 0.02 * 1.6
        elif 0.035 <= d < 0.05:
            y += 0.6 - (d - 0.035) / 0.015 * 0.6
    return y


def main():
    out = os.path.join(sys.argv[1], "gfx", "tremor")
    os.makedirs(out, exist_ok=True)
    w, h = 512, 256
    soft, grain = Fbm(91, 4, 3), Fbm(93, 2, 64)
    px = []
    for j in range(h):
        v = j / (h - 1)
        for i in range(w):
            u = i / (w - 1)
            n = 0.8 + 0.4 * soft.at(u * 2, v * 1)
            g = 0.92 + 0.16 * grain.at(u * 0.5, v * 16)
            base = [c * n * g for c in BACK]
            # panel seams every 1/8 across and 1/4 down, barely there
            if min(u * 8 % 1, v * 4 % 1) < 0.004:
                base = [c * 1.6 + 0.01 for c in base]
            # a dark band low down, then the heartbeat in logo red
            if v > 0.82:
                base = [c * 0.6 for c in base]
                trace = 0.91 - 0.05 * heartbeat(u)
                d = abs(v - trace)
                glow = max(0.0, 1 - d / 0.03) ** 2
                core = 1.0 if d < 0.006 else 0.0
                base = [b + r * (0.5 * glow + 0.6 * core) for b, r in zip(base, RED)]
            px.append([min(1.0, c) for c in base] + [1.0])
    write_tga(os.path.join(out, "console.tga"), w, h, px, False)


if __name__ == "__main__":
    main()
