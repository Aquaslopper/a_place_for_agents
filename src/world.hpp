// world.hpp — procedural JRPG world, Carcassonne plate overworld, and map transitions
#pragma once

#include "game.hpp"
#include <cstdint>
#include <string>
#include <vector>

// Overworld plate dimensions: each plate is 3x3 tiles (48x48 px at TILE=16).
constexpr int PLATE_TILES = 3;
constexpr int OW_REGION_COLS = 16;   // plates per overworld region (width)
constexpr int OW_REGION_ROWS = 12;   // plates per overworld region (height)
constexpr int OW_REGION_TILES_W = OW_REGION_COLS * PLATE_TILES; // 48
constexpr int OW_REGION_TILES_H = OW_REGION_ROWS * PLATE_TILES; // 36

// Key story / landmark coordinates (in tile space)
constexpr int BOSS_CASTLE_TX = 120;
constexpr int BOSS_CASTLE_TY = -84;
constexpr float LOCKET_PICKUP_X = -36.5f;
constexpr float LOCKET_PICKUP_Y = 28.5f;

enum OwTerrain {
    OW_GRASS = 0,
    OW_WATER,
    OW_FOREST,
    OW_SAND,
    OW_SNOW,
    OW_DIRT,
    OW_MOUNTAIN,
    OW_TERRAIN_COUNT
};

enum OwSiteKind {
    OW_SITE_NONE = 0,
    OW_SITE_TOWN,
    OW_SITE_CASTLE,
    OW_SITE_DUNGEON,
    OW_SITE_TEMPLE,
    OW_SITE_TOWER,
    OW_SITE_RUINS,
    OW_SITE_CAMP,
    OW_SITE_BRIDGE,
    OW_SITE_CAVE_PASS,
    OW_SITE_GATE_PASS
};

struct OwPlate {
    int plateId = 0;            // index into plate registry
    std::string name;           // e.g. "grass_plain", "town_red", "water_vchannel"
    OwTerrain terrain = OW_GRASS;
    OwSiteKind site = OW_SITE_NONE;
    bool isTransporter = false;
    uint8_t mask[3][3] = {};    // 1 = terrain, 0 = grass
    OwTerrain edges[4][3] = {}; // N, E, S, W edge labels (3 cells each)
};

struct OwRegion {
    int rx = 0;
    int ry = 0;
    uint32_t seed = 0;
    std::vector<OwPlate> plates; // OW_REGION_COLS * OW_REGION_ROWS
};

// Compatibility structs for render.cpp / regions.cpp / atlas.cpp
struct SurveyTerrain {
    bool water = false;
    bool deep = false;
    bool crag = false;
    bool ice = false;
    bool forest = false;
    bool sand = false;
    bool road = false;
    OwTerrain terrain = OW_GRASS;
};

struct LandmarkPart {
    int tx = 0;
    int ty = 0;
    int spanW = 1;
    int spanH = 1;
    int kind = 0;
    int spriteId = 0;
};

// World lifecycle & queries
void worldInit(uint32_t seed);
void worldReset(uint32_t seed);
const OwRegion& worldGetRegion(int rx, int ry);
const OwPlate& worldGetPlateAtTile(int tx, int ty);
OwTerrain worldTerrainAt(int tx, int ty);
bool worldTileWalkable(int tx, int ty);
bool worldTileIsWater(int tx, int ty);
OwSiteKind worldSiteAt(int tx, int ty);

// Transporter / map transition helpers
bool worldEnterSiteAt(int tx, int ty);
void worldReturnToOverworld();

// Compatibility helpers expected by existing modules
SurveyTerrain worldSurveyTerrain(int tx, int ty);
bool worldNearSpecialSite(int tx, int ty);
int worldTrailAt(int tx, int ty);
Landmark northRoadCavern();
std::string compassHint(float fromX, float fromY, float toX, float toY);
int buildingDoorTx(const Building& b);
int buildingDoorTy(const Building& b);

// Tile & chunk helpers
int tileAt(int tx, int ty);
bool isTileSolid(int tx, int ty);
void ensureChunksAround(int tx, int ty);
void updateWorldStreaming(float dt);
