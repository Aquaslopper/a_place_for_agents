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

// Key story / landmark coordinates (in tile space). The fixed locket site
// sits beside its pickup, while the watchtower keeps the coordinates used by
// the earlier authored world. The field boss stands at the castle's south gate.
constexpr int BOSS_CASTLE_TX = 120;
constexpr int BOSS_CASTLE_TY = -84;
constexpr float BOSS_KING_X = BOSS_CASTLE_TX + 0.5f;
constexpr float BOSS_KING_Y = BOSS_CASTLE_TY + 3.5f;
constexpr int LOCKET_SITE_TX = -37;
constexpr int LOCKET_SITE_TY = 28;
constexpr float LOCKET_PICKUP_X = -36.5f;
constexpr float LOCKET_PICKUP_Y = 28.5f;
constexpr int TOWER_SITE_TX = -64;
constexpr int TOWER_SITE_TY = -20;
constexpr int TOWER_CHEST_TX = -64;
constexpr int TOWER_CHEST_TY = -18;

inline int buildingX0(const Building& b) { return b.tx - b.w / 2; }
inline int buildingY0(const Building& b) { return b.ty - b.h + 1; }
inline float buildingCenterX(const Building& b) {
    return (buildingX0(b) + b.w * 0.5f) * static_cast<float>(TILE);
}

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

// Cheap terrain summary shared by site placement, regions and the Survey.
// These readings are derived from the same seeded atlas cell as the renderer;
// no second random/noise field is introduced here.
struct SurveyTerrain {
    float elevation = 0.0f;
    float brookDistance = 99.0f;
    Biome biome = BI_MEADOW;
    bool water = false;
    bool deep = false;
    bool crag = false;
    bool ice = false;
    bool swamp = false;
    bool forest = false;
    bool sand = false;
    bool road = false;
    OwTerrain terrain = OW_GRASS;
};

// Village decoration kinds shared by generation, collision and rendering.
enum VillageDeco {
    VD_WELL = 0, VD_SIGN, VD_BENCH, VD_TORCH, VD_CAMPFIRE, VD_BARREL, VD_PLANT,
    VD_FLOWERBED, VD_LADDER, VD_STATUE, VD_CAULDRON, VD_URN, VD_FENCE_H, VD_FENCE_V,
    VD_GATE, VD_TEMPLE_PILLAR, VD_HOUSE, VD_CRATE, VD_TABLE, VD_BUCKET, VD_SHADE_TREE,
    VD_MAILBOX, VD_NOTICEBOARD, VD_LONGTABLE, VD_FENCE_GATE,
    VD_BUSH, VD_FLOWER_BUSH, VD_ROCK, VD_STUMP, VD_PINE,
    VD_STATUS_STONE, VD_EMPTY_PLOT, VD_KIND_COUNT
};
void villageDecoTexture(int kind, int variant, Texture2D*& tex, int& w, int& h);
float villageDecoCenterX(const Village::Deco& d);
bool villageDecoSolid(int kind);

// Individual landmark sub-pieces are Y-sorted independently.
enum LandmarkPartKind {
    LP_CASTLE = 0, LP_TOWER, LP_STATUE, LP_PILLAR, LP_TORCH,
    LP_RUIN_ARCH, LP_MONUMENT, LP_TENT, LP_CAMPFIRE, LP_CAULDRON, LP_BARREL,
    LP_CAVE, LP_TIDEWELL
};
struct LandmarkPart {
    LandmarkPartKind part = LP_CASTLE;
    int tx = 0;
    int ty = 0;
    int spanW = 1;
    int spanH = 1;
    int spriteId = 0;
};
void expandLandmarkParts(const Landmark& lm, std::vector<LandmarkPart>& out);
void landmarksInView(int x0, int y0, int x1, int y1, std::vector<Landmark>& out);
std::string villageNameFor(int sx, int sy, uint32_t seed);

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
void worldDrawOverworld();
void worldDrawOverviewMap();
const char* worldSitePrompt(int tx, int ty);

// Compatibility helpers expected by existing modules
SurveyTerrain worldSurveyTerrain(int tx, int ty);
PropKind propAt(int tx, int ty);
Texture2D* propTexture(PropKind kind, int* outW = nullptr, int* outH = nullptr);
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
