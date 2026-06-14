# WebSocket Relay

Bridges browser/desktop WebSocket clients to the C++ game server's TCP stream port.

## Local dev

Terminal 1 — game server:

```powershell
.\tools\build_server.ps1
.\dist-server\TopDownSurviveServer.exe --port 27015 --tcp-port 27016
```

Terminal 2 — relay:

```powershell
cd relay
npm install
npm start
```

Terminal 3 — desktop client via relay path:

```powershell
.\TopDownSurvive.exe --transport tcp --host 127.0.0.1 --port 27016
```

Or WebSocket web build:

```html
<script>
  window.GAME_WS_URL = 'ws://127.0.0.1:8080/game';
</script>
```

## Railway

1. Build Linux server binary + Docker image (see `Dockerfile`).
2. Deploy service; Railway sets `PORT` for the relay.
3. Set `GAME_TCP_PORT=27016` (default).
4. Point web clients at `wss://<your-service>.up.railway.app/game`.
5. Optional: enable **TCP Proxy** on port `27016` for native desktop clients using `--transport tcp`.

## Environment

| Variable | Default | Purpose |
|----------|---------|---------|
| `PORT` | `8080` | Public HTTP/WebSocket port (Railway provides this) |
| `GAME_TCP_HOST` | `127.0.0.1` | Internal game server host |
| `GAME_TCP_PORT` | `27016` | Internal TCP stream port |
| `WS_PATH` | `/game` | WebSocket upgrade path |

## Protocol

- WebSocket: one binary message = one game packet (`NetCommon.h` structs).
- TCP (internal): `[uint16_be length][payload]` framing via `NetFraming.h`.

The relay is transport-only; the C++ server remains authoritative.
