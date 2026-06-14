#include "NetSession.h"
#include "NetClient.h"
#include "NetStreamClient.h"

struct NetSession::Impl
{
    NetTransport transport{NetTransport::Enet};
    NetClient enetClient;
    NetStreamClient streamClient;
};

NetSession::NetSession() : impl(new Impl()) {}

NetSession::~NetSession() = default;

void NetSession::setPacketHandler(PacketHandler handler)
{
    impl->enetClient.setPacketHandler(handler);
    impl->streamClient.setPacketHandler(handler);
}

void NetSession::setDisconnectHandler(DisconnectHandler handler)
{
    impl->enetClient.setDisconnectHandler(handler);
    impl->streamClient.setDisconnectHandler(handler);
}

bool NetSession::connect(const char *host, uint16_t port, NetTransport transport, uint32_t timeoutMs)
{
    disconnect();
    impl->transport = transport;
    if (transport == NetTransport::Stream)
        return impl->streamClient.connect(host, port, timeoutMs);
    return impl->enetClient.connect(host, port, timeoutMs);
}

void NetSession::disconnect()
{
    impl->enetClient.disconnect();
    impl->streamClient.disconnect();
}

bool NetSession::isConnected() const
{
    if (impl->transport == NetTransport::Stream)
        return impl->streamClient.isConnected();
    return impl->enetClient.isConnected();
}

void NetSession::poll(int timeoutMs)
{
    if (impl->transport == NetTransport::Stream)
        impl->streamClient.poll(timeoutMs);
    else
        impl->enetClient.poll(timeoutMs);
}

bool NetSession::sendJoin(const char *name)
{
    if (impl->transport == NetTransport::Stream)
        return impl->streamClient.sendJoin(name);
    return impl->enetClient.sendJoin(name);
}

bool NetSession::sendReady()
{
    if (impl->transport == NetTransport::Stream)
        return impl->streamClient.sendReady();
    return impl->enetClient.sendReady();
}

bool NetSession::sendLeave()
{
    if (impl->transport == NetTransport::Stream)
        return impl->streamClient.sendLeave();
    return impl->enetClient.sendLeave();
}

bool NetSession::sendInput(float moveX, float moveY, bool attackPressed, bool attackHeld, bool thunderPressed)
{
    if (impl->transport == NetTransport::Stream)
        return impl->streamClient.sendInput(moveX, moveY, attackPressed, attackHeld, thunderPressed);
    return impl->enetClient.sendInput(moveX, moveY, attackPressed, attackHeld, thunderPressed);
}

bool NetSession::sendSpectate(int direction)
{
    if (impl->transport == NetTransport::Stream)
        return impl->streamClient.sendSpectate(direction);
    return impl->enetClient.sendSpectate(direction);
}

int NetSession::getAssignedPlayerId() const
{
    if (impl->transport == NetTransport::Stream)
        return impl->streamClient.getAssignedPlayerId();
    return impl->enetClient.getAssignedPlayerId();
}

void NetSession::setAssignedPlayerId(int id)
{
    impl->enetClient.setAssignedPlayerId(id);
    impl->streamClient.setAssignedPlayerId(id);
}
