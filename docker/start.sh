#!/bin/sh
# Keep the relay as the main process so Railway always gets /health even if
# the game server crashes or is still starting.

GAME_TCP_PORT="${GAME_TCP_PORT:-27016}"

echo "Starting game server on TCP ${GAME_TCP_PORT} and UDP 27015..."
./TopDownSurviveServer --tcp-port "${GAME_TCP_PORT}" --port 27015 &
SERVER_PID=$!

cleanup() {
  kill "${SERVER_PID}" 2>/dev/null || true
}
trap cleanup INT TERM EXIT

echo "Starting WebSocket relay on PORT ${PORT:-8080}..."
cd relay
exec node index.js
