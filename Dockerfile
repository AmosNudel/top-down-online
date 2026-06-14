FROM debian:bookworm-slim AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
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
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

# Debian bookworm has no libraylib-dev — build raylib from source.
ARG RAYLIB_TAG=5.5
RUN git clone --depth 1 --branch "${RAYLIB_TAG}" https://github.com/raysan5/raylib.git /opt/raylib \
    || git clone --depth 1 https://github.com/raysan5/raylib.git /opt/raylib

WORKDIR /opt/raylib/src
RUN make RAYLIB_LIBTYPE=STATIC PLATFORM=PLATFORM_DESKTOP BUILD_MODE=RELEASE

WORKDIR /src
COPY . .

RUN test -f thirdparty/enet/include/enet/enet.h || ( \
    echo "ERROR: thirdparty/enet sources missing. Vendor ENet into the repo before deploying." && exit 1)

RUN make clean 2>/dev/null || true
RUN make \
    PROJECT_NAME=TopDownSurviveServer \
    RAYLIB_PATH=/opt/raylib \
    RAYLIB_LIBTYPE=STATIC \
    "OBJS=server_main.cpp BaseCharacter.cpp Character.cpp Enemy.cpp GameSimulation.cpp NetClient.cpp NetCommon.cpp NetServer.cpp NetStreamServer.cpp NetGameHost.cpp Pickup.cpp Prop.cpp Thunderstrike.cpp TileMap.cpp thirdparty/enet/callbacks.c thirdparty/enet/compress.c thirdparty/enet/host.c thirdparty/enet/list.c thirdparty/enet/packet.c thirdparty/enet/peer.c thirdparty/enet/protocol.c thirdparty/enet/unix.c" \
    "ENET_CFLAGS=-Ithirdparty/enet/include" \
    BUILD_MODE=RELEASE

FROM node:20-bookworm-slim AS relay

WORKDIR /relay
COPY relay/package.json relay/package-lock.json* ./
RUN npm install --omit=dev

# Final runtime: Node for relay + xvfb/xauth for headless raylib server.
FROM node:20-bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    libgl1 \
    libx11-6 \
    libxext6 \
    libxrandr2 \
    libxinerama1 \
    libxcursor1 \
    libxi6 \
    libasound2 \
    libstdc++6 \
    xvfb \
    xauth \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=builder /src/TopDownSurviveServer /app/TopDownSurviveServer
COPY --from=relay /relay/node_modules /app/relay/node_modules
COPY relay /app/relay
COPY docker/start.sh /app/start.sh
COPY characters map_tileset nature_tileset pickups sfx vfx /app/
RUN chmod +x /app/start.sh /app/TopDownSurviveServer

ENV GAME_TCP_HOST=127.0.0.1
ENV GAME_TCP_PORT=27016
ENV WS_PATH=/game

# Railway injects PORT at runtime; do not hardcode it here.
EXPOSE 8080

CMD ["/app/start.sh"]
