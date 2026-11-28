// town_gen.hpp — pure, deterministic, bounded Lufia II-style procedural town generator.
//
// Zero raylib dependency: shared by the runtime town mode (src/town.cpp),
// interiors (src/room.cpp), save/load, and the headless acceptance verifier.
#pragma once

#include "overworld_gen.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace apfa {

enum TownTier : std::uint8_t {
    TIER_HAMLET = 0,
    TIER_TOWN,
    TIER_CITY
};

enum TownTileKind : std::uint8_t {
    TT_GRASS = 0,
    TT_GRASS_DARK,
    TT_ROAD,
    TT_COBBLE,
    TT_COURT,
    TT_SOIL,
    TT_WATER,
    TT_BRIDGE,
    TT_WALL_TOP,
    TT_WALL_FACE,
    TT_GATE_FLOOR
};

// Building kinds align with legacy Building::kind (0..4 houses, 5 shop, 6 inn,
// 7 temple, 8 manor, 9 castle, 10 smithy).
enum TownBuildingKind : std::uint8_t {
    TB_HOUSE0 = 0,
    TB_HOUSE1 = 1,
    TB_HOUSE2 = 2,
    TB_HOUSE3 = 3,
    TB_HOUSE4 = 4,
    TB_SHOP   = 5,
    TB_INN    = 6,
    TB_TEMPLE = 7,
    TB_MANOR  = 8,
    TB_CASTLE = 9,
    TB_SMITHY = 10
};

enum TownPropKind : std::uint8_t {
    TP_WELL = 0,
    TP_NOTICEBOARD,
    TP_SIGN_GOODS,
    TP_SIGN_INN,
    TP_SIGN_WEAPON,
    TP_SIGN_ARMOR,
    TP_SIGN_TANKARD,
    TP_BANNER_VERT,
    TP_BANNER_SHORT,
    TP_MARKET_FLOWERS_RED,
    TP_MARKET_FLOWERS_ORANGE,
    TP_MARKET_SACKS,
    TP_BENCH,
    TP_TORCH,
    TP_FLOWERBED,
    TP_BARREL,
    TP_CRATE,
    TP_TREE,
    TP_PINE,
    TP_BUSH,
    TP_ROCK,
    TP_FENCE_H,
    TP_FENCE_POST,
    TP_TURRET,
    TP_GATE_DOUBLE,
    TP_WALL_IVY,
    TP_STATUE,
    TP_COMMONS_STONE,
    TP_COMMONS_BRAZIER
};

struct TownBuilding {
    TownBuildingKind kind = TB_HOUSE0;
    int tx = 0;         // bottom-center anchor tile X (matches Building::tx)
    int ty = 0;         // bottom-edge footprint tile Y (matches Building::ty)
    int w = 4;          // logical solid footprint width in tiles
    int h = 2;          // logical solid footprint height in tiles
    int doorTx = 0;     // walkable doorstep tile X (immediately south of door)
    int doorTy = 0;     // walkable doorstep tile Y (= ty + 1)
    int index = 0;      // 0-based building index within this town
    std::string label;  // e.g. "Mira's Sundries", "Oakford Inn"
};

struct TownProp {
    TownPropKind kind = TP_BENCH;
    int tx = 0;
    int ty = 0;
    std::uint8_t variant = 0;
    bool solid = false;
};

struct TownGate {
    int dir = 2;        // 0=N, 1=E, 2=S, 3=W
    int tx = 0;         // center-left gate threshold tile
    int ty = 0;
    int spawnTx = 0;    // safe tile just inside the gate where hero enters
    int spawnTy = 0;
};

struct TownChest {
    std::uint64_t id = 0;
    int tx = 0;
    int ty = 0;
    int item = -1;      // ItemId or -1 for gil
    int amount = 1;
    int gil = 0;
};

enum TownNpcArchetype : std::uint8_t {
    TNPC_GUARD = 0,
    TNPC_ELDER,
    TNPC_FARMER,
    TNPC_VILLAGER,
    TNPC_CHILD,
    TNPC_MERCHANT,
    TNPC_KEEPER,
    TNPC_FERRYMAN,
    TNPC_ANIMAL
};

struct TownNpcSpec {
    std::uint64_t id = 0;
    TownNpcArchetype archetype = TNPC_VILLAGER;
    int tx = 0;
    int ty = 0;
    int spriteVariant = 0;
    int questId = -1;
    std::string name;
};

struct TownDef {
    std::uint32_t worldSeed = 0;
    std::uint32_t townSeed = 0;
    int owTx = 0;
    int owTy = 0;
    TownTier tier = TIER_TOWN;
    OwClimate climate = OWC_TEMPERATE;
    bool isStartTown = false;
    int dangerTier = 1;
    std::string name;

    int w = 0;
    int h = 0;
    std::vector<TownTileKind> tiles;
    std::vector<std::uint8_t> tileVar;
    std::vector<TownBuilding> buildings;
    std::vector<TownProp> props;
    std::vector<TownGate> gates;
    std::vector<TownChest> chests;
    std::vector<TownNpcSpec> npcs;

    int spawnTx = 0;
    int spawnTy = 0;
};

TownTier townTierForSite(OwSiteType type);
TownDef townGenerate(std::uint32_t worldSeed, const OwSiteInfo& site);
TownDef townGenerateFromCoords(std::uint32_t worldSeed, int owTx, int owTy);

bool townInBounds(const TownDef& t, int tx, int ty);
TownTileKind townTileAt(const TownDef& t, int tx, int ty);
bool townCellWalkable(const TownDef& t, int tx, int ty);
int  townGateAt(const TownDef& t, int tx, int ty);         // returns gate index or -1
int  townDoorAt(const TownDef& t, int tx, int ty);         // returns building index or -1

// Pure structural & BFS reachability verifier (returns true iff all invariants pass)
bool townVerify(const TownDef& t, std::string* reason = nullptr);

} // namespace apfa
