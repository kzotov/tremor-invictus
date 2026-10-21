#!/bin/bash
# Quiet test runs on a Mac someone is working on. A run is never heard or
# seen, never takes the keyboard, mouse or focus, and is driven only through
# rcon on localhost. Each run has its own name, home folder and port, so
# several sessions can test at once without touching each other.
#
# Usage:
#   scripts/test-run.sh build                     build the separate test build
#   scripts/test-run.sh start [-n name] [-p port] [+set/+map args...]
#   scripts/test-run.sh rcon  [-n name] "command"
#   scripts/test-run.sh stop  [-n name]
#   scripts/test-run.sh status
#
# Quake III Arena data: set TREMOR_Q3 to the folder that holds baseq3/pak0.pk3.
# The console log is build-test/home-<name>/baseq3/qconsole.log (complete
# after stop).
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
BUILD="$ROOT/build-test"
RELEASE="$BUILD/Release"
APP="$BUILD/tremor-test.app"
BUNDLE_ID="org.tremor-invictus.test"

cmd="${1:-}"
[ $# -gt 0 ] && shift
name="test"
port=""
while [ $# -gt 0 ]; do
	case "$1" in
	-n) name="$2"; shift 2 ;;
	-p) port="$2"; shift 2 ;;
	*) break ;;
	esac
done
HOME_DIR="$BUILD/home-$name"

die() { echo "test-run: $*" >&2; exit 1; }

# send one rcon command to our own run and print the reply
send_rcon() {
	python3 -I - "$(cat "$HOME_DIR/port")" "$(cat "$HOME_DIR/rcon")" "$1" <<'PY'
import socket, sys
port, password, command = int(sys.argv[1]), sys.argv[2], sys.argv[3]
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.settimeout(2.0)
s.sendto(b"\xff\xff\xff\xffrcon " + password.encode() + b" " + command.encode(), ("127.0.0.1", port))
try:
    while True:
        data, _ = s.recvfrom(65536)
        text = data[4:].decode("latin-1")
        if text.startswith("print\n"):
            text = text[6:]
        sys.stdout.write(text)
        s.settimeout(0.3)
except socket.timeout:
    pass
PY
}

# pid of our run, only if it is still our game process
our_pid() {
	[ -f "$HOME_DIR/pid" ] || return 1
	local pid
	pid=$(cat "$HOME_DIR/pid")
	ps -p "$pid" -o command= 2>/dev/null | grep -qF "$HOME_DIR" || return 1
	echo "$pid"
}

case "$cmd" in
build)
	cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release
	cmake --build "$BUILD" -j"$(sysctl -n hw.ncpu)"
	# own bundle id, accessory app (no Dock icon, never activated)
	rm -rf "$APP"
	cp -R "$RELEASE/tremor-invictus.app" "$APP"
	/usr/libexec/PlistBuddy -c "Set :CFBundleIdentifier $BUNDLE_ID" "$APP/Contents/Info.plist"
	/usr/libexec/PlistBuddy -c "Delete :LSUIElement" "$APP/Contents/Info.plist" 2>/dev/null || true
	/usr/libexec/PlistBuddy -c "Add :LSUIElement bool true" "$APP/Contents/Info.plist"
	codesign --force --deep -s - "$APP" >/dev/null 2>&1
	echo "Test build ready: $APP"
	;;

start)
	[ -x "$APP/Contents/MacOS/tremor-invictus" ] || die "no test build; run: scripts/test-run.sh build"
	[ -f "${TREMOR_Q3:-}/baseq3/pak0.pk3" ] || die "set TREMOR_Q3 to the folder with baseq3/pak0.pk3"
	our_pid >/dev/null && die "run '$name' is already going; stop it first"
	[ -n "$port" ] || port=$(( 28000 + $(printf '%s' "$name" | cksum | cut -d' ' -f1) % 1000 ))

	mkdir -p "$HOME_DIR/baseq3"
	for f in cgame.dylib qagame.dylib ui.dylib vm; do
		ln -sfn "$RELEASE/baseq3/$f" "$HOME_DIR/baseq3/$f"
	done
	# our own data files (shaders), never game data
	ln -sfn "$ROOT/assets/baseq3/scripts" "$HOME_DIR/baseq3/scripts"
	ln -sfn "$ROOT/assets/baseq3/ui" "$HOME_DIR/baseq3/ui"
	# images made at build time (the menu logo)
	mkdir -p "$HOME_DIR/baseq3/gfx"
	ln -sfn "$BUILD/generated/gfx/tremor" "$HOME_DIR/baseq3/gfx/tremor"
	# local test assets from outside the repo (see scripts/local-assets.sh)
	. "$ROOT/scripts/local-assets.sh"
	LOCAL_ARGS=$(link_local_assets "$HOME_DIR/baseq3")

	# no stale "game is running" marker, so no safe-mode dialog
	rm -f "$HOME_DIR"/baseq3/*.pid "$HOME_DIR/baseq3/qconsole.log"
	echo "$port" > "$HOME_DIR/port"
	head -c 12 /dev/urandom | od -An -tx1 | tr -d ' \n' > "$HOME_DIR/rcon"

	SDL_MAC_BACKGROUND_APP=1 "$APP/Contents/MacOS/tremor-invictus" \
		+set fs_basepath "$TREMOR_Q3" +set fs_homepath "$HOME_DIR" \
		+set vm_game 0 +set vm_cgame 0 +set vm_ui 0 +set sv_pure 0 \
		+set s_initsound 0 +set s_volume 0 +set s_musicvolume 0 +set s_alCapture 0 \
		+set cl_voip 0 +set cl_useMumble 0 \
		+set in_ignoreInput 1 +set in_mouse 0 +set in_joystick 0 \
		+set r_fullscreen 0 +set r_offscreenWindow 1 +set r_mode -1 \
		+set r_customwidth 1280 +set r_customheight 720 \
		+set net_port "$port" +set net_enabled 1 +set rconpassword "$(cat "$HOME_DIR/rcon")" \
		+set logfile 2 +set com_introplayed 1 +set cl_allowDownload 0 \
		$LOCAL_ARGS "$@" > "$HOME_DIR/stdout.log" 2>&1 &
	echo $! > "$HOME_DIR/pid"
	echo "started '$name' (pid $!, port $port, home $HOME_DIR)"
	;;

rcon)
	our_pid >/dev/null || die "run '$name' is not going"
	send_rcon "$*"
	;;

stop)
	if pid=$(our_pid); then
		send_rcon "quit" >/dev/null || true
		for _ in $(seq 1 50); do
			kill -0 "$pid" 2>/dev/null || break
			sleep 0.2
		done
		if kill -0 "$pid" 2>/dev/null; then
			kill "$pid"
			echo "stopped '$name' by signal (rcon quit timed out)"
		else
			echo "stopped '$name'"
		fi
	else
		echo "run '$name' is not going"
	fi
	rm -f "$HOME_DIR/pid"
	;;

status)
	ps -axo pid=,command= | grep -F "$BUILD/" | grep -v grep || echo "no test runs"
	;;

*)
	sed -n '2,17p' "$0"
	exit 1
	;;
esac
