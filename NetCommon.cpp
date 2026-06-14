#include "NetCommon.h"

namespace NetSerialize
{
    void buildWorldState(
        const NetSWorldStateHeader &header,
        const NetPlayerSnapshot *players,
        const NetEnemySnapshot *enemies,
        const NetPickupSnapshot *pickups,
        const NetStrikeSnapshot *strikes,
        std::vector<uint8_t> &out)
    {
        size_t totalSize = sizeof(NetSWorldStateHeader)
            + header.playerCount * sizeof(NetPlayerSnapshot)
            + header.enemyCount * sizeof(NetEnemySnapshot)
            + header.pickupCount * sizeof(NetPickupSnapshot)
            + header.strikeCount * sizeof(NetStrikeSnapshot);

        out.resize(totalSize);
        uint8_t *cursor = out.data();
        std::memcpy(cursor, &header, sizeof(header));
        cursor += sizeof(header);

        if (header.playerCount > 0 && players)
        {
            size_t bytes = header.playerCount * sizeof(NetPlayerSnapshot);
            std::memcpy(cursor, players, bytes);
            cursor += bytes;
        }

        if (header.enemyCount > 0 && enemies)
        {
            size_t bytes = header.enemyCount * sizeof(NetEnemySnapshot);
            std::memcpy(cursor, enemies, bytes);
            cursor += bytes;
        }

        if (header.pickupCount > 0 && pickups)
        {
            size_t bytes = header.pickupCount * sizeof(NetPickupSnapshot);
            std::memcpy(cursor, pickups, bytes);
            cursor += bytes;
        }

        if (header.strikeCount > 0 && strikes)
        {
            size_t bytes = header.strikeCount * sizeof(NetStrikeSnapshot);
            std::memcpy(cursor, strikes, bytes);
        }
    }

    bool parseWorldState(
        const uint8_t *data,
        size_t size,
        NetSWorldStateHeader &header,
        std::vector<NetPlayerSnapshot> &players,
        std::vector<NetEnemySnapshot> &enemies,
        std::vector<NetPickupSnapshot> &pickups,
        std::vector<NetStrikeSnapshot> &strikes)
    {
        if (size < sizeof(NetSWorldStateHeader))
            return false;

        std::memcpy(&header, data, sizeof(header));
        const uint8_t *cursor = data + sizeof(header);
        size_t remaining = size - sizeof(header);

        size_t playersBytes = header.playerCount * sizeof(NetPlayerSnapshot);
        size_t enemiesBytes = header.enemyCount * sizeof(NetEnemySnapshot);
        size_t pickupsBytes = header.pickupCount * sizeof(NetPickupSnapshot);
        size_t strikesBytes = header.strikeCount * sizeof(NetStrikeSnapshot);

        if (remaining < playersBytes + enemiesBytes + pickupsBytes + strikesBytes)
            return false;

        players.resize(header.playerCount);
        if (playersBytes > 0)
        {
            std::memcpy(players.data(), cursor, playersBytes);
            cursor += playersBytes;
        }

        enemies.resize(header.enemyCount);
        if (enemiesBytes > 0)
        {
            std::memcpy(enemies.data(), cursor, enemiesBytes);
            cursor += enemiesBytes;
        }

        pickups.resize(header.pickupCount);
        if (pickupsBytes > 0)
        {
            std::memcpy(pickups.data(), cursor, pickupsBytes);
            cursor += pickupsBytes;
        }

        strikes.resize(header.strikeCount);
        if (strikesBytes > 0)
            std::memcpy(strikes.data(), cursor, strikesBytes);

        return true;
    }

    void buildLobbySync(
        const NetSLobbySyncHeader &header,
        const NetLobbyPlayerEntry *players,
        std::vector<uint8_t> &out)
    {
        size_t totalSize = sizeof(NetSLobbySyncHeader)
            + header.playerCount * sizeof(NetLobbyPlayerEntry);
        out.resize(totalSize);
        uint8_t *cursor = out.data();
        std::memcpy(cursor, &header, sizeof(header));
        cursor += sizeof(header);

        if (header.playerCount > 0 && players)
        {
            size_t bytes = header.playerCount * sizeof(NetLobbyPlayerEntry);
            std::memcpy(cursor, players, bytes);
        }
    }

    bool parseLobbySync(
        const uint8_t *data,
        size_t size,
        NetSLobbySyncHeader &header,
        std::vector<NetLobbyPlayerEntry> &players)
    {
        if (size < sizeof(NetSLobbySyncHeader))
            return false;

        std::memcpy(&header, data, sizeof(header));
        size_t playersBytes = header.playerCount * sizeof(NetLobbyPlayerEntry);
        if (size < sizeof(NetSLobbySyncHeader) + playersBytes)
            return false;

        players.resize(header.playerCount);
        if (playersBytes > 0)
            std::memcpy(players.data(), data + sizeof(header), playersBytes);
        return true;
    }
}
