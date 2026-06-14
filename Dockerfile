FROM debian:bookworm-slim AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    g++ \
    make \
    libraylib-dev \
    libgl1-mesa-dev \
    libx11-dev \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN test -f thirdparty/enet/include/enet/enet.h || ( \
    echo "ERROR: thirdparty/enet sources missing. Vendor ENet into the repo before deploying." && exit 1)

RUN make clean 2>/dev/null || true
RUN make \
    PROJECT_NAME=TopDownSurviveServer \
    "OBJS=server_main.cpp BaseCharacter.cpp Character.cpp Enemy.cpp GameSimulation.cpp NetClient.cpp NetCommon.cpp NetServer.cpp NetStreamServer.cpp NetGameHost.cpp Pickup.cpp Prop.cpp Thunderstrike.cpp TileMap.cpp thirdparty/enet/callbacks.c thirdparty/enet/compress.c thirdparty/enet/host.c thirdparty/enet/list.c thirdparty/enet/packet.c thirdparty/enet/peer.c thirdparty/enet/protocol.c thirdparty/enet/unix.c" \
    "ENET_CFLAGS=-Ithirdparty/enet/include" \
    BUILD_MODE=RELEASE

FROM node:20-bookworm-slim AS relay

WORKDIR /relay
COPY relay/package.json relay/package-lock.json* ./
RUN npm install --omit=dev

FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    libraylib-dev \
    libgl1 \
    libx11-6 \
    xvfb \
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
ENV PORT=8080
ENV WS_PATH=/game

EXPOSE 8080

CMD ["xvfb-run", "-a", "/app/start.sh"]
