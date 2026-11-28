// overworld_gen.cpp — pure deterministic unbounded Lufia II-style overworld generator.
#include "overworld_gen.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <unordered_map>

namespace apfa {
namespace {

inline int floorDiv(int a, int b) {
    int q = a / b;
    int r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) --q;
    return q;
}

inline int floorMod(int a, int b) {
    int r = a % b;
    if (r < 0) r += b;
    return r;
}

inline std::uint32_t mix32(std::uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

float fadeCurve(float t) { return t * t * (3.0f - 2.0f * t); }

// Raw continuous continental elevation (0..1). >= 0.43 is land, < 0.43 is sea.
float rawElevation(std::uint32_t seed, int tx, int ty) {
    // Low-frequency macro continents (~36 tile wavelength) + coastline detail
    float macro = owFbm(tx * 0.026f + 11.3f, ty * 0.026f - 7.7f, seed ^ 0x1A2B3C4Du, 3);
    float detail = owSmoothNoise(tx * 0.095f - 3.1f, ty * 0.095f + 5.9f, seed ^ 0x55AA1234u);
    float e = macro * 0.80f + detail * 0.20f;

    // Gentle continental bias around the homeland peninsula (0,0) so Elmswick
    // and its neighbouring homeland sites form a coherent Lufia-style starting
    // continent with scenic southern, eastern and western ocean bays.
    float dist = std::sqrt((float)(tx * tx + ty * ty));
    if (dist < 17.0f) {
        float w = 1.0f - dist / 17.0f;
        e = e * (1.0f - w * 0.55f) + 0.61f * (w * 0.55f);
    }
    // Carve scenic coastal bays around the homeland so sparkling ocean frames
    // the continent on the south and southeast/west like Lufia II's starter isle.
    float bayDx = (float)(tx - 2);
    float bayDy = (float)(ty - 17);
    float bayD = std::sqrt(bayDx * bayDx * 0.30f + bayDy * bayDy);
    if (bayD < 7.5f) {
        float w = 1.0f - bayD / 7.5f;
        e -= w * 0.42f;
    }
    return e;
}

float rawTemperature(std::uint32_t seed, int tx, int ty) {
    // North trends snow/frost, south trends desert/sand, with organic regional
    // climate waves across distant continents.
    float n = owFbm(tx * 0.019f + 23.1f, ty * 0.019f + 17.4f, seed ^ 0xB10E77u, 2);
    float lat = 0.0f;
    if (ty < -18) lat -= std::min(0.42f, (float)(-18 - ty) * 0.015f);
    if (ty > 18)  lat += std::min(0.42f, (float)(ty - 18) * 0.015f);
    // Keep the immediate starting homeland temperate green
    float d0 = std::sqrt((float)(tx * tx + ty * ty));
    if (d0 < 15.0f) {
        float keep = 1.0f - d0 / 15.0f;
        return 0.50f * keep + (n + lat) * (1.0f - keep);
    }
    // Far continents alternate naturally in large climate bands
    float wave = 0.18f * std::sin((float)ty * 0.014f + (float)(seed & 255) * 0.02f);
    return n + lat + wave;
}

OwClimate climateAt(std::uint32_t seed, int tx, int ty) {
    float t = rawTemperature(seed, tx, ty);
    if (t < 0.31f) return OWC_SNOW;
    if (t > 0.71f) return OWC_SAND;
    return OWC_TEMPERATE;
}

OwTileKind baseNaturalTile(std::uint32_t seed, int tx, int ty) {
    float e = rawElevation(seed, tx, ty);
    if (e < 0.385f) return OWT_DEEP_SEA;
    if (e < 0.465f) return OWT_SEA;

    OwClimate clim = climateAt(seed, tx, ty);

    // Mountain chains via ridge noise (natural Lufia mountain spines)
    float rNoise = owSmoothNoise(tx * 0.068f + 41.2f, ty * 0.068f + 13.8f, seed ^ 0xA417E9u);
    float ridge = 1.0f - std::fabs(2.0f * rNoise - 1.0f);
    float elevBoost = (e - 0.43f) * 0.65f;
    if (ridge + elevBoost > 0.90f && e > 0.51f) {
        return (clim == OWC_SNOW) ? OWT_MOUNTAIN_SNOW : OWT_MOUNTAIN;
    }

    // Clustered forests & marsh pockets
    float woods = owFbm(tx * 0.115f - 19.3f, ty * 0.115f + 29.7f, seed ^ 0xF02E57u, 2);
    float moist = owSmoothNoise(tx * 0.075f + 7.4f, ty * 0.075f - 31.2f, seed ^ 0x39A1Cu);

    if (clim == OWC_SNOW) {
        if (woods > 0.58f) return OWT_FOREST_SNOW;
        return OWT_SNOW;
    }
    if (clim == OWC_SAND) {
        return OWT_SAND;
    }
    // Temperate
    if (e < 0.475f && moist > 0.66f && woods < 0.52f) {
        return OWT_SWAMP;
    }
    if (woods > 0.56f) {
        return (rawTemperature(seed, tx, ty) < 0.48f) ? OWT_FOREST_PINE : OWT_FOREST_LEAF;
    }
    return OWT_GRASS;
}

OwTileKind dryGroundForClimate(OwClimate c) {
    if (c == OWC_SNOW) return OWT_SNOW;
    if (c == OWC_SAND) return OWT_SAND;
    return OWT_GRASS;
}

// Compute the canonical primary settlement for any macro-cell (cx, cy).
OwSiteInfo primaryTownForCell(std::uint32_t seed, int cx, int cy) {
    if (cx == 0 && cy == 0) {
        return owStartTown(seed);
    }

    OwSiteInfo s;
    s.cellX = cx;
    s.cellY = cy;
    s.seed = owHash2(cx, cy, seed ^ 0x70A17001u);

    const int x0 = cx * OW_CELL_SIZE + 4;
    const int y0 = cy * OW_CELL_SIZE + 4;
    const int span = OW_CELL_SIZE - 8;

    // Pick the best natural land tile in the inner 16x16 of this macro-cell.
    int bestX = x0 + (int)(s.seed % (std::uint32_t)span);
    int bestY = y0 + (int)((s.seed >> 8) % (std::uint32_t)span);
    float bestScore = -999.0f;

    for (int i = 0; i < 16; ++i) {
        std::uint32_t h = owHash2(cx * 31 + i, cy * 37 - i, seed ^ 0x4C11A5u);
        int candX = x0 + (int)(h % (std::uint32_t)span);
        int candY = y0 + (int)((h >> 10) % (std::uint32_t)span);
        float e = rawElevation(seed, candX, candY);
        OwTileKind k = baseNaturalTile(seed, candX, candY);
        float sc = e;
        if (owIsLand(k) && !owIsMountain(k)) sc += 2.0f;
        if (k == OWT_GRASS || k == OWT_SAND || k == OWT_SNOW) sc += 0.5f;
        if (sc > bestScore) {
            bestScore = sc;
            bestX = candX;
            bestY = candY;
        }
    }

    s.tx = bestX;
    s.ty = bestY;
    s.climate = climateAt(seed, s.tx, s.ty);

    // Tier distribution: guarantee at least one City per 2x2 macro block, rest Town/Hamlet
    int bx = floorMod(cx, 2);
    int by = floorMod(cy, 2);
    std::uint32_t blockHash = owHash2(floorDiv(cx, 2), floorDiv(cy, 2), seed ^ 0xC171E5u);
    int citySlotX = (int)(blockHash & 1u);
    int citySlotY = (int)((blockHash >> 1) & 1u);
    if (bx == citySlotX && by == citySlotY && !(cx == 0 && cy == 0)) {
        s.type = OWS_CITY;
    } else {
        s.type = ((s.seed >> 16) % 10u < 5u) ? OWS_TOWN : OWS_HAMLET;
    }

    s.gateMask = 0x0F; // connected to cardinal road network
    s.name = owSettlementName(seed, s.tx, s.ty, s.type, s.climate);
    return s;
}

// Compute all deterministic sites for macro-cell (cx, cy).
std::vector<OwSiteInfo> buildSitesForCell(std::uint32_t seed, int cx, int cy) {
    std::vector<OwSiteInfo> list;
    if (cx == 0 && cy == 0) {
        // The homeland macro-cell holds Elmswick, a nearby second town (Oakford),
        // a coastal harbour hamlet (Saltmere), and the core adventure landmarks.
        list.push_back(owStartTown(seed));
        list.push_back(owStartCavern(seed));
        list.push_back(owBossCastle(seed));
        list.push_back(owLocketSite(seed));
        list.push_back(owTowerSite(seed));

        // Second homeland town to the east (walkable in ~6 seconds along the east road!)
        {
            std::uint32_t h = owHash2(11, 2, seed ^ 0xEA5701u);
            OwSiteInfo eastTown;
            eastTown.type = OWS_HAMLET;
            eastTown.climate = OWC_TEMPERATE;
            eastTown.tx = 10 + (int)(h % 3u);        // 10..12
            eastTown.ty = 1 + (int)((h >> 4) % 3u);  // 1..3
            eastTown.cellX = 0;
            eastTown.cellY = 0;
            eastTown.seed = h;
            eastTown.gateMask = 0x0F;
            eastTown.name = owSettlementName(seed, eastTown.tx, eastTown.ty, eastTown.type, eastTown.climate);
            list.push_back(eastTown);
        }

        // Coastal harbour hamlet south of Elmswick
        {
            std::uint32_t h = owHash2(2, 11, seed ^ 0x50A702u);
            OwSiteInfo port;
            port.type = OWS_HAMLET;
            port.climate = OWC_TEMPERATE;
            port.tx = 1 + (int)(h % 3u);             // 1..3
            port.ty = 10 + (int)((h >> 4) % 2u);     // 10..11
            port.cellX = 0;
            port.cellY = 0;
            port.seed = h;
            port.gateMask = 0x0F;
            port.name = owSettlementName(seed, port.tx, port.ty, port.type, port.climate);
            list.push_back(port);
        }

        return list;
    }

    OwSiteInfo town = primaryTownForCell(seed, cx, cy);
    list.push_back(town);

    // Place 1-2 dungeon/adventure sites in each macro-cell, kept >= 7 tiles from the town.
    const int x0 = cx * OW_CELL_SIZE + 3;
    const int y0 = cy * OW_CELL_SIZE + 3;
    const int span = OW_CELL_SIZE - 6;
    const int extraCount = 1 + (int)((town.seed >> 20) & 1u);

    static const OwSiteType kDungeonPool[5] = {
        OWS_CAVE, OWS_TOWER, OWS_RUINS, OWS_TEMPLE, OWS_CASTLE
    };

    for (int k = 0; k < extraCount; ++k) {
        std::uint32_t dh = owHash2(cx * 17 + k * 5, cy * 23 - k * 7, seed ^ 0xD0A6E000u);
        int bestX = x0 + (int)(dh % (std::uint32_t)span);
        int bestY = y0 + (int)((dh >> 8) % (std::uint32_t)span);
        bool placed = false;
        for (int attempt = 0; attempt < 16; ++attempt) {
            std::uint32_t ah = owHash2(cx * 41 + attempt, cy * 53 + k * 19, seed ^ 0xD0A6E111u);
            int tx = x0 + (int)(ah % (std::uint32_t)span);
            int ty = y0 + (int)((ah >> 10) % (std::uint32_t)span);
            bool tooClose = false;
            for (const auto& existing : list) {
                int dist = std::abs(tx - existing.tx) + std::abs(ty - existing.ty);
                if (dist < 7) { tooClose = true; break; }
            }
            if (tooClose) continue;
            bestX = tx;
            bestY = ty;
            placed = true;
            if (owIsLand(baseNaturalTile(seed, tx, ty))) break;
        }
        if (!placed) continue;

        OwSiteInfo d;
        d.type = kDungeonPool[(dh >> 16) % 5u];
        d.climate = climateAt(seed, bestX, bestY);
        d.tx = bestX;
        d.ty = bestY;
        d.cellX = cx;
        d.cellY = cy;
        d.seed = dh;
        d.name = owSettlementName(seed, d.tx, d.ty, d.type, d.climate);
        list.push_back(d);
    }

    // Add any of the 3 Tidewell Islands that fall inside this macro-cell
    for (int i = 0; i < 3; ++i) {
        OwSiteInfo ti = owTideIsland(seed, i);
        if (floorDiv(ti.tx, OW_CELL_SIZE) == cx && floorDiv(ti.ty, OW_CELL_SIZE) == cy) {
            bool dup = false;
            for (const auto& ex : list) {
                if (std::abs(ex.tx - ti.tx) + std::abs(ex.ty - ti.ty) < 4) { dup = true; break; }
            }
            if (!dup) list.push_back(ti);
        }
    }

    return list;
}

// ================================================================ Chunk Cache
struct OwChunk {
    int cx = 0;
    int cy = 0;
    std::array<OwCellTile, OW_CELL_SIZE * OW_CELL_SIZE> tiles{};
};

struct ChunkKey {
    int cx, cy;
    bool operator==(const ChunkKey& o) const { return cx == o.cx && cy == o.cy; }
};
struct ChunkKeyHash {
    std::size_t operator()(const ChunkKey& k) const {
        return (static_cast<std::size_t>(static_cast<std::uint32_t>(k.cx)) << 32) ^
               static_cast<std::uint32_t>(k.cy);
    }
};

struct OwCache {
    std::uint32_t seed = 0xFFFFFFFFu;
    std::unordered_map<ChunkKey, OwChunk, ChunkKeyHash> chunks;
    std::unordered_map<ChunkKey, std::vector<OwSiteInfo>, ChunkKeyHash> sites;
};

OwCache& cacheFor(std::uint32_t seed) {
    static OwCache cache;
    if (cache.seed != seed) {
        cache.seed = seed;
        cache.chunks.clear();
        cache.sites.clear();
    }
    if (cache.chunks.size() > 512) cache.chunks.clear();
    if (cache.sites.size() > 512) cache.sites.clear();
    return cache;
}

const std::vector<OwSiteInfo>& getSitesCached(std::uint32_t seed, int cx, int cy) {
    OwCache& c = cacheFor(seed);
    ChunkKey key{cx, cy};
    auto it = c.sites.find(key);
    if (it != c.sites.end()) return it->second;
    auto [ins, _] = c.sites.emplace(key, buildSitesForCell(seed, cx, cy));
    return ins->second;
}

// Paint a single road step into chunk (cx, cy) if (tx, ty) falls inside it.
void stampRoadPoint(OwChunk& ch, int tx, int ty, bool horizontal) {
    int lx = tx - ch.cx * OW_CELL_SIZE;
    int ly = ty - ch.cy * OW_CELL_SIZE;
    if (lx < 0 || lx >= OW_CELL_SIZE || ly < 0 || ly >= OW_CELL_SIZE) return;
    OwCellTile& cell = ch.tiles[static_cast<std::size_t>(ly) * OW_CELL_SIZE + static_cast<std::size_t>(lx)];
    if (owIsWater(cell.kind)) {
        cell.kind = horizontal ? OWT_BRIDGE_H : OWT_BRIDGE_V;
        cell.hasRoad = true;
    } else if (owIsMountain(cell.kind)) {
        cell.kind = horizontal ? OWT_PASS_H : OWT_PASS_V;
        cell.hasRoad = true;
    } else {
        // Clear dense forest/swamp cleanly along the royal road
        if (cell.kind == OWT_FOREST_LEAF || cell.kind == OWT_FOREST_PINE || cell.kind == OWT_SWAMP)
            cell.kind = OWT_GRASS;
        else if (cell.kind == OWT_FOREST_SNOW)
            cell.kind = OWT_SNOW;
        cell.hasRoad = true;
    }
}

// Carve a clean single-elbow L-road from (ax, ay) to (bx, by) so roads never
// create parallel "ladder" artifacts while remaining 100% symmetric across chunks.
void carveRoadSegmentIntoChunk(OwChunk& ch, std::uint32_t seed, int ax, int ay, int bx, int by,
                               bool forceVertFirst = false) {
    const int minX = ch.cx * OW_CELL_SIZE - 2;
    const int maxX = (ch.cx + 1) * OW_CELL_SIZE + 2;
    const int minY = ch.cy * OW_CELL_SIZE - 2;
    const int maxY = (ch.cy + 1) * OW_CELL_SIZE + 2;
    int rMinX = std::min(ax, bx) - 2, rMaxX = std::max(ax, bx) + 2;
    int rMinY = std::min(ay, by) - 2, rMaxY = std::max(ay, by) + 2;
    if (rMaxX < minX || rMinX > maxX || rMaxY < minY || rMinY > maxY) return;

    if (!forceVertFirst && (ax > bx || (ax == bx && ay > by))) {
        std::swap(ax, bx);
        std::swap(ay, by);
    }
    bool hFirst = forceVertFirst ? false : (std::abs(bx - ax) >= std::abs(by - ay));
    int x = ax, y = ay;
    if (hFirst) {
        while (x != bx) {
            stampRoadPoint(ch, x, y, true);
            x += (bx > x) ? 1 : -1;
        }
        while (y != by) {
            stampRoadPoint(ch, x, y, false);
            y += (by > y) ? 1 : -1;
        }
    } else {
        while (y != by) {
            stampRoadPoint(ch, x, y, false);
            y += (by > y) ? 1 : -1;
        }
        while (x != bx) {
            stampRoadPoint(ch, x, y, true);
            x += (bx > x) ? 1 : -1;
        }
    }
    stampRoadPoint(ch, bx, by, hFirst);
    (void)seed;
}

OwChunk generateChunk(std::uint32_t seed, int cx, int cy) {
    OwChunk ch;
    ch.cx = cx;
    ch.cy = cy;
    const int baseX = cx * OW_CELL_SIZE;
    const int baseY = cy * OW_CELL_SIZE;

    // 1. Base continuous terrain
    for (int ly = 0; ly < OW_CELL_SIZE; ++ly) {
        for (int lx = 0; lx < OW_CELL_SIZE; ++lx) {
            int tx = baseX + lx;
            int ty = baseY + ly;
            OwCellTile t;
            t.kind = baseNaturalTile(seed, tx, ty);
            t.variant = static_cast<std::uint8_t>(owHash2(tx, ty, seed ^ 0x9933u) & 7u);
            ch.tiles[static_cast<std::size_t>(ly) * OW_CELL_SIZE + static_cast<std::size_t>(lx)] = t;
        }
    }

    // 2. Collect sites from this cell and all 8 neighbours so site pads and
    // connecting roads never break at chunk borders.
    for (int ny = cy - 1; ny <= cy + 1; ++ny) {
        for (int nx = cx - 1; nx <= cx + 1; ++nx) {
            const auto& sites = getSitesCached(seed, nx, ny);
            for (const auto& s : sites) {
                int padR = (s.tideIslandIdx >= 0) ? 1 : (owIsSettlement(s.type) ? 2 : 1);
                OwTileKind padGround = dryGroundForClimate(s.climate);
                for (int dy = -padR; dy <= padR; ++dy) {
                    for (int dx = -padR; dx <= padR; ++dx) {
                        if (padR == 2 && std::abs(dx) == 2 && std::abs(dy) == 2) continue;
                        int lx = (s.tx + dx) - baseX;
                        int ly = (s.ty + dy) - baseY;
                        if (lx < 0 || lx >= OW_CELL_SIZE || ly < 0 || ly >= OW_CELL_SIZE) continue;
                        OwCellTile& cell = ch.tiles[static_cast<std::size_t>(ly) * OW_CELL_SIZE + static_cast<std::size_t>(lx)];
                        // Ensure every site sits on clear, dry, non-mountain ground
                        if (std::abs(dx) <= 1 && std::abs(dy) <= 1) {
                            cell.kind = padGround;
                        } else if (owIsWater(cell.kind) || owIsMountain(cell.kind)) {
                            cell.kind = padGround;
                        }
                    }
                }
            }
        }
    }

    // 3. Carve the connected road network (with automatic bridges & passes!)
    //    Every macro-cell's primary town links E, S, W, N and to local sites.
    for (int ny = cy - 1; ny <= cy + 1; ++ny) {
        for (int nx = cx - 1; nx <= cx + 1; ++nx) {
            const auto& sites = getSitesCached(seed, nx, ny);
            if (sites.empty()) continue;
            const OwSiteInfo& hub = sites[0]; // primary town of (nx, ny)

            // Connect hub -> east neighbor's hub
            const auto& eastSites = getSitesCached(seed, nx + 1, ny);
            if (!eastSites.empty())
                carveRoadSegmentIntoChunk(ch, seed, hub.tx, hub.ty, eastSites[0].tx, eastSites[0].ty);

            // Connect hub -> south neighbor's hub
            const auto& southSites = getSitesCached(seed, nx, ny + 1);
            if (!southSites.empty())
                carveRoadSegmentIntoChunk(ch, seed, hub.tx, hub.ty, southSites[0].tx, southSites[0].ty);

            // Connect hub -> every other land site in this cell (skip Tidewell ocean islands!)
            for (std::size_t i = 1; i < sites.size(); ++i) {
                if (sites[i].tideIslandIdx >= 0) continue;
                carveRoadSegmentIntoChunk(ch, seed, sites[i].tx, sites[i].ty, hub.tx, hub.ty, true);
            }
        }
    }

    // 4. Keep site center tiles clean of bridge/pass overwrites
    for (int ny = cy - 1; ny <= cy + 1; ++ny) {
        for (int nx = cx - 1; nx <= cx + 1; ++nx) {
            for (const auto& s : getSitesCached(seed, nx, ny)) {
                int lx = s.tx - baseX;
                int ly = s.ty - baseY;
                if (lx < 0 || lx >= OW_CELL_SIZE || ly < 0 || ly >= OW_CELL_SIZE) continue;
                OwCellTile& cell = ch.tiles[static_cast<std::size_t>(ly) * OW_CELL_SIZE + static_cast<std::size_t>(lx)];
                cell.kind = dryGroundForClimate(s.climate);
            }
        }
    }

    return ch;
}

const OwChunk& getChunkCached(std::uint32_t seed, int cx, int cy) {
    OwCache& c = cacheFor(seed);
    ChunkKey key{cx, cy};
    auto it = c.chunks.find(key);
    if (it != c.chunks.end()) return it->second;
    auto [ins, _] = c.chunks.emplace(key, generateChunk(seed, cx, cy));
    return ins->second;
}

} // namespace

// ================================================================ Public API
std::uint32_t owHash2(int x, int y, std::uint32_t seed) {
    std::uint32_t h = seed
        ^ static_cast<std::uint32_t>(x) * 2654435761u
        ^ static_cast<std::uint32_t>(y) * 2246822519u
        ^ (static_cast<std::uint32_t>(x) * 374761393u +
           static_cast<std::uint32_t>(y) * 668265263u) * 3266489917u;
    return mix32(h);
}

float owHashF(int x, int y, std::uint32_t seed) {
    return static_cast<float>(owHash2(x, y, seed)) / 4294967295.0f;
}

float owSmoothNoise(float x, float y, std::uint32_t seed) {
    int xi = static_cast<int>(std::floor(x));
    int yi = static_cast<int>(std::floor(y));
    float xf = x - xi;
    float yf = y - yi;
    float v00 = owHashF(xi,     yi,     seed);
    float v10 = owHashF(xi + 1, yi,     seed);
    float v01 = owHashF(xi,     yi + 1, seed);
    float v11 = owHashF(xi + 1, yi + 1, seed);
    float u = fadeCurve(xf);
    float v = fadeCurve(yf);
    return v00 + (v10 - v00) * u + (v01 - v00) * v + (v00 - v10 - v01 + v11) * u * v;
}

float owFbm(float x, float y, std::uint32_t seed, int octaves) {
    float amp = 0.5f, freq = 1.0f, sum = 0.0f, norm = 0.0f;
    for (int i = 0; i < octaves; ++i) {
        sum += owSmoothNoise(x * freq, y * freq, seed + static_cast<std::uint32_t>(i) * 1013u) * amp;
        norm += amp;
        amp *= 0.5f;
        freq *= 2.0f;
    }
    return sum / norm;
}

bool owIsWater(OwTileKind k) {
    return k == OWT_SEA || k == OWT_DEEP_SEA;
}

bool owIsLand(OwTileKind k) {
    return !owIsWater(k);
}

bool owIsMountain(OwTileKind k) {
    return k == OWT_MOUNTAIN || k == OWT_MOUNTAIN_SNOW;
}

bool owIsWalkableOnFoot(OwTileKind k) {
    return !owIsWater(k) && !owIsMountain(k);
}

bool owIsSailable(OwTileKind k) {
    return k == OWT_SEA || k == OWT_DEEP_SEA || k == OWT_BRIDGE_H || k == OWT_BRIDGE_V;
}

bool owIsSettlement(OwSiteType t) {
    return t == OWS_HAMLET || t == OWS_TOWN || t == OWS_CITY;
}

bool owIsDungeonSite(OwSiteType t) {
    return t == OWS_CAVE || t == OWS_TOWER || t == OWS_TEMPLE ||
           t == OWS_RUINS || t == OWS_CASTLE;
}

OwCellTile owSampleTile(std::uint32_t seed, int tx, int ty) {
    int cx = floorDiv(tx, OW_CELL_SIZE);
    int cy = floorDiv(ty, OW_CELL_SIZE);
    int lx = floorMod(tx, OW_CELL_SIZE);
    int ly = floorMod(ty, OW_CELL_SIZE);
    const OwChunk& ch = getChunkCached(seed, cx, cy);
    return ch.tiles[static_cast<std::size_t>(ly) * OW_CELL_SIZE + static_cast<std::size_t>(lx)];
}

void owSitesInCell(std::uint32_t seed, int cellX, int cellY, std::vector<OwSiteInfo>& out) {
    const auto& sites = getSitesCached(seed, cellX, cellY);
    out.insert(out.end(), sites.begin(), sites.end());
}

void owSitesInRect(std::uint32_t seed, int x0, int y0, int x1, int y1, std::vector<OwSiteInfo>& out) {
    out.clear();
    int cx0 = floorDiv(x0 - 2, OW_CELL_SIZE);
    int cy0 = floorDiv(y0 - 2, OW_CELL_SIZE);
    int cx1 = floorDiv(x1 + 2, OW_CELL_SIZE);
    int cy1 = floorDiv(y1 + 2, OW_CELL_SIZE);
    for (int cy = cy0; cy <= cy1; ++cy) {
        for (int cx = cx0; cx <= cx1; ++cx) {
            const auto& sites = getSitesCached(seed, cx, cy);
            for (const auto& s : sites) {
                if (s.tx >= x0 - 2 && s.tx <= x1 + 2 && s.ty >= y0 - 2 && s.ty <= y1 + 2)
                    out.push_back(s);
            }
        }
    }
}

bool owSiteAtTile(std::uint32_t seed, int tx, int ty, OwSiteInfo& out) {
    int cx = floorDiv(tx, OW_CELL_SIZE);
    int cy = floorDiv(ty, OW_CELL_SIZE);
    for (int ny = cy - 1; ny <= cy + 1; ++ny) {
        for (int nx = cx - 1; nx <= cx + 1; ++nx) {
            for (const auto& s : getSitesCached(seed, nx, ny)) {
                if (s.tx == tx && s.ty == ty) {
                    out = s;
                    return true;
                }
            }
        }
    }
    return false;
}

bool owNearestSite(std::uint32_t seed, int tx, int ty, int maxDist, OwSiteInfo& out,
                   bool settlementsOnly, bool dungeonsOnly) {
    std::vector<OwSiteInfo> list;
    owSitesInRect(seed, tx - maxDist, ty - maxDist, tx + maxDist, ty + maxDist, list);
    int bestD2 = maxDist * maxDist + 1;
    bool found = false;
    for (const auto& s : list) {
        if (settlementsOnly && !owIsSettlement(s.type)) continue;
        if (dungeonsOnly && !owIsDungeonSite(s.type)) continue;
        int dx = s.tx - tx, dy = s.ty - ty;
        int d2 = dx * dx + dy * dy;
        if (d2 < bestD2) {
            bestD2 = d2;
            out = s;
            found = true;
        }
    }
    return found;
}

std::string owSettlementName(std::uint32_t seed, int tx, int ty, OwSiteType type, OwClimate climate) {
    std::uint32_t h = owHash2(tx, ty, seed ^ 0x4A3E01u);

    if (owIsSettlement(type)) {
        static const char* kPreTemp[] = {
            "Oak", "Mill", "Red", "Alder", "Briar", "Green", "Elm", "High",
            "West", "Gold", "Fern", "Ash", "River", "Brook", "Willow", "Thorn"
        };
        static const char* kPreSand[] = {
            "Sun", "Dune", "Amber", "Gold", "Red", "Cinder", "Mirage", "Oasis",
            "Copper", "Saffron", "Salt", "Sand"
        };
        static const char* kPreSnow[] = {
            "Frost", "Snow", "Ice", "Pine", "Winter", "White", "North", "Silver",
            "Rime", "Glacier", "Storm", "Cold"
        };
        static const char* kSufHamlet[] = { "ford", "wick", "brook", "glen", "stead", "well", "mere", "croft" };
        static const char* kSufTown[]   = { "haven", "bridge", "field", "wood", "port", "ridge", "vale", "holm" };
        static const char* kSufCity[]   = { "hold", "spire", "keep", "crown", "gate", "guard", "crest", "bastion" };

        const char* pre = nullptr;
        if (climate == OWC_SAND) pre = kPreSand[h % 12u];
        else if (climate == OWC_SNOW) pre = kPreSnow[h % 12u];
        else pre = kPreTemp[h % 16u];

        const char* suf = (type == OWS_CITY) ? kSufCity[(h >> 8) % 8u]
                        : (type == OWS_TOWN) ? kSufTown[(h >> 8) % 8u]
                                             : kSufHamlet[(h >> 8) % 8u];
        return std::string(pre) + suf;
    }

    static const char* kAdj[] = {
        "Gloam", "Whispering", "Sunken", "Silent", "Iron", "Ashen",
        "Verdant", "Ancient", "Frost", "Shattered", "Dusk", "Starlit"
    };
    const char* adj = kAdj[h % 12u];
    switch (type) {
        case OWS_CAVE:   return std::string(adj) + " Cavern";
        case OWS_TOWER:  return std::string(adj) + " Tower";
        case OWS_TEMPLE: return std::string(adj) + " Shrine";
        case OWS_RUINS:  return std::string(adj) + " Ruins";
        case OWS_CASTLE: return std::string(adj) + " Keep";
        case OWS_CAMP:   return std::string(adj) + " Camp";
        default:         return "Outpost";
    }
}

std::string owRegionTitle(std::uint32_t seed, int tx, int ty) {
    if (std::abs(tx) <= 15 && std::abs(ty) <= 14) return "Elmswick Vale";
    int rx = floorDiv(tx, OW_CELL_SIZE);
    int ry = floorDiv(ty, OW_CELL_SIZE);
    OwClimate clim = climateAt(seed, tx, ty);
    std::uint32_t h = owHash2(rx, ry, seed ^ 0x2E610Bu);
    static const char* kTemp[] = { "Greenmarch", "The Crownlands", "Alder Reach", "Highmeadow", "Silvervale", "Westweald" };
    static const char* kSand[] = { "Sunscorch Sands", "Amber Wastes", "Dune Reach", "Golden Flats" };
    static const char* kSnow[] = { "Frostmarch", "Whitepeak", "Rimehold", "Northern Tundra" };
    if (clim == OWC_SAND) return kSand[h % 4u];
    if (clim == OWC_SNOW) return kSnow[h % 4u];
    return kTemp[h % 6u];
}

int owDangerTier(int tx, int ty) {
    float dist = std::sqrt(static_cast<float>(tx * tx + ty * ty));
    if (dist < 10.0f) return 1;
    if (dist < 22.0f) return 2;
    if (dist < 38.0f) return 3;
    if (dist < 58.0f) return 4;
    if (dist < 84.0f) return 5;
    if (dist < 115.0f) return 6;
    return 7;
}

OwSiteInfo owStartTown(std::uint32_t seed) {
    OwSiteInfo s;
    s.type = OWS_TOWN;
    s.climate = OWC_TEMPERATE;
    s.tx = 0;
    s.ty = 0;
    s.cellX = 0;
    s.cellY = 0;
    s.seed = seed ^ 0xE13571Cu;
    s.gateMask = 0x0F;
    s.isStartTown = true;
    s.name = "Elmswick";
    return s;
}

OwSiteInfo owStartCavern(std::uint32_t seed) {
    std::uint32_t h = owHash2(-6, -4, seed ^ 0xCA7E01u);
    OwSiteInfo s;
    s.type = OWS_CAVE;
    s.climate = OWC_TEMPERATE;
    s.tx = -7 + (int)(h % 3u);       // -7..-5
    s.ty = -5 + (int)((h >> 4) % 3u); // -5..-3
    s.cellX = 0;
    s.cellY = 0;
    s.seed = h;
    s.isStartCavern = true;
    s.name = "Hollow Hill";
    return s;
}

OwSiteInfo owBossCastle(std::uint32_t seed) {
    std::uint32_t h = owHash2(9, -9, seed ^ 0xCA571Eu);
    OwSiteInfo s;
    s.type = OWS_CASTLE;
    s.climate = OWC_TEMPERATE;
    s.tx = 8 + (int)(h % 3u);        // 8..10
    s.ty = -10 + (int)((h >> 4) % 3u); // -10..-8
    s.cellX = 0;
    s.cellY = 0;
    s.seed = h;
    s.isBossCastle = true;
    s.name = "Castle of Elm";
    return s;
}

OwSiteInfo owLocketSite(std::uint32_t seed) {
    std::uint32_t h = owHash2(-9, 5, seed ^ 0x10C4E7u);
    OwSiteInfo s;
    s.type = OWS_RUINS;
    s.climate = OWC_TEMPERATE;
    s.tx = -10 + (int)(h % 3u);      // -10..-8
    s.ty = 4 + (int)((h >> 4) % 3u); // 4..6
    s.cellX = 0;
    s.cellY = 0;
    s.seed = h;
    s.isLocketSite = true;
    s.name = "Standing Stones";
    return s;
}

OwSiteInfo owTowerSite(std::uint32_t seed) {
    std::uint32_t h = owHash2(-11, -7, seed ^ 0x70AE21u);
    OwSiteInfo s;
    s.type = OWS_TOWER;
    s.climate = OWC_TEMPERATE;
    s.tx = -12 + (int)(h % 3u);      // -12..-10
    s.ty = -8 + (int)((h >> 4) % 2u); // -8..-7
    s.cellX = 0;
    s.cellY = 0;
    s.seed = h;
    s.isTowerSite = true;
    s.name = "Old Watchtower";
    return s;
}

OwSiteInfo owTideIsland(std::uint32_t seed, int idx) {
    static const char* kNames[3] = { "Tidewell Cay", "Lantern Shoal", "Saltspire Isle" };
    static const int kBaseX[3] = { -4, 4, 12 };
    static const int kBaseY[3] = { 19, 20, 17 };
    int i = std::max(0, std::min(2, idx));
    std::uint32_t h = owHash2(kBaseX[i], kBaseY[i], seed ^ 0x71DE00u);
    OwSiteInfo s;
    s.type = OWS_CAMP;
    s.climate = OWC_TEMPERATE;
    s.tx = kBaseX[i] + (int)(h % 3u) - 1;
    s.ty = kBaseY[i] + (int)((h >> 4) % 3u) - 1;
    s.cellX = floorDiv(s.tx, OW_CELL_SIZE);
    s.cellY = floorDiv(s.ty, OW_CELL_SIZE);
    s.seed = h;
    s.tideIslandIdx = i;
    s.name = kNames[i];
    return s;
}

} // namespace apfa
