#include "TileMap.h"
#include <raymath.h>
#include <cmath>

namespace
{
    // Dual-grid corner mask bit layout: TL=8, TR=4, BL=2, BR=1 (bit set = the
    // foreground terrain occupies that corner). Each table maps the 16 patterns
    // to a source tile (col, row) in RPG Nature Tileset.png. Verified visually.

    // Base layer: foreground = WATER over a grass background (opaque tiles).
    const int WATER_TABLE[16][2] = {
        {0, 4},   // 0000 all grass
        {17, 7},  // 0001 water nub BR
        {18, 7},  // 0010 water nub BL
        {11, 11}, // 0011 water along bottom
        {17, 10}, // 0100 water nub TR
        {15, 7},  // 0101 water along right
        {2, 4},   // 0110 saddle -> deep water
        {15, 11}, // 0111 grass nub TL
        {18, 10}, // 1000 water nub TL
        {2, 4},   // 1001 saddle -> deep water
        {10, 7},  // 1010 water along left
        {10, 11}, // 1011 grass nub TR
        {11, 6},  // 1100 water along top
        {15, 6},  // 1101 grass nub BL
        {10, 6},  // 1110 grass nub BR
        {2, 4},   // 1111 all water
    };

    // Overlay layer: foreground = DIRT over a transparent background. Pattern
    // 0000 draws nothing. Edges/corners are feathered so they blend over the base.
    const int DIRT_TABLE[16][2] = {
        {-1, -1}, // 0000 nothing
        {10, 13}, // 0001 dirt nub BR
        {15, 13}, // 0010 dirt nub BL
        {11, 13}, // 0011 dirt along bottom
        {10, 16}, // 0100 dirt nub TR
        {10, 14}, // 0101 dirt along right
        {4, 4},   // 0110 saddle -> solid dirt
        {18, 16}, // 0111 inner corner (gap TL)
        {15, 16}, // 1000 dirt nub TL
        {4, 4},   // 1001 saddle -> solid dirt
        {15, 14}, // 1010 dirt along left
        {17, 16}, // 1011 inner corner (gap TR)
        {11, 16}, // 1100 dirt along top
        {18, 13}, // 1101 inner corner (gap BL)
        {17, 13}, // 1110 inner corner (gap BR)
        {4, 4},   // 1111 solid dirt
    };

    // Solid-fill variants used to break up large flat areas of one terrain.
    const int GRASS_VARIANTS[4][2] = {{0, 4}, {1, 4}, {0, 5}, {1, 5}};
    const int WATER_VARIANTS[4][2] = {{2, 4}, {3, 4}, {2, 5}, {3, 5}};
    const int DIRT_VARIANTS[4][2] = {{4, 4}, {5, 4}, {4, 5}, {5, 5}};

    int hash2(int i, int j)
    {
        unsigned int h = (unsigned int)i * 73856093u ^ (unsigned int)j * 19349663u;
        h ^= h >> 13;
        return (int)(h & 0x7fffffff);
    }
}

TileMap::TileMap(int cols, int rows, float scale)
    : cols{cols}, rows{rows}, scale{scale}
{
    tileset = LoadTexture("map_tileset/RPG Nature Tileset.png");
    water.assign((size_t)cols * rows, 0);
    dirt.assign((size_t)cols * rows, 0);
}

bool TileMap::waterAt(int c, int r) const
{
    if (c < 0 || r < 0 || c >= cols || r >= rows)
        return true; // map is ringed by water
    return water[(size_t)r * cols + c] != 0;
}

bool TileMap::dirtAt(int c, int r) const
{
    if (c < 0 || r < 0 || c >= cols || r >= rows)
        return false;
    return dirt[(size_t)r * cols + c] != 0;
}

void TileMap::runCellularAutomata(std::vector<unsigned char> &grid, int fillPercent,
                                  int passes, bool borderValue)
{
    const int border = 2;
    // seed random noise, keeping a forced border value
    for (int r = 0; r < rows; r++)
    {
        for (int c = 0; c < cols; c++)
        {
                bool edge = c < border || r < border ||
                            c >= cols - border || r >= rows - border;
                unsigned char v = edge ? (borderValue ? 1 : 0)
                                       : (unsigned char)(GetRandomValue(0, 99) < fillPercent);
                grid[(size_t)r * cols + c] = v;
        }
    }

    // "cave" smoothing: a cell turns on with 5+ on-neighbours, off with 3 or
    // fewer, and stays put in between. Repeating grows organic rounded blobs.
    auto readGrid = [&](int c, int r) -> int
    {
        if (c < 0 || r < 0 || c >= cols || r >= rows)
            return borderValue ? 1 : 0;
        return grid[(size_t)r * cols + c];
    };
    std::vector<unsigned char> next = grid;
    for (int pass = 0; pass < passes; pass++)
    {
        for (int r = 0; r < rows; r++)
        {
            for (int c = 0; c < cols; c++)
            {
                int on = 0;
                for (int dr = -1; dr <= 1; dr++)
                    for (int dc = -1; dc <= 1; dc++)
                    {
                        if (dc == 0 && dr == 0)
                            continue;
                        on += readGrid(c + dc, r + dr);
                    }
                bool edge = c < border || r < border ||
                            c >= cols - border || r >= rows - border;
                unsigned char v;
                if (edge)
                    v = borderValue ? 1 : 0;
                else if (on >= 5)
                    v = 1;
                else if (on <= 3)
                    v = 0;
                else
                    v = grid[(size_t)r * cols + c];
                next[(size_t)r * cols + c] = v;
            }
        }
        grid.swap(next);
    }
}

void TileMap::generate(unsigned int seed)
{
    SetRandomSeed(seed);
    // water lakes: ringed border, ~45% interior fill
    runCellularAutomata(water, 45, 4, /*borderValue=*/true);
    // drop disconnected islands so all land is one reachable mass
    removeIslands();
    // dirt patches: no border, sparser fill so they read as patches not a sea
    runCellularAutomata(dirt, 30, 3, /*borderValue=*/false);
    // keep dirt off the water (and away from shorelines so edges don't bleed)
    clearDirtNearWater();
}

void TileMap::removeIslands()
{
    std::vector<int> comp((size_t)cols * rows, -1);
    std::vector<int> stack;
    int bestComp = -1;
    int bestSize = 0;
    int nextComp = 0;

    for (int r = 0; r < rows; r++)
    {
        for (int c = 0; c < cols; c++)
        {
            size_t start = (size_t)r * cols + c;
            if (water[start] != 0 || comp[start] != -1)
                continue;

            int id = nextComp++;
            int size = 0;
            stack.push_back((int)start);
            comp[start] = id;
            while (!stack.empty())
            {
                int idx = stack.back();
                stack.pop_back();
                size++;
                int cc = idx % cols;
                int cr = idx / cols;
                const int dx[4] = {1, -1, 0, 0};
                const int dy[4] = {0, 0, 1, -1};
                for (int k = 0; k < 4; k++)
                {
                    int nc = cc + dx[k];
                    int nr = cr + dy[k];
                    if (nc < 0 || nr < 0 || nc >= cols || nr >= rows)
                        continue;
                    size_t ni = (size_t)nr * cols + nc;
                    if (water[ni] == 0 && comp[ni] == -1)
                    {
                        comp[ni] = id;
                        stack.push_back((int)ni);
                    }
                }
            }
            if (size > bestSize)
            {
                bestSize = size;
                bestComp = id;
            }
        }
    }

    // flood every smaller landmass with water
    for (size_t i = 0; i < water.size(); i++)
        if (water[i] == 0 && comp[i] != bestComp)
            water[i] = 1;
}

void TileMap::clearDirtNearWater()
{
    std::vector<unsigned char> cleaned = dirt;
    for (int r = 0; r < rows; r++)
    {
        for (int c = 0; c < cols; c++)
        {
            if (dirt[(size_t)r * cols + c] == 0)
                continue;
            bool nearWater = false;
            for (int dr = -1; dr <= 1 && !nearWater; dr++)
                for (int dc = -1; dc <= 1; dc++)
                    if (waterAt(c + dc, r + dr))
                    {
                        nearWater = true;
                        break;
                    }
            if (nearWater)
                cleaned[(size_t)r * cols + c] = 0;
        }
    }
    dirt.swap(cleaned);
}

int TileMap::waterMask(int i, int j) const
{
    // corners of the dual-grid tile at corner (i, j)
    int tl = waterAt(i - 1, j - 1) ? 1 : 0;
    int tr = waterAt(i, j - 1) ? 1 : 0;
    int bl = waterAt(i - 1, j) ? 1 : 0;
    int br = waterAt(i, j) ? 1 : 0;
    return (tl << 3) | (tr << 2) | (bl << 1) | br;
}

int TileMap::dirtMask(int i, int j) const
{
    int tl = dirtAt(i - 1, j - 1) ? 1 : 0;
    int tr = dirtAt(i, j - 1) ? 1 : 0;
    int bl = dirtAt(i - 1, j) ? 1 : 0;
    int br = dirtAt(i, j) ? 1 : 0;
    return (tl << 3) | (tr << 2) | (bl << 1) | br;
}

void TileMap::draw(Vector2 cameraWorldPos) const
{
    const float ts = tileWorldSize();

    int firstI = (int)(cameraWorldPos.x / ts) - 1;
    int firstJ = (int)(cameraWorldPos.y / ts) - 1;
    int lastI = firstI + (int)(GetScreenWidth() / ts) + 3;
    int lastJ = firstJ + (int)(GetScreenHeight() / ts) + 3;
    if (firstI < 0) firstI = 0;
    if (firstJ < 0) firstJ = 0;
    if (lastI > cols) lastI = cols;
    if (lastJ > rows) lastJ = rows;

    // dual-grid render tiles are centred on cell corners (offset by -0.5 tile)
    for (int j = firstJ; j <= lastJ; j++)
    {
        for (int i = firstI; i <= lastI; i++)
        {
            float dx = (i - 0.5f) * ts - cameraWorldPos.x;
            float dy = (j - 0.5f) * ts - cameraWorldPos.y;
            // +1px on dest hides seams between scaled tiles
            Rectangle dest{dx, dy, ts + 1.f, ts + 1.f};

            // base water/grass, with varied solid fills
            int wm = waterMask(i, j);
            int wx, wy;
            if (wm == 0)
            {
                const int *v = GRASS_VARIANTS[hash2(i, j) & 3];
                wx = v[0]; wy = v[1];
            }
            else if (wm == 15)
            {
                const int *v = WATER_VARIANTS[hash2(i, j) & 3];
                wx = v[0]; wy = v[1];
            }
            else
            {
                wx = WATER_TABLE[wm][0]; wy = WATER_TABLE[wm][1];
            }
            Rectangle wsrc{(float)(wx * TILE_SRC), (float)(wy * TILE_SRC),
                           (float)TILE_SRC, (float)TILE_SRC};
            DrawTexturePro(tileset, wsrc, dest, Vector2{0, 0}, 0.f, WHITE);

            // dirt overlay
            int dm = dirtMask(i, j);
            if (dm != 0)
            {
                int dxi, dyi;
                if (dm == 15)
                {
                    const int *v = DIRT_VARIANTS[hash2(i, j) & 3];
                    dxi = v[0]; dyi = v[1];
                }
                else
                {
                    dxi = DIRT_TABLE[dm][0]; dyi = DIRT_TABLE[dm][1];
                }
                Rectangle dsrc{(float)(dxi * TILE_SRC), (float)(dyi * TILE_SRC),
                               (float)TILE_SRC, (float)TILE_SRC};
                DrawTexturePro(tileset, dsrc, dest, Vector2{0, 0}, 0.f, WHITE);
            }
        }
    }
}

void TileMap::cellAtWorld(Vector2 worldPos, int &c, int &r) const
{
    const float ts = tileWorldSize();
    // shift by half a tile so collision matches the dual-grid visuals
    c = (int)floorf(worldPos.x / ts - 0.5f);
    r = (int)floorf(worldPos.y / ts - 0.5f);
}

bool TileMap::isWaterAtWorld(Vector2 worldPos) const
{
    int c, r;
    cellAtWorld(worldPos, c, r);
    return waterAt(c, r);
}

Vector2 TileMap::findRandomLandWorldPos(float margin) const
{
    for (int attempt = 0; attempt < 200; attempt++)
    {
        float x = (float)GetRandomValue((int)margin, (int)(worldWidth() - margin));
        float y = (float)GetRandomValue((int)margin, (int)(worldHeight() - margin));
        if (!isWaterAtWorld(Vector2{x, y}))
            return Vector2{x, y};
    }
    return Vector2{worldWidth() / 2.f, worldHeight() / 2.f};
}

void TileMap::carveLand(Vector2 worldCenter, float radius)
{
    const float ts = tileWorldSize();
    int cc, cr;
    cellAtWorld(worldCenter, cc, cr);
    int tileRadius = (int)(radius / ts) + 1;
    for (int r = cr - tileRadius; r <= cr + tileRadius; r++)
    {
        for (int c = cc - tileRadius; c <= cc + tileRadius; c++)
        {
            if (c < 0 || r < 0 || c >= cols || r >= rows)
                continue;
            float dx = (float)(c - cc);
            float dy = (float)(r - cr);
            if (dx * dx + dy * dy <= (float)tileRadius * tileRadius)
            {
                water[(size_t)r * cols + c] = 0;
                dirt[(size_t)r * cols + c] = 0;
            }
        }
    }
}

void TileMap::exportPng(const char *path) const
{
    Image sheet = LoadImage("map_tileset/RPG Nature Tileset.png");
    Image out = GenImageColor(cols * TILE_SRC, rows * TILE_SRC, BLANK);

    for (int j = 0; j <= rows; j++)
    {
        for (int i = 0; i <= cols; i++)
        {
            Rectangle dst{(i - 0.5f) * TILE_SRC, (j - 0.5f) * TILE_SRC,
                          (float)TILE_SRC, (float)TILE_SRC};

            int wm = waterMask(i, j);
            int wx, wy;
            if (wm == 0) { wx = GRASS_VARIANTS[hash2(i, j) & 3][0]; wy = GRASS_VARIANTS[hash2(i, j) & 3][1]; }
            else if (wm == 15) { wx = WATER_VARIANTS[hash2(i, j) & 3][0]; wy = WATER_VARIANTS[hash2(i, j) & 3][1]; }
            else { wx = WATER_TABLE[wm][0]; wy = WATER_TABLE[wm][1]; }
            Rectangle wsrc{(float)(wx * TILE_SRC), (float)(wy * TILE_SRC), (float)TILE_SRC, (float)TILE_SRC};
            ImageDraw(&out, sheet, wsrc, dst, WHITE);

            int dm = dirtMask(i, j);
            if (dm != 0)
            {
                int dxi, dyi;
                if (dm == 15) { dxi = DIRT_VARIANTS[hash2(i, j) & 3][0]; dyi = DIRT_VARIANTS[hash2(i, j) & 3][1]; }
                else { dxi = DIRT_TABLE[dm][0]; dyi = DIRT_TABLE[dm][1]; }
                Rectangle dsrc{(float)(dxi * TILE_SRC), (float)(dyi * TILE_SRC), (float)TILE_SRC, (float)TILE_SRC};
                ImageDraw(&out, sheet, dsrc, dst, WHITE);
            }
        }
    }

    ExportImage(out, path);
    UnloadImage(out);
    UnloadImage(sheet);
}

void TileMap::drawMinimap(Rectangle bounds) const
{
    float cellW = bounds.width / (float)cols;
    float cellH = bounds.height / (float)rows;

    for (int r = 0; r < rows; r++)
    {
        for (int c = 0; c < cols; c++)
        {
            if (waterAt(c, r))
                continue;

            Color landColor = dirtAt(c, r) ? Fade(BROWN, 0.75f) : Fade(DARKGREEN, 0.85f);
            DrawRectangle(
                (int)(bounds.x + c * cellW),
                (int)(bounds.y + r * cellH),
                (int)ceilf(cellW),
                (int)ceilf(cellH),
                landColor);
        }
    }

    DrawRectangleLinesEx(bounds, 1.f, Fade(RAYWHITE, 0.6f));
}
