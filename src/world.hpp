// world.hpp — procedural infinite world for "A Place for Agents"
#pragma once

#include "game.hpp"

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
