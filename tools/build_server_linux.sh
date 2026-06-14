#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

apt-get update
apt-get install -y --no-install-recommends \
  build-essential \
  libraylib-dev \
  libgl1-mesa-dev \
  libx11-dev \
  pkg-config

make clean || true
make \
  PROJECT_NAME=TopDownSurviveServer \
  "OBJS=server_main.cpp BaseCharacter.cpp Character.cpp Enemy.cpp GameSimulation.cpp NetClient.cpp NetCommon.cpp NetServer.cpp NetStreamServer.cpp NetGameHost.cpp Pickup.cpp Prop.cpp Thunderstrike.cpp TileMap.cpp thirdparty/enet/callbacks.c thirdparty/enet/compress.c thirdparty/enet/host.c thirdparty/enet/list.c thirdparty/enet/packet.c thirdparty/enet/peer.c thirdparty/enet/protocol.c thirdparty/enet/unix.c" \
  "ENET_CFLAGS=-Ithirdparty/enet/include" \
  BUILD_MODE=RELEASE

echo "Built: $ROOT/TopDownSurviveServer"
