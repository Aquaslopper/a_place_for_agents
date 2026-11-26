// world.hpp — procedural infinite world for "A Place for Agents"
#pragma once

#include "game.hpp"
#include <vector>
#include <cstdint>

// tile kinds used by the renderer and movement
enum TileKind : uint8_t {
    TK_WATER = 0, TK_SAND, TK_GRASS, TK_SNOW, TK_ROCK, TK_ICE, TK_COBBLE
};
struct TileInfo {
    TileKind kind = TK_GRASS;
    uint8_t variant = 0;      // texture variant for grass-like tiles
    uint8_t deco = 0;         // 0 none, 1 tuft, 2 flower
    uint8_t decoVariant = 0;
};

TileInfo sampleTile(int tx, int ty);
float moisture(float x, float y, uint32_t seed);
bool tileWalkable(int tx, int ty);
bool inSpawnSafeZone(float x, float y);

// villages
void   villageLoadFor(float x, float y);          // fills g.village when near one
bool   villageContains(float x, float y);          // inside plaza/buildings area
bool   villageInteractAt(int tx, int ty, int* outRole, Village* outVillage);
std::string villageNameFor(uint32_t sx, uint32_t sy, uint32_t seed);

// walkability --------------------------------------------------------------
// The one true collision test for the overworld. Houses, wells, fences and the
// wild-object decoration are all evaluated here so that the renderer, the
// click-to-walk pathfinder and the movement code can never disagree about
// where a building really is.
bool cellWalkable(int tx, int ty);                   // tile is walkable at all
bool positionBlocked(float x, float y, float r);     // circle vs. world

// navigation ---------------------------------------------------------------
// Grid pathfinding for mouse control. A path is a list of tile centers the
// hero walks through in order; the last cell may be a tile the player clicked.
struct WalkPath {
    std::vector<std::pair<int,int>> cells;
    int   doorHouse = -1;      // >= 0: stop in front of this house and interact
    bool  reachable = false;
    size_t cursor = 0;
    void clear() { cells.clear(); doorHouse = -1; reachable = false; cursor = 0; }
};
// Finds a route from `from` to tile (toTx,toTy). If the target tile itself is
// occupied (a house, water, a well...) the nearest standable neighbour is used
// and the adjacent door is recorded in doorHouse so the player can interact
// with a single click. Search is a bounded A*; long clicks are clamped.
bool findWalkPath(float fromX, float fromY, int toTx, int toTy, WalkPath& out);
// Nearest walkable tile to (x,y) — used when a click lands on scenery.
void nearestWalkable(int tx, int ty, int& outTx, int& outTy);

// field monsters
struct FieldMonster {
    uint64_t id = 0;
    EnemyKind kind = EN_SLIME;
    float x = 0, y = 0;       // tile coords
    float anchorX = 0, anchorY = 0;
    float phase = 0;
    int sectorX = 0, sectorY = 0;
};
void fieldMonstersInView(float x, float y, std::vector<FieldMonster>& out);
int  zoneLevelAt(float x, float y);
const char* zoneNameAt(float x, float y);          // flavor name for HUD
EnemyKind rollBiomeEnemy(int tx, int ty);
