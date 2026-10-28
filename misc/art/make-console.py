"""Generate the console background.

Usage: python3 make-console.py OUT_DIR

Writes OUT_DIR/gfx/tremor/console.tga: 512 x 256, opaque. Dark brushed
gunmetal with soft noise, fine horizontal grain, a faint grid of panel
seams and a subdued straight red seam near the bottom edge; the console
shader (tremor/console) stretches it over the dropped console. A
placeholder: the approved branding pack replaces it
(misc/art/import-brand-art.py).
"""
import math, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tremorart import *


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
            # a dark band low down, then a straight seam in a quiet logo red
            if v > 0.82:
                base = [c * 0.6 for c in base]
                d = abs(v - 0.94)
                glow = max(0.0, 1 - d / 0.02) ** 2
                core = 1.0 if d < 0.004 else 0.0
                base = [b + r * (0.15 * glow + 0.3 * core) for b, r in zip(base, RED)]
            px.append([min(1.0, c) for c in base] + [1.0])
    write_tga(os.path.join(out, "console.tga"), w, h, px, False)


if __name__ == "__main__":
    main()
