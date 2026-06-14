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

Railway adds **`RAILWAY_TCP_APPLICATION_PORT=27016`** when you enable TCP Proxy — **leave it**, you cannot (and should not) delete it. That tells the C++ game server which port to accept desktop TCP connections on.

You run **two listeners** in one container:

| Listener | Port | Purpose |
|----------|------|---------|
| Node relay | `8080` (or Railway `PORT` if different) | HTTPS `/health`, WebSocket `/game` |
| C++ game server | `27016` (`RAILWAY_TCP_APPLICATION_PORT`) | Desktop `--transport tcp` via TCP Proxy |

### If `/health` returns 502

Railway sometimes sets `PORT=27016` (same as the TCP proxy port). HTTP then hits the game server instead of Node → 502.

**Fix in Railway UI:**

1. **Networking → Public domain** (your `*.up.railway.app` URL)
2. Set **target port** to **`8080`** (not 27016)
3. Redeploy

Deploy logs should show:

```
Boot: relay PORT=8080 game TCP=27016 (RAILWAY_TCP=27016)
Relay listening on 0.0.0.0:8080/game
```

### Desktop clients (TCP Proxy)

- **Networking → TCP Proxy** → internal port **27016** (creates `RAILWAY_TCP_APPLICATION_PORT`)
- Public address e.g. `thomas.proxy.rlwy.net:13034` → use in `play_online.bat`
- This is separate from the HTTPS domain / port 8080

### Variables

| Variable | Set by | Purpose |
|----------|--------|---------|
| `RAILWAY_TCP_APPLICATION_PORT` | Railway (TCP Proxy) | Game server TCP — **keep** |
| `PORT` | Railway (HTTP domain) | Relay HTTP/WS — should be **8080**, not 27016 |
| `GAME_TCP_PORT` | Optional override | Same as above if not using Railway TCP var |

## Protocol

- WebSocket: one binary message = one game packet (`NetCommon.h` structs).
- TCP (internal): `[uint16_be length][payload]` framing via `NetFraming.h`.

The relay is transport-only; the C++ server remains authoritative.
