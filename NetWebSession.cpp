#if defined(PLATFORM_WEB)

#include "NetWebSession.h"
#include "NetFraming.h"
#include <emscripten.h>
#include <cstring>
#include <string>

EM_JS(void, ReadGameWsUrl, (char *buf, int maxLen), {
    var url = (typeof window !== "undefined" && window.GAME_WS_URL) ? window.GAME_WS_URL : "";
    stringToUTF8(url, buf, maxLen);
});

std::string NetWebSession::configuredServerUrl()
{
    char buffer[512] = {};
    ReadGameWsUrl(buffer, static_cast<int>(sizeof(buffer)));
    return buffer;
}

EM_JS(int, NetWsConnect, (const char *url, int timeoutMs), {
    if (!Module.gameNet) {
        Module.gameNet = {
            ws: null,
            queue: [],
            connected: false,
            connectDeadline: 0
        };
    }

    var net = Module.gameNet;
    if (net.ws) {
        try { net.ws.close(); } catch (e) {}
        net.ws = null;
    }
    net.queue = [];
    net.connected = false;
    net.connectDeadline = Date.now() + timeoutMs;

    var wsUrl = UTF8ToString(url);
    try {
        net.ws = new WebSocket(wsUrl);
    } catch (e) {
        return 0;
    }
    net.ws.binaryType = 'arraybuffer';

    net.ws.onopen = function() {
        net.connected = true;
    };
    net.ws.onclose = function() {
        net.connected = false;
    };
    net.ws.onerror = function() {
        net.connected = false;
    };
    net.ws.onmessage = function(event) {
        if (event.data instanceof ArrayBuffer) {
            net.queue.push(new Uint8Array(event.data));
        }
    };

    return 1;
});

EM_JS(int, NetWsWaitForConnect, (), {
    var net = Module.gameNet;
    if (!net || !net.ws) return -1;
    if (net.connected) return 1;
    if (Date.now() > net.connectDeadline) return 0;
    return -1;
});

EM_JS(int, NetWsIsConnected, (), {
    return (Module.gameNet && Module.gameNet.connected) ? 1 : 0;
});

EM_JS(void, NetWsClose, (), {
    var net = Module.gameNet;
    if (!net) return;
    net.connected = false;
    if (net.ws) {
        try { net.ws.close(); } catch (e) {}
        net.ws = null;
    }
    net.queue = [];
});

EM_JS(int, NetWsSend, (const char *data, int len), {
    var net = Module.gameNet;
    if (!net || !net.ws || net.ws.readyState !== 1) return 0;
    var bytes = HEAPU8.subarray(data, data + len);
    net.ws.send(bytes);
    return 1;
});

EM_JS(int, NetWsTakeMessage, (char *buf, int maxLen), {
    var net = Module.gameNet;
    if (!net || net.queue.length === 0) return 0;
    var msg = net.queue.shift();
    var copyLen = Math.min(msg.length, maxLen);
    writeArrayToMemory(msg.subarray(0, copyLen), buf);
    return copyLen;
});

EM_JS(int, NetWsWasDisconnected, (), {
    var net = Module.gameNet;
    if (!net || !net.ws) return 1;
    if (net.connected) return 0;
    if (net.ws.readyState === WebSocket.CONNECTING) return 0;
    return 1;
});

void NetWebSession::setPacketHandler(PacketHandler handler)
{
    packetHandler = std::move(handler);
}

void NetWebSession::setDisconnectHandler(DisconnectHandler handler)
{
    disconnectHandler = std::move(handler);
}

bool NetWebSession::connect(const char *url, uint32_t timeoutMs)
{
    disconnect();
    if (!url || !url[0])
        return false;

    if (!NetWsConnect(url, static_cast<int>(timeoutMs)))
        return false;

    while (true)
    {
        const int state = NetWsWaitForConnect();
        if (state == 1)
        {
            connected = true;
            return true;
        }
        if (state == 0)
            break;
        emscripten_sleep(16);
    }

    disconnect();
    return false;
}

void NetWebSession::disconnect()
{
    NetWsClose();
    connected = false;
    assignedPlayerId = -1;
}

bool NetWebSession::isConnected() const
{
    return connected && NetWsIsConnected() != 0;
}

void NetWebSession::poll(int)
{
    if (!connected)
        return;

    if (NetWsWasDisconnected())
    {
        connected = false;
        if (disconnectHandler)
            disconnectHandler();
        return;
    }

    uint8_t buffer[NetFraming::kMaxPayload];
    while (true)
    {
        const int len = NetWsTakeMessage(reinterpret_cast<char *>(buffer), static_cast<int>(sizeof(buffer)));
        if (len <= 0)
            break;
        if (packetHandler)
            packetHandler(buffer, static_cast<size_t>(len));
    }
}

bool NetWebSession::sendPacket(const void *data, size_t size)
{
    if (!connected || !data || size == 0)
        return false;
    return NetWsSend(static_cast<const char *>(data), static_cast<int>(size)) != 0;
}

bool NetWebSession::sendJoin(const char *name)
{
    NetCJoin pkt{};
    NetSerialize::initHeader(pkt.hdr, NetMsgType::C_JOIN);
    if (name)
        std::strncpy(pkt.name, name, sizeof(pkt.name) - 1);
    return sendPacket(&pkt, sizeof(pkt));
}

bool NetWebSession::sendReady()
{
    NetCReady pkt{};
    NetSerialize::initHeader(pkt.hdr, NetMsgType::C_READY);
    return sendPacket(&pkt, sizeof(pkt));
}

bool NetWebSession::sendLeave()
{
    NetCLeave pkt{};
    NetSerialize::initHeader(pkt.hdr, NetMsgType::C_LEAVE);
    return sendPacket(&pkt, sizeof(pkt));
}

bool NetWebSession::sendInput(float moveX, float moveY, bool attackPressed, bool attackHeld, bool thunderPressed)
{
    NetCInput pkt{};
    NetSerialize::initHeader(pkt.hdr, NetMsgType::C_INPUT);
    pkt.moveX = moveX;
    pkt.moveY = moveY;
    pkt.attackPressed = attackPressed ? 1 : 0;
    pkt.attackHeld = attackHeld ? 1 : 0;
    pkt.thunderPressed = thunderPressed ? 1 : 0;
    return sendPacket(&pkt, sizeof(pkt));
}

bool NetWebSession::sendSpectate(int direction)
{
    NetCSpectate pkt{};
    NetSerialize::initHeader(pkt.hdr, NetMsgType::C_SPECTATE);
    pkt.direction = static_cast<int8_t>(direction);
    return sendPacket(&pkt, sizeof(pkt));
}

#endif // PLATFORM_WEB
