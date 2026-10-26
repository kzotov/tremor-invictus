#!/bin/sh
# Package a finished build for testers.
# Usage: scripts/package.sh macos|windows|server [extra.pk3 ...]
#   macos    build-dist-macos  -> build-dist/tremor-invictus-<version>-macos.dmg
#   windows  build-windows     -> build-dist/tremor-invictus-<version>-windows.zip
#   server   build-linux       -> build-dist/tremor-invictus-<version>-server.tar.gz
# Build first with scripts/build-macos.sh (into build-dist-macos, see below),
# build-windows.sh or build-linux.sh. Extra .pk3 files (local test data from
# outside the repo) are added as zz-local-<name>, sorting after our own data
# as they do in the run scripts. No Quake III data is ever packed: players
# copy their own pak0.pk3 to pak8.pk3 into baseq3.
# The macOS package is made from its own build folder so the play build is
# never rebuilt for it:
#   cmake -S . -B build-dist-macos -DCMAKE_BUILD_TYPE=Release && cmake --build build-dist-macos
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
TARGET="${1:?usage: scripts/package.sh macos|windows|server [extra.pk3 ...]}"
shift

case "$TARGET" in
macos) BUILD="$ROOT/build-dist-macos"; EXT=dylib ;;
windows) BUILD="$ROOT/build-windows"; EXT=dll ;;
server) BUILD="$ROOT/build-linux"; EXT=so ;;
*) echo "unknown target $TARGET" >&2; exit 1 ;;
esac
RELEASE="$BUILD/Release"
# the generated art is the same on every platform; the Docker builds have no
# Python, so they take it from a macOS build
# (a container build may draw it too, but without the fonts)
GENERATED="$ROOT/build-dist-macos/generated"
[ -f "$GENERATED/tremor-assets.pk3" ] || GENERATED="$ROOT/build-macos/generated"
VERSION=$(cat "$BUILD/version.txt" 2>/dev/null || echo dev)
NAME="tremor-invictus-$VERSION-$TARGET"
OUT="$ROOT/build-dist"
STAGE="$OUT/$NAME/Tremor Invictus"

rm -rf "$OUT/$NAME"
mkdir -p "$STAGE/baseq3"

# our loose data (shaders, menus, presets...) as one pack that sorts before
# tremor-assets.pk3, the same order the run scripts give the loose folders
DATA=$(mktemp -d)
trap 'rm -rf "$DATA"' EXIT
for d in scripts ui presets mappools configs hud; do
	cp -R "$ROOT/assets/baseq3/$d" "$DATA/$d"
done
mkdir -p "$DATA/gfx"
cp -R "$GENERATED/gfx/tremor" "$DATA/gfx/tremor"
(cd "$DATA" && zip -qrX "$STAGE/baseq3/tremor-0data.pk3" . -x '*.DS_Store' 'gfx/tremor/art.stamp')
cp "$GENERATED/tremor-assets.pk3" "$STAGE/baseq3/"

for f in "$@"; do
	# only map data may ride along: never local sound, model or weapon packs,
	# nor anything from the reference folders
	case "$f" in
	*reference*|*sound*|*weapon*|*portrait*|*model*|*.zip)
		echo "package: refusing $f (not a map pack)" >&2; exit 1 ;;
	esac
	cp "$f" "$STAGE/baseq3/zz-local-$(basename "$f")"
done

# licence and source notices
cp "$ROOT/COPYING.txt" "$STAGE/COPYING.txt"
cp "$ROOT/misc/THIRD-PARTY.txt" "$STAGE/THIRD-PARTY.txt"
mkdir -p "$STAGE/licenses"
cp "$ROOT/code/thirdparty/freetype-2.13.3/LICENSE.TXT" "$STAGE/licenses/FreeType-LICENSE.txt"
cp "$ROOT/code/thirdparty/freetype-2.13.3/docs/FTL.TXT" "$STAGE/licenses/FreeType-FTL.txt"
cp "$ROOT/code/thirdparty/RmlUi-6.3/LICENSE.txt" "$STAGE/licenses/RmlUi-LICENSE.txt"
unzip -p "$GENERATED/tremor-assets.pk3" ui/fonts/LICENSE-Roboto.txt > "$STAGE/licenses/Roboto-LICENSE.txt"
COMMIT=$(git -C "$ROOT" rev-parse HEAD)

if [ "$TARGET" = server ]; then
	cp "$RELEASE/baseq3/qagame.$EXT" "$STAGE/baseq3/"
	cp "$RELEASE/tremor-invictus-ded" "$STAGE/" 2>/dev/null || cp "$RELEASE"/ioq3ded "$STAGE/"
else
	for m in cgame qagame ui; do
		cp "$RELEASE/baseq3/$m.$EXT" "$STAGE/baseq3/"
	done
	# load the native game modules above (the engine default is QVMs)
	printf '%s\n' 'set vm_game 0' 'set vm_cgame 0' 'set vm_ui 0' > "$STAGE/baseq3/autoexec.cfg"
fi

case "$TARGET" in
macos)
	cp -R "$RELEASE/tremor-invictus.app" "$STAGE/"
	# seal the whole bundle (the linker signs only the executable), or a
	# downloaded copy is reported as damaged
	codesign --force --deep --sign - "$STAGE/tremor-invictus.app"
	;;
windows)
	for f in "$RELEASE"/*.exe "$RELEASE"/*.dll; do
		[ -f "$f" ] && cp "$f" "$STAGE/"
	done
	;;
esac

cat > "$STAGE/README.txt" <<EOF
Tremor Invictus $VERSION, test build

1. Quake III Arena data (not included):
   pak0.pk3     from your own Quake III Arena (Steam or GOG, Windows
                version: baseq3/pak0.pk3 in the install folder; on a Mac,
                unpack the GOG installer with innoextract)
   pak1-pak8    id's free 1.32 patch files, quake3-latest-pk3s.zip from
                https://ioquake3.org/extras/patch-data/
   Put all nine into:
   macOS:   ~/Library/Application Support/Quake3/baseq3/
   Windows: the baseq3 folder next to tremor-invictus.exe
2. Start the game:
   macOS:   copy the "Tremor Invictus" folder out of the disk image, then
            once in Terminal:
              xattr -dr com.apple.quarantine "<that folder>"
            (otherwise macOS runs the app from a hidden copy that can't
            see the maps next to it), then open tremor-invictus.app
   Windows: tremor-invictus.exe
3. Connect from the console (~):  connect <server address>
   A password-protected server needs:  password <password>  before connect.

Bind the champion ability with:  bind e +button12

Source and licences
Tremor Invictus is free software under the GNU General Public License v2
(COPYING.txt), built on ioquake3 and id Software's Quake III Arena source.
Source: https://github.com/kzotov/tremor-invictus
This build: commit $COMMIT
Bundled libraries and fonts: THIRD-PARTY.txt and the licenses folder
(FreeType License, MIT, Apache-2.0, zlib, BSD).
EOF

case "$TARGET" in
macos)
	rm -f "$OUT/$NAME.dmg"
	hdiutil create -quiet -volname "Tremor Invictus" -fs HFS+ -format UDZO \
		-srcfolder "$OUT/$NAME" "$OUT/$NAME.dmg"
	echo "Packaged $OUT/$NAME.dmg"
	;;
windows)
	rm -f "$OUT/$NAME.zip"
	(cd "$OUT/$NAME" && zip -qr "$OUT/$NAME.zip" "Tremor Invictus")
	echo "Packaged $OUT/$NAME.zip"
	;;
server)
	tar -C "$OUT/$NAME" -czf "$OUT/$NAME.tar.gz" "Tremor Invictus"
	echo "Packaged $OUT/$NAME.tar.gz"
	;;
esac
