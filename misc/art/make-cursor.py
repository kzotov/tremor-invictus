"""Generate the menu pointer.

Usage: python3 make-cursor.py OUT_DIR

Writes OUT_DIR/gfx/tremor/cursor.tga: 64 x 64 with alpha, an arrow whose
tip is the top-left corner (the hot spot), bone white with a dark edge and
a red stripe.
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tremorart import *


def main():
    out = os.path.join(sys.argv[1], "gfx", "tremor")
    os.makedirs(out, exist_ok=True)
    c = Canvas(64, 64)
    arrow = [(0.04, 0.03), (0.04, 0.78), (0.24, 0.6), (0.38, 0.92), (0.5, 0.86), (0.36, 0.55), (0.62, 0.55)]
    c.fill(polygon(arrow).outline(0.05), INK)
    c.fill(polygon(arrow), SILVER)
    c.fill(polygon([(0.1, 0.18), (0.1, 0.62), (0.22, 0.5)]), RED)
    write_tga(os.path.join(out, "cursor.tga"), 64, 64, c.px)


if __name__ == "__main__":
    main()
