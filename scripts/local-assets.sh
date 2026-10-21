#!/bin/sh
# Shared by the run scripts: link local test assets into a home folder.
# Usage: . scripts/local-assets.sh; link_local_assets <home>/baseq3
#
# TREMOR_LOCAL (default ~/projects/quake/local) is a folder outside the repo
# for assets that may be tried locally but never committed or shipped:
#   *.pk3 / *.zip   player models and other test data, loaded as pk3 files
#   local.cfg       run on start, for example:
#                   set g_championModels "deathknight=dark_knight galena=galena"
# Prints the extra command-line arguments to pass to the game.

TREMOR_LOCAL="${TREMOR_LOCAL:-$HOME/projects/quake/local}"

link_local_assets() {
	gamedir="$1"
	rm -f "$gamedir"/zz-local-*.pk3 "$gamedir/tremor-local.cfg"
	[ -d "$TREMOR_LOCAL" ] || return 0
	for f in "$TREMOR_LOCAL"/*.pk3 "$TREMOR_LOCAL"/*.zip; do
		[ -f "$f" ] || continue
		name=$(basename "$f")
		ln -s "$f" "$gamedir/zz-local-${name%.*}.pk3"
	done
	if [ -f "$TREMOR_LOCAL/local.cfg" ]; then
		ln -s "$TREMOR_LOCAL/local.cfg" "$gamedir/tremor-local.cfg"
		echo "+exec tremor-local.cfg"
	fi
}
