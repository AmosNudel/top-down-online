#include "NetGameHost.h"

bool NetGameHost::start(uint16_t enetPort, uint16_t streamPort, size_t maxClients)
{
    if (!enetServer.start(enetPort, maxClients))
        return false;

    if (!streamServer.start(streamPort, maxClients))
    {
        enetServer.shutdown();
        return false;
    }

    enetServer.setPacketHandler([this](void *peer, const uint8_t *data, size_t size) {
        if (packetHandler)
            packetHandler(NetPeerRef{NetPeerKind::Enet, peer}, data, size);
    });

    streamServer.setPacketHandler([this](void *peer, const uint8_t *data, size_t size) {
        if (packetHandler)
            packetHandler(NetPeerRef{NetPeerKind::Stream, peer}, data, size);
    });

    enetServer.setDisconnectHandler([this](int playerId) {
        if (disconnectHandler)
            disconnectHandler(playerId);
    });

    streamServer.setDisconnectHandler([this](int playerId) {
        if (disconnectHandler)
            disconnectHandler(playerId);
    });

    return true;
}

void NetGameHost::shutdown()
{
    enetServer.shutdown();
    streamServer.shutdown();
}

void NetGameHost::poll(int timeoutMs)
{
    enetServer.poll(timeoutMs);
    streamServer.poll(0);
}

void NetGameHost::setPacketHandler(PacketHandler handler)
{
    packetHandler = std::move(handler);
}

void NetGameHost::setDisconnectHandler(DisconnectHandler handler)
{
    disconnectHandler = std::move(handler);
}

void NetGameHost::broadcast(const void *data, size_t size, bool reliable)
{
    enetServer.broadcast(data, size, reliable);
    streamServer.broadcast(data, size);
}

void NetGameHost::sendToPeer(const NetPeerRef &peer, const void *data, size_t size, bool reliable)
{
    if (peer.kind == NetPeerKind::Enet)
        enetServer.sendToPeer(peer.handle, data, size, reliable);
    else
        streamServer.sendToPeer(peer.handle, data, size);
}

void NetGameHost::sendToPlayer(int playerId, const void *data, size_t size, bool reliable)
{
    enetServer.sendToPlayer(playerId, data, size, reliable);
    streamServer.sendToPlayer(playerId, data, size);
}

void NetGameHost::disconnectPeer(const NetPeerRef &peer)
{
    if (peer.kind == NetPeerKind::Enet)
        enetServer.disconnectPeer(peer.handle);
    else
        streamServer.disconnectPeer(peer.handle);
}

void NetGameHost::disconnectPlayer(int playerId, const char *reason)
{
    enetServer.disconnectPlayer(playerId, reason);
    streamServer.disconnectPlayer(playerId, reason);
}

void NetGameHost::flush()
{
    enetServer.flush();
    streamServer.flush();
}

void NetGameHost::registerClient(const NetPeerRef &peer, int playerId)
{
    if (peer.kind == NetPeerKind::Enet)
        enetServer.registerClient(peer.handle, playerId);
    else
        streamServer.registerClient(peer.handle, playerId);
}

void NetGameHost::unregisterPeer(const NetPeerRef &peer)
{
    if (peer.kind == NetPeerKind::Enet)
        enetServer.unregisterPeer(peer.handle);
    else
        streamServer.unregisterPeer(peer.handle);
}

int NetGameHost::peerToPlayerId(const NetPeerRef &peer) const
{
    if (peer.kind == NetPeerKind::Enet)
        return enetServer.peerToPlayerId(peer.handle);
    return streamServer.peerToPlayerId(peer.handle);
}

std::vector<int> NetGameHost::getConnectedPlayerIds() const
{
    std::vector<int> ids = enetServer.getConnectedPlayerIds();
    const std::vector<int> streamIds = streamServer.getConnectedPlayerIds();
    ids.insert(ids.end(), streamIds.begin(), streamIds.end());
    return ids;
}
