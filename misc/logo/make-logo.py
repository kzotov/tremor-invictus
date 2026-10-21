"""Generate the Tremor Invictus logo SVGs with all lettering outlined to paths.

Usage: python3 make-logo.py FONT_DIR OUT_DIR

FONT_DIR holds Cinzel-700.ttf and Cinzel-900.ttf (Cinzel is SIL OFL, from
Google Fonts). The fonts are only needed to regenerate; the SVGs embed none.
Requires fontTools (pip install fonttools).
"""
import os, sys
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

FONTS, OUT = sys.argv[1], sys.argv[2]
F900 = TTFont(os.path.join(FONTS, "Cinzel-900.ttf"))
F700 = TTFont(os.path.join(FONTS, "Cinzel-700.ttf"))


def outline(font, text, x, y, size, length):
    """Text as one path, letter spacing stretched so the run spans `length`."""
    upm = font["head"].unitsPerEm
    cmap = font.getBestCmap()
    gs = font.getGlyphSet()
    hmtx = font["hmtx"]
    s = size / upm
    names = [cmap[ord(c)] for c in text]
    natural = sum(hmtx[n][0] for n in names) * s
    extra = (length - natural) / (len(names) - 1)
    pen = SVGPathPen(gs, ntos=lambda v: ("%.2f" % v).rstrip("0").rstrip("."))
    cx = x
    for n in names:
        gs[n].draw(TransformPen(pen, (s, 0, 0, -s, cx, y)))
        cx += hmtx[n][0] * s + extra
    return pen.getCommands()


THEMES = {
    "dark": dict(red="#d3191f", stops=[(0, "#f6f5f1"), (.34, "#b3b2ad"), (.5, "#5a5b5e"), (.6, "#8e8e8b"), (1, "#d8d7d1")], fx=True),
    "light": dict(red="#a5131a", stops=[(0, "#4c4e53"), (.45, "#2a2b2f"), (.55, "#121214"), (1, "#3a3c41")], fx=False),
}


def defs(t, masks=""):
    stops = "".join(f'<stop offset="{o}" stop-color="{c}"/>' for o, c in t["stops"])
    fx = ""
    if t["fx"]:
        fx = ('<filter id="grit" x="-5%" y="-5%" width="110%" height="110%">'
              '<feTurbulence type="fractalNoise" baseFrequency=".7" numOctaves="2" seed="7" result="n"/>'
              '<feColorMatrix in="n" type="matrix" values="0 0 0 0 0  0 0 0 0 0  0 0 0 0 0  0 0 0 -1.3 1.4" result="spots"/>'
              '<feComposite in="SourceGraphic" in2="spots" operator="in"/></filter>'
              '<filter id="glow" x="-50%" y="-50%" width="200%" height="200%">'
              '<feGaussianBlur stdDeviation="1.4" result="b"/><feMerge><feMergeNode in="b"/><feMergeNode in="SourceGraphic"/></feMerge></filter>')
    return f'<defs><linearGradient id="metal" x1="0" y1="0" x2="0.15" y2="1">{stops}</linearGradient>{fx}{masks}</defs>'


def emblem(t):
    fx = ' filter="url(#grit)"' if t["fx"] else ""
    glow = ' filter="url(#glow)"' if t["fx"] else ""
    return (f'<circle cx="50" cy="50" r="42" fill="none" stroke="url(#metal)" stroke-width="7"{fx}/>'
            f'<polyline points="12,52 28,52 34,45 40,59 47,23 55,79 62,37 67,60 72,47 77,52 88,52" fill="none" '
            f'stroke="{t["red"]}" stroke-width="6.5" stroke-linejoin="miter" stroke-miterlimit="3.2"{glow}/>')


def crack_mask(w, h, points):
    pts = " ".join(f"{x},{y}" for x, y in points)
    return (f'<mask id="crack" maskUnits="userSpaceOnUse" x="0" y="0" width="{w}" height="{h}">'
            f'<rect width="{w}" height="{h}" fill="#fff"/>'
            f'<polyline points="{pts}" fill="none" stroke="#000" stroke-width="3.4" stroke-miterlimit="10"/></mask>')


# Seismograph-shaped fracture, relative to the spike's start and the crack's baseline.
SPIKE = [(0, 0), (16, -12), (30, 12), (46, -48), (66, 46), (82, -22), (96, 10), (108, -6), (118, 0)]


def wordmark(t, x, y_tremor, y_invictvs):
    fx = ' filter="url(#grit)"' if t["fx"] else ""
    tremor = outline(F900, "TREMOR", x, y_tremor, 102, 548)
    invictvs = outline(F700, "INVICTVS", x + 4, y_invictvs, 29, 540)
    return (f'<g mask="url(#crack)"{fx}><path d="{tremor}" fill="url(#metal)"/></g>'
            f'<path d="{invictvs}" fill="{t["red"]}"/>')


def svg(w, h, body, t, masks="", label="Tremor Invictus"):
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w} {h}" width="{w}" height="{h}" '
            f'role="img" aria-label="{label}">{defs(t, masks)}{body}</svg>\n')


def horizontal(t):
    mid = 88
    pts = [(170, mid)] + [(318 + dx, mid + dy) for dx, dy in SPIKE] + [(760, mid)]
    mask = crack_mask(760, 200, pts)
    body = f'<g transform="translate(8,24) scale(1.52)">{emblem(t)}</g>' + wordmark(t, 188, 122, 170)
    return svg(760, 200, body, t, mask)


def stacked(t):
    mid = 216
    pts = [(0, mid)] + [(262 + dx, mid + dy) for dx, dy in SPIKE] + [(640, mid)]
    mask = crack_mask(640, 340, pts)
    body = f'<g transform="translate(250,8) scale(1.4)">{emblem(t)}</g>' + wordmark(t, 46, 252, 302)
    return svg(640, 340, body, t, mask)


def emblem_svg(t):
    return svg(100, 100, emblem(t), t, label="Tremor Invictus emblem")


os.makedirs(OUT, exist_ok=True)
for name, t in THEMES.items():
    for kind, fn in (("horizontal", horizontal), ("stacked", stacked), ("emblem", emblem_svg)):
        with open(os.path.join(OUT, f"tremor-invictus-{kind}-on-{name}.svg"), "w") as f:
            f.write(fn(t))
