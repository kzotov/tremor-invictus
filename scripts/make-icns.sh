#!/bin/sh
# Make the macOS app icon from the logo emblem: the on-dark emblem on a dark
# rounded tile, rendered with Quick Look and packed with iconutil.
# Usage: scripts/make-icns.sh emblem.svg out.icns
set -e
SVG="$1"
OUT="$2"
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

# the emblem's 100x100 artwork inset on a 1024 tile with macOS's rounded corners
INNER=$(sed -e 's/^<svg[^>]*>//' -e 's#</svg>$##' "$SVG")
cat > "$WORK/icon.svg" <<SVG
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1024 1024" width="1024" height="1024">
<defs><linearGradient id="tile" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#2a2a2d"/><stop offset="1" stop-color="#121214"/></linearGradient></defs>
<rect x="100" y="100" width="824" height="824" rx="185" fill="url(#tile)"/>
<g transform="translate(172 172) scale(6.8)">$INNER</g>
</svg>
SVG

qlmanage -t -s 1024 -o "$WORK" "$WORK/icon.svg" >/dev/null 2>&1
PNG="$WORK/icon.svg.png"
[ -f "$PNG" ] || { echo "make-icns: Quick Look could not render $SVG" >&2; exit 1; }

mkdir -p "$WORK/icon.iconset"
for s in 16 32 128 256 512; do
	sips -z $s $s "$PNG" --out "$WORK/icon.iconset/icon_${s}x${s}.png" >/dev/null
	d=$((s * 2))
	sips -z $d $d "$PNG" --out "$WORK/icon.iconset/icon_${s}x${s}@2x.png" >/dev/null
done
iconutil -c icns "$WORK/icon.iconset" -o "$OUT"
