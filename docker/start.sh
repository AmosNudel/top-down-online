#!/bin/sh
set -e

GAME_TCP_PORT="${GAME_TCP_PORT:-27016}"

echo "Starting game server on TCP ${GAME_TCP_PORT} and UDP 27015..."
./TopDownSurviveServer --tcp-port "${GAME_TCP_PORT}" --port 27015 &
SERVER_PID=$!

echo "Starting WebSocket relay on PORT ${PORT:-8080}..."
cd relay
npm start &
RELAY_PID=$!

trap 'kill ${SERVER_PID} ${RELAY_PID} 2>/dev/null || true' INT TERM
wait ${SERVER_PID} ${RELAY_PID}
