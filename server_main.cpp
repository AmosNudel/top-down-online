#include "GameSimulation.h"
#include "GameConfig.h"
#include "NetCommon.h"
#include "NetGameHost.h"
#include "raylib.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <thread>
#include <vector>

static GameSimulation gSim;
static NetGameHost gServer;
static std::vector<uint8_t> gStatePacket;
static std::vector<uint8_t> gLobbyPacket;
static uint16_t gEnetPort = GameConfig::kDefaultServerPort;
static uint16_t gStreamPort = GameConfig::kDefaultStreamPort;
static FILE *gServerLog = nullptr;

static void serverLog(const char *msg)
{
    if (!gServerLog || !msg)
        return;
    fprintf(gServerLog, "%s\n", msg);
    fflush(gServerLog);
}

static void sendJoinFail(const NetPeerRef &peer, NetJoinFailReason reason)
{
    NetSJoinFail pkt{};
    NetSerialize::initHeader(pkt.hdr, NetMsgType::S_JOIN_FAIL);
    pkt.reason = static_cast<uint8_t>(reason);
    gServer.sendToPeer(peer, &pkt, sizeof(pkt), true);
}

static void sendJoinOk(const NetPeerRef &peer, int playerId, uint8_t colorIndex)
{
    NetSJoinOk pkt{};
    NetSerialize::initHeader(pkt.hdr, NetMsgType::S_JOIN_OK);
    pkt.playerId = static_cast<uint8_t>(playerId);
    pkt.colorIndex = colorIndex;
    gServer.sendToPeer(peer, &pkt, sizeof(pkt), true);
}

static void broadcastLobbySync()
{
    gSim.buildLobbySyncPacket(gLobbyPacket);
    gServer.broadcast(gLobbyPacket.data(), gLobbyPacket.size(), true);
}

static void broadcastWorldState()
{
    gSim.buildWorldStatePacket(gStatePacket);
    gServer.broadcast(gStatePacket.data(), gStatePacket.size(), true);
}

static void handleServerPacket(const NetPeerRef &peer, const uint8_t *data, size_t size)
{
    if (!data || size < sizeof(NetPacketHeader))
        return;

    auto type = static_cast<NetMsgType>(data[0]);
    int playerId = gServer.peerToPlayerId(peer);

    switch (type)
    {
    case NetMsgType::C_JOIN:
    {
        if (size < sizeof(NetCJoin))
            return;

        NetCJoin pkt{};
        std::memcpy(&pkt, data, sizeof(pkt));

        if (playerId >= 0)
            return;

        JoinResult result = gSim.tryJoinQueue(pkt.name, true, &playerId);
        if (result != JoinResult::OK)
        {
            NetJoinFailReason reason = NET_JOIN_INVALID_NAME;
            if (result == JoinResult::SERVER_FULL)
                reason = NET_JOIN_SERVER_FULL;
            else if (result == JoinResult::MATCH_IN_PROGRESS)
                reason = NET_JOIN_MATCH_IN_PROGRESS;
            sendJoinFail(peer, reason);
            gServer.disconnectPeer(peer);
            return;
        }

        if (playerId < 0)
            return;

        gServer.registerClient(peer, playerId);
        sendJoinOk(peer, playerId, gSim.getPlayers()[playerId].slot.colorIndex);

        char joined[128];
        snprintf(joined, sizeof(joined),
                 "Player '%s' joined (id %d). Queue=%d.",
                 pkt.name, playerId, gSim.getQueueCount());
        serverLog(joined);
        broadcastLobbySync();
        break;
    }
    case NetMsgType::C_READY:
        if (playerId < 0)
        {
            serverLog("Ready ignored: peer not registered.");
            break;
        }
        if (gSim.pressReadyForPlayer(playerId))
        {
            char readyMsg[96];
            snprintf(readyMsg, sizeof(readyMsg),
                     "Player %d ready. Queue=%d phase=COUNTDOWN.",
                     playerId, gSim.getQueueCount());
            serverLog(readyMsg);
            broadcastLobbySync();
            if (gSim.getPhase() == MatchPhase::COUNTDOWN)
                broadcastWorldState();
        }
        else
        {
            char reject[128];
            snprintf(reject, sizeof(reject),
                     "Ready rejected for player %d (phase=%d queue=%d need=%d).",
                     playerId, static_cast<int>(gSim.getPhase()),
                     gSim.getQueueCount(), GameConfig::kMinPlayersToStart);
            serverLog(reject);
        }
        break;
    case NetMsgType::C_LEAVE:
        if (playerId >= 0)
        {
            char left[64];
            snprintf(left, sizeof(left), "Player %d left queue.", playerId);
            serverLog(left);
            gSim.removePlayer(playerId);
            gServer.unregisterPeer(peer);
            gServer.disconnectPeer(peer);
            broadcastLobbySync();
            if (gSim.getPhase() == MatchPhase::IN_PROGRESS || gSim.getPhase() == MatchPhase::RESULTS)
                broadcastWorldState();
        }
        else
        {
            serverLog("Leave ignored: peer not registered.");
        }
        break;
    case NetMsgType::C_INPUT:
    {
        if (playerId < 0 || size < sizeof(NetCInput))
            return;

        NetCInput pkt{};
        std::memcpy(&pkt, data, sizeof(pkt));

        PlayerInput input{};
        input.move = Vector2{pkt.moveX, pkt.moveY};
        input.attackPressed = pkt.attackPressed != 0;
        input.attackHeld = pkt.attackHeld != 0;
        input.thunderPressed = pkt.thunderPressed != 0;
        gSim.setRemoteInput(playerId, input);
        break;
    }
    case NetMsgType::C_SPECTATE:
    {
        if (playerId < 0 || size < sizeof(NetCSpectate))
            return;

        NetCSpectate pkt{};
        std::memcpy(&pkt, data, sizeof(pkt));
        gSim.spectateForPlayer(playerId, pkt.direction >= 0 ? 1 : -1);
        break;
    }
    default:
        break;
    }
}

static void parseArgs(int argc, char **argv)
{
    for (int i = 1; i < argc; i++)
    {
        if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc)
        {
            gEnetPort = static_cast<uint16_t>(std::atoi(argv[i + 1]));
            i++;
        }
        else if (std::strcmp(argv[i], "--tcp-port") == 0 && i + 1 < argc)
        {
            gStreamPort = static_cast<uint16_t>(std::atoi(argv[i + 1]));
            i++;
        }
    }

    if (const char *streamPortEnv = std::getenv("GAME_TCP_PORT"))
    {
        const int parsed = std::atoi(streamPortEnv);
        if (parsed > 0 && parsed <= 65535)
            gStreamPort = static_cast<uint16_t>(parsed);
    }
    else if (const char *railwayTcp = std::getenv("RAILWAY_TCP_APPLICATION_PORT"))
    {
        const int parsed = std::atoi(railwayTcp);
        if (parsed > 0 && parsed <= 65535)
            gStreamPort = static_cast<uint16_t>(parsed);
    }
}

int main(int argc, char **argv)
{
    parseArgs(argc, argv);

    ChangeDirectory(GetApplicationDirectory());

    FILE *logFile = fopen("server.log", "a");
    gServerLog = logFile;
    auto logLine = [&](const char *msg) { ::serverLog(msg); };

    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(320, 240, "TopDownSurvive Server");

    gSim.init(static_cast<unsigned int>(time(nullptr)));

    if (!gServer.start(gEnetPort, gStreamPort, GameConfig::kMaxPlayers))
    {
        logLine("ERROR: Failed to start server (port in use or network init failed).");
        if (logFile)
            fclose(logFile);
        CloseWindow();
        return 1;
    }

    gServer.setPacketHandler(handleServerPacket);
    gServer.setDisconnectHandler([](int playerId) {
        char left[64];
        snprintf(left, sizeof(left), "Player %d disconnected.", playerId);
        serverLog(left);
        gSim.removePlayer(playerId);
        broadcastLobbySync();
        const MatchPhase phase = gSim.getPhase();
        if (phase == MatchPhase::IN_PROGRESS || phase == MatchPhase::RESULTS)
            broadcastWorldState();
    });

    logLine("Server build: wall-clock-ticks v3 + stream relay.");
    char started[160];
    snprintf(started, sizeof(started),
             "TopDownSurvive server listening on UDP %u and TCP %u.",
             gEnetPort, gStreamPort);
    logLine(started);
    TraceLog(LOG_INFO, "%s", started);

    double accumulator = 0.0;
    const double tickDt = 1.0 / GameConfig::kServerTickRate;
    double lobbyBroadcastAccum = 0.0;
    const double lobbyBroadcastInterval = 0.1;
    auto lastTick = std::chrono::steady_clock::now();
    MatchPhase lastLoggedPhase = MatchPhase::IDLE;
    MatchPhase prevPhase = MatchPhase::IDLE;
    bool loggedCountdownTick = false;
    bool matchEndHandled = false;

    while (!WindowShouldClose())
    {
        gServer.poll(0);
        PollInputEvents();

        auto now = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed = now - lastTick;
        lastTick = now;
        double dt = elapsed.count();
        if (dt <= 0.0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        if (dt > 0.25)
            dt = 0.25;

        accumulator += dt;
        while (accumulator >= tickDt)
        {
            gSim.tickServer(static_cast<float>(tickDt));

            const MatchPhase phase = gSim.getPhase();
            if (phase != lastLoggedPhase)
            {
                if (phase == MatchPhase::IN_PROGRESS)
                    serverLog("Match started.");
                else if (phase == MatchPhase::COUNTDOWN)
                {
                    char cd[64];
                    snprintf(cd, sizeof(cd), "Countdown started (%.0fs).",
                             gSim.getCountdownRemaining());
                    serverLog(cd);
                    loggedCountdownTick = false;
                }
                lastLoggedPhase = phase;
            }

            if (phase == MatchPhase::COUNTDOWN && !loggedCountdownTick)
            {
                char cd[80];
                snprintf(cd, sizeof(cd), "Countdown ticking (%.1fs left).",
                         gSim.getCountdownRemaining());
                serverLog(cd);
                loggedCountdownTick = true;
            }
            if (phase != MatchPhase::COUNTDOWN)
                loggedCountdownTick = false;

            if (phase == MatchPhase::IN_PROGRESS || phase == MatchPhase::RESULTS ||
                phase == MatchPhase::COUNTDOWN)
            {
                gSim.buildWorldStatePacket(gStatePacket);
                gServer.broadcast(gStatePacket.data(), gStatePacket.size(), true);
            }
            else
            {
                lobbyBroadcastAccum += tickDt;
                if (lobbyBroadcastAccum >= lobbyBroadcastInterval)
                {
                    broadcastLobbySync();
                    lobbyBroadcastAccum = 0.0;
                }
            }

            if (!matchEndHandled && phase == MatchPhase::RESULTS && gSim.isResultsCountdownFinished())
            {
                matchEndHandled = true;
                serverLog("Match ended — kicking players to menu.");
                std::vector<int> playerIds = gServer.getConnectedPlayerIds();
                for (int id : playerIds)
                    gServer.disconnectPlayer(id, "Match over. Thanks for playing!");
                gServer.flush();
                gSim.returnPlayersToMenu();
            }
            if (phase == MatchPhase::IN_PROGRESS || phase == MatchPhase::COUNTDOWN)
                matchEndHandled = false;
            prevPhase = phase;

            accumulator -= tickDt;
        }
    }

    gServer.shutdown();
    logLine("Server stopped.");
    if (logFile)
        fclose(logFile);
    CloseWindow();
    return 0;
}
