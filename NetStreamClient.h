#ifndef NET_STREAM_CLIENT_H
#define NET_STREAM_CLIENT_H

#include "NetCommon.h"
#include <cstdint>
#include <functional>
#include <vector>

class NetStreamClient
{
public:
    using PacketHandler = std::function<void(const uint8_t *, size_t)>;
    using DisconnectHandler = std::function<void()>;

    NetStreamClient();
    ~NetStreamClient();

    bool connect(const char *host, uint16_t port, uint32_t timeoutMs = 3000);
    void disconnect();
    bool isConnected() const;
    void poll(int timeoutMs = 0);

    bool sendJoin(const char *name);
    bool sendReady();
    bool sendLeave();
    bool sendInput(float moveX, float moveY, bool attackPressed, bool attackHeld, bool thunderPressed);
    bool sendSpectate(int direction);

    void setPacketHandler(PacketHandler handler);
    void setDisconnectHandler(DisconnectHandler handler);
    int getAssignedPlayerId() const { return assignedPlayerId; }
    void setAssignedPlayerId(int id) { assignedPlayerId = id; }

private:
    bool sendPacket(const void *data, size_t size);

    struct Impl;
    Impl *impl{};
    PacketHandler packetHandler;
    DisconnectHandler disconnectHandler;
    int assignedPlayerId{-1};
    std::vector<uint8_t> recvBuffer;
    std::vector<uint8_t> sendBuffer;
};

#endif // NET_STREAM_CLIENT_H
