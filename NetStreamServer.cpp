#include "NetStreamServer.h"
#include "NetFraming.h"
#include "NetStreamSocket.h"
#include <algorithm>
#include <cstring>
#include <vector>

struct NetStreamServer::Impl
{
    NetSocket listenSocket{kInvalidSocket};
    std::vector<NetSocket> clientSockets;
    std::unordered_map<NetSocket, std::vector<uint8_t>> recvBuffers;
    std::unordered_map<NetSocket, std::vector<uint8_t>> sendBuffers;
    size_t maxClients{GameConfig::kMaxPlayers};
    bool initialized{false};
};

NetStreamServer::NetStreamServer() : impl(new Impl()) {}

NetStreamServer::~NetStreamServer()
{
    shutdown();
    delete impl;
    impl = nullptr;
}

void NetStreamServer::setPacketHandler(PacketHandler handler)
{
    packetHandler = std::move(handler);
}

void NetStreamServer::setDisconnectHandler(DisconnectHandler handler)
{
    disconnectHandler = std::move(handler);
}

bool NetStreamServer::start(uint16_t port, size_t maxClients)
{
    if (!impl)
        return false;

    if (impl->listenSocket != kInvalidSocket)
        return true;

    static NetSocketInit socketInit;

    impl->maxClients = maxClients;
    impl->listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (impl->listenSocket == kInvalidSocket)
        return false;

    int yes = 1;
    setsockopt(impl->listenSocket, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char *>(&yes), sizeof(yes));

    if (!netSetNonBlocking(impl->listenSocket))
    {
        netSocketClose(impl->listenSocket);
        impl->listenSocket = kInvalidSocket;
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(impl->listenSocket, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0 ||
        listen(impl->listenSocket, static_cast<int>(maxClients)) != 0)
    {
        netSocketClose(impl->listenSocket);
        impl->listenSocket = kInvalidSocket;
        return false;
    }

    impl->initialized = true;
    return true;
}

void NetStreamServer::shutdown()
{
    if (!impl)
        return;

    for (NetSocket socket : impl->clientSockets)
        netSocketClose(socket);

    impl->clientSockets.clear();
    impl->recvBuffers.clear();
    impl->sendBuffers.clear();

    if (impl->listenSocket != kInvalidSocket)
    {
        netSocketClose(impl->listenSocket);
        impl->listenSocket = kInvalidSocket;
    }

    clientsByPlayerId.clear();
    playerIdByPeer.clear();
    impl->initialized = false;
}

static void flushSendBuffer(NetSocket socket, std::vector<uint8_t> &sendBuffer)
{
    while (!sendBuffer.empty())
    {
        const int sent = send(
            socket,
            reinterpret_cast<const char *>(sendBuffer.data()),
            static_cast<int>(sendBuffer.size()),
            0);

        if (sent > 0)
        {
            sendBuffer.erase(sendBuffer.begin(), sendBuffer.begin() + sent);
            continue;
        }

        if (sent < 0 && netSocketWouldBlock(netSocketLastError()))
            break;

        sendBuffer.clear();
        break;
    }
}

void NetStreamServer::flush()
{
    if (!impl)
        return;

    for (NetSocket socket : impl->clientSockets)
    {
        auto it = impl->sendBuffers.find(socket);
        if (it != impl->sendBuffers.end())
            flushSendBuffer(socket, it->second);
    }
}

void NetStreamServer::poll(int timeoutMs)
{
    if (!impl || impl->listenSocket == kInvalidSocket)
        return;

    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(impl->listenSocket, &readSet);
    NetSocket maxFd = impl->listenSocket;

    for (NetSocket socket : impl->clientSockets)
    {
        FD_SET(socket, &readSet);
        if (socket > maxFd)
            maxFd = socket;
    }

    timeval tv{};
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;
    timeval *tvPtr = timeoutMs >= 0 ? &tv : nullptr;

    if (select(static_cast<int>(maxFd + 1), &readSet, nullptr, nullptr, tvPtr) <= 0)
        return;

    if (FD_ISSET(impl->listenSocket, &readSet) &&
        impl->clientSockets.size() < impl->maxClients)
    {
        NetSocket client = accept(impl->listenSocket, nullptr, nullptr);
        if (client != kInvalidSocket)
        {
            netSetNonBlocking(client);
            impl->clientSockets.push_back(client);
            impl->recvBuffers[client] = {};
            impl->sendBuffers[client] = {};
        }
    }

    std::vector<NetSocket> toRemove;

    for (NetSocket socket : impl->clientSockets)
    {
        if (!FD_ISSET(socket, &readSet))
            continue;

        uint8_t chunk[4096];
        while (true)
        {
            const int received = recv(
                socket,
                reinterpret_cast<char *>(chunk),
                sizeof(chunk),
                0);

            if (received > 0)
            {
                std::vector<std::vector<uint8_t>> packets;
                auto &buffer = impl->recvBuffers[socket];
                NetFraming::feed(buffer, chunk, static_cast<size_t>(received), packets);

                void *peer = reinterpret_cast<void *>(static_cast<uintptr_t>(socket));
                for (const auto &packet : packets)
                {
                    if (packetHandler && !packet.empty())
                        packetHandler(peer, packet.data(), packet.size());
                }
                continue;
            }

            if (received == 0 ||
                (received < 0 && !netSocketWouldBlock(netSocketLastError())))
            {
                void *peer = reinterpret_cast<void *>(static_cast<uintptr_t>(socket));
                const int playerId = peerToPlayerId(peer);
                if (playerId >= 0 && disconnectHandler)
                    disconnectHandler(playerId);
                unregisterPeer(peer);
                toRemove.push_back(socket);
            }
            break;
        }
    }

    for (NetSocket socket : toRemove)
    {
        netSocketClose(socket);
        impl->clientSockets.erase(
            std::remove(impl->clientSockets.begin(), impl->clientSockets.end(), socket),
            impl->clientSockets.end());
        impl->recvBuffers.erase(socket);
        impl->sendBuffers.erase(socket);
    }

    flush();
}

void NetStreamServer::broadcast(const void *data, size_t size)
{
    if (!impl || !data || size == 0)
        return;

    for (const auto &entry : clientsByPlayerId)
    {
        if (entry.second.handle)
            sendToPeer(entry.second.handle, data, size);
    }
}

void NetStreamServer::sendToPeer(void *peer, const void *data, size_t size)
{
    if (!impl || !peer || !data || size == 0 || size > NetFraming::kMaxPayload)
        return;

    const auto socket = static_cast<NetSocket>(reinterpret_cast<uintptr_t>(peer));
    auto &sendBuffer = impl->sendBuffers[socket];
    NetFraming::encodeFrame(static_cast<const uint8_t *>(data), size, sendBuffer);
    flushSendBuffer(socket, sendBuffer);
}

void NetStreamServer::sendToPlayer(int playerId, const void *data, size_t size)
{
    auto it = clientsByPlayerId.find(playerId);
    if (it == clientsByPlayerId.end() || !it->second.handle)
        return;

    sendToPeer(it->second.handle, data, size);
}

void NetStreamServer::disconnectPeer(void *peer)
{
    if (!impl || !peer)
        return;

    const auto socket = static_cast<NetSocket>(reinterpret_cast<uintptr_t>(peer));
    netSocketShutdown(socket);
    netSocketClose(socket);

    impl->clientSockets.erase(
        std::remove(impl->clientSockets.begin(), impl->clientSockets.end(), socket),
        impl->clientSockets.end());
    impl->recvBuffers.erase(socket);
    impl->sendBuffers.erase(socket);
}

void NetStreamServer::disconnectPlayer(int playerId, const char *reason)
{
    auto it = clientsByPlayerId.find(playerId);
    if (it == clientsByPlayerId.end() || !it->second.handle)
        return;

    if (reason && reason[0])
    {
        NetSKick kick{};
        NetSerialize::initHeader(kick.hdr, NetMsgType::S_KICK);
        std::strncpy(kick.message, reason, sizeof(kick.message) - 1);
        sendToPlayer(playerId, &kick, sizeof(kick));
    }

    disconnectPeer(it->second.handle);
}

void NetStreamServer::registerClient(void *peer, int playerId)
{
    StreamPeer client;
    client.handle = peer;
    client.playerId = playerId;
    clientsByPlayerId[playerId] = client;
    playerIdByPeer[peer] = playerId;
}

void NetStreamServer::unregisterPeer(void *peer)
{
    const int playerId = peerToPlayerId(peer);
    if (playerId >= 0)
        clientsByPlayerId.erase(playerId);
    playerIdByPeer.erase(peer);
}

int NetStreamServer::peerToPlayerId(void *peer) const
{
    auto it = playerIdByPeer.find(peer);
    return it != playerIdByPeer.end() ? it->second : -1;
}

std::vector<int> NetStreamServer::getConnectedPlayerIds() const
{
    std::vector<int> ids;
    ids.reserve(clientsByPlayerId.size());
    for (const auto &entry : clientsByPlayerId)
        ids.push_back(entry.first);
    return ids;
}
