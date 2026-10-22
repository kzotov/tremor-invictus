#!/bin/sh
# Fails when a QVM is older than any of its sources, which means its build
# failed (lcc is C89: a declaration after a statement stops it) while the
# dylibs built fine.
# Usage: scripts/check-qvm.sh <Release folder>
ROOT=$(cd "$(dirname "$0")/.." && pwd)
VM="${1:?usage: check-qvm.sh <Release folder>}/baseq3/vm"
status=0
check() {
	qvm="$VM/$1.qvm"
	shift
	if [ ! -f "$qvm" ]; then
		echo "check-qvm: $qvm is missing" >&2
		status=1
		return
	fi
	newer=$(find "$@" -name '*.c' -newer "$qvm" | head -3)
	if [ -n "$newer" ]; then
		echo "check-qvm: $qvm is older than: $newer" >&2
		status=1
	fi
}
check cgame "$ROOT/code/cgame" "$ROOT"/code/game/bg_*.c "$ROOT/code/qcommon/q_shared.c" "$ROOT/code/qcommon/q_math.c"
check qagame "$ROOT/code/game"
check ui "$ROOT/code/q3_ui" "$ROOT"/code/game/bg_*.c "$ROOT/code/qcommon/q_shared.c" "$ROOT/code/qcommon/q_math.c"
exit $status
