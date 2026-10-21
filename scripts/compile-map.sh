#!/bin/sh
# Compile a map with q3map2 and pack it for the game.
# Usage: scripts/compile-map.sh path/to/name.map [test|release]
#
# Stages: BSP (-meta), visibility (-vis, -fast while testing), lighting
# (-light -fast; release adds patch shadows, 2 samples and 2 bounces).
# Then zz-map-<name>.pk3 gets maps/<name>.bsp, levelshots/<name>.jpg (if
# maps/<name>.jpg sits next to the .map) and scripts/<name>.arena (if
# <name>.arena sits next to the .map), and goes into the play build's home
# folder (build-macos/home, or TREMOR_HOME). Compiled files and pk3s never enter the repository.
#
# q3map2: Q3MAP2 or ~/projects/quake/tools/q3map2/q3map2 or q3map2 on PATH.
# Game data: TREMOR_Q3 is the folder that holds baseq3 (textures, shaders).
set -e

MAP="$1"
MODE="${2:-test}"
[ -f "$MAP" ] || { echo "usage: $0 path/to/name.map [test|release]" >&2; exit 1; }
case "$MODE" in test|release) ;; *) echo "compile-map: mode is test or release" >&2; exit 1 ;; esac

Q3MAP2="${Q3MAP2:-$HOME/projects/quake/tools/q3map2/q3map2}"
[ -x "$Q3MAP2" ] || Q3MAP2=$(command -v q3map2 || true)
[ -n "$Q3MAP2" ] && [ -x "$Q3MAP2" ] || { echo "compile-map: q3map2 not found (set Q3MAP2)" >&2; exit 1; }
[ -n "$TREMOR_Q3" ] && [ -d "$TREMOR_Q3/baseq3" ] || { echo "compile-map: set TREMOR_Q3 to the folder with baseq3" >&2; exit 1; }

ROOT=$(cd "$(dirname "$0")/.." && pwd)
NAME=$(basename "$MAP" .map)
SRC=$(cd "$(dirname "$MAP")" && pwd)
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
mkdir -p "$WORK/maps"
cp "$MAP" "$WORK/maps/$NAME.map"

GAME="-game quake3 -fs_basepath $TREMOR_Q3 -fs_game baseq3"
if [ -d "$ROOT/assets/baseq3" ]; then
	GAME="$GAME -fs_homepath $ROOT/assets"
fi

echo "== bsp"
"$Q3MAP2" $GAME -meta "$WORK/maps/$NAME.map"
echo "== vis"
if [ "$MODE" = test ]; then
	"$Q3MAP2" $GAME -vis -fast "$WORK/maps/$NAME.bsp"
	echo "== light (fast)"
	"$Q3MAP2" $GAME -light -fast "$WORK/maps/$NAME.bsp"
else
	"$Q3MAP2" $GAME -vis "$WORK/maps/$NAME.bsp"
	echo "== light"
	"$Q3MAP2" $GAME -light -fast -patchshadows -samples 2 -bounce 2 "$WORK/maps/$NAME.bsp"
fi

# pack
mkdir -p "$WORK/pk3/maps" "$WORK/pk3/levelshots" "$WORK/pk3/scripts"
cp "$WORK/maps/$NAME.bsp" "$WORK/pk3/maps/"
[ -f "$SRC/$NAME.jpg" ] && cp "$SRC/$NAME.jpg" "$WORK/pk3/levelshots/$NAME.jpg"
[ -f "$SRC/$NAME.arena" ] && cp "$SRC/$NAME.arena" "$WORK/pk3/scripts/$NAME.arena"

OUT="${TREMOR_HOME:-$ROOT/build-macos/home}/baseq3"
mkdir -p "$OUT"
( cd "$WORK/pk3" && zip -qr "$WORK/zz-map-$NAME.pk3" . )
cp "$WORK/zz-map-$NAME.pk3" "$OUT/"
echo "compile-map: $OUT/zz-map-$NAME.pk3"
