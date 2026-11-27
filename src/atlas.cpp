// atlas.cpp — the Survey: guaranteed places, joined by roads, with names.
//
// See atlas.hpp for why this exists. The short version: the old world rolled
// dice per sector and lost. Every lattice cell here is owed one place, placement
// is a pure function of the seed, and a fixed fallback ladder means a cell only
// goes unclaimed when it is genuinely open sea.
#include "atlas.hpp"
#include "world.hpp"
#include "commons.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <queue>
#include <string>
#include <tuple>
#include <vector>

// ================================================================ hashing
static inline uint32_t atlasHash(int x, int y, uint32_t salt) {
    uint32_t h = (uint32_t)x * 0x9e3779b1u ^ (uint32_t)y * 0x85ebca6bu ^ salt * 0xc2b2ae35u;
    h ^= h >> 16; h *= 0x7feb352du;
    h ^= h >> 15; h *= 0x846ca68bu;
    h ^= h >> 16;
    return h;
}

// ================================================================ caches
namespace {

struct SiteCell {
    int cx = 0, cy = 0;
    bool exists = false;
    AtlasSite site;
};

// One cache shared by the three lattices, keyed by (seed, lattice, cell).
std::map<std::tuple<uint32_t, int, int, int>, SiteCell> gSiteCache;
uint32_t gSiteSeed = 0;

struct RoadPath {
    std::vector<std::pair<int, int>> pts;
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    bool empty() const { return pts.empty(); }
};
std::map<std::tuple<uint32_t, int, int, int>, RoadPath> gRoadCache;   // seed, cellX, cellY, dir
std::map<std::tuple<uint32_t, int, int>, std::vector<RoadPath>> gSpurCache; // seed, 0, 0
std::map<std::tuple<uint32_t, int, int>, std::vector<uint8_t>> gChunkCache; // seed, chunkX, chunkY

constexpr int ROAD_CHUNK = 32;               // road mask resolution, tiles
constexpr int ROAD_STEP = 6;                 // routing grid, tiles per node
constexpr int ROAD_CACHE_MAX = 4096;         // polylines (~1MB) before a reset
constexpr int CHUNK_CACHE_MAX = 2048;        // 1KB each

// 0 = no road, 1 = runs N/S, 2 = runs E/W, 3 = crossing
uint8_t roadAxisAt(int tx, int ty);
bool originRoadPaths(std::vector<RoadPath>& out);

void cacheGuard() {
    if (gSiteSeed == g.worldSeed) return;
    gSiteSeed = g.worldSeed;
    gSiteCache.clear();
    gRoadCache.clear();
    gSpurCache.clear();
    gChunkCache.clear();
}

// ================================================================ terrace reads
// A spot is "clear" when a building, a camp or a cave mouth could stand on it.
bool spotClear(int tx, int ty, bool allowSwamp, bool allowIce, bool allowCrag) {
    SurveyTerrain st = worldSurveyTerrain(tx, ty);
    if (st.water || st.deep) return false;
    if (st.crag && !allowCrag) return false;
    if (st.ice && !allowIce) return false;
    if (st.swamp && !allowSwamp) return false;
    return true;
}

bool nearStory(int tx, int ty, int pad) {
    if (abs(tx - BOSS_CASTLE_TX) < pad + 10 && abs(ty - BOSS_CASTLE_TY) < pad + 10) return true;
    if (abs(tx - LOCKET_SITE_TX) < pad + 6 && abs(ty - LOCKET_SITE_TY) < pad + 6) return true;
    if (abs(tx - TOWER_SITE_TX) < pad + 6 && abs(ty - TOWER_SITE_TY) < pad + 6) return true;
    return false;
}

// ================================================================ candidate ladder
struct Cand { int x, y; };

// The heart of a cell, plus a deterministic spiral of eight spots per ring,
// never further from the cell centre than `reach`. Keeping the reach below half
// the gap between cells is what keeps two settlements — or two cave mouths —
// from crowding each other: the spacing is a property of the lattice, not a
// question one cell has to ask its neighbours (asking would recurse forever).
// 28 spots per cell is still a generous ladder for rough country.
void cellCandidates(int cx, int cy, int period, int jitter, int reach, uint32_t salt,
                    std::vector<Cand>& out) {
    out.clear();
    int baseX = cx * period + period / 2;
    int baseY = cy * period + period / 2;
    uint32_t h = atlasHash(cx, cy, salt);
    out.push_back({ baseX + (int)(h % (uint32_t)(2 * jitter + 1)) - jitter,
                    baseY + (int)((h >> 9) % (uint32_t)(2 * jitter + 1)) - jitter });
    const int radii[3] = { reach / 2, (reach * 3) / 4, reach };
    for (int r : radii) {
        int ang0 = (int)((h >> 18) & 7u);
        for (int a = 0; a < 8; ++a) {
            float ang = (float)((ang0 + a) & 7) * 0.7853982f;
            out.push_back({ baseX + (int)roundf(cosf(ang) * (float)r),
                            baseY + (int)roundf(sinf(ang) * (float)r) });
        }
    }
}

} // namespace

// ================================================================ villages
namespace {

// A village needs a whole footprint of dry, buildable ground — not just a centre.
// 17 probes: the heart, a ring at 7 tiles and a ring at 13 (inside a 15-tile plot).
int villageScore(int tx, int ty) {
    if (tx * tx + ty * ty < 58 * 58) return 0;      // Elmswick's meadow is hers
    if (nearStory(tx, ty, 26)) return 0;
    const int TOTAL = 17;
    int score = 0, seen = 0;
    for (int ring = 0; ring <= 2; ++ring) {
        static const int radius[3] = { 0, 7, 13 };
        int r = radius[ring];
        int probes = (r == 0) ? 1 : 8;
        for (int a = 0; a < probes; ++a) {
            float ang = (float)a * 0.7853982f;
            int px = tx + (int)roundf(cosf(ang) * (float)r);
            int py = ty + (int)roundf(sinf(ang) * (float)r);
            seen++;
            if (spotClear(px, py, false, false, false)) score++;
            else if (score + (TOTAL - seen) < 12) return 0;   // cannot reach the bar
        }
    }
    return score;
}

SiteCell placeVillage(int cx, int cy) {
    SiteCell out; out.cx = cx; out.cy = cy; out.exists = false;
    std::vector<Cand> cands;
    cellCandidates(cx, cy, ATLAS_VILLAGE_CELL, 17, 17, g.worldSeed ^ 0x71a9eu, cands);
    int bestScore = 0, bestX = 0, bestY = 0;
    for (const Cand& c : cands) {
        int s = villageScore(c.x, c.y);
        if (s >= 17) { bestScore = s; bestX = c.x; bestY = c.y; break; }
        if (s > bestScore) { bestScore = s; bestX = c.x; bestY = c.y; }
    }
    if (bestScore < 12) return out;                 // open sea / all crag: leave it empty
    out.exists = true;
    out.site.kind = AS_VILLAGE;
    out.site.tx = bestX; out.site.ty = bestY;
    out.site.cellX = cx; out.site.cellY = cy;
    out.site.seed = atlasHash(cx, cy, g.worldSeed ^ 0x5eed71u);
    return out;
}

SiteCell placeLandmark(int cx, int cy) {
    SiteCell out; out.cx = cx; out.cy = cy; out.exists = false;
    std::vector<Cand> cands;
    cellCandidates(cx, cy, ATLAS_LANDMARK_CELL, 24, 24, g.worldSeed ^ 0x1a4d4u, cands);
    // Kinds come from their own hash. The old code sliced the kind out of the
    // same word that gated the roll, which is why Ruins never once appeared.
    uint32_t kh = atlasHash(cx, cy, g.worldSeed ^ 0x9e3b17u);
    int kind = AS_RUINS + (int)(kh % 5u);
    for (const Cand& c : cands) {
        if (c.x * c.x + c.y * c.y < 44 * 44) continue;
        if (nearStory(c.x, c.y, 22)) continue;
        if (!spotClear(c.x, c.y, true, true, false)) continue;
        AtlasSite village;
        if (atlasVillageNear(c.x, c.y, 26, village)) continue;   // towns own their bounds
        out.exists = true;
        out.site.kind = kind;
        out.site.tx = c.x; out.site.ty = c.y;
        out.site.cellX = cx; out.site.cellY = cy;
        out.site.seed = atlasHash(cx, cy, g.worldSeed ^ 0x4a11e5u);
        return out;
    }
    return out;
}

SiteCell placeCavern(int cx, int cy) {
    SiteCell out; out.cx = cx; out.cy = cy; out.exists = false;
    std::vector<Cand> cands;
    cellCandidates(cx, cy, ATLAS_CAVERN_CELL, 21, 21, g.worldSeed ^ 0xCA7E51u, cands);
    for (const Cand& c : cands) {
        if (c.x * c.x + c.y * c.y < 46 * 46) continue;           // Hollow Hill owns that spur
        if (nearStory(c.x, c.y, 26)) continue;
        SurveyTerrain st = worldSurveyTerrain(c.x, c.y);
        if (st.water || st.deep || st.crag) continue;
        if (st.brookDistance < 5.0f) continue;
        if (st.biome == BI_FEN) continue;
        if (st.elevation < 0.34f || st.elevation > 0.76f) continue;
        AtlasSite village;
        if (atlasVillageNear(c.x, c.y, 30, village)) continue;
        AtlasSite landmark;
        if (atlasLandmarkNear(c.x, c.y, 18, landmark)) continue;
        // A mouth ringed by sea is a mouth nobody can walk to.
        int dry = 0, seen = 0;
        for (int dy = -6; dy <= 6; dy += 6)
            for (int dx = -6; dx <= 6; dx += 6) {
                SurveyTerrain n = worldSurveyTerrain(c.x + dx, c.y + dy);
                seen++;
                if (!n.water && !n.deep) dry++;
                else if (dry + (9 - seen) < 7) break;
            }
        if (dry < 7) continue;
        out.exists = true;
        out.site.kind = AS_CAVERN;
        out.site.tx = c.x; out.site.ty = c.y;
        out.site.cellX = cx; out.site.cellY = cy;
        out.site.seed = atlasHash(cx, cy, g.worldSeed ^ 0xd0e5a1u) | 1u;
        return out;
    }
    return out;
}


SiteCell& cachedSite(int lattice, int cx, int cy) {
    static SiteCell missing;
    auto key = std::make_tuple(g.worldSeed, lattice, cx, cy);
    auto it = gSiteCache.find(key);
    if (it != gSiteCache.end()) return it->second;
    if (gSiteCache.size() > 6000) gSiteCache.clear();
    SiteCell cell;
    switch (lattice) {
        case AS_VILLAGE:  cell = placeVillage(cx, cy); break;
        case AS_CAVERN:   cell = placeCavern(cx, cy); break;
        default:          cell = placeLandmark(cx, cy); break;
    }
    cell.cx = cx; cell.cy = cy;
    auto res = gSiteCache.emplace(key, cell);
    return res.first->second;
}

} // namespace

bool atlasVillageInCell(int cx, int cy, AtlasSite& out) {
    cacheGuard();
    const SiteCell& c = cachedSite(AS_VILLAGE, cx, cy);
    if (!c.exists) return false;
    out = c.site;
    return true;
}

bool atlasLandmarkInCell(int cx, int cy, AtlasSite& out) {
    cacheGuard();
    const SiteCell& c = cachedSite(AS_LANDMARK, cx, cy);
    if (!c.exists) return false;
    out = c.site;
    return true;
}

bool atlasCavernInCell(int cx, int cy, AtlasSite& out) {
    cacheGuard();
    const SiteCell& c = cachedSite(AS_CAVERN, cx, cy);
    if (!c.exists) return false;
    out = c.site;
    return true;
}

// ================================================================ near-queries
namespace {

bool nearestOfLattice(int lattice, int period, int tx, int ty, int radius, AtlasSite& out) {
    int c0x = (int)floorf((float)(tx - radius) / (float)period);
    int c1x = (int)floorf((float)(tx + radius) / (float)period);
    int c0y = (int)floorf((float)(ty - radius) / (float)period);
    int c1y = (int)floorf((float)(ty + radius) / (float)period);
    bool found = false;
    int bestD = radius + 1;
    for (int cy = c0y; cy <= c1y; ++cy)
        for (int cx = c0x; cx <= c1x; ++cx) {
            AtlasSite s;
            bool ok = false;
            switch (lattice) {
                case AS_VILLAGE: ok = atlasVillageInCell(cx, cy, s); break;
                case AS_CAVERN:  ok = atlasCavernInCell(cx, cy, s); break;
                default:         ok = atlasLandmarkInCell(cx, cy, s); break;
            }
            if (!ok) continue;
            int d = std::max(abs(s.tx - tx), abs(s.ty - ty));
            if (d <= bestD) { bestD = d; out = s; found = true; }
        }
    return found;
}

} // namespace

bool atlasVillageNear(int tx, int ty, int radius, AtlasSite& out) {
    return nearestOfLattice(AS_VILLAGE, ATLAS_VILLAGE_CELL, tx, ty, radius, out);
}
bool atlasCavernNear(int tx, int ty, int radius, AtlasSite& out) {
    return nearestOfLattice(AS_CAVERN, ATLAS_CAVERN_CELL, tx, ty, radius, out);
}
bool atlasLandmarkNear(int tx, int ty, int radius, AtlasSite& out) {
    return nearestOfLattice(AS_LANDMARK, ATLAS_LANDMARK_CELL, tx, ty, radius, out);
}

void atlasSitesInRect(int x0, int y0, int x1, int y1, std::vector<AtlasSite>& out) {
    out.clear();
    auto collect = [&](int lattice, int period) {
        int c0x = (int)floorf((float)x0 / (float)period) - 1;
        int c1x = (int)floorf((float)x1 / (float)period) + 1;
        int c0y = (int)floorf((float)y0 / (float)period) - 1;
        int c1y = (int)floorf((float)y1 / (float)period) + 1;
        for (int cy = c0y; cy <= c1y; ++cy)
            for (int cx = c0x; cx <= c1x; ++cx) {
                AtlasSite s;
                bool ok = false;
                switch (lattice) {
                    case AS_VILLAGE: ok = atlasVillageInCell(cx, cy, s); break;
                    case AS_CAVERN:  ok = atlasCavernInCell(cx, cy, s); break;
                    default:         ok = atlasLandmarkInCell(cx, cy, s); break;
                }
                if (!ok) continue;
                if (s.tx < x0 - 4 || s.tx > x1 + 4 || s.ty < y0 - 4 || s.ty > y1 + 4) continue;
                bool dup = false;
                for (const AtlasSite& e : out)
                    if (e.tx == s.tx && e.ty == s.ty) { dup = true; break; }
                if (!dup) out.push_back(s);
            }
    };
    collect(AS_VILLAGE, ATLAS_VILLAGE_CELL);
    collect(AS_CAVERN, ATLAS_CAVERN_CELL);
    collect(AS_LANDMARK, ATLAS_LANDMARK_CELL);
}

bool atlasNearestSite(int tx, int ty, int maxRadius, AtlasSite& out) {
    bool found = false;
    AtlasSite s;
    int bestD = maxRadius + 1;
    auto consider = [&](int period, int lattice) {
        // widen the search cell by cell until something is found
        for (int r = period; r <= maxRadius + period; r += period) {
            AtlasSite cand;
            bool ok = (lattice == AS_VILLAGE) ? atlasVillageNear(tx, ty, r, cand)
                    : (lattice == AS_CAVERN)  ? atlasCavernNear(tx, ty, r, cand)
                                              : atlasLandmarkNear(tx, ty, r, cand);
            if (!ok) continue;
            int d = std::max(abs(cand.tx - tx), abs(cand.ty - ty));
            if (d < bestD) { bestD = d; s = cand; found = true; }
            break;
        }
    };
    consider(ATLAS_VILLAGE_CELL, AS_VILLAGE);
    consider(ATLAS_CAVERN_CELL, AS_CAVERN);
    consider(ATLAS_LANDMARK_CELL, AS_LANDMARK);
    if (found) out = s;
    return found;
}

// ================================================================ roads
namespace {

// Roads read the terrain far more often than anything else in the Survey, and
// neighbouring roads walk over the same hills. One direct-mapped cache of the
// routing cost keeps a chunk build in the low milliseconds.
struct RoadCostEntry { int x = 1 << 30, y = 1 << 30; uint32_t seed = 0; float cost = 0.0f; };
RoadCostEntry gRoadCostCache[1 << 14];

float roadCostRaw(int tx, int ty) {
    SurveyTerrain st = worldSurveyTerrain(tx, ty);
    float c = 1.0f;
    if (st.deep) c = 120.0f;
    else if (st.water) c = 34.0f;
    if (st.crag) c = 20.0f;
    if (st.ice) c = 12.0f;
    if (st.swamp) c = 6.0f;
    return c;
}

float roadCostAt(int tx, int ty) {
    RoadCostEntry& e = gRoadCostCache[((uint32_t)tx * 73856093u ^ (uint32_t)ty * 19349663u) & ((1u << 14) - 1)];
    if (e.x == tx && e.y == ty && e.seed == g.worldSeed) return e.cost;
    float c = roadCostRaw(tx, ty);
    e = { tx, ty, g.worldSeed, c };
    return c;
}

// Dijkstra over a 3-tile grid. Water is expensive but crossable (the ground
// generator draws a plank bridge over it), crag and ice are simply slow, and
// both towns are skirted so a road arrives at a gate instead of a roof.
bool routeRoad(int ax, int ay, int bx, int by, RoadPath& out, float portal = 19.0f) {
    out.pts.clear();
    float dx = (float)(bx - ax), dy = (float)(by - ay);
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 24.0f) return false;
    float ux = dx / len, uy = dy / len;
    int sx = ax + (int)roundf(ux * portal);
    int sy = ay + (int)roundf(uy * portal);
    int gx = bx - (int)roundf(ux * portal);
    int gy = by - (int)roundf(uy * portal);

    const int pad = 24;
    int minX = std::min(sx, gx) - pad, maxX = std::max(sx, gx) + pad;
    int minY = std::min(sy, gy) - pad, maxY = std::max(sy, gy) + pad;
    int W = (maxX - minX) / ROAD_STEP + 1;
    int H = (maxY - minY) / ROAD_STEP + 1;
    if (W <= 0 || H <= 0 || W * H > 40000) return false;

    // snap endpoints onto the node grid
    auto nodeIdx = [&](int x, int y) {
        int nx = (int)floorf((float)(x - minX) / ROAD_STEP);
        int ny = (int)floorf((float)(y - minY) / ROAD_STEP);
        nx = std::max(0, std::min(W - 1, nx));
        ny = std::max(0, std::min(H - 1, ny));
        return ny * W + nx;
    };

    // The grid is filled lazily: a road only pays for the nodes it explores.
    std::vector<float> cost((size_t)W * H, -1.0f);
    auto nodeCost = [&](int node) {
        float& c = cost[(size_t)node];
        if (c >= 0.0f) return c;
        int tx = minX + (node % W) * ROAD_STEP, ty = minY + (node / W) * ROAD_STEP;
        c = roadCostAt(tx, ty);
        float o = sqrtf((float)(tx * tx + ty * ty));
        if (o < 28.0f) c += 90.0f;                           // leave Elmswick's square alone
        float da = sqrtf((float)((tx - ax) * (tx - ax) + (ty - ay) * (ty - ay)));
        float db = sqrtf((float)((tx - bx) * (tx - bx) + (ty - by) * (ty - by)));
        if (da < 20.0f || db < 20.0f) c += 70.0f;            // and both town plots
        // The Agents' Commons is another agent's work: roads go around it.
        if (tx >= COMMONS_X0 - 3 && tx <= COMMONS_X1 + 3 &&
            ty >= COMMONS_Y0 - 3 && ty <= COMMONS_Y1 + 3) c += 240.0f;
        // ...and the three story set-pieces keep their own approaches.
        if (abs(tx - BOSS_CASTLE_TX) < 22 && abs(ty - BOSS_CASTLE_TY) < 22) c += 120.0f;
        if (abs(tx - LOCKET_SITE_TX) < 16 && abs(ty - LOCKET_SITE_TY) < 16) c += 120.0f;
        if (abs(tx - TOWER_SITE_TX) < 16 && abs(ty - TOWER_SITE_TY) < 16) c += 120.0f;
        return c;
    };

    int start = nodeIdx(sx, sy), goal = nodeIdx(gx, gy);
    if (start == goal) return false;
    // A* with an octile heuristic: every step costs at least ROAD_STEP, so the
    // straight-line estimate is admissible and the search stays narrow.
    auto heuristic = [&](int node) {
        int nx = node % W, ny = node / W;
        float ddx = fabsf((float)(nx - goal % W));
        float ddy = fabsf((float)(ny - goal / W));
        float dmax = std::max(ddx, ddy), dmin = std::min(ddx, ddy);
        return (dmax + 0.41421356f * dmin) * (float)ROAD_STEP;
    };
    const float INF = 1e18f;
    std::vector<float> dist((size_t)W * H, INF);
    std::vector<int> prev((size_t)W * H, -1);
    using Node = std::pair<float, int>;
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> open;
    dist[start] = 0.0f;
    open.push({ heuristic(start), start });
    static const int DX[8] = { 1, -1, 0, 0, 1, 1, -1, -1 };
    static const int DY[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };
    while (!open.empty()) {
        auto [f, at] = open.top(); open.pop();
        float g = dist[at];
        if (f > g + heuristic(at) + 0.001f) continue;         // stale entry
        if (at == goal) break;
        int nx = at % W, ny = at / W;
        for (int k = 0; k < 8; ++k) {
            int px = nx + DX[k], py = ny + DY[k];
            if (px < 0 || py < 0 || px >= W || py >= H) continue;
            int ni = py * W + px;
            float step = (k < 4) ? (float)ROAD_STEP : (float)ROAD_STEP * 1.41421356f;
            float nd = g + step * (0.5f * nodeCost(at) + 0.5f * nodeCost(ni));
            if (nd >= dist[ni]) continue;
            dist[ni] = nd; prev[ni] = at;
            open.push({ nd + heuristic(ni), ni });
        }
    }
    if (prev[goal] < 0 && goal != start) return false;

    std::vector<int> chain;
    for (int at = goal; at >= 0; at = prev[at]) {
        chain.push_back(at);
        if (at == start) break;
    }
    std::reverse(chain.begin(), chain.end());
    out.pts.reserve(chain.size() + 2);
    out.pts.push_back({ sx, sy });
    for (int idx : chain)
        out.pts.push_back({ minX + (idx % W) * ROAD_STEP, minY + (idx / W) * ROAD_STEP });
    out.pts.push_back({ gx, gy });
    // The routing grid is coarse on purpose; two rounds of midpoint averaging
    // turn its right angles into the long bends a road actually takes.
    for (int pass = 0; pass < 2 && out.pts.size() > 3; ++pass) {
        std::vector<std::pair<int, int>> smooth;
        smooth.reserve(out.pts.size());
        smooth.push_back(out.pts.front());
        for (size_t i = 1; i + 1 < out.pts.size(); ++i)
            smooth.push_back({ (out.pts[i - 1].first + 2 * out.pts[i].first + out.pts[i + 1].first) / 4,
                               (out.pts[i - 1].second + 2 * out.pts[i].second + out.pts[i + 1].second) / 4 });
        smooth.push_back(out.pts.back());
        out.pts.swap(smooth);
    }
    out.x0 = minX; out.y0 = minY; out.x1 = maxX; out.y1 = maxY;
    return true;
}

const RoadPath& roadFor(int cx, int cy, int dir) {
    static RoadPath empty;
    auto key = std::make_tuple(g.worldSeed, cx, cy, dir);
    auto it = gRoadCache.find(key);
    if (it != gRoadCache.end()) return it->second;
    if ((int)gRoadCache.size() > ROAD_CACHE_MAX) gRoadCache.clear();
    RoadPath path;
    AtlasSite a, b;
    bool okA = atlasVillageInCell(cx, cy, a);
    bool okB = (dir == 0) ? atlasVillageInCell(cx + 1, cy, b) : atlasVillageInCell(cx, cy + 1, b);
    if (okA && okB) {
        if (!routeRoad(a.tx, a.ty, b.tx, b.ty, path)) path = RoadPath{};
    }
    auto res = gRoadCache.emplace(key, path);
    return res.first->second;
}

void rasterize(const RoadPath& p, int chx, int chy, std::vector<uint8_t>& axis) {
    if (p.pts.size() < 2) return;
    int cx0 = chx * ROAD_CHUNK, cy0 = chy * ROAD_CHUNK;
    for (size_t i = 0; i + 1 < p.pts.size(); ++i) {
        float fx = (float)p.pts[i].first, fy = (float)p.pts[i].second;
        float tx = (float)p.pts[i + 1].first, ty = (float)p.pts[i + 1].second;
        float seg = std::max(fabsf(tx - fx), fabsf(ty - fy));
        int steps = std::max(1, (int)(seg * 1.5f));
        for (int s = 0; s <= steps; ++s) {
            float t = (float)s / (float)steps;
            int x = (int)roundf(fx + (tx - fx) * t);
            int y = (int)roundf(fy + (ty - fy) * t);
            bool ns = fabsf(ty - fy) > fabsf(tx - fx);
            for (int k = 0; k < 2; ++k) {
                int px = ns ? x + k : x;
                int py = ns ? y : y + k;
                int lx = px - cx0, ly = py - cy0;
                if (lx < 0 || ly < 0 || lx >= ROAD_CHUNK || ly >= ROAD_CHUNK) continue;
                uint8_t& a = axis[(size_t)ly * ROAD_CHUNK + lx];
                a |= ns ? 1 : 2;
            }
        }
    }
}

const std::vector<uint8_t>& roadChunk(int chx, int chy) {
    static std::vector<uint8_t> empty;
    auto key = std::make_tuple(g.worldSeed, chx, chy);
    auto it = gChunkCache.find(key);
    if (it != gChunkCache.end()) return it->second;
    if ((int)gChunkCache.size() > CHUNK_CACHE_MAX) gChunkCache.clear();
    std::vector<uint8_t> axis((size_t)ROAD_CHUNK * ROAD_CHUNK, 0);

    int bx0 = chx * ROAD_CHUNK, by0 = chy * ROAD_CHUNK;
    int bx1 = bx0 + ROAD_CHUNK - 1, by1 = by0 + ROAD_CHUNK - 1;

    int c0x = (int)floorf((float)bx0 / ATLAS_VILLAGE_CELL) - 2;
    int c1x = (int)floorf((float)bx1 / ATLAS_VILLAGE_CELL) + 1;
    int c0y = (int)floorf((float)by0 / ATLAS_VILLAGE_CELL) - 2;
    int c1y = (int)floorf((float)by1 / ATLAS_VILLAGE_CELL) + 1;
    for (int cy = c0y; cy <= c1y; ++cy)
        for (int cx = c0x; cx <= c1x; ++cx) {
            for (int dir = 0; dir < 2; ++dir) {
                AtlasSite a, b;
                if (!atlasVillageInCell(cx, cy, a)) continue;
                bool okB = (dir == 0) ? atlasVillageInCell(cx + 1, cy, b)
                                      : atlasVillageInCell(cx, cy + 1, b);
                if (!okB) continue;
                // Cheap reject before paying for a route: if the straight line
                // between the two towns cannot come near this chunk, skip it.
                int lo = std::min(dir == 0 ? a.tx : a.ty, dir == 0 ? b.tx : b.ty) - 70;
                int hi = std::max(dir == 0 ? a.tx : a.ty, dir == 0 ? b.tx : b.ty) + 70;
                if (dir == 0 && (hi < bx0 || lo > bx1)) continue;
                if (dir == 1 && (hi < by0 || lo > by1)) continue;
                const RoadPath& p = roadFor(cx, cy, dir);
                if (p.empty()) continue;
                if (p.x1 < bx0 || p.x0 > bx1 || p.y1 < by0 || p.y0 > by1) continue;
                rasterize(p, chx, chy, axis);
            }
        }
    // Elmswick's own four gate roads are part of the network too.
    std::vector<RoadPath> spurs;
    originRoadPaths(spurs);
    for (const RoadPath& p : spurs) {
        if (p.empty()) continue;
        if (p.x1 < bx0 || p.x0 > bx1 || p.y1 < by0 || p.y0 > by1) continue;
        rasterize(p, chx, chy, axis);
    }

    auto res = gChunkCache.emplace(key, std::move(axis));
    return res.first->second;
}

uint8_t roadAxisAt(int tx, int ty) {
    int chx = (int)floorf((float)tx / ROAD_CHUNK);
    int chy = (int)floorf((float)ty / ROAD_CHUNK);
    int lx = tx - chx * ROAD_CHUNK, ly = ty - chy * ROAD_CHUNK;
    const std::vector<uint8_t>& axis = roadChunk(chx, chy);
    if (axis.empty()) return 0;
    return axis[(size_t)ly * ROAD_CHUNK + lx];
}

bool originRoadPaths(std::vector<RoadPath>& out) {
    out.clear();
    cacheGuard();
    auto key = std::make_tuple(g.worldSeed, 0, 0);
    auto it = gSpurCache.find(key);
    if (it != gSpurCache.end()) { out = it->second; return !out.empty(); }

    // Elmswick sends one road out of each gate, toward the nearest village in
    // that quarter. The town is the hub of the survey, not a hole in it.
    static const int DX[4] = { 1, -1, 0, 0 };
    static const int DY[4] = { 0, 0, -1, 1 };
    std::vector<AtlasSite> nearby;
    int span = ATLAS_VILLAGE_CELL * 3;
    for (int cy = -3; cy <= 2; ++cy)
        for (int cx = -3; cx <= 2; ++cx) {
            AtlasSite v;
            if (!atlasVillageInCell(cx, cy, v)) continue;
            if (abs(v.tx) > span || abs(v.ty) > span) continue;
            nearby.push_back(v);
        }
    std::vector<std::pair<int, int>> claimed;
    for (int d = 0; d < 4; ++d) {
        const AtlasSite* best = nullptr;
        float bestScore = 0.0f;
        for (const AtlasSite& v : nearby) {
            float dot = (float)v.tx * (float)DX[d] + (float)v.ty * (float)DY[d];
            float len = sqrtf((float)(v.tx * v.tx + v.ty * v.ty));
            if (len < 40.0f) continue;
            float align = dot / len;
            if (align < 0.55f) continue;
            bool taken = false;
            for (auto& c : claimed) if (c.first == v.tx && c.second == v.ty) taken = true;
            if (taken) continue;
            float score = align * 100.0f - len * 0.35f;
            if (score > bestScore) { bestScore = score; best = &v; }
        }
        if (best) claimed.push_back({ best->tx, best->ty });
        if (!best) continue;
        RoadPath p;
        // East leaves from beyond the Commons' own causeway rather than through
        // the ring of stones another agent raised there.
        if (d == 0) {
            if (routeRoad(COMMONS_X1 + 4, COMMONS_CY, best->tx, best->ty, p, 0.0f)) out.push_back(p);
        } else if (routeRoad(0, 0, best->tx, best->ty, p)) {
            out.push_back(p);
        }
    }
    gSpurCache.emplace(key, out);
    return !out.empty();
}

} // namespace

bool atlasRoadAt(int tx, int ty) { return roadAxisAt(tx, ty) != 0; }
int atlasRoadAxis(int tx, int ty) { return (int)roadAxisAt(tx, ty); }

void atlasOriginRoads(std::vector<AtlasRoad>& out) {
    out.clear();
    std::vector<RoadPath> paths;
    originRoadPaths(paths);
    for (const RoadPath& p : paths) out.push_back({ p.pts });
}

void atlasRoadsInRect(int x0, int y0, int x1, int y1, std::vector<AtlasRoad>& out) {
    out.clear();
    cacheGuard();
    int c0x = (int)floorf((float)x0 / ATLAS_VILLAGE_CELL) - 2;
    int c1x = (int)floorf((float)x1 / ATLAS_VILLAGE_CELL) + 1;
    int c0y = (int)floorf((float)y0 / ATLAS_VILLAGE_CELL) - 2;
    int c1y = (int)floorf((float)y1 / ATLAS_VILLAGE_CELL) + 1;
    for (int cy = c0y; cy <= c1y; ++cy)
        for (int cx = c0x; cx <= c1x; ++cx)
            for (int dir = 0; dir < 2; ++dir) {
                AtlasSite a, b;
                if (!atlasVillageInCell(cx, cy, a)) continue;
                bool okB = (dir == 0) ? atlasVillageInCell(cx + 1, cy, b)
                                      : atlasVillageInCell(cx, cy + 1, b);
                if (!okB) continue;
                int lo = std::min(dir == 0 ? a.tx : a.ty, dir == 0 ? b.tx : b.ty) - 70;
                int hi = std::max(dir == 0 ? a.tx : a.ty, dir == 0 ? b.tx : b.ty) + 70;
                if (dir == 0 && (hi < x0 || lo > x1)) continue;
                if (dir == 1 && (hi < y0 || lo > y1)) continue;
                const RoadPath& p = roadFor(cx, cy, dir);
                if (p.empty()) continue;
                if (p.x1 < x0 || p.x0 > x1 || p.y1 < y0 || p.y0 > y1) continue;
                out.push_back({ p.pts });
            }
    std::vector<RoadPath> spurs;
    originRoadPaths(spurs);
    for (const RoadPath& p : spurs) {
        if (p.empty()) continue;
        if (p.x1 < x0 || p.x0 > x1 || p.y1 < y0 || p.y0 > y1) continue;
        out.push_back({ p.pts });
    }
}

// ================================================================ names
const char* atlasSiteKindName(int kind) {
    switch (kind) {
        case AS_VILLAGE: return "Settlement";
        case AS_CAVERN:  return "Cavern";
        case AS_RUINS:   return "Ruins";
        case AS_TOWER:   return "Tower";
        case AS_CASTLE:  return "Castle";
        case AS_STONES:  return "Stone Circle";   // the story ring has its own name
        case AS_CAMP:    return "Camp";
        default:         return "Uncharted";
    }
}

std::string atlasSiteName(const AtlasSite& s) {
    if (s.kind == AS_VILLAGE) return villageNameFor(s.cellX, s.cellY, g.worldSeed);
    return std::string(atlasSiteKindName(s.kind));
}

// Region names are cut from a lead and a suffix chosen by the block's own hash,
// so the same block keeps its name for the life of the seed. The suffix follows
// the biome the block's heart sits in, which is why the fen is all -mire and the
// frost all -fell.
const char* atlasRegionName(int tx, int ty) {
    static std::map<std::tuple<uint32_t, int, int>, std::string> names;
    const int REG = 128;
    int rx = (int)floorf((float)tx / (float)REG);
    int ry = (int)floorf((float)ty / (float)REG);
    auto key = std::make_tuple(g.worldSeed, rx, ry);
    auto it = names.find(key);
    if (it != names.end()) return it->second.c_str();
    if (names.size() > 400) names.clear();

    static const char* lead[] = {
        "Mill", "Amber", "Fen", "Brack", "Glim", "Thorn", "Raven", "Hollow",
        "Ashen", "Wynd", "Stone", "Elder", "Grey", "Moss", "Sunning", "Dun",
        "Kest", "Briar", "Fall", "Har", "Wold", "Cald", "Tarn", "Ivy",
        "Red", "Sil", "Mar", "Vell", "Orrin", "Cade", "Low", "High"
    };
    static const char* meadowSuf[] = { "brook", "vale", "mead", "downs", "field" };
    static const char* forestSuf[] = { "wood", "thicket", "grove", "holt" };
    static const char* amberSuf[]  = { "wood", "reach", "weald" };
    static const char* fenSuf[]    = { "mire", "fen", "marsh", "sough" };
    static const char* heathSuf[]  = { "heath", "crags", "fell", "scar" };
    static const char* frostSuf[]  = { "fell", "rime", "frost", "snows" };
    static const char* dunesSuf[]  = { "dunes", "waste", "reach", "sands" };

    SurveyTerrain st = worldSurveyTerrain(rx * REG + REG / 2, ry * REG + REG / 2);
    const char** suf = meadowSuf;
    int sufCount = 5;
    switch (st.biome) {
        case BI_FOREST: suf = forestSuf; sufCount = 4; break;
        case BI_AMBER:  suf = amberSuf;  sufCount = 3; break;
        case BI_FEN:    suf = fenSuf;    sufCount = 4; break;
        case BI_HEATH:  suf = heathSuf;  sufCount = 4; break;
        case BI_FROST:  suf = frostSuf;  sufCount = 4; break;
        case BI_DUNES:  suf = dunesSuf;  sufCount = 4; break;
        default: break;
    }
    uint32_t h = atlasHash(rx, ry, g.worldSeed ^ 0x4e6a3u);
    const char* a = lead[(h >> 3) % (sizeof(lead) / sizeof(lead[0]))];
    const char* b = suf[(h >> 13) % (uint32_t)sufCount];
    std::string name = std::string(a) + b;
    auto res = names.emplace(key, name);
    return res.first->second.c_str();
}

// ================================================================ field hook
void atlasResetRegion() {
    g.regionName.clear();
    g.regionBannerT = 0.0f;
}

void atlasUpdate(float px, float py, float dt) {
    if (g.regionBannerT > 0.0f) g.regionBannerT = std::max(0.0f, g.regionBannerT - dt);
    const char* here = atlasRegionName((int)floorf(px), (int)floorf(py));
    if (g.regionName != here) {
        g.regionName = here;
        g.regionBannerT = 4.0f;
    }
}

// ================================================================ survey page
namespace {

struct SurveyTile { Color c; };
int gMapPx = 0, gMapPy = 0, gMapSize = 0;
uint32_t gMapSeed = 0;

// A sketch has to stay inside its panel: clip every road segment to it.
bool clipSegment(float& x0, float& y0, float& x1, float& y1,
                 float minX, float minY, float maxX, float maxY) {
    float dx = x1 - x0, dy = y1 - y0;
    float t0 = 0.0f, t1 = 1.0f;
    auto edge = [&](float p, float q) {
        if (p == 0.0f) return q >= 0.0f;
        float r = q / p;
        if (p < 0.0f) { if (r > t1) return false; if (r > t0) t0 = r; }
        else { if (r < t0) return false; if (r < t1) t1 = r; }
        return true;
    };
    if (!edge(-dx, x0 - minX) || !edge(dx, maxX - x0) ||
        !edge(-dy, y0 - minY) || !edge(dy, maxY - y0)) return false;
    float nx0 = x0 + t0 * dx, ny0 = y0 + t0 * dy;
    float nx1 = x0 + t1 * dx, ny1 = y0 + t1 * dy;
    x0 = nx0; y0 = ny0; x1 = nx1; y1 = ny1;
    return true;
}

// Labels are placed nearest-first and never on top of one another. A survey
// page that smears six region names into one line is worse than a blank one.
struct LabelBoxes {
    std::vector<Rectangle> boxes;
    bool tryPlace(const char* text, float x, float y, float panelX, float panelY,
                  float panelW, float panelH, Color c) {
        if (!text || !text[0]) return false;
        Vector2 size = measureText(text, 8);
        Rectangle r{ x, y - 1.0f, size.x + 2.0f, size.y + 2.0f };
        if (r.x < panelX + 1 || r.y < panelY + 1 ||
            r.x + r.width > panelX + panelW - 1 || r.y + r.height > panelY + panelH - 1) return false;
        for (const Rectangle& b : boxes) {
            if (r.x < b.x + b.width + 1 && r.x + r.width + 1 > b.x &&
                r.y < b.y + b.height + 1 && r.y + r.height + 1 > b.y) return false;
        }
        boxes.push_back(r);
        drawText(text, x, y, c, 8);
        return true;
    }
};

Color biomeColor(const SurveyTerrain& st) {
    if (st.deep) return Color{ 38, 62, 120, 255 };
    if (st.water) return Color{ 62, 104, 168, 255 };
    if (st.ice) return Color{ 196, 224, 236, 255 };
    if (st.crag) return Color{ 118, 112, 106, 255 };
    switch (st.biome) {
        case BI_FOREST: return Color{ 48, 96, 62, 255 };
        case BI_AMBER:  return Color{ 150, 118, 52, 255 };
        case BI_FEN:    return Color{ 78, 96, 74, 255 };
        case BI_HEATH:  return Color{ 132, 126, 94, 255 };
        case BI_FROST:  return Color{ 208, 216, 226, 255 };
        case BI_DUNES:  return Color{ 196, 168, 116, 255 };
        default:        return Color{ 88, 134, 84, 255 };
    }
}

} // namespace

// The page is a field sketch, not a save-tracked atlas: it draws the country
// the party is standing in — terrain, roads, named places — and it is rebuilt
// whenever the party has moved far enough that the sketch would be stale.
void atlasJournalDraw(float ox, float oy) {
    const float px = ox + 122.0f, py = oy + 36.0f;
    const float pw = 366.0f, ph = 206.0f;
    const int BLOCK = 4;                     // screen pixels per drawn cell
    const int TILES_PER_BLOCK = 8;           // world tiles per drawn cell
    const int BW = (int)(pw / BLOCK), BH = (int)(ph / BLOCK);
    const int originX = (int)floorf(g.px) - (BW / 2) * TILES_PER_BLOCK;
    const int originY = (int)floorf(g.py) - (BH / 2) * TILES_PER_BLOCK;
    const int spanX = BW * TILES_PER_BLOCK, spanY = BH * TILES_PER_BLOCK;
    const int size = BW * BH;

    static std::vector<Color> sketch;
    if (gMapSeed != g.worldSeed || gMapSize != size) {
        sketch.assign((size_t)size, Color{ 0, 0, 0, 0 });
        gMapSeed = g.worldSeed; gMapSize = size; gMapPx = 1 << 30; gMapPy = 1 << 30;
    }
    if (abs(gMapPx - originX) >= TILES_PER_BLOCK || abs(gMapPy - originY) >= TILES_PER_BLOCK) {
        gMapPx = originX; gMapPy = originY;
        for (int by = 0; by < BH; ++by)
            for (int bx = 0; bx < BW; ++bx) {
                int tx = originX + bx * TILES_PER_BLOCK + TILES_PER_BLOCK / 2;
                int ty = originY + by * TILES_PER_BLOCK + TILES_PER_BLOCK / 2;
                sketch[(size_t)by * BW + bx] = biomeColor(worldSurveyTerrain(tx, ty));
            }
    }

    drawPixelPanel({ px - 2, py - 2, pw + 4, ph + 4 }, Color{ 20, 26, 46, 255 },
                   Color{ 70, 88, 140, 220 });
    for (int by = 0; by < BH; ++by)
        for (int bx = 0; bx < BW; ++bx) {
            Color c = sketch[(size_t)by * BW + bx];
            DrawRectangle((int)(px + bx * BLOCK), (int)(py + by * BLOCK), BLOCK, BLOCK, c);
        }
    // parchment wash so the sketch reads as a drawing of the land, not the land
    DrawRectangle((int)px, (int)py, (int)pw, (int)ph, Fade(Color{ 226, 206, 160, 255 }, 0.14f));

    auto toScreen = [&](float tx, float ty, float& sx, float& sy) {
        sx = px + (tx - (float)originX) / (float)spanX * pw;
        sy = py + (ty - (float)originY) / (float)spanY * ph;
    };

    // roads
    std::vector<AtlasRoad> roads;
    atlasRoadsInRect(originX - 40, originY - 40, originX + spanX + 40, originY + spanY + 40, roads);
    for (const AtlasRoad& r : roads) {
        for (size_t i = 0; i + 1 < r.pts.size(); ++i) {
            float ax, ay, bx, by;
            toScreen((float)r.pts[i].first, (float)r.pts[i].second, ax, ay);
            toScreen((float)r.pts[i + 1].first, (float)r.pts[i + 1].second, bx, by);
            if (!clipSegment(ax, ay, bx, by, px + 1, py + 1, px + pw - 1, py + ph - 1)) continue;
            DrawLineEx({ ax, ay }, { bx, by }, 1.6f, Color{ 214, 186, 138, 230 });
        }
    }

    // places: every marker is drawn, but only the nearest few get a name, and
    // names are placed nearest-first so the closest place always wins.
    std::vector<std::pair<AtlasSite, std::string>> places;
    {
        std::vector<AtlasSite> sites;
        atlasSitesInRect(originX - 8, originY - 8, originX + spanX + 8, originY + spanY + 8, sites);
        for (const AtlasSite& s : sites) places.push_back({ s, std::string() });
    }
    // Hand-authored places belong on the sketch too: the town the party starts
    // in, and the three story destinations the game is actually about.
    auto addSpecial = [&](int kind, int tx, int ty, const char* name) {
        AtlasSite a;
        a.kind = kind; a.tx = tx; a.ty = ty;
        places.push_back({ a, std::string(name) });
    };
    addSpecial(AS_VILLAGE, 0, 0, "Elmswick");
    addSpecial(AS_CASTLE, BOSS_CASTLE_TX, BOSS_CASTLE_TY, "Castle of Elm");
    addSpecial(AS_STONES, LOCKET_SITE_TX, LOCKET_SITE_TY, "The Standing Stones");
    addSpecial(AS_TOWER, TOWER_SITE_TX, TOWER_SITE_TY, "The Old Watchtower");
    {
        int cx, cy;
        if (startCavernSite(cx, cy)) addSpecial(AS_CAVERN, cx, cy, "Hollow Hill");
    }
    std::sort(places.begin(), places.end(),
              [&](const std::pair<AtlasSite, std::string>& a,
                  const std::pair<AtlasSite, std::string>& b) {
        // The story places are named first — a sketch that labels six hamlets
        // and forgets the Castle of Elm has failed at its one job.
        bool namedA = !a.second.empty(), namedB = !b.second.empty();
        if (namedA != namedB) return namedA;
        int da = std::max(abs(a.first.tx - (int)g.px), abs(a.first.ty - (int)g.py));
        int db = std::max(abs(b.first.tx - (int)g.px), abs(b.first.ty - (int)g.py));
        return da < db;
    });
    // Around the named story places the map is crowded enough already: their
    // markers stay, but generic marks nearby do not get to shout over them.
    std::vector<std::pair<int, int>> story;
    for (const auto& entry : places)
        if (!entry.second.empty()) story.push_back({ entry.first.tx, entry.first.ty });

    LabelBoxes labels;
    int named = 0;
    for (const auto& entry : places) {
        const AtlasSite& s = entry.first;
        float sx, sy;
        toScreen((float)s.tx, (float)s.ty, sx, sy);
        if (sx < px + 2 || sx > px + pw - 2 || sy < py + 2 || sy > py + ph - 2) continue;
        int d = std::max(abs(s.tx - (int)g.px), abs(s.ty - (int)g.py));
        std::string name = entry.second;
        Color nameCol;
        if (s.kind == AS_VILLAGE) {
            DrawRectangle((int)sx - 2, (int)sy - 2, 5, 5, Color{ 240, 228, 186, 255 });
            DrawRectangleLines((int)sx - 2, (int)sy - 2, 5, 5, Color{ 92, 62, 40, 255 });
            if (name.empty()) name = atlasSiteName(s);
            nameCol = Color{ 255, 244, 210, 255 };
        } else if (s.kind == AS_CAVERN) {
            DrawTriangle({ sx, sy - 3.0f }, { sx - 3.0f, sy + 2.0f }, { sx + 3.0f, sy + 2.0f },
                         Color{ 44, 40, 54, 255 });
            if (name.empty()) {
                if (d > 90) continue;
                name = "cavern";
            }
            nameCol = Color{ 226, 220, 240, 255 };
        } else {
            DrawRectangle((int)sx - 1, (int)sy - 1, 3, 3, Color{ 176, 136, 78, 255 });
            if (name.empty()) {
                if (d > 110) continue;
                name = atlasSiteKindName(s.kind);
            }
            nameCol = Color{ 242, 226, 190, 255 };
        }
        bool shadowed = false;
        if (entry.second.empty()) {                       // unnamed, generic mark
            for (const auto& sp : story)
                if (std::max(abs(sp.first - s.tx), abs(sp.second - s.ty)) < 70) { shadowed = true; break; }
        }
        if (named < 9 && !shadowed) {
            // four tries per mark: right, below, left, above — so a name loses
            // its slot only when the neighbourhood really is crowded
            Vector2 size = measureText(name.c_str(), 8);
            float rx = sx + 4.0f;
            if (rx + size.x > px + pw - 2) rx = px + pw - 2 - size.x;   // slide, don't jump
            float bx = sx - size.x * 0.5f;                 // centred on the mark
            if (bx < px + 2) bx = px + 2;
            if (bx + size.x > px + pw - 2) bx = px + pw - 2 - size.x;
            const float tries[4][2] = {
                { rx, sy - 4.0f },                          // beside, above
                { bx, sy + 5.0f },                          // under the mark
                { bx, sy - 13.0f },                         // over the mark
                { sx - 4.0f - size.x, sy - 4.0f },          // finally, to the left
            };
            for (int t = 0; t < 4; ++t) {
                if (labels.tryPlace(name.c_str(), tries[t][0], tries[t][1],
                                    px + 2, py + 2, pw - 4, ph - 4, nameCol)) { named++; break; }
            }
        }
    }

    // Region titles: one per named block in view, but only where a name fits
    // without landing on a place name or another region's.
    int rx0 = (int)floorf((float)originX / 128.0f);
    int rx1 = (int)floorf((float)(originX + spanX) / 128.0f);
    int ry0 = (int)floorf((float)originY / 128.0f);
    int ry1 = (int)floorf((float)(originY + spanY) / 128.0f);
    // the party's own region gets first claim on its name
    int homeRx = (int)floorf(g.px / 128.0f), homeRy = (int)floorf(g.py / 128.0f);
    for (int pass = 0; pass < 2; ++pass)
    for (int ry = ry0; ry <= ry1; ++ry)
        for (int rx = rx0; rx <= rx1; ++rx) {
            bool home = (rx == homeRx && ry == homeRy);
            if (pass == 0 && !home) continue;
            if (pass == 1 && home) continue;
            float sx, sy;
            toScreen((float)(rx * 128 + 64), (float)(ry * 128 + 64), sx, sy);
            const char* rname = atlasRegionName(rx * 128 + 64, ry * 128 + 64);
            Vector2 size = measureText(rname, 8);
            if (sx - size.x * 0.5f < px + 3 || sx + size.x * 0.5f > px + pw - 3) continue;
            labels.tryPlace(rname, sx - size.x * 0.5f, sy - size.y * 0.5f,
                            px + 2, py + 2, pw - 4, ph - 4, Color{ 74, 56, 34, 235 });
        }

    // the party
    {
        float cx = px + pw * 0.5f, cy = py + ph * 0.5f;
        DrawTriangle({ cx, cy - 5.0f }, { cx - 4.0f, cy + 4.0f }, { cx + 4.0f, cy + 4.0f },
                     Color{ 232, 72, 64, 255 });
        DrawRectangle((int)cx - 1, (int)cy + 2, 2, 3, Color{ 120, 26, 22, 255 });
    }

    // footer: where we are, and what is worth a walk
    char line[192];
    snprintf(line, sizeof(line), "You are in %s", atlasRegionName((int)g.px, (int)g.py));
    drawText(line, px, py + ph + 4, Color{ 255, 236, 176, 255 }, 8);

    AtlasSite nearestV, nearestC;
    bool inStart = (g.px * g.px + g.py * g.py) < 30.0f * 30.0f;
    bool hasV = atlasVillageNear((int)g.px, (int)g.py, 400, nearestV);
    bool hasC = atlasCavernNear((int)g.px, (int)g.py, 400, nearestC);
    std::string hint;
    if (inStart) {
        hint = "Elmswick: where the roads begin";
    } else if (hasV) {
        int d = std::max(abs(nearestV.tx - (int)g.px), abs(nearestV.ty - (int)g.py));
        char buf[128];
        snprintf(buf, sizeof(buf), "%s %d tiles", atlasSiteName(nearestV).c_str(), d);
        hint = buf;
        if (hasC) {
            int dc = std::max(abs(nearestC.tx - (int)g.px), abs(nearestC.ty - (int)g.py));
            snprintf(buf, sizeof(buf), "   |   cavern %d tiles", dc);
            hint += buf;
        }
    }
    drawText(hint.c_str(), px, py + ph + 14, Color{ 206, 216, 240, 255 }, 8);
}
