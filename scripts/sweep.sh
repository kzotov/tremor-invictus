#!/bin/bash
# Behaviour sweep: quiet test runs that exercise every champion's ability,
# every weapon and every mode's start and end, with a screenshot of each and
# the console's errors and warnings, into build-test/sweep/report.md.
#
# Usage: scripts/sweep.sh [champions|weapons|modes|all]
# Needs a test build (scripts/test-run.sh build) and TREMOR_Q3.
set -uo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
RUN="$ROOT/scripts/test-run.sh"
OUT="$ROOT/build-test/sweep"
NAME=sweep
HOME_DIR="$ROOT/build-test/home-$NAME"
SHOTS="$HOME_DIR/baseq3/screenshots"
what="${1:-all}"

mkdir -p "$OUT" "$HOME_DIR/baseq3"
REPORT="$OUT/report.md"
echo "# Behaviour sweep $(date '+%Y-%m-%d %H:%M')" > "$REPORT"

r() { "$RUN" rcon -n "$NAME" "$1" 2>/dev/null; }
wait_map() {
	for _ in $(seq 1 60); do
		r "status" | grep -q "map:" && return 0
		sleep 1
	done
	return 1
}
shot() {
	r "screenshotJPEG sweep_$1" >/dev/null
	sleep 0.7
	[ -f "$SHOTS/sweep_$1.jpg" ] && mv "$SHOTS/sweep_$1.jpg" "$OUT/$1.jpg"
}
start() {
	"$RUN" stop -n "$NAME" >/dev/null 2>&1
	rm -rf "$SHOTS"
	printf '%s\n' "$@" > "$HOME_DIR/baseq3/sweep.cfg"
	"$RUN" start -n "$NAME" -p 27979 +exec sweep.cfg >/dev/null
	wait_map
}
problems() {
	grep -aiE "error|warning|couldn't|could not" "$HOME_DIR/baseq3/qconsole.log" \
		| grep -viE "synchronousClients|no font face|RmlUi: could not load ui-gen" | sort | uniq -c | sort -rn | head -12
}

champions() {
	echo "## Champions: spawn and ability" >> "$REPORT"
	start "set com_maxfps 125" "set cg_thirdperson 1" "exec presets/ffa.cfg" "devmap dm6" || { echo "map did not load" >> "$REPORT"; return; }
	r "team free" >/dev/null
	sleep 2
	r "rml_hide" >/dev/null
	for c in ranger anarki athena nyx slash bj deathknight doomslayer eisen galena strogg visor clutch keel scalebearer sorlag wrack sarge klesk hunter xaero raine beastmaster unumrey; do
		r "set champion $c" >/dev/null
		r "kill" >/dev/null
		sleep 3.5
		r "+button12" >/dev/null
		sleep 0.25
		r "-button12" >/dev/null
		sleep 0.5
		shot "champ_$c"
		echo "- $c: ![]($( [ -f "$OUT/champ_$c.jpg" ] && echo "champ_$c.jpg" || echo missing))" >> "$REPORT"
	done
	echo '```' >> "$REPORT"; problems >> "$REPORT"; echo '```' >> "$REPORT"
}

weapons() {
	echo "## Weapons: in hand and firing" >> "$REPORT"
	start "set com_maxfps 125" "exec presets/ffa.cfg" "devmap dm6" || { echo "map did not load" >> "$REPORT"; return; }
	r "team free" >/dev/null
	sleep 2
	r "rml_hide" >/dev/null
	r "give all" >/dev/null
	for w in 1 2 3 4 5 6 7 8 9; do
		r "weapon $w" >/dev/null
		sleep 1.2
		r "+attack" >/dev/null
		sleep 0.3
		shot "weapon_$w"
		r "-attack" >/dev/null
		echo "- weapon $w: ![](weapon_$w.jpg)" >> "$REPORT"
	done
	echo '```' >> "$REPORT"; problems >> "$REPORT"; echo '```' >> "$REPORT"
}

modes() {
	echo "## Modes: warm-up, start and end with bots" >> "$REPORT"
	for m in ffa duel tdm ctf ca sacrifice roundduel instagib unholytrinity; do
		start "set com_maxfps 125" "set g_log games.log" "set g_logsync 1" "set bot_enable 1" "exec presets/$m.cfg" \
			"set timelimit 1" "set fraglimit 0" "set g_warmupMax 15" "set g_readyUp 0" "set g_warmupTime 5" "devmap dm6" \
			|| { echo "- $m: map did not load" >> "$REPORT"; continue; }
		r "team free" >/dev/null
		r "addbot sarge 2" >/dev/null
		r "addbot klesk 2" >/dev/null
		sleep 3
		r "rml_hide" >/dev/null
		shot "mode_${m}_warmup"
		r "timescale 8" >/dev/null
		sleep 14
		r "timescale 1" >/dev/null
		shot "mode_${m}_late"
		started=$(grep -c "InitGame" "$HOME_DIR/baseq3/games.log" 2>/dev/null)
		ended=$(grep -aE "^ *[0-9]+:[0-9]+ Exit:" "$HOME_DIR/baseq3/games.log" | tail -1)
		echo "- $m: game starts $started, end: ${ended:-none yet} ![](mode_${m}_warmup.jpg) ![](mode_${m}_late.jpg)" >> "$REPORT"
		echo '```' >> "$REPORT"; problems >> "$REPORT"; echo '```' >> "$REPORT"
	done
}

case "$what" in
champions) champions ;;
weapons) weapons ;;
modes) modes ;;
*) champions; weapons; modes ;;
esac
"$RUN" stop -n "$NAME" >/dev/null 2>&1
echo "report: $REPORT"
