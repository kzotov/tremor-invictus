#!/bin/sh
# Build tremor-invictus natively on macOS.
# Usage: scripts/build-macos.sh [extra cmake -D options...]
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
BUILD="$ROOT/build-macos"

cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release "$@"
cmake --build "$BUILD" -j"$(sysctl -n hw.ncpu)"
echo "Built into $BUILD/Release"
