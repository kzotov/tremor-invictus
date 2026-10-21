#!/bin/sh
# Build tremor-invictus for 64-bit x86 Linux inside an Ubuntu 22.04 container
# (glibc 2.35) with a current CMake from PyPI, so the result runs on most current distributions. On an arm64
# Mac, Docker runs the container under emulation, which is slow but works.
# Usage: scripts/build-linux.sh [extra cmake -D options...]
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
IMAGE=tremor-invictus-linux

if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
	docker build --platform linux/amd64 -t "$IMAGE" - <<'DOCKERFILE'
FROM ubuntu:22.04
RUN apt-get update \
	&& DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
		build-essential ninja-build git ca-certificates python3-pip \
		libsdl2-dev libgl-dev libcurl4-openssl-dev libopenal-dev \
	&& pip3 install --no-cache-dir "cmake>=3.25,<4" && rm -rf /var/lib/apt/lists/*
DOCKERFILE
fi

docker run --rm --platform linux/amd64 -v "$ROOT":/src -w /src "$IMAGE" sh -ec '
	git config --global --add safe.directory /src
	cmake -S . -B build-linux -G Ninja -DCMAKE_BUILD_TYPE=Release "$@"
	cmake --build build-linux
' sh "$@"
echo "Built into $ROOT/build-linux/Release"
