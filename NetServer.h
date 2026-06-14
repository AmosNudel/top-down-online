#ifndef NET_SERVER_H
#define NET_SERVER_H

#include "NetCommon.h"
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

struct ConnectedClient
{
    void *peer{};
    int playerId{-1};
};

class NetServer
{
public:
    using PacketHandler = std::function<void(void *peer, const uint8_t *, size_t)>;
    using DisconnectHandler = std::function<void(int playerId)>;

    NetServer();
    ~NetServer();

    bool start(uint16_t port, size_t maxClients = GameConfig::kMaxPlayers);
    void shutdown();
    void poll(int timeoutMs = 0);
    void broadcast(const void *data, size_t size, bool reliable = false);
    void sendToPlayer(int playerId, const void *data, size_t size, bool reliable = true);
    void sendToPeer(void *peer, const void *data, size_t size, bool reliable = true);
    void disconnectPeer(void *peer);
    void disconnectPlayer(int playerId, const char *reason = nullptr);

    void flush();

    void setPacketHandler(PacketHandler handler);
    void setDisconnectHandler(DisconnectHandler handler);
    void registerClient(void *peer, int playerId);
    void unregisterPeer(void *peer);
    int peerToPlayerId(void *peer) const;
    std::vector<int> getConnectedPlayerIds() const;

private:
    struct Impl;
    Impl *impl{};
    std::unordered_map<int, ConnectedClient> clientsByPlayerId;
    std::unordered_map<void *, int> playerIdByPeer;
    PacketHandler packetHandler;
    DisconnectHandler disconnectHandler;
};

#endif // NET_SERVER_H
