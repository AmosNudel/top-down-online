FROM node:20-bookworm-slim AS relay

WORKDIR /relay
COPY relay/package.json relay/package-lock.json* ./
RUN npm install --omit=dev

FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    libgl1 \
    libx11-6 \
    xvfb \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=relay /relay/node_modules /app/relay/node_modules
COPY relay /app/relay
COPY docker/start.sh /app/start.sh
RUN chmod +x /app/start.sh

# Game binary + assets are copied in by CI/build step before docker build,
# or mount locally during development:
#   docker build -t top-down-survive-server .
COPY TopDownSurviveServer /app/TopDownSurviveServer
COPY characters map_tileset nature_tileset pickups sfx vfx /app/

ENV GAME_TCP_HOST=127.0.0.1
ENV GAME_TCP_PORT=27016
ENV PORT=8080
ENV WS_PATH=/game

EXPOSE 8080

CMD ["xvfb-run", "-a", "/app/start.sh"]
