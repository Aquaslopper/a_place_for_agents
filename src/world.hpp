// world.hpp — the endless world: biomes, villages, props, npcs, landmarks
#pragma once

#include "game.hpp"

// Deterministic quest & landmark sites in world tile coordinates.
constexpr int BOSS_CASTLE_TX = 0;
constexpr int BOSS_CASTLE_TY = -144;
constexpr float BOSS_KING_X = 0.5f;
constexpr float BOSS_KING_Y = -140.5f;

constexpr int LOCKET_SITE_TX = 52;
constexpr int LOCKET_SITE_TY = 46;
constexpr float LOCKET_PICKUP_X = 52.5f;
constexpr float LOCKET_PICKUP_Y = 48.0f;

constexpr int TOWER_SITE_TX = -64;
constexpr int TOWER_SITE_TY = -20;
constexpr int TOWER_CHEST_TX = -64;
constexpr int TOWER_CHEST_TY = -18;

// Exact building tile footprint & visual alignment helpers (shared by world, overworld, room, render).
inline int   buildingX0(const Building& b)      { return b.tx - b.w / 2; }
inline int   buildingY0(const Building& b)      { return b.ty - b.h + 1; }
inline float buildingCenterX(const Building& b) { return (buildingX0(b) + b.w * 0.5f) * (float)TILE; }
inline int   buildingDoorTx(const Building& b)  { return buildingX0(b) + b.w / 2; }
inline int   buildingDoorTy(const Building& b)  { return b.ty + 1; }

// Which prop stands on a tile (P_NONE when the tile is clear). The renderer,
// the collision test and the pathfinder all read this, so they cannot disagree.
PropKind propAt(int tx, int ty);
Texture2D* propTexture(PropKind kind, int* outW = nullptr, int* outH = nullptr);

// Village deco kinds used by Village::decos
enum VillageDeco {
    VD_WELL = 0, VD_SIGN, VD_BENCH, VD_TORCH, VD_CAMPFIRE, VD_BARREL, VD_PLANT,
    VD_FLOWERBED, VD_LADDER, VD_STATUE, VD_CAULDRON, VD_URN, VD_FENCE_H, VD_FENCE_V,
    VD_GATE, VD_TEMPLE_PILLAR, VD_HOUSE, VD_CRATE, VD_TABLE, VD_BUCKET, VD_SHADE_TREE,
    VD_MAILBOX, VD_NOTICEBOARD, VD_LONGTABLE, VD_FENCE_GATE,
    VD_BUSH, VD_FLOWER_BUSH, VD_ROCK, VD_STUMP, VD_PINE,
    VD_KIND_COUNT
};
void villageDecoTexture(int kind, int variant, Texture2D*& tex, int& w, int& h);
bool villageDecoSolid(int kind);
float villageDecoCenterX(const Village::Deco& d);

// Individual landmark sub-pieces so every pillar, torch, keep, arch and tent
// Y-sorts independently and matches tile collision 1-to-1.
enum LandmarkPartKind {
    LP_CASTLE = 0, LP_TOWER, LP_STATUE, LP_PILLAR, LP_TORCH,
    LP_RUIN_ARCH, LP_MONUMENT, LP_TENT, LP_CAMPFIRE, LP_CAULDRON, LP_BARREL,
    LP_CAVE
};
struct LandmarkPart {
    LandmarkPartKind part;
    int tx, ty;
    int spanW;   // horizontal footprint width in tiles (odd -> centred on tx+0.5, even -> centred on tx)
};
void expandLandmarkParts(const Landmark& lm, std::vector<LandmarkPart>& out);

// The hand-authored starting village ("Elmswick"), always present at the origin.
void buildStartVillage(Village& v, uint32_t seed);

// Landmarks visible within [x0..x1, y0..y1] (deduplicated, includes story sites).
void landmarksInView(int x0, int y0, int x1, int y1, std::vector<Landmark>& out);

// Names, compass & flavour
std::string villageNameFor(int sx, int sy, uint32_t seed);
std::string compassHint(float fromX, float fromY, float toX, float toY);
const char* npcRoleName(int role);
