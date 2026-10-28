#!/bin/sh
# Cross-compile tremor-invictus for 64-bit Windows with mingw-w64, inside a
# Debian container, so nothing has to be installed on the host. The native gcc
# builds the QVM tools that run during the build.
# Usage: scripts/build-windows.sh [extra cmake -D options...]
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
IMAGE=tremor-invictus-mingw

if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
	docker build -t "$IMAGE" - <<'DOCKERFILE'
FROM debian:trixie
RUN apt-get update \
	&& apt-get install -y --no-install-recommends \
		cmake ninja-build git ca-certificates gcc libc6-dev \
		gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64 binutils-mingw-w64-x86-64 \
	&& rm -rf /var/lib/apt/lists/*
DOCKERFILE
fi

# the container has no Python: the UI branding pack's icons (and textures, for
# a package made from this build) are staged here on the host, from outside
# the repository (misc/art/import-brand-art.py)
mkdir -p "$ROOT/build-windows/generated/gfx/tremor"
python3 -I "$ROOT/misc/art/import-brand-art.py" sync "$ROOT/build-windows/generated" \
	--icons "$ROOT/build-windows/brand" || true

docker run --rm -v "$ROOT":/src -w /src "$IMAGE" sh -ec '
	git config --global --add safe.directory /src
	cmake -S . -B build-windows -G Ninja -DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64-x86_64.cmake "$@"
	cmake --build build-windows
' sh "$@"
echo "Built into $ROOT/build-windows/Release"
