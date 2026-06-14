#ifndef NET_GAME_HOST_H
#define NET_GAME_HOST_H

#include "NetCommon.h"
#include "NetServer.h"
#include "NetStreamServer.h"
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

enum class NetPeerKind : uint8_t
{
    Enet = 1,
    Stream = 2
};

struct NetPeerRef
{
    NetPeerKind kind{NetPeerKind::Enet};
    void *handle{};

    bool operator==(const NetPeerRef &other) const
    {
        return kind == other.kind && handle == other.handle;
    }
};

struct NetPeerRefHash
{
    size_t operator()(const NetPeerRef &peer) const noexcept
    {
        return std::hash<void *>()(peer.handle) ^
               (static_cast<size_t>(peer.kind) << 1);
    }
};

class NetGameHost
{
public:
    using PacketHandler = std::function<void(const NetPeerRef &peer, const uint8_t *, size_t)>;
    using DisconnectHandler = std::function<void(int playerId)>;

    bool start(uint16_t enetPort, uint16_t streamPort, size_t maxClients = GameConfig::kMaxPlayers);
    void shutdown();
    void poll(int timeoutMs = 0);

    void broadcast(const void *data, size_t size, bool reliable = false);
    void sendToPeer(const NetPeerRef &peer, const void *data, size_t size, bool reliable = true);
    void sendToPlayer(int playerId, const void *data, size_t size, bool reliable = true);
    void disconnectPeer(const NetPeerRef &peer);
    void disconnectPlayer(int playerId, const char *reason = nullptr);
    void flush();

    void setPacketHandler(PacketHandler handler);
    void setDisconnectHandler(DisconnectHandler handler);
    void registerClient(const NetPeerRef &peer, int playerId);
    void unregisterPeer(const NetPeerRef &peer);
    int peerToPlayerId(const NetPeerRef &peer) const;
    std::vector<int> getConnectedPlayerIds() const;

private:
    NetServer enetServer;
    NetStreamServer streamServer;
    PacketHandler packetHandler;
    DisconnectHandler disconnectHandler;
};

#endif // NET_GAME_HOST_H
