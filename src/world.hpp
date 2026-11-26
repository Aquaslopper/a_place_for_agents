// world.hpp — the endless world: biomes, villages, props, npcs, landmarks
#pragma once

#include "game.hpp"

// Which prop stands on a tile (P_NONE when the tile is clear).  The renderer,
// the collision test and the pathfinder all read this, so they cannot disagree.
PropKind propAt(int tx, int ty);
Texture2D* propTexture(PropKind kind, int* outW = nullptr, int* outH = nullptr);

// Village deco kinds used by Village::decos
enum VillageDeco {
    VD_WELL = 0, VD_SIGN, VD_BENCH, VD_TORCH, VD_CAMPFIRE, VD_BARREL, VD_PLANT,
    VD_FLOWERBED, VD_LADDER, VD_STATUE, VD_CAULDRON, VD_URN, VD_FENCE_H, VD_FENCE_V,
    VD_GATE, VD_TEMPLE_PILLAR, VD_HOUSE, VD_CRATE, VD_TABLE, VD_BUCKET, VD_KIND_COUNT
};
void villageDecoTexture(int kind, int variant, Texture2D*& tex, int& w, int& h);

// The hand-authored starting village ("Elmswick"), always present at the origin.
void buildStartVillage(Village& v, uint32_t seed);

// Names + flavour
std::string villageNameFor(int sx, int sy, uint32_t seed);
const char* npcRoleName(int role);
