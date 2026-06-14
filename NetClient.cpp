#include "NetClient.h"
#include <enet/enet.h>
#include <cstring>

struct NetClient::Impl
{
    ENetHost *host{};
    ENetPeer *peer{};
};

NetClient::NetClient() : impl(new Impl()) {}

NetClient::~NetClient()
{
    disconnect();
    delete impl;
    impl = nullptr;
}

void NetClient::setPacketHandler(PacketHandler handler)
{
    packetHandler = std::move(handler);
}

void NetClient::setDisconnectHandler(DisconnectHandler handler)
{
    disconnectHandler = std::move(handler);
}

bool NetClient::isConnected() const
{
    return impl && impl->peer != nullptr;
}

bool NetClient::connect(const char *hostName, uint16_t port, uint32_t timeoutMs)
{
    disconnect();

    if (enet_initialize() != 0)
        return false;

    impl->host = enet_host_create(nullptr, 1, 2, 0, 0);
    if (!impl->host)
    {
        enet_deinitialize();
        return false;
    }

    ENetAddress address{};
    enet_address_set_host(&address, hostName);
    address.port = port;

    impl->peer = enet_host_connect(impl->host, &address, 2, 0);
    if (!impl->peer)
    {
        enet_host_destroy(impl->host);
        impl->host = nullptr;
        enet_deinitialize();
        return false;
    }

    ENetEvent event{};
    if (enet_host_service(impl->host, &event, timeoutMs) > 0 &&
        event.type == ENET_EVENT_TYPE_CONNECT)
    {
        return true;
    }

    enet_peer_reset(impl->peer);
    impl->peer = nullptr;
    enet_host_destroy(impl->host);
    impl->host = nullptr;
    enet_deinitialize();
    return false;
}

void NetClient::disconnect()
{
    if (!impl)
        return;

    if (impl->host && impl->peer)
        enet_peer_disconnect_now(impl->peer, 0);

    if (impl->host)
        enet_host_destroy(impl->host);

    impl->host = nullptr;
    impl->peer = nullptr;
    assignedPlayerId = -1;
    enet_deinitialize();
}

void NetClient::poll(int timeoutMs)
{
    if (!impl || !impl->host)
        return;

    ENetEvent event{};
    while (enet_host_service(impl->host, &event, timeoutMs) > 0)
    {
        switch (event.type)
        {
        case ENET_EVENT_TYPE_RECEIVE:
            if (packetHandler && event.packet && event.packet->dataLength > 0)
                packetHandler(event.packet->data, event.packet->dataLength);
            enet_packet_destroy(event.packet);
            break;
        case ENET_EVENT_TYPE_DISCONNECT:
            impl->peer = nullptr;
            assignedPlayerId = -1;
            if (disconnectHandler)
                disconnectHandler();
            break;
        default:
            break;
        }
    }
}

bool NetClient::sendPacket(const void *data, size_t size, bool reliable)
{
    if (!impl || !impl->peer || !data || size == 0)
        return false;

    ENetPacket *packet = enet_packet_create(
        data, size,
        reliable ? ENET_PACKET_FLAG_RELIABLE : ENET_PACKET_FLAG_UNSEQUENCED);

    if (enet_peer_send(impl->peer, 0, packet) != 0)
        return false;

    enet_host_flush(impl->host);
    return true;
}

bool NetClient::sendJoin(const char *name)
{
    NetCJoin pkt{};
    NetSerialize::initHeader(pkt.hdr, NetMsgType::C_JOIN);
    if (name)
        std::strncpy(pkt.name, name, sizeof(pkt.name) - 1);
    return sendPacket(&pkt, sizeof(pkt), true);
}

bool NetClient::sendReady()
{
    NetCReady pkt{};
    NetSerialize::initHeader(pkt.hdr, NetMsgType::C_READY);
    return sendPacket(&pkt, sizeof(pkt), true);
}

bool NetClient::sendLeave()
{
    NetCLeave pkt{};
    NetSerialize::initHeader(pkt.hdr, NetMsgType::C_LEAVE);
    return sendPacket(&pkt, sizeof(pkt), true);
}

bool NetClient::sendInput(float moveX, float moveY, bool attackPressed, bool attackHeld, bool thunderPressed)
{
    NetCInput pkt{};
    NetSerialize::initHeader(pkt.hdr, NetMsgType::C_INPUT);
    pkt.moveX = moveX;
    pkt.moveY = moveY;
    pkt.attackPressed = attackPressed ? 1 : 0;
    pkt.attackHeld = attackHeld ? 1 : 0;
    pkt.thunderPressed = thunderPressed ? 1 : 0;
    return sendPacket(&pkt, sizeof(pkt), attackPressed || thunderPressed);
}

bool NetClient::sendSpectate(int direction)
{
    NetCSpectate pkt{};
    NetSerialize::initHeader(pkt.hdr, NetMsgType::C_SPECTATE);
    pkt.direction = static_cast<int8_t>(direction);
    return sendPacket(&pkt, sizeof(pkt), true);
}
