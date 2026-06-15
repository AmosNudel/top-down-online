#include "NetStreamClient.h"
#include "NetFraming.h"
#include "NetStreamSocket.h"
#include <chrono>
#include <cstring>
#include <string>

struct NetStreamClient::Impl
{
    NetSocket socket{kInvalidSocket};
};

NetStreamClient::NetStreamClient() : impl(new Impl()) {}

NetStreamClient::~NetStreamClient()
{
    disconnect();
    delete impl;
    impl = nullptr;
}

void NetStreamClient::setPacketHandler(PacketHandler handler)
{
    packetHandler = std::move(handler);
}

void NetStreamClient::setDisconnectHandler(DisconnectHandler handler)
{
    disconnectHandler = std::move(handler);
}

bool NetStreamClient::isConnected() const
{
    return impl && impl->socket != kInvalidSocket;
}

bool NetStreamClient::connect(const char *hostName, uint16_t port, uint32_t timeoutMs)
{
    disconnect();

    if (!hostName || !hostName[0])
        return false;

    static NetSocketInit socketInit;

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo *result = nullptr;
    const std::string portStr = std::to_string(port);
    if (getaddrinfo(hostName, portStr.c_str(), &hints, &result) != 0 || !result)
        return false;

    impl->socket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (impl->socket == kInvalidSocket)
    {
        freeaddrinfo(result);
        return false;
    }

    const bool connected = ::connect(impl->socket, result->ai_addr, static_cast<int>(result->ai_addrlen)) == 0;
    freeaddrinfo(result);

    if (!connected)
    {
        netSocketClose(impl->socket);
        impl->socket = kInvalidSocket;
        return false;
    }

    if (!netSetNonBlocking(impl->socket))
    {
        disconnect();
        return false;
    }

    netSetTcpNoDelay(impl->socket);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline)
    {
        poll(10);
        if (!isConnected())
            break;
        // Connected socket stays open; wait for server-driven packets after join.
        return true;
    }

    disconnect();
    return false;
}

void NetStreamClient::disconnect()
{
    if (!impl)
        return;

    if (impl->socket != kInvalidSocket)
    {
        netSocketShutdown(impl->socket);
        netSocketClose(impl->socket);
        impl->socket = kInvalidSocket;
    }

    recvBuffer.clear();
    sendBuffer.clear();
    assignedPlayerId = -1;
}

void NetStreamClient::poll(int timeoutMs)
{
    if (!impl || impl->socket == kInvalidSocket)
        return;

    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(impl->socket, &readSet);

    timeval tv{};
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;

    const int ready = select(static_cast<int>(impl->socket + 1), &readSet, nullptr, nullptr, &tv);
    if (ready <= 0)
        return;

    uint8_t chunk[4096];
    while (true)
    {
        const int received = recv(
            impl->socket,
            reinterpret_cast<char *>(chunk),
            sizeof(chunk),
            0);

        if (received > 0)
        {
            std::vector<std::vector<uint8_t>> packets;
            NetFraming::feed(recvBuffer, chunk, static_cast<size_t>(received), packets);
            for (const auto &packet : packets)
            {
                if (packetHandler && !packet.empty())
                    packetHandler(packet.data(), packet.size());
            }
            continue;
        }

        if (received == 0 ||
            (received < 0 && !netSocketWouldBlock(netSocketLastError())))
        {
            disconnect();
            if (disconnectHandler)
                disconnectHandler();
        }
        break;
    }

    if (!sendBuffer.empty())
    {
        const int sent = send(
            impl->socket,
            reinterpret_cast<const char *>(sendBuffer.data()),
            static_cast<int>(sendBuffer.size()),
            0);

        if (sent > 0)
            sendBuffer.erase(sendBuffer.begin(), sendBuffer.begin() + sent);
    }
}

bool NetStreamClient::sendPacket(const void *data, size_t size)
{
    if (!impl || impl->socket == kInvalidSocket || !data || size == 0)
        return false;

    NetFraming::encodeFrame(static_cast<const uint8_t *>(data), size, sendBuffer);

    const int sent = send(
        impl->socket,
        reinterpret_cast<const char *>(sendBuffer.data()),
        static_cast<int>(sendBuffer.size()),
        0);

    if (sent > 0)
        sendBuffer.erase(sendBuffer.begin(), sendBuffer.begin() + sent);

    return sent >= 0;
}

bool NetStreamClient::sendJoin(const char *name)
{
    NetCJoin pkt{};
    NetSerialize::initHeader(pkt.hdr, NetMsgType::C_JOIN);
    if (name)
        std::strncpy(pkt.name, name, sizeof(pkt.name) - 1);
    return sendPacket(&pkt, sizeof(pkt));
}

bool NetStreamClient::sendReady()
{
    NetCReady pkt{};
    NetSerialize::initHeader(pkt.hdr, NetMsgType::C_READY);
    return sendPacket(&pkt, sizeof(pkt));
}

bool NetStreamClient::sendLeave()
{
    NetCLeave pkt{};
    NetSerialize::initHeader(pkt.hdr, NetMsgType::C_LEAVE);
    return sendPacket(&pkt, sizeof(pkt));
}

bool NetStreamClient::sendInput(float moveX, float moveY, bool attackPressed, bool attackHeld, bool thunderPressed)
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

bool NetStreamClient::sendSpectate(int direction)
{
    NetCSpectate pkt{};
    NetSerialize::initHeader(pkt.hdr, NetMsgType::C_SPECTATE);
    pkt.direction = static_cast<int8_t>(direction);
    return sendPacket(&pkt, sizeof(pkt));
}
