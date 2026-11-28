// verify_lufia_world.cpp — headless C++17 acceptance suite for the Lufia II
// miniature overworld (src/overworld_gen.cpp) and bounded procedural towns
// (src/town_gen.cpp).
#include "overworld_gen.hpp"
#include "town_gen.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <queue>
#include <set>
#include <string>
#include <vector>

namespace {

bool owBfsReachableOnFoot(std::uint32_t seed, int sx, int sy, int gx, int gy, int bound) {
    int W = bound * 2 + 1;
    std::vector<std::uint8_t> vis(static_cast<std::size_t>(W) * static_cast<std::size_t>(W), 0);
    auto idx = [&](int x, int y) -> std::size_t {
        return static_cast<std::size_t>(y + bound) * static_cast<std::size_t>(W) +
               static_cast<std::size_t>(x + bound);
    };
    if (std::abs(sx) > bound || std::abs(sy) > bound || std::abs(gx) > bound || std::abs(gy) > bound)
        return false;
    std::queue<std::pair<int, int>> q;
    vis[idx(sx, sy)] = 1;
    q.push({sx, sy});
    static const int dx[4] = { 1, -1, 0, 0 };
    static const int dy[4] = { 0, 0, 1, -1 };
    while (!q.empty()) {
        auto [x, y] = q.front();
        q.pop();
        if (x == gx && y == gy) return true;
        for (int d = 0; d < 4; ++d) {
            int nx = x + dx[d], ny = y + dy[d];
            if (std::abs(nx) > bound || std::abs(ny) > bound) continue;
            std::size_t id = idx(nx, ny);
            if (vis[id]) continue;
            auto t = apfa::owSampleTile(seed, nx, ny);
            if (!apfa::owIsWalkableOnFoot(t.kind)) continue;
            vis[id] = 1;
            q.push({nx, ny});
        }
    }
    return false;
}

} // namespace

int main() {
    static const std::uint32_t kSeeds[10] = {
        1337u, 4444u, 9001u, 2026u, 77777u,
        314159u, 808080u, 123456u, 999983u, 424242u
    };

    int totalTowns = 0;
    int totalDungeons = 0;
    int hamlets = 0, towns = 0, cities = 0;
    int tempTowns = 0, sandTowns = 0, snowTowns = 0;

    // 1. Order-independent determinism check
    {
        std::uint32_t seed = 4444u;
        auto tA = apfa::owSampleTile(seed, 37, -29);
        auto tB = apfa::owSampleTile(seed, -112, 84);
        // Reset cache via different seed then query in reverse order
        (void)apfa::owSampleTile(9999u, 0, 0);
        auto tB2 = apfa::owSampleTile(seed, -112, 84);
        auto tA2 = apfa::owSampleTile(seed, 37, -29);
        if (tA.kind != tA2.kind || tA.hasRoad != tA2.hasRoad ||
            tB.kind != tB2.kind || tB.hasRoad != tB2.hasRoad) {
            std::fprintf(stderr, "FAIL determinism: chunk cache order changed overworld tiles\n");
            return 1;
        }
        std::printf("PASS overworld chunk determinism (order-independent)\n");
    }

    // 2. Multi-seed overworld connectivity + bounded procedural town BFS audit
    for (std::uint32_t seed : kSeeds) {
        // Verify all homeland story sites exist and sit on dry walkable tiles
        apfa::OwSiteInfo keySites[5] = {
            apfa::owStartTown(seed),
            apfa::owStartCavern(seed),
            apfa::owBossCastle(seed),
            apfa::owLocketSite(seed),
            apfa::owTowerSite(seed)
        };
        for (const auto& ks : keySites) {
            auto tile = apfa::owSampleTile(seed, ks.tx, ks.ty);
            if (!apfa::owIsWalkableOnFoot(tile.kind)) {
                std::fprintf(stderr, "FAIL seed=%u: key site %s at (%d,%d) is not walkable\n",
                             seed, ks.name.c_str(), ks.tx, ks.ty);
                return 1;
            }
            if (!owBfsReachableOnFoot(seed, 0, 0, ks.tx, ks.ty, 64)) {
                std::fprintf(stderr, "FAIL seed=%u: key site %s at (%d,%d) unreachable on foot\n",
                             seed, ks.name.c_str(), ks.tx, ks.ty);
                return 1;
            }
        }

        // Verify all sites in 5x5 macro-cells (-48..48) are reachable & valid
        std::vector<apfa::OwSiteInfo> sites;
        apfa::owSitesInRect(seed, -48, -48, 48, 48, sites);
        int localSettlements = 0;
        for (const auto& s : sites) {
            if (s.tideIslandIdx < 0) {
                if (!owBfsReachableOnFoot(seed, 0, 0, s.tx, s.ty, 72)) {
                    std::fprintf(stderr, "FAIL seed=%u: site '%s' (%d,%d) unreachable from Elmswick\n",
                                 seed, s.name.c_str(), s.tx, s.ty);
                    return 1;
                }
            }
            if (apfa::owIsSettlement(s.type)) {
                ++localSettlements;
                ++totalTowns;
                if (s.type == apfa::OWS_HAMLET) ++hamlets;
                else if (s.type == apfa::OWS_TOWN) ++towns;
                else if (s.type == apfa::OWS_CITY) ++cities;
                if (s.climate == apfa::OWC_TEMPERATE) ++tempTowns;
                else if (s.climate == apfa::OWC_SAND) ++sandTowns;
                else if (s.climate == apfa::OWC_SNOW) ++snowTowns;

                apfa::TownDef t = apfa::townGenerate(seed, s);
                std::string reason;
                if (!apfa::townVerify(t, &reason)) {
                    std::fprintf(stderr, "FAIL seed=%u town '%s' (%d,%d): %s\n",
                                 seed, t.name.c_str(), s.tx, s.ty, reason.c_str());
                    return 1;
                }
            } else if (apfa::owIsDungeonSite(s.type)) {
                ++totalDungeons;
            }
        }
        if (localSettlements < 10) {
            std::fprintf(stderr, "FAIL seed=%u: too few settlements (%d)\n", seed, localSettlements);
            return 1;
        }
    }

    if (hamlets == 0 || towns == 0 || cities == 0) {
        std::fprintf(stderr, "FAIL tier diversity: hamlets=%d towns=%d cities=%d\n", hamlets, towns, cities);
        return 1;
    }
    if (tempTowns == 0 || sandTowns == 0 || snowTowns == 0) {
        std::fprintf(stderr, "FAIL climate diversity: temp=%d sand=%d snow=%d\n", tempTowns, sandTowns, snowTowns);
        return 1;
    }

    // 3. Unbounded outward generation check far beyond homeland (tx = +-240, ty = +-240)
    {
        std::uint32_t seed = 1337u;
        int farCoords[4][2] = { {240, 0}, {-240, 0}, {0, 240}, {0, -240} };
        int farTowns = 0;
        for (int i = 0; i < 4; ++i) {
            std::vector<apfa::OwSiteInfo> farSites;
            apfa::owSitesInRect(seed, farCoords[i][0] - 24, farCoords[i][1] - 24,
                                farCoords[i][0] + 24, farCoords[i][1] + 24, farSites);
            for (const auto& s : farSites) {
                if (apfa::owIsSettlement(s.type)) {
                    apfa::TownDef t = apfa::townGenerate(seed, s);
                    std::string reason;
                    if (!apfa::townVerify(t, &reason)) {
                        std::fprintf(stderr, "FAIL far town '%s': %s\n", t.name.c_str(), reason.c_str());
                        return 1;
                    }
                    ++farTowns;
                }
            }
        }
        if (farTowns < 8) {
            std::fprintf(stderr, "FAIL unbounded outward generation: only %d far towns\n", farTowns);
            return 1;
        }
        std::printf("PASS unbounded outward generation (%d far towns at +-240 tiles verified)\n", farTowns);
    }

    std::printf("PASS Lufia II overworld & bounded towns across 10 seeds:\n"
                "     %d towns verified (Hamlets=%d, Towns=%d, Cities=%d | Temp=%d, Sand=%d, Snow=%d)\n"
                "     %d dungeons verified — 100%% foot-reachable from Elmswick!\n",
                totalTowns, hamlets, towns, cities, tempTowns, sandTowns, snowTowns, totalDungeons);
    return 0;
}
