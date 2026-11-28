// overworld_gen.cpp — the Lufia II world: bounded continents, drawn geography.
//
// The previous pass generated terrain as continuous fBm noise and stitched a
// site and a road into every 24×24 macro-cell. That is a field, not a world: a
// coastline was wherever the noise happened to cross the sea level, roads met
// at right angles on a lattice, and a "continent" was confetti.
//
// This generator works at the scale a world map is designed at. The world is
// divided into WORLD_REGION-sized cells; each cell holds one continent,
// generated from a seeded recipe:
//
//   1. a landmass built from a handful of overlapping, noise-warped blobs
//      (bays, peninsulas and a real coast instead of a threshold);
//   2. mountain ranges drawn as polylines across the land, with passes carved
//      only where a road crosses them;
//   3. rivers drawn from a highland source to a coastal mouth, with bridges
//      where the roads cross them;
//   4. biomes from latitude, elevation and moisture, with a beach along the sea;
//   5. settlements in valleys, on riverbanks and at harbours; dungeons in
//      mountains, deep woods and remote ruins;
//   6. a minimum-spanning road network, A*-routed around water and through
//      passes, so every settlement and every dungeon can be walked to.
//
// Every continent is a pure function of (worldSeed, regionX, regionY): the same
// seed always draws the same coast. The world stays endless — the ring beyond
// the horizon is simply the next continent — but it is a world of places now,
// not an infinite field of noise.
#include "overworld_gen.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <memory>
#include <queue>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace apfa {
namespace {

// ---------------------------------------------------------------- constants
constexpr int REGION = 320;             // tiles on a side of one continent cell
constexpr int HALF = REGION / 2;
constexpr int RIM = 20;                 // open ocean kept inside every region edge
constexpr int N = REGION * REGION;

// A continent is ~200 tiles across: a dozen screens of coast and country.
constexpr float SEA_LEVEL = 0.115f;     // blob field threshold (higher = less land)

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

// splitmix64: deterministic, platform-stable, no global state.
struct Rng {
    std::uint64_t s;
    explicit Rng(std::uint64_t seed) : s(seed) {}
    std::uint32_t u32() {
        s += 0x9E3779B97F4A7C15ull;
        std::uint64_t z = s;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return static_cast<std::uint32_t>(z ^ (z >> 31));
    }
    float f() { return static_cast<float>(u32() >> 8) * (1.0f / 16777216.0f); }
    int range(int n) { return n <= 0 ? 0 : static_cast<int>(u32() % static_cast<std::uint32_t>(n)); }
    float frange(float a, float b) { return a + (b - a) * f(); }
    bool chance(float p) { return f() < p; }
};

inline int ridx(int lx, int ly) {
    return (ly + HALF) * REGION + (lx + HALF);
}
inline bool inRegion(int lx, int ly) {
    return lx >= -HALF && lx < HALF && ly >= -HALF && ly < HALF;
}

struct Province {
    int x = 0, y = 0;
    std::string name;
};

struct Region {
    int rx = 0, ry = 0;
    std::string name;
    std::vector<std::uint8_t> kind;      // OwTileKind
    std::vector<std::uint8_t> road;      // 1 where a road runs
    std::vector<std::uint8_t> river;     // 1 where a river or river-pool runs
    std::vector<Province> provinces;
    std::vector<OwSiteInfo> sites;
};

// ---------------------------------------------------------------- noise
float hashF(int x, int y, std::uint32_t seed) {
    std::uint32_t h = seed ^ static_cast<std::uint32_t>(x) * 2654435761u ^
                      static_cast<std::uint32_t>(y) * 2246822519u;
    h ^= (static_cast<std::uint32_t>(x) * 374761393u +
          static_cast<std::uint32_t>(y) * 668265263u) * 3266489917u;
    return static_cast<float>(mix32(h)) / 4294967295.0f;
}

float smoothNoise(float x, float y, std::uint32_t seed) {
    int xi = static_cast<int>(std::floor(x));
    int yi = static_cast<int>(std::floor(y));
    float xf = x - xi;
    float yf = y - yi;
    float v00 = hashF(xi, yi, seed);
    float v10 = hashF(xi + 1, yi, seed);
    float v01 = hashF(xi, yi + 1, seed);
    float v11 = hashF(xi + 1, yi + 1, seed);
    float u = fadeCurve(xf);
    float v = fadeCurve(yf);
    return v00 + (v10 - v00) * u + (v01 - v00) * v + (v00 - v10 - v01 + v11) * u * v;
}

float fbm(float x, float y, std::uint32_t seed, int octaves) {
    float amp = 0.5f, freq = 1.0f, sum = 0.0f, norm = 0.0f;
    for (int i = 0; i < octaves; ++i) {
        sum += smoothNoise(x * freq, y * freq, seed + static_cast<std::uint32_t>(i) * 1013u) * amp;
        norm += amp;
        amp *= 0.5f;
        freq *= 2.0f;
    }
    return sum / norm;
}

// ---------------------------------------------------------------- climate
OwClimate continentClimate(std::uint32_t seed, int rx, int ry) {
    // The world runs cold in the north and hot in the south, like a world map;
    // the starting continent is temperate. A little hash variation keeps the
    // bands from feeling like wallpaper stripes.
    std::uint32_t h = owHash2(rx * 7 + 13, ry * 11 - 5, seed ^ 0xC11A7Eu);
    if (ry < 0) return (h % 9u == 0u) ? OWC_TEMPERATE : OWC_SNOW;
    if (ry > 0) return (h % 8u == 0u) ? OWC_TEMPERATE : OWC_SAND;
    if (rx == 0 && ry == 0) return OWC_TEMPERATE;
    return (h % 3u == 0u) ? OWC_SAND : OWC_TEMPERATE;
}

OwTileKind plainForClimate(OwClimate c) {
    if (c == OWC_SNOW) return OWT_SNOW;
    if (c == OWC_SAND) return OWT_SAND;
    return OWT_GRASS;
}

bool isWaterKind(OwTileKind k) { return k == OWT_SEA || k == OWT_DEEP_SEA; }
bool isMountainKind(OwTileKind k) { return k == OWT_MOUNTAIN || k == OWT_MOUNTAIN_SNOW; }

// A "land" tile during continent construction (before rivers/mountains).
inline bool isLandKind(OwTileKind k) {
    return k != OWT_DEEP_SEA && k != OWT_SEA;
}

// ---------------------------------------------------------------- land shape
struct Blob {
    float cx, cy, rx, ry;
};
void carveBay(Region& R, float cx, float cy, float rx, float ry);



void buildLand(Region& R, Rng& rng, bool start) {
    const int baseX = R.rx * REGION;
    const int baseY = R.ry * REGION;
    std::vector<Blob> blobs;

    if (start) {
        // The homeland is generated like every other continent: a seeded ring
        // of lobes around one central mass. Only the *shape grammar* is
        // authored (a broad temperate heartland), never the map.
        int lobes = 4 + rng.range(3);
        float phase = rng.frange(0.0f, 6.2831853f);
        Blob heart{0.0f, 0.0f, rng.frange(62.0f, 78.0f), rng.frange(54.0f, 70.0f)};
        blobs.push_back(heart);
        for (int i = 0; i < lobes; ++i) {
            float ang = phase + 6.2831853f * static_cast<float>(i) / static_cast<float>(lobes)
                      + rng.frange(-0.28f, 0.28f);
            float dist = rng.frange(36.0f, 62.0f);
            Blob b;
            b.cx = std::cos(ang) * dist;
            b.cy = std::sin(ang) * dist;
            b.rx = rng.frange(34.0f, 58.0f);
            b.ry = b.rx * rng.frange(0.70f, 1.15f);
            blobs.push_back(b);
        }
    } else {
        const int count = 2 + rng.range(3);
        const float maxCentre = static_cast<float>(HALF - RIM - 84);
        for (int i = 0; i < count; ++i) {
            Blob b;
            b.cx = rng.frange(-maxCentre, maxCentre);
            b.cy = rng.frange(-maxCentre, maxCentre);
            b.rx = rng.frange(48.0f, 86.0f);
            b.ry = b.rx * rng.frange(0.72f, 1.18f);
            blobs.push_back(b);
        }
    }

    R.kind.assign(N, static_cast<std::uint8_t>(OWT_DEEP_SEA));

    for (int ly = -HALF; ly < HALF; ++ly) {
        for (int lx = -HALF; lx < HALF; ++lx) {
            // The warp and coast detail are functions of WORLD coordinates:
            // the same fields every cell sees, so two cells never clone the
            // same coastline. (Blob centres stay local because each cell owns
            // its continent.)
            float wxWorld = static_cast<float>(lx + baseX);
            float wyWorld = static_cast<float>(ly + baseY);
            float wx = static_cast<float>(lx) +
                       (fbm(wxWorld * 0.018f + 31.0f, wyWorld * 0.018f - 11.0f, 0x51A9u, 2) - 0.5f) * 34.0f;
            float wy = static_cast<float>(ly) +
                       (fbm(wxWorld * 0.018f - 7.0f, wyWorld * 0.018f + 23.0f, 0x77C3u, 2) - 0.5f) * 34.0f;

            float shape = -9.0f;
            for (const Blob& b : blobs) {
                float dx = (wx - b.cx) / b.rx;
                float dy = (wy - b.cy) / b.ry;
                shape = std::max(shape, 1.0f - std::sqrt(dx * dx + dy * dy));
            }

            // Coast detail, and extra detail only close to the waterline so
            // the interior stays solid.
            float detail = (fbm(wxWorld * 0.075f + 5.0f, wyWorld * 0.075f + 17.0f, 0xC0A57u, 3) - 0.5f) * 0.16f;
            float v = shape + detail;

            OwTileKind k = OWT_DEEP_SEA;
            if (v > SEA_LEVEL) {
                float depth = v - SEA_LEVEL;
                k = (depth < 0.055f) ? OWT_SEA : OWT_GRASS;
            }
            R.kind[ridx(lx, ly)] = static_cast<std::uint8_t>(k);
        }
    }

    // Clean up one-tile speckle: two majority passes make a coast read as a
    // coast instead of a spilled bag of pixels.
    for (int pass = 0; pass < 2; ++pass) {
        std::vector<std::uint8_t> next = R.kind;
        for (int ly = -HALF + 1; ly < HALF - 1; ++ly) {
            for (int lx = -HALF + 1; lx < HALF - 1; ++lx) {
                int landN = 0;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx)
                        if (isLandKind(static_cast<OwTileKind>(R.kind[ridx(lx + dx, ly + dy)])))
                            ++landN;
                bool self = isLandKind(static_cast<OwTileKind>(R.kind[ridx(lx, ly)]));
                bool land = (landN >= 5);
                if (land && !self) next[ridx(lx, ly)] = static_cast<std::uint8_t>(OWT_GRASS);
                if (!land && self && landN <= 3) next[ridx(lx, ly)] = static_cast<std::uint8_t>(OWT_SEA);
            }
        }
        R.kind.swap(next);
    }

    // Outlying islets: little green specks offshore, not places. They belong
    // near a coast (a screen of sailing away), never alone in the deep ocean
    // where they read as map noise.
    const int islets = start ? 7 : 6 + rng.range(6);
    for (int i = 0; i < islets; ++i) {
        float cx = 0.0f, cy = 0.0f;
        bool placed = false;
        for (int attempt = 0; attempt < 24 && !placed; ++attempt) {
            cx = rng.frange(-HALF + RIM + 14, HALF - RIM - 14);
            cy = rng.frange(-HALF + RIM + 14, HALF - RIM - 14);
            bool tooClose = false, nearCoast = false;
            for (int dy = -24; dy <= 24 && !tooClose; ++dy)
                for (int dx = -24; dx <= 24; ++dx) {
                    int x = static_cast<int>(cx) + dx, y = static_cast<int>(cy) + dy;
                    if (!inRegion(x, y) || !isLandKind(static_cast<OwTileKind>(R.kind[ridx(x, y)]))) continue;
                    int d = std::max(std::abs(dx), std::abs(dy));
                    if (d <= 8) tooClose = true;
                    else nearCoast = true;
                    if (tooClose) break;
                }
            placed = !tooClose && nearCoast;
        }
        if (!placed) continue;
        float rad = rng.frange(1.6f, 3.4f);
        for (int dy = -4; dy <= 4; ++dy)
            for (int dx = -4; dx <= 4; ++dx) {
                int x = static_cast<int>(cx) + dx, y = static_cast<int>(cy) + dy;
                if (!inRegion(x, y)) continue;
                float d = std::sqrt(static_cast<float>(dx * dx + dy * dy));
                if (d <= rad) R.kind[ridx(x, y)] = static_cast<std::uint8_t>(OWT_GRASS);
            }
    }

    // One to three seeded bays: inlets, capes and a coast worth sailing.
    {
        int bays = 1 + rng.range(3);
        for (int i = 0; i < bays; ++i) {
            float ang = rng.frange(0.0f, 6.2831853f);
            float dist = rng.frange(start ? 88.0f : 60.0f, start ? 118.0f : 96.0f);
            carveBay(R, std::cos(ang) * dist, std::sin(ang) * dist,
                     rng.frange(9.0f, 17.0f), rng.frange(7.0f, 13.0f));
        }
    }

    // The world edge is water. Continents never bleed into a neighbour's cell.
    for (int ly = -HALF; ly < HALF; ++ly) {
        for (int lx = -HALF; lx < HALF; ++lx) {
            int edge = std::max(std::abs(lx), std::abs(ly));
            if (edge > HALF - RIM) {
                if (isLandKind(static_cast<OwTileKind>(R.kind[ridx(lx, ly)])))
                    R.kind[ridx(lx, ly)] = static_cast<std::uint8_t>(OWT_SEA);
            }
        }
    }

    (void)baseX;
    (void)baseY;
}

// A bay is a bite taken out of the coast: it only counts if it reaches the
// water, and it keeps coastlines from reading as one smooth ellipse.
void carveBay(Region& R, float cx, float cy, float rx, float ry) {
    int ix0 = static_cast<int>(cx), iy0 = static_cast<int>(cy);
    int r = static_cast<int>(std::ceil(std::max(rx, ry))) + 2;
    bool touchesWater = false;
    for (int dy = -r; dy <= r && !touchesWater; ++dy)
        for (int dx = -r; dx <= r; ++dx) {
            int x = ix0 + dx, y = iy0 + dy;
            if (!inRegion(x, y)) continue;
            float nx = static_cast<float>(dx) / rx, ny = static_cast<float>(dy) / ry;
            if (nx * nx + ny * ny > 1.0f) continue;
            if (isWaterKind(static_cast<OwTileKind>(R.kind[ridx(x, y)]))) { touchesWater = true; break; }
        }
    if (!touchesWater) return;
    for (int dy = -r; dy <= r; ++dy)
        for (int dx = -r; dx <= r; ++dx) {
            int x = ix0 + dx, y = iy0 + dy;
            if (!inRegion(x, y)) continue;
            float nx = static_cast<float>(dx) / rx, ny = static_cast<float>(dy) / ry;
            float wobble = (fbm((x + R.rx * REGION) * 0.19f + 13.0f,
                               (y + R.ry * REGION) * 0.19f - 5.0f, 0xB4A17u, 2) - 0.5f) * 0.30f;
            if (nx * nx + ny * ny + wobble > 1.0f) continue;
            int id = ridx(x, y);
            if (!isLandKind(static_cast<OwTileKind>(R.kind[id]))) continue;
            R.kind[id] = static_cast<std::uint8_t>(OWT_SEA);
        }
}

// The largest connected landmass (4-neighbour, ignoring mountains/rivers since
// those are carved later). Only this component is allowed to hold settlements.
std::vector<std::uint8_t> largestLandComponent(const Region& R) {
    std::vector<std::uint8_t> seen(N, 0);
    std::vector<std::uint8_t> best(N, 0);
    std::vector<int> nodes;
    nodes.reserve(4096);
    int bestSize = 0;
    for (int ly = -HALF; ly < HALF; ++ly) {
        for (int lx = -HALF; lx < HALF; ++lx) {
            int start = ridx(lx, ly);
            if (seen[start] || !isLandKind(static_cast<OwTileKind>(R.kind[start]))) continue;
            nodes.clear();
            nodes.push_back(start);
            seen[start] = 1;
            for (std::size_t head = 0; head < nodes.size(); ++head) {
                int cur = nodes[head];
                int cy = cur / REGION - HALF;
                int cx = cur % REGION - HALF;
                static const int dx4[4] = {1, -1, 0, 0};
                static const int dy4[4] = {0, 0, 1, -1};
                for (int d = 0; d < 4; ++d) {
                    int nx = cx + dx4[d], ny = cy + dy4[d];
                    if (!inRegion(nx, ny)) continue;
                    int ni = ridx(nx, ny);
                    if (seen[ni] || !isLandKind(static_cast<OwTileKind>(R.kind[ni]))) continue;
                    seen[ni] = 1;
                    nodes.push_back(ni);
                }
            }
            if (static_cast<int>(nodes.size()) > bestSize) {
                bestSize = static_cast<int>(nodes.size());
                std::fill(best.begin(), best.end(), 0);
                for (int id : nodes) best[id] = 1;
            }
        }
    }
    return best;
}

// ---------------------------------------------------------------- mountain ranges
struct Pt {
    float x, y;
};

void stampMountainRange(Region& R, const std::vector<Pt>& spine, float width, bool snowCaps) {
    // Rasterise the polyline at half-tile spacing, then widen it. Width tapers
    // at both ends so ranges emerge from the land instead of stopping square.
    for (std::size_t i = 0; i + 1 < spine.size(); ++i) {
        float ax = spine[i].x, ay = spine[i].y;
        float bx = spine[i + 1].x, by = spine[i + 1].y;
        float len = std::sqrt((bx - ax) * (bx - ax) + (by - ay) * (by - ay));
        int steps = std::max(1, static_cast<int>(len / 0.6f));
        for (int s = 0; s < steps; ++s) {
            float t = static_cast<float>(s) / steps;
            float x = ax + (bx - ax) * t;
            float y = ay + (by - ay) * t;
            float along = (i + t) / std::max(1.0f, static_cast<float>(spine.size() - 1));
            float taper = std::min(1.0f, std::min(along * 5.0f, (1.0f - along) * 5.0f));
            float w = width * (0.55f + 0.45f * taper);
            int r = static_cast<int>(std::ceil(w));
            for (int dy = -r; dy <= r; ++dy) {
                for (int dx = -r; dx <= r; ++dx) {
                    int tx = static_cast<int>(std::floor(x)) + dx;
                    int ty = static_cast<int>(std::floor(y)) + dy;
                    if (!inRegion(tx, ty)) continue;
                    float d = std::sqrt(static_cast<float>(dx * dx + dy * dy));
                    if (d > w) continue;
                    int id = ridx(tx, ty);
                    if (!isLandKind(static_cast<OwTileKind>(R.kind[id]))) continue;
                    R.kind[id] = static_cast<std::uint8_t>(snowCaps ? OWT_MOUNTAIN_SNOW : OWT_MOUNTAIN);
                }
            }
        }
    }
}

void buildMountains(Region& R, Rng& rng, bool start, bool snowy) {
    std::vector<std::vector<Pt>> ranges;

    // A wavy spine: a straight polyline reads as a wall of bricks, not a range.
    auto wavy = [&](float x0, float x1, float y0, float y1, float amp, int segs) {
        std::vector<Pt> spine;
        for (int i = 0; i <= segs; ++i) {
            float t = static_cast<float>(i) / segs;
            float x = x0 + (x1 - x0) * t;
            float y = y0 + (y1 - y0) * t;
            if (i > 0 && i < segs) {
                y += rng.frange(-amp, amp);
                x += rng.frange(-amp * 0.5f, amp * 0.5f);
            }
            spine.push_back({x, y});
        }
        return spine;
    };

    if (start) {
        // One northern range is guaranteed — the Lufia silhouette, the wall a
        // young party cannot cross — but its latitude, tilt, length and wave
        // come from the seed, as do the one or two interior ranges. No two
        // homelands have the same mountains.
        float ny = rng.frange(-48.0f, -24.0f);
        float tilt = rng.frange(-16.0f, 16.0f);
        float x0 = -126.0f + rng.frange(-8.0f, 8.0f);
        float x1 =  126.0f + rng.frange(-8.0f, 8.0f);
        ranges.push_back(wavy(x0, x1, ny, ny + tilt, 8.5f, 9 + rng.range(3)));

        int extra = 1 + rng.range(2);
        for (int r = 0; r < extra; ++r) {
            float cx = rng.frange(-46.0f, 46.0f);
            float cy = rng.frange(-6.0f, 52.0f);
            float ang = rng.frange(0.0f, 3.1415926f);
            float len = rng.frange(44.0f, 86.0f);
            float hx = std::cos(ang) * len * 0.5f;
            float hy = std::sin(ang) * len * 0.5f;
            ranges.push_back(wavy(cx - hx, cx + hx, cy - hy, cy + hy, 6.5f, 4 + rng.range(3)));
        }
    } else {
        // One or two ranges, each crossing most of the continent so it is a
        // real barrier with real passes rather than scattered noise ridges.
        int rangesCount = 1 + rng.range(2);
        for (int r = 0; r < rangesCount; ++r) {
            float y0 = rng.frange(-80.0f, 80.0f);
            float y1 = y0 + rng.frange(-70.0f, 70.0f);
            ranges.push_back(wavy(-128.0f, 128.0f, y0, y1, 12.0f, 6 + rng.range(3)));
        }
    }

    for (const auto& range : ranges) stampMountainRange(R, range, 2.15f, snowy);

    // A few isolated massifs / tors to break up the plains.
    const int massifs = start ? 4 : 3 + rng.range(4);
    for (int i = 0; i < massifs; ++i) {
        float cx = rng.frange(-HALF + RIM + 20, HALF - RIM - 20);
        float cy = rng.frange(-HALF + RIM + 20, HALF - RIM - 20);
        int tx = static_cast<int>(cx), ty = static_cast<int>(cy);
        if (!inRegion(tx, ty)) continue;
        if (!isLandKind(static_cast<OwTileKind>(R.kind[ridx(tx, ty)]))) continue;
        float rad = rng.frange(1.4f, 2.8f);
        for (int dy = -4; dy <= 4; ++dy)
            for (int dx = -4; dx <= 4; ++dx) {
                int x = tx + dx, y = ty + dy;
                if (!inRegion(x, y)) continue;
                if (std::sqrt(static_cast<float>(dx * dx + dy * dy)) > rad) continue;
                int id = ridx(x, y);
                if (!isLandKind(static_cast<OwTileKind>(R.kind[id]))) continue;
                R.kind[id] = static_cast<std::uint8_t>(snowy ? OWT_MOUNTAIN_SNOW : OWT_MOUNTAIN);
            }
    }
}

// ---------------------------------------------------------------- rivers
// Draw a smooth polyline from a highland source to a coastal mouth. Rivers are
// one tile wide with occasional two-tile pools; roads later bridge them.
void carveRiver(Region& R, Rng& rng, float sx, float sy, float mx, float my) {
    // The mouth snaps to the nearest sea tile so every river actually reaches
    // the sea instead of stopping in a meadow.
    int mouthX = static_cast<int>(mx), mouthY = static_cast<int>(my);
    bool foundMouth = false;
    for (int r = 0; r <= 26 && !foundMouth; ++r) {
        for (int dy = -r; dy <= r && !foundMouth; ++dy)
            for (int dx = -r; dx <= r; ++dx) {
                if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
                int x = static_cast<int>(mx) + dx, y = static_cast<int>(my) + dy;
                if (!inRegion(x, y)) continue;
                if (isWaterKind(static_cast<OwTileKind>(R.kind[ridx(x, y)]))) {
                    mouthX = x;
                    mouthY = y;
                    foundMouth = true;
                    break;
                }
            }
    }
    if (!foundMouth) return;

    // Control points: source -> two meanders -> mouth.
    float j1x = sx + (mouthX - sx) * 0.33f + rng.frange(-22.0f, 22.0f);
    float j1y = sy + (mouthY - sy) * 0.33f + rng.frange(-16.0f, 16.0f);
    float j2x = sx + (mouthX - sx) * 0.66f + rng.frange(-22.0f, 22.0f);
    float j2y = sy + (mouthY - sy) * 0.66f + rng.frange(-16.0f, 16.0f);
    Pt ctrl[4] = {{sx, sy}, {j1x, j1y}, {j2x, j2y},
                  {static_cast<float>(mouthX), static_cast<float>(mouthY)}};

    auto stampRiverPoint = [&](float fx, float fy, bool pool) {
        int x = static_cast<int>(std::floor(fx));
        int y = static_cast<int>(std::floor(fy));
        int radius = pool ? 1 : 0;
        for (int dy = -radius; dy <= radius; ++dy)
            for (int dx = -radius; dx <= radius; ++dx) {
                int nx = x + dx, ny = y + dy;
                if (!inRegion(nx, ny)) continue;
                int id = ridx(nx, ny);
                if (!isLandKind(static_cast<OwTileKind>(R.kind[id]))) continue;
                if (isMountainKind(static_cast<OwTileKind>(R.kind[id]))) continue;
                R.kind[id] = static_cast<std::uint8_t>(OWT_SEA);
                R.river[id] = 1;
            }
    };

    // Sample a Catmull-Rom spline through the four control points, and stamp
    // every tile along each segment. Point sampling alone leaves dry gaps that
    // a road can walk through — and then there is no bridge and no reason for
    // one, which is how the old generator ended up with no water crossings.
    bool havePrev = false;
    float prevX = 0.0f, prevY = 0.0f;
    auto connect = [&](float x, float y, bool pool) {
        if (havePrev) {
            float dx = x - prevX, dy = y - prevY;
            float len = std::sqrt(dx * dx + dy * dy);
            int steps = std::max(1, static_cast<int>(std::ceil(len * 2.0f)));
            for (int i = 1; i <= steps; ++i) {
                float t = static_cast<float>(i) / steps;
                stampRiverPoint(prevX + dx * t, prevY + dy * t, pool);
            }
        } else {
            stampRiverPoint(x, y, pool);
        }
        prevX = x;
        prevY = y;
        havePrev = true;
    };

    for (int seg = 0; seg < 3; ++seg) {
        Pt p0 = ctrl[seg == 0 ? 0 : seg - 1];
        Pt p1 = ctrl[seg];
        Pt p2 = ctrl[seg + 1];
        Pt p3 = ctrl[seg + 2 > 3 ? 3 : seg + 2];
        int steps = 26;
        for (int s = 0; s < steps; ++s) {
            float t = static_cast<float>(s) / steps;
            float t2 = t * t, t3 = t2 * t;
            float x = 0.5f * ((2.0f * p1.x) + (-p0.x + p2.x) * t +
                              (2.0f * p0.x - 5.0f * p1.x + 4.0f * p2.x - p3.x) * t2 +
                              (-p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x) * t3);
            float y = 0.5f * ((2.0f * p1.y) + (-p0.y + p2.y) * t +
                              (2.0f * p0.y - 5.0f * p1.y + 4.0f * p2.y - p3.y) * t2 +
                              (-p0.y + 3.0f * p1.y - 3.0f * p2.y + p3.y) * t3);
            bool pool = (seg == 1 && (s == 8 || s == 9 || s == 10));
            connect(x, y, pool);
        }
    }
}

void carveLake(Region& R, float cx, float cy, float rx, float ry) {
    int ix0 = static_cast<int>(cx), iy0 = static_cast<int>(cy);
    int r = static_cast<int>(std::ceil(std::max(rx, ry))) + 2;
    for (int dy = -r; dy <= r; ++dy)
        for (int dx = -r; dx <= r; ++dx) {
            int x = ix0 + dx, y = iy0 + dy;
            if (!inRegion(x, y)) continue;
            float nx = static_cast<float>(dx) / rx;
            float ny = static_cast<float>(dy) / ry;
            float wobble = (fbm((x + R.rx * REGION) * 0.22f + 3.0f,
                               (y + R.ry * REGION) * 0.22f - 8.0f, 0x1A4E5u, 2) - 0.5f) * 0.35f;
            if (nx * nx + ny * ny + wobble > 1.0f) continue;
            int id = ridx(x, y);
            if (!isLandKind(static_cast<OwTileKind>(R.kind[id]))) continue;
            R.kind[id] = static_cast<std::uint8_t>(OWT_SEA);
            R.river[id] = 0;   // a lake is not a river: no bridge bookkeeping
        }
}

void buildRivers(Region& R, Rng& rng, bool start, const std::vector<Blob>& blobs, OwClimate climate) {
    if (start) {
        // Rivers are seeded too: two or three run from the vale to the sea,
        // and one or two lakes sit in the hollows between the ranges. Only the
        // rule is authored — every river reaches salt water, every lake is
        // inland — never the course.
        int rivers = 2 + rng.range(2);
        for (int i = 0; i < rivers; ++i) {
            float sx = rng.frange(-58.0f, 58.0f);
            float sy = rng.frange(-26.0f, 12.0f);
            float mx = sx + rng.frange(-34.0f, 34.0f);
            float my = rng.frange(86.0f, 116.0f);
            carveRiver(R, rng, sx, sy, mx, my);
        }
        int lakes = 1 + rng.range(2);
        for (int i = 0; i < lakes; ++i) {
            for (int attempt = 0; attempt < 40; ++attempt) {
                float ang = rng.frange(0.0f, 6.2831853f);
                float dist = rng.frange(26.0f, 62.0f);
                int cx = static_cast<int>(std::cos(ang) * dist);
                int cy = static_cast<int>(std::sin(ang) * dist);
                if (std::abs(cx) < 14 && std::abs(cy) < 14) continue;
                if (!isLandKind(static_cast<OwTileKind>(R.kind[ridx(cx, cy)]))) continue;
                carveLake(R, static_cast<float>(cx), static_cast<float>(cy),
                          rng.frange(6.0f, 11.0f), rng.frange(4.0f, 8.0f));
                break;
            }
        }
    } else {
        if (rng.chance(0.55f))
            carveLake(R, rng.frange(-60.0f, 60.0f), rng.frange(-60.0f, 60.0f),
                      rng.frange(5.0f, 11.0f), rng.frange(4.0f, 8.0f));
        int count = 2 + rng.range(2);
        for (int i = 0; i < count; ++i) {
            // Pick a land source and a coast, then let the mouth snap to water.
            float sx = rng.frange(-90.0f, 90.0f);
            float sy = rng.frange(-90.0f, 60.0f);
            float mx = sx + rng.frange(-40.0f, 40.0f);
            float my = rng.frange(40.0f, 110.0f);
            if (climate == OWC_SNOW) my = rng.frange(-110.0f, -40.0f);
            carveRiver(R, rng, sx, sy, mx, my);
        }
    }
    (void)blobs;
}

// ---------------------------------------------------------------- biomes
void paintBiomes(Region& R, Rng& rng, bool start, OwClimate climate) {
    (void)rng;
    // A single world-wide field: two neighbouring cells agree about where the
    // woods are, and no cell repeats another's forest pattern.
    const std::uint32_t seed = 0x5EEDu;
    const int baseX = R.rx * REGION, baseY = R.ry * REGION;

    for (int ly = -HALF; ly < HALF; ++ly) {
        for (int lx = -HALF; lx < HALF; ++lx) {
            int id = ridx(lx, ly);
            OwTileKind k = static_cast<OwTileKind>(R.kind[id]);
            if (isWaterKind(k) || isMountainKind(k)) continue;

            float x = static_cast<float>(lx + baseX);
            float y = static_cast<float>(ly + baseY);
            // Rotated, higher-frequency fields: value noise on an axis-aligned
            // lattice reads as rectangles at low frequency, and rectangles are
            // exactly what a forest must not look like.
            float fx = x * 0.062f + y * 0.024f;
            float fy = y * 0.062f - x * 0.024f;
            float woods = fbm(fx + 9.0f, fy - 4.0f, seed ^ 0xF02E57u, 3);
            float moist = fbm(fx * 0.55f - 3.0f, fy * 0.55f + 12.0f, seed ^ 0x39A1Cu, 2);

            // Distance to the nearest water, cheaply: cardinal scan of 6 tiles.
            bool nearWater = false;
            for (int d = 1; d <= 6 && !nearWater; ++d) {
                int nx = lx + d, ny = ly + d;
                if (inRegion(nx, ly) && isWaterKind(static_cast<OwTileKind>(R.kind[ridx(nx, ly)])))
                    nearWater = true;
                if (inRegion(lx, ny) && isWaterKind(static_cast<OwTileKind>(R.kind[ridx(lx, ny)])))
                    nearWater = true;
                if (inRegion(lx - d, ly) && isWaterKind(static_cast<OwTileKind>(R.kind[ridx(lx - d, ly)])))
                    nearWater = true;
                if (inRegion(lx, ly - d) && isWaterKind(static_cast<OwTileKind>(R.kind[ridx(lx, ly - d)])))
                    nearWater = true;
            }

            OwTileKind out = plainForClimate(climate);
            if (climate == OWC_SNOW) {
                out = (woods > 0.56f) ? OWT_FOREST_SNOW : OWT_SNOW;
            } else if (climate == OWC_SAND) {
                out = (nearWater && moist > 0.62f) ? OWT_GRASS : OWT_SAND;
            } else {
                if (woods > 0.60f) {
                    out = (moist > 0.52f && y < 20.0f) ? OWT_FOREST_PINE : OWT_FOREST_LEAF;
                } else if (nearWater && moist > 0.66f && woods < 0.56f) {
                    out = OWT_SWAMP;
                } else {
                    out = OWT_GRASS;
                }
            }

            // The homeland's deliberate set-dressing: a broad western wood, a
            // darker pine belt under the north wall, and open heath east of the
            // river so the vale reads as three different places.
            if (start && out == OWT_GRASS) {
                if (x < -26.0f && x > -78.0f && y > -22.0f && y < 34.0f &&
                    fbm(x * 0.11f, y * 0.11f, seed ^ 0xEA57u, 2) > 0.42f)
                    out = OWT_FOREST_LEAF;
                if (y < -20.0f && x > -30.0f && x < 52.0f)
                    out = OWT_FOREST_PINE;
            }

            R.kind[id] = static_cast<std::uint8_t>(out);
        }
    }

    // Copses: seeded 2x2 stands of canopy on open ground, on a WORLD grid so
    // a stand that straddles a cell border is stamped by both cells. Single
    // forest tiles render as bare corners of the canopy mask, so always plant
    // a block.
    int wgx0 = baseX - HALF - 7, wgx1 = baseX + HALF + 7;
    int wgy0 = baseY - HALF - 7, wgy1 = baseY + HALF + 7;
    for (int gy = wgy0 - (wgy0 % 7); gy < wgy1; gy += 7) {
        for (int gx = wgx0 - (wgx0 % 7); gx < wgx1; gx += 7) {
            std::uint32_t h = owHash2(gx, gy, seed ^ 0xB055u);
            if ((h % 100u) >= 46u) continue;
            int cx = gx + static_cast<int>((h >> 8) % 5u) - baseX;
            int cy = gy + static_cast<int>((h >> 12) % 5u) - baseY;
            OwTileKind base = plainForClimate(climate);
            OwTileKind forest = (climate == OWC_SNOW) ? OWT_FOREST_SNOW
                              : (climate == OWC_SAND ? OWT_GRASS : OWT_FOREST_LEAF);
            bool ok = true;
            for (int dy = 0; dy < 2 && ok; ++dy)
                for (int dx = 0; dx < 2; ++dx) {
                    int x = cx + dx, y = cy + dy;
                    if (!inRegion(x, y)) { ok = false; break; }
                    if (R.kind[ridx(x, y)] != static_cast<std::uint8_t>(base)) { ok = false; break; }
                }
            if (!ok) continue;
            for (int dy = 0; dy < 2; ++dy)
                for (int dx = 0; dx < 2; ++dx)
                    R.kind[ridx(cx + dx, cy + dy)] = static_cast<std::uint8_t>(forest);
        }
    }

    // The vale around Elmswick gets a few hand-set stands so the opening view
    // is a place and not a lawn.
    if (start) {
        struct Copse { int x, y; };
        static const Copse kCopses[] = {
            {-9, -5}, {-7, 2}, {11, -7}, {17, 4}, {-14, 6}, {8, 13}, {-2, -12}, {20, -2},
        };
        for (const Copse& c : kCopses)
            for (int dy = 0; dy < 2; ++dy)
                for (int dx = 0; dx < 2; ++dx) {
                    int x = c.x + dx, y = c.y + dy;
                    if (!inRegion(x, y)) continue;
                    if (R.kind[ridx(x, y)] == static_cast<std::uint8_t>(OWT_GRASS))
                        R.kind[ridx(x, y)] = static_cast<std::uint8_t>(OWT_FOREST_LEAF);
                }
    }

    // Beaches: a one-tile sand (or snow) rim where land meets the open sea.
    std::vector<std::uint8_t> next = R.kind;
    for (int ly = -HALF + 1; ly < HALF - 1; ++ly) {
        for (int lx = -HALF + 1; lx < HALF - 1; ++lx) {
            int id = ridx(lx, ly);
            OwTileKind k = static_cast<OwTileKind>(R.kind[id]);
            if (!isLandKind(k) || isMountainKind(k)) continue;
            bool seaSide = false;
            static const int dx4[4] = {1, -1, 0, 0};
            static const int dy4[4] = {0, 0, 1, -1};
            for (int d = 0; d < 4; ++d) {
                int nx = lx + dx4[d], ny = ly + dy4[d];
                if (!inRegion(nx, ny)) continue;
                int nid = ridx(nx, ny);
                OwTileKind nk = static_cast<OwTileKind>(R.kind[nid]);
                // A river bank is moss and grass, not a beach; only the open sea
                // gets sand. (Rivers share the water tile kind.)
                if ((nk == OWT_SEA || nk == OWT_DEEP_SEA) && !R.river[nid]) seaSide = true;
            }
            if (!seaSide) continue;
            next[id] = static_cast<std::uint8_t>(climate == OWC_SNOW ? OWT_SNOW : OWT_SAND);
        }
    }
    R.kind.swap(next);
}

// ---------------------------------------------------------------- site placement
bool walkableGroundKind(OwTileKind k) {
    return isLandKind(k) && !isMountainKind(k);
}

bool sitePredicateOk(const Region& R, int lx, int ly, OwSiteType type) {
    if (!inRegion(lx, ly)) return false;
    OwTileKind k = static_cast<OwTileKind>(R.kind[ridx(lx, ly)]);
    if (!walkableGroundKind(k)) return false;
    if (type == OWS_CAVE || type == OWS_CASTLE) {
        bool nearCrag = false;
        for (int dy = -3; dy <= 3 && !nearCrag; ++dy)
            for (int dx = -3; dx <= 3; ++dx) {
                int x = lx + dx, y = ly + dy;
                if (inRegion(x, y) && isMountainKind(static_cast<OwTileKind>(R.kind[ridx(x, y)])))
                    nearCrag = true;
            }
        if (!nearCrag) {
            // A cave can also open in a deep wood; a castle cannot.
            if (type == OWS_CASTLE) return false;
            int forest = 0;
            for (int dy = -2; dy <= 2; ++dy)
                for (int dx = -2; dx <= 2; ++dx) {
                    int x = lx + dx, y = ly + dy;
                    if (!inRegion(x, y)) continue;
                    OwTileKind nk = static_cast<OwTileKind>(R.kind[ridx(x, y)]);
                    if (nk == OWT_FOREST_LEAF || nk == OWT_FOREST_PINE || nk == OWT_FOREST_SNOW)
                        ++forest;
                }
            if (forest < 12) return false;
        }
    }
    return true;
}

struct SiteSnap {
    int x = 0, y = 0;
    bool found = false;
};

SiteSnap snapToSiteGround(const Region& R, int dx, int dy, OwSiteType type, int maxR = 34) {
    for (int r = 0; r <= maxR; ++r) {
        for (int oy = -r; oy <= r; ++oy) {
            for (int ox = -r; ox <= r; ++ox) {
                if (std::max(std::abs(ox), std::abs(oy)) != r) continue;
                int x = dx + ox, y = dy + oy;
                if (sitePredicateOk(R, x, y, type)) return {x, y, true};
            }
        }
    }
    return {dx, dy, false};
}

std::string uniqueSettlementName(std::uint32_t seed, int tx, int ty, OwSiteType type,
                                 OwClimate climate, int salt) {
    // Names come from the shared tables; the salt keeps two sites that happen
    // to share a coordinate-hash from also sharing a name inside one country.
    std::string name = owSettlementName(static_cast<std::uint32_t>(seed ^ (salt * 0x9E3779B9u)),
                                        tx, ty, type, climate);
    return name;
}

struct SitePlan {
    OwSiteType type;
    OwClimate climate;
    int x, y;
    std::uint32_t seed;
    bool story = false;
    bool island = false;
    int islandIdx = -1;
    const char* fixedName = nullptr;
};

void addSite(Region& R, std::vector<SitePlan>& planned, const SitePlan& p) {
    (void)R;
    planned.push_back(p);
}

// Choose a handful of good settlement and dungeon positions inside the main
// land component. Settlements like riverbanks and coasts; dungeons like crags,
// deep woods and lonely corners.
void planGenericSites(Region& R, Rng& rng, OwClimate climate,
                      const std::vector<std::uint8_t>& mainLand,
                      std::vector<SitePlan>& out, float scale = 1.0f) {
    struct Candidate {
        int x, y;
        float score;
    };
    std::vector<Candidate> land;
    land.reserve(4096);
    for (int ly = -HALF + 10; ly < HALF - 10; ++ly) {
        for (int lx = -HALF + 10; lx < HALF - 10; ++lx) {
            int id = ridx(lx, ly);
            OwTileKind k = static_cast<OwTileKind>(R.kind[id]);
            if (!mainLand[id] || !walkableGroundKind(k)) continue;
            if (k == OWT_SWAMP) continue;
            float s = rng.f();
            land.push_back({lx, ly, s});
        }
    }
    if (land.empty()) return;

    auto nearWater = [&](int lx, int ly, int r) {
        for (int dy = -r; dy <= r; ++dy)
            for (int dx = -r; dx <= r; ++dx) {
                int x = lx + dx, y = ly + dy;
                if (!inRegion(x, y)) continue;
                if (isWaterKind(static_cast<OwTileKind>(R.kind[ridx(x, y)]))) return true;
            }
        return false;
    };
    auto nearCrag = [&](int lx, int ly, int r) {
        for (int dy = -r; dy <= r; ++dy)
            for (int dx = -r; dx <= r; ++dx) {
                int x = lx + dx, y = ly + dy;
                if (!inRegion(x, y)) continue;
                if (isMountainKind(static_cast<OwTileKind>(R.kind[ridx(x, y)]))) return true;
            }
        return false;
    };
    auto farFromSites = [&](int lx, int ly, int minD2) {
        for (const auto& p : out) {
            int dx = p.x - lx, dy = p.y - ly;
            if (dx * dx + dy * dy < minD2) return false;
        }
        return true;
    };

    // Largest landmass colonies: 6-9 settlements, one of them a city.
    const int settlements = std::max(1, static_cast<int>((6 + rng.range(4)) * scale));
    for (int s = 0; s < settlements; ++s) {
        Candidate best{-9999, -9999, -1e9f};
        for (int trial = 0; trial < 700; ++trial) {
            const Candidate& c = land[rng.range(static_cast<int>(land.size()))];
            if (!farFromSites(c.x, c.y, 22 * 22)) continue;
            float score = rng.f() * 3.0f;
            if (nearWater(c.x, c.y, 3)) score += 4.0f;
            if (nearWater(c.x, c.y, 1)) score += 2.0f;
            if (nearCrag(c.x, c.y, 2)) score -= 1.5f;
            // towns like the middle of the land, not the last rock in the sea
            float d = std::sqrt(static_cast<float>(c.x * c.x + c.y * c.y));
            score += std::max(0.0f, 2.0f - d * 0.015f);
            if (score > best.score) best = {c.x, c.y, score};
        }
        if (best.score < -1000.0f) continue;
        SitePlan p;
        p.climate = climate;
        p.x = best.x;
        p.y = best.y;
        int tierRoll = rng.range(100);
        p.type = (s == 0) ? OWS_CITY
               : (tierRoll < 42) ? OWS_HAMLET : OWS_TOWN;
        p.seed = owHash2(p.x, p.y, 0x7011u * (static_cast<std::uint32_t>(R.rx + 3) ^ 0x9E37u));
        addSite(R, out, p);
    }

    // Dungeons: 5-8, at crags, in woods, or in remote corners.
    const int dungeons = std::max(1, static_cast<int>((5 + rng.range(4)) * scale));
    for (int d = 0; d < dungeons; ++d) {
        Candidate best{-9999, -9999, -1e9f};
        for (int trial = 0; trial < 700; ++trial) {
            const Candidate& c = land[rng.range(static_cast<int>(land.size()))];
            if (!farFromSites(c.x, c.y, 16 * 16)) continue;
            float score = rng.f() * 4.0f;
            if (nearCrag(c.x, c.y, 2)) score += 4.0f;
            if (nearWater(c.x, c.y, 1)) score += 0.5f;
            float d2 = std::sqrt(static_cast<float>(c.x * c.x + c.y * c.y));
            score += std::min(2.0f, d2 * 0.012f);
            if (score > best.score) best = {c.x, c.y, score};
        }
        if (best.score < -1000.0f) continue;
        SitePlan p;
        p.climate = climate;
        p.x = best.x;
        p.y = best.y;
        static const OwSiteType kPool[5] = {OWS_CAVE, OWS_TOWER, OWS_RUINS, OWS_TEMPLE, OWS_CASTLE};
        p.type = kPool[rng.range(5)];
        p.seed = owHash2(p.x, p.y, 0xD0A6u ^ static_cast<std::uint32_t>(R.rx * 31 + R.ry * 17));
        addSite(R, out, p);
    }
}

void planStartSites(Region& R, std::uint32_t seed, Rng& rng, std::vector<SitePlan>& out) {
    // The story places are anchors, not coordinates. Each pin is a rule about
    // where it belongs (the castle in the northern crags, the watchtower on the
    // north-west shoulder, the stones in the west), and the seed chooses the
    // actual site. Two seeds with the same story still have different maps.
    auto nearSea = [&](int x, int y, int r) {
        for (int dy = -r; dy <= r; ++dy)
            for (int dx = -r; dx <= r; ++dx) {
                int nx = x + dx, ny = y + dy;
                if (!inRegion(nx, ny)) continue;
                if (isWaterKind(static_cast<OwTileKind>(R.kind[ridx(nx, ny)]))) return true;
            }
        return false;
    };
    auto fits = [&](int x, int y, int spacing) {
        for (const SitePlan& p : out)
            if (std::max(std::abs(p.x - x), std::abs(p.y - y)) < spacing) return false;
        return true;
    };

    const int townX = 0, townY = 0;
    // The one fixed point in the world: the party's home gate. If a river or
    // lake wandered over it, make an island of dry ground and reconnect it.
    for (int dy = -2; dy <= 2; ++dy)
        for (int dx = -2; dx <= 2; ++dx) {
            int id = ridx(townX + dx, townY + dy);
            if (owIsWater(static_cast<OwTileKind>(R.kind[id]))) {
                R.kind[id] = static_cast<std::uint8_t>(OWT_GRASS);
                R.river[id] = 0;
            }
        }

    auto place = [&](OwSiteType type, const char* name, int minR, int maxR,
                     float a0, float a1, int spacing, bool wantCrag, bool wantSea,
                     int boxX0, int boxY0, int boxX1, int boxY1) -> bool {
        // Try the authored conditions first (angle/radius band, a crag for a
        // castle, a shore for a cave). Then relax them one at a time so a
        // story site is never silently missing from a continent.
        for (int strict = 0; strict < 3; ++strict) {
            for (int attempt = 0; attempt < 420; ++attempt) {
                float ang = rng.frange(a0, a1);
                float dist = rng.frange(static_cast<float>(minR), static_cast<float>(maxR));
                int x = static_cast<int>(std::round(std::cos(ang) * dist));
                int y = static_cast<int>(std::round(std::sin(ang) * dist));
                if (!inRegion(x, y) || !inRegion(x + 2, y + 2)) continue;
                if (x < boxX0 || x > boxX1 || y < boxY0 || y > boxY1) continue;
                if (!isLandKind(static_cast<OwTileKind>(R.kind[ridx(x, y)]))) continue;
                if (strict == 0 && wantCrag && !sitePredicateOk(R, x, y, type)) continue;
                if (strict <= 1 && wantSea && !nearSea(x, y, 6)) continue;
                if (!fits(x, y, spacing)) continue;
                SitePlan p;
                p.type = type;
                p.climate = OWC_TEMPERATE;
                p.x = x;
                p.y = y;
                p.seed = owHash2(x, y, seed ^ 0xE135u);
                p.story = true;
                p.fixedName = name;
                out.push_back(p);
                return true;
            }
        }
        // Absolute fallback: any dry tile in the allowed box.
        for (int r = 0; r <= 90; ++r) {
            for (int dy = -r; dy <= r; ++dy)
                for (int dx = -r; dx <= r; ++dx) {
                    if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
                    int x = townX + dx, y = townY + dy;
                    if (x < boxX0 || x > boxX1 || y < boxY0 || y > boxY1) continue;
                    if (!isLandKind(static_cast<OwTileKind>(R.kind[ridx(x, y)]))) continue;
                    if (!fits(x, y, spacing)) continue;
                    SitePlan p;
                    p.type = type;
                    p.climate = OWC_TEMPERATE;
                    p.x = x;
                    p.y = y;
                    p.seed = owHash2(x, y, seed ^ 0xE135u);
                    p.story = true;
                    p.fixedName = name;
                    out.push_back(p);
                    return true;
                }
        }
        return false;
    };

    // Elmswick: the origin anchor (the runtime starts here), on the vale floor.
    {
        SitePlan p;
        p.type = OWS_TOWN;
        p.climate = OWC_TEMPERATE;
        p.x = townX;
        p.y = townY;
        p.seed = owHash2(townX, townY, seed ^ 0xE135u);
        p.story = true;
        p.fixedName = "Elmswick";
        out.push_back(p);
    }

    // North: the castle among the crags, the tower on the north-west shoulder.
    place(OWS_CASTLE, "Castle of Elm", 34, 74, -2.55f, -0.60f, 12, true, false,
          -76, -76, 76, -20);
    place(OWS_TOWER, "Old Watchtower", 22, 58, -2.60f, -1.20f, 10, false, false,
          -72, -58, -8, -8);

    // West and south-west: the standing stones on open ground.
    place(OWS_RUINS, "Standing Stones", 18, 56, 1.90f, 3.60f, 10, false, false,
          -72, -28, -8, 54);

    // The cave in the home woods, a short walk from the gate.
    place(OWS_CAVE, "Hollow Hill", 13, 40, -3.14f, 3.14f, 9, true, false,
          -56, -56, 56, 56);

    // The hamlets, one per seeded sector so they scatter around the vale.
    struct HamletPin { const char* name; };
    static const HamletPin kHamlets[] = {
        {"Oakford"}, {"Saltmere"}, {"Millbrook"}, {"Briarwick"}, {"Thornbury"}, {"Fenwick"},
    };
    float phase = rng.frange(0.0f, 6.2831853f);
    for (int i = 0; i < 6; ++i) {
        float a0 = phase + 6.2831853f * static_cast<float>(i) / 6.0f - 0.55f;
        float a1 = a0 + 1.10f;
        place(OWS_HAMLET, kHamlets[i].name, 18, 68, a0, a1, 11, false, false,
              -100, -100, 100, 100);
    }

    // Two outlying story errands: the shrine in the south, the gull cave on a
    // southern shore, a spire watching the east.
    place(OWS_TEMPLE, "Shrine of the Vale", 26, 62, 0.55f, 2.45f, 10, false, false,
          -60, 0, 70, 80);
    place(OWS_CAVE, "Gull Rock Cave", 34, 74, 0.85f, 2.60f, 10, false, true,
          -50, 16, 80, 92);
    place(OWS_TOWER, "Eastwatch Spire", 38, 74, -0.85f, 0.55f, 10, false, false,
          18, -50, 92, 30);
}

// Tidewell islands: three small, deliberately placed isles in the southern sea,
// each with a camp and a landing beach. They are the reason to buy a boat.
void carveTideIslands(Region& R, std::uint32_t seed, Rng& rng, std::vector<SitePlan>& out) {
    static const char* kNames[3] = {"Tidewell Cay", "Lantern Shoal", "Saltspire Isle"};
    int made = 0;
    for (int i = 0; i < 3; ++i) {
        bool found = false;
        for (int attempt = 0; attempt < 400 && !found; ++attempt) {
            // The isles live in the southern sea the runtime searches for them
            // (x -100..100, y 60..130), well clear of any coast.
            int cx = -94 + rng.range(189);
            int cy = 74 + rng.range(51);
            bool allWater = true;
            for (int dy = -7; dy <= 7 && allWater; ++dy)
                for (int dx = -7; dx <= 7; ++dx) {
                    int x = cx + dx, y = cy + dy;
                    if (!inRegion(x, y)) { allWater = false; break; }
                    if (isLandKind(static_cast<OwTileKind>(R.kind[ridx(x, y)]))) { allWater = false; break; }
                }
            if (!allWater) continue;
            bool tooNear = false;
            for (const SitePlan& p : out)
                if (p.island && std::max(std::abs(p.x - cx), std::abs(p.y - cy)) < 30) { tooNear = true; break; }
            if (tooNear) continue;

            // Own the surrounding water so the isle really is offshore: clear
            // any random islet the continent's islet pass dropped nearby.
            for (int dy = -10; dy <= 10; ++dy)
                for (int dx = -10; dx <= 10; ++dx) {
                    int x = cx + dx, y = cy + dy;
                    if (!inRegion(x, y)) continue;
                    int id = ridx(x, y);
                    if (isLandKind(static_cast<OwTileKind>(R.kind[id])))
                        R.kind[id] = static_cast<std::uint8_t>(OWT_DEEP_SEA);
                    R.river[id] = 0;
                }
            float rad = 3.1f + static_cast<float>(owHash2(cx, cy, seed ^ 0x71DEu) % 5u) * 0.22f;
            for (int dy = -5; dy <= 5; ++dy)
                for (int dx = -5; dx <= 5; ++dx) {
                    int x = cx + dx, y = cy + dy;
                    if (!inRegion(x, y)) continue;
                    float d = std::sqrt(static_cast<float>(dx * dx + dy * dy));
                    if (d <= rad)
                        R.kind[ridx(x, y)] = static_cast<std::uint8_t>(OWT_SAND);
                    else if (d <= rad + 1.0f)
                        R.kind[ridx(x, y)] = static_cast<std::uint8_t>(OWT_SEA);
                }
            SitePlan p;
            p.type = OWS_CAMP;
            p.climate = OWC_TEMPERATE;
            p.x = cx;
            p.y = cy;
            p.seed = owHash2(cx, cy, seed ^ 0x71DEu);
            p.island = true;
            p.islandIdx = i;
            p.fixedName = kNames[i];
            out.push_back(p);
            ++made;
            found = true;
        }
    }
    (void)made;
}

// ---------------------------------------------------------------- pads
void clearSitePad(Region& R, const OwSiteInfo& s, OwClimate climate) {
    const bool settlement = owIsSettlement(s.type);
    const int radius = settlement ? 2 : 1;
    OwTileKind ground = plainForClimate(climate);
    for (int dy = -radius; dy <= radius; ++dy)
        for (int dx = -radius; dx <= radius; ++dx) {
            // Round the clearing: a square of mown grass in the middle of a
            // forest is the tell of a generator, not a place.
            if (std::abs(dx) == radius && std::abs(dy) == radius) continue;
            int x = s.tx + dx, y = s.ty + dy;
            if (!inRegion(x, y)) continue;
            int id = ridx(x, y);
            OwTileKind k = static_cast<OwTileKind>(R.kind[id]);
            if (!isLandKind(k)) {
                // A town may stand on its own harbour headland; a cave does not
                // get to fill the sea.
                if (!settlement) continue;
            }
            R.kind[id] = static_cast<std::uint8_t>(ground);
            // Keep the road: a country road should run into a town gate, not
            // stop in the grass two tiles short of it. Rivers still yield.
            R.river[id] = 0;
        }
}

void ensurePadIsland(Region& R, int cx, int cy, OwClimate climate) {
    // Every site must be standable, including caves on a crag's shoulder.
    for (int dy = -2; dy <= 2; ++dy)
        for (int dx = -2; dx <= 2; ++dx) {
            int x = cx + dx, y = cy + dy;
            if (!inRegion(x, y)) continue;
            OwTileKind k = static_cast<OwTileKind>(R.kind[ridx(x, y)]);
            if (isMountainKind(k) || isWaterKind(k))
                R.kind[ridx(x, y)] = static_cast<std::uint8_t>(plainForClimate(climate));
        }
}

// ---------------------------------------------------------------- roads
struct RoadPath {
    std::vector<std::pair<int, int>> pts;
    bool ok = false;
};

int roadCost(const Region& R, int lx, int ly, bool allowSea) {
    int id = ridx(lx, ly);
    OwTileKind k = static_cast<OwTileKind>(R.kind[id]);
    if (R.road[id]) return 1;
    if (R.river[id]) return allowSea ? 40 : 55;
    switch (k) {
        case OWT_DEEP_SEA: return allowSea ? 400 : 1200;
        case OWT_SEA:      return allowSea ? 120 : 1200;
        case OWT_BRIDGE_H:
        case OWT_BRIDGE_V: return 1;
        case OWT_PASS_H:
        case OWT_PASS_V:   return 1;
        case OWT_MOUNTAIN:
        case OWT_MOUNTAIN_SNOW: return 150;
        case OWT_SWAMP:    return 14;
        case OWT_FOREST_LEAF:
        case OWT_FOREST_PINE:
        case OWT_FOREST_SNOW: return 6;
        case OWT_SAND:
        case OWT_SNOW:     return 3;
        default:
            // A little per-tile friction makes A* draw a winding country road
            // instead of a ruler line between towns.
            return 2 + static_cast<int>(owHash2(lx, ly, 0x5A17u) & 3u);
    }
}

// A* on the 320×320 continent. 4-neighbour, because roads are drawn from
// cardinal tiles; the result zig-zags like a hand-drawn SNES road.
RoadPath routeRoad(const Region& R, int ax, int ay, int bx, int by, bool allowSea) {
    RoadPath path;
    if (!inRegion(ax, ay) || !inRegion(bx, by)) return path;
    if (ax == bx && ay == by) {
        path.ok = true;
        path.pts.push_back({ax, ay});
        return path;
    }

    // Direction-aware A*: a small penalty per turn turns the old one-tile
    // staircase into long country runs with occasional bends. Tiles themselves
    // are still 4-connected, so the road stamp never has to interpolate.
    // State = (tile, incoming direction), direction 4 meaning "no direction yet".
    static const int TURN_PENALTY = 2;
    std::vector<int> dist(static_cast<std::size_t>(N) * 5, INT_MAX);
    std::vector<int> prev(static_cast<std::size_t>(N) * 5, -1);
    using Item = std::pair<int, int>;
    std::priority_queue<Item, std::vector<Item>, std::greater<Item>> open;
    auto heuristic = [&](int x, int y) { return (std::abs(x - bx) + std::abs(y - by)) * 2; };

    int startId = ridx(ax, ay);
    int goalId = ridx(bx, by);
    int startState = startId * 5 + 4;
    dist[startState] = 0;
    open.push({heuristic(ax, ay), startState});

    static const int dx4[4] = {1, -1, 0, 0};
    static const int dy4[4] = {0, 0, 1, -1};
    int expanded = 0;
    while (!open.empty() && expanded < 300000) {
        auto [f, sid] = open.top();
        open.pop();
        (void)f;
        int id = sid / 5;
        int dir0 = sid % 5;
        if (id == goalId) break;
        int y = id / REGION - HALF;
        int x = id % REGION - HALF;
        int d0 = dist[sid];
        if (d0 == INT_MAX) continue;
        ++expanded;
        for (int dir = 0; dir < 4; ++dir) {
            int nx = x + dx4[dir], ny = y + dy4[dir];
            if (!inRegion(nx, ny)) continue;
            int ni = ridx(nx, ny);
            int step = roadCost(R, nx, ny, allowSea);
            if (step >= 1200) continue;
            int nd = d0 + step;
            if (dir0 != 4 && dir0 != dir) nd += TURN_PENALTY;
            int nsi = ni * 5 + dir;
            if (nd >= dist[nsi]) continue;
            dist[nsi] = nd;
            prev[nsi] = sid;
            open.push({nd + heuristic(nx, ny), nsi});
        }
    }

    int goalState = -1, best = INT_MAX;
    for (int dir = 0; dir < 5; ++dir)
        if (dist[goalId * 5 + dir] < best) { best = dist[goalId * 5 + dir]; goalState = goalId * 5 + dir; }
    if (goalState < 0) return path;

    for (int sid = goalState; sid != -1; sid = prev[sid]) {
        int id = sid / 5;
        int y = id / REGION - HALF;
        int x = id % REGION - HALF;
        path.pts.push_back({x, y});
        if (sid == startState) break;
    }
    std::reverse(path.pts.begin(), path.pts.end());
    path.ok = true;
    return path;
}

// Route through two seeded, off-axis waypoints so a country road bends around
// the land instead of ruling a straight line between two towns. The A* legs are
// short; the road that comes out of it looks walked, not drawn with a ruler.
RoadPath routeMeander(const Region& R, int ax, int ay, int bx, int by, std::uint32_t salt) {
    RoadPath direct = routeRoad(R, ax, ay, bx, by, false);
    if (!direct.ok) return direct;

    float dx = static_cast<float>(bx - ax), dy = static_cast<float>(by - ay);
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 22.0f) return direct;

    std::uint32_t h = owHash2(ax * 7 + bx, ay * 11 + by, salt ^ 0x51DEu);
    float spread = std::min(15.0f, len * 0.22f);
    float o1 = (static_cast<float>((h & 31u)) / 31.0f * 2.0f - 1.0f) * spread;
    float o2 = (static_cast<float>(((h >> 8) & 31u)) / 31.0f * 2.0f - 1.0f) * spread;
    float px = -dy / len, py = dx / len;

    int w1x = static_cast<int>(std::round(ax + dx * 0.34f + px * o1));
    int w1y = static_cast<int>(std::round(ay + dy * 0.34f + py * o1));
    int w2x = static_cast<int>(std::round(ax + dx * 0.68f + px * o2));
    int w2y = static_cast<int>(std::round(ay + dy * 0.68f + py * o2));

    int xs[4] = {ax, w1x, w2x, bx};
    int ys[4] = {ay, w1y, w2y, by};
    RoadPath out;
    for (int leg = 0; leg < 3; ++leg) {
        RoadPath part = routeRoad(R, xs[leg], ys[leg], xs[leg + 1], ys[leg + 1], false);
        if (!part.ok) return direct;
        std::size_t first = out.pts.empty() ? 0 : 1;
        out.pts.insert(out.pts.end(), part.pts.begin() + static_cast<long>(first), part.pts.end());
    }
    out.ok = true;
    return out;
}

void stampRoad(Region& R, const RoadPath& path, OwClimate climate) {
    if (!path.ok) return;
    for (std::size_t i = 0; i < path.pts.size(); ++i) {
        int x = path.pts[i].first, y = path.pts[i].second;
        if (!inRegion(x, y)) continue;
        int id = ridx(x, y);
        OwTileKind k = static_cast<OwTileKind>(R.kind[id]);

        bool horizontal = true;
        if (i + 1 < path.pts.size()) horizontal = (path.pts[i + 1].second == y);
        else if (i > 0) horizontal = (path.pts[i - 1].second == y);

        if (isWaterKind(k)) {
            R.kind[id] = static_cast<std::uint8_t>(horizontal ? OWT_BRIDGE_H : OWT_BRIDGE_V);
            R.river[id] = 0;
        } else if (isMountainKind(k)) {
            R.kind[id] = static_cast<std::uint8_t>(horizontal ? OWT_PASS_H : OWT_PASS_V);
        } else {
            if (k == OWT_FOREST_LEAF || k == OWT_FOREST_PINE || k == OWT_FOREST_SNOW ||
                k == OWT_SWAMP) {
                R.kind[id] = static_cast<std::uint8_t>(
                    climate == OWC_SNOW ? OWT_SNOW : (climate == OWC_SAND ? OWT_SAND : OWT_GRASS));
            }
        }
        R.road[id] = 1;
    }
}

// Walkability for the post-road reachability audit: water and bare mountains
// and bridges count as walkable (a bridge is a road; a pass is a road).
bool footPassable(const Region& R, int lx, int ly) {
    if (!inRegion(lx, ly)) return false;
    int id = ridx(lx, ly);
    if (R.road[id]) return true;
    OwTileKind k = static_cast<OwTileKind>(R.kind[id]);
    return isLandKind(k) && !isMountainKind(k);
}

bool regionBfsReachable(const Region& R, int sx, int sy, int gx, int gy,
                        std::vector<std::uint8_t>* outVisited = nullptr) {
    if (!inRegion(sx, sy) || !inRegion(gx, gy)) return false;
    std::vector<std::uint8_t> vis(N, 0);
    std::queue<int> q;
    int s = ridx(sx, sy);
    vis[s] = 1;
    q.push(s);
    static const int dx4[4] = {1, -1, 0, 0};
    static const int dy4[4] = {0, 0, 1, -1};
    while (!q.empty()) {
        int id = q.front();
        q.pop();
        int y = id / REGION - HALF;
        int x = id % REGION - HALF;
        if (x == gx && y == gy) {
            if (outVisited) *outVisited = std::move(vis);
            return true;
        }
        for (int d = 0; d < 4; ++d) {
            int nx = x + dx4[d], ny = y + dy4[d];
            if (!footPassable(R, nx, ny)) continue;
            int ni = ridx(nx, ny);
            if (vis[ni]) continue;
            vis[ni] = 1;
            q.push(ni);
        }
    }
    if (outVisited) *outVisited = std::move(vis);
    return false;
}

// Minimum spanning tree over settlements (Prim, Manhattan metric), plus a spur
// from every dungeon to its nearest settlement and a spine from Elmswick to the
// story sites. Far nicer than the old every-cell-to-its-neighbour lattice.
struct Link {
    int a, b;
};

std::vector<Link> planLinks(const std::vector<OwSiteInfo>& sites, bool start) {
    std::vector<int> settlements;
    std::vector<int> dungeons;
    for (int i = 0; i < static_cast<int>(sites.size()); ++i) {
        if (owIsSettlement(sites[i].type)) settlements.push_back(i);
        else dungeons.push_back(i);
    }
    std::vector<Link> links;
    if (settlements.empty()) return links;

    auto dist2 = [&](int i, int j) {
        int dx = sites[i].tx - sites[j].tx;
        int dy = sites[i].ty - sites[j].ty;
        return dx * dx + dy * dy;
    };

    // Prim over the settlements.
    std::vector<std::uint8_t> inTree(sites.size(), 0);
    inTree[settlements[0]] = 1;
    int remaining = static_cast<int>(settlements.size()) - 1;
    while (remaining > 0) {
        int bestA = -1, bestB = -1;
        int bestD = INT_MAX;
        for (int a : settlements) {
            if (!inTree[a]) continue;
            for (int b : settlements) {
                if (inTree[b]) continue;
                int d = dist2(a, b);
                if (d < bestD) {
                    bestD = d;
                    bestA = a;
                    bestB = b;
                }
            }
        }
        if (bestA < 0) break;
        inTree[bestB] = 1;
        links.push_back({bestA, bestB});
        --remaining;
    }

    // Dungeons hang off the nearest settlement (or, for a lonely story site,
    // off the nearest other site).
    for (int d : dungeons) {
        int best = -1, bestD = INT_MAX;
        for (int s : settlements) {
            int dd = dist2(s, d);
            if (dd < bestD) {
                bestD = dd;
                best = s;
            }
        }
        if (best >= 0) links.push_back({best, d});
    }

    (void)start;
    return links;
}

// ---------------------------------------------------------------- naming
std::string continentNameFor(std::uint32_t seed, int rx, int ry) {
    static const char* kNames[] = {
        "The Elmlands", "Alderreach", "The Crownlands", "Silvervale",
        "Briarwold", "Ashfen", "Highmeadow", "The Amber Waste",
        "Frostmarch", "The Golden Flats", "Westweald", "The Sunken Coast",
        "Copper Reach", "The White Barrens", "Mossgate", "Dunmoor",
    };
    if (rx == 0 && ry == 0) return kNames[0];
    std::uint32_t h = owHash2(rx, ry, seed ^ 0x2E610Bu);
    return kNames[h % (sizeof(kNames) / sizeof(kNames[0]))];
}

std::string provinceNameFor(std::uint32_t seed, int rx, int ry, int index, OwClimate climate) {
    static const char* kTemp[] = {
        "Millbrook Vale", "Greenmarch", "Alderwood", "Highmeadow",
        "Silvervale", "Westweald", "The Briars", "Fernhollow",
    };
    static const char* kSand[] = {
        "Sunscorch Flats", "Amber Wastes", "Dune Reach", "The Golden Flats",
        "Cactus Hollow", "Mirage Basin",
    };
    static const char* kSnow[] = {
        "Frostmarch", "Whitepeak", "Rimehold", "The Northern Tundra",
        "Glacier Vale", "Coldwater",
    };
    std::uint32_t h = owHash2(rx * 13 + index, ry * 7 - index, seed ^ 0x9117u);
    if (climate == OWC_SAND) return kSand[h % 6u];
    if (climate == OWC_SNOW) return kSnow[h % 6u];
    return kTemp[h % 8u];
}

} // namespace

// ================================================================ region cache
namespace {

struct RegionCache {
    std::uint32_t seed = 0xFFFFFFFFu;
    std::unordered_map<std::uint64_t, std::unique_ptr<Region>> regions;
    std::vector<std::uint64_t> order;      // LRU, oldest first
};

std::uint64_t packRegion(int rx, int ry) {
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(rx)) << 32) |
           static_cast<std::uint32_t>(ry);
}

RegionCache& cacheFor(std::uint32_t seed) {
    static RegionCache cache;
    if (cache.seed != seed) {
        cache.seed = seed;
        cache.regions.clear();
        cache.order.clear();
    }
    return cache;
}

void buildRegion(Region& R, std::uint32_t seed, int rx, int ry);

Region& getRegion(std::uint32_t seed, int rx, int ry) {
    RegionCache& c = cacheFor(seed);
    std::uint64_t key = packRegion(rx, ry);
    auto it = c.regions.find(key);
    if (it != c.regions.end()) return *it->second;

    auto region = std::make_unique<Region>();
    buildRegion(*region, seed, rx, ry);

    // Keep the six most recently used continents. Rebuilding is cheap and
    // deterministic, so eviction never changes the world.
    if (c.order.size() >= 6) {
        std::uint64_t old = c.order.front();
        c.order.erase(c.order.begin());
        c.regions.erase(old);
    }
    Region& ref = *region;
    c.regions[key] = std::move(region);
    c.order.push_back(key);
    return ref;
}

void buildRegion(Region& R, std::uint32_t seed, int rx, int ry) {
    R.rx = rx;
    R.ry = ry;
    R.name = continentNameFor(seed, rx, ry);
    R.road.assign(N, 0);
    R.river.assign(N, 0);
    R.sites.clear();
    R.provinces.clear();

    const bool start = (rx == 0 && ry == 0);
    const OwClimate climate = start ? OWC_TEMPERATE : continentClimate(seed, rx, ry);

    Rng rng(static_cast<std::uint64_t>(seed) * 0x9E3779B97F4A7C15ull ^
            static_cast<std::uint32_t>(packRegion(rx, ry)) ^ 0xD15EA5EDull);

    auto t0 = std::chrono::steady_clock::now();
    buildLand(R, rng, start);
    auto t1 = std::chrono::steady_clock::now();
    std::vector<std::uint8_t> mainLand = largestLandComponent(R);
    auto t2 = std::chrono::steady_clock::now();
    buildMountains(R, rng, start, climate == OWC_SNOW);
    buildRivers(R, rng, start, {}, climate);
    auto t3 = std::chrono::steady_clock::now();
    paintBiomes(R, rng, start, climate);
    auto t4 = std::chrono::steady_clock::now();
    if (std::getenv("APFA_TIME"))
        std::fprintf(stderr, "[world] %dx%d land %lld comp %lld crag/river %lld biome %lld ms\n", rx, ry,
            (long long)std::chrono::duration_cast<std::chrono::milliseconds>(t1-t0).count(),
            (long long)std::chrono::duration_cast<std::chrono::milliseconds>(t2-t1).count(),
            (long long)std::chrono::duration_cast<std::chrono::milliseconds>(t3-t2).count(),
            (long long)std::chrono::duration_cast<std::chrono::milliseconds>(t4-t3).count());

    // ---- sites
    std::vector<SitePlan> plans;
    if (start) {
        planStartSites(R, seed, rng, plans);
        carveTideIslands(R, seed, rng, plans);
        // The isles just cleared a hole in the sea; re-read the land so a
        // generic site cannot be planned onto what is now open water.
        mainLand = largestLandComponent(R);
        planGenericSites(R, rng, OWC_TEMPERATE, mainLand, plans, 0.45f);
    } else {
        planGenericSites(R, rng, climate, mainLand, plans);
    }

    // Drop plans that landed outside the main component (a dungeon chosen on a
    // tide islet would be unreachable and the quest compass would lie).
    std::vector<SitePlan> kept;
    for (const SitePlan& p : plans) {
        if (!p.island) {
            int id = ridx(p.x, p.y);
            if (!inRegion(p.x, p.y)) continue;
            if (!mainLand[id]) continue;
        }
        kept.push_back(p);
    }
    plans.swap(kept);

    // ---- pack into OwSiteInfo, clear pads
    int salt = 0;
    for (const SitePlan& p : plans) {
        if (!inRegion(p.x, p.y)) continue;
        ensurePadIsland(R, p.x, p.y, p.island ? OWC_TEMPERATE : climate);

        OwSiteInfo s;
        s.type = p.type;
        s.climate = p.island ? OWC_TEMPERATE : climate;
        s.tx = p.x;
        s.ty = p.y;
        s.cellX = floorDiv(p.x, OW_CELL_SIZE);
        s.cellY = floorDiv(p.y, OW_CELL_SIZE);
        s.seed = p.seed;
        s.gateMask = owIsSettlement(p.type) ? 0x0F : 0;
        s.tideIslandIdx = p.islandIdx;
        s.name = p.fixedName ? p.fixedName
                             : uniqueSettlementName(seed, p.x + rx * REGION, p.y + ry * REGION,
                                                    p.type, s.climate, salt);
        if (p.type == OWS_CAVE && s.name.empty()) s.name = "Cavern";
        if (start) {
            if (p.fixedName && std::string(p.fixedName) == "Elmswick") s.isStartTown = true;
            if (p.fixedName && std::string(p.fixedName) == "Hollow Hill") s.isStartCavern = true;
            if (p.fixedName && std::string(p.fixedName) == "Castle of Elm") s.isBossCastle = true;
            if (p.fixedName && std::string(p.fixedName) == "Standing Stones") s.isLocketSite = true;
            if (p.fixedName && std::string(p.fixedName) == "Old Watchtower") s.isTowerSite = true;
        }
        clearSitePad(R, s, s.climate);
        R.sites.push_back(s);
        ++salt;
    }

    // ---- roads
    auto tRoads0 = std::chrono::steady_clock::now();
    std::vector<Link> links = planLinks(R.sites, start);
    int linkIndex = 0;
    for (const Link& link : links) {
        const OwSiteInfo& a = R.sites[link.a];
        const OwSiteInfo& b = R.sites[link.b];
        ++linkIndex;
        if (a.tideIslandIdx >= 0 || b.tideIslandIdx >= 0) continue;
        RoadPath path = routeMeander(R, a.tx, a.ty, b.tx, b.ty,
                                     static_cast<std::uint32_t>(linkIndex) * 0x9E3779B9u);
        if (path.ok) stampRoad(R, path, climate);
    }

    if (std::getenv("APFA_TIME")) {
        int riv = 0, br = 0, ps = 0, mt = 0, rd = 0;
        for (int i = 0; i < N; ++i) {
            if (R.river[i]) ++riv;
            if (R.road[i]) ++rd;
            OwTileKind k = static_cast<OwTileKind>(R.kind[i]);
            if (k == OWT_BRIDGE_H || k == OWT_BRIDGE_V) ++br;
            if (k == OWT_PASS_H || k == OWT_PASS_V) ++ps;
            if (isMountainKind(k)) ++mt;
        }
        std::fprintf(stderr, "[world] %dx%d river=%d bridge=%d pass=%d mtn=%d road=%d\n",
                     rx, ry, riv, br, ps, mt, rd);
    }
    if (std::getenv("APFA_TIME"))
        std::fprintf(stderr, "[world] roads %dx%d: %lld ms (%zu sites, %zu links)\n", rx, ry,
                     (long long)std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::steady_clock::now() - tRoads0).count(),
                     R.sites.size(), links.size());
    // ---- repair: guarantee every mainland site can be walked to from the
    // first settlement. A river with no bridge is a bug, not a feature.
    int hub = -1;
    for (int i = 0; i < static_cast<int>(R.sites.size()); ++i)
        if (owIsSettlement(R.sites[i].type) && R.sites[i].tideIslandIdx < 0) { hub = i; break; }
    if (hub >= 0) {
        for (int i = 0; i < static_cast<int>(R.sites.size()); ++i) {
            const OwSiteInfo& s = R.sites[i];
            if (i == hub || s.tideIslandIdx >= 0) continue;
            if (regionBfsReachable(R, R.sites[hub].tx, R.sites[hub].ty, s.tx, s.ty)) continue;
            RoadPath path = routeRoad(R, R.sites[hub].tx, R.sites[hub].ty, s.tx, s.ty, true);
            stampRoad(R, path, climate);
        }
    }

    // ---- site centers clean again (a repair road may have run over one)
    for (const OwSiteInfo& s : R.sites) clearSitePad(R, s, s.climate);

    // Everything above is region-local (pads, roads and BFS all want a 320×320
    // grid); only now do the sites become world coordinates.
    for (OwSiteInfo& s : R.sites) {
        s.tx += rx * REGION;
        s.ty += ry * REGION;
        s.cellX = floorDiv(s.tx, OW_CELL_SIZE);
        s.cellY = floorDiv(s.ty, OW_CELL_SIZE);
    }

    // ---- provinces (named countries inside the continent)
    const int provinceCount = start ? 4 : 3;
    for (int i = 0; i < provinceCount; ++i) {
        Province p;
        p.x = static_cast<int>(std::round(rng.frange(-70.0f, 70.0f)));
        p.y = static_cast<int>(std::round(rng.frange(-70.0f, 70.0f)));
        p.name = provinceNameFor(seed, rx, ry, i, climate);
        R.provinces.push_back(p);
    }
    if (start) {
        // The vale is the one place whose name the player must recognise.
        if (!R.provinces.empty()) {
            R.provinces[0].x = 0;
            R.provinces[0].y = 0;
            R.provinces[0].name = "Elmswick Vale";
        }
    }
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
    return smoothNoise(x, y, seed);
}

float owFbm(float x, float y, std::uint32_t seed, int octaves) {
    return fbm(x, y, seed, octaves);
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
    int rx = floorDiv(tx + HALF, REGION);
    int ry = floorDiv(ty + HALF, REGION);
    int lx = tx - rx * REGION;
    int ly = ty - ry * REGION;
    const Region& R = getRegion(seed, rx, ry);

    OwCellTile t;
    int id = ridx(lx, ly);
    t.kind = static_cast<OwTileKind>(R.kind[id]);
    t.hasRoad = R.road[id] != 0;
    t.variant = static_cast<std::uint8_t>(owHash2(tx, ty, seed ^ 0x9933u) & 7u);
    return t;
}

void owSitesInCell(std::uint32_t seed, int cellX, int cellY, std::vector<OwSiteInfo>& out) {
    int x0 = cellX * OW_CELL_SIZE;
    int y0 = cellY * OW_CELL_SIZE;
    std::vector<OwSiteInfo> tmp;
    owSitesInRect(seed, x0, y0, x0 + OW_CELL_SIZE - 1, y0 + OW_CELL_SIZE - 1, tmp);
    for (const auto& s : tmp)
        if (s.tx >= x0 && s.tx < x0 + OW_CELL_SIZE && s.ty >= y0 && s.ty < y0 + OW_CELL_SIZE)
            out.push_back(s);
}

void owSitesInRect(std::uint32_t seed, int x0, int y0, int x1, int y1, std::vector<OwSiteInfo>& out) {
    out.clear();
    if (x0 > x1) std::swap(x0, x1);
    if (y0 > y1) std::swap(y0, y1);
    const int pad = 3;

    int rx0 = floorDiv(x0 - pad + HALF, REGION);
    int rx1 = floorDiv(x1 + pad + HALF, REGION);
    int ry0 = floorDiv(y0 - pad + HALF, REGION);
    int ry1 = floorDiv(y1 + pad + HALF, REGION);

    for (int ry = ry0; ry <= ry1; ++ry) {
        for (int rx = rx0; rx <= rx1; ++rx) {
            const Region& R = getRegion(seed, rx, ry);
            for (const auto& s : R.sites) {
                if (s.tx >= x0 - pad && s.tx <= x1 + pad &&
                    s.ty >= y0 - pad && s.ty <= y1 + pad)
                    out.push_back(s);
            }
        }
    }
}

bool owSiteAtTile(std::uint32_t seed, int tx, int ty, OwSiteInfo& out) {
    int rx = floorDiv(tx + HALF, REGION);
    int ry = floorDiv(ty + HALF, REGION);
    const Region& R = getRegion(seed, rx, ry);
    for (const auto& s : R.sites) {
        if (s.tx == tx && s.ty == ty) {
            out = s;
            return true;
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
            "West", "Gold", "Fern", "Ash", "River", "Brook", "Willow", "Thorn",
            "Moss", "Stone", "Wind", "Hazel", "Crow", "Dun", "Marrow", "Ember"
        };
        static const char* kPreSand[] = {
            "Sun", "Dune", "Amber", "Gold", "Red", "Cinder", "Mirage", "Oasis",
            "Copper", "Saffron", "Salt", "Sand", "Pale", "Dust",
        };
        static const char* kPreSnow[] = {
            "Frost", "Snow", "Ice", "Pine", "Winter", "White", "North", "Silver",
            "Rime", "Glacier", "Storm", "Cold", "Hollow", "Grey",
        };
        static const char* kSufHamlet[] = {"ford", "wick", "brook", "glen", "stead", "well", "mere", "croft"};
        static const char* kSufTown[]   = {"haven", "bridge", "field", "wood", "port", "ridge", "vale", "holm"};
        static const char* kSufCity[]   = {"hold", "spire", "keep", "crown", "gate", "guard", "crest", "bastion"};

        const char* pre = kPreTemp[h % 24u];
        if (climate == OWC_SAND) pre = kPreSand[h % 14u];
        else if (climate == OWC_SNOW) pre = kPreSnow[h % 14u];

        const char* suf = (type == OWS_CITY) ? kSufCity[(h >> 8) % 8u]
                        : (type == OWS_TOWN) ? kSufTown[(h >> 8) % 8u]
                                             : kSufHamlet[(h >> 8) % 8u];
        return std::string(pre) + suf;
    }

    static const char* kAdj[] = {
        "Gloam", "Whispering", "Sunken", "Silent", "Iron", "Ashen",
        "Verdant", "Ancient", "Frost", "Shattered", "Dusk", "Starlit",
        "Weeping", "Hollow", "Bleak", "Gilded"
    };
    const char* adj = kAdj[h % 16u];
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
    if (std::abs(tx) <= 18 && std::abs(ty) <= 16) return "Elmswick Vale";
    int rx = floorDiv(tx + HALF, REGION);
    int ry = floorDiv(ty + HALF, REGION);
    const Region& R = getRegion(seed, rx, ry);
    if (R.provinces.empty()) return R.name;
    int lx = tx - rx * REGION, ly = ty - ry * REGION;
    int best = 0;
    int bestD = INT_MAX;
    for (int i = 0; i < static_cast<int>(R.provinces.size()); ++i) {
        int dx = R.provinces[i].x - lx, dy = R.provinces[i].y - ly;
        int d = dx * dx + dy * dy;
        if (d < bestD) {
            bestD = d;
            best = i;
        }
    }
    return R.provinces[best].name;
}

std::string owContinentName(std::uint32_t seed, int tx, int ty) {
    int rx = floorDiv(tx + HALF, REGION);
    int ry = floorDiv(ty + HALF, REGION);
    return getRegion(seed, rx, ry).name;
}

int owDangerTier(int tx, int ty) {
    // A smooth swell away from Elmswick, not a jump at each atlas border: the
    // party can walk or sail from one cell to the next, so the difficulty
    // curve must not step there.
    float d = std::sqrt(static_cast<float>(tx) * static_cast<float>(tx) +
                        static_cast<float>(ty) * static_cast<float>(ty));
    int tier = 1 + static_cast<int>(d / 105.0f);
    if (tier < 1) tier = 1;
    if (tier > 8) tier = 8;
    return tier;
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
    // The actual instance lives in the generated continent; these accessors
    // exist so quest text, the compass and the verifier agree with it.
    std::vector<OwSiteInfo> list;
    owSitesInRect(seed, -64, -64, 64, 64, list);
    for (const auto& s : list)
        if (s.isStartCavern) return s;
    for (const auto& s : list)
        if (s.type == OWS_CAVE) return s;
    OwSiteInfo s;
    s.type = OWS_CAVE;
    s.tx = -18;
    s.ty = -16;
    s.seed = owHash2(-18, -16, seed ^ 0xCA7E01u);
    s.isStartCavern = true;
    s.name = "Hollow Hill";
    s.cellX = floorDiv(s.tx, OW_CELL_SIZE);
    s.cellY = floorDiv(s.ty, OW_CELL_SIZE);
    return s;
}

OwSiteInfo owBossCastle(std::uint32_t seed) {
    std::vector<OwSiteInfo> list;
    owSitesInRect(seed, -80, -80, 80, 80, list);
    for (const auto& s : list)
        if (s.isBossCastle) return s;
    OwSiteInfo s;
    s.type = OWS_CASTLE;
    s.tx = 18;
    s.ty = -54;
    s.seed = owHash2(18, -54, seed ^ 0xCA571Eu);
    s.isBossCastle = true;
    s.name = "Castle of Elm";
    s.cellX = floorDiv(s.tx, OW_CELL_SIZE);
    s.cellY = floorDiv(s.ty, OW_CELL_SIZE);
    return s;
}

OwSiteInfo owLocketSite(std::uint32_t seed) {
    std::vector<OwSiteInfo> list;
    owSitesInRect(seed, -80, -30, 0, 60, list);
    for (const auto& s : list)
        if (s.isLocketSite) return s;
    OwSiteInfo s;
    s.type = OWS_RUINS;
    s.tx = -44;
    s.ty = 16;
    s.seed = owHash2(-44, 16, seed ^ 0x10C4E7u);
    s.isLocketSite = true;
    s.name = "Standing Stones";
    s.cellX = floorDiv(s.tx, OW_CELL_SIZE);
    s.cellY = floorDiv(s.ty, OW_CELL_SIZE);
    return s;
}

OwSiteInfo owTowerSite(std::uint32_t seed) {
    std::vector<OwSiteInfo> list;
    owSitesInRect(seed, -80, -60, 0, 0, list);
    for (const auto& s : list)
        if (s.isTowerSite) return s;
    OwSiteInfo s;
    s.type = OWS_TOWER;
    s.tx = -36;
    s.ty = -30;
    s.seed = owHash2(-36, -30, seed ^ 0x70AE21u);
    s.isTowerSite = true;
    s.name = "Old Watchtower";
    s.cellX = floorDiv(s.tx, OW_CELL_SIZE);
    s.cellY = floorDiv(s.ty, OW_CELL_SIZE);
    return s;
}

OwSiteInfo owTideIsland(std::uint32_t seed, int idx) {
    int i = std::max(0, std::min(2, idx));
    std::vector<OwSiteInfo> list;
    owSitesInRect(seed, -100, 60, 100, 130, list);
    for (const auto& s : list)
        if (s.tideIslandIdx == i) return s;

    static const char* kNames[3] = {"Tidewell Cay", "Lantern Shoal", "Saltspire Isle"};
    static const int kBaseX[3] = {-58, 6, 66};
    static const int kBaseY[3] = {96, 108, 88};
    OwSiteInfo s;
    s.type = OWS_CAMP;
    s.climate = OWC_TEMPERATE;
    s.tx = kBaseX[i];
    s.ty = kBaseY[i];
    s.seed = owHash2(s.tx, s.ty, seed ^ 0x71DE00u);
    s.tideIslandIdx = i;
    s.name = kNames[i];
    s.cellX = floorDiv(s.tx, OW_CELL_SIZE);
    s.cellY = floorDiv(s.ty, OW_CELL_SIZE);
    return s;
}

} // namespace apfa
