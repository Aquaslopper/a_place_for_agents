// town_gen.cpp — pure, deterministic, bounded Lufia II-style procedural town generator.
#include "town_gen.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <queue>
#include <sstream>

namespace apfa {
namespace {

struct LotSlot {
    int x0 = 0;         // inclusive left tile of lot
    int width = 5;      // lot width in tiles
    int yBase = 0;      // bottom solid tile row of building placed here
    bool isNorth = true;
    int maxFootW = 5;
};

struct Rng {
    std::uint32_t s;
    explicit Rng(std::uint32_t seed) : s(seed ? seed : 0x9E3779B9u) {}
    std::uint32_t next() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }
    int range(int lo, int hi) {
        if (hi <= lo) return lo;
        return lo + static_cast<int>(next() % static_cast<std::uint32_t>(hi - lo + 1));
    }
};

inline void setTile(TownDef& t, int tx, int ty, TownTileKind k) {
    if (tx < 0 || ty < 0 || tx >= t.w || ty >= t.h) return;
    t.tiles[static_cast<std::size_t>(ty) * static_cast<std::size_t>(t.w) + static_cast<std::size_t>(tx)] = k;
}

inline void fillRect(TownDef& t, int x0, int y0, int x1, int y1, TownTileKind k) {
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x)
            setTile(t, x, y, k);
}

void addPropSafe(TownDef& t, TownPropKind kind, int tx, int ty, bool solid, std::uint8_t var = 0) {
    if (!townInBounds(t, tx, ty)) return;
    // Never overwrite another solid prop on the exact same tile
    for (const auto& p : t.props) {
        if (p.tx == tx && p.ty == ty) return;
    }
    t.props.push_back(TownProp{kind, tx, ty, var, solid});
}

std::pair<int, int> footprintForKind(TownBuildingKind k) {
    switch (k) {
        case TB_SHOP:   return {5, 2};
        case TB_INN:    return {5, 2};
        case TB_SMITHY: return {5, 2};
        case TB_TEMPLE: return {5, 3};
        case TB_MANOR:  return {5, 2};
        case TB_CASTLE: return {7, 3};
        default:        return {4, 2};
    }
}

std::string buildingLabel(TownBuildingKind k, const std::string& townName, bool isStartTown, int houseIdx) {
    switch (k) {
        case TB_SHOP:   return isStartTown ? "Mira's Sundries" : (townName + " General Store");
        case TB_INN:    return isStartTown ? "The Old Elm Inn" : (townName + " Inn");
        case TB_SMITHY: return isStartTown ? "Bram's Forge" : (townName + " Smithy");
        case TB_TEMPLE: return isStartTown ? "Sanctuary of Elm" : (townName + " Sanctuary");
        case TB_MANOR:  return isStartTown ? "Elder's Hall" : (townName + " Manor");
        case TB_CASTLE: return townName + " Citadel";
        default: {
            static const char* kCottage[5] = {
                "Weaver's Cottage", "Baker's Cottage", "Woodcutter's Home",
                "Herbalist's Home", "Miller's Cottage"
            };
            return kCottage[houseIdx % 5];
        }
    }
}

void placeBuildingInLot(TownDef& t, const LotSlot& lot, TownBuildingKind kind,
                        int laneY, int& houseCounter, bool isStartTown) {
    auto [fw, fh] = footprintForKind(kind);
    if (fw > lot.width) fw = lot.width;

    int leftX = lot.x0 + (lot.width - fw) / 2;
    int centerTx = leftX + fw / 2;
    int baseTy = lot.yBase;

    TownBuilding b;
    b.kind = kind;
    b.tx = centerTx;
    b.ty = baseTy;
    b.w = fw;
    b.h = fh;
    b.doorTx = centerTx;
    b.doorTy = baseTy + 1;
    b.index = static_cast<int>(t.buildings.size());
    b.label = buildingLabel(kind, t.name, isStartTown, houseCounter);
    if (kind <= TB_HOUSE4) ++houseCounter;

    // Pave courtyard under civic halls & guarantee crisp 2-tile doorstep apron
    TownTileKind apron = (kind == TB_TEMPLE || kind == TB_MANOR || kind == TB_CASTLE)
        ? TT_COURT : (t.tier == TIER_HAMLET ? TT_ROAD : TT_COBBLE);
    if (kind == TB_TEMPLE || kind == TB_MANOR || kind == TB_CASTLE) {
        fillRect(t, leftX, baseTy - fh + 1, leftX + fw - 1, baseTy + 2, TT_COURT);
    } else {
        fillRect(t, b.doorTx - 1, baseTy + 1, b.doorTx + 1, baseTy + 2, apron);
    }
    // Connect doorstep cleanly down to the horizontal street lane
    int y0 = std::min(b.doorTy, laneY);
    int y1 = std::max(b.doorTy, laneY);
    for (int y = y0; y <= y1; ++y) {
        TownTileKind cur = townTileAt(t, b.doorTx, y);
        if (cur != TT_COBBLE && cur != TT_COURT && cur != TT_BRIDGE)
            setTile(t, b.doorTx, y, apron);
    }

    // Civic signage & lantern accents cleanly beside the doorway (NEVER on doorTx!)
    int signX = leftX;
    if (signX == b.doorTx) signX = leftX + fw - 1;
    switch (kind) {
        case TB_SHOP:
            addPropSafe(t, TP_SIGN_GOODS, signX, baseTy + 1, true);
            addPropSafe(t, TP_CRATE, leftX + fw - 1, baseTy + 1, true);
            break;
        case TB_INN:
            addPropSafe(t, TP_SIGN_INN, signX, baseTy + 1, true);
            addPropSafe(t, TP_TORCH, leftX + fw - 1, baseTy + 1, false);
            break;
        case TB_SMITHY:
            addPropSafe(t, TP_SIGN_WEAPON, signX, baseTy + 1, true);
            addPropSafe(t, TP_BARREL, leftX + fw - 1, baseTy + 1, true);
            break;
        case TB_TEMPLE:
            addPropSafe(t, TP_BANNER_VERT, leftX, baseTy + 1, false);
            addPropSafe(t, TP_BANNER_VERT, leftX + fw - 1, baseTy + 1, false);
            break;
        case TB_MANOR:
            addPropSafe(t, TP_TORCH, leftX, baseTy + 1, false);
            addPropSafe(t, TP_TORCH, leftX + fw - 1, baseTy + 1, false);
            break;
        case TB_CASTLE:
            addPropSafe(t, TP_BANNER_VERT, leftX, baseTy + 1, false);
            addPropSafe(t, TP_STATUE, leftX + 1, baseTy + 1, true);
            addPropSafe(t, TP_STATUE, leftX + fw - 2, baseTy + 1, true);
            addPropSafe(t, TP_BANNER_VERT, leftX + fw - 1, baseTy + 1, false);
            break;
        default:
            if (b.index % 2 == 0)
                addPropSafe(t, TP_FLOWERBED, leftX, baseTy + 1, false);
            else
                addPropSafe(t, TP_BARREL, leftX + fw - 1, baseTy + 1, true);
            break;
    }

    t.buildings.push_back(b);
}

void decorateOpenLot(TownDef& t, const LotSlot& lot, int kind, Rng& rng, bool isElmswickCommons) {
    int x0 = lot.x0;
    int x1 = lot.x0 + lot.width - 1;
    int yTop = lot.yBase - 3;
    int yBot = lot.yBase;

    if (isElmswickCommons) {
        // The Agents' Commons Grove right inside Elmswick!
        fillRect(t, x0, yTop, x1, yBot + 1, TT_GRASS_DARK);
        int cx = (x0 + x1) / 2;
        int cy = (yTop + yBot) / 2;
        setTile(t, cx, cy, TT_COURT);
        addPropSafe(t, TP_COMMONS_BRAZIER, cx, cy, true);
        addPropSafe(t, TP_COMMONS_STONE, cx - 2, cy - 1, true, 0);
        addPropSafe(t, TP_COMMONS_STONE, cx + 2, cy - 1, true, 1);
        addPropSafe(t, TP_COMMONS_STONE, cx,     cy - 2, true, 2);
        addPropSafe(t, TP_FLOWERBED, x0, yBot, false);
        addPropSafe(t, TP_FLOWERBED, x1, yBot, false);
        return;
    }

    if (kind == 0) {
        // Turnip kitchen garden with wooden rail on top
        fillRect(t, x0 + 1, yTop + 1, x1 - 1, yBot, TT_SOIL);
        for (int x = x0; x <= x1; ++x) {
            addPropSafe(t, TP_FENCE_H, x, yTop, true);
        }
        addPropSafe(t, TP_BARREL, x0, yBot, true);
    } else if (kind == 1) {
        // Open-air market stalls
        fillRect(t, x0, yTop, x1, yBot + 1, t.tier == TIER_HAMLET ? TT_ROAD : TT_COBBLE);
        addPropSafe(t, TP_MARKET_FLOWERS_RED,    x0 + 1, yTop + 1, true);
        if (lot.width >= 5)
            addPropSafe(t, TP_MARKET_FLOWERS_ORANGE, x0 + 3, yTop + 1, true);
        addPropSafe(t, TP_MARKET_SACKS,          x0 + 1, yBot,     true);
        addPropSafe(t, TP_CRATE,                 x1 - 1, yBot,     true);
    } else {
        // Shaded town park grove with biome-matched trees & bench
        fillRect(t, x0, yTop, x1, yBot, TT_GRASS_DARK);
        TownPropKind treeK = (t.climate == OWC_SNOW) ? TP_PINE
                           : (rng.range(0, 2) == 0 ? TP_PINE : TP_TREE);
        addPropSafe(t, treeK, x0 + 1, yTop + 1, true, static_cast<std::uint8_t>(rng.next() & 3u));
        if (lot.width >= 5)
            addPropSafe(t, treeK, x1 - 1, yTop + 1, true, static_cast<std::uint8_t>(rng.next() & 3u));
        addPropSafe(t, TP_BENCH, (x0 + x1) / 2, yBot, true);
        addPropSafe(t, TP_FLOWERBED, x0, yBot, false);
        addPropSafe(t, TP_BUSH, x1, yBot, true);
    }
}

bool occupiedByBuilding(const TownDef& t, int tx, int ty) {
    for (const auto& b : t.buildings) {
        int leftX = b.tx - b.w / 2;
        int rightX = leftX + b.w - 1;
        int topY = b.ty - b.h + 1;
        if (tx >= leftX && tx <= rightX && ty >= topY && ty <= b.ty)
            return true;
    }
    return false;
}

} // namespace

TownTier townTierForSite(OwSiteType type) {
    if (type == OWS_CITY) return TIER_CITY;
    if (type == OWS_TOWN) return TIER_TOWN;
    return TIER_HAMLET;
}

bool townInBounds(const TownDef& t, int tx, int ty) {
    return tx >= 0 && ty >= 0 && tx < t.w && ty < t.h;
}

TownTileKind townTileAt(const TownDef& t, int tx, int ty) {
    if (!townInBounds(t, tx, ty)) return TT_WALL_FACE;
    return t.tiles[static_cast<std::size_t>(ty) * static_cast<std::size_t>(t.w) + static_cast<std::size_t>(tx)];
}

bool townCellWalkable(const TownDef& t, int tx, int ty) {
    if (!townInBounds(t, tx, ty)) return false;
    TownTileKind k = townTileAt(t, tx, ty);
    if (k == TT_WALL_TOP || k == TT_WALL_FACE || k == TT_WATER) return false;
    if (occupiedByBuilding(t, tx, ty)) return false;
    for (const auto& p : t.props) {
        if (p.solid && p.tx == tx && p.ty == ty) return false;
    }
    return true;
}

int townGateAt(const TownDef& t, int tx, int ty) {
    for (int i = 0; i < static_cast<int>(t.gates.size()); ++i) {
        const auto& g = t.gates[i];
        if (g.dir == 0 || g.dir == 2) {
            if (ty == g.ty && (tx == g.tx || tx == g.tx + 1)) return i;
            if (g.dir == 0 && ty == 1 && (tx == g.tx || tx == g.tx + 1)) return i;
        } else {
            if (tx == g.tx && (ty == g.ty || ty == g.ty + 1)) return i;
        }
    }
    return -1;
}

int townDoorAt(const TownDef& t, int tx, int ty) {
    for (int i = 0; i < static_cast<int>(t.buildings.size()); ++i) {
        const auto& b = t.buildings[i];
        if (tx == b.doorTx && (ty == b.doorTy || ty == b.ty)) return i;
    }
    return -1;
}

TownDef townGenerate(std::uint32_t worldSeed, const OwSiteInfo& site) {
    TownDef t;
    t.worldSeed = worldSeed;
    t.owTx = site.tx;
    t.owTy = site.ty;
    t.townSeed = owHash2(site.tx, site.ty, worldSeed ^ 0x70A470A4u);
    t.tier = site.isStartTown ? TIER_TOWN : townTierForSite(site.type);
    t.climate = site.climate;
    t.isStartTown = site.isStartTown;
    t.dangerTier = owDangerTier(site.tx, site.ty);
    t.name = site.name.empty() ? owSettlementName(worldSeed, site.tx, site.ty, site.type, site.climate) : site.name;

    Rng rng(t.townSeed);

    // 1. Dimensions & primary axes per tier
    int mainX = 0;       // N-S boulevard left column (2 tiles wide: mainX, mainX+1)
    int mainY = 0;       // E-W main avenue top row (2 tiles tall: mainY, mainY+1)
    int southLaneY = 0;  // lower residential street row
    bool hasCanal = false;
    int canalY = 0;

    if (t.tier == TIER_HAMLET) {
        t.w = 34; t.h = 22;
        mainX = 16;
        mainY = 10;
        southLaneY = 19;
    } else if (t.tier == TIER_TOWN) {
        t.w = 48; t.h = 28;
        mainX = 23;
        mainY = 12;
        southLaneY = 24;
    } else {
        t.w = 56; t.h = 36;
        mainX = 27;
        mainY = 13;
        hasCanal = true;
        canalY = 19;
        southLaneY = 32;
    }

    t.tiles.assign(static_cast<std::size_t>(t.w) * static_cast<std::size_t>(t.h), TT_GRASS);
    t.tileVar.resize(static_cast<std::size_t>(t.w) * static_cast<std::size_t>(t.h));
    for (int y = 0; y < t.h; ++y)
        for (int x = 0; x < t.w; ++x)
            t.tileVar[static_cast<std::size_t>(y) * static_cast<std::size_t>(t.w) + static_cast<std::size_t>(x)] =
                static_cast<std::uint8_t>(owHash2(x, y, t.townSeed ^ 0x51A1u) & 7u);

    // 2. Perimeter stone fortifications (2-row north wall for SNES 3/4 depth)
    for (int x = 0; x < t.w; ++x) {
        setTile(t, x, 0, TT_WALL_TOP);
        setTile(t, x, 1, TT_WALL_FACE);
        setTile(t, x, t.h - 1, TT_WALL_FACE);
    }
    for (int y = 1; y < t.h; ++y) {
        setTile(t, 0, y, TT_WALL_FACE);
        setTile(t, t.w - 1, y, TT_WALL_FACE);
    }
    // Corner turrets + ivy accents on temperate masonry
    addPropSafe(t, TP_TURRET, 0, 1, false);
    addPropSafe(t, TP_TURRET, t.w - 1, 1, false);
    addPropSafe(t, TP_TURRET, 0, t.h - 1, false);
    addPropSafe(t, TP_TURRET, t.w - 1, t.h - 1, false);
    if (t.climate == OWC_TEMPERATE) {
        for (int x = 4; x < t.w - 4; x += 7) {
            if (std::abs(x - mainX) > 3 && (owHash2(x, 1, t.townSeed) & 1u))
                addPropSafe(t, TP_WALL_IVY, x, 1, false);
        }
    }

    // 3. Street Spine, Central Square & City Canal
    TownTileKind street = (t.tier == TIER_HAMLET) ? TT_ROAD : TT_COBBLE;
    // Main E-W avenue
    fillRect(t, 1, mainY, t.w - 2, mainY + 1, street);
    // Main N-S boulevard
    fillRect(t, mainX, 2, mainX + 1, t.h - 2, street);
    // South service lane connecting south-row doorsteps to N-S boulevard
    fillRect(t, 3, southLaneY, t.w - 4, southLaneY + 1, TT_ROAD);
    // Side vertical walkways near west & east quarters
    int westWalk = 2, eastWalk = t.w - 3;
    fillRect(t, westWalk, mainY, westWalk, southLaneY, TT_ROAD);
    fillRect(t, eastWalk, mainY, eastWalk, southLaneY, TT_ROAD);

    // Central Town Square plaza
    int px0 = mainX - (t.tier == TIER_HAMLET ? 2 : 3);
    int px1 = mainX + 1 + (t.tier == TIER_HAMLET ? 2 : 3);
    int py0 = mainY - 1;
    int py1 = mainY + 2;
    fillRect(t, px0, py0, px1, py1, TT_COBBLE);

    // Ornamental stone canal in Cities with 3 wide bridges
    if (hasCanal) {
        fillRect(t, 1, canalY, t.w - 2, canalY + 1, TT_WATER);
        // Central bridge + west/east pedestrian bridges
        fillRect(t, mainX, canalY, mainX + 1, canalY + 1, TT_BRIDGE);
        fillRect(t, westWalk, canalY, westWalk + 1, canalY + 1, TT_BRIDGE);
        fillRect(t, eastWalk - 1, canalY, eastWalk, canalY + 1, TT_BRIDGE);
        // Secondary avenue along north bank of canal
        fillRect(t, 2, canalY - 1, t.w - 3, canalY - 1, TT_COBBLE);
    }

    // 4. Open Cardinal Town Gates (South gate ALWAYS open!)
    std::uint8_t gmask = static_cast<std::uint8_t>(site.gateMask | 0x04u);
    if (gmask & 0x04u) { // South gate
        setTile(t, mainX, t.h - 1, TT_GATE_FLOOR);
        setTile(t, mainX + 1, t.h - 1, TT_GATE_FLOOR);
        t.gates.push_back(TownGate{2, mainX, t.h - 1, mainX, t.h - 3});
        addPropSafe(t, TP_TORCH, mainX - 1, t.h - 2, false);
        addPropSafe(t, TP_TORCH, mainX + 2, t.h - 2, false);
    }
    if (gmask & 0x01u) { // North gate
        setTile(t, mainX, 0, TT_GATE_FLOOR);
        setTile(t, mainX + 1, 0, TT_GATE_FLOOR);
        setTile(t, mainX, 1, TT_GATE_FLOOR);
        setTile(t, mainX + 1, 1, TT_GATE_FLOOR);
        t.gates.push_back(TownGate{0, mainX, 0, mainX, 3});
        addPropSafe(t, TP_GATE_DOUBLE, mainX, 1, false);
        addPropSafe(t, TP_BANNER_SHORT, mainX - 1, 1, false);
        addPropSafe(t, TP_BANNER_SHORT, mainX + 2, 1, false);
    }
    if (gmask & 0x08u) { // West gate
        setTile(t, 0, mainY, TT_GATE_FLOOR);
        setTile(t, 0, mainY + 1, TT_GATE_FLOOR);
        t.gates.push_back(TownGate{3, 0, mainY, 2, mainY});
        addPropSafe(t, TP_TORCH, 1, mainY - 1, false);
    }
    if (gmask & 0x02u) { // East gate
        setTile(t, t.w - 1, mainY, TT_GATE_FLOOR);
        setTile(t, t.w - 1, mainY + 1, TT_GATE_FLOOR);
        t.gates.push_back(TownGate{1, t.w - 1, mainY, t.w - 3, mainY});
        addPropSafe(t, TP_TORCH, t.w - 2, mainY - 1, false);
    }

    t.spawnTx = mainX;
    t.spawnTy = t.h - 3;

    // 5. Plaza Landmarks (Well, Noticeboard, Benches, Lanterns — placed on the
    //    south plaza corners so north-row building facades remain 100% unobstructed!)
    addPropSafe(t, TP_WELL, px0, py1, true);
    addPropSafe(t, TP_NOTICEBOARD, px1, py1, true);
    addPropSafe(t, TP_TORCH, px0, py0, false);
    addPropSafe(t, TP_TORCH, px1, py0, false);
    if (t.tier != TIER_HAMLET) {
        addPropSafe(t, TP_FLOWERBED, px0 - 1, py1, false);
        addPropSafe(t, TP_FLOWERBED, px1 + 1, py1, false);
        addPropSafe(t, TP_BENCH, px0 - 2, py1, true);
        addPropSafe(t, TP_BENCH, px1 + 2, py1, true);
    }

    // 6. Define Structured Non-Overlapping Building Lots
    std::vector<LotSlot> northLots;
    std::vector<LotSlot> southLots;

    if (t.tier == TIER_HAMLET) {
        int nY = 7, sY = 17;
        northLots = {
            {3,  5, nY, true, 5},
            {10, 5, nY, true, 5},
            {19, 5, nY, true, 5},
            {26, 5, nY, true, 5}
        };
        southLots = {
            {3,  5, sY, false, 5},
            {10, 5, sY, false, 5},
            {19, 5, sY, false, 5},
            {26, 5, sY, false, 5}
        };
    } else if (t.tier == TIER_TOWN) {
        int nY = 8, sY = 22;
        northLots = {
            {3,  6, nY, true, 6},
            {10, 5, nY, true, 5},
            {16, 5, nY, true, 5},
            {27, 5, nY, true, 5},
            {33, 5, nY, true, 5},
            {39, 6, nY, true, 6}
        };
        southLots = {
            {3,  6, sY, false, 6},
            {10, 5, sY, false, 5},
            {16, 5, sY, false, 5},
            {27, 5, sY, false, 5},
            {33, 5, sY, false, 5},
            {39, 6, sY, false, 6}
        };
    } else {
        int nY = 9, sY = 29;
        northLots = {
            {3,  7, nY, true, 7},
            {11, 6, nY, true, 6},
            {18, 7, nY, true, 7},
            {31, 7, nY, true, 7},
            {39, 6, nY, true, 6},
            {46, 7, nY, true, 7}
        };
        southLots = {
            {3,  7, sY, false, 7},
            {11, 6, sY, false, 6},
            {18, 7, sY, false, 7},
            {31, 7, sY, false, 7},
            {39, 6, sY, false, 6},
            {46, 7, sY, false, 7}
        };
    }

    // 7. Assign Required Civic Buildings + Houses deterministically with seed variation
    int houseCounter = 0;
    auto pickHouseKind = [&]() -> TownBuildingKind {
        return static_cast<TownBuildingKind>(TB_HOUSE0 + rng.range(0, 4));
    };

    // Build north-row roster (civic heart of town!)
    std::vector<TownBuildingKind> northRoster;
    if (t.tier == TIER_HAMLET) {
        northRoster = { TB_SHOP, TB_INN, pickHouseKind(), pickHouseKind() };
    } else if (t.tier == TIER_TOWN) {
        northRoster = { TB_SHOP, TB_INN, TB_MANOR, TB_TEMPLE, TB_SMITHY, pickHouseKind() };
    } else {
        northRoster = { TB_CASTLE, TB_MANOR, TB_TEMPLE, TB_SHOP, TB_INN, TB_SMITHY };
    }

    // Deterministic Fisher-Yates shuffle on non-start towns (and keep Castle on a 7-wide slot!)
    if (!t.isStartTown) {
        for (int i = static_cast<int>(northRoster.size()) - 1; i > 0; --i) {
            int j = rng.range(0, i);
            std::swap(northRoster[i], northRoster[j]);
        }
        // If City, ensure TB_CASTLE sits in a slot wide enough (width >= 7)
        if (t.tier == TIER_CITY) {
            for (std::size_t i = 0; i < northRoster.size(); ++i) {
                if (northRoster[i] == TB_CASTLE && northLots[i].width < 7) {
                    std::swap(northRoster[i], northRoster[2]); // slot 2 is width 7 right by plaza!
                    break;
                }
            }
        }
    } else {
        // Elmswick iconic arrangement: Shop, Inn, Elder's Hall (left of plaza) |
        // Sanctuary, Smithy, Weaver's Cottage (right of plaza)
        northRoster = { TB_SHOP, TB_INN, TB_MANOR, TB_TEMPLE, TB_SMITHY, TB_HOUSE0 };
    }

    for (std::size_t i = 0; i < northLots.size() && i < northRoster.size(); ++i) {
        placeBuildingInLot(t, northLots[i], northRoster[i], mainY, houseCounter, t.isStartTown);
    }

    // South-row lots: mix of cottages, market stalls, turnip garden, town park (and Elmswick Commons!)
    std::vector<int> southRoles;
    if (t.tier == TIER_HAMLET) {
        southRoles = { -1, 0, 1, 2 }; // 1 cottage, 1 turnip garden, 1 market, 1 park
    } else if (t.isStartTown) {
        // Elmswick south row: Cottage, Turnip Garden, Market, Commons Grove, Cottage, Park
        southRoles = { -1, 0, 1, 99, -1, 2 };
    } else {
        // Town/City: 3 cottages + garden + market + park
        southRoles = { -1, -1, -1, 0, 1, 2 };
    }
    if (!t.isStartTown) {
        for (int i = static_cast<int>(southRoles.size()) - 1; i > 0; --i) {
            int j = rng.range(0, i);
            std::swap(southRoles[i], southRoles[j]);
        }
    }

    for (std::size_t i = 0; i < southLots.size() && i < southRoles.size(); ++i) {
        int role = southRoles[i];
        if (role < 0) {
            placeBuildingInLot(t, southLots[i], pickHouseKind(), southLaneY, houseCounter, t.isStartTown);
        } else {
            decorateOpenLot(t, southLots[i], role, rng, role == 99);
        }
    }

    // 8. Perimeter Decorative Trees & Bushes in quiet grass pockets (strictly off streets/aprons!)
    TownPropKind edgeTree = (t.climate == OWC_SNOW) ? TP_PINE : TP_TREE;
    int decoCandidates[6][2] = {
        {2, 3}, {t.w - 3, 3}, {2, t.h - 3}, {t.w - 3, t.h - 3},
        {mainX - 3, 3}, {mainX + 4, 3}
    };
    for (int i = 0; i < 6; ++i) {
        int tx = decoCandidates[i][0], ty = decoCandidates[i][1];
        if (townCellWalkable(t, tx, ty) && townTileAt(t, tx, ty) == TT_GRASS &&
            townDoorAt(t, tx, ty) < 0 && std::abs(tx - mainX) > 2) {
            addPropSafe(t, (i < 4) ? edgeTree : TP_BUSH, tx, ty, true, static_cast<std::uint8_t>(i & 3));
        }
    }

    // 9. Hidden Town Chests (1 in Hamlet, 2 in Town, 3 in City) — classic Lufia II town loot!
    const int chestCount = (t.tier == TIER_HAMLET) ? 1 : (t.tier == TIER_TOWN ? 2 : 3);
    int chestSpots[4][2] = {
        {2, 4},
        {t.w - 3, 4},
        {3, t.h - 3},
        {t.w - 4, t.h - 3}
    };
    // Match ItemId indices in game.hpp: 0=Potion, 1=Hi-Potion, 2=Ether, 3=Antidote, 4=Phoenix Feather
    for (int i = 0; i < chestCount; ++i) {
        int cx = chestSpots[i][0];
        int cy = chestSpots[i][1];
        // Nudge onto nearest walkable grass/road tile if occupied
        for (int dy = 0; dy <= 3 && !townCellWalkable(t, cx, cy); ++dy)
            for (int dx = -2; dx <= 2 && !townCellWalkable(t, cx, cy); ++dx)
                if (townCellWalkable(t, chestSpots[i][0] + dx, chestSpots[i][1] + dy)) {
                    cx = chestSpots[i][0] + dx;
                    cy = chestSpots[i][1] + dy;
                }
        if (!townCellWalkable(t, cx, cy)) continue;

        TownChest ch;
        ch.id = (static_cast<std::uint64_t>(t.townSeed) << 32) | static_cast<std::uint64_t>(0xC000u + i);
        ch.tx = cx;
        ch.ty = cy;
        std::uint32_t lh = owHash2(cx, cy, t.townSeed ^ 0x1007u);
        if ((lh % 3u) == 0u) {
            ch.item = -1;
            ch.gil = 25 + t.dangerTier * 20 + static_cast<int>(lh % 30u);
        } else {
            static const int kItems[5] = { 0, 3, 2, 1, 4 };
            ch.item = kItems[(lh >> 4) % (t.dangerTier >= 3 ? 5u : 3u)];
            ch.amount = 1;
        }
        t.chests.push_back(ch);
    }

    // 10. Populate Living Town NPCs
    auto addNpc = [&](TownNpcArchetype arch, int tx, int ty, int spriteVar, int questId, const std::string& name) {
        // Find nearest walkable non-door tile within radius 3
        int bestX = tx, bestY = ty;
        bool ok = false;
        for (int r = 0; r <= 3 && !ok; ++r) {
            for (int dy = -r; dy <= r && !ok; ++dy) {
                for (int dx = -r; dx <= r && !ok; ++dx) {
                    int nx = tx + dx, ny = ty + dy;
                    if (!townCellWalkable(t, nx, ny)) continue;
                    if (townDoorAt(t, nx, ny) >= 0) continue;
                    if (townGateAt(t, nx, ny) >= 0) continue;
                    bool clash = false;
                    for (const auto& ex : t.npcs)
                        if (ex.tx == nx && ex.ty == ny) { clash = true; break; }
                    if (clash) continue;
                    bestX = nx; bestY = ny; ok = true;
                }
            }
        }
        if (!ok) return;
        TownNpcSpec n;
        n.id = (static_cast<std::uint64_t>(t.townSeed) << 32) | static_cast<std::uint64_t>(t.npcs.size() + 1);
        n.archetype = arch;
        n.tx = bestX;
        n.ty = bestY;
        n.spriteVariant = spriteVar;
        n.questId = questId;
        n.name = name;
        t.npcs.push_back(n);
    };

    // South gate guard
    addNpc(TNPC_GUARD, mainX - 1, t.h - 3, 2, -1, "Gate Guard");
    if (t.tier != TIER_HAMLET)
        addNpc(TNPC_GUARD, mainX + 2, t.h - 3, 2, -1, "Town Watch");

    // Elder / Mayor in the town square
    addNpc(TNPC_ELDER, mainX + 1, mainY, 0, t.isStartTown ? 0 : -1,
           t.isStartTown ? "Elder Rowan" : ("Mayor of " + t.name));

    if (t.isStartTown) {
        // Core Elmswick story NPCs!
        addNpc(TNPC_FARMER,   12, southLaneY - 1, 3, 1, "Farmer Hale");
        addNpc(TNPC_VILLAGER, mainX - 2, mainY + 1, 1, 2, "Maren");
        addNpc(TNPC_KEEPER,   29, southLaneY - 2, 5, 3, "Keeper Oren");
        addNpc(TNPC_FERRYMAN, mainX + 2, t.h - 5, 4, 4, "Ferryman Bel");
        addNpc(TNPC_CHILD,    mainX - 5, mainY + 1, 6, -1, "Pip");
        addNpc(TNPC_MERCHANT, mainX + 5, mainY,     4, -1, "Peddler");
    } else {
        static const char* kNames[8] = { "Hanna", "Corin", "Bram", "Lina", "Toby", "Sera", "Oswin", "Dora" };
        addNpc(TNPC_MERCHANT, mainX - 3, mainY + 1, 4, -1, "Market Vendor");
        addNpc(TNPC_VILLAGER, mainX + 4, mainY + 1, 1, -1, kNames[rng.range(0, 7)]);
        addNpc(TNPC_FARMER,   6,         southLaneY, 3, -1, kNames[rng.range(0, 7)]);
        addNpc(TNPC_CHILD,    mainX - 4, southLaneY, 6, -1, kNames[rng.range(0, 7)]);
        if (t.tier != TIER_HAMLET) {
            addNpc(TNPC_VILLAGER, t.w - 7, mainY,      5, -1, kNames[rng.range(0, 7)]);
            addNpc(TNPC_VILLAGER, t.w - 9, southLaneY, 1, -1, kNames[rng.range(0, 7)]);
        }
    }
    // Town animal (dog / cat / chicken)
    addNpc(TNPC_ANIMAL, mainX + 3, mainY + 2, rng.range(0, 2), -1, "Town Pet");

    return t;
}

TownDef townGenerateFromCoords(std::uint32_t worldSeed, int owTx, int owTy) {
    OwSiteInfo site;
    if (owSiteAtTile(worldSeed, owTx, owTy, site) && owIsSettlement(site.type)) {
        return townGenerate(worldSeed, site);
    }
    site.type = OWS_TOWN;
    site.tx = owTx;
    site.ty = owTy;
    site.gateMask = 0x0F;
    site.isStartTown = (owTx == 0 && owTy == 0);
    site.name = site.isStartTown ? "Elmswick" : owSettlementName(worldSeed, owTx, owTy, OWS_TOWN, OWC_TEMPERATE);
    return townGenerate(worldSeed, site);
}

bool townVerify(const TownDef& t, std::string* reason) {
    auto fail = [&](const std::string& msg) {
        if (reason) *reason = msg;
        return false;
    };

    if (t.w < 24 || t.h < 20) return fail("town dimensions too small");
    if (t.gates.empty()) return fail("town has no exit gates");
    if (t.buildings.size() < 4) return fail("town has fewer than 4 buildings");
    if (!townCellWalkable(t, t.spawnTx, t.spawnTy)) return fail("town spawn tile is blocked");

    // Verify required civic services exist
    bool hasShop = false, hasInn = false;
    for (const auto& b : t.buildings) {
        if (b.kind == TB_SHOP) hasShop = true;
        if (b.kind == TB_INN)  hasInn = true;
    }
    if (!hasShop || !hasInn) return fail("town missing required Shop or Inn");

    // Verify no two building solid footprints overlap
    for (std::size_t i = 0; i < t.buildings.size(); ++i) {
        const auto& a = t.buildings[i];
        int ax0 = a.tx - a.w / 2, ax1 = ax0 + a.w - 1;
        int ay0 = a.ty - a.h + 1, ay1 = a.ty;
        if (ax0 <= 0 || ax1 >= t.w - 1 || ay0 <= 1 || ay1 >= t.h - 2)
            return fail("building " + a.label + " clips perimeter wall");
        for (std::size_t j = i + 1; j < t.buildings.size(); ++j) {
            const auto& b = t.buildings[j];
            int bx0 = b.tx - b.w / 2, bx1 = bx0 + b.w - 1;
            int by0 = b.ty - b.h + 1, by1 = b.ty;
            if (ax0 <= bx1 && ax1 >= bx0 && ay0 <= by1 && ay1 >= by0)
                return fail("buildings overlap: " + a.label + " and " + b.label);
        }
    }

    // BFS flood-fill from spawn tile on walkable cells
    std::vector<std::uint8_t> vis(static_cast<std::size_t>(t.w) * static_cast<std::size_t>(t.h), 0);
    std::queue<std::pair<int, int>> q;
    auto mark = [&](int x, int y) {
        vis[static_cast<std::size_t>(y) * static_cast<std::size_t>(t.w) + static_cast<std::size_t>(x)] = 1;
        q.push({x, y});
    };
    mark(t.spawnTx, t.spawnTy);

    static const int kDx[4] = { 1, -1, 0, 0 };
    static const int kDy[4] = { 0, 0, 1, -1 };
    while (!q.empty()) {
        auto [x, y] = q.front();
        q.pop();
        for (int d = 0; d < 4; ++d) {
            int nx = x + kDx[d], ny = y + kDy[d];
            if (!townInBounds(t, nx, ny)) continue;
            std::size_t idx = static_cast<std::size_t>(ny) * static_cast<std::size_t>(t.w) + static_cast<std::size_t>(nx);
            if (vis[idx]) continue;
            if (!townCellWalkable(t, nx, ny)) continue;
            vis[idx] = 1;
            q.push({nx, ny});
        }
    }

    auto reachable = [&](int x, int y) {
        if (!townInBounds(t, x, y)) return false;
        return vis[static_cast<std::size_t>(y) * static_cast<std::size_t>(t.w) + static_cast<std::size_t>(x)] != 0;
    };

    // Every active gate threshold must be reachable
    for (const auto& g : t.gates) {
        if (!reachable(g.tx, g.ty) && !reachable(g.spawnTx, g.spawnTy))
            return fail("gate dir " + std::to_string(g.dir) + " unreachable from spawn");
    }

    // Every building doorstep must be reachable
    for (const auto& b : t.buildings) {
        if (!reachable(b.doorTx, b.doorTy))
            return fail("building door unreachable: " + b.label);
    }

    // Every NPC spawn must be reachable
    for (const auto& n : t.npcs) {
        if (!reachable(n.tx, n.ty))
            return fail("NPC unreachable: " + n.name);
    }

    // Every town chest must be reachable
    for (const auto& c : t.chests) {
        if (!reachable(c.tx, c.ty))
            return fail("town chest unreachable at (" + std::to_string(c.tx) + "," + std::to_string(c.ty) + ")");
    }

    return true;
}

} // namespace apfa
