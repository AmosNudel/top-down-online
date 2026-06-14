#!/bin/sh
# Start the HTTP/WebSocket relay immediately (Railway healthcheck hits /health).
# Run the raylib game server under xvfb in the background.

GAME_TCP_PORT="${GAME_TCP_PORT:-27016}"

echo "Boot: PORT=${PORT:-unset} GAME_TCP_PORT=${GAME_TCP_PORT}"

(
  cd /app || exit 1
  echo "Starting game server under xvfb..."
  xvfb-run -a ./TopDownSurviveServer --tcp-port "${GAME_TCP_PORT}" --port 27015
) >> /tmp/game-server.log 2>&1 &
SERVER_PID=$!

cleanup() {
  kill "${SERVER_PID}" 2>/dev/null || true
}
trap cleanup INT TERM EXIT

echo "Starting WebSocket relay on PORT ${PORT:-8080}..."
cd /app/relay || exit 1
exec node index.js
