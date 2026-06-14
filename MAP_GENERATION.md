# Map Generation

This document explains how the tile world in `TileMap.cpp` is generated and
rendered, and which parts are well-known algorithms versus project-specific glue.

## TL;DR

The map is **not** a single novel algorithm. It is a small **pipeline of three
established techniques**, wired together for this game:

1. **Cellular-automata "cave" generation** — to grow organic water and dirt blobs.
2. **Connected-component flood fill (BFS)** — to delete unreachable islands.
3. **Dual-grid marching-squares autotiling** — to pick the correct shoreline/edge
   tile for every cell so transitions look continuous.

Everything is driven from one tileset image (`map_tileset/RPG Nature Tileset.png`)
sliced into 16x16 px tiles. Nothing is hand-authored; a `seed` makes a run
reproducible.

---

## The data model

The world is two boolean grids of `cols x rows` logical cells:

- `water[c][r]` — 1 = water, 0 = land
- `dirt[c][r]`  — 1 = dirt overlay present (always on land)

Grass is simply "land with no dirt", so it needs no flag.

---

## Step 1 — Cellular-automata "cave" generation

*Known algorithm.* This is the classic **cellular automata cave generation**
popularised by the RogueBasin article *"Cellular Automata Method for Generating
Random Cave-Like Levels"* and used in many roguelikes. It is run once for water
and once for dirt (`runCellularAutomata`).

1. **Seed noise.** Each interior cell is randomly set on with probability
   `fillPercent` (45% for water, 30% for dirt). The outer 2-cell border is forced
   to a fixed value — water uses a forced *on* border so the map is ringed by
   water; dirt uses a forced *off* border.
2. **Smoothing passes.** A few iterations (4 for water, 3 for dirt) apply the
   standard "4-5 rule":
   - count the 8 neighbours that are "on";
   - `>= 5` on  -> the cell becomes on;
   - `<= 3` on  -> the cell becomes off;
   - otherwise  -> the cell keeps its current value.

   Repeating this erases lone speckles and grows smooth, rounded blobs, turning
   white noise into natural-looking lakes and dirt patches.

Tuning knobs live in `TileMap::generate`:
`runCellularAutomata(water, 45, 4, true)` and
`runCellularAutomata(dirt, 30, 3, false)`.

## Step 2 — Remove islands (connected components)

*Known algorithm.* `removeIslands()` is a standard **connected-component
labelling** done with an iterative **flood fill (BFS/DFS)** over 4-connected land
cells:

1. Walk every land cell; when an unvisited one is found, flood-fill its whole
   region and record the region's size.
2. Keep the **largest** region; flood every smaller region back to water.

This guarantees all walkable land is one connected mass, so the player can never
be stranded on (or stare at) an unreachable island.

## Step 3 — Keep dirt off the water

*Project-specific cleanup (trivial).* `clearDirtNearWater()` removes dirt from any
cell that is water or 8-neighbours water. Because dirt is drawn with feathered
edges (see below), this buffer stops those edges from bleeding over the shoreline.

---

## Rendering — Dual-grid marching-squares autotiling

*Known technique.* The tricky part of any tile map is choosing the right edge and
corner tiles so terrains blend instead of showing hard squares. This project uses
the **dual grid** method (a.k.a. corner-based / marching-squares autotiling).

Key idea: instead of drawing one sprite per logical cell, we draw a grid that is
**offset by half a tile**, so each rendered tile sits on the **corner where four
logical cells meet**. That rendered tile is chosen by looking at those four
corners:

- Each corner is either foreground or background -> 4 bits -> **16 possible
  patterns** (`TL=8, TR=4, BL=2, BR=1`).
- A 16-entry lookup table maps each pattern to a source tile in the sheet
  (`WATER_TABLE` and `DIRT_TABLE` in `TileMap.cpp`).

Why dual-grid instead of the naive "look at my 4 edge neighbours" approach: the
naive method cannot represent a 1-tile-wide channel or a diagonal touch and ends
up drawing foam on only one side. Being corner-based, dual-grid resolves every
edge, convex corner, **and concave corner** consistently, with no one-sided
artifacts.

The map is drawn in two layers:

1. **Base layer** — water vs. grass, drawn opaque using `WATER_TABLE`.
2. **Dirt overlay** — drawn on top using `DIRT_TABLE`, whose edge tiles have a
   **transparent feathered border**. Because it is transparent on the outside,
   the dirt blends naturally over whatever is underneath, which is what gives
   dirt its soft edge against grass.

Large flat areas of a single terrain are de-duplicated by randomly picking among
four solid-fill variants (`GRASS_VARIANTS`, `WATER_VARIANTS`, `DIRT_VARIANTS`),
chosen with a small position hash so it stays stable frame to frame.

A couple of practical details:

- **Collision** (`isWaterAtWorld`) samples the logical grid shifted by half a tile
  so it lines up with the half-tile-offset visuals.
- **Saddle patterns** (two opposite corners are water, the other two land) have no
  dedicated tile in this sheet, so they fall back to a solid fill. They are rare
  after the smoothing passes.

---

## So: known or original?

**Known algorithms, original combination.** Every individual stage is a textbook
technique:

| Stage | Technique | Origin |
| --- | --- | --- |
| Terrain shapes | Cellular-automata cave generation (4-5 rule) | Classic roguelike technique |
| Island removal | Connected-component flood fill (BFS) | Classic graph/grid algorithm |
| Tile selection | Dual-grid marching-squares autotiling | Well-known tilemap technique |

What is specific to this project is the **pipeline and the tile mappings**: the
order of the stages, the parameters, the water-ringed border, the dirt-as-a
transparent-overlay layer, and the hand-derived 16-entry lookup tables that match
*this* particular tileset. There is no novel generation algorithm being claimed.

## Where to look in the code

- `TileMap::generate` — the full pipeline.
- `TileMap::runCellularAutomata` — step 1.
- `TileMap::removeIslands` — step 2.
- `TileMap::clearDirtNearWater` — step 3.
- `TileMap::draw` / `WATER_TABLE` / `DIRT_TABLE` — dual-grid rendering.
- `TileMap::exportPng` — dumps the whole map to `generated_map.png` for inspection.
