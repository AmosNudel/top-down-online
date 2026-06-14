#ifndef NET_SESSION_H
#define NET_SESSION_H

#include "NetCommon.h"
#include <cstdint>
#include <functional>
#include <memory>

enum class NetTransport
{
    Enet,
    Stream
};

class NetSession
{
public:
    using PacketHandler = std::function<void(const uint8_t *, size_t)>;
    using DisconnectHandler = std::function<void()>;

    NetSession();
    ~NetSession();

    bool connect(const char *host, uint16_t port, NetTransport transport = NetTransport::Enet, uint32_t timeoutMs = 3000);
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
    int getAssignedPlayerId() const;
    void setAssignedPlayerId(int id);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

#endif // NET_SESSION_H
