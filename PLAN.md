# Top Down Survive — Dedicated Server Migration Plan

**Last updated:** 2026-06-13  
**Status:** Phases 1–4 implemented (desktop). Phase 5 polish pending.

---

## Implementation Progress (summary)

| Phase | Status | Notes |
|-------|--------|-------|
| 0 Planning | Done | This document |
| 1 Simulation extraction | Done | `GameSimulation`, multi-player vector |
| 2 Game rules (local) | Done | Queue, kills, spectate, enemies @ 200 |
| 3 Networking | Done | ENet, join/input/snapshots |
| 4 Headless server | Done | `TopDownSurviveServer.exe`, hidden window |
| 5 Polish | In progress | Interpolation, thunder VFX sync, match-end kick |

### Build & run (Windows)

```powershell
# Debug client (connects to server by default)
.\tools\build_debug.ps1

# Dedicated server
.\tools\build_server.ps1

# Terminal 1
.\dist-server\TopDownSurviveServer.exe --port 27015

# Terminal 2 (online — default)
.\TopDownSurvive.exe

# Solo local testing without server (debug only, min 1 player)
.\TopDownSurvive.exe --offline
```

**Toolchain:** use full path if `mingw32-make` is not on PATH:

`C:\raylib\w64devkit\bin\mingw32-make.exe`

**IDE:** `.clangd` and `.vscode/c_cpp_properties.json` include raylib + ENet paths.

### Client modes

| Mode | Flag | Behavior |
|------|------|----------|
| Online (default) | — | Connects to `127.0.0.1:27015`, server-authoritative |
| Offline | `--offline` | Local `GameSimulation` only (debug solo) |
| Custom host | `--host IP --port N` | Connect to remote server |

### Ready-button rule (decided)

One player pressing **Ready** starts the 5s countdown when `players >= kMinPlayersToStart` (2 release / 1 debug).

### Web multiplayer

Deferred (Phase 5). Web build remains offline-only; WASM cannot use raw UDP without a relay.

---

## Table of Contents

1. [Current State Summary](#1-current-state-summary)
2. [Target Architecture](#2-target-architecture)
3. [Networking Stack Choice](#3-networking-stack-choice)
4. [Headless Dedicated Server Setup](#4-headless-dedicated-server-setup)
5. [Game State Machine (New)](#5-game-state-machine-new)
6. [Main Menu & Queue Flow](#6-main-menu--queue-flow)
7. [Player Identity & Colors](#7-player-identity--colors)
8. [Scoring: Kill Count & Scoreboard Overlay](#8-scoring-kill-count--scoreboard-overlay)
9. [Death, Spectating & Camera](#9-death-spectating--camera)
10. [Match Lifecycle & Timers](#10-match-lifecycle--timers)
11. [Enemy System Changes](#11-enemy-system-changes)
12. [Method Audit: Local → Network Roles](#12-method-audit-local--network-roles)
13. [Refactoring Phases](#13-refactoring-phases)
14. [Proposed File Structure](#14-proposed-file-structure)
15. [Build & Deployment](#15-build--deployment)
16. [Open Questions & Risks](#16-open-questions--risks)

---

## 1. Current State Summary

| Area | Current behavior | Key files |
|------|------------------|-----------|
| Build | Makefile + raylib; desktop (`TopDownSurvive.exe`) and web (Emscripten WASM) | `Makefile`, `tools/build_*.ps1` |
| Networking | **None** | — |
| Game loop | Monolithic `main()` with inline lambdas | `main.cpp` (~1022 lines) |
| Players | Single `Character knight` | `Character.{h,cpp}` |
| Enemies | Chase single knight; cap **50**; spawn waves every 3s | `Enemy.{h,cpp}`, `main.cpp` |
| Score | Passive rate + 25/kill; displayed live and on game over | `main.cpp` |
| Menu | Title → "Start Game" → play immediately | `main.cpp` `GameState::TITLE` |
| Death | `GAME_OVER` screen; no respawn | `main.cpp` |

Everything runs locally in one process. There is no separation between simulation, rendering, or input.

---

## 2. Target Architecture

### Topology: Dedicated Server (no lobbies)

```
┌─────────────┐     UDP/TCP      ┌──────────────────────┐
│  Client 1   │◄────────────────►│  Dedicated Server    │
│  (raylib)   │                  │  (headless, no GPU)  │
└─────────────┘                  │                      │
┌─────────────┐                  │  • Authoritative sim │
│  Client 2   │◄────────────────►│  • One room, max 10  │
│  (raylib)   │                  │  • Queue + match     │
└─────────────┘                  └──────────────────────┘
       ...
┌─────────────┐
│  Client N   │◄────────────────►
│  (web/desktop)
└─────────────┘
```

### Authority model

| System | Authority | Notes |
|--------|-----------|-------|
| Player movement | Server validates | Client sends input; server integrates and resolves collisions |
| Combat (sword, thunder) | Server | Client sends intent; server checks hitboxes |
| Enemy AI & spawning | Server | Clients receive replicated positions |
| Pickups | Server | Spawn + collection validated server-side |
| Kill counts | Server | Attribution on enemy death |
| Match flow (queue, timer, winner) | Server | Clients display UI from server messages |
| Rendering, audio, UI | Client | Pure presentation |
| Player name (menu) | Client → Server | Sent on join; server stores and broadcasts |
| Spectate camera target | Client choice | Client sends spectate index; server confirms target is alive |

### Single room rules

- **Max players:** 10
- **No lobby browser:** clients connect to a configured host/port
- **Server full:** join attempt returns rejection message; client stays on main menu
- **Shared world:** all players exist in the same map instance (same seed per match)

---

## 3. Networking Stack Choice

### Recommended: **ENet** (primary) or **GameNetworkingSockets** (alternative)

| Library | Pros | Cons |
|---------|------|------|
| **ENet** | Lightweight UDP, reliable + unreliable channels, common in indie/raylib projects, easy headless use | Manual serialization; no built-in encryption |
| **GameNetworkingSockets** | Valve-backed, NAT-friendly, encryption option | Heavier dependency; more setup |
| **Web clients** | Need WebSocket gateway or separate web build limitation | WASM cannot use raw UDP to arbitrary servers without a relay |

**Practical recommendation for this project:**

1. **Desktop dedicated server + desktop clients:** ENet over UDP.
2. **Web build:** Either defer multiplayer on web initially, or add a small **WebSocket-to-UDP relay** (Node or C++ sidecar) that the WASM client talks to. The relay is not a game-authority server — it only bridges transport.

### Message design (minimal v1)

Use a small custom binary protocol (or flat structs + `memcpy` with endian fix):

```
// Client → Server
C_JOIN          { name[32] }
C_READY         { }
C_INPUT         { seq, moveX, moveY, attack, thunder, spectateDir }
C_DISCONNECT    { }

// Server → Client
S_JOIN_OK       { playerId, assignedColor }
S_JOIN_FAIL     { reason }          // e.g. SERVER_FULL
S_QUEUE_STATE   { playerCount, showReady, countdown }
S_MATCH_START   { mapSeed, spawnIndex }
S_SNAPSHOT      { tick, players[], enemies[], pickups[], strikes[] }
S_PLAYER_DIED   { playerId }
S_KILL_EVENT    { killerId, victimId? }   // victimId only for PvP later; enemy kill = killer only
S_MATCH_END     { winnerId, kills[] }
S_KICK_TO_MENU  { message[64] }
```

Start with **20–30 Hz server tick** and full snapshots; optimize to deltas later if needed.

---

## 4. Headless Dedicated Server Setup

### Goal

A second executable (or compile flag) that runs **without** `InitWindow`, textures, or audio — only simulation + networking.

### Approach A: Separate target (recommended)

```
TopDownSurvive.exe        → client (existing raylib entry)
TopDownSurviveServer.exe  → server (new server_main.cpp)
```

Shared simulation code compiled into both:

| Module | Client | Server |
|--------|--------|--------|
| `GameSimulation.cpp` | read snapshots | runs tick |
| `Character.cpp` (logic only) | predict/interpolate | full tick |
| `Enemy.cpp` | — | full tick |
| `TileMap.cpp` (collision only) | draw + collision | collision only (`#ifndef SERVER_HEADLESS` around draw) |
| `main.cpp` | client UI loop | **not linked** |
| `server_main.cpp` | **not linked** | headless loop |

### Server main loop (pseudocode)

```cpp
// server_main.cpp
int main(int argc, char** argv) {
    uint16_t port = ParsePort(argc, argv);  // default 27015
    NetHost* host = NetHostCreate(port, MAX_CLIENTS);

    GameSimulation sim;
    sim.InitMap(DeterministicSeed());  // seed fixed at match start, not at boot

    double accumulator = 0;
    const double tickDt = 1.0 / 30.0;

    while (running) {
        NetPoll(host);                     // drain incoming packets
        ApplyClientMessages(host, sim);  // join, ready, input

        accumulator += GetElapsedSeconds();
        while (accumulator >= tickDt) {
            sim.Tick((float)tickDt);
            accumulator -= tickDt;
        }

        BroadcastSnapshot(host, sim);
        MaybeBroadcastQueueState(host, sim);
    }
}
```

### Headless considerations

| Concern | Solution |
|---------|----------|
| raylib collision/math | Keep `raymath.h`; avoid linking full raylib GUI on server — extract collision helpers or use `-DPLATFORM_DEDICATED_SERVER` stub header |
| Texture loading in constructors | **Refactor** `Character`/`Enemy` constructors: split `CharacterData` (logic) from `CharacterRenderer` (textures). Server uses `CharacterData` only |
| Logging | `printf` / file log to `server.log` |
| No display | Do not call `InitWindow`, `InitAudioDevice`, `LoadTexture` on server |
| Deployment | Run as Windows service, Docker (Linux build), or VPS process |

### Makefile changes (sketch)

```makefile
# Client (default)
OBJS = main.cpp Character.cpp Enemy.cpp ... GameSimulation.cpp NetClient.cpp

# Server
SERVER_OBJS = server_main.cpp GameSimulation.cpp Enemy.cpp ... NetServer.cpp
SERVER_NAME = TopDownSurviveServer
```

Add `make server` target producing `TopDownSurviveServer.exe`.

# My note:
I will deploy the headless server on railway, so a linux docker container build

### Configuration

| Flag / env | Default | Purpose |
|------------|---------|---------|
| `--port` | 27015 | Listen port |
| `--max-players` | 10 | Room cap |
| `--tick-rate` | 30 | Simulation Hz |
| `--map-seed` | (at match start) | Procedural map per round |

---

## 5. Game State Machine (New)

Replace/extend the current `GameState` enum in `main.cpp`:

```cpp
enum class GameState {
    TITLE,           // main menu: name + join queue
    QUEUE,           // waiting in server queue (was "Start Game")
    COUNTDOWN,       // 5s after ready (2+ players)
    PLAYING,         // alive and in match
    SPECTATING,      // dead, watching another player
    SCOREBOARD,      // Tab overlay while playing/spectating (input flag, not full state)
    MATCH_END,       // winner screen, 20 seconds
    // PAUSED — keep web-only pause or remove for multiplayer fairness
    // GAME_OVER — replaced by SPECTATING + MATCH_END
};
```

**Server-side parallel enum** `MatchPhase`:

```
LOBBY_IDLE       // no players queued
QUEUE            // 1+ players in queue, waiting for 2nd player
READY_AVAILABLE  // 2+ players, ready button enabled
COUNTDOWN        // ready pressed, 5s timer
IN_PROGRESS      // 5-minute match timer running
RESULTS          // 20s winner display
COOLDOWN         // brief reset before queue reopens
```

Client UI state is driven by server `S_QUEUE_STATE`, `S_MATCH_START`, `S_MATCH_END`, `S_KICK_TO_MENU`.

---

## 6. Main Menu & Queue Flow

### Main menu (TITLE)

| Element | Change |
|---------|--------|
| Name input | **Add on desktop and web** (move/reuse web `HandlePlayerNameInput` pattern; allow longer names, e.g. 16 chars) |
| Button | Rename **"Start Game"** → **"Join Queue"** |
| Validation | Non-empty name required to join |
| Server full | Show message: *"Server is full (10/10). Try again later."* |
| Connection | Client connects to `host:port` from config (`config.txt` or compile-time default `127.0.0.1:27015`) |

### Queue flow (server-authoritative)

```
Player joins queue (name sent)
        │
        ▼
┌───────────────────┐
│ 1 player in queue │ → "Waiting for players..."
└───────────────────┘
        │ 2nd player joins
        ▼
┌───────────────────┐
│ 2+ in queue       │ → Show **Ready** button on each client
└───────────────────┘
        │ any player presses Ready (design: one ready or all ready?)
        ▼
┌───────────────────┐
│ 5 second countdown│ → "Starting in 5..." (more players can still join queue until match starts)
└───────────────────┘
        │
        ▼
   MATCH START (spawn all queued players)
        │
        ▼
   5-minute timer OR no living players
        │
        ▼
   Winner screen (20s) → kick all to main menu
```

**Design note — Ready trigger:** Plan assumes **one player pressing Ready** starts the 5s countdown once `playerCount >= 2`. Document alternative (all must ready) if you prefer stricter flow.

**Late joiners during countdown:** Allowed until `MATCH START` fires; they spawn at match start with everyone else.

**Comment out / remove:**

- Web game-over score submission flow (`SendScoreToParent`, submit score button) — replaced by in-match kill table and match-end screen.
- Desktop/web **"Play Again"** during match — return only via `S_KICK_TO_MENU` after match end.

---

## 7. Player Identity & Colors

### Color pool (no duplicates)

Define a constant array of **10 distinct colors** (max players):

```cpp
static const Color kPlayerColors[MAX_PLAYERS] = {
    { 230,  41,  55, 255 },  // red
    {  0, 121, 241, 255 },  // blue
    {  0, 228,  48, 255 },  // green
    { 255, 214,   0, 255 },  // gold
    { 200, 122, 255, 255 },  // purple
    { 255, 161,   0, 255 },  // orange
    {  0, 228, 228, 255 },  // cyan
    { 255, 109, 194, 255 },  // pink
    { 130, 201,  30, 255 },  // lime
    { 102, 191, 255, 255 },  // sky
};
```

**Server:** on join, assign lowest unused index `0..9` into `PlayerSlot::colorIndex`. On disconnect before match, free the slot.

**Client:** apply `tint = kPlayerColors[colorIndex]` in `BaseCharacter::render()` for all player characters.

**Shared space:** no teams; friendly fire policy = **off** (sword/thunder only hit enemies). Players can overlap freely.

### Spawn placement

Per match, server assigns spawn points in a ring around map center (reuse `carveLand` at center once). Offset each player by angle `2π * i / N` to avoid overlap.

---

## 8. Scoring: Kill Count & Scoreboard Overlay

### Remove / comment out high score system

In `main.cpp`, **comment out** (do not delete yet, for reference):

- `score`, `scoreRate`, `scoreRateTimer`, `scoreRateGrowth`
- Passive score tick in simulate block
- HUD `Score: N` text
- Game over score display
- `registerKill()` score bonus → replace with kill count increment
- Web `SendScoreToParent` / submit score UI

### Kill count

Per-player struct on server:

```cpp
struct PlayerStats {
    uint8_t  id;
    char     name[32];
    uint16_t kills;
    bool     alive;
    uint8_t  colorIndex;
};
```

**Increment rule:** When an enemy dies, server credits the player whose attack caused death (sword hit or thunder AoE owner). Thunder AoE may kill multiple — one kill each, same killer.

### Tab overlay (client)

- While `PLAYING` or `SPECTATING`, hold **Tab** to show overlay (not a separate game state — use `showScoreboard` bool).
- Table columns: `Name | Kills | Status (Alive/Dead)`
- Sort by kills descending; highlight local player row.
- Optional: tint swatch column using `kPlayerColors`.

---

## 9. Death, Spectating & Camera

### No respawn

When `health <= 0`, server sets `alive = false` and notifies `S_PLAYER_DIED`.

### Client transition

```
PLAYING → SPECTATING (if other alive players exist)
        → MATCH_END soon if no alive players remain
```

### Spectate controls

| Input | Action |
|-------|--------|
| `[` / `←` or on-screen **Prev** | Previous alive player |
| `]` / `→` or on-screen **Next** | Next alive player |

Client sends `spectateDir` in input packet; server responds with `cameraTargetPlayerId` in snapshot (must be alive). Client renders world from spectated player's `worldPos` (same as current `knight.getWorldPos()` camera model).

### Local player HUD while spectating

Hide health bar or show spectated player's health. Show banner: *"Spectating: {name}"*.

### Minimap

Show all **alive** players as colored dots; dead players omitted or gray.

---

## 10. Match Lifecycle & Timers

| Timer | Duration | Trigger |
|-------|----------|---------|
| Ready countdown | 5 seconds | Ready pressed AND `players >= 2` |
| Match duration | **5 minutes** | Match start (timer runs only while `IN_PROGRESS`) |
| Results screen | **20 seconds** | Match end condition met |
| Kick to menu | After results | `S_KICK_TO_MENU` to all clients |

### Match end conditions (either)

1. **5-minute timer expires** → winner = highest kills (tie: multiple winners shown).
2. **No living players remain** → end immediately; winner = highest kills among all who played.

### After match

- Server resets simulation: clear enemies, pickups, strikes, player entities.
- Return to `QUEUE` phase; players must press **Join Queue** again from title (they were kicked to TITLE).
- Map seed: new random seed per match for variety.

---

## 11. Enemy System Changes

### Cap increase

Change `maxEnemies` from **50** → **200** in simulation (`GameSimulation` / server).

### Target selection algorithm

**Yes, your description makes sense.** It is load-balanced proximity targeting:

- Prefer **nearby** players with **few** enemies already chasing them.
- Deprioritize players already swarmed.
- A **far** player with **few** chasers can become the best target when all nearby players are overloaded.

**Suggested score** (lower = better target):

```
score(p) = distance(p, spawnPos) * (1.0f + kLoad * enemiesTargeting[p])
```

- `distance`: world-space from spawn point to player center.
- `enemiesTargeting[p]`: count of living enemies currently assigned to player `p`.
- `kLoad`: tuning constant (start with `0.75`; playtest 0.5–1.5).

Pick player with **minimum** score. Tie-break: random among tied.

**Refactor `Enemy`:** replace `Character* target` with `uint8_t targetPlayerId` and resolve through `GameSimulation::GetPlayer(id)`.

### Periodic group spawn

Every **N** waves (e.g. every 5th wave) or every **T** seconds (e.g. 45s):

1. Pick random alive player `R`.
2. Spawn **G** enemies (e.g. 8–15) in a ring around `R` (or around map center toward `R`).
3. Assign **all** in the group to `R` regardless of score (explicit swarm event).

### Spawn position

Current logic spawns relative to **single knight**. Change to spawn relative to **selected target player** or **map center** for group spawns.

### Wave scaling

Keep wave interval (`spawnInterval = 3s`) and `spawnCount` increment; verify performance at 200 enemies on server CPU.

---

## 12. Method Audit: Local → Network Roles

Legend:

| Role | Meaning |
|------|---------|
| **LOCAL-RENDER** | Client only; no network |
| **LOCAL-INPUT** | Client captures; sent to server |
| **SERVER-SIM** | Server authoritative |
| **SERVER→CLIENT** | Replicated snapshot or event |
| **RPC-C→S** | Client request message |
| **RPC-S→C** | Server event message |
| **SPLIT** | Must separate simulation vs presentation |
| **DEPRECATE** | Remove or comment out for multiplayer |

### `main.cpp`

| Symbol | Current role | Target role | Refactor |
|--------|--------------|-------------|----------|
| `main()` | Entire game | Client entry only | Move sim to `GameSimulation`; server uses `server_main.cpp` |
| `GameState` enum | Local UI | Client UI states | Extend per §5 |
| `startGame()` | Reset session | `MatchStart` server event | Server resets; client reacts to `S_MATCH_START` |
| `spawnEnemy()` | Local spawn | **SERVER-SIM** | Move to `GameSimulation::SpawnEnemy` |
| `spawnPickup()` / `spawnPickupNearPlayer()` | Local | **SERVER-SIM** | Move to simulation |
| `registerKill()` | Score += 25 | **DEPRECATE** score; **SERVER-SIM** kill++ | Kill count only |
| `drawMinimap()` | Local | **LOCAL-RENDER** | Draw all players from snapshot |
| Score HUD | Passive score | **DEPRECATE** | Kill table on Tab |
| Title "Start Game" | Local start | **RPC-C→S** `C_JOIN` | Join queue |
| Game over screen | Local | **DEPRECATE** | `MATCH_END` + kick |
| Combat loop (sword/thunder) | Local hit test | **SPLIT** | Client: intent; Server: hit test |
| `simulate` flag | Pause sim | Client: predict? Server: always sim in match | — |

### `BaseCharacter`

| Method | Target | Notes |
|--------|--------|-------|
| `getWorldPos` / `setWorldPos` | **SERVER→CLIENT** | Replicated |
| `undoMovement` | **SERVER-SIM** | Collision resolution |
| `getCollisionRec` | **SERVER-SIM** | Hit detection on server |
| `update` | **SERVER-SIM** | Position integration |
| `render` | **LOCAL-RENDER** | Client only |
| `tick` | **SPLIT** | Sim on server; client interpolates for display |
| `getAlive` / `setAlive` | **SERVER-SIM** | |
| `tint` | **SERVER→CLIENT** | Color from `colorIndex` |

### `Character`

| Method | Target | Notes |
|--------|--------|-------|
| `tick` | **SPLIT** | Input read → **LOCAL-INPUT**; movement/combat → **SERVER-SIM** |
| `getScreenPos` | **LOCAL-RENDER** | Depends on camera owner (self or spectate target) |
| `getWeaponCollisionRec` | **SERVER-SIM** | Hit validation |
| `getPropCollisionRec` | **SERVER-SIM** | |
| `getHealth` | **SERVER→CLIENT** | |
| `takeDamage` | **SERVER-SIM** | |
| `heal` | **SERVER-SIM** | |
| `reset` | **SERVER-SIM** | On match start |
| `registerSwordHit` | **SERVER-SIM** | Charge logic |
| `isCharged` / `consumeCharge` | **SERVER-SIM** / **SERVER→CLIENT** | |
| `setVirtualInput` | **LOCAL-INPUT** | Web touch |
| `setBlockMouseAttack` | **LOCAL-INPUT** | |
| `updateWeaponPose` / `drawWeapon` | **LOCAL-RENDER** | Client |
| Constructor (loads textures) | **SPLIT** | `CharacterData` vs `CharacterRenderer` |

### `Enemy`

| Method | Target | Notes |
|--------|--------|-------|
| `tick` | **SERVER-SIM** | AI runs only on server |
| `setTarget(Character*)` | **DEPRECATE** | Replace with `setTargetPlayerId(uint8_t)` |
| `setObstacles` | **SERVER-SIM** | |
| `getScreenPos` | **LOCAL-RENDER** | Client uses snapshot + camera |
| `getWorldCenter` | **SERVER-SIM** / **SERVER→CLIENT** | Thunder targeting |
| `steerAroundProps` | **SERVER-SIM** | |
| `collidesWithProps` | **SERVER-SIM** | |

### `TileMap`

| Method | Target | Notes |
|--------|--------|-------|
| `generate` | **SERVER-SIM** | Seed from server at match start; replicate seed to clients |
| `isWaterAtWorld` | **SERVER-SIM** | Collision |
| `carveLand` | **SERVER-SIM** | Spawn clearing |
| `findRandomLandWorldPos` | **SERVER-SIM** | Pickup/enemy spawn |
| `draw` / `drawMinimap` / `exportPng` | **LOCAL-RENDER** | Client only |

### `Prop`

| Method | Target | Notes |
|--------|--------|-------|
| All | **SERVER-SIM** (collision) + **LOCAL-RENDER** (draw) | Static; generated from seed on both sides identically |

### `Pickup`

| Method | Target | Notes |
|--------|--------|-------|
| `collect` / `apply` | **SERVER-SIM** | Server validates overlap |
| `Render` | **LOCAL-RENDER** | |
| `isActive` | **SERVER→CLIENT** | |

### `Thunderstrike`

| Method | Target | Notes |
|--------|--------|-------|
| `tick` (hit frame) | **SERVER-SIM** | AoE damage |
| `Render` | **LOCAL-RENDER** | Client plays VFX from snapshot event |
| `justHit` | **SERVER-SIM** | |

### `ScoreBridge.h` / `NameInputBridge.h`

| Symbol | Target | Notes |
|--------|--------|-------|
| `SendScoreToParent` | **DEPRECATE** | Comment out |
| `HandlePlayerNameInput` | **LOCAL-INPUT** | Reuse on title screen (all platforms) |

### `TouchControls.h`

| Method | Target | Notes |
|--------|--------|-------|
| All | **LOCAL-INPUT** | Maps to `C_INPUT` fields |

### New networking types (to add)

| Symbol | Role |
|--------|------|
| `NetHost` / `NetPeer` | Transport wrapper |
| `GameSimulation::Tick` | **SERVER-SIM** |
| `GameSimulation::BuildSnapshot` | **SERVER→CLIENT** |
| `Client::ApplySnapshot` | **LOCAL-RENDER** interpolate |
| `Client::SendJoin` | **RPC-C→S** |
| `Client::SendReady` | **RPC-C→S** |
| `Client::SendInput` | **RPC-C→S** |
| `Server::OnJoin` | **RPC-C→S** handler |
| `Server::BroadcastMatchEnd` | **RPC-S→C** |

---

## 13. Refactoring Phases

### Phase 0 — Planning & scaffolding (this document)

- [x] Write `PLAN.md`
- [x] Ready-button rule: one Ready starts countdown at 2+ players
- [x] Web multiplayer deferred to Phase 5

### Phase 1 — Simulation extraction (no network yet)

**Goal:** `GameSimulation` runs the current game locally without raylib drawing.

1. [x] Create `GameSimulation.{h,cpp}` — owns players, enemies, pickups, strikes, props, tile map, timers.
2. [~] Split `Character` into logic + renderer — partial (`tick(simulate, drawSprite)` split; textures still in `Character`)
3. [x] Move spawn/combat/collision from `main.cpp` into simulation.
4. [x] Replace single `knight` with `std::vector<Player>` (max 10).
5. [ ] Unit-test: headless bot player (optional, not done).

**Exit criteria:** Desktop client still plays using `GameSimulation` embedded. **Met** (`--offline`).

### Phase 2 — New game rules (local)

**Goal:** All design changes work in offline/local multi before netcode.

1. [x] Main menu name + "Join Queue" UI.
2. [x] Kill count + Tab scoreboard; score/high score removed.
3. [x] Player color assignment from `kPlayerColors`.
4. [x] Spectate mode + prev/next buttons.
5. [x] Queue → ready → 5s countdown → 5 min match → 20s winner → menu.
6. [x] Enemy cap 200 + new targeting + group spawn.

**Exit criteria:** Two clients complete a match via server. **Met** (online mode).

### Phase 3 — Networking core

1. [x] ENet vendored in `thirdparty/enet/`; `NetServer` / `NetClient` wrappers.
2. [x] Join / server full / disconnect.
3. [x] Input packets → server; `S_WORLD_STATE` snapshot broadcast @ 30 Hz.
4. [x] Client renders from snapshots (`applyWorldStatePacket`); no local combat authority online.

**Exit criteria:** Server + 2 clients play one match. **Ready for testing.**

### Phase 4 — Headless dedicated server

1. [x] `server_main.cpp` + `tools/build_server.ps1` → `TopDownSurviveServer.exe`.
2. [~] Hidden raylib window (`FLAG_WINDOW_HIDDEN`) — loads textures; not fully headless stub.
3. [x] `tools/run_server.ps1`, assets copied to `dist-server/`.

### Phase 5 — Polish & web decision

1. [x] Interpolation for movement (players + enemies between 30 Hz snapshots).
2. [ ] Latency-compensated hit feel (optional).
3. [ ] Web: WebSocket relay or defer (deferred).
4. [ ] Remove deprecated `ScoreBridge` code.
5. [x] Thunder VFX sync on client (strikes in snapshot).
6. [x] Match-end flow: broadcast RESULTS phase + server kick to menu.
7. [x] Client-local spectate camera (no server round-trip).

---

## 14. Proposed File Structure

```
top-down-section-online/
├── PLAN.md
├── main.cpp                 # Client: window, UI, render loop
├── server_main.cpp          # Dedicated server entry
├── GameSimulation.{h,cpp}   # Authoritative game state + tick
├── GameConfig.h
├── PlayerSlot.h
├── NetCommon.{h,cpp}        # Packet enums + serialize
├── NetServer.{h,cpp}        # ENet host
├── NetClient.{h,cpp}        # ENet client
├── thirdparty/enet/         # Vendored ENet library
├── Character.{h,cpp}
├── Enemy.{h,cpp}
├── ... (existing files)
├── Makefile                 # ENET_CFLAGS, EXTRA_LIBS, server target
└── tools/
    ├── build_common.ps1
    ├── build_debug.ps1
    ├── build_release.ps1
    ├── build_server.ps1
    └── run_server.ps1
```

---

## 15. Build & Deployment

### Desktop client

```powershell
.\tools\build_debug.ps1      # debug + ENet
.\tools\build_release.ps1    # release client → dist/
```

### Dedicated server

```powershell
.\tools\build_server.ps1
# Output: dist-server/TopDownSurviveServer.exe + assets
```

### Run locally for development

```powershell
# Terminal 1
.\dist-server\TopDownSurviveServer.exe --port 27015

# Terminal 2+ (online)
.\TopDownSurvive.exe

# Solo without server (debug build)
.\TopDownSurvive.exe --offline
```

### Production

- Host `TopDownSurviveServer.exe` on a VPS (Windows or Linux with cross-compile).
- Open UDP port on firewall.
- Clients point to public IP:port (baked into `GameConfig.h` or config file).

### Web build

- **v1:** Single-player web build remains; multiplayer desktop-only.
- **v2:** WebSocket relay for WASM clients (document separately).

---

## 16. Open Questions & Risks

| Topic | Question | Recommendation |
|-------|----------|----------------|
| Ready button | One player ready or all must ready? | One ready starts countdown (faster matches) |
| Late join mid-match | Allow? | **No** — queue only between matches |
| PvP | Sword hit other players? | **No** — PvE only; kills from enemies |
| Cheat risk | Client sends aim position | Server validates thunder target vs charged state + range |
| Map sync | Same seed on all clients | Server sends `mapSeed` in `S_MATCH_START`; clients regenerate identically |
| `Character` textures on server | Constructors load PNGs today | Split logic/render before server build |
| Performance | 200 enemies × 10 players | Profile server tick; spatial hash if needed |
| Unity Netcode familiarity | Distributed authority vs dedicated | This plan is **server-authoritative** (like dedicated server topology in Netcode) |

---

## Enemy Targeting — Worked Example

Spawn at position `S`. Players A (near, 4 chasers), B (near, 1 chaser), C (far, 1 chaser).

```
score(A) = 100 * (1 + 0.75*4) = 400
score(B) = 120 * (1 + 0.75*1) = 210
score(C) = 400 * (1 + 0.75*1) = 700
```

Best target: **B** (lowest score). If B later gets 5 chasers, C can win — matching your intent.

---

## Summary Checklist

| Requirement | Plan section |
|-------------|--------------|
| Dedicated server, no lobbies | §2, §4 |
| One room, max 10, server full message | §2, §6 |
| Main menu name input | §6 |
| Join queue (not start game) | §6 |
| 2+ players → ready → 5s → start | §6, §10 |
| 5-minute match | §10 |
| No respawn, spectate + switch | §9 |
| Winner 20s → kick to menu | §10 |
| Random unique player colors | §7 |
| Kill count, Tab table; remove high score | §8 |
| Enemy cap 200 + targeting algorithm | §11 |
| Group spawn on random player | §11 |
| Method RPC audit | §12 |
| Headless server setup | §4, §15 |

---

*Document version: 1.1 — Phases 1–4 implemented; networking + dedicated server added.*
