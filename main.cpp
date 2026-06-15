#include "raylib.h"
#include "raymath.h"
#include "Character.h"
#include "GameConfig.h"
#include "GameSimulation.h"
#include "PlayerSlot.h"
#include "TouchControls.h"
#include "NameInputBridge.h"
#ifndef PLATFORM_WEB
#include "NetSession.h"
#include "NetCommon.h"
#else
#include "NetWebSession.h"
#include "NetCommon.h"
#endif
#include <string>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <cctype>
#include <cstring>
#include <cmath>

#include "ClientDiagLog.h"

#define ClientNetLog ClientDiagLog

enum class GameState
{
    TITLE,
    QUEUE,
    PLAYING,
    SPECTATING,
    MATCH_END,
    PAUSED
};

#ifndef PLATFORM_WEB
static void ParseClientArgs(
    int argc, char **argv, bool &offlineMode, std::string &serverHost,
    uint16_t &serverPort, NetTransport &transport)
{
    offlineMode = false;
    serverHost = GameConfig::kDefaultServerHost;
    serverPort = GameConfig::kDefaultServerPort;
    transport = NetTransport::Enet;

    for (int i = 1; i < argc; i++)
    {
        if (std::strcmp(argv[i], "--offline") == 0)
            offlineMode = true;
        else if (std::strcmp(argv[i], "--host") == 0 && i + 1 < argc)
        {
            serverHost = argv[i + 1];
            i++;
        }
        else if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc)
        {
            serverPort = static_cast<uint16_t>(std::atoi(argv[i + 1]));
            i++;
        }
        else if (std::strcmp(argv[i], "--transport") == 0 && i + 1 < argc)
        {
            if (std::strcmp(argv[i + 1], "tcp") == 0)
                transport = NetTransport::Stream;
            else
                transport = NetTransport::Enet;
            i++;
        }
    }
}
#endif

static void FinishMatchReturnToTitle(
    GameState &state, GameSimulation &sim, std::string &statusMessage, std::string &playerName)
{
    if (sim.getPhase() != MatchPhase::IDLE)
        sim.returnPlayersToMenu();
    state = GameState::TITLE;
    playerName.clear();
    (void)statusMessage;
}

static void SyncUiStateFromSimulation(
    GameState &state, const GameSimulation &sim, int localPlayerId, bool onlineConnected)
{
    (void)onlineConnected;
    switch (sim.getPhase())
    {
    case MatchPhase::IDLE:
        state = GameState::TITLE;
        break;
    case MatchPhase::QUEUE:
    case MatchPhase::COUNTDOWN:
        state = GameState::QUEUE;
        break;
    case MatchPhase::IN_PROGRESS:
        if (localPlayerId >= 0 && localPlayerId < (int)sim.getPlayers().size())
        {
            const auto &local = sim.getPlayers()[localPlayerId];
            if (local.slot.inMatch && !local.character.getAlive() && sim.countAlivePlayers() > 0)
                state = GameState::SPECTATING;
            else
                state = GameState::PLAYING;
        }
        else
        {
            state = GameState::PLAYING;
        }
        break;
    case MatchPhase::RESULTS:
        state = GameState::MATCH_END;
        break;
    }
}

static const int gameWidth = GameConfig::kGameWidth;
static const int gameHeight = GameConfig::kGameHeight;

static Vector2 gVirtualMouse{};
static bool gMouseInGame{false};

static Rectangle GetGameViewport()
{
    float scale = fminf(
        (float)GetScreenWidth() / gameWidth,
        (float)GetScreenHeight() / gameHeight);
    float destW = gameWidth * scale;
    float destH = gameHeight * scale;
    return Rectangle{
        ((float)GetScreenWidth() - destW) * 0.5f,
        ((float)GetScreenHeight() - destH) * 0.5f,
        destW, destH};
}

static Vector2 WindowToGameMouse(Rectangle viewport)
{
    return Vector2{
        (GetMouseX() - viewport.x) / viewport.width * gameWidth,
        (GetMouseY() - viewport.y) / viewport.height * gameHeight};
}

static void PresentGame(const RenderTexture2D &target)
{
    Rectangle viewport = GetGameViewport();
    BeginDrawing();
    ClearBackground(BLACK);
    Rectangle src{0.f, 0.f, (float)gameWidth, -(float)gameHeight};
    DrawTexturePro(target.texture, src, viewport, Vector2{}, 0.f, WHITE);
    EndDrawing();
}

#if defined(PLATFORM_WEB)
static bool Button(Rectangle bounds, const char *text,
                   bool uiTouchPressed, Vector2 uiTouchGamePos)
#else
static bool Button(Rectangle bounds, const char *text)
#endif
{
    bool mouseHovered = gMouseInGame && CheckCollisionPointRec(gVirtualMouse, bounds);
#if defined(PLATFORM_WEB)
    bool touchHovered = uiTouchPressed && CheckCollisionPointRec(uiTouchGamePos, bounds);
    bool hovered = mouseHovered || touchHovered;
#else
    bool hovered = mouseHovered;
#endif
    DrawRectangleRec(bounds, hovered ? SKYBLUE : DARKBLUE);
    DrawRectangleLinesEx(bounds, 2.f, RAYWHITE);

    const int fontSize{24};
    int textWidth = MeasureText(text, fontSize);
    DrawText(text,
             (int)(bounds.x + (bounds.width - textWidth) / 2.f),
             (int)(bounds.y + (bounds.height - fontSize) / 2.f),
             fontSize, RAYWHITE);

#if defined(PLATFORM_WEB)
    bool mouseClicked = mouseHovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    return mouseClicked || touchHovered;
#else
    return mouseHovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
#endif
}

static void DrawCenteredText(const char *text, int viewWidth, int y, int fontSize, Color color)
{
    int textWidth = MeasureText(text, fontSize);
    DrawText(text, (viewWidth - textWidth) / 2, y, fontSize, color);
}

static void HandlePlayerNameInput(std::string &name, bool mobileKeyboardOpen)
{
#if defined(PLATFORM_WEB)
    if (mobileKeyboardOpen)
    {
        char fromDom[32]{};
        ReadPlayerNameIntoBuffer(fromDom, sizeof(fromDom));
        name = fromDom;
        return;
    }
#endif

    if (IsKeyPressed(KEY_BACKSPACE) && !name.empty())
        name.pop_back();

    int ch = GetCharPressed();
    while (ch > 0)
    {
        if (std::isprint(ch) && ch != ' ')
        {
            if ((int)name.size() < GameConfig::kPlayerNameMaxLen)
                name += (char)ch;
        }
        ch = GetCharPressed();
    }
}

static void DrawNameInput(const Rectangle &bounds, const std::string &name)
{
    DrawRectangleRec(bounds, Fade(RAYWHITE, 0.15f));
    DrawRectangleLinesEx(bounds, 2.f, RAYWHITE);

    const int fontSize{18};
    std::string display = name;
    while ((int)display.size() < GameConfig::kPlayerNameMaxLen)
        display += '_';

    int textWidth = MeasureText(display.c_str(), fontSize);
    DrawText(display.c_str(),
             (int)(bounds.x + (bounds.width - textWidth) / 2.f),
             (int)(bounds.y + (bounds.height - fontSize) / 2.f),
             fontSize, RAYWHITE);
}

#if defined(PLATFORM_WEB)
static bool TouchTapInRect(Rectangle rect, Rectangle viewport, bool &wasTouching)
{
    bool touching = false;
    for (int i = 0; i < GetTouchPointCount(); i++)
    {
        Vector2 screenPos = GetTouchPosition(i);
        if (!CheckCollisionPointRec(screenPos, viewport))
            continue;

        Vector2 gamePos = VirtualTouchControls::MapScreenToGame(
            screenPos, viewport, (float)gameWidth, (float)gameHeight);
        if (CheckCollisionPointRec(gamePos, rect))
            touching = true;
    }

    const bool tapped = touching && !wasTouching;
    wasTouching = touching;
    return tapped;
}

static bool TouchPressInViewport(Rectangle viewport, bool &wasTouching, Vector2 &gamePos)
{
    bool touching = false;
    gamePos = Vector2{};
    for (int i = 0; i < GetTouchPointCount(); i++)
    {
        Vector2 screenPos = GetTouchPosition(i);
        if (!CheckCollisionPointRec(screenPos, viewport))
            continue;

        gamePos = VirtualTouchControls::MapScreenToGame(
            screenPos, viewport, (float)gameWidth, (float)gameHeight);
        touching = true;
        break;
    }

    const bool pressed = touching && !wasTouching;
    wasTouching = touching;
    return pressed;
}

static void SyncPlayerNameFromKeyboard(std::string &name)
{
    char fromDom[32]{};
    ReadPlayerNameIntoBuffer(fromDom, sizeof(fromDom));
    name = fromDom;
}
#endif

static void DrawKillScoreboard(const GameSimulation &sim, int localPlayerId,
                               int panelTop = 80, int titleFontSize = 28,
                               int rowFontSize = 20, int rowHeight = 28)
{
    std::vector<const PlayerEntry *> rows;
    for (const auto &p : sim.getPlayers())
    {
        if (p.slot.connected && p.slot.inMatch)
            rows.push_back(&p);
    }

    const int panelX = panelTop >= 180 ? 70 : 40;
    const int panelW = gameWidth - panelX * 2;
    const int headerBlock = titleFontSize + 36;
    const int panelH = headerBlock + (int)rows.size() * rowHeight + 16;

    DrawRectangle(panelX, panelTop, panelW, panelH, Fade(BLACK, 0.75f));
    DrawRectangleLines(panelX, panelTop, panelW, panelH, RAYWHITE);
    DrawCenteredText("Kill Count", gameWidth, panelTop + 8, titleFontSize, RAYWHITE);

    int y = panelTop + titleFontSize + 22;
    DrawText("Name", panelX + 20, y, rowFontSize, LIGHTGRAY);
    DrawText("Kills", panelX + panelW - 170, y, rowFontSize, LIGHTGRAY);
    DrawText("Status", panelX + panelW - 90, y, rowFontSize, LIGHTGRAY);
    y += rowHeight + 2;

    std::sort(rows.begin(), rows.end(), [](const PlayerEntry *a, const PlayerEntry *b) {
        return a->slot.kills > b->slot.kills;
    });

    for (const auto *entry : rows)
    {
        Color rowColor = (entry->slot.id == localPlayerId) ? SKYBLUE : RAYWHITE;
        DrawRectangle(panelX + 10, y - 2, 18, rowFontSize + 2, kPlayerColors[entry->slot.colorIndex]);
        DrawText(entry->slot.name, panelX + 34, y, rowFontSize, rowColor);
        DrawText(std::to_string(entry->slot.kills).c_str(), panelX + panelW - 170, y, rowFontSize, rowColor);
        DrawText(entry->character.getAlive() ? "Alive" : "Dead", panelX + panelW - 90, y, rowFontSize,
                 entry->character.getAlive() ? GREEN : RED);
        y += rowHeight;
    }
}

static void DrawMatchTimer(const GameSimulation &sim)
{
    int seconds = (int)sim.getMatchTimeRemaining();
    int mins = seconds / 60;
    int secs = seconds % 60;
    char buf[32];
    snprintf(buf, sizeof(buf), "Time: %d:%02d", mins, secs);
    DrawCenteredText(buf, gameWidth, 10, 24, BLACK);
}

static void CollectLocalInput(PlayerInput &input
#if defined(PLATFORM_WEB)
    , VirtualTouchControls &touchControls, Rectangle viewport, bool simulate
#endif
)
{
#if defined(PLATFORM_WEB)
    if (simulate)
    {
        touchControls.update(viewport, (float)gameWidth, (float)gameHeight);
        input.move = touchControls.moveDir;
        input.attackPressed = touchControls.attackPressed;
        input.attackHeld = touchControls.attackHeld;
        input.thunderPressed = touchControls.thunderPressed;
        return;
    }
#endif

    input.move = Vector2{};
    if (IsKeyDown(KEY_A)) input.move.x -= 1.f;
    if (IsKeyDown(KEY_D)) input.move.x += 1.f;
    if (IsKeyDown(KEY_W)) input.move.y -= 1.f;
    if (IsKeyDown(KEY_S)) input.move.y += 1.f;

#if defined(PLATFORM_WEB)
    input.attackPressed = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    input.attackHeld = IsMouseButtonDown(MOUSE_LEFT_BUTTON);
    input.thunderPressed = IsMouseButtonPressed(MOUSE_RIGHT_BUTTON);
#else
    input.attackPressed = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    input.attackHeld = IsMouseButtonDown(MOUSE_LEFT_BUTTON);
    input.thunderPressed = IsMouseButtonPressed(MOUSE_RIGHT_BUTTON);
#endif
}

int main(int argc, char **argv)
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(960, 960, "Top Down Survive Online");
    SetWindowMinSize(gameWidth, gameHeight);
    ChangeDirectory(GetApplicationDirectory());
#if !defined(PLATFORM_WEB)
    ClientDiagLog("=== Top Down Survive Online started (log: %s) ===", ClientDiagLogPath());
#endif

    RenderTexture2D gameTarget = LoadRenderTexture(gameWidth, gameHeight);
    SetTextureFilter(gameTarget.texture, TEXTURE_FILTER_POINT);

    InitAudioDevice();
    Music bgMusic = LoadMusicStream("sfx/bg_music.mp3");
    bgMusic.looping = true;
    PlayMusicStream(bgMusic);
    bool audioMuted{false};

    GameSimulation sim;
    std::string playerName;
    std::string statusMessage;
    bool showScoreboard{false};
    GameState state = GameState::TITLE;

    bool offlineMode{true};
    std::string serverHost;
    uint16_t serverPort{GameConfig::kDefaultServerPort};
    NetTransport transport{NetTransport::Enet};
#if defined(PLATFORM_WEB)
    NetWebSession netClient;
    std::string serverWsUrl = NetWebSession::configuredServerUrl();
    offlineMode = serverWsUrl.empty();
#else
    NetSession netClient;
    ParseClientArgs(argc, argv, offlineMode, serverHost, serverPort, transport);
    if (!offlineMode)
    {
        ClientDiagLog(
            "connect cfg transport=%s host=%s port=%u",
            transport == NetTransport::Stream ? "tcp" : "enet",
            serverHost.c_str(), static_cast<unsigned>(serverPort));
    }
#endif
    sim.setOnlineClientMode(!offlineMode);
    if (offlineMode)
        sim.init(static_cast<unsigned int>(time(nullptr)));

    int lastLoggedQueueCount = -1;
    int lastLoggedPhase = -1;
    int lastLoggedCountdownSec = -1;
    double onlineCountdownEndTime = 0.0;
    bool onlineCountdownActive = false;
    bool joinedServerSession = false;
    bool connectPending = false;
    double connectAttemptStartTime = 0.0;
    constexpr double kJoinResponseTimeoutSec = 12.0;
#ifndef PLATFORM_WEB
    double inputSendAccum = 0.0;
    PlayerInput lastSentInput{};
    bool hasLastSentInput = false;
    const double inputSendInterval = 1.0 / static_cast<double>(GameConfig::kServerTickRate);
#endif

    auto cancelPendingConnect = [&](const char *message)
    {
        connectPending = false;
        joinedServerSession = false;
        if (message && message[0])
            statusMessage = message;
        if (netClient.isConnected())
            netClient.disconnect();
    };

    auto handleNetDisconnect = [&]()
    {
        if (connectPending)
        {
            cancelPendingConnect("Server disconnected before join completed.");
            return;
        }

        if (!joinedServerSession)
            return;

        joinedServerSession = false;
        ClientNetLog("Disconnected from server");
        statusMessage = "Disconnected from server.";
        sim.resetWorldStateSync();
        FinishMatchReturnToTitle(state, sim, statusMessage, playerName);
    };

    netClient.setDisconnectHandler(handleNetDisconnect);
    netClient.setPacketHandler([&](const uint8_t *data, size_t size) {
        if (!data || size < sizeof(NetPacketHeader))
            return;

        auto type = static_cast<NetMsgType>(data[0]);
        switch (type)
        {
        case NetMsgType::S_JOIN_OK:
            if (size >= sizeof(NetSJoinOk))
            {
                NetSJoinOk pkt{};
                std::memcpy(&pkt, data, sizeof(pkt));
                connectPending = false;
                joinedServerSession = true;
                netClient.setAssignedPlayerId(pkt.playerId);
                sim.resetWorldStateSync();
                sim.applyJoinAck(pkt.playerId, pkt.colorIndex, playerName.c_str());
                ClientNetLog("S_JOIN_OK playerId=%d color=%d queue=%d",
                             pkt.playerId, pkt.colorIndex, sim.getQueueCount());
                statusMessage.clear();
                state = GameState::QUEUE;
            }
            break;
        case NetMsgType::S_JOIN_FAIL:
            if (size >= sizeof(NetSJoinFail))
            {
                NetSJoinFail pkt{};
                std::memcpy(&pkt, data, sizeof(pkt));
                ClientNetLog("S_JOIN_FAIL reason=%u", pkt.reason);
                connectPending = false;
                joinedServerSession = false;
                if (pkt.reason == NET_JOIN_SERVER_FULL)
                    statusMessage = "Server is full (10/10). Try again later.";
                else if (pkt.reason == NET_JOIN_MATCH_IN_PROGRESS)
                    statusMessage = "Match in progress. Try again when it ends.";
                else
                    statusMessage = "Could not join (invalid name).";
                sim.returnPlayersToMenu();
                sim.resetWorldStateSync();
                state = GameState::TITLE;
                netClient.disconnect();
            }
            break;
        case NetMsgType::S_WORLD_STATE:
        {
            if (!joinedServerSession)
                break;

            NetSWorldStateHeader hdr{};
            if (size >= sizeof(hdr))
                std::memcpy(&hdr, data, sizeof(hdr));
            const bool applied = sim.applyWorldStatePacket(data, size);
            if (applied)
            {
                const float interArrivalMs = sim.noteSnapshotArrival(hdr.tick);
                const float expectedMs = 1000.f / GameConfig::kServerTickRate;
                if (interArrivalMs >= 0.f)
                {
                    ClientNetLog(
                        "S_WORLD_STATE tick=%u phase=%d players=%d interArrival=%.1fms jitter=%.1fms",
                        hdr.tick, hdr.phase, hdr.playerCount, interArrivalMs,
                        std::fabs(interArrivalMs - expectedMs));
                }
                else
                {
                    ClientNetLog(
                        "S_WORLD_STATE tick=%u phase=%d players=%d (first snapshot)",
                        hdr.tick, hdr.phase, hdr.playerCount);
                }
            }
            else
            {
                ClientNetLog("S_WORLD_STATE skipped tick=%u (stale/parse fail)", hdr.tick);
            }
            SyncUiStateFromSimulation(state, sim, sim.getLocalPlayerId(), netClient.isConnected());
            if (applied && sim.isResultsCountdownFinished())
            {
                if (!offlineMode && netClient.isConnected())
                {
                    statusMessage = "Match over. Thanks for playing!";
                    joinedServerSession = false;
                    netClient.disconnect();
                }
                FinishMatchReturnToTitle(state, sim, statusMessage, playerName);
                sim.resetWorldStateSync();
            }
            break;
        }
        case NetMsgType::S_LOBBY_SYNC:
        {
            if (!joinedServerSession)
                break;

            const bool applied = sim.applyLobbySyncPacket(data, size);
            const int phase = static_cast<int>(sim.getPhase());
            const int queue = sim.getQueueCount();
            if (applied)
            {
                if (phase == static_cast<int>(MatchPhase::COUNTDOWN))
                {
                    onlineCountdownEndTime = GetTime() + sim.getCountdownRemaining();
                    onlineCountdownActive = true;
                    const int sec = static_cast<int>(sim.getCountdownRemaining() + 0.999f);
                    if (sec != lastLoggedCountdownSec)
                    {
                        ClientNetLog("S_LOBBY_SYNC countdown=%d queue=%d", sec, queue);
                        lastLoggedCountdownSec = sec;
                    }
                }
                else
                {
                    onlineCountdownActive = false;
                    if (queue != lastLoggedQueueCount || phase != lastLoggedPhase)
                    {
                        ClientNetLog("S_LOBBY_SYNC phase=%d queue=%d", phase, queue);
                        lastLoggedQueueCount = queue;
                        lastLoggedPhase = phase;
                        lastLoggedCountdownSec = -1;
                    }
                }
            }
            else
            {
                ClientNetLog("S_LOBBY_SYNC parse fail size=%zu", size);
            }
            SyncUiStateFromSimulation(state, sim, sim.getLocalPlayerId(), netClient.isConnected());
            break;
        }
        case NetMsgType::S_KICK:
            if (size >= sizeof(NetSKick))
            {
                NetSKick pkt{};
                std::memcpy(&pkt, data, sizeof(pkt));
                ClientNetLog("S_KICK: %s", pkt.message);
                statusMessage = pkt.message[0] ? pkt.message : "Disconnected from server.";
            }
            else
            {
                ClientNetLog("S_KICK disconnected");
                statusMessage = "Disconnected from server.";
            }
            joinedServerSession = false;
            sim.resetWorldStateSync();
            FinishMatchReturnToTitle(state, sim, statusMessage, playerName);
            netClient.disconnect();
            break;
        default:
            ClientNetLog("unknown pkt type=%u size=%zu", static_cast<unsigned>(data[0]), size);
            break;
        }
    });

#if defined(PLATFORM_WEB)
    bool nameKeyboardOpen{false};
    bool wasTouchOnNameField{false};
    bool wasUiTouching{false};
    VirtualTouchControls touchControls;
    touchControls.init((float)gameWidth, (float)gameHeight);
    (void)argc;
    (void)argv;
#endif

    const Rectangle joinButtonRec{
        gameWidth / 2.f - 110.f, gameHeight / 2.f + 60.f, 220.f, 56.f};
    const Rectangle nameInputRec{
        gameWidth / 2.f - 75.f, gameHeight / 2.f + 5.f, 150.f, 32.f};
    const Rectangle readyButtonRec{
        gameWidth / 2.f - 110.f, gameHeight / 2.f + 40.f, 220.f, 56.f};
    const Rectangle leaveQueueRec{
        gameWidth / 2.f - 110.f, gameHeight / 2.f + 110.f, 220.f, 56.f};
    const Rectangle returnMenuRec{
        gameWidth / 2.f - 110.f, gameHeight / 2.f + 120.f, 220.f, 48.f};
    const Rectangle spectatePrevRec{20.f, gameHeight - 60.f, 100.f, 44.f};
    const Rectangle spectateNextRec{gameWidth - 120.f, gameHeight - 60.f, 100.f, 44.f};
#if defined(PLATFORM_WEB)
    const Rectangle pauseContinueRec{
        gameWidth / 2.f - 110.f, gameHeight / 2.f - 30.f, 220.f, 56.f};
#endif

    const Rectangle muteRec{gameWidth - 100.f, 10.f, 90.f, 36.f};
#if defined(PLATFORM_WEB)
    auto drawMuteButton = [&](bool touchPressed, Vector2 touchPos)
    {
        if (Button(muteRec, audioMuted ? "Unmute" : "Mute", touchPressed, touchPos))
        {
            audioMuted = !audioMuted;
            SetMasterVolume(audioMuted ? 0.f : 1.f);
        }
    };
#else
    auto drawMuteButton = [&]()
    {
        if (Button(muteRec, audioMuted ? "Unmute" : "Mute"))
        {
            audioMuted = !audioMuted;
            SetMasterVolume(audioMuted ? 0.f : 1.f);
        }
    };
#endif

    const float minimapSize{130.f};
    const Rectangle minimapBounds{
        gameWidth - minimapSize - 8.f, 50.f, minimapSize, minimapSize};

    auto drawMinimap = [&]()
    {
        const Character &camera = sim.getCameraCharacter();
        (void)camera;
        sim.getTileMap().drawMinimap(minimapBounds);

        for (const auto &entry : sim.getPlayers())
        {
            if (!entry.slot.connected || !entry.slot.inMatch)
                continue;

            Vector2 center = entry.character.getWorldCenter();
            float px = minimapBounds.x + (center.x / sim.getWorldWidth()) * minimapBounds.width;
            float py = minimapBounds.y + (center.y / sim.getWorldHeight()) * minimapBounds.height;
            Color dotColor = entry.character.getAlive()
                                 ? kPlayerColors[entry.slot.colorIndex]
                                 : GRAY;
            DrawCircle((int)px, (int)py, 4, dotColor);
        }

        for (const auto &pickup : sim.getPickups())
        {
            if (!pickup.isActive())
                continue;
            Vector2 center = pickup.getWorldCenter();
            float px = minimapBounds.x + (center.x / sim.getWorldWidth()) * minimapBounds.width;
            float py = minimapBounds.y + (center.y / sim.getWorldHeight()) * minimapBounds.height;
            DrawCircle((int)px, (int)py, 3, RED);
        }
    };

#ifndef PLATFORM_WEB
    SetTargetFPS(60);
#endif

    while (!WindowShouldClose())
    {
        UpdateMusicStream(bgMusic);

        Rectangle viewport = GetGameViewport();
        gVirtualMouse = WindowToGameMouse(viewport);
        gMouseInGame = CheckCollisionPointRec(GetMousePosition(), viewport);
#if defined(PLATFORM_WEB)
        bool uiTouchPressed{false};
        Vector2 uiTouchGamePos{};
        uiTouchPressed = TouchPressInViewport(viewport, wasUiTouching, uiTouchGamePos);
#endif

        float dt = GetFrameTime();
#if defined(PLATFORM_WEB)
        if (dt > 0.033f)
            dt = 0.033f;
#endif

        if (!offlineMode && netClient.isConnected())
            netClient.poll(0);

        BeginTextureMode(gameTarget);
        ClearBackground(WHITE);

        // ---- Title screen ----
        if (state == GameState::TITLE)
        {
            if (connectPending && !joinedServerSession &&
                GetTime() - connectAttemptStartTime > kJoinResponseTimeoutSec)
            {
                cancelPendingConnect(
                    "Server did not respond. Railway may be down — check deploy logs.");
            }

            ClearBackground(DARKGREEN);
            DrawCenteredText("Top Down Survive Online", gameWidth, gameHeight / 2 - 120, 48, RAYWHITE);
            DrawCenteredText("Enter your name", gameWidth, gameHeight / 2 - 55, 20, LIGHTGRAY);

#if defined(PLATFORM_WEB)
            if (TouchTapInRect(nameInputRec, viewport, wasTouchOnNameField))
            {
                OpenPlayerNameKeyboard(playerName.c_str());
                nameKeyboardOpen = true;
            }
            else if (uiTouchPressed && nameKeyboardOpen &&
                     !CheckCollisionPointRec(uiTouchGamePos, nameInputRec))
            {
                SyncPlayerNameFromKeyboard(playerName);
                nameKeyboardOpen = false;
                ClosePlayerNameKeyboard();
            }
            else if (nameKeyboardOpen && !IsPlayerNameKeyboardFocused())
            {
                SyncPlayerNameFromKeyboard(playerName);
                nameKeyboardOpen = false;
                ClosePlayerNameKeyboard();
            }
            HandlePlayerNameInput(playerName, nameKeyboardOpen);
#else
            HandlePlayerNameInput(playerName, false);
#endif
            DrawNameInput(nameInputRec, playerName);

            if (!statusMessage.empty())
                DrawCenteredText(statusMessage.c_str(), gameWidth, gameHeight / 2 + 130, 20, ORANGE);

            if (Button(joinButtonRec, "Join Queue"
#if defined(PLATFORM_WEB)
                , uiTouchPressed, uiTouchGamePos
#endif
            ))
            {
                if (!offlineMode)
                {
                    if (playerName.empty())
                    {
                        statusMessage = "Enter a name to join.";
                    }
#if defined(PLATFORM_WEB)
                    else if (!netClient.connect(serverWsUrl.c_str()))
                    {
                        ClientNetLog("Connect failed url=%s", serverWsUrl.c_str());
                        statusMessage = "Could not connect to server.";
                    }
#else
                    else if (!netClient.connect(serverHost.c_str(), serverPort, transport))
                    {
                        ClientNetLog("Connect failed host=%s port=%u", serverHost.c_str(), serverPort);
                        statusMessage = "Could not connect to server. Is it running?";
                    }
#endif
                    else if (!netClient.sendJoin(playerName.c_str()))
                    {
                        ClientNetLog("sendJoin failed");
                        cancelPendingConnect("Failed to send join request.");
                    }
                    else
                    {
#if defined(PLATFORM_WEB)
                        ClientNetLog("Connected to %s, sent join as '%s'",
                                     serverWsUrl.c_str(), playerName.c_str());
#else
                        ClientNetLog("Connected to %s:%u, sent join as '%s'",
                                     serverHost.c_str(), serverPort, playerName.c_str());
#endif
                        joinedServerSession = false;
                        connectPending = true;
                        connectAttemptStartTime = GetTime();
                        statusMessage = "Connecting...";
                    }
                }
                else
                {
                JoinResult result = sim.tryJoinQueue(playerName.c_str());
                switch (result)
                {
                case JoinResult::OK:
                    statusMessage.clear();
                    state = GameState::QUEUE;
                    break;
                case JoinResult::INVALID_NAME:
                    statusMessage = "Enter a name to join.";
                    break;
                case JoinResult::SERVER_FULL:
                    statusMessage = "Server is full (10/10). Try again later.";
                    break;
                case JoinResult::ALREADY_IN_QUEUE:
                    statusMessage = "Already in queue.";
                    break;
                case JoinResult::MATCH_IN_PROGRESS:
                    statusMessage = "Match in progress.";
                    break;
                }
                }
            }

#if defined(PLATFORM_WEB)
            drawMuteButton(uiTouchPressed, uiTouchGamePos);
#else
            drawMuteButton();
#endif
            EndTextureMode();
            PresentGame(gameTarget);
            continue;
        }

        // ---- Queue / countdown ----
        if (state == GameState::QUEUE)
        {
            if (!offlineMode && netClient.isConnected() && sim.getPhase() == MatchPhase::COUNTDOWN)
                sim.tickInterpolation();

            ClearBackground(DARKGREEN);
            DrawCenteredText("In Queue", gameWidth, gameHeight / 2 - 100, 40, RAYWHITE);

            char queueText[64];
            snprintf(queueText, sizeof(queueText), "Players: %d / %d",
                     sim.getQueueCount(), GameConfig::kMaxPlayers);
            DrawCenteredText(queueText, gameWidth, gameHeight / 2 - 50, 24, RAYWHITE);

            if (sim.getPhase() == MatchPhase::COUNTDOWN)
            {
                float displayCountdown = sim.getCountdownRemaining();
                if (!offlineMode && onlineCountdownActive)
                {
                    displayCountdown = static_cast<float>(onlineCountdownEndTime - GetTime());
                    if (displayCountdown < 0.f)
                        displayCountdown = 0.f;
                }
                char countdown[48];
                snprintf(countdown, sizeof(countdown), "Starting in %.0f...",
                         displayCountdown);
                DrawCenteredText(countdown, gameWidth, gameHeight / 2, 32, GOLD);

                if (!offlineMode && !sim.isMatchSyncReady())
                    DrawCenteredText("Preparing match...", gameWidth, gameHeight / 2 + 40, 20, LIGHTGRAY);
            }
            else if (sim.canPressReady())
            {
                if (sim.hasUnreadyQueuePlayers())
                    DrawCenteredText("Waiting for all players to ready up...", gameWidth, gameHeight / 2, 22, LIGHTGRAY);
                else
                    DrawCenteredText("Ready when you are!", gameWidth, gameHeight / 2, 22, LIGHTGRAY);
                if (Button(readyButtonRec, "Ready"
#if defined(PLATFORM_WEB)
                    , uiTouchPressed, uiTouchGamePos
#endif
                ))
                {
                    if (!offlineMode && netClient.isConnected())
                    {
                        const bool sent = netClient.sendReady();
                        ClientNetLog("Ready clicked sent=%d localQueue=%d phase=%d canReady=%d",
                                     sent ? 1 : 0, sim.getQueueCount(),
                                     static_cast<int>(sim.getPhase()),
                                     sim.canPressReady() ? 1 : 0);
                    }
                    else
                        sim.pressReady();
                }
            }
            else
            {
                DrawCenteredText("Waiting for more players...", gameWidth, gameHeight / 2, 22, LIGHTGRAY);
            }

            int listY = gameHeight / 2 + 160;
            for (const auto &entry : sim.getPlayers())
            {
                if (!entry.slot.connected || !entry.slot.inQueue)
                    continue;

                char line[GameConfig::kPlayerNameMaxLen + 16];
                snprintf(line, sizeof(line), "%s%s",
                         entry.slot.name,
                         entry.slot.ready ? " (ready)" : "");
                DrawCenteredText(line, gameWidth, listY, 18, RAYWHITE);
                listY += 24;
            }

            if (Button(leaveQueueRec, "Leave Queue"
#if defined(PLATFORM_WEB)
                , uiTouchPressed, uiTouchGamePos
#endif
            ))
            {
                if (!offlineMode && netClient.isConnected())
                {
                    ClientNetLog("Leave queue clicked");
                    netClient.sendLeave();
                    sim.leaveQueue();
                    joinedServerSession = false;
                    netClient.disconnect();
                }
                else
                    sim.leaveQueue();
                state = GameState::TITLE;
                statusMessage.clear();
            }

            sim.tick(dt, true, sim.getLocalPlayerId(), PlayerInput{});

            if (offlineMode)
            {
                if (sim.getPhase() == MatchPhase::IN_PROGRESS)
                    state = GameState::PLAYING;
            }
            else
                SyncUiStateFromSimulation(state, sim, sim.getLocalPlayerId(), netClient.isConnected());

#if defined(PLATFORM_WEB)
            drawMuteButton(uiTouchPressed, uiTouchGamePos);
#else
            drawMuteButton();
#endif
            EndTextureMode();
            PresentGame(gameTarget);
            continue;
        }

        // ---- Match end ----
        if (state == GameState::MATCH_END)
        {
            ClearBackground(DARKGREEN);
            DrawCenteredText("Match Over!", gameWidth, 55, 40, GOLD);

            std::vector<int> winners;
            sim.getWinners(winners);
            if (!winners.empty())
            {
                std::string winnerLine = "Winner: ";
                for (size_t i = 0; i < winners.size(); i++)
                {
                    if (i > 0)
                        winnerLine += ", ";
                    winnerLine += sim.getPlayers()[winners[i]].slot.name;
                }
                DrawCenteredText(winnerLine.c_str(), gameWidth, 105, 22, RAYWHITE);
            }

            DrawKillScoreboard(sim, sim.getLocalPlayerId(), 145, 22, 16, 22);

            char remain[48];
            snprintf(remain, sizeof(remain), "Auto return in %.0fs",
                     sim.getResultsTimeRemaining());
            DrawCenteredText(remain, gameWidth, gameHeight - 28, 18, LIGHTGRAY);

#if defined(PLATFORM_WEB)
            if (Button(returnMenuRec, "Main Menu", uiTouchPressed, uiTouchGamePos))
#else
            if (Button(returnMenuRec, "Main Menu"))
#endif
            {
                if (!offlineMode && netClient.isConnected())
                {
                    netClient.sendLeave();
                    joinedServerSession = false;
                    netClient.disconnect();
                }
                FinishMatchReturnToTitle(state, sim, statusMessage, playerName);
                sim.resetWorldStateSync();
            }

            if (!offlineMode)
                sim.tickResultsCountdownClient(dt);
            else
                sim.tick(dt, true, sim.getLocalPlayerId(), PlayerInput{});

            if (sim.isResultsCountdownFinished() || sim.getPhase() == MatchPhase::IDLE)
            {
                if (!offlineMode && netClient.isConnected())
                {
                    statusMessage = "Match over. Thanks for playing!";
                    joinedServerSession = false;
                    netClient.disconnect();
                }
                FinishMatchReturnToTitle(state, sim, statusMessage, playerName);
                sim.resetWorldStateSync();
            }

#if defined(PLATFORM_WEB)
            drawMuteButton(uiTouchPressed, uiTouchGamePos);
#else
            drawMuteButton();
#endif
            EndTextureMode();
            PresentGame(gameTarget);
            continue;
        }

        // ---- Playing / spectating / paused ----
        const bool isPlaying = (state == GameState::PLAYING);
        const bool isSpectating = (state == GameState::SPECTATING);
        const bool matchSyncReady = offlineMode || sim.isMatchSyncReady();
        const bool showMatchSyncOverlay =
            !offlineMode && netClient.isConnected() && !matchSyncReady &&
            (sim.getPhase() == MatchPhase::COUNTDOWN || sim.getPhase() == MatchPhase::IN_PROGRESS);
#if defined(PLATFORM_WEB)
        const bool applyLocalInput = isPlaying && state != GameState::PAUSED && !showMatchSyncOverlay;
#else
        const bool applyLocalInput = isPlaying && !showMatchSyncOverlay;
#endif

        if (isPlaying || isSpectating
#if defined(PLATFORM_WEB)
            || state == GameState::PAUSED
#endif
        )
        {
            if (!offlineMode && netClient.isConnected())
                sim.tickInterpolation();

            sim.refreshEnemyRenderAnchors();
            sim.refreshPlayerRenderAnchors();

            const Character &camera = sim.getCameraCharacter();
            Vector2 cameraPos = camera.getWorldPos();

            sim.getTileMap().draw(cameraPos);
            for (const auto &prop : sim.getProps())
                prop.Render(cameraPos);

            PlayerInput input{};
            if (applyLocalInput)
            {
#if defined(PLATFORM_WEB)
                const bool touchInput = GetTouchPointCount() > 0;
                int localId = sim.getLocalPlayerId();
                if (localId >= 0 && localId < (int)sim.getPlayers().size())
                {
                    Character &local = const_cast<Character &>(sim.getPlayers()[localId].character);
                    local.setBlockMouseAttack(!touchInput && touchControls.blocksMouseAttack());
                }
                CollectLocalInput(input, touchControls, viewport, true);
#else
                CollectLocalInput(input);
#endif
            }

            if (!offlineMode && applyLocalInput)
            {
#if !defined(PLATFORM_WEB)
                const bool edgePress = input.attackPressed || input.thunderPressed;
                const bool moveChanged =
                    !hasLastSentInput ||
                    std::fabs(input.move.x - lastSentInput.move.x) > 0.01f ||
                    std::fabs(input.move.y - lastSentInput.move.y) > 0.01f ||
                    input.attackHeld != lastSentInput.attackHeld;
                inputSendAccum += dt;

                const bool sendDue = inputSendAccum >= inputSendInterval;
                if (!hasLastSentInput || edgePress || (sendDue && moveChanged))
                {
                    netClient.sendInput(
                        input.move.x, input.move.y,
                        input.attackPressed, input.attackHeld, input.thunderPressed);
                    lastSentInput = input;
                    hasLastSentInput = true;
                    if (sendDue)
                        inputSendAccum = 0.0;
                }
#else
                netClient.sendInput(
                    input.move.x, input.move.y,
                    input.attackPressed, input.attackHeld, input.thunderPressed);
#endif
                sim.predictLocalPlayerMovement(sim.getLocalPlayerId(), dt, input);
                sim.predictLocalThunderCast(sim.getLocalPlayerId(), input);
            }
            sim.tick(dt, applyLocalInput, sim.getLocalPlayerId(), input);

            int localId = sim.getLocalPlayerId();
            if (!offlineMode)
            {
                SyncUiStateFromSimulation(state, sim, localId, netClient.isConnected());
            }
            else
            {
                if (localId >= 0 && localId < (int)sim.getPlayers().size())
                {
                    const auto &localEntry = sim.getPlayers()[localId];
                    if (localEntry.slot.inMatch)
                    {
                        if (!localEntry.character.getAlive() && sim.countAlivePlayers() > 0)
                            state = GameState::SPECTATING;
                        else if (localEntry.character.getAlive())
                            state = GameState::PLAYING;
                    }
                }
            }

            if (sim.getPhase() == MatchPhase::RESULTS)
                state = GameState::MATCH_END;

            if (!offlineMode && applyLocalInput && localId >= 0 && localId < (int)sim.getPlayers().size())
            {
                Character &local = const_cast<Character &>(sim.getPlayers()[localId].character);
                local.setNetworkVisualState(
                    Vector2LengthSqr(input.move) > 0.0001f,
                    input.attackHeld);
                if (input.move.x < -0.01f)
                    local.setNetworkFacing(-1.f);
                else if (input.move.x > 0.01f)
                    local.setNetworkFacing(1.f);
            }

            for (const auto &entry : sim.getPlayers())
            {
                if (!entry.slot.connected || !entry.slot.inMatch)
                    continue;
                const_cast<Character &>(entry.character).tick(dt, false, true);
            }

            for (const auto &pickup : sim.getPickups())
                pickup.Render(cameraPos);

            for (auto &enemy : const_cast<std::vector<Enemy> &>(sim.getEnemies()))
                enemy.tick(dt, false, true);

            for (auto &strike : const_cast<std::vector<Thunderstrike> &>(sim.getStrikes()))
                strike.tick(dt);

            for (auto &strike : const_cast<std::vector<Thunderstrike> &>(sim.getStrikes()))
                strike.Render(cameraPos);

            DrawMatchTimer(sim);

            if (localId >= 0 && localId < (int)sim.getPlayers().size())
            {
                const auto &local = sim.getPlayers()[localId];
                if (isPlaying && local.character.getAlive())
                {
                    std::string healthText = "Health: ";
                    healthText.append(std::to_string(local.character.getHealth()), 0, 5);
                    DrawText(healthText.c_str(), 55.f, 45.f, 32, RED);

                    std::string killText = "Kills: " + std::to_string(local.slot.kills);
                    DrawText(killText.c_str(), 55.f, 82.f, 24, BLACK);
                }
            }

            if (isSpectating)
            {
                int targetId = sim.getSpectateTargetId();
                if (targetId >= 0 && targetId < (int)sim.getPlayers().size())
                {
                    char banner[64];
                    snprintf(banner, sizeof(banner), "Spectating: %s",
                             sim.getPlayers()[targetId].slot.name);
                    DrawCenteredText(banner, gameWidth, gameHeight - 100, 22, RAYWHITE);
                }

                if (Button(spectatePrevRec, "Prev"
#if defined(PLATFORM_WEB)
                    , uiTouchPressed, uiTouchGamePos
#endif
                ) || IsKeyPressed(KEY_LEFT_BRACKET))
                {
                    sim.spectatePrev();
                }

                if (Button(spectateNextRec, "Next"
#if defined(PLATFORM_WEB)
                    , uiTouchPressed, uiTouchGamePos
#endif
                ) || IsKeyPressed(KEY_RIGHT_BRACKET))
                {
                    sim.spectateNext();
                }
            }

            showScoreboard = IsKeyDown(KEY_TAB);
            if (showScoreboard)
                DrawKillScoreboard(sim, sim.getLocalPlayerId());

            drawMinimap();

            if (showMatchSyncOverlay)
            {
                DrawRectangle(0, 0, gameWidth, gameHeight, ColorAlpha(BLACK, 0.72f));
                DrawCenteredText("Syncing...", gameWidth, gameHeight / 2 - 12, 40, RAYWHITE);
                char bufferText[48];
                snprintf(bufferText, sizeof(bufferText), "Buffering snapshots (%zu/%zu)",
                         sim.getMatchSyncBufferCount(), GameConfig::kMinSnapshotsForPlay);
                DrawCenteredText(bufferText, gameWidth, gameHeight / 2 + 36, 18, LIGHTGRAY);
            }

#if defined(PLATFORM_WEB)
            if (isPlaying)
            {
                int lid = sim.getLocalPlayerId();
                bool charged = false;
                if (lid >= 0 && lid < (int)sim.getPlayers().size())
                    charged = sim.getPlayers()[lid].character.isCharged()
                        && !sim.isThunderActiveForPlayer(lid);
                touchControls.draw(charged);
            }
            drawMuteButton(uiTouchPressed, uiTouchGamePos);

            if (state == GameState::PAUSED)
            {
                DrawRectangle(0, 0, gameWidth, gameHeight, Fade(BLACK, 0.55f));
                DrawCenteredText("Paused", gameWidth, gameHeight / 2 - 60, 48, RAYWHITE);
                if (Button(pauseContinueRec, "Continue", uiTouchPressed, uiTouchGamePos))
                    state = GameState::PLAYING;
            }
            else if (isPlaying && IsKeyPressed(KEY_ESCAPE))
            {
                state = GameState::PAUSED;
            }
#else
            drawMuteButton();
#endif

            EndTextureMode();
            PresentGame(gameTarget);
            continue;
        }

        EndTextureMode();
        PresentGame(gameTarget);
    }

    UnloadRenderTexture(gameTarget);
    UnloadMusicStream(bgMusic);
    CloseAudioDevice();
    CloseWindow();
}
