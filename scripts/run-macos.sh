#!/bin/sh
# Run the macOS build against your own Quake III Arena data.
# Usage: scripts/run-macos.sh "/path/to/Quake 3 Arena" [extra +set / +map arguments...]
# The game data stays where it is; the build's game modules are loaded from a
# home folder next to the build (build-macos/home), so nothing is copied into
# the Quake 3 install. Bind the champion ability with: bind e +button12
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
RELEASE="$ROOT/build-macos/Release"
Q3="${1:?usage: scripts/run-macos.sh /path/to/Quake3 [args...]}"
shift
HOME_DIR="$ROOT/build-macos/home"

if [ ! -f "$Q3/baseq3/pak0.pk3" ]; then
	echo "No baseq3/pak0.pk3 in $Q3" >&2
	exit 1
fi
# link only the game modules so configs and logs stay out of the build output
mkdir -p "$HOME_DIR/baseq3"
for f in cgame.dylib qagame.dylib ui.dylib vm; do
	ln -sfn "$RELEASE/baseq3/$f" "$HOME_DIR/baseq3/$f"
done
# our own data files (shaders), never game data
ln -sfn "$ROOT/assets/baseq3/scripts" "$HOME_DIR/baseq3/scripts"
ln -sfn "$ROOT/assets/baseq3/ui" "$HOME_DIR/baseq3/ui"
ln -sfn "$ROOT/assets/baseq3/presets" "$HOME_DIR/baseq3/presets"
ln -sfn "$ROOT/assets/baseq3/mappools" "$HOME_DIR/baseq3/mappools"
ln -sfn "$ROOT/assets/baseq3/configs" "$HOME_DIR/baseq3/configs"
ln -sfn "$ROOT/assets/baseq3/hud" "$HOME_DIR/baseq3/hud"
# images made at build time (the menu logo)
mkdir -p "$HOME_DIR/baseq3/gfx"
ln -sfn "$ROOT/build-macos/generated/gfx/tremor" "$HOME_DIR/baseq3/gfx/tremor"
# models and replaceable ability assets made at build time (a pack;
# local packs sort after it and win)
ln -sfn "$ROOT/build-macos/generated/tremor-assets.pk3" "$HOME_DIR/baseq3/tremor-assets.pk3"

# local test assets from outside the repo (see scripts/local-assets.sh)
. "$ROOT/scripts/local-assets.sh"
LOCAL_ARGS=$(link_local_assets "$HOME_DIR/baseq3")
bake_local_form "$Q3" "$ROOT/build-macos" "$HOME_DIR/baseq3"

# the console output also goes to build-macos/home/run.log (a crash's
# backtrace with it; also in build-macos/home/crash.log)
set +e
"$RELEASE/tremor-invictus.app/Contents/MacOS/tremor-invictus" \
	+set fs_basepath "$Q3" +set fs_homepath "$HOME_DIR" \
	+set vm_game 0 +set vm_cgame 0 +set vm_ui 0 +set sv_pure 0 $LOCAL_ARGS "$@" 2>&1 | tee "$HOME_DIR/run.log"
