#ifndef NET_WEB_SESSION_H
#define NET_WEB_SESSION_H

#if defined(PLATFORM_WEB)

#include "NetCommon.h"
#include <cstdint>
#include <functional>
#include <string>

class NetWebSession
{
public:
    using PacketHandler = std::function<void(const uint8_t *, size_t)>;
    using DisconnectHandler = std::function<void()>;

    bool connect(const char *url, uint32_t timeoutMs = 5000);
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

    static std::string configuredServerUrl();

private:
    bool sendPacket(const void *data, size_t size);

    PacketHandler packetHandler;
    DisconnectHandler disconnectHandler;
    int assignedPlayerId{-1};
    bool connected{false};
};

#endif // PLATFORM_WEB

#endif // NET_WEB_SESSION_H
