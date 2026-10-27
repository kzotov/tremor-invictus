#!/bin/sh
# Shared by the run scripts: link local test assets into a home folder.
# Usage: . scripts/local-assets.sh; link_local_assets <home>/baseq3
#
# TREMOR_LOCAL (default ~/projects/quake/local) is a folder outside the repo
# for assets that may be tried locally but never committed or shipped:
#   *.pk3 / *.zip   player models, maps and other test data, loaded as pk3 files
#                   (also baseq3/*.pk3)
#   local.cfg       run on start, for example:
#                   set g_championModels "deathknight=dark_knight galena=galena"
# Prints the extra command-line arguments to pass to the game.

TREMOR_LOCAL="${TREMOR_LOCAL:-$HOME/projects/quake/local}"

link_local_assets() {
	gamedir="$1"
	rm -f "$gamedir"/zz-local-*.pk3 "$gamedir/tremor-local.cfg"
	[ -d "$TREMOR_LOCAL" ] || return 0
	for f in "$TREMOR_LOCAL"/*.pk3 "$TREMOR_LOCAL"/*.zip "$TREMOR_LOCAL"/baseq3/*.pk3; do
		[ -f "$f" ] || continue
		name=$(basename "$f")
		ln -sf "$f" "$gamedir/zz-local-$name.pk3"
	done
	if [ -f "$TREMOR_LOCAL/local.cfg" ]; then
		ln -s "$TREMOR_LOCAL/local.cfg" "$gamedir/tremor-local.cfg"
		echo "+exec tremor-local.cfg"
	fi
}

# The looks' form shading (misc/art/bake-form.py), worked out from the
# player models in the player's own pak0.pk3 into <build>/form/zz-tremor-form.pk3
# and linked into the home folder. It stays on this machine: it is never in
# the repo, tremor-assets.pk3 or a package (the looks ship it neutral).
# Baked again only when pak0 or the art scripts change; quiet without Python.
# Usage: bake_local_form <Quake 3 folder> <build folder> <home>/baseq3
bake_local_form() {
	pak="$1/baseq3/pak0.pk3"
	out="$2/form/zz-tremor-form.pk3"
	gamedir="$3"
	art="$(cd "$(dirname "$0")/.." && pwd)/misc/art"
	rm -f "$gamedir/zz-tremor-form.pk3"
	[ -f "$pak" ] && command -v python3 >/dev/null 2>&1 || return 0
	mkdir -p "$2/form"
	stamp=$( (ls -l "$pak"; cat "$art"/*.py "$art"/looks/*.py) | cksum | cut -d' ' -f1)
	if [ ! -f "$out" ] || [ "$(cat "$2/form/stamp" 2>/dev/null)" != "$stamp" ]; then
		python3 -I "$art/bake-form.py" "$1/baseq3" "$out" >&2 && echo "$stamp" > "$2/form/stamp" || return 0
	fi
	ln -sf "$out" "$gamedir/zz-tremor-form.pk3"
}
