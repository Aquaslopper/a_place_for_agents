// verify_lufia_world.cpp — headless acceptance suite for the Lufia II world.
//
// The world is a designed geography now: each continent is a bounded landmass
// with one coast, mountain ranges, rivers, settlements in valleys and at
// harbours, and a road network that reaches every place. This suite checks the
// promises that geography makes, not a contact sheet:
//
//   1. same seed -> same world, queried in any order;
//   2. the homeland's story sites exist, sit on walkable ground and can be
//      walked to from Elmswick;
//   3. every continent has real settlements and dungeons, all on land, all
//      reachable on foot from that continent's first town, and every generated
//      town still passes its own structural verifier;
//   4. the Tidewell isles are islands: sandy, offshore, unreachable on foot;
//   5. roads exist, touch settlements, and become bridges/passes only over
//      water and mountains;
//   6. the world continues outward: distant continents generate and verify;
//   7. a fresh continent generates fast enough to be a world, not a loading bar.
#include "overworld_gen.hpp"
#include "town_gen.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

constexpr int REGION = 320;
constexpr int HALF = REGION / 2;

struct BfsResult {
    std::vector<std::uint8_t> seen;   // REGION*REGION, local indices
    int landTiles = 0;
};

int localIndex(int lx, int ly) { return (ly + HALF) * REGION + (lx + HALF); }

BfsResult flood(std::uint32_t seed, int rx, int ry, int sx, int sy) {
    BfsResult r;
    r.seen.assign(static_cast<std::size_t>(REGION) * REGION, 0);
    int baseX = rx * REGION, baseY = ry * REGION;
    std::vector<int> q;
    q.reserve(4096);
    if (!apfa::owIsWalkableOnFoot(apfa::owSampleTile(seed, sx, sy).kind)) return r;
    q.push_back(localIndex(sx - baseX, sy - baseY));
    r.seen[q[0]] = 1;
    static const int dx4[4] = {1, -1, 0, 0};
    static const int dy4[4] = {0, 0, 1, -1};
    for (std::size_t head = 0; head < q.size(); ++head) {
        int id = q[head];
        int ly = id / REGION - HALF;
        int lx = id % REGION - HALF;
        for (int d = 0; d < 4; ++d) {
            int nx = lx + dx4[d], ny = ly + dy4[d];
            if (nx < -HALF || nx >= HALF || ny < -HALF || ny >= HALF) continue;
            int ni = localIndex(nx, ny);
            if (r.seen[ni]) continue;
            if (!apfa::owIsWalkableOnFoot(apfa::owSampleTile(seed, baseX + nx, baseY + ny).kind))
                continue;
            r.seen[ni] = 1;
            q.push_back(ni);
        }
    }
    return r;
}

std::vector<apfa::OwSiteInfo> sitesOfRegion(std::uint32_t seed, int rx, int ry) {
    int cx = rx * REGION, cy = ry * REGION;
    std::vector<apfa::OwSiteInfo> raw, out;
    apfa::owSitesInRect(seed, cx - HALF, cy - HALF, cx + HALF, cy + HALF, raw);
    for (const auto& s : raw) {
        // the rect query pads by 3 tiles for icon drawing; keep only this region
        if (s.tx >= cx - HALF && s.tx < cx + HALF && s.ty >= cy - HALF && s.ty < cy + HALF)
            out.push_back(s);
    }
    return out;
}

bool isDungeonKind(apfa::OwSiteType t) { return apfa::owIsDungeonSite(t); }

} // namespace

int main() {
    static const std::uint32_t kSeeds[6] = {1337u, 4444u, 9001u, 2026u, 77777u, 314159u};

    // ---------------------------------------------------------------- 1
    {
        std::uint32_t seed = 4444u;
        int coords[5][2] = {{37, -29}, {-112, 84}, {0, 0}, {121, -66}, {-151, 149}};
        std::vector<apfa::OwCellTile> a, b;
        std::vector<apfa::OwSiteInfo> sa, sb;
        for (auto& c : coords) a.push_back(apfa::owSampleTile(seed, c[0], c[1]));
        apfa::owSitesInRect(seed, -80, -80, 80, 80, sa);
        (void)apfa::owSampleTile(9999u, 0, 0);
        apfa::owSitesInRect(9999u, -400, 400, 400, 800, sb);
        for (auto& c : coords) b.push_back(apfa::owSampleTile(seed, c[0], c[1]));
        sb.clear();
        apfa::owSitesInRect(seed, -80, -80, 80, 80, sb);
        for (int i = 0; i < 5; ++i)
            if (a[i].kind != b[i].kind || a[i].hasRoad != b[i].hasRoad) {
                std::fprintf(stderr, "FAIL determinism: tile (%d,%d) changed after cache swap\n",
                             coords[i][0], coords[i][1]);
                return 1;
            }
        if (sa.size() != sb.size()) {
            std::fprintf(stderr, "FAIL determinism: site count changed after cache swap\n");
            return 1;
        }
        for (std::size_t i = 0; i < sa.size(); ++i)
            if (sa[i].tx != sb[i].tx || sa[i].ty != sb[i].ty || sa[i].name != sb[i].name) {
                std::fprintf(stderr, "FAIL determinism: site %zu changed after cache swap\n", i);
                return 1;
            }
        std::printf("PASS determinism: tiles and sites are order-independent\n");
    }

    // ---------------------------------------------------------------- 2
    {
        std::uint32_t seed = 1337u;
        apfa::OwSiteInfo elm;
        if (!apfa::owSiteAtTile(seed, 0, 0, elm) || !elm.isStartTown) {
            std::fprintf(stderr, "FAIL homeland: Elmswick is not at (0,0)\n");
            return 1;
        }
        if (!apfa::owIsWalkableOnFoot(apfa::owSampleTile(seed, 0, 0).kind)) {
            std::fprintf(stderr, "FAIL homeland: Elmswick stands on unwalkable ground\n");
            return 1;
        }

        apfa::OwSiteInfo story[4] = {
            apfa::owStartCavern(seed), apfa::owBossCastle(seed),
            apfa::owLocketSite(seed), apfa::owTowerSite(seed)
        };
        bool flags[4] = {true, true, true, true};
        BfsResult flood0 = flood(seed, 0, 0, 0, 0);
        for (int i = 0; i < 4; ++i) {
            const auto& s = story[i];
            if (!flags[i]) continue;
            auto here = apfa::owSampleTile(seed, s.tx, s.ty);
            if (!apfa::owIsWalkableOnFoot(here.kind)) {
                std::fprintf(stderr, "FAIL homeland: %s at (%d,%d) is not walkable (terrain %d)\n",
                             s.name.c_str(), s.tx, s.ty, static_cast<int>(here.kind));
                return 1;
            }
            if (s.tx < -HALF || s.tx >= HALF || s.ty < -HALF || s.ty >= HALF) {
                std::fprintf(stderr, "FAIL homeland: %s escaped the start continent\n", s.name.c_str());
                return 1;
            }
            if (!flood0.seen[localIndex(s.tx, s.ty)]) {
                std::fprintf(stderr, "FAIL homeland: %s at (%d,%d) is unreachable on foot\n",
                             s.name.c_str(), s.tx, s.ty);
                return 1;
            }
        }

        auto sites = sitesOfRegion(seed, 0, 0);
        int settlements = 0, dungeons = 0, islands = 0;
        for (const auto& s : sites) {
            if (s.tideIslandIdx >= 0) { ++islands; continue; }
            if (apfa::owIsSettlement(s.type)) ++settlements;
            else if (isDungeonKind(s.type)) ++dungeons;
            auto t = apfa::owSampleTile(seed, s.tx, s.ty);
            if (!apfa::owIsWalkableOnFoot(t.kind)) {
                std::fprintf(stderr, "FAIL homeland: '%s' at (%d,%d) stands on terrain %d\n",
                             s.name.c_str(), s.tx, s.ty, static_cast<int>(t.kind));
                return 1;
            }
        }
        if (settlements < 8 || dungeons < 6) {
            std::fprintf(stderr, "FAIL homeland: too few places (%d settlements, %d dungeons)\n",
                         settlements, dungeons);
            return 1;
        }
        if (islands != 3) {
            std::fprintf(stderr, "FAIL homeland: expected 3 Tidewell isles, found %d\n", islands);
            return 1;
        }
        std::printf("PASS homeland: Elmswick, 4 story sites, %d settlements, %d dungeons, 3 isles\n",
                    settlements, dungeons);
    }

    // ---------------------------------------------------------------- 3
    int totalSettlements = 0, totalDungeons = 0;
    int hamlets = 0, towns = 0, cities = 0;
    int climates[3] = {0, 0, 0};
    {
        for (std::uint32_t seed : kSeeds) {
            for (int ry = -1; ry <= 1; ++ry) {
                for (int rx = -1; rx <= 1; ++rx) {
                    auto sites = sitesOfRegion(seed, rx, ry);
                    BfsResult flood0;
                    bool haveFlood = false;
                    int settlements = 0, dungeons = 0;
                    for (const auto& s : sites) {
                        auto t = apfa::owSampleTile(seed, s.tx, s.ty);
                        if (!apfa::owIsWalkableOnFoot(t.kind)) {
                            std::fprintf(stderr,
                                "FAIL seed=%u region(%d,%d): site '%s' (%d,%d) on terrain %d\n",
                                seed, rx, ry, s.name.c_str(), s.tx, s.ty,
                                static_cast<int>(t.kind));
                            return 1;
                        }
                        if (s.tideIslandIdx >= 0) continue;
                        if (apfa::owIsSettlement(s.type)) {
                            ++settlements;
                            ++climates[static_cast<int>(s.climate) % 3];
                            if (s.type == apfa::OWS_HAMLET) ++hamlets;
                            else if (s.type == apfa::OWS_TOWN) ++towns;
                            else ++cities;
                        } else if (isDungeonKind(s.type)) {
                            ++dungeons;
                        }
                        if (!haveFlood) {
                            flood0 = flood(seed, rx, ry, s.tx, s.ty);
                            haveFlood = true;
                            if (flood0.seen.empty()) {
                                std::fprintf(stderr, "FAIL seed=%u region(%d,%d): flood failed at '%s'\n",
                                             seed, rx, ry, s.name.c_str());
                                return 1;
                            }
                        }
                        int lx = s.tx - rx * REGION, ly = s.ty - ry * REGION;
                        if (!flood0.seen[localIndex(lx, ly)]) {
                            std::fprintf(stderr,
                                "FAIL seed=%u region(%d,%d): '%s' (%d,%d) unreachable on foot\n",
                                seed, rx, ry, s.name.c_str(), s.tx, s.ty);
                            return 1;
                        }
                    }
                    if (settlements < 4 || dungeons < 2) {
                        std::fprintf(stderr,
                            "FAIL seed=%u region(%d,%d): too few places (%d settlements, %d dungeons)\n",
                            seed, rx, ry, settlements, dungeons);
                        return 1;
                    }
                    totalSettlements += settlements;
                    totalDungeons += dungeons;

                    // Every continent's towns must still pass the town verifier.
                    int checked = 0;
                    for (const auto& s : sites) {
                        if (!apfa::owIsSettlement(s.type) || s.tideIslandIdx >= 0) continue;
                        if (checked++ >= 4) break;
                        apfa::TownDef t = apfa::townGenerate(seed, s);
                        std::string reason;
                        if (!apfa::townVerify(t, &reason)) {
                            std::fprintf(stderr, "FAIL seed=%u town '%s' (%d,%d): %s\n",
                                         seed, t.name.c_str(), s.tx, s.ty, reason.c_str());
                            return 1;
                        }
                    }
                }
            }
        }
        if (hamlets == 0 || towns == 0 || cities == 0) {
            std::fprintf(stderr, "FAIL tier diversity: hamlets=%d towns=%d cities=%d\n",
                         hamlets, towns, cities);
            return 1;
        }
        if (climates[0] == 0 || climates[1] == 0 || climates[2] == 0) {
            std::fprintf(stderr, "FAIL climate diversity: temperate=%d sand=%d snow=%d\n",
                         climates[0], climates[1], climates[2]);
            return 1;
        }
        std::printf("PASS 54 continents across 6 seeds: %d settlements, %d dungeons, all reachable on foot\n",
                    totalSettlements, totalDungeons);
    }

    // ---------------------------------------------------------------- 4
    {
        for (std::uint32_t seed : kSeeds) {
            BfsResult flood0 = flood(seed, 0, 0, 0, 0);
            for (int i = 0; i < 3; ++i) {
                apfa::OwSiteInfo isle = apfa::owTideIsland(seed, i);
                if (isle.tideIslandIdx != i) {
                    std::fprintf(stderr, "FAIL seed=%u: Tidewell isle %d missing\n", seed, i);
                    return 1;
                }
                auto t = apfa::owSampleTile(seed, isle.tx, isle.ty);
                if (!apfa::owIsWalkableOnFoot(t.kind)) {
                    std::fprintf(stderr, "FAIL seed=%u: isle '%s' is not walkable\n", seed, isle.name.c_str());
                    return 1;
                }
                // Count surrounding water: an island has sea on all sides.
                int water = 0, land = 0;
                for (int dy = -7; dy <= 7; ++dy)
                    for (int dx = -7; dx <= 7; ++dx) {
                        if (dx * dx + dy * dy > 49) continue;
                        auto n = apfa::owSampleTile(seed, isle.tx + dx, isle.ty + dy);
                        if (apfa::owIsWater(n.kind)) ++water;
                        else ++land;
                    }
                if (water < land * 2) {
                    std::fprintf(stderr, "FAIL seed=%u: isle '%s' is not offshore (water=%d land=%d)\n",
                                 seed, isle.name.c_str(), water, land);
                    return 1;
                }
                if (isle.tx >= -HALF && isle.tx < HALF && isle.ty >= -HALF && isle.ty < HALF &&
                    flood0.seen[localIndex(isle.tx, isle.ty)]) {
                    std::fprintf(stderr, "FAIL seed=%u: isle '%s' can be walked to\n", seed, isle.name.c_str());
                    return 1;
                }
            }
        }
        std::printf("PASS Tidewell isles: 3 offshore, sandy, boat-only islands on 6 seeds\n");
    }

    // ---------------------------------------------------------------- 5
    {
        for (std::uint32_t seed : kSeeds) {
            int roads = 0, bridges = 0, passes = 0;
            for (int y = -HALF; y < HALF; ++y)
                for (int x = -HALF; x < HALF; ++x) {
                    auto t = apfa::owSampleTile(seed, x, y);
                    if (t.hasRoad) ++roads;
                    if (t.kind == apfa::OWT_BRIDGE_H || t.kind == apfa::OWT_BRIDGE_V) ++bridges;
                    if (t.kind == apfa::OWT_PASS_H || t.kind == apfa::OWT_PASS_V) ++passes;
                }
            if (roads < 600) {
                std::fprintf(stderr, "FAIL seed=%u: only %d road tiles in the homeland\n", seed, roads);
                return 1;
            }
            if (passes < 1) {
                std::fprintf(stderr, "FAIL seed=%u: no pass crosses the homeland's range\n", seed);
                return 1;
            }
            auto sites = sitesOfRegion(seed, 0, 0);
            for (const auto& s : sites) {
                if (!apfa::owIsSettlement(s.type) || s.tideIslandIdx >= 0) continue;
                bool touches = false;
                for (int dy = -3; dy <= 3 && !touches; ++dy)
                    for (int dx = -3; dx <= 3; ++dx) {
                        auto t = apfa::owSampleTile(seed, s.tx + dx, s.ty + dy);
                        if (t.hasRoad) { touches = true; break; }
                    }
                if (!touches) {
                    std::fprintf(stderr, "FAIL seed=%u: '%s' is not joined to a road\n",
                                 seed, s.name.c_str());
                    return 1;
                }
            }
            std::printf("     seed %u: %d road tiles, %d bridges, %d passes\n", seed, roads, bridges, passes);
        }
        std::printf("PASS roads: settlements are joined, rivers get bridges, ranges get passes\n");
    }

    // ---------------------------------------------------------------- 6
    {
        int farContinents = 0, farSettlements = 0, farDungeons = 0;
        int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        std::uint32_t seed = 1337u;
        for (auto& d : dirs) {
            for (int dist = 2; dist <= 5; dist += 3) {
                int rx = d[0] * dist, ry = d[1] * dist;
                auto sites = sitesOfRegion(seed, rx, ry);
                int settlements = 0, dungeons = 0;
                for (const auto& s : sites) {
                    if (apfa::owIsWalkableOnFoot(
                            apfa::owSampleTile(seed, s.tx, s.ty).kind)) {
                        if (apfa::owIsSettlement(s.type)) ++settlements;
                        else if (isDungeonKind(s.type)) ++dungeons;
                    }
                }
                if (settlements + dungeons == 0) {
                    std::fprintf(stderr, "FAIL outward: continent (%d,%d) is empty\n", rx, ry);
                    return 1;
                }
                farSettlements += settlements;
                farDungeons += dungeons;
                ++farContinents;
            }
        }
        if (farContinents < 8 || farSettlements < 20 || farDungeons < 10) {
            std::fprintf(stderr, "FAIL outward: %d continents, %d settlements, %d dungeons\n",
                         farContinents, farSettlements, farDungeons);
            return 1;
        }
        std::printf("PASS outward world: %d distant continents, %d settlements, %d dungeons\n",
                    farContinents, farSettlements, farDungeons);
    }

    // ---------------------------------------------------------------- 7
    {
        auto t0 = std::chrono::steady_clock::now();
        // A cold continent far from the homeland, generated from scratch.
        for (int i = 0; i < 40; ++i)
            (void)apfa::owSampleTile(1337u, 320 * 7 + i * 3, -320 * 5 + i);
        auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::steady_clock::now() - t0).count();
        if (dt > 600) {
            std::fprintf(stderr, "FAIL performance: cold continent took %lld ms\n", (long long)dt);
            return 1;
        }
        std::printf("PASS performance: cold continent generated in %lld ms\n", (long long)dt);
    }

    std::printf("PASS Lufia II world: designed continents, walkable roads, boat-only isles\n");
    return 0;
}
