const http = require("http");
const net = require("net");
const WebSocket = require("ws");

const GAME_TCP_HOST = process.env.GAME_TCP_HOST || "127.0.0.1";
const GAME_TCP_PORT = Number(
  process.env.RAILWAY_TCP_APPLICATION_PORT ||
    process.env.GAME_TCP_PORT ||
    27016
);

let PORT = Number(process.env.PORT || 8080);
if (!Number.isFinite(PORT) || PORT <= 0) {
  console.error("Invalid PORT env:", process.env.PORT);
  process.exit(1);
}

if (PORT === GAME_TCP_PORT) {
  PORT = Number(process.env.HTTP_PORT || 8080);
  console.warn(
    `PORT conflict: game TCP is ${GAME_TCP_PORT}; relay HTTP/WebSocket using ${PORT}.`
  );
  console.warn(
    "In Railway: Networking → public domain → set target port to",
    String(PORT)
  );
}

console.log(`Relay booting on 0.0.0.0:${PORT}`);
const WS_PATH = process.env.WS_PATH || "/game";

function writeFrame(socket, payload) {
  const header = Buffer.alloc(2);
  header.writeUInt16BE(payload.length, 0);
  socket.write(Buffer.concat([header, payload]));
}

const server = http.createServer((req, res) => {
  const path = (req.url || "/").split("?")[0];
  if (path === "/health" || path === "/health/") {
    res.writeHead(200, { "Content-Type": "text/plain" });
    res.end("ok");
    return;
  }

  res.writeHead(200, { "Content-Type": "text/plain" });
  res.end("Top Down Survive relay\n");
});

const wss = new WebSocket.Server({ server, path: WS_PATH });

wss.on("connection", (ws, req) => {
  const tcp = net.connect(GAME_TCP_PORT, GAME_TCP_HOST);
  const tcpState = { buffer: Buffer.alloc(0) };

  const cleanup = () => {
    if (ws.readyState === WebSocket.OPEN) {
      try {
        ws.close();
      } catch (_) {}
    }
    if (!tcp.destroyed) {
      tcp.destroy();
    }
  };

  ws.on("message", (data) => {
    if (!Buffer.isBuffer(data)) {
      return;
    }
    if (tcp.writable) {
      writeFrame(tcp, data);
    }
  });

  ws.on("close", cleanup);
  ws.on("error", cleanup);

  tcp.on("connect", () => {
    tcp.setNoDelay(true);
  });

  tcp.on("data", (chunk) => {
    tcpState.buffer = Buffer.concat([tcpState.buffer, chunk]);
    while (tcpState.buffer.length >= 2) {
      const payloadLen = tcpState.buffer.readUInt16BE(0);
      if (payloadLen > 60000) {
        cleanup();
        return;
      }
      const frameSize = 2 + payloadLen;
      if (tcpState.buffer.length < frameSize) {
        break;
      }
      const payload = tcpState.buffer.subarray(2, frameSize);
      tcpState.buffer = tcpState.buffer.subarray(frameSize);
      if (ws.readyState === WebSocket.OPEN) {
        ws.send(payload, { binary: true });
      }
    }
  });

  tcp.on("error", cleanup);
  tcp.on("close", cleanup);
});

server.listen(PORT, "0.0.0.0", () => {
  console.log(`Relay listening on 0.0.0.0:${PORT}${WS_PATH}`);
  console.log(`Health check: http://0.0.0.0:${PORT}/health`);
  console.log(`Forwarding WebSocket -> TCP ${GAME_TCP_HOST}:${GAME_TCP_PORT}`);
});

server.on("error", (err) => {
  console.error("Relay failed to start:", err);
  process.exit(1);
});
