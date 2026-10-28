#!/bin/sh
# Package a finished build for testers.
# Usage: scripts/package.sh macos|windows|server [extra.pk3 ...]
#   macos    build-dist-macos  -> build-dist/tremor-invictus-<version>-macos.dmg
#   windows  build-windows     -> build-dist/tremor-invictus-<version>-windows.zip
#   server   build-linux       -> build-dist/tremor-invictus-<version>-server.tar.gz
# Build first with scripts/build-macos.sh (into build-dist-macos, see below),
# build-windows.sh or build-linux.sh. Extra .pk3 files (local test data from
# outside the repo) are added as zz-local-<name>, sorting after our own data
# as they do in the run scripts. With TREMOR_Q3_PATCH set (see below),
# Quake III's free point release files pak1-pak8 are packed too, with id's
# licence for them;
# pak0.pk3 never is: players copy their own into their home folder.
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
# the approved UI branding (a pack outside the repo) over the drawn
# placeholders; a package never ships the placeholder logo by accident:
# TREMOR_ALLOW_PLACEHOLDER_BRAND=1 allows it
python3 -I "$ROOT/misc/art/import-brand-art.py" sync "$GENERATED" || true
if BRAND=$(python3 -I "$ROOT/misc/art/import-brand-art.py" status "$GENERATED"); then
	echo "package: UI branding $BRAND"
elif [ "$TARGET" != server ]; then
	[ "${TREMOR_ALLOW_PLACEHOLDER_BRAND:-}" = 1 ] || {
		echo "package: $GENERATED has the placeholder UI art, not the branding pack (see misc/art/import-brand-art.py)" >&2
		exit 1; }
	echo "package: packing the placeholder UI art" >&2
fi
# the painted ability icons (a second pack outside the repo) the same way:
# TREMOR_ALLOW_DRAWN_ABILITY_ART=1 allows the drawn ones
python3 -I "$ROOT/misc/art/import-ability-art.py" sync "$GENERATED" || true
if ABILITY_ART=$(python3 -I "$ROOT/misc/art/import-ability-art.py" status "$GENERATED"); then
	echo "package: ability icons $ABILITY_ART"
elif [ "$TARGET" != server ]; then
	[ "${TREMOR_ALLOW_DRAWN_ABILITY_ART:-}" = 1 ] || {
		echo "package: $GENERATED has the drawn ability icons, not the painted pack (see misc/art/import-ability-art.py)" >&2
		exit 1; }
	echo "package: packing the drawn ability icons" >&2
fi
[ "$TARGET" = server ] || [ -f "$BUILD/brand/tremor-invictus.ico" ] ||
	echo "package: $BUILD was configured without the branding pack: the app icon is the placeholder (configure and build again)" >&2
VERSION=$(cat "$BUILD/version.txt" 2>/dev/null || echo dev)
NAME="tremor-invictus-$VERSION-$TARGET"
OUT="$ROOT/build-dist"
STAGE="$OUT/$NAME/Tremor Invictus"

rm -rf "$OUT/$NAME"
mkdir -p "$STAGE"
if [ "$TARGET" = macos ]; then
	# the data goes inside the app, so a downloaded copy that macOS starts
	# from a hidden location still has it (the engine looks there first)
	cp -R "$RELEASE/tremor-invictus.app" "$STAGE/"
	GAMEDIR="$STAGE/tremor-invictus.app/Contents/Resources/baseq3"
else
	GAMEDIR="$STAGE/baseq3"
fi
mkdir -p "$GAMEDIR"

# our loose data (shaders, menus, presets...) as one pack that sorts before
# tremor-assets.pk3, the same order the run scripts give the loose folders
DATA=$(mktemp -d)
trap 'rm -rf "$DATA"' EXIT
for d in scripts ui presets mappools configs hud; do
	cp -R "$ROOT/assets/baseq3/$d" "$DATA/$d"
done
mkdir -p "$DATA/gfx"
cp -R "$GENERATED/gfx/tremor" "$DATA/gfx/tremor"
# the painted charge icons (import-ability-art.py), when there are any
[ ! -d "$GENERATED/gfx/abilities" ] || cp -R "$GENERATED/gfx/abilities" "$DATA/gfx/abilities"
(cd "$DATA" && zip -qrX "$GAMEDIR/tremor-0data.pk3" . -x '*.DS_Store' 'gfx/tremor/art.stamp' 'gfx/tremor/brand.txt')
cp "$GENERATED/tremor-assets.pk3" "$GAMEDIR/"

for f in "$@"; do
	# only map data may ride along: never local sound, model or weapon packs,
	# the looks painted from the player's own models (zz-tremor-*), nor
	# anything from the reference folders
	case "$f" in
	*reference*|*sound*|*weapon*|*portrait*|*model*|*champion*|*/last/*|*.zip|*zz-tremor-*)
		echo "package: refusing $f (not a map pack)" >&2; exit 1 ;;
	esac
	cp "$f" "$GAMEDIR/zz-local-$(basename "$f")"
done

# Quake III's free 1.32 point release files (pak1-pak8) from TREMOR_Q3_PATCH,
# a folder with baseq3/pak1-8.pk3 and id-point-release-EULA.txt as ioquake3
# hands them out; id's licence must travel with them. pak0 is the player's
# own and never packed.
if [ "$TARGET" != server ] && [ -n "${TREMOR_Q3_PATCH:-}" ]; then
	[ -f "$TREMOR_Q3_PATCH/id-point-release-EULA.txt" ] || {
		echo "package: no id-point-release-EULA.txt in $TREMOR_Q3_PATCH" >&2; exit 1; }
	for n in 1 2 3 4 5 6 7 8; do
		cp "$TREMOR_Q3_PATCH/baseq3/pak$n.pk3" "$GAMEDIR/"
	done
	cp "$TREMOR_Q3_PATCH/id-point-release-EULA.txt" "$GAMEDIR/"
	mkdir -p "$STAGE/licenses"
	cp "$TREMOR_Q3_PATCH/id-point-release-EULA.txt" "$STAGE/licenses/"
fi

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
	cp "$RELEASE/baseq3/qagame.$EXT" "$GAMEDIR/"
	cp "$RELEASE/tremor-invictus-ded" "$STAGE/" 2>/dev/null || cp "$RELEASE"/ioq3ded "$STAGE/"
else
	for m in cgame qagame ui; do
		cp "$RELEASE/baseq3/$m.$EXT" "$GAMEDIR/"
	done
	# load the native game modules above (the engine default is QVMs)
	printf '%s\n' 'set vm_game 0' 'set vm_cgame 0' 'set vm_ui 0' > "$GAMEDIR/autoexec.cfg"
fi

case "$TARGET" in
macos)
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

1. Quake III Arena data: pak1-pak8 (id's free 1.32 point release) come
   with this build. pak0.pk3 comes from your own Quake III Arena (Steam:
   https://store.steampowered.com/app/2200, GOG:
   https://www.gog.com/en/game/quake_iii_arena; it is baseq3/pak0.pk3 in
   the install folder, and the GOG installer unpacks on a Mac with
   innoextract). Copy it to:
   macOS:   ~/Library/Application Support/TremorInvictus/baseq3/
   Windows: %APPDATA%\TremorInvictus\baseq3\  (or the baseq3 folder
            next to tremor-invictus.exe)
   Without it the game says where it looks and opens that folder.
2. Start the game:
   macOS:   drag tremor-invictus.app to Applications and open it (first
            time: right-click, Open, as the build is not notarised)
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
id's point release files (pak1-pak8): licenses/id-point-release-EULA.txt
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
