#ifndef GAME_SIMULATION_H
#define GAME_SIMULATION_H

#include "Character.h"
#include "Enemy.h"
#include "GameConfig.h"
#include "NetCommon.h"
#include "Pickup.h"
#include "PlayerSlot.h"
#include "Prop.h"
#include "Thunderstrike.h"
#include "TileMap.h"
#include <raylib.h>
#include <chrono>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

enum class MatchPhase
{
    IDLE,
    QUEUE,
    COUNTDOWN,
    IN_PROGRESS,
    RESULTS
};

enum class JoinResult
{
    OK,
    INVALID_NAME,
    SERVER_FULL,
    ALREADY_IN_QUEUE,
    MATCH_IN_PROGRESS
};

struct PlayerInput
{
    Vector2 move{};
    bool attackPressed{false};
    bool attackHeld{false};
    bool thunderPressed{false};
};

struct PlayerEntry
{
    Character character;
    PlayerSlot slot;

    PlayerEntry()
        : character{GameConfig::kGameWidth, GameConfig::kGameHeight}
    {
    }
};

class GameSimulation
{
public:
    GameSimulation();
    ~GameSimulation();

    void init(unsigned int mapSeed);
    JoinResult tryJoinQueue(const char *name, bool isServer = false, int *outPlayerId = nullptr);
    void removePlayer(int playerId);
    void markPlayerDisconnected(int playerId);
    void leaveQueue();
    void pressReady();
    bool pressReadyForPlayer(int playerId);
    void tick(float dt, bool applyLocalInput, int localPlayerId, const PlayerInput &input);
    void tickServer(float dt);
    void setRemoteInput(int playerId, const PlayerInput &input);
    void spectateForPlayer(int playerId, int direction);
    void tickInterpolation();
    void tickResultsCountdownClient(float dt);
    void refreshEnemyRenderAnchors();
    void refreshPlayerRenderAnchors();

    void buildWorldStatePacket(std::vector<uint8_t> &out) const;
    bool applyWorldStatePacket(const uint8_t *data, size_t size);
    void buildLobbySyncPacket(std::vector<uint8_t> &out) const;
    bool applyLobbySyncPacket(const uint8_t *data, size_t size);
    void applyJoinAck(int playerId, uint8_t colorIndex, const char *name);
    void resetWorldStateSync();
    float noteSnapshotArrival(uint32_t serverTick);
    void predictLocalPlayerMovement(int localPlayerId, float dt, const PlayerInput &input);
    void predictLocalThunderCast(int localPlayerId, const PlayerInput &input);

    unsigned int getMapSeed() const { return mapSeed; }
    bool isOnlineClient() const { return onlineClientMode; }
    void setOnlineClientMode(bool enabled) { onlineClientMode = enabled; }

    void spectateNext();
    void spectatePrev();
    void setLocalPlayerId(int id) { localPlayerId = id; }

    MatchPhase getPhase() const { return phase; }
    int getQueueCount() const;
    bool canPressReady() const;
    bool hasUnreadyQueuePlayers() const;
    float getCountdownRemaining() const { return countdownRemaining; }
    float getMatchTimeRemaining() const { return matchTimeRemaining; }
    float getResultsTimeRemaining() const { return resultsTimeRemaining; }
    const char *getStatusMessage() const { return statusMessage.c_str(); }

    Character &getCameraCharacter();
    const Character &getCameraCharacter() const;
    int getLocalPlayerId() const { return localPlayerId; }
    int getSpectateTargetId() const { return spectateTargetId; }

    const std::vector<PlayerEntry> &getPlayers() const { return players; }
    const std::vector<Enemy> &getEnemies() const { return enemies; }
    const std::vector<Pickup> &getPickups() const { return pickups; }
    const std::vector<Thunderstrike> &getStrikes() const { return strikes; }
    const std::vector<Prop> &getProps() const { return props; }
    TileMap &getTileMap() { return tileMap; }
    const TileMap &getTileMap() const { return tileMap; }

    float getWorldWidth() const { return worldWidth; }
    float getWorldHeight() const { return worldHeight; }
    Vector2 getMapCenter() const { return mapCenter; }

    bool isThunderActive() const;
    bool isThunderActiveForPlayer(int playerId) const;
    int getWaveNumber() const { return waveNumber; }

    void getWinners(std::vector<int> &outPlayerIds) const;
    int countAlivePlayers() const;
    bool isResultsComplete() const { return phase == MatchPhase::RESULTS && resultsTimeRemaining <= 0.f; }
    bool isResultsCountdownFinished() const
    {
        return phase == MatchPhase::RESULTS && resultsTimeRemaining < 0.5f;
    }
    void returnPlayersToMenu();

private:
    void resetMatchState();
    void startMatch();
    void endMatch();
    void tickMatchFlow(float dt);
    void tickGameplay(float dt, int localPlayerId, const PlayerInput &input, bool applyLocalInput);

    int allocateColorIndex() const;
    Vector2 spawnWorldPosForCharacter(const Character &character) const;
    Vector2 playerWorldCenter(const Character &character) const;

    void resolvePlayerMovement(Character &player, float dt, const PlayerInput &input);
    void tickPickupSpawns(float dt);
    void tickPickups(float dt, Character &player);
    bool allQueuePlayersReady() const;
    void tickEnemyWaves(float dt);
    void tickCombat(float dt, int localPlayerId, const PlayerInput &input, Character &player);
    void tickThunder(float dt, int killerPlayerId, Character &player, const PlayerInput &input);
    void tryThunderCast(int killerPlayerId, Character &player, const PlayerInput &input);
    void tickThunderStrikes(float dt);
    void registerKill(uint8_t killerPlayerId);
    bool isEnemyInSwordCone(const Character &player, const Enemy &enemy) const;

    Character *getPlayerCharacter(int playerId);
    const Character *getPlayerCharacter(int playerId) const;
    PlayerSlot *getPlayerSlot(int playerId);
    int nextRoundRobinTargetPlayerId();
    void retargetEnemiesFromDeadPlayers();
    void removeDeadEnemies();
    void refreshEnemyChaseTargets();
    void tickEnemyPlayerContactDamage(float dt);
    void spawnEnemyAt(uint8_t forcedTargetId = 255);
    void spawnEnemyWave(int count, bool groupSpawn, uint8_t groupTargetId = 255);
    bool isValidEnemySpawnPos(Vector2 pos, float w, float h) const;
    bool findEnemySpawnPos(Vector2 anchorPos, float w, float h, Vector2 &outPos) const;
    bool trySpawnPickupAt(Vector2 pos);
    void spawnPickup();
    void spawnPickupNearPlayer(const Character &player);
    void updateEnemyCameraAnchors();
    void rebuildSpectateTarget();

    TileMap tileMap;
    std::vector<Prop> props;
    std::vector<PlayerEntry> players;
    std::vector<Enemy> enemies;
    std::vector<Pickup> pickups;
    std::vector<Thunderstrike> strikes;

    Texture2D rockTexture{};
    Texture2D logTexture{};
    Texture2D goblinIdle{};
    Texture2D goblinRun{};
    Texture2D slimeIdle{};
    Texture2D slimeRun{};
    Texture2D thunderTexture{};
    Texture2D pickupSheet{};
    Sound thunderSound{};

    Rectangle healthPotionSrc{};
    Vector2 mapCenter{};
    Vector2 baseSpawnPos{};
    float worldWidth{};
    float worldHeight{};

    MatchPhase phase{MatchPhase::IDLE};
    std::string statusMessage;

    float countdownRemaining{};
    float matchTimeRemaining{};
    float resultsTimeRemaining{};

    float pickupSpawnTimer{};
    float spawnTimer{};
    int spawnCount{1};
    int waveNumber{0};
    int nextEnemySpawnTargetIdx{0};
    uint16_t nextEnemyId{1};
    bool playerThunderCasting[GameConfig::kMaxPlayers]{};

    int localPlayerId{-1};
    int spectateTargetId{-1};
    bool texturesLoaded{false};
    bool onlineClientMode{false};
    unsigned int mapSeed{0};
    uint32_t lastWorldStateTick{0};
    mutable uint32_t worldStateTick{0};
    PlayerInput remoteInputs[GameConfig::kMaxPlayers]{};
    PlayerInput lastRemoteInputs[GameConfig::kMaxPlayers]{};
    bool hasRemoteInput[GameConfig::kMaxPlayers]{};
    bool pendingAttack[GameConfig::kMaxPlayers]{};
    bool pendingThunder[GameConfig::kMaxPlayers]{};

    struct EntityInterp
    {
        Vector2 from{};
        Vector2 to{};
    };
    struct BufferedSnapshot
    {
        std::chrono::steady_clock::time_point receiveTime{};
        uint32_t serverTick{0};
        std::unordered_map<uint8_t, Vector2> playerPos;
        std::unordered_map<uint16_t, Vector2> enemyPos;
    };
    void reconcileLocalPlayerPosition(Character &player, Vector2 serverPos);
    static Vector2 interpolateEntityPos(const EntityInterp &interp, float t);
    static Vector2 lerpPos(Vector2 from, Vector2 to, float t);
    static void updateFacingFromMotion(BaseCharacter &entity, Vector2 motion);
    void pushSnapshotBuffer(
        uint32_t serverTick,
        const std::vector<NetPlayerSnapshot> &playerSnaps,
        const std::vector<NetEnemySnapshot> &enemySnaps);
    void pruneSnapshotBuffer(std::chrono::steady_clock::time_point displayTime);
    std::deque<BufferedSnapshot> snapshotBuffer;
    std::chrono::steady_clock::time_point lastSnapshotTime{};
    std::chrono::steady_clock::time_point lastSnapshotArrivalTime{};
    bool hasSnapshotTime{false};
    bool hasSnapshotArrivalTime{false};
};

#endif // GAME_SIMULATION_H
