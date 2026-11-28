// overworld_gen.hpp — pure, deterministic, unbounded Lufia II-style overworld generator.
//
// Zero raylib or global-state dependency so the runtime renderer, collision,
// encounters, save/load, map overview and headless verifier all share one
// single source of truth.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace apfa {

constexpr int OW_CELL_SIZE = 24;   // overworld macro-cell size in tiles (24x24)

enum OwTileKind : std::uint8_t {
    OWT_DEEP_SEA = 0,
    OWT_SEA,
    OWT_GRASS,
    OWT_FOREST_LEAF,
    OWT_FOREST_PINE,
    OWT_SAND,
    OWT_SNOW,
    OWT_FOREST_SNOW,
    OWT_SWAMP,
    OWT_MOUNTAIN,
    OWT_MOUNTAIN_SNOW,
    OWT_BRIDGE_H,
    OWT_BRIDGE_V,
    OWT_PASS_H,
    OWT_PASS_V,
    OWT_COUNT
};

enum OwSiteType : std::uint8_t {
    OWS_NONE = 0,
    OWS_HAMLET,
    OWS_TOWN,
    OWS_CITY,
    OWS_CASTLE,       // dungeon/boss fortress
    OWS_CAVE,         // cavern dungeon
    OWS_TOWER,        // tower dungeon
    OWS_TEMPLE,       // shrine / temple dungeon
    OWS_RUINS,        // ancient ruins / locket site
    OWS_CAMP,         // tidewell island / frontier camp
    OWS_COUNT
};

enum OwClimate : std::uint8_t {
    OWC_TEMPERATE = 0,
    OWC_SAND,
    OWC_SNOW
};

struct OwSiteInfo {
    OwSiteType type = OWS_NONE;
    OwClimate climate = OWC_TEMPERATE;
    int tx = 0;
    int ty = 0;
    int cellX = 0;
    int cellY = 0;
    std::uint32_t seed = 0;
    std::uint8_t gateMask = 0x0F; // bit0=N, bit1=E, bit2=S, bit3=W
    bool isStartTown = false;
    bool isStartCavern = false;
    bool isBossCastle = false;
    bool isLocketSite = false;
    bool isTowerSite = false;
    int tideIslandIdx = -1;       // 0..2 for the three Tidewell islands, else -1
    std::string name;
};

struct OwCellTile {
    OwTileKind kind = OWT_GRASS;
    bool hasRoad = false;
    std::uint8_t variant = 0;
};

// Pure hashing & noise utilities
std::uint32_t owHash2(int x, int y, std::uint32_t seed);
float owHashF(int x, int y, std::uint32_t seed);
float owSmoothNoise(float x, float y, std::uint32_t seed);
float owFbm(float x, float y, std::uint32_t seed, int octaves);

// Pure terrain & site queries (deterministic for any integer (tx, ty)!)
OwCellTile owSampleTile(std::uint32_t seed, int tx, int ty);
bool owIsWater(OwTileKind k);
bool owIsLand(OwTileKind k);
bool owIsMountain(OwTileKind k);
bool owIsWalkableOnFoot(OwTileKind k);
bool owIsSailable(OwTileKind k);
bool owIsSettlement(OwSiteType t);
bool owIsDungeonSite(OwSiteType t);

// Site lookup — every 24x24 macro-cell deterministically owns sites.
void owSitesInCell(std::uint32_t seed, int cellX, int cellY, std::vector<OwSiteInfo>& out);
void owSitesInRect(std::uint32_t seed, int x0, int y0, int x1, int y1, std::vector<OwSiteInfo>& out);
bool owSiteAtTile(std::uint32_t seed, int tx, int ty, OwSiteInfo& out);
bool owNearestSite(std::uint32_t seed, int tx, int ty, int maxDist, OwSiteInfo& out,
                   bool settlementsOnly = false, bool dungeonsOnly = false);

// Region & site metadata
std::string owSettlementName(std::uint32_t seed, int tx, int ty, OwSiteType type, OwClimate climate);
std::string owRegionTitle(std::uint32_t seed, int tx, int ty);
// The name of the continent (region cell) a world tile belongs to.
std::string owContinentName(std::uint32_t seed, int tx, int ty);
int owDangerTier(int tx, int ty); // 1..8 scaling smoothly with distance from Elmswick

// Key homeland story sites (always exist near (0,0) for every seed)
OwSiteInfo owStartTown(std::uint32_t seed);
OwSiteInfo owStartCavern(std::uint32_t seed);
OwSiteInfo owBossCastle(std::uint32_t seed);
OwSiteInfo owLocketSite(std::uint32_t seed);
OwSiteInfo owTowerSite(std::uint32_t seed);
OwSiteInfo owTideIsland(std::uint32_t seed, int idx); // idx = 0..2

} // namespace apfa
