#!/bin/sh
# Render the logo for the menus: the horizontal on-dark lockup as a PNG the
# engine can load (gfx/tremor/logo.png under OUTDIR). Quick Look renders
# onto white, so the lockup sits on black and the menus add it over their
# background.
# Usage: scripts/make-logo-images.sh logo-dir out-dir
set -e
LOGO="$1"
OUT="$2/gfx/tremor"
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
mkdir -p "$OUT"

# Quick Look renders into a 1024 square, so put the 760 x 200 lockup in the
# middle of a square and crop that band back out
INNER=$(sed -e 's/^<svg[^>]*>//' -e 's#</svg>$##' "$LOGO/tremor-invictus-horizontal-on-dark.svg")
cat > "$WORK/logo.svg" <<SVG
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 760 760" width="760" height="760">
<rect width="760" height="760" fill="#000000"/>
<g transform="translate(0 280)">$INNER</g>
</svg>
SVG
qlmanage -t -s 1024 -o "$WORK" "$WORK/logo.svg" >/dev/null 2>&1
PNG="$WORK/logo.svg.png"
[ -f "$PNG" ] || { echo "make-logo-images: Quick Look could not render the logo" >&2; exit 1; }
sips -c 270 1024 "$PNG" --out "$OUT/logo.png" >/dev/null
