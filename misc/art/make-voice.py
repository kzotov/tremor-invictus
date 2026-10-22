"""Generate the voice chat icons for the scoreboard and team overlay.

Usage: python3 make-voice.py OUT_DIR

Writes OUT_DIR/gfx/tremor/speaker.tga (a speaker with two sound waves,
shown for whoever is talking) and speaker_muted.tga (the speaker crossed
out in red, for a muted player): 64 x 64 with alpha, bone white with a
dark edge.
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tremorart import *


def speaker():
    return union(box(0.22, 0.5, 0.1, 0.12, 0.02),
                 polygon([(0.28, 0.38), (0.5, 0.18), (0.5, 0.82), (0.28, 0.62)]))


def main():
    out = os.path.join(sys.argv[1], "gfx", "tremor")
    os.makedirs(out, exist_ok=True)

    waves = union(arc(0.55, 0.5, 0.14, W, -50, 50), arc(0.55, 0.5, 0.3, W, -55, 55))
    c = Canvas(64, 64)
    c.fill(union(speaker(), waves).outline(0.04), INK)
    c.fill(speaker(), SILVER)
    c.fill(waves, SILVER)
    write_tga(os.path.join(out, "speaker.tga"), 64, 64, c.px)

    cross = union(line(0.6, 0.32, 0.9, 0.68, W * 1.3), line(0.6, 0.68, 0.9, 0.32, W * 1.3))
    c = Canvas(64, 64)
    c.fill(union(speaker(), cross).outline(0.04), INK)
    c.fill(speaker(), SILVER)
    c.fill(cross, RED)
    write_tga(os.path.join(out, "speaker_muted.tga"), 64, 64, c.px)


if __name__ == "__main__":
    main()
