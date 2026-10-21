#!/bin/sh
# Build the master server (the server list) for this machine.
# Usage: scripts/build-master.sh            -> build-master/tremor-master
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
mkdir -p "$ROOT/build-master"
cc -O2 -Wall -o "$ROOT/build-master/tremor-master" "$ROOT/code/tools/master/tremor-master.c"
echo "Built $ROOT/build-master/tremor-master"
