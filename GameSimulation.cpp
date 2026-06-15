#include "GameSimulation.h"
#include "NetCommon.h"
#include "TextureUtil.h"
#include "ClientDiagLog.h"
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>

namespace
{
    Vector2 CharacterScreenOffset(const Character &character)
    {
        return character.getScreenPos();
    }

    Vector2 SpawnOffsetForIndex(int idx, int count)
    {
        if (count <= 1)
            return {0.f, 0.f};

        const float ring = GameConfig::kPlayerSpawnMinSpacing / (2.f * sinf(PI / count));
        const float angle = (2.f * PI * idx) / count;
        return {cosf(angle) * ring, sinf(angle) * ring};
    }

    Rectangle inflateCollisionRec(Rectangle rec, float minSize)
    {
        if (rec.width < minSize)
        {
            rec.x -= (minSize - rec.width) * 0.5f;
            rec.width = minSize;
        }
        if (rec.height < minSize)
        {
            rec.y -= (minSize - rec.height) * 0.5f;
            rec.height = minSize;
        }
        return rec;
    }
}

GameSimulation::GameSimulation()
    : tileMap{GameConfig::kMapCols, GameConfig::kMapRows, GameConfig::kMapScale}
{
}

GameSimulation::~GameSimulation()
{
    if (!texturesLoaded)
        return;

    UnloadTexture(rockTexture);
    UnloadTexture(logTexture);
    UnloadTexture(goblinIdle);
    UnloadTexture(goblinRun);
    UnloadTexture(slimeIdle);
    UnloadTexture(slimeRun);
    UnloadTexture(thunderTexture);
    UnloadTexture(pickupSheet);
    UnloadSound(thunderSound);
}

void GameSimulation::init(unsigned int seed)
{
    mapSeed = seed;
    tileMap.generate(mapSeed);
    worldWidth = tileMap.worldWidth();
    worldHeight = tileMap.worldHeight();
    mapCenter = Vector2{worldWidth / 2.f, worldHeight / 2.f};

    Character tempChar{GameConfig::kGameWidth, GameConfig::kGameHeight};
    baseSpawnPos = Vector2Subtract(mapCenter, tempChar.getScreenPos());
    tileMap.carveLand(mapCenter, 160.f);

    if (!texturesLoaded)
    {
        rockTexture = LoadTexture("nature_tileset/Rock.png");
        logTexture = LoadTexture("nature_tileset/Log.png");
        goblinIdle = LoadTexture("characters/goblin_idle_spritesheet.png");
        goblinRun = LoadTexture("characters/goblin_run_spritesheet.png");
        slimeIdle = LoadTexture("characters/slime_idle_spritesheet.png");
        slimeRun = LoadTexture("characters/slime_run_spritesheet.png");
        ConfigurePixelArtTexture(goblinIdle);
        ConfigurePixelArtTexture(goblinRun);
        ConfigurePixelArtTexture(slimeIdle);
        ConfigurePixelArtTexture(slimeRun);
        thunderTexture = LoadTexture("vfx/Thunderstrike w blur.png");
        thunderSound = LoadSound("sfx/18_Thunder_02.wav");
        pickupSheet = LoadTexture("pickups/#2 - Transparent Icons & Drop Shadow.png");

        const int pickupCellSize{32};
        healthPotionSrc = Rectangle{
            0.f, (10 - 1) * pickupCellSize,
            (float)pickupCellSize, (float)pickupCellSize};

        texturesLoaded = true;
    }

    props.clear();
    props.reserve(GameConfig::kPropsPerType * 2);

    SetRandomSeed(seed ^ 0x50524F50u);

    const Vector2 startCenter = Vector2Add(baseSpawnPos, CharacterScreenOffset(tempChar));
    const float clearance{200.f};
    const Rectangle startRec{
        startCenter.x - clearance, startCenter.y - clearance,
        clearance * 2.f, clearance * 2.f};

    std::vector<Rectangle> occupiedRecs;
    auto placeProp = [&](Texture2D tex)
    {
        float w = tex.width * GameConfig::kMapScale;
        float h = tex.height * GameConfig::kMapScale;
        Vector2 pos{};
        Rectangle rec{};
        for (int attempt = 0; attempt < 100; attempt++)
        {
            pos = tileMap.findRandomLandWorldPos(GameConfig::kMapMargin);
            rec = Rectangle{pos.x, pos.y, w, h};
            if (CheckCollisionRecs(rec, startRec))
                continue;
            bool overlaps = false;
            for (const auto &other : occupiedRecs)
            {
                if (CheckCollisionRecs(rec, other))
                {
                    overlaps = true;
                    break;
                }
            }
            if (!overlaps)
                break;
        }
        occupiedRecs.push_back(rec);
        props.emplace_back(pos, tex);
    };

    for (int i = 0; i < GameConfig::kPropsPerType; i++)
    {
        placeProp(rockTexture);
        placeProp(logTexture);
    }

    if (!onlineClientMode)
    {
        phase = MatchPhase::IDLE;
        statusMessage.clear();
    }
}

JoinResult GameSimulation::tryJoinQueue(const char *name, bool isServer, int *outPlayerId)
{
    if (!name || name[0] == '\0')
        return JoinResult::INVALID_NAME;

    if (phase == MatchPhase::IN_PROGRESS || phase == MatchPhase::RESULTS)
        return JoinResult::MATCH_IN_PROGRESS;

    if (!isServer && localPlayerId >= 0)
        return JoinResult::ALREADY_IN_QUEUE;

    int connectedCount = 0;
    for (const auto &p : players)
    {
        if (p.slot.connected)
            connectedCount++;
    }
    if (connectedCount >= GameConfig::kMaxPlayers)
        return JoinResult::SERVER_FULL;

    int slotIndex = -1;
    for (int i = 0; i < (int)players.size(); i++)
    {
        if (!players[i].slot.connected)
        {
            slotIndex = i;
            break;
        }
    }

    PlayerEntry *entryPtr = nullptr;
    if (slotIndex >= 0)
    {
        entryPtr = &players[slotIndex];
        entryPtr->slot.id = static_cast<uint8_t>(slotIndex);
        entryPtr->character.reset();
    }
    else
    {
        players.emplace_back();
        entryPtr = &players.back();
        entryPtr->slot.id = static_cast<uint8_t>(players.size() - 1);
    }

    PlayerEntry &entry = *entryPtr;
    PlayerSlotSetName(entry.slot, name);
    entry.slot.colorIndex = static_cast<uint8_t>(allocateColorIndex());
    entry.slot.inQueue = true;
    entry.slot.inMatch = false;
    entry.slot.ready = false;
    entry.slot.connected = true;
    entry.slot.kills = 0;
    entry.character.setPlayerColor(kPlayerColors[entry.slot.colorIndex]);

    if (!isServer)
        localPlayerId = entry.slot.id;

    if (outPlayerId)
        *outPlayerId = entry.slot.id;

    if (phase == MatchPhase::IDLE)
        phase = MatchPhase::QUEUE;

    statusMessage = "Waiting for players...";
    return JoinResult::OK;
}

void GameSimulation::markPlayerDisconnected(int playerId)
{
    if (playerId < 0 || playerId >= (int)players.size())
        return;

    auto &slot = players[playerId].slot;
    slot.connected = false;
    slot.inQueue = false;
    slot.inMatch = false;
    slot.ready = false;
}

void GameSimulation::removePlayer(int playerId)
{
    if (playerId < 0 || playerId >= (int)players.size())
        return;

    markPlayerDisconnected(playerId);

    if (localPlayerId == playerId)
        localPlayerId = -1;

    if (spectateTargetId == playerId)
        spectateTargetId = -1;

    bool anyoneConnected = false;
    for (const auto &p : players)
    {
        if (p.slot.connected)
        {
            anyoneConnected = true;
            break;
        }
    }

    if (!anyoneConnected)
    {
        phase = MatchPhase::IDLE;
        statusMessage.clear();
    }
    else if (phase == MatchPhase::QUEUE || phase == MatchPhase::COUNTDOWN)
    {
        for (auto &p : players)
            p.slot.ready = false;
        if (phase == MatchPhase::COUNTDOWN)
        {
            phase = MatchPhase::QUEUE;
            countdownRemaining = 0.f;
        }
    }
}

void GameSimulation::leaveQueue()
{
    if (localPlayerId < 0)
        return;

    removePlayer(localPlayerId);
}

void GameSimulation::pressReady()
{
    if (localPlayerId >= 0)
        pressReadyForPlayer(localPlayerId);
}

bool GameSimulation::pressReadyForPlayer(int playerId)
{
    if (phase != MatchPhase::QUEUE || !canPressReady())
        return false;

    if (playerId >= 0 && playerId < (int)players.size())
        players[playerId].slot.ready = true;

    if (!allQueuePlayersReady())
        return true;

    prepareMatchForCountdown();
    countdownRemaining = GameConfig::kReadyCountdownSec;
    phase = MatchPhase::COUNTDOWN;
    statusMessage = "Match starting soon...";
    return true;
}

bool GameSimulation::allQueuePlayersReady() const
{
    int queueCount = 0;
    int readyCount = 0;
    for (const auto &p : players)
    {
        if (!p.slot.connected || !p.slot.inQueue)
            continue;
        queueCount++;
        if (p.slot.ready)
            readyCount++;
    }
    return queueCount > 0 && readyCount == queueCount;
}

int GameSimulation::getQueueCount() const
{
    int count = 0;
    for (const auto &p : players)
    {
        if (p.slot.connected && p.slot.inQueue)
            count++;
    }
    return count;
}

bool GameSimulation::canPressReady() const
{
    return phase == MatchPhase::QUEUE && getQueueCount() >= GameConfig::kMinPlayersToStart;
}

bool GameSimulation::hasUnreadyQueuePlayers() const
{
    return canPressReady() && !allQueuePlayersReady();
}

void GameSimulation::tick(float dt, bool applyLocalInput, int localPlayerIdParam, const PlayerInput &input)
{
    if (onlineClientMode)
        return;

    tickMatchFlow(dt);

    if (phase == MatchPhase::IN_PROGRESS)
        tickGameplay(dt, localPlayerIdParam, input, applyLocalInput);
}

void GameSimulation::setRemoteInput(int playerId, const PlayerInput &input)
{
    if (playerId < 0 || playerId >= GameConfig::kMaxPlayers)
        return;
    remoteInputs[playerId] = input;
    lastRemoteInputs[playerId] = input;
    hasRemoteInput[playerId] = true;
    if (input.attackPressed)
        pendingAttack[playerId] = true;
    if (input.thunderPressed)
        pendingThunder[playerId] = true;
}

void GameSimulation::tickServer(float dt)
{
    tickMatchFlow(dt);

    if (phase != MatchPhase::IN_PROGRESS)
        return;

    if (localPlayerId < 0 && !players.empty())
        localPlayerId = 0;

    for (int i = 0; i < (int)players.size(); i++)
    {
        auto &entry = players[i];
        if (!entry.slot.connected || !entry.slot.inMatch || !entry.character.getAlive())
            continue;

        const PlayerInput &input = hasRemoteInput[i] ? remoteInputs[i] : lastRemoteInputs[i];
        resolvePlayerMovement(entry.character, dt, input);
    }

    tickPickupSpawns(dt);

    for (int i = 0; i < (int)players.size(); i++)
    {
        auto &entry = players[i];
        if (!entry.slot.connected || !entry.slot.inMatch || !entry.character.getAlive())
            continue;

        Character *player = getPlayerCharacter(i);
        if (!player)
            continue;

        const PlayerInput &input = hasRemoteInput[i] ? remoteInputs[i] : lastRemoteInputs[i];
        tickPickups(dt, *player);
        tickCombat(dt, i, input, *player);
        tryThunderCast(i, *player, input);
    }

    tickThunderStrikes(dt);

    for (int i = 0; i < GameConfig::kMaxPlayers; i++)
        hasRemoteInput[i] = false;

    tickEnemyWaves(dt);

    updateEnemyCameraAnchors();
    refreshEnemyChaseTargets();
    retargetEnemiesFromDeadPlayers();

    for (auto &enemy : enemies)
        enemy.tick(dt, true, false);

    tickEnemyPlayerContactDamage(dt);

    removeDeadEnemies();
}

void GameSimulation::tickEnemyPlayerContactDamage(float dt)
{
    const float minContact = GameConfig::kContactHitboxMin;

    for (auto &enemy : enemies)
    {
        if (!enemy.getAlive())
            continue;

        Rectangle enemyRec = inflateCollisionRec(enemy.getWorldCollisionRec(), minContact);
        for (int i = 0; i < (int)players.size(); i++)
        {
            auto &entry = players[i];
            if (!entry.slot.connected || !entry.slot.inMatch || !entry.character.getAlive())
                continue;

            Rectangle playerRec = inflateCollisionRec(
                entry.character.getWorldCollisionRec(), minContact);
            if (!CheckCollisionRecs(playerRec, enemyRec))
                continue;

            entry.character.takeDamage(enemy.getDamagePerSec() * dt);
        }
    }
}

void GameSimulation::returnPlayersToMenu()
{
    for (auto &p : players)
        p.slot.connected = false;
    players.clear();
    localPlayerId = -1;
    spectateTargetId = -1;
    phase = MatchPhase::IDLE;
    statusMessage = "Returned to main menu.";
    resetMatchState();
    resetWorldStateSync();
}

void GameSimulation::tickMatchFlow(float dt)
{
    if (phase == MatchPhase::COUNTDOWN)
    {
        countdownRemaining -= dt;
        if (countdownRemaining <= 0.f)
            startMatch();
    }
    else if (phase == MatchPhase::IN_PROGRESS)
    {
        matchTimeRemaining -= dt;
        if (matchTimeRemaining <= 0.f || countAlivePlayers() == 0)
            endMatch();
    }
    else if (phase == MatchPhase::RESULTS)
    {
        resultsTimeRemaining -= dt;
        if (resultsTimeRemaining < 0.f)
            resultsTimeRemaining = 0.f;
    }
}

void GameSimulation::resetMatchState()
{
    enemies.clear();
    strikes.clear();
    pickups.clear();
    std::memset(playerThunderCasting, 0, sizeof(playerThunderCasting));
    pickupSpawnTimer = 0.f;
    spawnTimer = 0.f;
    spawnCount = GameConfig::kInitialSpawnWaveCount;
    waveNumber = 0;
    nextEnemySpawnTargetIdx = 0;
    nextEnemyId = 1;
    std::memset(pendingAttack, 0, sizeof(pendingAttack));
    std::memset(pendingThunder, 0, sizeof(pendingThunder));
    matchTimeRemaining = 0.f;
    resultsTimeRemaining = 0.f;
    countdownRemaining = 0.f;
}

void GameSimulation::placePlayersAtSpawn()
{
    std::vector<int> queueIds;
    for (int i = 0; i < (int)players.size(); i++)
    {
        if (players[i].slot.connected && players[i].slot.inQueue)
            queueIds.push_back(i);
    }

    const int matchPlayerCount = static_cast<int>(queueIds.size());
    for (int idx = 0; idx < matchPlayerCount; idx++)
    {
        auto &entry = players[queueIds[idx]];
        entry.slot.inMatch = true;
        entry.slot.inQueue = false;
        entry.slot.ready = false;
        entry.slot.kills = 0;
        entry.character.reset();
        entry.character.setPlayerColor(kPlayerColors[entry.slot.colorIndex]);
        entry.character.setAlive(true);

        Vector2 offset = SpawnOffsetForIndex(idx, matchPlayerCount);
        entry.character.setWorldPos(Vector2Add(baseSpawnPos, offset));
    }

    for (auto &entry : players)
    {
        if (!entry.slot.inMatch)
            entry.slot.inQueue = false;
    }

    if (localPlayerId >= 0 && localPlayerId < (int)players.size())
        spectateTargetId = localPlayerId;
}

void GameSimulation::prepareMatchForCountdown()
{
    placePlayersAtSpawn();
}

void GameSimulation::startMatch()
{
    resetMatchState();

    matchTimeRemaining = GameConfig::kMatchDurationSec;
    nextEnemySpawnTargetIdx = 0;

    for (int i = 0; i < 2; i++)
        spawnEnemyWave(1, false);

    if (localPlayerId >= 0 && localPlayerId < (int)players.size())
        spawnPickupNearPlayer(players[localPlayerId].character);

    phase = MatchPhase::IN_PROGRESS;
    statusMessage.clear();
}

void GameSimulation::endMatch()
{
    phase = MatchPhase::RESULTS;
    resultsTimeRemaining = GameConfig::kResultsDurationSec;
    statusMessage = "Match over!";
}

int GameSimulation::allocateColorIndex() const
{
    bool used[GameConfig::kMaxPlayers]{};
    for (const auto &p : players)
    {
        if (p.slot.connected)
            used[p.slot.colorIndex] = true;
    }

    for (int i = 0; i < GameConfig::kMaxPlayers; i++)
    {
        if (!used[i])
            return i;
    }
    return 0;
}

Vector2 GameSimulation::spawnWorldPosForCharacter(const Character &character) const
{
    return Vector2Add(character.getWorldPos(), character.getScreenPos());
}

Vector2 GameSimulation::playerWorldCenter(const Character &character) const
{
    return character.getWorldCenter();
}

int GameSimulation::nextRoundRobinTargetPlayerId()
{
    std::vector<int> alive;
    for (int i = 0; i < (int)players.size(); i++)
    {
        if (players[i].slot.inMatch && players[i].character.getAlive())
            alive.push_back(i);
    }

    if (alive.empty())
        return -1;

    int id = alive[nextEnemySpawnTargetIdx % alive.size()];
    nextEnemySpawnTargetIdx = (nextEnemySpawnTargetIdx + 1) % alive.size();
    return id;
}

void GameSimulation::retargetEnemiesFromDeadPlayers()
{
    for (auto &enemy : enemies)
    {
        if (!enemy.getAlive())
            continue;

        int tid = enemy.getTargetPlayerId();
        Character *target = getPlayerCharacter(tid);
        if (target && target->getAlive())
            continue;

        int newTarget = nextRoundRobinTargetPlayerId();
        if (newTarget < 0)
            continue;

        enemy.setTargetPlayerId(static_cast<uint8_t>(newTarget));
        enemy.setChaseTarget(getPlayerCharacter(newTarget));
    }
}

void GameSimulation::refreshEnemyChaseTargets()
{
    for (auto &enemy : enemies)
    {
        if (!enemy.getAlive())
            continue;
        enemy.setChaseTarget(getPlayerCharacter(enemy.getTargetPlayerId()));
    }
}

void GameSimulation::removeDeadEnemies()
{
    enemies.erase(
        std::remove_if(enemies.begin(), enemies.end(),
                       [](Enemy &enemy) { return !enemy.getAlive(); }),
        enemies.end());
}

Character *GameSimulation::getPlayerCharacter(int playerId)
{
    if (playerId < 0 || playerId >= (int)players.size() || !players[playerId].slot.connected)
        return nullptr;
    return &players[playerId].character;
}

const Character *GameSimulation::getPlayerCharacter(int playerId) const
{
    if (playerId < 0 || playerId >= (int)players.size() || !players[playerId].slot.connected)
        return nullptr;
    return &players[playerId].character;
}

PlayerSlot *GameSimulation::getPlayerSlot(int playerId)
{
    if (playerId < 0 || playerId >= (int)players.size())
        return nullptr;
    return &players[playerId].slot;
}

bool GameSimulation::isValidEnemySpawnPos(Vector2 pos, float w, float h) const
{
    const Vector2 corners[5] = {
        {pos.x + w * 0.5f, pos.y + h * 0.5f},
        {pos.x, pos.y},
        {pos.x + w, pos.y},
        {pos.x, pos.y + h},
        {pos.x + w, pos.y + h},
    };

    for (const Vector2 &point : corners)
    {
        if (tileMap.isWaterAtWorld(point))
            return false;
    }

    const Rectangle rec{pos.x, pos.y, w, h};
    for (const auto &prop : props)
    {
        if (CheckCollisionRecs(rec, prop.getWorldCollisionRec()))
            return false;
    }

    return true;
}

bool GameSimulation::findEnemySpawnPos(Vector2 anchorPos, float w, float h, Vector2 &outPos) const
{
    for (int attempt = 0; attempt < 50; attempt++)
    {
        float angle = (float)GetRandomValue(0, 359) * DEG2RAD;
        float dist = (float)GetRandomValue(GameConfig::kSpawnRadiusMin, GameConfig::kSpawnRadiusMax);
        Vector2 offset{cosf(angle) * dist, sinf(angle) * dist};
        Vector2 candidate = Vector2Add(anchorPos, offset);
        candidate.x = Clamp(candidate.x, GameConfig::kMapMargin, worldWidth - GameConfig::kMapMargin);
        candidate.y = Clamp(candidate.y, GameConfig::kMapMargin, worldHeight - GameConfig::kMapMargin);

        if (!isValidEnemySpawnPos(candidate, w, h))
            continue;

        outPos = candidate;
        return true;
    }

    return false;
}

void GameSimulation::spawnEnemyAt(uint8_t forcedTargetId)
{
    if (enemies.size() >= GameConfig::kMaxEnemies)
        return;

    int targetId = (forcedTargetId < players.size())
                       ? static_cast<int>(forcedTargetId)
                       : nextRoundRobinTargetPlayerId();
    if (targetId < 0)
        return;

    Character *targetChar = getPlayerCharacter(targetId);
    if (!targetChar || !targetChar->getAlive())
        return;

    Vector2 anchorPos = playerWorldCenter(*targetChar);

    bool useGoblin = GetRandomValue(0, 1) == 0;
    Texture2D idleTex = useGoblin ? goblinIdle : slimeIdle;
    Texture2D runTex = useGoblin ? goblinRun : slimeRun;
    float w = (float)idleTex.width / 6.f * GameConfig::kMapScale;
    float h = (float)idleTex.height * GameConfig::kMapScale;

    Vector2 pos{};
    if (!findEnemySpawnPos(anchorPos, w, h, pos))
        return;

    enemies.emplace_back(pos, idleTex, runTex);
    Enemy &enemy = enemies.back();
    enemy.setId(nextEnemyId++);
    enemy.setEnemyType(useGoblin ? 0 : 1);
    enemy.setTargetPlayerId(static_cast<uint8_t>(targetId));
    enemy.setChaseTarget(targetChar);
    enemy.setObstacles(&props);
}

void GameSimulation::spawnEnemyWave(int count, bool groupSpawn, uint8_t groupTargetId)
{
    for (int i = 0; i < count; i++)
        spawnEnemyAt(groupSpawn ? groupTargetId : 255);
}

bool GameSimulation::trySpawnPickupAt(Vector2 pos)
{
    float w = healthPotionSrc.width * Pickup::kScale;
    float h = healthPotionSrc.height * Pickup::kScale;
    Rectangle rec{pos.x, pos.y, w, h};

    Vector2 center{pos.x + w / 2.f, pos.y + h / 2.f};
    if (tileMap.isWaterAtWorld(center))
        return false;

    for (auto &prop : props)
    {
        if (CheckCollisionRecs(rec, prop.getWorldCollisionRec()))
            return false;
    }

    for (auto &pickup : pickups)
    {
        if (!pickup.isActive())
            continue;
        if (CheckCollisionRecs(rec, pickup.getWorldCollisionRec()))
            return false;
    }

    pickups.emplace_back(pos, pickupSheet, healthPotionSrc, PickupType::HEALTH);
    return true;
}

void GameSimulation::spawnPickup()
{
    size_t activeCount = 0;
    for (const auto &pickup : pickups)
    {
        if (pickup.isActive())
            activeCount++;
    }
    if (activeCount >= GameConfig::kMaxPickups)
        return;

    for (int attempt = 0; attempt < 50; attempt++)
    {
        Vector2 pos = tileMap.findRandomLandWorldPos(GameConfig::kMapMargin);
        if (trySpawnPickupAt(pos))
            return;
    }
}

void GameSimulation::spawnPickupNearPlayer(const Character &player)
{
    size_t activeCount = 0;
    for (const auto &pickup : pickups)
    {
        if (pickup.isActive())
            activeCount++;
    }
    if (activeCount >= GameConfig::kMaxPickups)
        return;

    Vector2 playerCenter = playerWorldCenter(player);
    float w = healthPotionSrc.width * Pickup::kScale;
    float h = healthPotionSrc.height * Pickup::kScale;

    for (int attempt = 0; attempt < 30; attempt++)
    {
        float angle = (float)GetRandomValue(0, 359) * DEG2RAD;
        float dist = (float)GetRandomValue(80, 180);
        Vector2 pos{
            playerCenter.x + cosf(angle) * dist - w / 2.f,
            playerCenter.y + sinf(angle) * dist - h / 2.f};
        if (trySpawnPickupAt(pos))
            return;
    }
}

void GameSimulation::updateEnemyCameraAnchors()
{
    Character &camera = getCameraCharacter();
    for (auto &enemy : enemies)
    {
        enemy.setCameraAnchor(&camera);
        int tid = enemy.getTargetPlayerId();
        enemy.setChaseTarget(getPlayerCharacter(tid));
    }
}

void GameSimulation::resolvePlayerMovement(Character &player, float dt, const PlayerInput &input, bool checkCollision)
{
    (void)dt;
    player.setVirtualInput(input.move, input.attackPressed, input.attackHeld);
    if (input.move.x < -0.01f)
        player.setFacingDirection(-1.f);
    else if (input.move.x > 0.01f)
        player.setFacingDirection(1.f);
    player.tick(dt, true, false, false);

    if (!checkCollision)
        return;

    if (player.getWorldPos().x < 0.f ||
        player.getWorldPos().y < 0.f ||
        player.getWorldPos().x + GameConfig::kGameWidth > worldWidth ||
        player.getWorldPos().y + GameConfig::kGameHeight > worldHeight)
    {
        player.undoMovement();
    }

    Rectangle body = player.getWorldCollisionRec();
    Vector2 feet{body.x + body.width * 0.5f, body.y + body.height * 0.85f};
    if (tileMap.isWaterAtWorld(feet))
        player.undoMovement();

    for (auto &prop : props)
    {
        if (CheckCollisionRecs(prop.getPlayerCollisionRec(player.getWorldPos()), player.getPropCollisionRec()))
            player.undoMovement();
    }
}

void GameSimulation::tickPickupSpawns(float dt)
{
    pickupSpawnTimer += dt;
    if (pickupSpawnTimer >= GameConfig::kPickupSpawnInterval)
    {
        pickupSpawnTimer = 0.f;
        spawnPickup();
    }
}

void GameSimulation::tickPickups(float dt, Character &player)
{
    (void)dt;
    for (auto &pickup : pickups)
    {
        if (pickup.isActive() &&
            CheckCollisionRecs(pickup.getWorldCollisionRec(), player.getWorldCollisionRec()))
        {
            pickup.collect(player);
        }
    }

    pickups.erase(
        std::remove_if(pickups.begin(), pickups.end(),
                       [](const Pickup &p) { return !p.isActive(); }),
        pickups.end());
}

void GameSimulation::tickEnemyWaves(float dt)
{
    spawnTimer += dt;
    if (spawnTimer < GameConfig::kSpawnInterval || enemies.size() >= GameConfig::kMaxEnemies)
        return;

    spawnTimer = 0.f;
    waveNumber++;

    bool groupSpawn = (waveNumber % GameConfig::kGroupSpawnEveryNWaves) == 0;
    if (groupSpawn)
    {
        std::vector<int> aliveIds;
        for (int i = 0; i < (int)players.size(); i++)
        {
            if (players[i].slot.inMatch && players[i].character.getAlive())
                aliveIds.push_back(i);
        }
        if (!aliveIds.empty())
        {
            int pick = aliveIds[GetRandomValue(0, (int)aliveIds.size() - 1)];
            int groupCount = GetRandomValue(GameConfig::kGroupSpawnMin, GameConfig::kGroupSpawnMax);
            spawnEnemyWave(groupCount, true, static_cast<uint8_t>(pick));
        }
    }
    else
    {
        spawnEnemyWave(spawnCount, false);
        spawnCount++;
    }
}

void GameSimulation::registerKill(uint8_t killerPlayerId)
{
    PlayerSlot *slot = getPlayerSlot(killerPlayerId);
    if (slot && slot->inMatch)
        slot->kills++;
}

namespace
{
    float normalizeAngleRad(float radians)
    {
        while (radians > PI)
            radians -= 2.f * PI;
        while (radians < -PI)
            radians += 2.f * PI;
        return radians;
    }
}

bool GameSimulation::isEnemyInSwordCone(const Character &player, const Enemy &enemy) const
{
    Vector2 playerCenter = playerWorldCenter(player);
    Vector2 enemyCenter = enemy.getWorldCenter();
    Vector2 toEnemy = Vector2Subtract(enemyCenter, playerCenter);
    float dist = Vector2Length(toEnemy);

    Rectangle enemyRec = enemy.getWorldCollisionRec();
    const float enemyRadius = (enemyRec.width + enemyRec.height) * 0.15f;
    const float maxReach = GameConfig::kSwordHitRadius;
    const float minReach = GameConfig::kSwordHitMinDist;
    const float halfAngle = GameConfig::kSwordConeHalfAngle;

    if (dist <= 0.001f)
        return false;
    if (dist + enemyRadius < minReach)
        return false;
    if (dist - enemyRadius > maxReach)
        return false;

    const float facing = player.getFacingDirection();
    const float forwardAngle = facing > 0.f ? 0.f : PI;
    const float enemyAngle = atan2f(toEnemy.y, toEnemy.x);
    const float angleDiff = normalizeAngleRad(enemyAngle - forwardAngle);
    const float angularSlack = atan2f(enemyRadius, fmaxf(dist, minReach));

    if (fabsf(angleDiff) > halfAngle + angularSlack)
        return false;

    const float forwardDist = toEnemy.x * facing;
    if (forwardDist + enemyRadius < minReach)
        return false;

    return true;
}

void GameSimulation::tickCombat(float dt, int localPlayerIdParam, const PlayerInput &input, Character &player)
{
    (void)dt;
    if (!player.getAlive())
        return;

    const bool attackNow = (localPlayerIdParam >= 0 && pendingAttack[localPlayerIdParam])
        || input.attackPressed;

    if (attackNow)
    {
        player.prepareWeaponHitTest(input.attackHeld || attackNow);
        for (auto &enemy : enemies)
        {
            if (!enemy.getAlive())
                continue;

            if (isEnemyInSwordCone(player, enemy))
            {
                enemy.setAlive(false);
                player.registerSwordHit();
                if (localPlayerIdParam >= 0)
                    registerKill(static_cast<uint8_t>(localPlayerIdParam));
            }
        }
        if (localPlayerIdParam >= 0)
            pendingAttack[localPlayerIdParam] = false;
    }
}

void GameSimulation::tryThunderCast(int killerPlayerId, Character &player, const PlayerInput &input)
{
    if (!player.getAlive())
        return;
    if (killerPlayerId < 0 || killerPlayerId >= GameConfig::kMaxPlayers)
        return;

    const bool thunderNow = input.thunderPressed
        || (killerPlayerId >= 0 && pendingThunder[killerPlayerId]);
    if (thunderNow && player.isCharged() && !playerThunderCasting[killerPlayerId]
        && !enemies.empty())
    {
        Vector2 playerCenter = playerWorldCenter(player);
        Enemy *nearest = nullptr;
        float bestDist = 0.f;
        for (auto &enemy : enemies)
        {
            if (!enemy.getAlive())
                continue;
            float d = Vector2DistanceSqr(playerCenter, enemy.getWorldCenter());
            if (!nearest || d < bestDist)
            {
                nearest = &enemy;
                bestDist = d;
            }
        }
        if (nearest)
        {
            strikes.emplace_back(nearest->getWorldCenter(), thunderTexture, thunderSound);
            strikes.back().setOwnerPlayerId(static_cast<uint8_t>(killerPlayerId));
            playerThunderCasting[killerPlayerId] = true;
            if (killerPlayerId >= 0)
                pendingThunder[killerPlayerId] = false;
        }
    }
}

void GameSimulation::tickThunderStrikes(float dt)
{
    for (auto &strike : strikes)
    {
        strike.tick(dt);
        if (!strike.justHit())
            continue;

        uint8_t ownerId = strike.getOwnerPlayerId();
        float radiusSqr = GameConfig::kStrikeHitRadius * GameConfig::kStrikeHitRadius;
        for (auto &enemy : enemies)
        {
            if (!enemy.getAlive())
                continue;
            if (Vector2DistanceSqr(strike.getTargetCenter(), enemy.getWorldCenter()) <= radiusSqr)
            {
                enemy.setAlive(false);
                if (ownerId < GameConfig::kMaxPlayers)
                    registerKill(ownerId);
            }
        }
    }

    strikes.erase(
        std::remove_if(strikes.begin(), strikes.end(),
                       [](const Thunderstrike &s) { return s.isFinished(); }),
        strikes.end());

    for (int i = 0; i < GameConfig::kMaxPlayers; i++)
    {
        if (!playerThunderCasting[i])
            continue;

        bool hasOwnedStrike = false;
        for (const auto &strike : strikes)
        {
            if (strike.getOwnerPlayerId() == static_cast<uint8_t>(i))
            {
                hasOwnedStrike = true;
                break;
            }
        }

        if (!hasOwnedStrike)
        {
            Character *caster = getPlayerCharacter(i);
            if (caster)
                caster->consumeCharge();
            playerThunderCasting[i] = false;
        }
    }
}

void GameSimulation::tickThunder(float dt, int killerPlayerId, Character &player, const PlayerInput &input)
{
    tryThunderCast(killerPlayerId, player, input);
    tickThunderStrikes(dt);
}

void GameSimulation::tickGameplay(float dt, int localPlayerIdParam, const PlayerInput &input, bool applyLocalInput)
{
    updateEnemyCameraAnchors();

    for (int i = 0; i < (int)players.size(); i++)
    {
        auto &entry = players[i];
        if (!entry.slot.connected || !entry.slot.inMatch || !entry.character.getAlive())
            continue;

        if (i == localPlayerIdParam && applyLocalInput)
            resolvePlayerMovement(entry.character, dt, input);
    }

    Character *localChar = getPlayerCharacter(localPlayerIdParam);
    if (applyLocalInput && localChar && localChar->getAlive())
    {
        tickPickupSpawns(dt);
        tickPickups(dt, *localChar);
        tickCombat(dt, localPlayerIdParam, input, *localChar);
        tickThunder(dt, localPlayerIdParam, *localChar, input);
    }
    else
    {
        tickPickupSpawns(dt);
    }

    tickEnemyWaves(dt);

    refreshEnemyChaseTargets();
    retargetEnemiesFromDeadPlayers();

    for (auto &enemy : enemies)
        enemy.tick(dt, true, false);

    tickEnemyPlayerContactDamage(dt);
    removeDeadEnemies();

    if (localPlayerIdParam >= 0)
    {
        Character *local = getPlayerCharacter(localPlayerIdParam);
        if (local && !local->getAlive())
            rebuildSpectateTarget();
    }
}

void GameSimulation::rebuildSpectateTarget()
{
    std::vector<int> alive;
    for (int i = 0; i < (int)players.size(); i++)
    {
        if (players[i].slot.connected && players[i].slot.inMatch && players[i].character.getAlive())
            alive.push_back(i);
    }

    if (alive.empty())
    {
        spectateTargetId = -1;
        return;
    }

    bool currentValid = false;
    for (int id : alive)
    {
        if (id == spectateTargetId)
        {
            currentValid = true;
            break;
        }
    }

    if (!currentValid)
        spectateTargetId = alive[0];
}

void GameSimulation::spectateNext()
{
    std::vector<int> alive;
    for (int i = 0; i < (int)players.size(); i++)
    {
        if (players[i].slot.connected && players[i].slot.inMatch && players[i].character.getAlive())
            alive.push_back(i);
    }
    if (alive.size() < 2)
        return;

    for (size_t i = 0; i < alive.size(); i++)
    {
        if (alive[i] == spectateTargetId)
        {
            spectateTargetId = alive[(i + 1) % alive.size()];
            return;
        }
    }
    spectateTargetId = alive[0];
}

void GameSimulation::spectatePrev()
{
    std::vector<int> alive;
    for (int i = 0; i < (int)players.size(); i++)
    {
        if (players[i].slot.connected && players[i].slot.inMatch && players[i].character.getAlive())
            alive.push_back(i);
    }
    if (alive.size() < 2)
        return;

    for (size_t i = 0; i < alive.size(); i++)
    {
        if (alive[i] == spectateTargetId)
        {
            spectateTargetId = alive[(i + alive.size() - 1) % alive.size()];
            return;
        }
    }
    spectateTargetId = alive[0];
}

Character &GameSimulation::getCameraCharacter()
{
    if (phase == MatchPhase::IN_PROGRESS && localPlayerId >= 0)
    {
        Character *local = getPlayerCharacter(localPlayerId);
        if (local && local->getAlive())
            return *local;

        Character *spectated = getPlayerCharacter(spectateTargetId);
        if (spectated && spectated->getAlive())
            return *spectated;
    }

    if (localPlayerId >= 0)
    {
        Character *local = getPlayerCharacter(localPlayerId);
        if (local)
            return *local;
    }

    static Character fallback{GameConfig::kGameWidth, GameConfig::kGameHeight};
    return fallback;
}

const Character &GameSimulation::getCameraCharacter() const
{
    return const_cast<GameSimulation *>(this)->getCameraCharacter();
}

int GameSimulation::countAlivePlayers() const
{
    int count = 0;
    for (const auto &p : players)
    {
        if (p.slot.connected && p.slot.inMatch && p.character.getAlive())
            count++;
    }
    return count;
}

void GameSimulation::getWinners(std::vector<int> &outPlayerIds) const
{
    outPlayerIds.clear();
    uint16_t bestKills = 0;
    for (const auto &p : players)
    {
        if (!p.slot.connected || !p.slot.inMatch)
            continue;
        if (p.slot.kills > bestKills)
            bestKills = p.slot.kills;
    }

    for (int i = 0; i < (int)players.size(); i++)
    {
        if (players[i].slot.connected && players[i].slot.inMatch && players[i].slot.kills == bestKills)
            outPlayerIds.push_back(i);
    }
}

bool GameSimulation::isThunderActive() const
{
    return !strikes.empty();
}

bool GameSimulation::isThunderActiveForPlayer(int playerId) const
{
    if (playerId < 0 || playerId >= GameConfig::kMaxPlayers)
        return false;
    return playerThunderCasting[playerId];
}

void GameSimulation::resetWorldStateSync()
{
    lastWorldStateTick = 0;
    snapshotBuffer.clear();
    latestEnemyServerPos.clear();
    hasSnapshotTime = false;
    hasSnapshotArrivalTime = false;
    awaitingSpawnSync = false;
}

void GameSimulation::beginMatchPrewarm()
{
    if (!onlineClientMode)
        return;

    lastWorldStateTick = 0;
    snapshotBuffer.clear();
    latestEnemyServerPos.clear();
    hasSnapshotTime = false;
    awaitingSpawnSync = true;
}

bool GameSimulation::isMatchSyncReady() const
{
    if (!onlineClientMode)
        return true;
    return snapshotBuffer.size() >= GameConfig::kMinSnapshotsForPlay;
}

std::size_t GameSimulation::getMatchSyncBufferCount() const
{
    return snapshotBuffer.size();
}

float GameSimulation::noteSnapshotArrival(uint32_t serverTick)
{
    const auto now = std::chrono::steady_clock::now();
    float interArrivalMs = -1.f;
    if (hasSnapshotArrivalTime)
    {
        interArrivalMs = std::chrono::duration<float, std::milli>(now - lastSnapshotArrivalTime).count();
    }

    lastSnapshotArrivalTime = now;
    hasSnapshotArrivalTime = true;
    return interArrivalMs;
}

void GameSimulation::pushSnapshotBuffer(
    uint32_t serverTick,
    const std::vector<NetPlayerSnapshot> &playerSnaps,
    const std::vector<NetEnemySnapshot> &enemySnaps)
{
    BufferedSnapshot frame{};
    frame.receiveTime = std::chrono::steady_clock::now();
    frame.serverTick = serverTick;

    for (const auto &snap : playerSnaps)
    {
        if (snap.id >= GameConfig::kMaxPlayers)
            continue;
        frame.playerPos[snap.id] = {snap.worldX, snap.worldY};
    }

    for (const auto &snap : enemySnaps)
    {
        if (!snap.alive)
            continue;
        frame.enemyPos[snap.id] = {snap.worldX, snap.worldY};
    }

    snapshotBuffer.push_back(std::move(frame));
    while (snapshotBuffer.size() > GameConfig::kInterpBufferMaxSnapshots)
        snapshotBuffer.pop_front();
}

void GameSimulation::pruneSnapshotBuffer(std::chrono::steady_clock::time_point displayTime)
{
    const float keepBehind = GameConfig::kInterpBufferDelaySec +
                             (2.f / GameConfig::kServerTickRate);
    const auto pruneBefore = displayTime - std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                                              std::chrono::duration<float>(keepBehind));

    while (snapshotBuffer.size() > 2 &&
           snapshotBuffer.front().receiveTime < pruneBefore)
    {
        snapshotBuffer.pop_front();
    }

    while (snapshotBuffer.size() > GameConfig::kInterpBufferMaxSnapshots)
        snapshotBuffer.pop_front();
}

void GameSimulation::predictLocalPlayerMovement(int localPlayerId, float dt, const PlayerInput &input)
{
    if (!onlineClientMode || phase != MatchPhase::IN_PROGRESS)
        return;

    Character *local = getPlayerCharacter(localPlayerId);
    if (!local || !local->getAlive())
        return;

    resolvePlayerMovement(*local, dt, input, false);
}

void GameSimulation::predictLocalThunderCast(int localPlayerId, const PlayerInput &input)
{
    if (!onlineClientMode || phase != MatchPhase::IN_PROGRESS)
        return;
    if (localPlayerId < 0 || localPlayerId >= GameConfig::kMaxPlayers)
        return;
    if (!input.thunderPressed)
        return;

    Character *local = getPlayerCharacter(localPlayerId);
    if (!local || !local->getAlive() || !local->isCharged())
        return;
    if (playerThunderCasting[localPlayerId] || enemies.empty())
        return;

    for (const auto &strike : strikes)
    {
        if (strike.isClientPredicted()
            && strike.getOwnerPlayerId() == static_cast<uint8_t>(localPlayerId)
            && !strike.isFinished())
            return;
    }

    Vector2 playerCenter = playerWorldCenter(*local);
    Enemy *nearest = nullptr;
    float bestDist = 0.f;
    for (auto &enemy : enemies)
    {
        if (!enemy.getAlive())
            continue;
        float d = Vector2DistanceSqr(playerCenter, enemy.getWorldCenter());
        if (!nearest || d < bestDist)
        {
            nearest = &enemy;
            bestDist = d;
        }
    }
    if (!nearest)
        return;

    strikes.emplace_back(nearest->getWorldCenter(), thunderTexture, thunderSound);
    strikes.back().setOwnerPlayerId(static_cast<uint8_t>(localPlayerId));
    strikes.back().setClientPredicted(true);
    strikes.back().playImpactSound();
    playerThunderCasting[localPlayerId] = true;
    local->consumeCharge();
}

void GameSimulation::reconcileLocalPlayerPosition(Character &player, Vector2 serverPos)
{
    Vector2 predicted = player.getWorldPos();
    float err = Vector2Distance(predicted, serverPos);
    if (err > GameConfig::kReconcileHardDist)
    {
#if !defined(PLATFORM_WEB)
        ClientDiagLog("reconcile hard snap err=%.1fpx", err);
#endif
        player.setWorldPos(serverPos);
    }
    else if (err > GameConfig::kReconcileSoftDist)
    {
#if !defined(PLATFORM_WEB)
        ClientDiagLog("reconcile soft blend err=%.1fpx", err);
#endif
        player.setWorldPos(Vector2Lerp(predicted, serverPos, 0.35f));
    }
}

Vector2 GameSimulation::lerpPos(Vector2 from, Vector2 to, float t)
{
    EntityInterp interp{};
    interp.from = from;
    interp.to = to;
    return interpolateEntityPos(interp, t);
}

Vector2 GameSimulation::interpolateEntityPos(const EntityInterp &interp, float t)
{
    Vector2 delta = Vector2Subtract(interp.to, interp.from);
    if (t <= 1.f)
        return Vector2Add(interp.from, Vector2Scale(delta, t));

    float extrap = t - 1.f;
    const float snapshotInterval = 1.f / GameConfig::kServerTickRate;
    const float maxExtrap = GameConfig::kInterpExtrapolateSec / snapshotInterval;
    if (extrap > maxExtrap)
        extrap = maxExtrap;
    return Vector2Add(interp.to, Vector2Scale(delta, extrap));
}

void GameSimulation::updateFacingFromMotion(BaseCharacter &entity, Vector2 motion)
{
    if (motion.x < -0.25f)
        entity.setFacingDirection(-1.f);
    else if (motion.x > 0.25f)
        entity.setFacingDirection(1.f);
}

void GameSimulation::spectateForPlayer(int playerId, int direction)
{
    int savedLocal = localPlayerId;
    localPlayerId = playerId;
    if (direction >= 0)
        spectateNext();
    else
        spectatePrev();
    localPlayerId = savedLocal;
}

void GameSimulation::tickResultsCountdownClient(float dt)
{
    if (!onlineClientMode || phase != MatchPhase::RESULTS)
        return;

    resultsTimeRemaining -= dt;
    if (resultsTimeRemaining < 0.f)
        resultsTimeRemaining = 0.f;
}

void GameSimulation::tickInterpolation()
{
    if (!onlineClientMode || snapshotBuffer.size() < 1)
        return;

    const auto now = std::chrono::steady_clock::now();
    const auto displayTime = now - std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                                        std::chrono::duration<float>(GameConfig::kInterpBufferDelaySec));
    pruneSnapshotBuffer(displayTime);

    const BufferedSnapshot *fromSnap = nullptr;
    const BufferedSnapshot *toSnap = nullptr;

    for (size_t i = 0; i + 1 < snapshotBuffer.size(); i++)
    {
        if (snapshotBuffer[i].receiveTime <= displayTime &&
            snapshotBuffer[i + 1].receiveTime >= displayTime)
        {
            fromSnap = &snapshotBuffer[i];
            toSnap = &snapshotBuffer[i + 1];
            break;
        }
    }

    if (!fromSnap)
    {
        if (snapshotBuffer.size() >= 2)
        {
            fromSnap = &snapshotBuffer[snapshotBuffer.size() - 2];
            toSnap = &snapshotBuffer.back();
        }
        else
        {
            fromSnap = &snapshotBuffer.front();
            toSnap = fromSnap;
        }
    }
    else if (!toSnap)
    {
        toSnap = fromSnap;
    }

    float t = 0.f;
    if (fromSnap != toSnap)
    {
        const float span = std::chrono::duration<float>(toSnap->receiveTime - fromSnap->receiveTime).count();
        if (span > 0.f)
        {
            const float elapsed = std::chrono::duration<float>(displayTime - fromSnap->receiveTime).count();
            t = elapsed / span;
        }
    }

    for (const auto &entry : players)
    {
        if (!entry.slot.connected)
            continue;

        const int id = entry.slot.id;
        if (id == localPlayerId)
            continue;

        auto fromIt = fromSnap->playerPos.find(static_cast<uint8_t>(id));
        auto toIt = toSnap->playerPos.find(static_cast<uint8_t>(id));
        if (fromIt == fromSnap->playerPos.end() && toIt == toSnap->playerPos.end())
            continue;

        Vector2 fromPos = fromIt != fromSnap->playerPos.end() ? fromIt->second : toIt->second;
        Vector2 toPos = toIt != toSnap->playerPos.end() ? toIt->second : fromPos;
        Vector2 pos = lerpPos(fromPos, toPos, t);
        players[id].character.setWorldPosInterpolated(pos);
        updateFacingFromMotion(players[id].character, Vector2Subtract(toPos, fromPos));
    }

    for (auto &enemy : enemies)
    {
        const uint16_t id = enemy.getId();
        auto fromIt = fromSnap->enemyPos.find(id);
        auto toIt = toSnap->enemyPos.find(id);
        if (fromIt == fromSnap->enemyPos.end() && toIt == toSnap->enemyPos.end())
            continue;

        Vector2 fromPos = fromIt != fromSnap->enemyPos.end() ? fromIt->second : toIt->second;
        Vector2 toPos = toIt != toSnap->enemyPos.end() ? toIt->second : fromPos;
        Vector2 pos = lerpPos(fromPos, toPos, t);

        if (localPlayerId >= 0)
        {
            const Character *local = getPlayerCharacter(localPlayerId);
            const bool targetsLocal =
                enemy.getTargetPlayerId() == static_cast<uint8_t>(localPlayerId);
            if (local && local->getAlive() && targetsLocal)
            {
                auto latestIt = latestEnemyServerPos.find(id);
                if (latestIt != latestEnemyServerPos.end())
                    pos = latestIt->second;
            }
            else if (local && local->getAlive())
            {
                auto latestIt = latestEnemyServerPos.find(id);
                if (latestIt != latestEnemyServerPos.end())
                {
                    const float nearDist = Vector2Distance(latestIt->second, local->getWorldCenter());
                    if (nearDist < GameConfig::kEnemyNearSnapDist)
                        pos = latestIt->second;
                }
            }
        }

        enemy.setWorldPosInterpolated(pos);
        updateFacingFromMotion(enemy, Vector2Subtract(toPos, fromPos));
    }
}

void GameSimulation::refreshEnemyRenderAnchors()
{
    updateEnemyCameraAnchors();
}

void GameSimulation::refreshPlayerRenderAnchors()
{
    const Character &camera = getCameraCharacter();
    for (auto &entry : players)
    {
        if (!entry.slot.connected || !entry.slot.inMatch)
            continue;
        entry.character.setCameraAnchor(&camera);
    }
}

void GameSimulation::buildWorldStatePacket(std::vector<uint8_t> &out) const
{
    NetSWorldStateHeader header{};
    NetSerialize::initHeader(header.hdr, NetMsgType::S_WORLD_STATE);
    header.tick = ++worldStateTick;
    header.phase = static_cast<uint8_t>(phase);
    header.countdownRemaining = countdownRemaining;
    header.matchTimeRemaining = matchTimeRemaining;
    header.resultsTimeRemaining = resultsTimeRemaining;
    header.mapSeed = mapSeed;
    header.enemyCount = static_cast<uint16_t>(enemies.size());
    header.pickupCount = static_cast<uint8_t>(pickups.size());

    std::vector<NetPlayerSnapshot> playerSnaps;
    for (const auto &entry : players)
    {
        if (!entry.slot.connected)
            continue;

        NetPlayerSnapshot snap{};
        snap.id = entry.slot.id;
        snap.colorIndex = entry.slot.colorIndex;
        snap.kills = entry.slot.kills;
        std::strncpy(snap.name, entry.slot.name, sizeof(snap.name) - 1);

        uint8_t flags = 0;
        if (entry.slot.inQueue) flags |= NET_PLAYER_IN_QUEUE;
        if (entry.slot.inMatch) flags |= NET_PLAYER_IN_MATCH;
        if (entry.character.getAlive()) flags |= NET_PLAYER_ALIVE;
        if (entry.slot.ready) flags |= NET_PLAYER_READY;
        snap.flags = flags;

        snap.worldX = entry.character.getWorldPos().x;
        snap.worldY = entry.character.getWorldPos().y;
        snap.health = entry.character.getHealth();
        snap.charged = entry.character.isCharged() ? 1 : 0;

        const PlayerInput &lastInput = lastRemoteInputs[entry.slot.id];
        snap.attackHeld = lastInput.attackHeld ? 1 : 0;
        snap.moving = Vector2LengthSqr(lastInput.move) > 0.0001f ? 1 : 0;
        if (lastInput.move.x < -0.01f)
            snap.facing = -1;
        else if (lastInput.move.x > 0.01f)
            snap.facing = 1;
        else if (entry.character.getFacingDirection() < 0.f)
            snap.facing = -1;
        else
            snap.facing = 1;

        playerSnaps.push_back(snap);
    }

    header.playerCount = static_cast<uint8_t>(playerSnaps.size());

    std::vector<NetEnemySnapshot> enemySnaps;
    enemySnaps.reserve(enemies.size());
    for (const auto &enemy : enemies)
    {
        NetEnemySnapshot snap{};
        snap.id = enemy.getId();
        snap.worldX = enemy.getWorldPos().x;
        snap.worldY = enemy.getWorldPos().y;
        snap.targetPlayerId = enemy.getTargetPlayerId();
        snap.alive = enemy.getAlive() ? 1 : 0;
        snap.enemyType = enemy.getEnemyType();
        snap.frame = static_cast<uint8_t>(enemy.getAnimationFrame());
        snap.moving = enemy.getNetworkMoving() ? 1 : 0;
        snap.facing = enemy.getFacingDirection() < 0.f ? -1 : 1;
        enemySnaps.push_back(snap);
    }

    std::vector<NetPickupSnapshot> pickupSnaps;
    pickupSnaps.reserve(pickups.size());
    for (const auto &pickup : pickups)
    {
        NetPickupSnapshot snap{};
        Vector2 center = pickup.getWorldCenter();
        snap.worldX = center.x;
        snap.worldY = center.y;
        snap.active = pickup.isActive() ? 1 : 0;
        pickupSnaps.push_back(snap);
    }

    std::vector<NetStrikeSnapshot> strikeSnaps;
    strikeSnaps.reserve(strikes.size());
    for (const auto &strike : strikes)
    {
        if (strike.isFinished())
            continue;

        NetStrikeSnapshot snap{};
        Vector2 center = strike.getTargetCenter();
        snap.worldX = center.x;
        snap.worldY = center.y;
        snap.frame = static_cast<uint8_t>(strike.getFrame());
        snap.ownerPlayerId = strike.getOwnerPlayerId();
        strikeSnaps.push_back(snap);
    }
    header.strikeCount = static_cast<uint8_t>(strikeSnaps.size());

    NetSerialize::buildWorldState(
        header,
        playerSnaps.empty() ? nullptr : playerSnaps.data(),
        enemySnaps.empty() ? nullptr : enemySnaps.data(),
        pickupSnaps.empty() ? nullptr : pickupSnaps.data(),
        strikeSnaps.empty() ? nullptr : strikeSnaps.data(),
        out);
}

void GameSimulation::buildLobbySyncPacket(std::vector<uint8_t> &out) const
{
    NetSLobbySyncHeader header{};
    NetSerialize::initHeader(header.hdr, NetMsgType::S_LOBBY_SYNC);
    header.phase = static_cast<uint8_t>(phase);
    header.countdownRemaining = countdownRemaining;

    std::vector<NetLobbyPlayerEntry> entries;
    for (const auto &entry : players)
    {
        if (!entry.slot.connected)
            continue;

        NetLobbyPlayerEntry snap{};
        snap.id = entry.slot.id;
        snap.colorIndex = entry.slot.colorIndex;
        std::strncpy(snap.name, entry.slot.name, sizeof(snap.name) - 1);

        uint8_t flags = 0;
        if (entry.slot.inQueue) flags |= NET_PLAYER_IN_QUEUE;
        if (entry.slot.inMatch) flags |= NET_PLAYER_IN_MATCH;
        if (entry.slot.ready) flags |= NET_PLAYER_READY;
        snap.flags = flags;
        entries.push_back(snap);
    }

    header.playerCount = static_cast<uint8_t>(entries.size());
    NetSerialize::buildLobbySync(
        header,
        entries.empty() ? nullptr : entries.data(),
        out);
}

bool GameSimulation::applyLobbySyncPacket(const uint8_t *data, size_t size)
{
    NetSLobbySyncHeader header{};
    std::vector<NetLobbyPlayerEntry> entries;
    if (!NetSerialize::parseLobbySync(data, size, header, entries))
        return false;

    const MatchPhase prevPhase = phase;
    phase = static_cast<MatchPhase>(header.phase);
    countdownRemaining = header.countdownRemaining;

    if (onlineClientMode && prevPhase == MatchPhase::QUEUE && phase == MatchPhase::COUNTDOWN)
        beginMatchPrewarm();

    for (const auto &snap : entries)
    {
        if (snap.id >= GameConfig::kMaxPlayers)
            continue;

        while ((int)players.size() <= snap.id)
            players.emplace_back();

        PlayerEntry &entry = players[snap.id];
        entry.slot.id = snap.id;
        entry.slot.colorIndex = snap.colorIndex;
        PlayerSlotSetName(entry.slot, snap.name);
        entry.slot.inQueue = (snap.flags & NET_PLAYER_IN_QUEUE) != 0;
        entry.slot.inMatch = (snap.flags & NET_PLAYER_IN_MATCH) != 0;
        entry.slot.ready = (snap.flags & NET_PLAYER_READY) != 0;
        entry.slot.connected = true;
        entry.character.setPlayerColor(kPlayerColors[entry.slot.colorIndex]);
    }

    for (int i = 0; i < (int)players.size(); i++)
    {
        bool inSnapshot = false;
        for (const auto &snap : entries)
        {
            if (snap.id == static_cast<uint8_t>(i))
            {
                inSnapshot = true;
                break;
            }
        }
        if (!inSnapshot)
            markPlayerDisconnected(i);
    }
    return true;
}

void GameSimulation::applyJoinAck(int playerId, uint8_t colorIndex, const char *name)
{
    if (playerId < 0 || playerId >= GameConfig::kMaxPlayers)
        return;

    localPlayerId = playerId;
    while ((int)players.size() <= playerId)
        players.emplace_back();

    PlayerEntry &entry = players[playerId];
    if (name)
        PlayerSlotSetName(entry.slot, name);
    entry.slot.id = static_cast<uint8_t>(playerId);
    entry.slot.colorIndex = colorIndex;
    entry.slot.connected = true;
    entry.slot.inQueue = true;
    entry.slot.inMatch = false;
    entry.slot.ready = false;
    entry.character.setPlayerColor(kPlayerColors[colorIndex]);

    if (phase == MatchPhase::IDLE)
        phase = MatchPhase::QUEUE;
}

bool GameSimulation::applyWorldStatePacket(const uint8_t *data, size_t size)
{
    NetSWorldStateHeader header{};
    std::vector<NetPlayerSnapshot> playerSnaps;
    std::vector<NetEnemySnapshot> enemySnaps;
    std::vector<NetPickupSnapshot> pickupSnaps;
    std::vector<NetStrikeSnapshot> strikeSnaps;

    if (!NetSerialize::parseWorldState(data, size, header, playerSnaps, enemySnaps, pickupSnaps, strikeSnaps))
        return false;

    if (header.tick != 0 && header.tick <= lastWorldStateTick)
    {
        if (header.tick < lastWorldStateTick / 2u)
            lastWorldStateTick = 0;
        else
            return false;
    }
    lastWorldStateTick = header.tick;

    if (header.mapSeed != 0 && header.mapSeed != mapSeed)
    {
        init(header.mapSeed);
#if !defined(PLATFORM_WEB)
        ClientDiagLog("map loaded seed=%u", header.mapSeed);
#endif
    }

    const MatchPhase prevPhase = phase;
    phase = static_cast<MatchPhase>(header.phase);
    countdownRemaining = header.countdownRemaining;
    matchTimeRemaining = header.matchTimeRemaining;
    resultsTimeRemaining = header.resultsTimeRemaining;

    if (onlineClientMode && prevPhase == MatchPhase::QUEUE && phase == MatchPhase::COUNTDOWN)
        beginMatchPrewarm();

    for (const auto &snap : playerSnaps)
    {
        if (snap.id >= GameConfig::kMaxPlayers)
            continue;

        while ((int)players.size() <= snap.id)
            players.emplace_back();

        PlayerEntry &entry = players[snap.id];
        Vector2 serverPos{snap.worldX, snap.worldY};
        const bool isLocal = (static_cast<int>(snap.id) == localPlayerId);

        if (isLocal)
        {
            if (awaitingSpawnSync && entry.slot.inMatch)
            {
                entry.character.setWorldPos(serverPos);
                awaitingSpawnSync = false;
#if !defined(PLATFORM_WEB)
                ClientDiagLog("spawn snap pos=(%.0f,%.0f)", serverPos.x, serverPos.y);
#endif
            }
            else
            {
                reconcileLocalPlayerPosition(entry.character, serverPos);
            }
        }

        entry.slot.id = snap.id;
        entry.slot.colorIndex = snap.colorIndex;
        entry.slot.kills = snap.kills;
        PlayerSlotSetName(entry.slot, snap.name);
        entry.slot.inQueue = (snap.flags & NET_PLAYER_IN_QUEUE) != 0;
        entry.slot.inMatch = (snap.flags & NET_PLAYER_IN_MATCH) != 0;
        entry.slot.ready = (snap.flags & NET_PLAYER_READY) != 0;
        entry.slot.connected = true;
        entry.character.setPlayerColor(kPlayerColors[snap.colorIndex]);
        entry.character.setAlive((snap.flags & NET_PLAYER_ALIVE) != 0);
        const float prevHealth = entry.character.getHealth();
        entry.character.setHealth(snap.health);
        if (isLocal && snap.health < prevHealth - 0.5f)
            entry.character.playHitFeedback();
        else if (isLocal && snap.health > prevHealth + 0.5f)
            entry.character.playHealFeedback();
        entry.character.setCharged(snap.charged != 0);
        entry.character.setNetworkVisualState(snap.moving != 0, snap.attackHeld != 0);
        if (!isLocal)
            entry.character.setNetworkFacing(static_cast<float>(snap.facing));
    }

    for (int i = 0; i < (int)players.size(); i++)
    {
        bool inSnapshot = false;
        for (const auto &snap : playerSnaps)
        {
            if (snap.id == static_cast<uint8_t>(i))
            {
                inSnapshot = true;
                break;
            }
        }
        if (!inSnapshot)
            markPlayerDisconnected(i);
    }

    std::vector<Enemy> nextEnemies;
    nextEnemies.reserve(enemySnaps.size());

    for (const auto &snap : enemySnaps)
    {
        if (!snap.alive)
            continue;

        Vector2 pos{snap.worldX, snap.worldY};
        latestEnemyServerPos[snap.id] = pos;

        Texture2D idleTex = snap.enemyType == 0 ? goblinIdle : slimeIdle;
        Texture2D runTex = snap.enemyType == 0 ? goblinRun : slimeRun;

        nextEnemies.emplace_back(pos, idleTex, runTex);
        Enemy &enemy = nextEnemies.back();
        enemy.setId(snap.id);
        enemy.setEnemyType(snap.enemyType);
        enemy.setTargetPlayerId(snap.targetPlayerId);
        enemy.setChaseTarget(getPlayerCharacter(snap.targetPlayerId));
        enemy.setObstacles(&props);
        enemy.setCameraAnchor(&getCameraCharacter());
        enemy.setAnimationFrame(snap.frame);
        enemy.setNetworkMoving(snap.moving != 0);
        enemy.setNetworkFacing(static_cast<float>(snap.facing));
    }

    enemies = std::move(nextEnemies);

    pickups.clear();
    for (const auto &snap : pickupSnaps)
    {
        if (!snap.active)
            continue;
        Vector2 pos{
            snap.worldX - healthPotionSrc.width * Pickup::kScale / 2.f,
            snap.worldY - healthPotionSrc.height * Pickup::kScale / 2.f};
        pickups.emplace_back(pos, pickupSheet, healthPotionSrc, PickupType::HEALTH);
    }

    std::vector<Thunderstrike> keptPredicted;
    keptPredicted.reserve(strikes.size());
    for (const auto &strike : strikes)
    {
        if (!strike.isClientPredicted() || strike.isFinished())
            continue;
        if (strike.getOwnerPlayerId() != static_cast<uint8_t>(localPlayerId))
            continue;
        keptPredicted.push_back(strike);
    }

    strikes.clear();
    std::memset(playerThunderCasting, 0, sizeof(playerThunderCasting));
    for (const auto &snap : strikeSnaps)
    {
        Vector2 center{snap.worldX, snap.worldY};
        strikes.emplace_back(center, thunderTexture, thunderSound);
        strikes.back().setFrame(snap.frame);
        strikes.back().setOwnerPlayerId(snap.ownerPlayerId);
        if (snap.ownerPlayerId < GameConfig::kMaxPlayers)
            playerThunderCasting[snap.ownerPlayerId] = true;
    }

    for (const auto &predicted : keptPredicted)
    {
        uint8_t ownerId = predicted.getOwnerPlayerId();
        bool onServer = false;
        for (const auto &strike : strikes)
        {
            if (strike.getOwnerPlayerId() == ownerId)
            {
                onServer = true;
                break;
            }
        }
        if (!onServer)
        {
            strikes.push_back(predicted);
            if (ownerId < GameConfig::kMaxPlayers)
                playerThunderCasting[ownerId] = true;
        }
    }

    pushSnapshotBuffer(header.tick, playerSnaps, enemySnaps);
    lastSnapshotTime = std::chrono::steady_clock::now();
    hasSnapshotTime = true;
    refreshEnemyRenderAnchors();

    if (localPlayerId >= 0)
    {
        Character *local = getPlayerCharacter(localPlayerId);
        if (local && !local->getAlive())
            rebuildSpectateTarget();
    }
    return true;
}
