// dump_world.cpp — print one generated continent for the offline preview tool.
//
// Headless (no raylib): it links only overworld_gen.cpp, so a world can be
// inspected on a machine with no GPU. `tools/preview_world.py` compiles and
// runs this, then composites the real runtime atlas and site icons into PNGs.
//
//   c++ -std=c++17 -O2 -Isrc src/overworld_gen.cpp tools/dump_world.cpp -o dump_world
//   ./dump_world <seed> <regionX> <regionY> > world.txt
#include "overworld_gen.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    std::uint32_t seed = argc > 1 ? static_cast<std::uint32_t>(std::strtoul(argv[1], nullptr, 10)) : 1337u;
    int rx = argc > 2 ? std::atoi(argv[2]) : 0;
    int ry = argc > 3 ? std::atoi(argv[3]) : 0;

    constexpr int REGION = 320;
    constexpr int HALF = REGION / 2;
    const int baseX = rx * REGION;
    const int baseY = ry * REGION;

    std::printf("seed %u\n", seed);
    std::printf("region %d %d\n", rx, ry);
    std::printf("name %s\n", apfa::owContinentName(seed, baseX, baseY).c_str());
    std::printf("title %s\n", apfa::owRegionTitle(seed, baseX, baseY).c_str());

    // grid: one line per row, kind as a letter 'a'+kind, road as 0/1
    for (int ly = -HALF; ly < HALF; ++ly) {
        for (int lx = -HALF; lx < HALF; ++lx) {
            auto t = apfa::owSampleTile(seed, baseX + lx, baseY + ly);
            std::putchar('a' + static_cast<int>(t.kind));
            std::putchar(t.hasRoad ? '1' : '0');
        }
        std::putchar('\n');
    }

    std::vector<apfa::OwSiteInfo> sites;
    apfa::owSitesInRect(seed, baseX - HALF, baseY - HALF, baseX + HALF - 1, baseY + HALF - 1, sites);
    for (const auto& s : sites) {
        if (s.tx < baseX - HALF || s.tx >= baseX + HALF) continue;
        if (s.ty < baseY - HALF || s.ty >= baseY + HALF) continue;
        std::string name = s.name;
        for (char& c : name) if (c == ' ') c = '_';
        std::printf("site %d %d %d %d %d %s\n", s.tx, s.ty, static_cast<int>(s.type),
                    static_cast<int>(s.climate), s.tideIslandIdx, name.c_str());
    }
    return 0;
}
