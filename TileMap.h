#ifndef TILEMAP_H
#define TILEMAP_H

#include <raylib.h>
#include <vector>

// TileMap slices a single tileset image (RPG Nature Tileset.png) into 16x16 px
// source tiles and procedurally builds a grass / water / dirt level from them.
//
// Rendering uses "dual-grid" (marching-squares) autotiling: each drawn tile sits
// on the corner intersection of four logical cells and is chosen from the 16
// possible corner patterns. This makes shorelines/edges follow the land on every
// side with no one-sided artifacts, and handles channels and diagonals cleanly.
//
//   * Layer 0 (base): grass vs water, drawn opaque.
//   * Layer 1 (overlay): dirt, drawn with feathered transparent edges on top of
//     the base so dirt blends into both grass and water for free.
//
// Tiles are drawn scaled (see `scale`), so a 16px source tile becomes 16*scale
// pixels on screen. The rest of the game uses 4x, keeping everything consistent.
class TileMap
{
public:
    // Source tile size in the png. The RPG Nature Tileset is 16x16 per tile.
    static constexpr int TILE_SRC = 16;

    TileMap(int cols, int rows, float scale);

    // Builds the water lakes + dirt patches and is deterministic for a given
    // `seed`. The outer edge of the map is always water (a natural boundary).
    void generate(unsigned int seed);

    // Draws every visible tile, offset so `cameraWorldPos` is the world point at
    // the top-left of the screen (matches how the game positions the world).
    void draw(Vector2 cameraWorldPos) const;

    float tileWorldSize() const { return TILE_SRC * scale; }
    float worldWidth() const { return cols * tileWorldSize(); }
    float worldHeight() const { return rows * tileWorldSize(); }

    // True when the given world position falls on water (used for collision).
    bool isWaterAtWorld(Vector2 worldPos) const;

    // Random land (non-water) world position, `margin` px away from the edges.
    Vector2 findRandomLandWorldPos(float margin) const;

    // Clears water and dirt within `radius` px of `worldCenter` (clean spawn).
    void carveLand(Vector2 worldCenter, float radius);

    // Writes the whole resolved map to a PNG (native 16px per tile) so the full
    // generated level can be inspected at once.
    void exportPng(const char *path) const;

    // Top-down land outline for the HUD minimap (transparent where water).
    void drawMinimap(Rectangle bounds) const;

private:
    int cols{};
    int rows{};
    float scale{};
    Texture2D tileset{};

    std::vector<unsigned char> water; // 1 = water, 0 = land, per logical cell
    std::vector<unsigned char> dirt;  // 1 = dirt overlay present, per logical cell

    // Cell readers. Out-of-bounds water reads as water (so the map is ringed by
    // water); out-of-bounds dirt reads as absent.
    bool waterAt(int c, int r) const;
    bool dirtAt(int c, int r) const;

    // Logical cell under a world position, shifted by half a tile so collision
    // lines up with the dual-grid visuals.
    void cellAtWorld(Vector2 worldPos, int &c, int &r) const;

    // Marching-squares corner mask for the dual-grid tile at corner (i, j).
    int waterMask(int i, int j) const;
    int dirtMask(int i, int j) const;

    void runCellularAutomata(std::vector<unsigned char> &grid, int fillPercent,
                             int passes, bool borderValue);

    // Floods every landmass except the largest with water, so there are no
    // unreachable islands.
    void removeIslands();

    // Removes dirt from any cell that is water or borders water, keeping dirt
    // (and its feathered edges) strictly on land.
    void clearDirtNearWater();
};

#endif // TILEMAP_H
