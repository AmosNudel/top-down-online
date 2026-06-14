#include "NetServer.h"
#include <enet/enet.h>
#include <cstring>

struct NetServer::Impl
{
    ENetHost *host{};
};

NetServer::NetServer() : impl(new Impl()) {}

NetServer::~NetServer()
{
    shutdown();
    delete impl;
    impl = nullptr;
}

void NetServer::setPacketHandler(PacketHandler handler)
{
    packetHandler = std::move(handler);
}

void NetServer::setDisconnectHandler(DisconnectHandler handler)
{
    disconnectHandler = std::move(handler);
}

bool NetServer::start(uint16_t port, size_t maxClients)
{
    if (!impl)
        return false;

    if (impl->host)
        return true;

    if (enet_initialize() != 0)
        return false;

    ENetAddress address{};
    address.host = ENET_HOST_ANY;
    address.port = port;

    impl->host = enet_host_create(&address, maxClients, 2, 0, 0);
    return impl->host != nullptr;
}

void NetServer::shutdown()
{
    if (!impl)
        return;

    if (impl->host)
    {
        enet_host_destroy(impl->host);
        impl->host = nullptr;
    }

    clientsByPlayerId.clear();
    playerIdByPeer.clear();
    enet_deinitialize();
}

void NetServer::poll(int timeoutMs)
{
    if (!impl || !impl->host)
        return;

    ENetEvent event{};
    while (enet_host_service(impl->host, &event, timeoutMs) > 0)
    {
        switch (event.type)
        {
        case ENET_EVENT_TYPE_CONNECT:
            event.peer->data = nullptr;
            break;
        case ENET_EVENT_TYPE_RECEIVE:
            if (packetHandler && event.packet && event.packet->dataLength > 0)
                packetHandler(event.peer, event.packet->data, event.packet->dataLength);
            enet_packet_destroy(event.packet);
            break;
        case ENET_EVENT_TYPE_DISCONNECT:
        {
            void *peer = event.peer;
            int playerId = peerToPlayerId(peer);
            if (playerId >= 0 && disconnectHandler)
                disconnectHandler(playerId);
            unregisterPeer(peer);
            event.peer->data = nullptr;
            break;
        }
        default:
            break;
        }
    }
}

void NetServer::broadcast(const void *data, size_t size, bool reliable)
{
    if (!impl || !impl->host || !data || size == 0)
        return;

    for (const auto &entry : clientsByPlayerId)
    {
        if (!entry.second.peer)
            continue;

        ENetPacket *packet = enet_packet_create(
            data, size,
            reliable ? ENET_PACKET_FLAG_RELIABLE : ENET_PACKET_FLAG_UNSEQUENCED);
        enet_peer_send(static_cast<ENetPeer *>(entry.second.peer), 0, packet);
    }

    enet_host_flush(impl->host);
}

void NetServer::sendToPeer(void *peer, const void *data, size_t size, bool reliable)
{
    if (!impl || !impl->host || !peer || !data || size == 0)
        return;

    auto *enetPeer = static_cast<ENetPeer *>(peer);
    ENetPacket *packet = enet_packet_create(
        data, size,
        reliable ? ENET_PACKET_FLAG_RELIABLE : ENET_PACKET_FLAG_UNSEQUENCED);

    enet_peer_send(enetPeer, 0, packet);
    if (impl && impl->host)
        enet_host_flush(impl->host);
}

void NetServer::disconnectPeer(void *peer)
{
    if (!peer)
        return;
    enet_peer_disconnect(static_cast<ENetPeer *>(peer), 0);
}

void NetServer::sendToPlayer(int playerId, const void *data, size_t size, bool reliable)
{
    auto it = clientsByPlayerId.find(playerId);
    if (it == clientsByPlayerId.end() || !it->second.peer)
        return;

    auto *peer = static_cast<ENetPeer *>(it->second.peer);
    ENetPacket *packet = enet_packet_create(
        data, size,
        reliable ? ENET_PACKET_FLAG_RELIABLE : ENET_PACKET_FLAG_UNSEQUENCED);

    enet_peer_send(peer, 0, packet);
}

void NetServer::disconnectPlayer(int playerId, const char *reason)
{
    auto it = clientsByPlayerId.find(playerId);
    if (it == clientsByPlayerId.end() || !it->second.peer)
        return;

    if (reason && reason[0])
    {
        NetSKick kick{};
        NetSerialize::initHeader(kick.hdr, NetMsgType::S_KICK);
        std::strncpy(kick.message, reason, sizeof(kick.message) - 1);
        sendToPlayer(playerId, &kick, sizeof(kick), true);
    }

    auto *peer = static_cast<ENetPeer *>(it->second.peer);
    enet_peer_disconnect(peer, 0);
}

void NetServer::registerClient(void *peer, int playerId)
{
    ConnectedClient client;
    client.peer = peer;
    client.playerId = playerId;
    clientsByPlayerId[playerId] = client;
    playerIdByPeer[peer] = playerId;
}

void NetServer::unregisterPeer(void *peer)
{
    int playerId = peerToPlayerId(peer);
    if (playerId >= 0)
        clientsByPlayerId.erase(playerId);
    playerIdByPeer.erase(peer);
}

int NetServer::peerToPlayerId(void *peer) const
{
    auto it = playerIdByPeer.find(peer);
    return it != playerIdByPeer.end() ? it->second : -1;
}

std::vector<int> NetServer::getConnectedPlayerIds() const
{
    std::vector<int> ids;
    ids.reserve(clientsByPlayerId.size());
    for (const auto &entry : clientsByPlayerId)
        ids.push_back(entry.first);
    return ids;
}

void NetServer::flush()
{
    if (impl && impl->host)
        enet_host_flush(impl->host);
}
