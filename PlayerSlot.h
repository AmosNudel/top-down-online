#ifndef PLAYER_SLOT_H
#define PLAYER_SLOT_H

#include <raylib.h>
#include <cstdint>
#include <cstring>

#include "GameConfig.h"

struct PlayerSlot
{
    uint8_t id{0};
    char name[GameConfig::kPlayerNameMaxLen + 1]{};
    uint16_t kills{0};
    uint8_t colorIndex{0};
    bool inQueue{false};
    bool inMatch{false};
    bool ready{false};
    bool connected{false};
};

static const Color kPlayerColors[GameConfig::kMaxPlayers] = {
    {230, 41, 55, 255},
    {0, 121, 241, 255},
    {0, 228, 48, 255},
    {255, 214, 0, 255},
    {200, 122, 255, 255},
    {255, 161, 0, 255},
    {0, 228, 228, 255},
    {255, 109, 194, 255},
    {130, 201, 30, 255},
    {102, 191, 255, 255},
};

inline void PlayerSlotSetName(PlayerSlot &slot, const char *name)
{
    if (!name)
    {
        slot.name[0] = '\0';
        return;
    }
    std::strncpy(slot.name, name, GameConfig::kPlayerNameMaxLen);
    slot.name[GameConfig::kPlayerNameMaxLen] = '\0';
}

#endif // PLAYER_SLOT_H
