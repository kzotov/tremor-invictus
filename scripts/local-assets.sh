#!/bin/sh
# Shared by the run scripts: link local test assets into a home folder.
# Usage: . scripts/local-assets.sh; link_local_assets <home>/baseq3
#
# TREMOR_LOCAL (default ~/projects/quake/local) is a folder outside the repo
# for assets that may be tried locally but never committed or shipped:
#   *.pk3 / *.zip   player models, maps and other test data, loaded as pk3 files
#                   (also baseq3/*.pk3)
#   last/*.pk3      packs that must win over every other pack, the local ones
#                   and the painted looks included (finished champion art):
#                   linked as zzzz-local-<name>, the last name in load order
#   local.cfg       run on start, for example:
#                   set g_championModels "deathknight=dark_knight galena=galena"
# Prints the extra command-line arguments to pass to the game.

TREMOR_LOCAL="${TREMOR_LOCAL:-$HOME/projects/quake/local}"

link_local_assets() {
	gamedir="$1"
	rm -f "$gamedir"/zz-local-*.pk3 "$gamedir"/zzzz-local-*.pk3 "$gamedir/tremor-local.cfg"
	[ -d "$TREMOR_LOCAL" ] || return 0
	for f in "$TREMOR_LOCAL"/*.pk3 "$TREMOR_LOCAL"/*.zip "$TREMOR_LOCAL"/baseq3/*.pk3; do
		[ -f "$f" ] || continue
		name=$(basename "$f")
		ln -sf "$f" "$gamedir/zz-local-$name.pk3"
	done
	for f in "$TREMOR_LOCAL"/last/*.pk3; do
		[ -f "$f" ] || continue
		ln -sf "$f" "$gamedir/zzzz-local-$(basename "$f")"
	done
	if [ -f "$TREMOR_LOCAL/local.cfg" ]; then
		ln -s "$TREMOR_LOCAL/local.cfg" "$gamedir/tremor-local.cfg"
		echo "+exec tremor-local.cfg"
	fi
}

# The champions' looks (misc/art/bake-looks.py), painted from the player
# models in the player's own pak0.pk3 into <build>/looks/zz-tremor-looks.pk3
# and linked into the home folder. They stay on this machine: never in the
# repo, tremor-assets.pk3 or a package. Painted again only when pak0 or the
# art scripts change; without Python the champions wear the models' skins.
# The game does not wait for a paint (a first one takes a few minutes): the
# last pack is used and the new one is ready at the next start. With
# "wait" (test runs) the paint finishes first.
# Usage: bake_local_looks <Quake 3 folder> <build folder> <home>/baseq3 [wait]
bake_local_looks() {
	pak="$1/baseq3/pak0.pk3"
	out="$2/looks/zz-tremor-looks.pk3"
	gamedir="$3"
	art="$ROOT/misc/art"			# (ROOT: the repo, set by the run scripts)
	rm -f "$gamedir/zz-tremor-looks.pk3" "$gamedir/zz-tremor-form.pk3"
	[ -f "$pak" ] && command -v python3 >/dev/null 2>&1 || return 0
	mkdir -p "$2/looks"
	stamp=$( (ls -l "$pak"; cat "$art"/*.py "$art"/looks/*.py) | cksum | cut -d' ' -f1)
	if [ ! -f "$out" ] || [ "$(cat "$2/looks/stamp" 2>/dev/null)" != "$stamp" ]; then
		# one paint at a time (a lock folder, cleared if its painter is gone)
		lock="$2/looks/painting"
		if [ -d "$lock" ] && ! kill -0 "$(cat "$lock/pid" 2>/dev/null)" 2>/dev/null; then
			rm -rf "$lock"
		fi
		if mkdir "$lock" 2>/dev/null; then
			paint="python3 -I \"$art/bake-looks.py\" \"$1/baseq3\" \"$2/looks\" \"$out\" && echo $stamp > \"$2/looks/stamp\"; rm -rf \"$lock\""
			if [ "${4:-}" = wait ]; then
				echo $$ > "$lock/pid"
				sh -c "$paint" >&2
			else
				echo "Painting the champions' looks from your Quake III models in the background (ready at the next start)" >&2
				nohup sh -c "$paint" >"$2/looks/paint.log" 2>&1 &
				echo $! > "$lock/pid"
			fi
		fi
	fi
	[ -f "$out" ] && ln -sf "$out" "$gamedir/zz-tremor-looks.pk3"
	return 0
}
