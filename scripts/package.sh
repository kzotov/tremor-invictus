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
GENERATED="$BUILD/generated"
[ -f "$GENERATED/tremor-assets.pk3" ] || GENERATED="$ROOT/build-dist-macos/generated"
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
	cp "$f" "$STAGE/baseq3/zz-local-$(basename "$f")"
done

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

1. Copy pak0.pk3 to pak8.pk3 from your own Quake III Arena install
   (its baseq3 folder) into the baseq3 folder next to this file.
2. Start the game:
   macOS:   tremor-invictus.app (first time: right-click, Open, as the
            build is not notarised)
   Windows: tremor-invictus.exe
3. Connect from the console (~):  connect <server address>
   A password-protected server needs:  password <password>  before connect.

Bind the champion ability with:  bind e +button12
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
