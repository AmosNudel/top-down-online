#!/bin/sh
# Start the HTTP/WebSocket relay immediately (Railway healthcheck hits /health).
# Run the raylib game server under xvfb in the background.

# Railway TCP Proxy sets RAILWAY_TCP_APPLICATION_PORT (e.g. 27016) — keep it.
# The C++ game server listens there; the Node relay must use a different PORT.
if [ -n "${RAILWAY_TCP_APPLICATION_PORT:-}" ]; then
  GAME_TCP_PORT="${RAILWAY_TCP_APPLICATION_PORT}"
else
  GAME_TCP_PORT="${GAME_TCP_PORT:-27016}"
fi

RELAY_PORT="${PORT:-8080}"
if [ "$RELAY_PORT" = "$GAME_TCP_PORT" ]; then
  RELAY_PORT=8080
  export PORT=8080
  echo "WARNING: PORT matched game TCP port ${GAME_TCP_PORT}."
  echo "Relay will listen on 8080 for HTTP/WebSocket (/health)."
  echo "In Railway: Networking → your public domain → set target port to 8080."
fi

echo "Boot: relay PORT=${RELAY_PORT} game TCP=${GAME_TCP_PORT} (RAILWAY_TCP=${RAILWAY_TCP_APPLICATION_PORT:-unset})"

(
  cd /app || exit 1
  echo "Starting game server under xvfb on TCP ${GAME_TCP_PORT}..."
  xvfb-run -a ./TopDownSurviveServer --tcp-port "${GAME_TCP_PORT}" --port 27015
) >> /tmp/game-server.log 2>&1 &
SERVER_PID=$!

cleanup() {
  kill "${SERVER_PID}" 2>/dev/null || true
}
trap cleanup INT TERM EXIT

echo "Starting WebSocket relay on PORT ${RELAY_PORT}..."
cd /app/relay || exit 1
exec node index.js
