// world.cpp — Carcassonne 3x3 plate overworld generator & discrete map transitions
#include "world.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <unordered_map>

namespace {

// 17 canonical 3x3 binary masks matching tools/build_overworld_plates.py
struct MaskDef {
    const char* name;
    uint8_t m[3][3];
};

const MaskDef kMasks[17] = {
    { "full",     {{1,1,1},{1,1,1},{1,1,1}} },
    { "pond",     {{0,0,0},{0,1,0},{0,0,0}} },
    { "top",      {{1,1,1},{0,0,0},{0,0,0}} },
    { "bottom",   {{0,0,0},{0,0,0},{1,1,1}} },
    { "left",     {{1,0,0},{1,0,0},{1,0,0}} },
    { "right",    {{0,0,1},{0,0,1},{0,0,1}} },
    { "top2",     {{1,1,1},{1,1,1},{0,0,0}} },
    { "bottom2",  {{0,0,0},{1,1,1},{1,1,1}} },
    { "left2",    {{1,1,0},{1,1,0},{1,1,0}} },
    { "right2",   {{0,1,1},{0,1,1},{0,1,1}} },
    { "nw",       {{1,1,0},{1,1,0},{0,0,0}} },
    { "ne",       {{0,1,1},{0,1,1},{0,0,0}} },
    { "sw",       {{0,0,0},{1,1,0},{1,1,0}} },
    { "se",       {{0,0,0},{0,1,1},{0,1,1}} },
    { "vchannel", {{0,1,0},{0,1,0},{0,1,0}} },
    { "hchannel", {{0,0,0},{1,1,1},{0,0,0}} },
    { "cross",    {{0,1,0},{1,1,1},{0,1,0}} },
};

static uint32_t hashCoords(uint32_t seed, int x, int y, uint32_t salt = 0) {
    uint32_t h = seed ^ (uint32_t)(x * 374761393) ^ (uint32_t)(y * 668265263) ^ salt;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

static int floorDiv(int a, int b) {
    int q = a / b;
    int r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) --q;
    return q;
}

static int floorMod(int a, int b) {
    int r = a % b;
    if (r < 0) r += b;
    return r;
}

static void computeEdges(OwPlate& p) {
    for (int i = 0; i < 3; ++i) {
        p.edges[0][i] = p.mask[0][i] ? p.terrain : OW_GRASS; // N
        p.edges[1][i] = p.mask[i][2] ? p.terrain : OW_GRASS; // E
        p.edges[2][i] = p.mask[2][i] ? p.terrain : OW_GRASS; // S
        p.edges[3][i] = p.mask[i][0] ? p.terrain : OW_GRASS; // W
    }
}

static std::vector<OwPlate> buildPlateCatalog() {
    std::vector<OwPlate> cat;
    int nextId = 0;

    // 0: grass_plain
    {
        OwPlate p;
        p.plateId = nextId++;
        p.name = "grass_plain";
        p.terrain = OW_GRASS;
        computeEdges(p);
        cat.push_back(p);
    }

    // Terrain plates (17 masks for water & forest; 10 masks for sand, snow, dirt, mountain)
    const OwTerrain terrains[] = { OW_WATER, OW_FOREST, OW_SAND, OW_SNOW, OW_DIRT, OW_MOUNTAIN };
    const char* tnames[] = { "water", "forest", "sand", "snow", "dirt", "mountain" };
    for (int ti = 0; ti < 6; ++ti) {
        int maskCount = (ti < 2) ? 17 : 10;
        for (int mi = 0; mi < maskCount; ++mi) {
            OwPlate p;
            p.plateId = nextId++;
            p.name = std::string(tnames[ti]) + "_" + kMasks[mi].name;
            p.terrain = terrains[ti];
            for (int r = 0; r < 3; ++r)
                for (int c = 0; c < 3; ++c)
                    p.mask[r][c] = kMasks[mi].m[r][c];
            computeEdges(p);
            cat.push_back(p);
        }
    }

    // Special site & transporter plates
    auto addSite = [&](const char* name, OwSiteKind site, OwTerrain baseTerrain, int maskIdx, bool transporter) {
        OwPlate p;
        p.plateId = nextId++;
        p.name = name;
        p.terrain = baseTerrain;
        p.site = site;
        p.isTransporter = transporter;
        if (maskIdx >= 0) {
            for (int r = 0; r < 3; ++r)
                for (int c = 0; c < 3; ++c)
                    p.mask[r][c] = kMasks[maskIdx].m[r][c];
        }
        computeEdges(p);
        cat.push_back(p);
    };

    addSite("town_red",          OW_SITE_TOWN,      OW_GRASS,    -1, true);
    addSite("town_wood",         OW_SITE_TOWN,      OW_GRASS,    -1, true);
    addSite("town_snow",         OW_SITE_TOWN,      OW_GRASS,    -1, true);
    addSite("castle_blue",       OW_SITE_CASTLE,    OW_GRASS,    -1, true);
    addSite("castle_grey",       OW_SITE_CASTLE,    OW_GRASS,    -1, true);
    addSite("cave",              OW_SITE_DUNGEON,   OW_GRASS,    -1, true);
    addSite("temple",            OW_SITE_TEMPLE,    OW_GRASS,    -1, true);
    addSite("tower",             OW_SITE_TOWER,     OW_GRASS,    -1, true);
    addSite("ruins",             OW_SITE_RUINS,     OW_GRASS,    -1, true);
    addSite("camp",              OW_SITE_CAMP,      OW_GRASS,    -1, true);
    addSite("bridge_over_water", OW_SITE_BRIDGE,    OW_WATER,    14, true); // water_vchannel
    addSite("cave_in_mountain",  OW_SITE_CAVE_PASS, OW_MOUNTAIN,  0, true); // mountain_full
    addSite("gate_pass",         OW_SITE_GATE_PASS, OW_MOUNTAIN, 15, true); // mountain_hchannel

    return cat;
}

const std::vector<OwPlate>& plateCatalog() {
    static std::vector<OwPlate> cat = buildPlateCatalog();
    return cat;
}

static bool edgesMatch(const OwTerrain a[3], const OwTerrain b[3]) {
    return a[0] == b[0] && a[1] == b[1] && a[2] == b[2];
}

struct RegionKey {
    int rx, ry;
    bool operator==(const RegionKey& o) const { return rx == o.rx && ry == o.ry; }
};
struct RegionKeyHash {
    size_t operator()(const RegionKey& k) const {
        return ((size_t)(uint32_t)k.rx << 32) ^ (uint32_t)k.ry;
    }
};

uint32_t gWorldSeed = 4444;
std::unordered_map<RegionKey, OwRegion, RegionKeyHash> gRegions;

// Biome guide for a plate coordinate (global plate coords)
OwTerrain biomeTargetAt(int gx, int gy, uint32_t seed) {
    // Keep starting area (around 0,0) mostly grass with a nearby road/forest
    if (std::abs(gx) <= 2 && std::abs(gy) <= 2) return OW_GRASS;

    float nx = (float)gx * 0.14f + (float)(seed & 0xFF) * 0.01f;
    float ny = (float)gy * 0.14f + (float)((seed >> 8) & 0xFF) * 0.01f;
    float v = std::sin(nx) * std::cos(ny) + 0.5f * std::sin(nx * 0.5f + ny * 0.7f);

    if (gy < -10 && v > 0.2f) return OW_SNOW;
    if (gy > 10 && v > 0.25f) return OW_SAND;
    if (v > 0.65f) return OW_MOUNTAIN;
    if (v > 0.25f) return OW_FOREST;
    if (v < -0.55f) return OW_WATER;
    if (std::abs(v + 0.15f) < 0.08f) return OW_DIRT;
    return OW_GRASS;
}

OwRegion generateRegion(int rx, int ry, uint32_t seed) {
    OwRegion reg;
    reg.rx = rx;
    reg.ry = ry;
    reg.seed = seed;
    reg.plates.resize(OW_REGION_COLS * OW_REGION_ROWS);

    const auto& cat = plateCatalog();
    // Index 0 is grass_plain
    for (int r = 0; r < OW_REGION_ROWS; ++r) {
        for (int c = 0; c < OW_REGION_COLS; ++c) {
            int gx = rx * OW_REGION_COLS + c;
            int gy = ry * OW_REGION_ROWS + r;
            OwTerrain target = biomeTargetAt(gx, gy, seed);

            std::vector<int> valid;
            for (int i = 0; i < (int)cat.size(); ++i) {
                const OwPlate& cand = cat[i];
                if (cand.site != OW_SITE_NONE) continue; // punch sites after
                if (c > 0) {
                    const OwPlate& left = reg.plates[r * OW_REGION_COLS + (c - 1)];
                    if (!edgesMatch(left.edges[1], cand.edges[3])) continue;
                }
                if (r > 0) {
                    const OwPlate& top = reg.plates[(r - 1) * OW_REGION_COLS + c];
                    if (!edgesMatch(top.edges[2], cand.edges[0])) continue;
                }
                valid.push_back(i);
            }

            int chosen = 0; // fallback grass_plain
            if (!valid.empty()) {
                // Prefer candidates matching biome target
                std::vector<int> preferred;
                for (int idx : valid) {
                    if (cat[idx].terrain == target) preferred.push_back(idx);
                }
                const auto& pool = preferred.empty() ? valid : preferred;
                uint32_t h = hashCoords(seed, gx, gy, 0x9E3779B9u);
                chosen = pool[h % pool.size()];
            }
            reg.plates[r * OW_REGION_COLS + c] = cat[chosen];
        }
    }

    // Punch rare special sites & transporters (NEVER-BLOCKED rule)
    for (int r = 1; r < OW_REGION_ROWS - 1; ++r) {
        for (int c = 1; c < OW_REGION_COLS - 1; ++c) {
            OwPlate& p = reg.plates[r * OW_REGION_COLS + c];
            uint32_t h = hashCoords(seed, rx * OW_REGION_COLS + c, ry * OW_REGION_ROWS + r, 0xA5A5A5A5u);

            if (p.name == "water_vchannel" && (h % 5 == 0)) {
                for (const auto& cp : cat) if (cp.name == "bridge_over_water") { p = cp; break; }
            } else if (p.name == "mountain_hchannel" && (h % 4 == 0)) {
                for (const auto& cp : cat) if (cp.name == "gate_pass") { p = cp; break; }
            } else if (p.name == "mountain_full" && (h % 7 == 0)) {
                for (const auto& cp : cat) if (cp.name == "cave_in_mountain") { p = cp; break; }
            } else if (p.name == "grass_plain" && (h % 37 == 0)) {
                static const char* sites[] = { "town_red", "town_wood", "castle_blue", "cave", "temple", "tower", "ruins", "camp" };
                const char* sname = sites[(h >> 8) % 8];
                for (const auto& cp : cat) if (cp.name == sname) { p = cp; break; }
            }
        }
    }

    // Guarantee a starting town near (0,0) in region (0,0)
    if (rx == 0 && ry == 0) {
        for (const auto& cp : cat) {
            if (cp.name == "town_red") {
                reg.plates[1 * OW_REGION_COLS + 1] = cp;
                break;
            }
        }
    }

    return reg;
}

} // namespace

void worldInit(uint32_t seed) {
    gWorldSeed = seed;
    gRegions.clear();
    worldGetRegion(0, 0);
}

void worldReset(uint32_t seed) {
    worldInit(seed);
}

const OwRegion& worldGetRegion(int rx, int ry) {
    RegionKey key{ rx, ry };
    auto it = gRegions.find(key);
    if (it != gRegions.end()) return it->second;
    auto [ins, _] = gRegions.emplace(key, generateRegion(rx, ry, gWorldSeed));
    return ins->second;
}

const OwPlate& worldGetPlateAtTile(int tx, int ty) {
    int px = floorDiv(tx, PLATE_TILES);
    int py = floorDiv(ty, PLATE_TILES);
    int rx = floorDiv(px, OW_REGION_COLS);
    int ry = floorDiv(py, OW_REGION_ROWS);
    int lx = floorMod(px, OW_REGION_COLS);
    int ly = floorMod(py, OW_REGION_ROWS);
    const OwRegion& reg = worldGetRegion(rx, ry);
    return reg.plates[ly * OW_REGION_COLS + lx];
}

OwTerrain worldTerrainAt(int tx, int ty) {
    const OwPlate& p = worldGetPlateAtTile(tx, ty);
    int cx = floorMod(tx, PLATE_TILES);
    int cy = floorMod(ty, PLATE_TILES);
    return p.mask[cy][cx] ? p.terrain : OW_GRASS;
}

bool worldTileWalkable(int tx, int ty) {
    const OwPlate& p = worldGetPlateAtTile(tx, ty);
    if (p.isTransporter) return true;
    OwTerrain t = worldTerrainAt(tx, ty);
    return t != OW_WATER && t != OW_MOUNTAIN;
}

bool worldTileIsWater(int tx, int ty) {
    const OwPlate& p = worldGetPlateAtTile(tx, ty);
    if (p.site == OW_SITE_BRIDGE) return false;
    return worldTerrainAt(tx, ty) == OW_WATER;
}

OwSiteKind worldSiteAt(int tx, int ty) {
    const OwPlate& p = worldGetPlateAtTile(tx, ty);
    int cx = floorMod(tx, PLATE_TILES);
    int cy = floorMod(ty, PLATE_TILES);
    if (cx == 1 && cy == 1) return p.site;
    return OW_SITE_NONE;
}

bool worldEnterSiteAt(int tx, int ty) {
    OwSiteKind site = worldSiteAt(tx, ty);
    if (site == OW_SITE_NONE) return false;
    if (site == OW_SITE_DUNGEON || site == OW_SITE_TEMPLE || site == OW_SITE_TOWER || site == OW_SITE_RUINS) {
        enterDungeon(tx, ty, site == OW_SITE_DUNGEON);
        return true;
    }
    if (site == OW_SITE_TOWN || site == OW_SITE_CASTLE || site == OW_SITE_CAMP) {
        // Enter first building / town interior
        enterBuilding(0);
        return true;
    }
    return false;
}

void worldReturnToOverworld() {
    overworldSnapCamera();
}

SurveyTerrain worldSurveyTerrain(int tx, int ty) {
    SurveyTerrain s;
    OwTerrain t = worldTerrainAt(tx, ty);
    s.terrain = t;
    s.water = (t == OW_WATER);
    s.crag = (t == OW_MOUNTAIN);
    s.ice = (t == OW_SNOW);
    s.forest = (t == OW_FOREST);
    s.sand = (t == OW_SAND);
    s.road = (t == OW_DIRT);
    return s;
}

bool worldNearSpecialSite(int tx, int ty) {
    return worldGetPlateAtTile(tx, ty).site != OW_SITE_NONE;
}

int worldTrailAt(int tx, int ty) {
    return worldTerrainAt(tx, ty) == OW_DIRT ? 1 : 0;
}

Landmark northRoadCavern() {
    Landmark lm{};
    lm.tx = 12;
    lm.ty = -18;
    return lm;
}

std::string compassHint(float fromX, float fromY, float toX, float toY) {
    float dx = toX - fromX;
    float dy = toY - fromY;
    int dist = (int)std::sqrt(dx * dx + dy * dy);
    if (dist < 4) return "nearby";
    const char* ns = (std::abs(dy) > std::abs(dx) * 0.4f) ? (dy < 0 ? "N" : "S") : "";
    const char* ew = (std::abs(dx) > std::abs(dy) * 0.4f) ? (dx > 0 ? "E" : "W") : "";
    return std::string(ns) + ew + " (" + std::to_string(dist) + " tiles)";
}

int buildingDoorTx(const Building& b) {
    return b.tx + b.w / 2;
}

int buildingDoorTy(const Building& b) {
    return b.ty + b.h;
}

int tileAt(int tx, int ty) {
    return (int)worldTerrainAt(tx, ty);
}

bool isTileSolid(int tx, int ty) {
    return !worldTileWalkable(tx, ty);
}

void ensureChunksAround(int tx, int ty) {
    int px = floorDiv(tx, PLATE_TILES);
    int py = floorDiv(ty, PLATE_TILES);
    int rx = floorDiv(px, OW_REGION_COLS);
    int ry = floorDiv(py, OW_REGION_ROWS);
    worldGetRegion(rx, ry);
}

void updateWorldStreaming(float /*dt*/) {
    ensureChunksAround((int)g.px, (int)g.py);
}
