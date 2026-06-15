#ifndef GAME_CONFIG_H
#define GAME_CONFIG_H

#include <cstddef>
#include <cstdint>

namespace GameConfig
{
    constexpr int kGameWidth = 600;
    constexpr int kGameHeight = 600;

    constexpr int kMaxPlayers = 10;
    constexpr int kPlayerNameMaxLen = 5;

#if defined(NDEBUG) && !defined(DEV_SERVER)
    constexpr int kMinPlayersToStart = 2;
#else
    constexpr int kMinPlayersToStart = 1;
#endif

    constexpr float kReadyCountdownSec = 5.f;
    constexpr float kMatchDurationSec = 5.f * 60.f;
    constexpr float kResultsDurationSec = 20.f;

    constexpr std::size_t kMaxEnemies = 200;
    constexpr float kSpawnInterval = 2.f;
    constexpr int kInitialSpawnWaveCount = 2;
    constexpr int kSpawnRadiusMin = 250;
    constexpr int kSpawnRadiusMax = 450;

    constexpr float kPickupSpawnInterval = 10.f;
    constexpr std::size_t kMaxPickups = 5;
    constexpr float kStrikeHitRadius = 120.f;
    constexpr float kSwordHitRadius = 72.f;
    constexpr float kSwordHitMinDist = 8.f;
    // half-width of the forward sword cone (45 deg each side of facing = 90 deg arc)
    constexpr float kSwordConeHalfAngle = 3.14159265f / 4.f;

    constexpr int kGroupSpawnEveryNWaves = 4;
    constexpr int kGroupSpawnMin = 10;
    constexpr int kGroupSpawnMax = 16;

    constexpr float kPlayerSpawnMinSpacing = 140.f;
    // Minimum body hitbox used for enemy contact damage (covers headless / missing textures).
    constexpr float kContactHitboxMin = 72.f;

    constexpr float kMapScale = 4.f;
    constexpr int kMapCols = 48;
    constexpr int kMapRows = 48;
    constexpr float kMapMargin = 150.f;
    constexpr int kPropsPerType = 20;

    constexpr uint16_t kDefaultServerPort = 27015;
    constexpr uint16_t kDefaultStreamPort = 27016;
    constexpr float kServerTickRate = 45.f;
    constexpr const char *kDefaultServerHost = "127.0.0.1";

    // client-side prediction / interpolation tuning
    constexpr float kReconcileSoftDist = 24.f;
    constexpr float kReconcileHardDist = 96.f;
    constexpr float kInterpExtrapolateSec = 0.18f;
    constexpr float kInterpBufferDelaySec = 0.09f; // ~2 snapshots at 45 Hz
    constexpr float kEnemyNearSnapDist = 220.f; // use latest server pos when close to local player
    constexpr std::size_t kInterpBufferMaxSnapshots = 16;
    constexpr std::size_t kMinSnapshotsForPlay = 3;
}

#endif // GAME_CONFIG_H
