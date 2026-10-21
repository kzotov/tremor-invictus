#!/bin/sh
# Install the Tremor Invictus game configuration for TrenchBroom (macOS),
# with the emblem rendered as the 32 px game icon.
# Usage: misc/trenchbroom/install.sh
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
DEST="$HOME/Library/Application Support/TrenchBroom/games/TremorInvictus"
mkdir -p "$DEST"
cp "$HERE/GameConfig.cfg" "$HERE/tremor-invictus.fgd" "$DEST/"

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
qlmanage -t -s 256 -o "$WORK" "$HERE/../logo/tremor-invictus-emblem-on-dark.svg" >/dev/null 2>&1 || true
if [ -f "$WORK/tremor-invictus-emblem-on-dark.svg.png" ]; then
	sips -z 32 32 "$WORK/tremor-invictus-emblem-on-dark.svg.png" --out "$DEST/icon.png" >/dev/null
fi
echo "Installed to $DEST"
echo "In TrenchBroom: Preferences > Tremor Invictus > Game Path = the folder holding baseq3"
