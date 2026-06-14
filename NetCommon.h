#ifndef NET_COMMON_H
#define NET_COMMON_H

#include "GameConfig.h"
#include <cstdint>
#include <cstring>
#include <vector>

enum class NetMsgType : uint8_t
{
    C_JOIN = 1,
    C_READY,
    C_INPUT,
    C_LEAVE,
    C_SPECTATE,
    S_JOIN_OK,
    S_JOIN_FAIL,
    S_WORLD_STATE,
    S_KICK,
    S_LOBBY_SYNC
};

enum NetPlayerFlags : uint8_t
{
    NET_PLAYER_IN_QUEUE = 1 << 0,
    NET_PLAYER_IN_MATCH = 1 << 1,
    NET_PLAYER_ALIVE = 1 << 2,
    NET_PLAYER_READY = 1 << 3
};

enum NetJoinFailReason : uint8_t
{
    NET_JOIN_INVALID_NAME = 1,
    NET_JOIN_SERVER_FULL = 2,
    NET_JOIN_MATCH_IN_PROGRESS = 3
};

#pragma pack(push, 1)

struct NetPacketHeader
{
    uint8_t type;
};

struct NetCJoin
{
    NetPacketHeader hdr;
    char name[GameConfig::kPlayerNameMaxLen + 1];
};

struct NetCReady
{
    NetPacketHeader hdr;
};

struct NetCLeave
{
    NetPacketHeader hdr;
};

struct NetCSpectate
{
    NetPacketHeader hdr;
    int8_t direction; // -1 prev, +1 next
};

struct NetCInput
{
    NetPacketHeader hdr;
    float moveX;
    float moveY;
    uint8_t attackPressed;
    uint8_t attackHeld;
    uint8_t thunderPressed;
};

struct NetSJoinOk
{
    NetPacketHeader hdr;
    uint8_t playerId;
    uint8_t colorIndex;
};

struct NetSJoinFail
{
    NetPacketHeader hdr;
    uint8_t reason;
};

struct NetSKick
{
    NetPacketHeader hdr;
    char message[64];
};

struct NetPlayerSnapshot
{
    uint8_t id;
    uint8_t colorIndex;
    uint8_t flags;
    uint16_t kills;
    char name[GameConfig::kPlayerNameMaxLen + 1];
    float worldX;
    float worldY;
    float health;
    uint8_t charged;
    uint8_t attackHeld;
    uint8_t moving;
    int8_t facing;
};

struct NetEnemySnapshot
{
    uint16_t id;
    float worldX;
    float worldY;
    uint8_t targetPlayerId;
    uint8_t alive;
    uint8_t enemyType;
    uint8_t frame;
    uint8_t moving;
    int8_t facing;
};

struct NetSWorldStateHeader
{
    NetPacketHeader hdr;
    uint32_t tick;
    uint8_t phase;
    float countdownRemaining;
    float matchTimeRemaining;
    float resultsTimeRemaining;
    uint32_t mapSeed;
    uint8_t playerCount;
    uint16_t enemyCount;
    uint8_t pickupCount;
    uint8_t strikeCount;
};

struct NetPickupSnapshot
{
    float worldX;
    float worldY;
    uint8_t active;
};

struct NetStrikeSnapshot
{
    float worldX;
    float worldY;
    uint8_t frame;
    uint8_t ownerPlayerId;
};

struct NetLobbyPlayerEntry
{
    uint8_t id;
    uint8_t colorIndex;
    uint8_t flags;
    char name[GameConfig::kPlayerNameMaxLen + 1];
};

struct NetSLobbySyncHeader
{
    NetPacketHeader hdr;
    uint8_t phase;
    float countdownRemaining;
    uint8_t playerCount;
};

#pragma pack(pop)

namespace NetSerialize
{
    inline void initHeader(NetPacketHeader &hdr, NetMsgType type)
    {
        hdr.type = static_cast<uint8_t>(type);
    }

    void buildWorldState(
        const NetSWorldStateHeader &header,
        const NetPlayerSnapshot *players,
        const NetEnemySnapshot *enemies,
        const NetPickupSnapshot *pickups,
        const NetStrikeSnapshot *strikes,
        std::vector<uint8_t> &out);

    bool parseWorldState(
        const uint8_t *data,
        size_t size,
        NetSWorldStateHeader &header,
        std::vector<NetPlayerSnapshot> &players,
        std::vector<NetEnemySnapshot> &enemies,
        std::vector<NetPickupSnapshot> &pickups,
        std::vector<NetStrikeSnapshot> &strikes);

    void buildLobbySync(
        const NetSLobbySyncHeader &header,
        const NetLobbyPlayerEntry *players,
        std::vector<uint8_t> &out);

    bool parseLobbySync(
        const uint8_t *data,
        size_t size,
        NetSLobbySyncHeader &header,
        std::vector<NetLobbyPlayerEntry> &players);
}

#endif // NET_COMMON_H
