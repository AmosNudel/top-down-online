#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

apt-get update
apt-get install -y --no-install-recommends \
  build-essential \
  g++ \
  make \
  git \
  ca-certificates \
  libgl1-mesa-dev \
  libglu1-mesa-dev \
  libx11-dev \
  libxrandr-dev \
  libxinerama-dev \
  libxcursor-dev \
  libxi-dev \
  libasound2-dev \
  pkg-config

RAYLIB_TAG="${RAYLIB_TAG:-5.5}"
if [ ! -f /opt/raylib/src/libraylib.a ]; then
  git clone --depth 1 --branch "${RAYLIB_TAG}" https://github.com/raysan5/raylib.git /opt/raylib \
    || git clone --depth 1 https://github.com/raysan5/raylib.git /opt/raylib
  make -C /opt/raylib/src RAYLIB_LIBTYPE=STATIC PLATFORM=PLATFORM_DESKTOP BUILD_MODE=RELEASE
fi

make clean || true
make \
  PROJECT_NAME=TopDownSurviveServer \
  RAYLIB_PATH=/opt/raylib \
  RAYLIB_LIBTYPE=STATIC \
  "OBJS=server_main.cpp BaseCharacter.cpp Character.cpp Enemy.cpp GameSimulation.cpp NetClient.cpp NetCommon.cpp NetServer.cpp NetStreamServer.cpp NetGameHost.cpp Pickup.cpp Prop.cpp Thunderstrike.cpp TileMap.cpp thirdparty/enet/callbacks.c thirdparty/enet/compress.c thirdparty/enet/host.c thirdparty/enet/list.c thirdparty/enet/packet.c thirdparty/enet/peer.c thirdparty/enet/protocol.c thirdparty/enet/unix.c" \
  "ENET_CFLAGS=-Ithirdparty/enet/include" \
  BUILD_MODE=RELEASE

echo "Built: $ROOT/TopDownSurviveServer"
