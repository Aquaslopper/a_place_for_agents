// field.cpp — noise, biomes, villages, props, npcs, chests, landmarks, A*
#include "field.hpp"
#include "pieces.hpp"
#include "atlas.hpp"
#include "commons.hpp"
#include "regions.hpp"
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <queue>
#include <limits>
#include <string>
#include <algorithm>
#include <set>
#include <deque>

// ================================================================ hashing
uint32_t hash2i(int x, int y, uint32_t seed) {
    uint32_t h = seed
        ^ (uint32_t)x * 2654435761u
        ^ (uint32_t)y * 2246822519u
        ^ ((uint32_t)x * 374761393u + (uint32_t)y * 668265263u) * 3266489917u;
    h ^= h >> 16; h *= 0x7feb352dU; h ^= h >> 15; h *= 0x846ca68bU; h ^= h >> 16;
    return h;
}
float hashf(int x, int y, uint32_t seed) {
    return (float)hash2i(x, y, seed) / 4294967295.0f;
}
static float fade(float t) { return t * t * (3.0f - 2.0f * t); }
float smoothNoise(float x, float y, uint32_t seed) {
    int xi = (int)floorf(x), yi = (int)floorf(y);
    float xf = x - xi, yf = y - yi;
    float v00 = hashf(xi,     yi,     seed);
    float v10 = hashf(xi + 1, yi,     seed);
    float v01 = hashf(xi,     yi + 1, seed);
    float v11 = hashf(xi + 1, yi + 1, seed);
    float u = fade(xf), v = fade(yf);
    return v00 + (v10 - v00) * u + (v01 - v00) * v + (v00 - v10 - v01 + v11) * u * v;
}
float fbm(float x, float y, uint32_t seed, int octaves) {
    float amp = 0.5f, freq = 1.0f, sum = 0.0f, norm = 0.0f;
    for (int i = 0; i < octaves; i++) {
        sum += smoothNoise(x * freq, y * freq, seed + (uint32_t)i * 1013u) * amp;
        norm += amp;
        amp *= 0.5f; freq *= 2.0f;
    }
    return sum / norm;
}

// ================================================================ terrain
// Wild ways (defined after cellWalkable): the endless lattice of crossings that
// keeps the terrain from fencing the world into rooms.
// The decided way cells for the current seed, and which sectors have been
// surveyed. Both are process state, pure functions of the seed, so a reloaded
// save and a fresh walk always agree on where the ways run.
static std::unordered_set<uint64_t> gWayCells;
static std::unordered_set<int64_t> gWaySectorsDone;
static uint32_t                    gWaySeed = 0;
static uint32_t                    gWayEpoch = 1;   // bumped whenever ways are added

static inline uint64_t wayKey(int tx, int ty) {
    return ((uint64_t)(uint32_t)tx << 32) | (uint32_t)ty;
}
// A sector's identity. Exact, not a hash: a collision here would silently skip
// a survey and leave its dell sealed, which is the whole bug this fixes.
static inline int64_t waySectorIndex(int sx, int sy) {
    return ((int64_t)(sx + 100000) << 32) | (uint32_t)(sy + 100000);
}

static bool onWildWay(int tx, int ty);
static GroundKind wayGroundKind(int tx, int ty, GroundKind natural);

// ================================================================ the coast
// The world used to be endless in every direction: noise all the way out, so
// a player who walked north for half an hour met nothing but more of the same
// forest. An SNES world map is finite and you can see its edge. This is ours:
// one shore per seed, a ring whose radius breathes with the seed and a little
// noise, and past it the sea. Land keeps its shape inside the ring and simply
// falls away outside it, so every biome, road and village the Survey placed
// still sits where it always sat — the continent just stops.
//
// The far coast is deliberately unreachable on foot: the water there is deep,
// and deep water refuses a hull. The only way off the continent is the ferry
// (src/sail.cpp), and the only destinations are its islands.
constexpr float SHORE_BASE_R = 286.0f;   // tiles from the well to the mean shoreline
constexpr float SHORE_BREATH = 34.0f;    // ± how far the seed moves the shore
constexpr float SHORE_WOBBLE = 19.0f;    // ± how far the coast meanders within a seed

float shoreRadius() {
    static uint32_t cachedSeed = ~0u;
    static float cached = SHORE_BASE_R;
    if (cachedSeed != g.worldSeed) {
        cachedSeed = g.worldSeed;
        cached = SHORE_BASE_R + ((float)(hash2i(7, 11, g.worldSeed) % 1000u) / 1000.0f - 0.5f) * 2.0f * SHORE_BREATH;
    }
    return cached;
}

// 0 in the middle of the continent, 1 at the mean shoreline, >1 at sea.
// The wobble is angular so the coast reads as bays and headlands, not a circle.
static float shoreT(float x, float y) {
    double d = sqrt((double)x * (double)x + (double)y * (double)y);
    double ang = atan2((double)y, (double)x);
    float wob = smoothNoise((float)cos(ang) * 2.3f + 17.0f, (float)sin(ang) * 2.3f - 9.0f,
                            g.worldSeed ^ 0x5401u);
    float reach = shoreRadius() + (wob - 0.5f) * 2.0f * SHORE_WOBBLE;
    if (!(reach > 40.0f)) reach = 40.0f;
    float r = (float)(d / (double)reach);
    return r;
}

// The authored places near the well keep their ground exactly as it was.
static bool nearHomeSites(float x, float y) {
    return (x * x + y * y) < 30.0f * 30.0f ||
           (fabsf(x - (float)BOSS_CASTLE_TX) < 24.0f && fabsf(y - (float)BOSS_CASTLE_TY) < 45.0f) ||
           (fabsf(x - (float)LOCKET_SITE_TX) < 16.0f && fabsf(y - (float)LOCKET_SITE_TY) < 16.0f) ||
           (fabsf(x - (float)TOWER_SITE_TX) < 16.0f && fabsf(y - (float)TOWER_SITE_TY) < 16.0f);
}

static float elevationAt(float x, float y, uint32_t seed) {
    float e = fbm(x * 0.016f, y * 0.016f, seed ^ 0x1eafu, 5);
    e = (e - 0.5f) * 1.4f + 0.5f;
    if (nearHomeSites(x, y)) return e;
    float t = shoreT(x, y);
    float shelf = 0.29f;                                  // dry land ends here
    if (t <= 1.0f) return e + (1.0f - t) * 0.0f;          // untouched inside the shore
    // past the mean shore the ground eases down to a beach, then into the sea
    float k = t < 1.035f ? (t - 1.0f) / 0.035f : 1.0f;
    float coast = e * (1.0f - k) + shelf * k;
    if (t > 1.035f) {
        float deepK = std::min(1.0f, (t - 1.035f) / 0.11f);
        coast = shelf * (1.0f - deepK) + 0.19f * deepK;
    }
    return coast;
}
static float temperatureAt(float x, float y, uint32_t seed) {
    float t = fbm(x * 0.010f + 313.1f, y * 0.010f + 47.9f, seed ^ 0x7e47u, 3);
    t += (y < 0 ? -0.05f : 0.04f);
    return t;
}
float moistureAt(float x, float y) {
    return fbm(x * 0.022f + 91.7f, y * 0.022f + 17.3f, g.worldSeed ^ 0x5eedu, 4);
}

// Domain-warped canopy density shared by ground shading AND tree grove placement
// so dark forest turf / fallen leaves sit directly beneath tree crowns.
static float canopyDensityAt(int tx, int ty, uint32_t seed) {
    float x = (float)tx, y = (float)ty;
    float wx = x + (smoothNoise(x * 0.052f, y * 0.052f, seed ^ 0x1234u) - 0.5f) * 7.5f;
    float wy = y + (smoothNoise(x * 0.052f + 41.3f, y * 0.052f - 19.7f, seed ^ 0x5678u) - 0.5f) * 7.5f;
    float g0 = smoothNoise(wx * 0.088f, wy * 0.088f, seed ^ 0x61a4u);
    float g1 = smoothNoise(wx * 0.195f, wy * 0.195f, seed ^ 0x9bcdu);
    float grove = g0 * 0.64f + g1 * 0.36f;
    // Meandering sunlit glade ribbons so deep woods always stay navigable and scenic
    float ribbon = fabsf(smoothNoise(wx * 0.044f + 73.1f, wy * 0.044f - 28.4f, seed ^ 0xbeefu) - 0.5f);
    if (ribbon < 0.075f) {
        float k = ribbon / 0.075f;
        grove *= (0.35f + 0.65f * k * k);
    }
    return grove;
}

static float brookCenterAt(float x, uint32_t seed) {
    int sector = (int)floorf((x + 32.0f) / 64.0f);
    int ford = sector * 64 + (sector ? (int)(hash2i(sector, 3, seed) % 19) - 9 : 0);
    float fx = ((int)floorf(x) == ford + 1) ? (float)ford : x;
    return 18.5f + 1.6f * sinf(fx * 0.13f) + std::min(1.0f, fabsf(fx) / 64.0f) *
           (smoothNoise(fx * 0.021f, 31.7f, seed ^ 0x7368u) - 0.5f) * 22.0f;
}

static void flattenSite(float x, float y, float cx, float cy, float innerR, float outerR,
                        float& e, float& t, float& m, float targetE, float targetT, float targetM) {
    float dx = x - cx, dy = y - cy;
    float d = sqrtf(dx * dx + dy * dy);
    if (d < outerR) {
        float k = (d <= innerR) ? 1.0f : 1.0f - (d - innerR) / (outerR - innerR);
        k = k * k * (3.0f - 2.0f * k);
        e = e * (1.0f - k) + targetE * k;
        t = t * (1.0f - k) + targetT * k;
        m = m * (1.0f - k) + targetM * k;
        if (d < innerR + 2.0f) {
            if (e < 0.47f) e = 0.47f;
            if (e > 0.62f) e = 0.62f;
        }
    }
}

// ================================================================ the millpond
// Elmswick's millpond is authored, not scavenged. It is one lake with a fixed
// centre, a meandering shore, one island in it and one jetty on the town bank.
// That matters for more than looks: the boat, the Tidewell quest and the
// Journal's sea all need a body of water that is *always* there, always wide
// enough to sail, and always surrounded by dry land — a noise pond can be a
// marsh one seed and a river the next, and then the ferryman sells a boat with
// nowhere to go.
// Centre and radii are chosen so the pond's north-east lobe reaches the
// Millbrook at the town's own bank: the landing is 16 tiles from the square,
// the pond runs away south-west from it, and the isles sit along that run. One
// body of water, one journey — if the lake and the landing were separate
// puddles the boat would be a toy.
constexpr float MILLPOND_CX = -40.0f;
constexpr float MILLPOND_CY = 40.0f;
constexpr float MILLPOND_RX = 33.0f;
constexpr float MILLPOND_RY = 27.0f;

// Three isles: one in the pond, two out in its bay, so the sail has a near
// destination and a far one. Radii are in tiles.
struct MillIsle { float x, y, r; };
static const MillIsle MILL_ISLES[TIDE_ISLANDS] = {
    { MILLPOND_CX - 1.0f,  MILLPOND_CY - 2.0f,  4.5f },  // the mill isle, west bay
    { MILLPOND_CX - 12.0f, MILLPOND_CY + 10.0f, 4.0f },  // Gull Rock, off the south shore
    { MILLPOND_CX + 10.0f, MILLPOND_CY + 11.0f, 4.0f },  // the Little Kettle, east bay
};

// 0 in the middle, 1 at the mean shore, >1 outside.
static float millpondT(float x, float y) {
    float dx = (x - MILLPOND_CX) / MILLPOND_RX;
    float dy = (y - MILLPOND_CY) / MILLPOND_RY;
    float d = sqrtf(dx * dx + dy * dy);
    float ang = atan2f(dy, dx);
    float wob = smoothNoise(cosf(ang) * 2.1f + 5.0f, sinf(ang) * 2.1f + 3.0f, g.worldSeed ^ 0x9d0du);
    return d / (0.88f + 0.22f * wob);
}

// The millrace. A mill pond is not a pond sunk in a field: it is water the
// brook was made to fill. This is the cut that does it — the Millbrook at the
// town's own landing, running south-west into the pond's north-east lobe. It is
// the reason the ferryman's boat is not moored in a puddle of its own.
// The landing pool: a widened berth at the town's own south bank, west of the
// bridge and clear of the South Highway. Without it the landing is wherever the
// seed's river noise happens to leave a puddle — and on some seeds that puddle
// is thirty tiles of water walled off by its own banks, so the ferryman rents
// out a boat that cannot reach the isles.
static float millPoolT(float x, float y) {
    float dx = (x + 6.0f) / 4.6f;
    float dy = (y - 15.0f) / 3.2f;
    float ang = atan2f(dy, dx);
    float wob = smoothNoise(cosf(ang) * 1.7f + 11.0f, sinf(ang) * 1.7f + 2.0f, g.worldSeed ^ 0x51c3u);
    return sqrtf(dx * dx + dy * dy) / (0.90f + 0.12f * wob);
}

static float millraceT(float x, float y) {
    const float ax = -9.0f, ay = 17.0f;    // out of the landing pool
    const float bx = -21.0f, by = 25.5f;   // spill into the pond
    float vx = bx - ax, vy = by - ay;
    float t = ((x - ax) * vx + (y - ay) * vy) / (vx * vx + vy * vy);
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    float px = x - (ax + vx * t), py = y - (ay + vy * t);
    float d = sqrtf(px * px + py * py);
    float w = 2.45f + 0.45f * smoothNoise(x * 0.19f + 3.1f, y * 0.19f - 8.2f, g.worldSeed ^ 0x2ee1u);
    return d / w;
}

// Everywhere the mill's water actually reaches.
static float pondWaterT(float x, float y) {
    return std::min(millpondT(x, y), std::min(millraceT(x, y), millPoolT(x, y)));
}

static bool onMillIsle(int tx, int ty, float pad = 0.0f) {
    for (int i = 0; i < TIDE_ISLANDS; i++) {
        float dx = (float)tx - MILL_ISLES[i].x, dy = (float)ty - MILL_ISLES[i].y;
        float r = MILL_ISLES[i].r + pad;
        if (dx * dx + dy * dy <= r * r) return true;
    }
    return false;
}

static void flattenSpecialSites(float x, float y, float& e, float& t, float& m) {
    // The millpond: water out to its meandering shore, meadow banks.
    {
        float mp = pondWaterT(x, y);
        if (mp < 1.35f) {
            float k = mp < 1.0f ? 1.0f : 1.0f - (mp - 1.0f) / 0.35f;
            k = k * k * (3.0f - 2.0f * k);
            float bed = onMillIsle((int)x, (int)y) ? 0.50f : (mp < 0.72f ? 0.20f : 0.24f);
            e = e * (1.0f - k) + bed * k;
            if (k > 0.5f) { t = t * (1.0f - k) + 0.54f * k; m = m * (1.0f - k) + 0.52f * k; }
        }
        // The isles themselves rise out of the water as meadow.
        for (int i = 0; i < TIDE_ISLANDS; i++) {
            float dx = x - MILL_ISLES[i].x, dy = y - MILL_ISLES[i].y;
            float d = sqrtf(dx * dx + dy * dy);
            if (d < MILL_ISLES[i].r + 3.0f) {
                float k = d <= MILL_ISLES[i].r ? 1.0f : 1.0f - (d - MILL_ISLES[i].r) / 3.0f;
                k = k * k * (3.0f - 2.0f * k);
                e = e * (1.0f - k) + 0.52f * k;
            }
        }
    }
    flattenSite(x, y, 0.0f, 0.0f, 22.0f, 30.0f, e, t, m, 0.52f, 0.55f, 0.58f);
    flattenSite(x, y, (float)BOSS_CASTLE_TX, (float)BOSS_CASTLE_TY, 12.0f, 19.0f,
                e, t, m, 0.54f, 0.27f, 0.48f);
    flattenSite(x, y, (float)LOCKET_SITE_TX, (float)LOCKET_SITE_TY, 8.0f, 14.0f,
                e, t, m, 0.52f, 0.54f, 0.52f);
    flattenSite(x, y, (float)TOWER_SITE_TX, (float)TOWER_SITE_TY, 7.0f, 13.0f,
                e, t, m, 0.53f, 0.50f, 0.50f);
    int fx, fy; frontierCavernSite(fx, fy);
    flattenSite(x, y, (float)fx, (float)fy, 6.0f, 10.0f,
                e, t, m, 0.52f, 0.36f, 0.48f);
}

static bool nearSpecialSite(int tx, int ty) {
    auto nearPt = [&](int cx, int cy, int r) {
        int dx = tx - cx, dy = ty - cy;
        return dx * dx + dy * dy <= r * r;
    };
    if (nearPt(BOSS_CASTLE_TX, BOSS_CASTLE_TY, 10)) return true;
    if (nearPt(LOCKET_SITE_TX, LOCKET_SITE_TY, 7)) return true;
    if (nearPt(TOWER_SITE_TX, TOWER_SITE_TY, 6)) return true;
    return false;
}

static Biome climate(int tx, int ty, float e, float t, float m, float patch) {
    auto basin = [&](int cx, int cy, float rx, float ry) {
        float dx = (tx - cx) / rx, dy = (ty - cy) / ry;
        return dx * dx + dy * dy < 0.86f + 0.28f * patch;
    };
    if (abs(tx) < 19 && ty > -18 && ty < 18) return BI_MEADOW;
    if (basin(32, 39, 30, 23)) return BI_AMBER;
    if (basin(-44, -11, 30, 19)) return BI_FEN;
    if (basin(-26, 64, 30, 21)) return BI_DUNES;
    if (basin(0, -113, 40, 29)) return BI_FROST;
    if (basin(0, -80, 32, 19)) return BI_HEATH;
    if (basin(0, -43, 26, 22)) return BI_FOREST;
    if (tx * float(tx) + ty * float(ty) < 24 * 24) return BI_MEADOW;
    if (t < 0.33f || (t < 0.50f && e > 0.78f)) return BI_FROST;
    if (t > 0.58f && m < 0.47f) return BI_DUNES;
    if (m > 0.56f && e < 0.47f) return BI_FEN;
    if (e > 0.64f) return BI_HEATH;
    if (t > 0.48f && m < 0.40f) return BI_AMBER;
    if (m > 0.54f) return BI_FOREST;
    return BI_MEADOW;
}

// 0 = no trail, 1 = N/S planks, 2 = E/W planks. Two cells wide even on bends.
static int trailAt(int x, int y) {
    // North Road: Elmswick north gate (0..1, -15) -> Castle of Elm gate (0..1, -137)
    //
    // The bend must be a *function of the road*, not of a wave: a sine starts at
    // zero slope, so the old formula spent its first forty tiles within one tile
    // of x=0 and then swung to x=±10 and back, leaving the middle of the road
    // to fall wherever the noise put a lake. One gentle, readable drift across
    // the whole run — and the far end must land on the castle gate, so the
    // road ends where the castle is instead of twenty tiles off to one side.
    if (y <= -15 && y >= BOSS_CASTLE_TY + 7) {
        float u = (float)(-15 - y) / (float)(-15 - (BOSS_CASTLE_TY + 7));
        float drift = 7.0f * sinf(3.14159265f * u) + 2.0f * (1.0f - cosf(6.2831853f * u)) * 0.5f;
        int bend = (int)roundf(drift * (1.0f - u));     // returns to the gate at u=1
        if (x == bend || x == bend + 1) return 1;
    }
    // South Highway across Millbrook Bridge (0..1, 13..25)
    if ((x == 0 || x == 1) && y >= 13 && y <= 25) return 1;
    // Southwest Sunscorch branch from south bank crossroads
    if (y >= 25 && y <= 66) {
        float u = (float)(y - 25) / 41.0f;
        int bend = (int)roundf(-26.0f * fade(u) + 2.0f * sinf(u * 9.4247f));
        if (x == bend || x == bend + 1) return 1;
    }
    // Southeast Amberwood trail to the Standing Stones clearing
    if (x >= 0 && x <= LOCKET_SITE_TX) {
        float u = (float)x / (float)LOCKET_SITE_TX;
        int cy = 24 + (int)roundf(25.0f * fade(u) + 2.2f * sinf(u * 12.566f) * sinf(u * 3.14159f));
        if (y == cy || y == cy + 1) return 2;
    }
    // West Mirefen causeway from Elmswick west gate (-15, 0..1) to the Old Watchtower
    if (x <= -15 && x >= TOWER_SITE_TX + 3) {
        float u = (float)(-15 - x) / (float)(-15 - (TOWER_SITE_TX + 3));
        int cy = (int)roundf((TOWER_CHEST_TY)*fade(u) + 2.0f * sinf(u * 9.4247f) * sinf(u * 3.14159f));
        if (y == cy || y == cy + 1) return 2;
    }
    return 0;
}

// How much water lies under this trail tile along the road's own axis? A
// crossing is a short bridge; a road that runs twenty tiles out over a lake is
// not a road, it is a pier to nowhere — and it steals the only reason to own a
// boat. Planks are laid only where the far bank is genuinely close.
static float shoreT(float x, float y);   // forward: defined below
static bool trailCrossingIsShort(int tx, int ty, int axis) {
    // Never sampleGround() here: this runs *inside* generateGround and would
    // recurse until the stack dies. Water at this stage is either the pond's own
    // authored reach or terrain low enough to be a lake bed — both are pure
    // functions of the seed, so they can be asked directly.
    auto isWater = [](int x, int y) {
        if (pondWaterT((float)x + 0.5f, (float)y + 0.5f) < 1.0f) return true;
        if (shoreT((float)x + 0.5f, (float)y + 0.5f) >= 1.02f) return true;
        float e = elevationAt((float)x, (float)y, g.worldSeed);
        float t, m;
        flattenSpecialSites((float)x, (float)y, e, t, m);
        return e < 0.325f;
    };
    if (!isWater(tx, ty)) return true;
    // axis 1 = road runs north-south, so test that direction; 2 = east-west
    int stepX = (axis == 2) ? 1 : 0;
    int stepY = (axis == 1) ? 1 : 0;
    int fwd = 0, back = 0;
    for (int i = 1; i <= 12; i++) {
        if (isWater(tx + stepX * i, ty + stepY * i)) fwd++; else break;
    }
    for (int i = 1; i <= 12; i++) {
        if (isWater(tx - stepX * i, ty - stepY * i)) back++; else break;
    }
    return (fwd + back + 1) <= 12;
}

static bool nearTrail(int tx, int ty, int r) {
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++)
            if (trailAt(tx + dx, ty + dy) != 0) return true;
    return false;
}

// Cavern mouths are planned by the Survey (src/atlas.cpp): one per 88-tile
// lattice cell, on ground a party can walk to. Hollow Hill and the north-road
// mouth are authored, and are consulted first.


static bool surveyedCavernNear(int tx, int ty, int radius, Landmark& out) {
    int x, y;
    startCavernSite(x, y);
    if (abs(tx - x) <= radius && abs(ty - y) <= radius) {
        out = {LK_CAVERN, x, y, 0x57acu};
        return true;
    }
    Landmark north = northRoadCavern();
    if (abs(tx - north.tx) <= radius && abs(ty - north.ty) <= radius) {
        out = north; return true;
    }
    int fx, fy; frontierCavernSite(fx, fy);
    if (abs(tx - fx) <= radius && abs(ty - fy) <= radius) {
        out = {LK_CAVERN, fx, fy, 0xf204u}; return true;
    }
    AtlasSite site;
    if (atlasCavernNear(tx, ty, radius, site)) {
        out = {LK_CAVERN, site.tx, site.ty, site.seed};
        return true;
    }
    return false;
}

static bool isleStamp(int tx, int ty, GroundTile& out);
static bool onTidemillIsle(int tx, int ty);
static bool nearTidemillIsle(int tx, int ty);

static GroundTile generateGround(int tx, int ty) {
    GroundTile out;
    float x = (float)tx, y = (float)ty;
    uint32_t seed = g.worldSeed;
    float e = elevationAt(x, y, seed), t = temperatureAt(x, y, seed), m = moistureAt(x, y);
    flattenSpecialSites(x, y, e, t, m);

    // Authored roads are graded, not drawn on whatever the noise left behind.
    // The North Road runs 130 tiles from the town gate to the Castle of Elm; a
    // lake astride the middle of it used to swallow the road entirely, and the
    // party would walk into a wall of water halfway to the castle. A roadbed
    // lifts the ground just clear of the waterline instead, so the road crosses
    // its lakes on a natural dry causeway — the way a real road does.
    {
        float mpHere = pondWaterT(x, y);
        if (mpHere >= 1.18f) {
            int near = 0;
            for (int dy = -2; dy <= 2 && !near; dy++) for (int dx = -2; dx <= 2; dx++)
                if (trailAt(tx + dx, ty + dy) != 0) { near = 1; break; }
            if (near) {
                // 0.30 is open water, 0.368 the beach line: 0.40 is dry roadbed.
                const float roadE = 0.395f;
                if (e < roadE) e = roadE;
            }
        }
    }
    float patch = smoothNoise(x * 0.095f, y * 0.095f, seed ^ 0x4921u);
    float canopy = canopyDensityAt(tx, ty, seed);
    uint32_t h = hash2i(tx, ty, seed ^ 0x9a11u);
    out.biome = climate(tx, ty, e, t, m, patch);
    out.variant = (uint8_t)((h >> 8) % 3);

    const GroundKind soils[] = { G_GRASS, G_GRASS_DARK, G_AMBER, G_SWAMP, G_HEATH, G_SNOW, G_SAND };
    out.kind = soils[out.biome];

    // Couple ground shading directly to tree canopy so every woodland copse
    // casts autotiled dark forest turf or golden leaf-litter beneath its crowns.
    if (out.biome == BI_MEADOW && canopy > 0.56f && (tx * tx + ty * ty) > 19 * 19) out.kind = G_GRASS_DARK;
    if (out.biome == BI_AMBER && canopy > 0.43f) out.kind = G_LEAVES;
    if (out.biome == BI_DUNES && patch > 0.47f) out.kind = G_RIPPLES;
    if (out.biome == BI_FOREST && canopy < 0.40f) out.kind = G_GRASS;

    // Mountain ranges are the authority over every later land-cutting system:
    // authored trails, royal roads, cave aprons and wild ways cannot tunnel a
    // second opening through them. Only the seeded defiles may be crossed.
    RegionCell rc = regionCellAt(tx, ty);
    if (rc.terrain != REGION_NONE) {
        out.kind = (rc.terrain == REGION_RANGE || (rc.gate && !regionPassOpen(rc.ring)))
                 ? G_ROCK : G_DIRT;
        out.biome = BI_HEATH;
        out.deco = 0;
        return out;
    }
    // A legible, dry spur links the historic North Road, the Crown pass and
    // the guaranteed frontier mouth without erasing the castle's own yard.
    if (frontierRoadAt(tx, ty)) {
        out.kind = G_DIRT; out.deco = 0;
        return out;
    }

    // The Tidemill Isles are built, not found: stamp them before the water
    // rules below so the pond closes around their sand skirts naturally.
    if (isleStamp(tx, ty, out)) return out;

    // The millpond is authored water. Nothing downstream — the Millbrook, the
    // North Road, a survey road, a cave apron — may cut land into it, so it
    // answers here and returns.
    if (pondWaterT(x, y) < 1.0f) {
        // A mill pond is a *pond*: two to four tiles of depth the whole way
        // across. Nothing here is G_DEEP, because the boat's landing test looks
        // for open water and an isle ringed in "deep" strands the party afloat.
        out.kind = G_WATER;
        out.biome = BI_MEADOW;
        out.deco = (uint8_t)(((h >> 5) & 7) == 0 ? 4 : 0);      // lilies
        return out;
    }

    // Past the coast it is sea, full stop. This has to come before every later
    // rule (trails, roads, turnip plot, castle apron, cave apron, wild ways):
    // any single one of them re-cutting "land" out there would put a beach in
    // the middle of the ocean and undo the horizon.
    if (shoreT(x, y) >= 1.02f) { out.kind = G_DEEP; out.deco = 0; return out; }

    // Stepped SNES rocky bluffs & hillside outcrops (rendered with 3D cliff faces via relief.png)
    float ridge = fbm(x * 0.062f + 19.3f, y * 0.082f - 41.7f, seed ^ 0x31ffu, 3);
    float shelf = smoothNoise(x * 0.13f - 7.1f, y * 0.13f + 13.9f, seed ^ 0x8821u);
    float brook = brookCenterAt(x, seed);
    float bank = fabsf(y - brook);
    bool keepClearForTownOrRoad = ((tx * tx + ty * ty) < 22 * 22) || nearSpecialSite(tx, ty) ||
                                  nearTrail(tx, ty, 2) || bank < 4.5f;

    if (out.biome == BI_FROST) {
        if (patch < 0.24f && e < 0.55f) out.kind = G_ICE;
        else if (!keepClearForTownOrRoad && ((ridge > 0.62f && shelf > 0.50f) || patch > 0.70f)) out.kind = G_ROCK;
    } else if (out.biome == BI_FEN) {
        if (patch < 0.24f || e < 0.29f) out.kind = G_WATER;
        else if (canopy > 0.48f) out.kind = G_GRASS_DARK;
    } else if (e < 0.325f) {
        out.kind = e < 0.265f ? G_DEEP : G_WATER;
    } else if (e < 0.368f && out.biome != BI_DUNES) {
        out.kind = G_BEACH;
    } else if (!keepClearForTownOrRoad) {
        if (e > 0.745f ||
            ((out.biome == BI_HEATH || out.biome == BI_DUNES) && ridge > 0.61f && shelf > 0.50f) ||
            ((out.biome == BI_MEADOW || out.biome == BI_FOREST || out.biome == BI_AMBER) &&
             ridge > 0.68f && shelf > 0.55f)) {
            out.kind = G_ROCK;
        }
    }

    // Everything from here down cuts land out of the world: trails, survey
    // roads, cave aprons, the castle yard. None of it may cut land out of the
    // millpond, so the lake claims its own bank first and returns.
    {
        float mp = pondWaterT(x, y);
        if (mp >= 1.0f && mp < 1.18f) {
            // The bank frames the lake; it must never beach over water that is
            // already running into it, or the Millbrook ends at the pond's rim
            // and the landing is left sitting in a puddle of its own.
            if (out.kind == G_WATER || out.kind == G_DEEP) return out;
            out.kind = (mp < 1.05f) ? G_BEACH : G_GRASS;
            out.biome = BI_MEADOW;
            out.deco = 0;
            return out;
        }
    }

    // Millbrook River & structural crossings
    int sector = (int)floorf((x + 32.0f) / 64.0f);
    int ford = sector * 64 + (sector ? (int)(hash2i(sector, 3, seed) % 19) - 9 : 0);
    if (bank < 2.10f && !nearSpecialSite(tx, ty)) out.kind = G_WATER;
    else if (bank < 3.15f && !nearSpecialSite(tx, ty) && out.kind == G_GRASS && patch > 0.48f)
        out.kind = G_GRASS_DARK;

    int trail = trailAt(tx, ty);
    if ((tx == ford || tx == ford + 1) && bank < 4.6f) trail = 1;
    if (trail) {
        bool wet = out.kind == G_WATER || out.kind == G_DEEP;
        // Only short crossings get planks. A long stretch of deep water under a
        // trail is the boat's business; the trail walks up to the bank and stops.
        if (wet && !trailCrossingIsShort(tx, ty, trail)) return out;
        out.kind = wet ? (trail == 1 ? G_BRIDGE_NS : G_BRIDGE_EW) : G_DIRT;
        return out;
    }

    // The Survey's roads (src/atlas.cpp) join the settlements. Two tiles of
    // packed dirt, and planks wherever a road meets water — a crossing the party
    // can actually walk, which is the whole point of drawing them. Past the
    // coast no road runs: the sea is the only way on.
    if (shoreT(x, y) < 1.02f) {
        int axis = atlasRoadAxis(tx, ty);
        if (axis) {
            bool wet = out.kind == G_WATER || out.kind == G_DEEP;
            if (wet && !trailCrossingIsShort(tx, ty, (axis & 1) ? 1 : 2)) return out;
            out.kind = wet ? ((axis & 1) ? G_BRIDGE_NS : G_BRIDGE_EW) : G_DIRT;
            out.deco = 0;
            return out;
        }
    }

    // Grade a rocky shoulder and a clear, two-wide approach into each cave.
    // The story spur meets the west road exactly; other mouths get a dry apron.
    Landmark cave;
    if (surveyedCavernNear(tx, ty, 6, cave)) {
        int dx = tx - cave.tx, dy = ty - cave.ty;
        bool story = cave.seed == 0x57acu;
        if ((cave.seed == 0xB21A2u && dx >= -6 && dx <= 0 && dy >= 1 && dy <= 2) ||
            (abs(dx) <= 1 && dy >= 1 && dy <= (story ? 5 : 4))) {
            out.kind = G_DIRT; out.deco = 0; return out;
        }
        if (dx * dx + dy * dy <= 25) {
            out.kind = (dy <= -1 && abs(dx) <= 3) ? G_ROCK : G_HEATH;
            out.deco = 0; return out;
        }
    }

    // Castle of Elm grand stone courtyard & rocky rampart shoulders
    if (abs(tx - BOSS_CASTLE_TX) <= 9 && ty >= BOSS_CASTLE_TY - 6 && ty <= BOSS_CASTLE_TY + 7) {
        int dx = abs(tx - BOSS_CASTLE_TX);
        if (dx <= 6 && ty >= BOSS_CASTLE_TY - 4) {
            out.kind = (dx <= 2 && ty > BOSS_CASTLE_TY) ? G_COBBLE : G_FLOOR;
            return out;
        }
        if ((dx >= 7 && ty <= BOSS_CASTLE_TY + 4) || ty <= BOSS_CASTLE_TY - 5) {
            out.kind = G_ROCK;
            return out;
        }
    }

    // Standing Stones ancient paved ring (52, 46)
    {
        float dx = x - (float)LOCKET_SITE_TX, dy = y - (float)(LOCKET_SITE_TY + 0.5f);
        float r2 = dx * dx + dy * dy;
        if (r2 <= 2.4f * 2.4f) { out.kind = G_FLOOR; return out; }
        if (r2 <= 4.2f * 4.2f) { out.kind = G_COBBLE; return out; }
    }

    // Old Watchtower raised cobbled keep & marsh moat (-64, -20)
    {
        int dx = tx - TOWER_SITE_TX, dy = ty - TOWER_SITE_TY;
        if (abs(dx) <= 3 && dy >= -2 && dy <= 3) {
            out.kind = (abs(dx) <= 1 && dy >= 1) ? G_COBBLE : G_FLOOR;
            return out;
        }
        if (abs(dx) <= 5 && dy >= -4 && dy <= 5) {
            // Leave eastern causeway approach open
            if (!(dx >= 3 && (ty == TOWER_CHEST_TY || ty == TOWER_CHEST_TY + 1))) {
                out.kind = G_WATER;
                if ((h & 7) == 0) out.deco = 4;
                return out;
            } else {
                out.kind = G_BRIDGE_EW;
                return out;
            }
        }
    }

    if (nearSpecialSite(tx, ty) && (out.kind == G_ROCK || out.kind == G_WATER || out.kind == G_DEEP))
        out.kind = soils[out.biome];


    // The millpond's bank: meadow on the rim, a beach where it meets water.
    {
        float mp = millpondT(x, y);
        if (mp >= 1.0f && mp < 1.22f) {
            bool wetHere = out.kind == G_WATER || out.kind == G_DEEP;
            if (!wetHere && (out.kind == G_WATER || out.kind == G_DEEP || mp < 1.06f)) {
                out.kind = G_BEACH;
                out.biome = BI_MEADOW;
                out.deco = 0;
                return out;
            }
            if (!wetHere && (out.kind == G_ROCK || out.kind == G_SNOW || out.kind == G_ICE || out.kind == G_SWAMP))
                out.kind = G_GRASS;
            out.biome = BI_MEADOW;
        }
    }

    // The coast: a sand ring wherever the land meets the sea, and no crag
    // standing in the shallows. The millpond keeps its grassy bank (its water
    // is inland, where shoreT is small), so Elmswick still looks like Elmswick.
    {
        float st = shoreT(x, y);
        if (st > 0.955f && st < 1.06f) {
            bool wetHere = out.kind == G_WATER || out.kind == G_DEEP;
            bool nearWet = false;
            // Ask the elevation field, never sampleGround(): sampling from
            // inside the generator is a recursion the terrain does not survive.
            for (int oy = -1; oy <= 1 && !wetHere; oy++)
                for (int ox = -1; ox <= 1 && !wetHere; ox++) {
                    if (!ox && !oy) continue;
                    if (elevationAt(x + (float)ox, y + (float)oy, seed) < 0.325f) nearWet = true;
                }
            if (!wetHere && (nearWet || out.kind == G_BEACH || st > 1.0f)) {
                out.kind = G_BEACH;
                out.deco = 0;
                return out;
            }
            if (wetHere && st > 1.0f) { out.kind = G_WATER; out.deco = 0; return out; }
        }
        if (st >= 1.06f && st < 1.16f && out.kind != G_WATER && out.kind != G_DEEP) {
            out.kind = G_BEACH;
            out.deco = 0;
            return out;
        }
    }

    // Hedrick's fenced kitchen turnip plot in southwest Elmswick
    if (tx >= -13 && tx <= -9 && ty >= 9 && ty <= 11) {
        out.kind = G_SOIL;
        out.deco = 5;
        return out;
    }

    float d = (float)(h & 0xffff) / 65536.0f;
    out.decoVariant = (uint8_t)((h >> 16) % 3);
    if (out.kind == G_GRASS || out.kind == G_GRASS_DARK) {
        bool treelineSkirt = (canopy > 0.36f && canopy < 0.55f);
        if ((treelineSkirt || bank < 4.2f) && d < 0.24f) out.deco = 2; // wildflowers along treelines & banks
        else if (patch > 0.54f && d < 0.16f) out.deco = 2;
        else if (d < 0.14f) out.deco = 1;
    } else if (out.kind == G_AMBER || out.kind == G_LEAVES) {
        if (d < 0.10f) out.deco = 1;
    } else if (out.kind == G_WATER && (out.biome == BI_FEN || bank < 2.2f) && d < 0.16f) {
        out.deco = 4; // water lilies
    } else if ((out.kind == G_HEATH || out.kind == G_BEACH || out.kind == G_SAND) && d < 0.08f) {
        out.deco = 3; // pebbles
    }

    // A wild way runs through here: the cut is trodden, and nothing grows back
    // over it. Only consultable once the survey for this sector has run.
    if (onWildWay(tx, ty)) {
        out.kind = wayGroundKind(tx, ty, out.kind);
        out.deco = 0;
    }
    return out;
}

// ================================================================ survey terrain
// The Survey cannot call sampleGround() — sampleGround asks the Survey where the
// caverns are, and the survey would then depend on visit order. This runs the
// same noise generateGround starts from, one layer below every cache, and answers
// the only question placement needs: is this tile water, ice, crag or marsh?
// It is asked from several directions at once (placement rings, road nodes, the
// survey page), so a direct-mapped cache keeps a noise field from being paid twice.
namespace {
struct SurveyCacheEntry {
    int x = 1 << 30, y = 1 << 30;
    uint32_t seed = 0;
    FieldSurvey t;
};
SurveyCacheEntry gSurveyCache[1 << 15];
}

FieldSurvey fieldSurveyTerrain(int tx, int ty) {
    SurveyCacheEntry& c = gSurveyCache[((uint32_t)tx * 73856093u ^ (uint32_t)ty * 19349663u) & ((1u << 15) - 1)];
    if (c.x == tx && c.y == ty && c.seed == g.worldSeed) return c.t;
    FieldSurvey out;
    float x = (float)tx, y = (float)ty;
    uint32_t seed = g.worldSeed;
    float e = elevationAt(x, y, seed), t = temperatureAt(x, y, seed), m = moistureAt(x, y);
    flattenSpecialSites(x, y, e, t, m);
    float patch = smoothNoise(x * 0.095f, y * 0.095f, seed ^ 0x4921u);
    float ridge = fbm(x * 0.062f + 19.3f, y * 0.082f - 41.7f, seed ^ 0x31ffu, 3);
    float shelf = smoothNoise(x * 0.13f - 7.1f, y * 0.13f + 13.9f, seed ^ 0x8821u);
    out.elevation = e;
    out.biome = climate(tx, ty, e, t, m, patch);
    // The Tidemill Isles are meadow and sand wherever the noise put them, so
    // the Survey must not file them as open water and start a village there.
    bool isle = onTidemillIsle(tx, ty);
    if (isle) out.biome = BI_MEADOW;
    out.brookDistance = fabsf(y - brookCenterAt(x, seed));

    if (out.biome == BI_FEN) {
        if (patch < 0.24f || e < 0.29f) out.water = true;
        else out.swamp = true;
    } else if (e < 0.325f && !isle) {
        out.water = true;
        out.deep = e < 0.265f;
    } else if (isle) {
        // meadow crown: no crag, no ice
    } else if (e > 0.745f ||
               ((out.biome == BI_HEATH || out.biome == BI_DUNES) && ridge > 0.61f && shelf > 0.50f) ||
               ((out.biome == BI_MEADOW || out.biome == BI_FOREST || out.biome == BI_AMBER) &&
                ridge > 0.68f && shelf > 0.55f)) {
        out.crag = true;
    }
    if (out.biome == BI_FROST && patch < 0.24f && e < 0.55f) out.ice = true;
    if (out.brookDistance < 2.10f) out.water = true;      // Millbrook
    c = { tx, ty, g.worldSeed, out };
    return out;
}

GroundTile sampleGround(int tx, int ty) {
    struct Entry { int x = 0, y = 0; uint32_t seed = 0, epoch = 0; int access = 0; bool valid = false; GroundTile tile; };
    static Entry cache[8192];
    Entry& c = cache[((uint32_t)ty * 131u + (uint32_t)tx) & 8191];
    int access = (regionPassOpen(1) ? 1 : 0) | (regionPassOpen(2) ? 2 : 0);
    if (!c.valid || c.x != tx || c.y != ty || c.seed != g.worldSeed || c.epoch != gWayEpoch || c.access != access) {
        GroundTile t = generateGround(tx, ty);
        pieceDressGround(tx, ty, t);     // the map-piece seam frames the wilds
        c = { tx, ty, g.worldSeed, gWayEpoch, access, true, t };
    }
    return c.tile;
}

static bool groundWalkable(GroundKind k) {
    return k != G_WATER && k != G_DEEP && k != G_ROCK;
}
// The water a boat may cross. Inside the continent every pond, river and lake
// is fair water; past the coast the sea refuses a hull, so the horizon stays a
// horizon and the whole ocean is a wall, not another 30 minutes of nothing.
bool inOpenSea(float x, float y) { return shoreT(x, y) >= 1.02f; }

bool waterSailableAt(int tx, int ty) {
    GroundKind k = sampleGround(tx, ty).kind;
    if (k != G_WATER && k != G_DEEP) return false;
    return !inOpenSea((float)tx + 0.5f, (float)ty + 0.5f);
}

// ================================================================ wild ways
// The world is endless, but cliffs, rivers and shoulder-to-shoulder woods were
// quietly fencing it into rooms: a crag three tiles deep, a packed conifer wood,
// a salt marsh, and behind them a sunlit dell that nothing could reach. This
// survey fixes that from the terrain's side, for the whole world and from the
// seed alone.
//
// A door belongs to an EDGE of a 64-tile sector, and both sectors that share
// that edge compute it with the same call, so they always name the same tile.
// Inside a sector, its four doors and one meeting stone are joined by the
// cheapest route: open ground is free, crag, water and wood cost a cut (the
// route grades a trodden crossing through them), authored ground is impassable.
// Every sector connects its own doors to each other, so the whole world is one
// connected lattice of ways by construction — an endless grid of crossings
// hidden inside the terrain, invisible until a wall stops you and you find the
// cut through it. A dell the ways cannot reach gets a short cut to the sector's
// heart, so every glade is a find and no place is a room without a door.
//
// The survey is per-sector and on demand — there is no ready-wave at load, only
// the sectors you stand in and are about to enter are worked out — and it is a
// pure function of the seed, so the renderer, the collision test, the click-to-
// walk pathfinder and a reloaded save always agree on where the ways run.
constexpr int WAY_SECTOR = 64;
constexpr int WAY_MIN = 12, WAY_MAX = 51;       // door offsets inside a sector
static const int d8Static[8][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 }, { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 } };

// A tile the survey may cut through. Crag, water, ice and wood are the whole
// point — a crossing is the cut in them — so only authored ground is refused:
// roofs, paving, soil, a landmark's own stones, a dungeon mouth, the road, or a
// scripted story site.
static bool wayCarvable(int tx, int ty) {
    // The sector lattice joins paths within a country, not through a range or
    // a sealed pass. The ground generator enforces the same boundary as a
    // second line of defence, even for ways surveyed before a gate opened.
    if (regionCellAt(tx, ty).terrain != REGION_NONE) return false;
    if (frontierRoadAt(tx, ty)) return false;
    // The Open Water: the wild ways connect the LAND, not the sea. A way may
    // plank across a brook or a pond arm inside the coast, but never past the
    // coast, never across the millpond's deep middle, and never onto one of
    // the Tidemill Isles — those are a boat's business. Without this the
    // survey would lay a dirt track over open water and quietly undo both the
    // horizon and the islands.
    if (shoreT((float)tx + 0.5f, (float)ty + 0.5f) >= 1.0f) return false;
    {
        GroundKind k = sampleGround(tx, ty).kind;
        if ((k == G_WATER || k == G_DEEP) && (k == G_DEEP || onTidemillIsle(tx, ty) || nearTidemillIsle(tx, ty)))
            return false;
    }
    switch (sampleGround(tx, ty).kind) {
        case G_COBBLE: case G_FLOOR: case G_SOIL:
        case G_BRIDGE_NS: case G_BRIDGE_EW: case G_DIRT:
            return false;
        default: break;
    }
    if (nearSpecialSite(tx, ty)) return false;
    if (nearTrail(tx, ty, 1)) return false;              // the authored roads stay theirs
    if (tx * tx + ty * ty < 34 * 34) return false;        // Elmswick's meadow stays hand-set
    if (g.villageLoaded && abs(tx - g.village.cx) <= 19 && abs(ty - g.village.cy) <= 19) return false;
    Landmark lm;
    if (landmarkNear(tx, ty, lm) && abs(tx - lm.tx) <= 7 && abs(ty - lm.ty) <= 7) return false;
    int cx, cy;
    if (startCavernSite(cx, cy) && abs(tx - cx) <= 7 && abs(ty - cy) <= 7) return false;
    return true;
}

static bool wayGroundOpen(int tx, int ty) {
    GroundKind k = sampleGround(tx, ty).kind;
    return k != G_WATER && k != G_DEEP && k != G_ROCK;
}

// Trees, rocks and the rest of the prop layer are solids a survey must route
// around or cut down; the rest is scenery.
static bool wayPropSolid(PropKind p) {
    switch (p) {
        case P_TREE0: case P_TREE1: case P_TREE2: case P_PINE0: case P_PINE1: case P_PINE2:
        case P_DEADTREE: case P_TREE_SNOW: case P_ROCK0: case P_ROCK1: case P_ROCK2:
        case P_BOULDER: case P_CRYSTAL: case P_STUMP:
        case P_AMBER_OAK: case P_COPPER_OAK: case P_WILLOW:
            return true;
        default:
            return false;
    }
}

// The survey's own walkable judgement. It matches cellWalkable() everywhere it
// can be consulted without recursing, because a way may only be opened where
// this says a body could stand.
static bool landmarkCellBlocked(int tx, int ty);   // defined with the landmark pass below
// True iff this tile is water with dry land within five steps on BOTH sides of
// one axis: a brook, a ditch, a pond's narrow arm. Anything wider is a lake, and
// the water is the boat's business.
static bool wayCrossableWater(int tx, int ty) {
    GroundKind k = sampleGround(tx, ty).kind;
    if (k != G_WATER && k != G_DEEP) return false;
    static const int DX[4] = { 1, -1, 0, 0 }, DY[4] = { 0, 0, 1, -1 };
    for (int axis = 0; axis < 2; axis++) {          // 0 = east/west, 1 = north/south
        bool bothSides = true;
        for (int side = 0; side < 2; side++) {
            int dir = axis * 2 + side;
            bool land = false;
            for (int step = 1; step <= 5; step++) {
                GroundKind nk = sampleGround(tx + DX[dir] * step, ty + DY[dir] * step).kind;
                if (nk != G_WATER && nk != G_DEEP) { land = true; break; }
            }
            if (!land) { bothSides = false; break; }
        }
        if (bothSides) return true;
    }
    return false;
}

static bool wayStandable(int tx, int ty) {
    if (!wayGroundOpen(tx, ty)) return false;
    if (wayPropSolid(propAt(tx, ty))) return false;
    if (landmarkCellBlocked(tx, ty)) return false;
    return true;
}

// A way is only ever stamped where it had to cut. Ground a body can already
// stand on stays exactly as the seed made it, so the wilds keep their look and
// the survey's whole visible footprint is the cuts through real walls.
static bool wayCrossableWater(int tx, int ty);   // defined with the water rules below
static void wayCarve(int tx, int ty) {
    // A cut through trees or crag becomes dirt; a cut through water becomes a
    // boardwalk, and only across a crossing. Anything wider is a lake: the way
    // routes around it and the party takes the boat.
    GroundKind k = sampleGround(tx, ty).kind;
    if ((k == G_WATER || k == G_DEEP) && !wayCrossableWater(tx, ty)) return;
    gWayCells.insert(wayKey(tx, ty));
}

// Step `(xa,ya) -> (xb,yb)` as part of a route: grade the far tile if it is a
// wall, and where the step is diagonal also open one shoulder, because the game
// will not cut a corner through a wall.
static void wayRecordStep(int xa, int ya, int xb, int yb) {
    if (!wayStandable(xb, yb) && wayCarvable(xb, yb)) wayCarve(xb, yb);
    if (xa == xb || ya == yb) return;
    int c1x = xb, c1y = ya, c2x = xa, c2y = yb;
    if (wayStandable(c1x, c1y) || wayStandable(c2x, c2y)) return;
    if (wayCarvable(c1x, c1y)) wayCarve(c1x, c1y);
    else if (wayCarvable(c2x, c2y)) wayCarve(c2x, c2y);
}

// The opening in a sector's north edge (row sy*64). Both the sector above and
// the sector below call this with the same arguments and get the same tile; if
// the edge is walled there, the tile is cut so the pair really is walkable.
static int wayEdgeOpeningX(int sx, int sy) {
    int edgeX = sx * WAY_SECTOR, edgeY = sy * WAY_SECTOR;
    int base = edgeX + WAY_MIN +
               (int)(hash2i(sx, sy, g.worldSeed ^ 0x3d0du) % (uint32_t)(WAY_MAX - WAY_MIN));
    for (int k = 0; k <= 20; k++) {
        for (int sgn = (k == 0 ? 1 : -1); sgn <= 1; sgn += 2) {
            int x = base + sgn * k;
            if (x < edgeX || x >= edgeX + WAY_SECTOR) continue;
            if (wayStandable(x, edgeY)) return x;
            if (wayCarvable(x, edgeY)) { wayCarve(x, edgeY); return x; }
        }
    }
    return base;
}

// The opening in a sector's west edge (column sx*64), shared with the sector
// to the left in the same way.
static int wayEdgeOpeningY(int sx, int sy) {
    int edgeX = sx * WAY_SECTOR, edgeY = sy * WAY_SECTOR;
    int base = edgeY + WAY_MIN +
               (int)(hash2i(sx, sy, g.worldSeed ^ 0x9e11u) % (uint32_t)(WAY_MAX - WAY_MIN));
    for (int k = 0; k <= 20; k++) {
        for (int sgn = (k == 0 ? 1 : -1); sgn <= 1; sgn += 2) {
            int y = base + sgn * k;
            if (y < edgeY || y >= edgeY + WAY_SECTOR) continue;
            if (wayStandable(edgeX, y)) return y;
            if (wayCarvable(edgeX, y)) { wayCarve(edgeX, y); return y; }
        }
    }
    return base;
}

struct WaySurveyScratch {
    std::vector<int> cost, prev;
    size_t at(int x, int y, int x0, int y0) const {
        return (size_t)(y - y0) * WAY_SECTOR + (size_t)(x - x0);
    }
};

// One sector's survey. Its five way-points — the openings on its four edges and
// a meeting stone inside — are joined pairwise by the cheapest route, which is
// what makes the lattice connected and the world walkable forever.
static void wildWaySurveySector(int sx, int sy) {
    const int x0 = sx * WAY_SECTOR, y0 = sy * WAY_SECTOR;
    constexpr int N = WAY_SECTOR;
    constexpr int INF = 1 << 28;
    auto at = [&](int x, int y) { return (size_t)(y - y0) * N + (size_t)(x - x0); };

    const int doors[5][2] = {
        { wayEdgeOpeningX(sx, sy), y0 },                    // north edge
        { x0 + N - 1, wayEdgeOpeningY(sx + 1, sy) },        // east edge (the next sector's west edge)
        { wayEdgeOpeningX(sx, sy + 1), y0 + N - 1 },        // south edge (the next sector's north edge)
        { x0, wayEdgeOpeningY(sx, sy) },                    // west edge
        { x0 + 10 + (int)(hash2i(sx * 3 + 1, sy * 5 + 2, g.worldSeed ^ 0x51b7u) % (uint32_t)(N - 20)),
          y0 + 10 + (int)(hash2i(sx * 7 + 4, sy * 3 + 9, g.worldSeed ^ 0x2ad3u) % (uint32_t)(N - 20)) },
    };

    // An edge opening is carved on the shared edge line; the tile just inside it
    // must be walkable too, or the opening leads nowhere. Both sides of every
    // shared edge do this for their own inner tile, so a door is a door on both
    // faces of the wall.
    {
        // A short trodden stub leading in from each opening, so the door is
        // joined to the sector's interior and never a doorway into its own wall.
        const int inDir[4][2] = { { 0, 1 }, { -1, 0 }, { 0, -1 }, { 1, 0 } };
        for (int d = 0; d < 4; d++) {
            for (int k = 0; k < 3; k++) {
                int cx = doors[d][0] + inDir[d][0] * k;
                int cy = doors[d][1] + inDir[d][1] * k;
                if (cx < x0 || cx >= x0 + N || cy < y0 || cy >= y0 + N) continue;
                if (wayStandable(cx, cy) && wayGroundOpen(cx, cy)) continue;
                if (wayCarvable(cx, cy)) wayCarve(cx, cy);
            }
        }
    }

    std::vector<int> cost((size_t)N * N, INF);
    std::vector<int> prev((size_t)N * N, -1);
    using Node = std::pair<int, int>;

    auto route = [&](int fx, int fy, int tx, int ty) {
        if (fx < x0 || fx >= x0 + N || fy < y0 || fy >= y0 + N) return;
        if (tx < x0 || tx >= x0 + N || ty < y0 || ty >= y0 + N) return;
        std::fill(cost.begin(), cost.end(), INF);
        std::fill(prev.begin(), prev.end(), -1);
        std::priority_queue<Node, std::vector<Node>, std::greater<Node>> open;
        cost[at(fx, fy)] = 0;
        open.push({ 0, (int)at(fx, fy) });
        int expanded = 0;
        while (!open.empty() && expanded < N * N) {
            auto [c, i] = open.top(); open.pop();
            if (c != cost[(size_t)i]) continue;
            expanded++;
            int x = (int)((size_t)i % N) + x0, y = (int)((size_t)i / N) + y0;
            if (x == tx && y == ty) break;
            for (auto& d : d8Static) {
                int nx = x + d[0], ny = y + d[1];
                if (nx < x0 || nx >= x0 + N || ny < y0 || ny >= y0 + N) continue;
                bool stand = wayStandable(nx, ny);
                // A step that is not standable has to be carvable *as a crossing*.
                // wayCarvable alone would let the survey plank the whole pond:
                // this is the difference between a ford and a causeway.
                if (!stand && !(wayCarvable(nx, ny) && (wayGroundOpen(nx, ny) || wayCrossableWater(nx, ny)))) continue;
                // Open ground is cheap; a stand of trees is a worn track waiting
                // to be cut (a groove through a wood is a pleasant thing to find);
                // crag costs more and open water most, so ways prefer the
                // shallowest wall available while still being willing to cut one.
                // Without this a dense grove's staggered tree lattice was a wall
                // that only a diagonal step could cross — and the game does not
                // allow diagonal steps through walls.
                bool wooded = wayGroundOpen(nx, ny) && wayPropSolid(propAt(nx, ny));
                int step = stand ? 2 : (wooded ? 4 : (wayGroundOpen(nx, ny) ? 9 : 12));
                if (d[0] && d[1]) step += 1;
                size_t j = at(nx, ny);
                if (c + step >= cost[j]) continue;
                cost[j] = c + step;
                prev[j] = (int)i;
                open.push({ c + step, (int)j });
            }
        }
        if (cost[at(tx, ty)] >= INF) return;
        std::vector<int> chain;
        for (int i = (int)at(tx, ty); i >= 0 && chain.size() < (size_t)N * N; i = prev[(size_t)i]) {
            chain.push_back(i);
            if (i == (int)at(fx, fy)) break;
        }
        std::reverse(chain.begin(), chain.end());
        for (size_t k = 1; k < chain.size(); k++) {
            int p = chain[k - 1], c = chain[k];
            wayRecordStep((int)((size_t)p % N) + x0, (int)((size_t)p / N) + y0,
                          (int)((size_t)c % N) + x0, (int)((size_t)c / N) + y0);
        }
    };

    for (int d = 0; d < 4; d++) route(doors[d][0], doors[d][1], doors[4][0], doors[4][1]);

    // ---- pockets ------------------------------------------------------------
    // A dell behind a crag is a find, never a room with no door. Flood the open
    // ground of a 3-sector window around this sector from the ways in it; any
    // pocket of a dozen tiles or more that the ways cannot reach gets a short
    // cut to the nearest way. Pockets are judged whole (a pocket is only acted
    // on when its own bounding box sits inside the window), so two sectors never
    // half-open the same dell, and a pocket a player is walking toward is always
    // opened by one of the sectors their approach touches.
    constexpr int WAY_MARGIN = 16;
    const int wx0 = x0 - WAY_MARGIN, wy0 = y0 - WAY_MARGIN;
    const int ww = WAY_SECTOR + 2 * WAY_MARGIN;
    auto wat = [&](int x, int y) { return (size_t)(y - wy0) * ww + (size_t)(x - wx0); };
    auto inWin = [&](int x, int y) { return x >= wx0 && x < wx0 + ww && y >= wy0 && y < wy0 + ww; };
    // Standability is asked nine times per cell by the flood, and each ask costs
    // a prop query. Decide it once per window cell, then flood the bitmap.
    std::vector<uint8_t> standBit((size_t)ww * ww, 0);
    for (int y = wy0; y < wy0 + ww; y++)
        for (int x = wx0; x < wx0 + ww; x++)
            standBit[wat(x, y)] = (wayGroundOpen(x, y) && !wayPropSolid(propAt(x, y))) ? 1 : 0;
    auto standNow = [&](int x, int y) {
        if (!inWin(x, y)) return false;
        if (standBit[wat(x, y)]) return true;
        return gWayCells.count(wayKey(x, y)) != 0;          // a recorded cut is walkable by construction
    };
    std::vector<uint8_t> reached((size_t)ww * ww, 0);
    std::vector<int> fq;
    for (int y = wy0; y < wy0 + ww; y++)
        for (int x = wx0; x < wx0 + ww; x++) {
            if (!gWayCells.count(wayKey(x, y))) continue;
            size_t i = wat(x, y);
            if (reached[i]) continue;
            reached[i] = 1;
            fq.push_back((int)i);
        }
    for (size_t k = 0; k < fq.size(); k++) {
        int x = (int)((size_t)fq[k] % ww) + wx0, y = (int)((size_t)fq[k] / ww) + wy0;
        for (auto& d : d8Static) {
            int nx = x + d[0], ny = y + d[1];
            if (!inWin(nx, ny)) continue;
            size_t j = wat(nx, ny);
            if (reached[j] || !standNow(nx, ny)) continue;
            reached[j] = 1;
            fq.push_back((int)j);
        }
    }
    // The window's ways can have opened a good deal on their own; a pocket only
    // needs a cut when it is big by itself, so the threshold here is the one the
    // ladder of tests settled on: twelve tiles of open ground with no way into
    // them is a room without a door, and it gets one.
    int adopted = 0;
    for (int y = wy0; y < wy0 + ww && adopted < 8; y++)
        for (int x = wx0; x < wx0 + ww && adopted < 8; x++) {
            size_t start2 = wat(x, y);
            if (reached[start2] || !standNow(x, y)) continue;
            std::vector<int> pocket, pq;
            pq.push_back((int)start2);
            reached[start2] = 1;
            int minX = x, maxX = x, minY = y, maxY = y;
            for (size_t head = 0; head < pq.size(); head++) {
                int px = (int)((size_t)pq[head] % ww) + wx0, py = (int)((size_t)pq[head] / ww) + wy0;
                pocket.push_back(pq[head]);
                minX = std::min(minX, px); maxX = std::max(maxX, px);
                minY = std::min(minY, py); maxY = std::max(maxY, py);
                for (auto& d : d8Static) {
                    int nx = px + d[0], ny = py + d[1];
                    if (!inWin(nx, ny)) continue;
                    size_t j = wat(nx, ny);
                    if (reached[j] || !standNow(nx, ny)) continue;
                    reached[j] = 1;
                    pq.push_back((int)j);
                }
            }
            if (pocket.size() < 12) continue;
            if (minX <= wx0 || minY <= wy0 || maxX >= wx0 + ww - 1 || maxY >= wy0 + ww - 1) continue;
            if (abs(x - x0) >= WAY_SECTOR || abs(y - y0) >= WAY_SECTOR) continue;
            std::pair<int, int> from = { x, y }, to = { x, y };
            int bestD = 1 << 28;
            for (int k : pocket) {
                int px = (int)((size_t)k % ww) + wx0, py = (int)((size_t)k / ww) + wy0;
                for (int r = 1; r <= 24 && r * r < bestD; r++)
                    for (int dy = -r; dy <= r; dy++)
                        for (int dx = -r; dx <= r; dx++) {
                            if (std::max(abs(dx), abs(dy)) != r) continue;
                            int nx = px + dx, ny = py + dy;
                            if (!inWin(nx, ny) || !gWayCells.count(wayKey(nx, ny))) continue;
                            int d2 = dx * dx + dy * dy;
                            if (d2 < bestD) { bestD = d2; from = { px, py }; to = { nx, ny }; }
                        }
            }
            if (bestD == (1 << 28)) continue;
            route(from.first, from.second, to.first, to.second);
            adopted++;
            for (int k : pocket) {
                int px = (int)((size_t)k % ww) + wx0, py = (int)((size_t)k / ww) + wy0;
                for (auto& d : d8Static) {
                    int nx = px + d[0], ny = py + d[1];
                    if (!inWin(nx, ny)) continue;
                    size_t j = wat(nx, ny);
                    if (reached[j] || !standNow(nx, ny)) continue;
                    reached[j] = 1;
                }
            }
        }

    gWayEpoch++;
}

// True when the ways of the seed's surveyed sectors include this cell.
static bool onWildWay(int tx, int ty) {
    if (gWaySeed != g.worldSeed) return false;
    return gWayCells.count(wayKey(tx, ty)) != 0;
}

// Survey the sectors the party stands in and is about to enter. Cheap: nine
// sectors of a 64x64 Dijkstra, once each per seed and never again.
static void wildWayEnsureFor(int sx, int sy) {
    if (gWaySeed != g.worldSeed) {
        gWayCells.clear();
        gWaySectorsDone.clear();
        gWaySeed = g.worldSeed;
        gWayEpoch++;
    }
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
            int cx = sx + dx, cy = sy + dy;
            int64_t key = waySectorIndex(cx, cy);
            if (gWaySectorsDone.count(key)) continue;
            gWaySectorsDone.insert(key);
            wildWaySurveySector(cx, cy);
        }
}

// Ground a way runs over is trodden: the survey's cuts are DIRT, the same worn
// surface as the roads, so no undergrowth grows back over the cut.
static GroundKind wayGroundKind(int tx, int ty, GroundKind natural) {
    if (!onWildWay(tx, ty)) return natural;
    if (natural == G_COBBLE || natural == G_FLOOR || natural == G_SOIL ||
        natural == G_BRIDGE_NS || natural == G_BRIDGE_EW || natural == G_DIRT)
        return natural;
    return G_DIRT;
}

bool villageDecoSolid(int kind) {
    switch (kind) {
        case VD_SHADE_TREE:
        case VD_PINE:
        case VD_WELL:
        case VD_SIGN:
        case VD_BENCH:
        case VD_TORCH:
        case VD_CAMPFIRE:
        case VD_BARREL:
        case VD_STATUE:
        case VD_CAULDRON:
        case VD_URN:
        case VD_FENCE_H:
        case VD_FENCE_V:
        case VD_FENCE_GATE:
        case VD_GATE:
        case VD_TEMPLE_PILLAR:
        case VD_CRATE:
        case VD_TABLE:
        case VD_LONGTABLE:
        case VD_BUCKET:
        case VD_FLOWERBED:
        case VD_MAILBOX:
        case VD_NOTICEBOARD:
        case VD_ROCK:
        case VD_STUMP:
        case VD_STATUS_STONE:
        case VD_EMPTY_PLOT:
            return true;
        default:
            return false; // VD_PLANT, VD_BUSH, VD_FLOWER_BUSH, VD_LADDER stay walkable/brushable
    }
}

float villageDecoCenterX(const Village::Deco& d) {
    int dx = (int)floorf(d.x + 0.5f);
    if (d.kind == VD_STATUS_STONE || d.kind == VD_EMPTY_PLOT) return (float)((dx + 0.5f) * TILE);
    if (d.kind == VD_WELL || d.kind == VD_BENCH || d.kind == VD_FLOWERBED || d.kind == VD_FENCE_GATE) {
        // 2-tile wide [dx-1 .. dx]: exact pixel midpoint is dx * TILE
        return (float)(dx * TILE);
    }
    // 1-tile [dx] or 3-tile [dx-1 .. dx+1]: exact pixel midpoint is (dx + 0.5) * TILE
    return ((float)dx + 0.5f) * (float)TILE;
}

// ================================================================ landmarks
// Hollow Hill — the story cavern west of Elmswick on the Old Watchtower road.
// Derived directly from the seed and west causeway so rendering, collision
// and interaction always agree, and no failed search can erase the quest site.
bool startCavernSite(int& outTx, int& outTy) {
    // A seeded spur immediately off the west causeway. This site must never
    // disappear because a terrain-search happened not to find a candidate.
    outTx = -29 - (int)(hash2i(0, 0, g.worldSeed ^ 0x57acu) % 10u);
    static uint32_t seed = 0;
    static int roadY = 0;
    static bool valid = false;
    if (!valid || seed != g.worldSeed) {
        seed = g.worldSeed; valid = true; roadY = 0;
        for (int y = -24; y <= 8; ++y)
            if (trailAt(outTx, y) == 2) { roadY = y; break; }
    }
    outTy = roadY - 5;
    return true;
}

// The Survey owns the five ruin-kinds (src/atlas.cpp). Every landmark now has a
// stable position and a stable seed instead of a lucky roll.
static bool proceduralLandmarkNear(int tx, int ty, int radius, Landmark& out) {
    AtlasSite site;
    if (!atlasLandmarkNear(tx, ty, radius, site)) return false;
    out = { (LandmarkKind)(site.kind - 1), site.tx, site.ty, site.seed };   // AS_RUINS==LK_RUINS+1
    return true;
}

void expandLandmarkParts(const Landmark& lm, std::vector<LandmarkPart>& out) {
    out.clear();
    // A Tidemill Isle's standing stones put a Tidewell at their heart. The
    // island is identified by its seed tag, which outranks the generic ring.
    if ((lm.seed & 0xFFFF0000u) == 0x71de0000u) {
        out.push_back({ LP_TIDEWELL, lm.tx, lm.ty + 1, 1 });
        out.push_back({ LP_PILLAR, lm.tx - 2, lm.ty, 1 });
        out.push_back({ LP_PILLAR, lm.tx + 2, lm.ty, 1 });
        out.push_back({ LP_PILLAR, lm.tx - 2, lm.ty + 2, 1 });
        out.push_back({ LP_PILLAR, lm.tx + 2, lm.ty + 2, 1 });
        out.push_back({ LP_TORCH, lm.tx - 1, lm.ty - 2, 1 });
        out.push_back({ LP_TORCH, lm.tx + 1, lm.ty - 2, 1 });
        return;
    }
    bool isBossCastle = (lm.kind == LK_CASTLE && lm.tx == BOSS_CASTLE_TX && lm.ty == BOSS_CASTLE_TY);
    bool isStoryStones = (lm.kind == LK_STONES && lm.tx == LOCKET_SITE_TX && lm.ty == LOCKET_SITE_TY);
    bool isStoryTower = (lm.kind == LK_TOWER && lm.tx == TOWER_SITE_TX && lm.ty == TOWER_SITE_TY);

    switch (lm.kind) {
        case LK_CASTLE:
            out.push_back({ LP_CASTLE, lm.tx, lm.ty, 5 });
            if (isBossCastle) {
                out.push_back({ LP_PILLAR, lm.tx - 4, lm.ty + 2, 1 });
                out.push_back({ LP_PILLAR, lm.tx + 4, lm.ty + 2, 1 });
                out.push_back({ LP_PILLAR, lm.tx - 4, lm.ty + 5, 1 });
                out.push_back({ LP_PILLAR, lm.tx + 4, lm.ty + 5, 1 });
                out.push_back({ LP_TORCH,  lm.tx - 3, lm.ty + 3, 1 });
                out.push_back({ LP_TORCH,  lm.tx + 3, lm.ty + 3, 1 });
                out.push_back({ LP_TORCH,  lm.tx - 3, lm.ty + 6, 1 });
                out.push_back({ LP_TORCH,  lm.tx + 3, lm.ty + 6, 1 });
            } else {
                out.push_back({ LP_TORCH, lm.tx - 3, lm.ty + 1, 1 });
                out.push_back({ LP_TORCH, lm.tx + 3, lm.ty + 1, 1 });
            }
            break;
        case LK_TOWER:
            out.push_back({ LP_TOWER, lm.tx, lm.ty, 3 });
            if (isStoryTower) {
                out.push_back({ LP_RUIN_ARCH, lm.tx - 2, lm.ty + 2, 1 });
                out.push_back({ LP_BARREL,    lm.tx + 2, lm.ty,     1 });
                out.push_back({ LP_TORCH,     lm.tx + 2, lm.ty + 2, 1 });
            } else {
                out.push_back({ LP_TORCH, lm.tx + 2, lm.ty + 1, 1 });
            }
            break;
        case LK_STONES:
            out.push_back({ LP_STATUE, lm.tx, lm.ty, 1 });
            out.push_back({ LP_PILLAR, lm.tx - 3, lm.ty - 1, 1 });
            out.push_back({ LP_PILLAR, lm.tx + 3, lm.ty - 1, 1 });
            out.push_back({ LP_PILLAR, lm.tx - 2, lm.ty + 2, 1 });
            out.push_back({ LP_PILLAR, lm.tx + 2, lm.ty + 2, 1 });
            if (isStoryStones) {
                out.push_back({ LP_TORCH, lm.tx - 1, lm.ty - 2, 1 });
                out.push_back({ LP_TORCH, lm.tx + 1, lm.ty - 2, 1 });
            }
            break;
        case LK_RUINS:
            out.push_back({ LP_RUIN_ARCH, lm.tx - 2, lm.ty, 2 });
            out.push_back({ LP_MONUMENT,  lm.tx + 1, lm.ty, 2 });
            out.push_back({ LP_PILLAR,    lm.tx + 3, lm.ty - 1, 1 });
            break;
        case LK_CAMP:
            out.push_back({ LP_TENT,     lm.tx - 1, lm.ty - 1, 3 });
            out.push_back({ LP_CAMPFIRE, lm.tx,     lm.ty + 1, 1 });
            out.push_back({ LP_CAULDRON, lm.tx + 2, lm.ty,     1 });
            out.push_back({ LP_BARREL,   lm.tx - 3, lm.ty,     1 });
            break;
        case LK_CAVERN:
            out.push_back({ LP_CAVE, lm.tx, lm.ty, 3 });
            // A previous expedition's abandoned kit flanks the mouth.
            out.push_back({ LP_BARREL, lm.tx - 2, lm.ty + 1, 1 });
            out.push_back({ LP_TORCH,  lm.tx + 2, lm.ty + 1, 1 });
            break;
        default:
            break;
    }
}

static bool partBlocksCell(const LandmarkPart& p, int tx, int ty) {
    switch (p.part) {
        case LP_CASTLE:
            return tx >= p.tx - 2 && tx <= p.tx + 2 && ty >= p.ty - 3 && ty <= p.ty;
        case LP_TOWER:
            return tx >= p.tx - 1 && tx <= p.tx + 1 && ty >= p.ty - 1 && ty <= p.ty;
        case LP_CAVE:
            return ty == p.ty && tx >= p.tx - 1 && tx <= p.tx + 1;
        case LP_TENT:
            return tx >= p.tx - 1 && tx <= p.tx + 1 && ty >= p.ty - 1 && ty <= p.ty;
        case LP_RUIN_ARCH:
        case LP_MONUMENT:
            if (p.spanW == 2) return ty == p.ty && (tx == p.tx - 1 || tx == p.tx);
            return tx == p.tx && ty == p.ty;
        default:
            return tx == p.tx && ty == p.ty;
    }
}

static bool landmarkCellBlocked(int tx, int ty) {
    Landmark lm;
    if (!landmarkNear(tx, ty, lm)) return false;
    std::vector<LandmarkPart> parts;
    expandLandmarkParts(lm, parts);
    for (const auto& p : parts) {
        if (partBlocksCell(p, tx, ty)) return true;
    }
    return false;
}

// A tile a hull may occupy: open water inside the coast.
bool cellSailable(int tx, int ty) { return waterSailableAt(tx, ty); }

bool cellWalkable(int tx, int ty) {
    // The world is endless, so the terrain's ways must exist before anything is
    // judged blocked. Three sectors are surveyed on first touch of a region and
    // cached; the survey only ever opens a way, never closes one.
    wildWayEnsureFor((int)floorf((float)tx / WAY_SECTOR), (int)floorf((float)ty / WAY_SECTOR));
    if (g.villageLoaded) {
        for (const auto& rc : g.village.rocks) {
            if (rc.first == tx && rc.second == ty) return false;
        }
    }

    GroundTile gt = sampleGround(tx, ty);
    if (!groundWalkable(gt.kind)) return false;
    if (landmarkCellBlocked(tx, ty)) return false;

    PropKind p = propAt(tx, ty);
    if (p == P_TREE0 || p == P_TREE1 || p == P_TREE2 || p == P_PINE0 || p == P_PINE1 ||
        p == P_PINE2 || p == P_DEADTREE || p == P_TREE_SNOW || p == P_ROCK0 || p == P_ROCK1 ||
        p == P_ROCK2 || p == P_BOULDER || p == P_CRYSTAL || p == P_STUMP ||
        p == P_AMBER_OAK || p == P_COPPER_OAK || p == P_WILLOW)
        return false;

    if (g.villageLoaded) {
        for (const Building& b : g.village.buildings) {
            int x0 = buildingX0(b), y0 = buildingY0(b);
            if (tx >= x0 && tx < x0 + b.w && ty >= y0 && ty <= b.ty) return false;
        }
        for (const auto& d : g.village.decos) {
            if (!villageDecoSolid(d.kind)) continue;
            int dx = (int)floorf(d.x + 0.5f), dy = (int)floorf(d.y + 0.5f);
            if (d.kind == VD_WELL) {
                if ((ty == dy || ty == dy - 1) && (tx == dx || tx == dx - 1)) return false;
            } else if (d.kind == VD_FLOWERBED || d.kind == VD_BENCH || d.kind == VD_FENCE_GATE) {
                if (ty == dy && (tx == dx || tx == dx - 1)) return false;
            } else if (d.kind == VD_LONGTABLE || d.kind == VD_GATE) {
                if (ty == dy && tx >= dx - 1 && tx <= dx + 1) return false;
            } else if (d.kind == VD_STATUS_STONE) {
                // 30x44 stone drawn on the two tiles it stands between.
                if (ty == dy && (tx == dx || tx == dx - 1)) return false;
            } else {
                if (tx == dx && ty == dy) return false;
            }
        }
    }
    return true;
}

bool positionBlocked(float x, float y, float r) {
    const float off[4][2] = { { -r, -r }, { r, -r }, { -r, r }, { r, r } };
    for (auto& o : off) {
        int tx = (int)floorf(x + o[0]), ty = (int)floorf(y + o[1]);
        if (!cellWalkable(tx, ty)) return true;
    }
    return false;
}

// The map-piece layer (src/pieces.cpp) has to know which ground the older
// systems already claimed, without re-deriving trails and sites itself.

bool inVillage(float x, float y) {
    if (!g.villageLoaded) return false;
    float dx = x - g.village.cx, dy = y - g.village.cy;
    return dx * dx + dy * dy < 19.0f * 19.0f;
}
bool nearStartVillage(float x, float y) {
    return (x * x + y * y) < 30.0f * 30.0f;
}

// ================================================================ props
// SNES grove placement:
// - Broadleaf crowns (40x39 px) pack on a staggered 2-tile canopy lattice so
//   adjacent crowns overlap by 12-20 px into lush layered forest walls while
//   keeping every grove solid and grounded.
// - Narrow conifers (16x32 px) pack on a tight staggered weave so evergreens
//   stand shoulder-to-shoulder in tiered ranks instead of lonely toothpicks.
static bool broadleafSlot(int tx, int ty, float grove, float hf) {
    bool evenRow = ((ty & 1) == 0);
    int xm = ((tx % 2) + 2) % 2;
    if (evenRow && xm == 0) return hf < 0.88f;
    if (!evenRow && xm == 1 && grove > 0.56f) return hf < 0.78f;
    return false;
}

static bool coniferSlot(int tx, int ty, float grove, float hf) {
    int parity = ((tx + (ty & 1)) & 1);
    if (parity == 0) return hf < 0.90f;
    if (grove > 0.64f && (ty & 1) == 0) return hf < 0.52f;
    return false;
}

static PropKind propAtUncached(int tx, int ty);

// propAt is a pure function of the seed and the loaded village, but it costs a
// few microseconds (landmark and cavern surveys, two noise fields, the trail
// pass) and the terrain survey asks it again and again for the same tiles. A
// small direct-mapped cache keeps the survey cheap enough to run while walking.
struct PropCacheEntry { int x = -1, y = -1; uint32_t seed = 0; uint64_t village = ~0ull; PropKind kind = P_NONE; };
static PropCacheEntry gPropCache[1 << 14];
static uint64_t gPropCacheVillage() { return g.villageLoaded ? g.villageKey : ~0ull; }

PropKind propAt(int tx, int ty) {
    PropCacheEntry& c = gPropCache[((uint32_t)tx * 73856093u ^ (uint32_t)ty * 19349663u) & ((1u << 14) - 1)];
    uint64_t vk = gPropCacheVillage();
    if (c.x == tx && c.y == ty && c.seed == g.worldSeed && c.village == vk) return c.kind;
    PropKind k = propAtUncached(tx, ty);
    c = { tx, ty, g.worldSeed, vk, k };
    return k;
}

static PropKind propAtUncached(int tx, int ty) {
    if (g.villageLoaded && (inVillage((float)tx + 0.5f, (float)ty + 0.5f) || inCommons((float)tx + 0.5f, (float)ty + 0.5f)))
        return P_NONE;
    // A map-piece seam's treeline stands on its own band, and its gate stays
    // open. Anything the older generators authored into place outranks it.
    {
        PropKind seamProp = pieceBorderProp(tx, ty);
        if (seamProp != P_NONE) return seamProp;
        PropKind featureProp = pieceFeatureProp(tx, ty);
        if (featureProp != P_NONE) return featureProp;
    }
    // Passes and their approaches must stay clear even after a seal opens.
    if (regionCellAt(tx, ty).terrain == REGION_PASS || frontierRoadAt(tx, ty)) return P_NONE;

    // A wild way is a worn line through the terrain: keep it trodden, never
    // walled by a tree or boulder that happens to fall on it.
    if (onWildWay(tx, ty)) return P_NONE;

    // ...and the Survey's roads are trodden ground too.
    if (atlasRoadAt(tx, ty)) return P_NONE;

    // Sacred ring of ancient amber oaks framing the Standing Stones glade
    {
        int dx = tx - LOCKET_SITE_TX, dy = ty - LOCKET_SITE_TY;
        int r2 = dx * dx + dy * dy;
        if (r2 >= 30 && r2 <= 46 && !nearTrail(tx, ty, 1) && !(abs(dx) <= 2 && dy >= 2)) {
            if (((tx + ty) & 1) == 0 && hashf(tx, ty, g.worldSeed ^ 0x5704u) < 0.68f)
                return ((tx ^ ty) & 2) ? P_AMBER_OAK : P_COPPER_OAK;
        }
    }
    if (nearSpecialSite(tx, ty)) return P_NONE;
    Landmark surveyed;
    if (surveyedCavernNear(tx, ty, 6, surveyed)) return P_NONE;

    Landmark lm;
    if (proceduralLandmarkNear(tx, ty, 5, lm)) return P_NONE;
    {
        int cx, cy;
        if (startCavernSite(cx, cy) && abs(tx - cx) <= 4 && abs(ty - cy) <= 3) return P_NONE;
    }
    if (nearTrail(tx, ty, 1)) return P_NONE;

    GroundTile gt = sampleGround(tx, ty);
    if (gt.kind == G_DIRT || gt.kind == G_BRIDGE_NS || gt.kind == G_BRIDGE_EW ||
        gt.kind == G_COBBLE || gt.kind == G_FLOOR || gt.kind == G_SOIL ||
        gt.kind == G_WATER || gt.kind == G_DEEP || gt.kind == G_ICE || gt.kind == G_BEACH)
        return P_NONE;

    uint32_t h = hash2i(tx, ty, g.worldSeed ^ 0x77ee5u);
    float hf = (float)h / 4294967295.0f;
    float grove = canopyDensityAt(tx, ty, g.worldSeed);

    // Millbrook riverbank framing: weeping willows, reed clumps & river stones
    float brook = brookCenterAt((float)tx, g.worldSeed);
    float bank = fabsf((float)ty - brook);
    if (bank >= 2.15f && bank <= 3.85f && abs(tx) > 3) {
        if (bank < 2.95f && hf < 0.24f) return P_REEDS;
        if (bank >= 2.75f && broadleafSlot(tx, ty, 0.62f, hf) && hf < 0.28f) return P_WILLOW;
        if (hf > 0.965f) return P_ROCK0;
    }

    // Cliff-toe boulders & shrubs tucked cleanly against G_ROCK escarpments
    bool rockNorth = (sampleGround(tx, ty - 1).kind == G_ROCK);
    if (gt.kind != G_ROCK && rockNorth) {
        if (hf < 0.22f) return (PropKind)(P_ROCK0 + (h % 3));
        if (hf < 0.36f && gt.biome != BI_DUNES && gt.biome != BI_FROST) return P_BUSH_PLAIN;
    }

    switch (gt.kind) {
        case G_GRASS:
        case G_GRASS_DARK: {
            float thresh = (gt.biome == BI_FOREST) ? 0.42f : 0.57f;
            if (grove > thresh) {
                if (gt.biome == BI_FOREST && ((h >> 8) % 3) != 0) {
                    if (coniferSlot(tx, ty, grove, hf)) return (PropKind)(P_PINE0 + (h % 3));
                } else {
                    if (broadleafSlot(tx, ty, grove, hf)) return (PropKind)(P_TREE0 + (h % 3));
                }
                if (hf > 0.92f) return (gt.biome == BI_FOREST) ? P_MUSHROOM : P_BUSH_PLAIN;
                if (hf < 0.03f) return P_STUMP;
            } else if (grove > thresh - 0.085f) {
                // Lush undergrowth skirt hugging the outer edge of every copse
                if (hf < 0.16f) return ((h >> 5) & 1) ? P_BUSH : P_BUSH_PLAIN;
                if (hf > 0.955f) return (gt.biome == BI_FOREST) ? P_MUSHROOM : P_ROCK0;
                if (hf > 0.935f && hf <= 0.955f) return P_STUMP;
            } else {
                // Open sunlit glade: breathable space with rare solitary landmark oaks/stones
                if (hf < 0.006f && broadleafSlot(tx, ty, 0.5f, 0.1f)) return (PropKind)(P_TREE0 + (h % 3));
                if (hf > 0.992f) return P_ROCK0;
                if (hf > 0.984f && hf <= 0.992f) return P_BUSH;
            }
            break;
        }
        case G_AMBER:
        case G_LEAVES: {
            if (grove > 0.43f) {
                if (broadleafSlot(tx, ty, grove, hf)) return (h % 4) ? P_AMBER_OAK : P_COPPER_OAK;
                if (hf < 0.06f) return P_MUSHROOM;
                if (hf > 0.965f) return P_STUMP;
            } else if (grove > 0.34f) {
                if (hf < 0.13f) return P_BUSH_PLAIN;
                if (hf > 0.955f) return P_MUSHROOM;
                if (hf > 0.935f && hf <= 0.955f) return P_ROCK1;
            } else {
                if (hf < 0.006f && broadleafSlot(tx, ty, 0.5f, 0.1f)) return P_AMBER_OAK;
                if (hf > 0.990f) return P_STUMP;
            }
            break;
        }
        case G_SNOW: {
            if (grove > 0.47f) {
                if (coniferSlot(tx, ty, grove, hf)) return P_TREE_SNOW;
                if (hf > 0.96f) return P_ROCK2;
            } else if (grove > 0.38f) {
                if (hf < 0.07f) return P_ROCK2;
                if (hf > 0.975f) return P_CRYSTAL;
            } else if (hf > 0.991f) {
                return P_CRYSTAL;
            }
            break;
        }
        case G_HEATH: {
            if (grove > 0.50f) {
                if (coniferSlot(tx, ty, grove, hf)) return (h & 1) ? P_PINE2 : P_PINE1;
                if (hf > 0.94f) return P_ROCK2;
            } else if (grove > 0.40f) {
                if (hf < 0.10f) return (PropKind)(P_ROCK0 + (h % 3));
                if (hf > 0.95f) return P_BUSH_PLAIN;
            } else if (hf > 0.985f) {
                return P_BOULDER;
            }
            break;
        }
        case G_ROCK:
            if (hf < 0.055f) return P_BOULDER;
            if (hf > 0.985f) return P_CRYSTAL;
            break;
        case G_SAND:
        case G_RIPPLES:
            if (grove > 0.48f && hf < 0.09f) return P_YUCCA;
            if (hf > 0.984f) return P_ROCK1;
            break;
        case G_SWAMP:
            if (grove > 0.46f && broadleafSlot(tx, ty, grove, hf)) return (h % 3) ? P_WILLOW : P_DEADTREE;
            if (hf < 0.12f + grove * 0.10f) return P_REEDS;
            if (hf > 0.985f) return P_MUSHROOM;
            break;
        default: break;
    }
    return P_NONE;
}

Texture2D* propTexture(PropKind kind, int* outW, int* outH) {
    Texture2D* t = nullptr;
    switch (kind) {
        case P_TREE0: t = &g.assets.tree[0]; break;
        case P_TREE1: t = &g.assets.tree[1]; break;
        case P_TREE2: t = &g.assets.tree[2]; break;
        case P_PINE0: t = &g.assets.pine[0]; break;
        case P_PINE1: t = &g.assets.pine[1]; break;
        case P_PINE2: t = &g.assets.pine[2]; break;
        case P_DEADTREE: t = &g.assets.deadTree; break;
        case P_TREE_SNOW: t = &g.assets.treeSnow; break;
        case P_ROCK0: t = &g.assets.rock[0]; break;
        case P_ROCK1: t = &g.assets.rock[1]; break;
        case P_ROCK2: t = &g.assets.rock[2]; break;
        case P_BOULDER: t = &g.assets.boulder; break;
        case P_CRYSTAL: t = &g.assets.crystal; break;
        case P_BUSH: t = &g.assets.bush[0]; break;
        case P_BUSH_PLAIN: t = &g.assets.bush[1]; break;
        case P_MUSHROOM: t = &g.assets.mushroom; break;
        case P_PLANT: t = &g.assets.plant; break;
        case P_URN: t = &g.assets.urn; break;
        case P_AMBER_OAK: t = &g.assets.amberOak; break;
        case P_COPPER_OAK: t = &g.assets.copperOak; break;
        case P_WILLOW: t = &g.assets.willow; break;
        case P_REEDS: t = &g.assets.reeds; break;
        case P_YUCCA: t = &g.assets.yucca; break;
        case P_STUMP: t = &g.assets.stump; break;
        default: break;
    }
    if (t && outW) { *outW = t->width; *outH = t->height; }
    return t;
}

// ================================================================ village deco
void villageDecoTexture(int kind, int variant, Texture2D*& tex, int& w, int& h) {
    Texture2D* t = nullptr;
    switch (kind) {
        case VD_WELL: t = &g.assets.well; break;
        case VD_SIGN: t = &g.assets.sign; break;
        case VD_BENCH: t = &g.assets.bench; break;
        case VD_TORCH: t = &g.assets.torch; break;
        case VD_CAMPFIRE: t = &g.assets.campfire; break;
        case VD_BARREL: t = &g.assets.barrel; break;
        case VD_PLANT: t = &g.assets.plant; break;
        case VD_FLOWERBED: t = &g.assets.flowerbed; break;
        case VD_LADDER: t = &g.assets.ladder; break;
        case VD_STATUE: t = &g.assets.statue; break;
        case VD_CAULDRON: t = &g.assets.cauldron; break;
        case VD_URN: t = &g.assets.urn; break;
        case VD_FENCE_H: t = &g.assets.fence; break;
        case VD_FENCE_V: t = &g.assets.fencePost; break;
        case VD_FENCE_GATE: t = &g.assets.fenceGate; break;
        case VD_GATE: t = &g.assets.gate; break;
        case VD_TEMPLE_PILLAR: t = &g.assets.pillar; break;
        case VD_HOUSE: t = (variant >= 0 && variant < 5) ? &g.assets.house[variant] : &g.assets.house[0]; break;
        case VD_PINE: {
            Biome b = sampleGround(g.village.cx, g.village.cy).biome;
            t = (b == BI_FROST) ? &g.assets.treeSnow : &g.assets.pine[(variant >= 0 ? variant : 0) % 3];
            break;
        }
        case VD_SHADE_TREE: {
            switch (sampleGround(g.village.cx, g.village.cy).biome) {
                case BI_AMBER: t = (variant % 3) ? &g.assets.amberOak : &g.assets.copperOak; break;
                case BI_FOREST:
                case BI_HEATH: t = (variant & 1) ? &g.assets.tree[variant % 3] : &g.assets.pine[variant % 3]; break;
                case BI_FROST: t = &g.assets.treeSnow; break;
                case BI_FEN:   t = &g.assets.willow; break;
                case BI_DUNES: t = &g.assets.yucca; break;
                default:       t = &g.assets.tree[(variant >= 0 ? variant : 0) % 3]; break;
            }
            break;
        }
        case VD_CRATE: t = &g.assets.crate; break;
        case VD_TABLE: t = &g.assets.table; break;
        case VD_LONGTABLE: t = (g.assets.longtable.id != 0) ? &g.assets.longtable : &g.assets.table; break;
        case VD_BUCKET: t = &g.assets.bucket; break;
        case VD_MAILBOX: t = (g.assets.mailbox.id != 0) ? &g.assets.mailbox : &g.assets.sign; break;
        case VD_NOTICEBOARD: t = (g.assets.noticeboard.id != 0) ? &g.assets.noticeboard : &g.assets.sign; break;
        case VD_BUSH: t = &g.assets.bush[1]; break;
        case VD_FLOWER_BUSH: t = &g.assets.bush[0]; break;
        case VD_ROCK: t = &g.assets.rock[(variant >= 0 ? variant : 0) % 3]; break;
        case VD_STUMP: t = &g.assets.stump; break;
        case VD_STATUS_STONE: t = &g.assets.statusStone[(variant % 3 + 3) % 3]; break;
        case VD_EMPTY_PLOT: t = nullptr; break;   // a prepared footing: nothing stands here yet
        default: break;
    }
    tex = t;
    w = t ? t->width : 0;
    h = t ? t->height : 0;
}

// ================================================================ names & compass
static const char* NAME_A[] = {
    "Elm", "Bran", "Kes", "Dun", "Wych", "Tor", "Ash", "Raven", "Kar", "Sten",
    "Hol", "Grim", "Mar", "Bel", "Thorn", "Karn", "Ul", "Wyn", "Oak", "Silver"
};
static const char* NAME_B[] = {
    "wick", "hollow", "fell", "ford", "by", "stead", "mere", "dale", "hurst",
    "brook", "gate", "field", "wood", "shaw", "port", "haven", "crest", "moor"
};
std::string villageNameFor(int sx, int sy, uint32_t seed) {
    uint32_t h = hash2i(sx, sy, seed ^ 0x7ab1e5u);
    return std::string(NAME_A[h % 20]) + NAME_B[(h / 20) % 18];
}

static bool buildingsOverlap(const Building& a, const Building& b, int pad = 2) {
    int ax0 = buildingX0(a) - pad, ax1 = buildingX0(a) + a.w - 1 + pad;
    int ay0 = buildingY0(a) - pad, ay1 = a.ty + 1 + pad;
    int bx0 = buildingX0(b),       bx1 = buildingX0(b) + b.w - 1;
    int by0 = buildingY0(b),       by1 = b.ty + 1;
    return !(ax1 < bx0 || ax0 > bx1 || ay1 < by0 || ay0 > by1);
}

static bool cellReservedInVillage(const Village& v, int tx, int ty, int doorPad = 1) {
    for (const Building& b : v.buildings) {
        int x0 = buildingX0(b) - 1, x1 = buildingX0(b) + b.w;
        int y0 = buildingY0(b) - 1, y1 = b.ty + doorPad + 1;
        if (tx >= x0 && tx <= x1 && ty >= y0 && ty <= y1) return true;
    }
    auto inVec = [&](const std::vector<std::pair<int, int>>& vec) {
        return std::find(vec.begin(), vec.end(), std::make_pair(tx, ty)) != vec.end();
    };
    if (inVec(v.paths) || inVec(v.plaza) || inVec(v.court) || inVec(v.soil) || inVec(v.rocks))
        return true;
    GroundKind gk = sampleGround(tx, ty).kind;
    if (!groundWalkable(gk) || gk == G_DIRT || gk == G_BRIDGE_NS || gk == G_BRIDGE_EW || gk == G_SOIL)
        return true;
    for (const auto& d : v.decos) {
        int dx = (int)floorf(d.x + 0.5f), dy = (int)floorf(d.y + 0.5f);
        if (abs(tx - dx) <= 1 && abs(ty - dy) <= 1 && (d.kind == VD_WELL || d.kind == VD_LONGTABLE))
            return true;
        if (tx == dx && ty == dy) return true;
    }
    return false;
}

// Orthogonal / gently stepped 2-tile street builder (classic SNES town lanes, never jagged diagonals)
static void paveStreetL(std::vector<std::pair<int, int>>& paths, int x0, int y0, int x1, int y1, bool horizFirst) {
    auto add2 = [&](int x, int y, bool ns) {
        paths.push_back({ x, y });
        if (ns) paths.push_back({ x + 1, y });
        else    paths.push_back({ x, y + 1 });
    };
    if (horizFirst) {
        int sx = (x0 <= x1) ? 1 : -1;
        for (int x = x0; ; x += sx) {
            add2(x, y0, false);
            if (x == x1) break;
        }
        int sy = (y0 <= y1) ? 1 : -1;
        for (int y = y0; ; y += sy) {
            add2(x1, y, true);
            if (y == y1) break;
        }
    } else {
        int sy = (y0 <= y1) ? 1 : -1;
        for (int y = y0; ; y += sy) {
            add2(x0, y, true);
            if (y == y1) break;
        }
        int sx = (x0 <= x1) ? 1 : -1;
        for (int x = x0; ; x += sx) {
            add2(x, y1, false);
            if (x == x1) break;
        }
    }
}

static void dedupCells(std::vector<std::pair<int, int>>& cells) {
    std::sort(cells.begin(), cells.end());
    cells.erase(std::unique(cells.begin(), cells.end()), cells.end());
}

void buildStartVillage(Village& v, uint32_t seed) {
    v = Village{};
    v.valid = true;
    v.startVillage = true;
    v.cx = 0; v.cy = 0;
    v.name = "Elmswick";
    v.seed = seed ^ 0x5171u;

    auto addBuilding = [&](int kind, int tx, int ty) {
        Building b;
        b.kind = kind;
        b.tx = tx; b.ty = ty;
        b.w = (kind >= 5) ? 5 : 4;   // shop/inn/temple/manor = 5 tiles (70-78px), cottages = 4 tiles (64px)
        b.h = (kind == 7) ? 4 : 3;
        b.sprite = kind;
        v.buildings.push_back(b);
    };

    // Asymmetric, zoned SNES town districts:
    addBuilding(5, -8, -1);     // 0: Mira's Sundries (Market West)       [-10..-6, -3..-1], door (-8, 0)
    addBuilding(6,  8,  1);     // 1: The Sleeping Stag Inn (East Patio)  [  6..10, -1.. 1], door ( 8, 2)
    addBuilding(7, -1, -10);    // 2: Chapel of the Meadow (North Ridge)  [ -3.. 1,-13..-10], door (-1,-9)
    addBuilding(8, -9, -7);     // 3: Elder's Hall / Field Office (NW)    [-11..-7, -9..-7], door (-9,-6)
    addBuilding(1,  9, -6);     // 4: Artisan's Cottage (NE Terrace)      [  7..10, -8..-6], door ( 9,-5)
    addBuilding(2, -12, 4);     // 5: Weaver's Cottage (West Lane)        [-14..-11, 2.. 4], door (-12,5)
    addBuilding(4, -5,  8);     // 6: Farmer Hedrick's Farmhouse (SW)     [ -7..-4,  6.. 8], door (-5, 9)
    addBuilding(3,  6,  8);     // 7: Shepherd's Cottage (SE Pasture)     [  4.. 7,  6.. 8], door ( 6, 9)
    // The expansion forms three small neighbourhoods, not infill squeezed
    // between roof silhouettes. Leave daylight around the chapel and hall.
    addBuilding(10, -15, -3);   // 8: Forge and work yard, west gate
    addBuilding(0,   14, -7);   // 9: Cooper's cottage, east lane
    addBuilding(2,  -15,-11);   // 10: Ridge cottage, below the western bluff
    addBuilding(3,    6,-11);   // 11: Chapel-side cottage, northeast terrace
    addBuilding(1,   14,  5);   // 12: Orchard cottage, above the paddock

    // Cut-stone sanctuary terrace in front of the Chapel
    for (int ty = -9; ty <= -7; ty++)
        for (int tx = -3; tx <= 1; tx++)
            v.court.push_back({ tx, ty });

    // Stepped cobbled Town Square around the central Well
    for (int ty = -4; ty <= 4; ty++) {
        for (int tx = -4; tx <= 4; tx++) {
            if (abs(tx) + abs(ty) <= 6 || (abs(tx) <= 2 && abs(ty) <= 4))
                v.plaza.push_back({ tx, ty });
        }
    }

    // Orthogonal & gently stepped 2-tile dirt lanes linking every doorway & all 4 gates
    paveStreetL(v.paths, -16, 0, -4, 0, true);     // West Mirefen Gate -> Market & Plaza
    paveStreetL(v.paths, -9, -6, -9, 0, false);    // Elder's Hall -> Market Lane
    paveStreetL(v.paths, -13, 1, -13, 5, false);   // Market Lane -> Weaver's Cottage doorstep
    paveStreetL(v.paths, 4, 2, 16, 2, true);       // Plaza -> Inn doorstep & East Woodland Lane
    paveStreetL(v.paths, 16, 2, 19, 0, false);     // Connect East Woodland Lane cleanly to East Highway (y=0)
    paveStreetL(v.paths, 8, -5, 8, 2, false);      // Artisan's Cottage -> Inn Lane
    paveStreetL(v.paths, -2, -7, -2, -4, false);   // Chapel Sanctuary Steps -> Plaza
    paveStreetL(v.paths, 1, -8, 3, -15, true);     // North Road curving east of Chapel
    paveStreetL(v.paths, 3, -15, 0, -16, true);    // North Road gateway alignment (x = 0..1)
    paveStreetL(v.paths, 0, 4, 0, 14, false);      // South Millbrook Highway -> Bridge
    paveStreetL(v.paths, -8, 9, 0, 9, true);       // Hedrick's Farmhouse & Turnip Croft Gate -> South Road
    paveStreetL(v.paths, 1, 9, 8, 9, true);        // South Road -> Shepherd's Cottage & Sheep Paddock Gate
    // Neighbourhood lanes keep through-traffic out of private courtyards.
    paveStreetL(v.paths, -15, -2, -15, 0, false);
    paveStreetL(v.paths, -15,-10, -12,-10, true);
    paveStreetL(v.paths, -12,-10, -12, 0, false);
    paveStreetL(v.paths,   6,-10,   3, -8, true);
    paveStreetL(v.paths,  14, -6,  17, -6, true);
    paveStreetL(v.paths,  17, -6,  17,  2, false);
    paveStreetL(v.paths,  14,  6,  17,  6, true);
    paveStreetL(v.paths,  17,  6,  17,  2, false);

    // Doorstep welcome aprons
    for (const Building& b : v.buildings) {
        int dtx = buildingDoorTx(b), dty = buildingDoorTy(b);
        v.paths.push_back({ dtx, dty });
        v.paths.push_back({ dtx - 1, dty });
    }

    // Stepped northern hillside cliff ridge behind the Chapel & Elder's Hall (3D crags via relief.png!)
    auto addCliff = [&](int x0, int x1, int y0, int y1) {
        for (int y = y0; y <= y1; y++)
            for (int x = x0; x <= x1; x++)
                v.rocks.push_back({ x, y });
    };
    addCliff(-14, -7, -14, -13);
    addCliff(-6,  -4, -15, -14);
    addCliff(-4,   1, -16, -15);
    addCliff( 6,  13, -13, -12);

    // Hedrick's tilled turnip garden
    for (int ty = 9; ty <= 11; ty++)
        for (int tx = -13; tx <= -9; tx++)
            v.soil.push_back({ tx, ty });

    auto deco = [&](int kind, int x, int y, int variant = 0) {
        int left = x - ((kind == VD_FLOWERBED || kind == VD_BENCH || kind == VD_WELL || kind == VD_LONGTABLE) ? 1 : 0);
        int right = x + (kind == VD_LONGTABLE ? 1 : 0);
        for (const auto& b : v.buildings)
            if (right >= buildingX0(b) && left < buildingX0(b) + b.w &&
                y >= buildingY0(b) - 1 && y <= b.ty) return;
        if (villageDecoSolid(kind))
            for (int tx = left; tx <= right; ++tx)
                if (std::find(v.paths.begin(), v.paths.end(), std::make_pair(tx, y)) != v.paths.end()) return;
        v.decos.push_back({ kind, (float)x, (float)y, variant });
    };

    // --- Central Square setpiece
    deco(VD_WELL, 0, 0);                    // 2x2 well at [-1..0, -1..0]
    deco(VD_BUCKET, 1, 0);                  // wooden water basin right beside well
    deco(VD_BENCH, -2, -3);                 // NW plaza bench [-3..-2, -3] under shade oak
    deco(VD_BENCH,  3, -2);                 // E plaza bench  [ 2.. 3, -2]
    deco(VD_SHADE_TREE, -4, -4, 0);         // big plaza shade oak
    deco(VD_SHADE_TREE,  4,  4, 2);         // SE plaza shade oak
    deco(VD_NOTICEBOARD, 2,  4);            // town noticeboard at south square exit
    deco(VD_TORCH, -3,  3);                 // warm plaza lanterns
    deco(VD_TORCH,  3,  3);

    // --- Chapel of the Meadow forecourt
    deco(VD_TEMPLE_PILLAR, -4, -9);
    deco(VD_TEMPLE_PILLAR,  2, -9);
    deco(VD_FLOWERBED, -4, -7);             // [-5..-4, -7]
    deco(VD_FLOWERBED,  3, -7);             // [ 2.. 3, -7]
    deco(VD_TORCH, -3, -6);
    deco(VD_TORCH,  0, -6);
    deco(VD_STATUE, -5, -10);

    // --- Market & Elder's Hall (NW)
    deco(VD_BARREL, -11, -2);
    deco(VD_CRATE,  -11, -1);
    deco(VD_PLANT,   -5, -1);
    deco(VD_SIGN,    -5,  1);
    deco(VD_MAILBOX, -6, -6);
    deco(VD_FLOWERBED, -11, -6);            // [-12..-11, -6]
    deco(VD_TORCH,  -10, -5);

    // --- Weaver's Cottage (W)
    deco(VD_BARREL,  -15,  4);
    deco(VD_PLANT,   -10,  4);
    deco(VD_MAILBOX, -10,  5);

    // --- Artisan's Cottage & Sleeping Stag Inn Patio (NE / E)
    deco(VD_STUMP,    6, -6);
    deco(VD_MAILBOX, 11, -5);
    deco(VD_FLOWERBED, 12, -6);             // [11..12, -6]
    deco(VD_TORCH,    5,  1);
    deco(VD_BARREL,  11,  0);
    deco(VD_CRATE,   12,  0);
    deco(VD_LONGTABLE, 13, 1);              // 3-tile outdoor tavern table [12..14, 1]
    deco(VD_BENCH,   13,  0);               // [12..13, 0]

    // --- SW: Farmer Hedrick's Farmhouse & Complete Fenced Turnip Croft (walkable gate at -8, 9..10)
    deco(VD_BARREL,  -3,  8);
    deco(VD_MAILBOX, -3,  9);
    deco(VD_FENCE_V, -14, 8);
    for (int x = -13; x <= -9; x++) deco(VD_FENCE_H, x, 8);
    deco(VD_FENCE_V, -8,  8);
    for (int y = 9; y <= 12; y++) deco(VD_FENCE_V, -14, y);
    for (int x = -13; x <= -9; x++) deco(VD_FENCE_H, x, 12);
    deco(VD_FENCE_V, -8, 12);
    deco(VD_FENCE_V, -8, 11);               // leaves (-8, 9..10) open as yard entrance!
    deco(VD_BUCKET,  -9, 11);

    // --- SE: Shepherd's Cottage & Complete Fenced Sheep Paddock (walkable gate at 9, 9..10)
    deco(VD_PLANT,    3,  8);
    deco(VD_MAILBOX,  8,  8);
    deco(VD_FENCE_V,  9,  8);
    for (int x = 10; x <= 13; x++) deco(VD_FENCE_H, x, 8);
    deco(VD_FENCE_V, 14,  8);
    for (int y = 9; y <= 13; y++) deco(VD_FENCE_V, 14, y);
    deco(VD_FENCE_V,  9, 13);
    for (int x = 10; x <= 13; x++) deco(VD_FENCE_H, x, 13);
    deco(VD_FENCE_V,  9, 11);
    deco(VD_FENCE_V,  9, 12);               // leaves (9, 9..10) open as pasture gate!
    deco(VD_SHADE_TREE, 13, 9, 1);          // shade oak inside sheep paddock

    // --- South Millbrook Bridge Gateposts & Sign
    deco(VD_SIGN,    2, 11);
    deco(VD_TORCH,  -1, 13);
    deco(VD_TORCH,   2, 13);
    deco(VD_FENCE_V,-2, 13);
    deco(VD_FENCE_V, 3, 13);

    // --- Village 2.0: the quiet dead in the eastern woodland belt
    deco(VD_URN,     15, 6);
    deco(VD_URN,     17, 6);
    deco(VD_URN,     15, 8);
    deco(VD_URN,     17, 8);
    deco(VD_STATUE,  16, 7);
    deco(VD_FLOWERBED, 16, 10);
    deco(VD_TORCH,   14, 7);

    // Forge working yard: stock and water off the public lane.
    deco(VD_CRATE, -18, -2);
    deco(VD_BARREL, -18, -1);
    deco(VD_BUCKET, -13, -2);
    deco(VD_SIGN, -13, 0);

    // --- Market quarter trestles by the west gate lane
    deco(VD_TABLE,   -11, 1);
    deco(VD_CRATE,   -12, 2);
    deco(VD_BARREL,   -6,  2);
    deco(VD_BENCH,     5,  3);
    deco(VD_FLOWERBED, 6, -3);

    // --- Orchard rows & garden corners
    for (int x = 6; x <= 8; x += 2) {
        if (!cellReservedInVillage(v, x, 12, 1)) {
            v.grassDark.push_back({ x, 12 });
            deco(VD_SHADE_TREE, x, 12, (x >> 1) % 3);
        }
    }

    // --- Dense SNES Perimeter Woodland Belt & Shaded Turf (frames the entire town!)
    // Leaves crisp gateways for North (x=0..4, y<=-14), West (y=0..1), East (y=2..3), and South (x=-1..2, y>=12)
    for (int ty = -17; ty <= 15; ty++) {
        for (int tx = -18; tx <= 18; tx++) {
            float r = sqrtf((float)(tx * tx + ty * ty));
            // Keep Millbrook stream & banks natural on the south edge
            float brook = brookCenterAt((float)tx, seed);
            if ((float)ty > brook - 2.6f) continue;

            // Road gateway corridors & fenced farmyards stay open and sunny
            if (tx >= -1 && tx <= 5 && ty <= -12) continue;   // North sanctuary & road
            if (tx <= -13 && ty >= -1 && ty <= 2) continue;   // West Mirefen gate
            if (tx >= 13 && ty >= -1 && ty <= 4) continue;    // East woodland lane & highway merge
            if (tx >= -2 && tx <= 3 && ty >= 10) continue;    // South bridge approach
            if (tx >= -15 && tx <= -7 && ty >= 7 && ty <= 13) continue; // SW Turnip Croft
            if (tx >= 8 && tx <= 15 && ty >= 7 && ty <= 14) continue;   // SE Sheep Paddock

            bool inWoodBelt = (r >= 13.6f && r <= 18.6f) ||
                              (ty <= -11 && abs(tx) >= 5 && r <= 18.6f) ||
                              (abs(tx) >= 15 && ty >= -10 && ty <= 12);
            if (!inWoodBelt) continue;
            if (cellReservedInVillage(v, tx, ty, 1)) continue;

            v.grassDark.push_back({ tx, ty });

            uint32_t h = hash2i(tx, ty, v.seed ^ 0x7711u);
            float hf = (float)(h & 0xffff) / 65536.0f;
            if (ty <= -9) {
                // Northern hillside: tiered evergreen pines + oaks + boulders
                if (((tx + (ty & 1)) & 1) == 0 && hf < 0.82f)
                    deco((h & 3) ? VD_PINE : VD_SHADE_TREE, tx, ty, (int)(h % 3));
                else if (hf > 0.90f)
                    deco(VD_ROCK, tx, ty, (int)(h % 3));
                else if (hf < 0.20f)
                    deco(VD_BUSH, tx, ty);
            } else {
                // Lush meadow-edge broadleaf woods & flowering hedgerows
                bool slot = (((tx & 1) == 0 && (ty & 1) == 0) ||
                             ((tx & 1) == 1 && (ty & 1) == 1 && r > 15.2f));
                if (slot && hf < 0.84f) {
                    deco((ty < -3 && (h % 4 == 0)) ? VD_PINE : VD_SHADE_TREE, tx, ty, (int)(h % 3));
                } else if (hf < 0.18f) {
                    deco((h & 1) ? VD_FLOWER_BUSH : VD_BUSH, tx, ty);
                } else if (hf > 0.94f) {
                    deco((h & 1) ? VD_STUMP : VD_ROCK, tx, ty, (int)(h % 3));
                } else if (hf > 0.72f) {
                    v.flowers.push_back({ tx, ty });
                }
            }
        }
    }

    // Interior garden shade trees & wildflower accents tucked beside cottages and fences
    const std::pair<int, int> innerTrees[] = {
        { -6, -9 }, { -13, -10 }, { 5, -10 }, { 12, -9 },
        { -15, 6 }, { -12, 13 }, { 12, 14 },
        { -16, -4 }, { 16, -2 }, { -4, -13 }, { 7, -13 }, { 11, 7 }
    };
    for (auto [tx, ty] : innerTrees) {
        if (!cellReservedInVillage(v, tx, ty, 1)) {
            v.grassDark.push_back({ tx, ty });
            v.grassDark.push_back({ tx - 1, ty });
            deco(VD_SHADE_TREE, tx, ty, (abs(tx) + abs(ty)) % 3);
        }
    }
    const std::pair<int, int> flowerSpots[] = {
        { -6, -5 }, { -4, -8 }, { 2, -8 }, { 6, -5 }, { 11, -4 },
        { -11, 5 }, { -4, 9 }, { 8, 7 }, { 10, 10 }, { 12, 12 },
        { -3, 12 }, { 3, 12 },
        { -16, -6 }, { -12, -8 }, { 6, -14 }, { 15, 4 }, { -7, 3 }, { 12, -5 }
    };
    for (auto [tx, ty] : flowerSpots) {
        if (!cellReservedInVillage(v, tx, ty, 0)) v.flowers.push_back({ tx, ty });
    }

    auto insideBuilding = [&](const std::pair<int, int>& cell) {
        for (const auto& b : v.buildings)
            if (cell.first >= buildingX0(b) && cell.first < buildingX0(b) + b.w &&
                cell.second >= buildingY0(b) && cell.second <= b.ty) return true;
        return false;
    };
    v.paths.erase(std::remove_if(v.paths.begin(), v.paths.end(), insideBuilding), v.paths.end());
    v.rocks.erase(std::remove_if(v.rocks.begin(), v.rocks.end(), insideBuilding), v.rocks.end());

    dedupCells(v.paths);
    dedupCells(v.plaza);
    dedupCells(v.court);
    dedupCells(v.grassDark);
    dedupCells(v.soil);
    dedupCells(v.flowers);
    dedupCells(v.rocks);

    // The Seed: the Agents' Commons on the meadow east of the village. It is
    // stamped last so it owns its ground and nothing overwrites its floor; its
    // content (which stones stand, which plots wait) comes from the chronicle
    // table in commons.cpp, never from here.
    commonsBuild(v);
    dedupCells(v.paths);
    dedupCells(v.plaza);
    dedupCells(v.court);
    dedupCells(v.grassDark);
    dedupCells(v.flowers);
    dedupCells(v.rocks);
}

// Route a two-cell-wide lane around architecture. Stamping Ls through whatever
// happened to occupy a row made generated settlements look like accidental infill.
static void routeVillageLane(Village& v, int tx, int ty) {
    constexpr int W = 41;
    int ox = v.cx - 20, oy = v.cy - 20;
    auto index = [&](int x, int y) { return (y - oy) * W + x - ox; };
    auto free = [&](int x, int y) {
        if (x < ox || y < oy || x >= ox + W - 1 || y >= oy + W - 1) return false;
        for (const auto& b : v.buildings)
            if (x + 1 >= buildingX0(b) && x < buildingX0(b) + b.w &&
                y + 1 >= buildingY0(b) - 1 && y <= b.ty) return false;
        return true;
    };
    int sx = v.cx + 1, sy = v.cy + 1;
    if (!free(sx, sy) || !free(tx, ty)) return;
    std::vector<int> cost(W * W, 1000000), prev(W * W, -1);
    std::vector<uint8_t> road(W * W, 0);
    for (auto cell : v.paths)
        if (cell.first >= ox && cell.first < ox + W && cell.second >= oy && cell.second < oy + W)
            road[index(cell.first, cell.second)] = 1;
    using Node = std::pair<int, int>;
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> queue;
    int start = index(sx, sy), goal = index(tx, ty);
    cost[start] = 0; queue.push({0, start});
    const int dirs[4][2] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
    while (!queue.empty()) {
        auto [distance, current] = queue.top(); queue.pop();
        if (distance != cost[current]) continue;
        if (current == goal) break;
        int x = current % W + ox, y = current / W + oy;
        for (const auto& dir : dirs) {
            int nx = x + dir[0], ny = y + dir[1];
            if (!free(nx, ny)) continue;
            int next = index(nx, ny), weight = road[next] ? 1 : 4;
            if (distance + weight >= cost[next]) continue;
            cost[next] = distance + weight; prev[next] = current;
            queue.push({cost[next], next});
        }
    }
    if (goal != start && prev[goal] < 0) return;
    for (int at = goal; ; at = prev[at]) {
        int x = at % W + ox, y = at / W + oy;
        for (int dy = 0; dy < 2; ++dy)
            for (int dx = 0; dx < 2; ++dx) v.paths.emplace_back(x + dx, y + dy);
        if (at == start) break;
    }
}

// Generated villages --------------------------------------------------------
static void layoutVillage(Village& v) {
    uint32_t rs = v.seed;
    v.buildings.clear();
    v.plaza.clear();
    v.paths.clear();
    v.court.clear();
    v.grassDark.clear();
    v.soil.clear();
    v.flowers.clear();
    v.rocks.clear();
    v.decos.clear();

    int plan = (int)(rs % 3);
    int side = (rs & 8) ? 1 : -1;

    // 5-tile wide service buildings (shop, inn, temple, forge) + staggered cottages
    Building shop{ 5, v.cx + 7 * side, v.cy + (plan == 0 ? 0 : -2), 5, 3, 5 };
    Building inn{ 6, v.cx - 7 * side, v.cy + (plan == 1 ? 3 : 1), 5, 3, 6 };
    Building temple{ 7, v.cx + (plan == 2 ? -2 * side : 0), v.cy - 9, 5, 4, 7 };
    v.buildings.push_back(shop);
    v.buildings.push_back(inn);
    v.buildings.push_back(temple);

    // A real service quarter gets first claim on space; houses fill around it.
    Building smithy{10, v.cx - 6 * side, v.cy + 7, 5, 3, 10};
    v.buildings.push_back(smithy);

    const std::pair<int, int> slots[] = {
        { -9 * side, -6 }, { 9 * side, -6 }, { -10 * side, 6 },
        { 8 * side, 7 },   { -4 * side, 8 }, { 4 * side, 9 },
        { -6 * side, -7 }, { 11 * side, -3 }, { -12 * side, -3 },
        { 6 * side, -8 }
    };
    int targetHouses = 4 + (int)(rs % 4);
    for (int i = 0; i < 10 && (int)v.buildings.size() < 4 + targetHouses; i++) {
        int jx = (int)(hash2i(i, 17, rs) % 3) - 1;
        int jy = (int)(hash2i(i, 29, rs) % 2);
        Building b;
        b.w = 4; b.h = 3;
        b.tx = v.cx + slots[i].first + jx;
        b.ty = v.cy + slots[i].second + jy;
        b.kind = (int)(hash2i(i, 31, rs) % 5);
        b.sprite = b.kind;
        if (abs(b.tx - v.cx) <= 5 && abs(b.ty - v.cy) <= 4) continue;
        bool bad = false;
        for (const auto& ex : v.buildings) {
            if (buildingsOverlap(b, ex, 2)) { bad = true; break; }
        }
        if (!bad) v.buildings.push_back(b);
    }


    // Temple forecourt flagstones
    for (int y = temple.ty + 1; y <= temple.ty + 2; y++)
        for (int x = buildingX0(temple); x < buildingX0(temple) + temple.w; x++)
            v.court.push_back({ x, y });

    // Central cobbled square
    for (int y = -3; y <= 3; ++y) {
        for (int x = -4; x <= 4; ++x) {
            bool paved = (plan == 0) ? (abs(y) <= 2 && abs(x) <= 4) :
                         (plan == 1) ? (abs(x) + abs(y) <= 5) :
                                       (x * x / 16.0f + y * y / 9.0f <= 1.05f);
            if (paved) v.plaza.emplace_back(v.cx + x, v.cy + y);
        }
    }

    // Shared lanes bend around full roof envelopes and service courtyards.
    for (const auto& b : v.buildings)
        routeVillageLane(v, buildingDoorTx(b) - 1, buildingDoorTy(b));
    routeVillageLane(v, v.cx, v.cy + 16);
    routeVillageLane(v, v.cx - 16, v.cy);
    routeVillageLane(v, v.cx + 16, v.cy);
    routeVillageLane(v, v.cx, v.cy - 16);

    auto tryDeco = [&](int kind, int x, int y, int variant = 0) {
        if (cellReservedInVillage(v, x, y, 1)) return;
        if (kind == VD_WELL || kind == VD_BENCH || kind == VD_FLOWERBED) {
            if (cellReservedInVillage(v, x - 1, y, 1)) return;
        }
        v.decos.push_back({ kind, (float)x, (float)y, variant });
    };

    // Well at center + benches & sign tucked at plaza corners
    v.decos.push_back({ VD_WELL, (float)v.cx, (float)v.cy, 0 });
    v.decos.push_back({ VD_BUCKET, (float)(v.cx + 1), (float)v.cy, 0 });
    tryDeco(VD_SIGN, v.cx + 2, v.cy + 4);
    tryDeco(VD_BENCH, v.cx - 2, v.cy - 3);
    tryDeco(VD_TORCH, v.cx - 3, v.cy + 3);
    tryDeco(VD_TORCH, v.cx + 3, v.cy + 3);

    // Contextual props tucked right beside each building wall
    for (size_t i = 0; i < v.buildings.size(); i++) {
        const Building& b = v.buildings[i];
        int lx = buildingX0(b) - 1;
        int rx = buildingX0(b) + b.w;
        if (b.kind == 5) {
            tryDeco(VD_CRATE, lx, b.ty);
            tryDeco(VD_BARREL, lx, b.ty - 1);
        } else if (b.kind == 6) {
            tryDeco(VD_BARREL, rx, b.ty);
            tryDeco(VD_TORCH, lx, b.ty + 1);
        } else if (b.kind == 7) {
            tryDeco(VD_TEMPLE_PILLAR, lx + 1, b.ty + 1);
            tryDeco(VD_TEMPLE_PILLAR, rx - 1, b.ty + 1);
        } else {
            tryDeco((i & 1) ? VD_MAILBOX : VD_PLANT, rx, b.ty + 1);
            tryDeco(VD_BARREL, lx, b.ty);
        }
    }

    // Perimeter woodland belt framing the generated village
    for (int dy = -17; dy <= 17; dy++) {
        for (int dx = -17; dx <= 17; dx++) {
            float r = sqrtf((float)(dx * dx + dy * dy));
            if (r < 12.8f || r > 18.2f) continue;
            if (abs(dx) <= 2 && dy >= 10) continue;   // South gate
            if (abs(dy) <= 2 && abs(dx) >= 10) continue; // E/W gates
            int tx = v.cx + dx, ty = v.cy + dy;
            if (cellReservedInVillage(v, tx, ty, 1)) continue;

            v.grassDark.push_back({ tx, ty });
            uint32_t h = hash2i(dx, dy, rs ^ 0x3391u);
            float hf = (float)(h & 0xffff) / 65536.0f;
            if (((dx & 1) == 0 && (dy & 1) == 0) && hf < 0.78f)
                v.decos.push_back({ VD_SHADE_TREE, (float)tx, (float)ty, (int)(h % 3) });
            else if (hf < 0.16f)
                v.decos.push_back({ (h & 1) ? VD_BUSH : VD_FLOWER_BUSH, (float)tx, (float)ty, 0 });
            else if (hf > 0.94f)
                v.decos.push_back({ VD_ROCK, (float)tx, (float)ty, (int)(h % 3) });
            else if (hf > 0.75f)
                v.flowers.push_back({ tx, ty });
        }
    }

    dedupCells(v.paths);
    dedupCells(v.plaza);
    dedupCells(v.court);
    dedupCells(v.grassDark);
    dedupCells(v.flowers);
}

void villageLoadAt(float x, float y) {
    if (nearStartVillage(x, y)) {
        if (g.villageLoaded && g.village.startVillage) return;
        buildStartVillage(g.village, g.worldSeed);
        g.villageLoaded = true;
        g.villageKey = 0x5157454cu;
        return;
    }

    // The Survey guarantees one settlement per 112-tile lattice cell (atlas.hpp),
    // so this only has to ask which of the nearby ones the party has walked up to.
    const int TRIGGER = 52;
    AtlasSite site;
    if (!atlasVillageNear((int)floorf(x), (int)floorf(y), TRIGGER, site)) {
        g.villageLoaded = false;
        g.villageKey = ~0ull;
        return;
    }

    uint64_t key = ((uint64_t)(uint32_t)(site.cellX + 4096) << 32) | (uint32_t)(site.cellY + 4096);
    if (g.villageLoaded && g.villageKey == key) return;

    Village v;
    v.valid = true;
    v.startVillage = false;
    v.cx = site.tx;
    v.cy = site.ty;
    v.sectorX = site.cellX;
    v.sectorY = site.cellY;
    v.seed = site.seed;
    v.name = villageNameFor(site.cellX, site.cellY, g.worldSeed);
    layoutVillage(v);
    g.village = v;
    g.villageKey = key;
    g.villageLoaded = true;
}

// ================================================================ npcs
static const char* VILLAGER_LINES[] = {
    "The wilds hum tonight. The trees keep secrets that aren't yours.",
    "They say a castle waits straight up the North Road. The Slime King sits on Elm's old throne.",
    "Rest well, traveller. The meadow feeds the bold and the foolish alike.",
    "I heard bells under the hills last winter. Beautiful, terrible bells.",
    "Take the left fork at the old pine. The right fork bites.",
    "Every map in this house is a lie. Beautiful, useless lies.",
    "The slimes are thick in the south meadow. Steady work for a sword.",
    "My grandmother sold potions to a hero once. He never came back for the change.",
    "Stone mouths open in the hills if you know where to look. Mind what goes in before you follow it down.",
    "Nell at the forge pays well for star metal. Says the deep vaults still hold it.",
    "The old road west passes hollows that breathe. Torchlight and a level blade, that's the rule.",
    "Caverns keep their own weather. Down there it's always autumn, and always dark.",
};
static const char* ELDER_LINES[] = {
    "You have the look of someone the road has been waiting for.",
    "Old kings built towers to be remembered. The towers remember them better than we do.",
    "When the moon is fat, the wisps walk the frozen reach. Keep your ether close.",
    "Listen: under the hills west of here, older halls than ours still stand. Bring light.",
};
static const char* CHILD_LINES[] = {
    "Are you an adventurer? Do you have a SWORD? Can I see it?",
    "I caught a slime once! It was mostly jelly. Mum made me put it back.",
    "The well echoes if you shout into it. The echo knows your name.",
    "The bad caves are the ones that echo back when you hum.",
};
static const char* GUARD_LINES[] = {
    "Keep your blade sheathed in town and we'll get along fine.",
    "North road climbs to the old castle. West lies a ruined watchtower.",
    "West of the causeway the ground opens into hollows. Nobody posts a guard at a hole in the earth.",
};
static const char* SHOP_LINES[] = {
    "Steel, salves and sundries. No credit, no haggling, no exceptions.",
};
static const char* INN_LINES[] = {
    "A warm bed and a hot meal. You look like you need both.",
};
static const char* PRIEST_LINES[] = {
    "The light mends what the road breaks. Rest a moment, child.",
};
static const char* ANIMAL_LINES[] = {
    "Woof! *wag*", "Arf! *sniffs your boots*", "*happy tail thump*", "Woof!",
};
static const char* SHEEP_LINES[] = {
    "Baa! Her wool smells of clover and warm meadow sun.",
    "Maaa... She munches a buttercup without losing rhythm.",
};
static const char* QUEST_LINES[] = {
    "Those slimes are eating my turnips. Thin them out and I'll make it worth your while.",
};
// The Commons keeper speaks a stone at a time; the chronicle table itself
// holds the real content (src/commons.cpp) and the full briefing lives in
// overworld.cpp, where the quest actions are resolved.
static const char* KEEPER_LINES[] = {
    "Mind the ring. Those stones are the only full record of what was built here.",
    "Each stone has a name, and under the name the thing itself: a road, a song, a cellar.",
    "The empty plots matter more than the stones. They are the ones still owed to us.",
    "The brazier is cold. I'll keep it that way until the ring is tended again.",
    "Agents come, survey, build one thing well, and chronicle it. Then the ring is one wider.",
    "I am the keeper. I do not write the Chronicle. I only make sure it is read.",
};

const char* npcLine(const Npc& npc, int index) {
    if (npc.sprite == NS_CAT) return "The cat blinks slowly, then looks away.";
    if (npc.sprite == NS_CHICKEN) return "Cluck! She goes back to the turnips.";
    if (npc.sprite == NS_SHEEP) return SHEEP_LINES[(unsigned)index % 2u];
    if (npc.sprite == NS_COW) return "Moo. She flicks her tail and keeps chewing the meadow grass.";
    if (npc.sprite == NS_HORSE) return "The horse snorts softly, eager for the road beyond the village.";
    const char** set = VILLAGER_LINES;
    int n = 12;
    switch (npc.role) {
        case NPC_ELDER: set = ELDER_LINES; n = 4; break;
        case NPC_CHILD: set = CHILD_LINES; n = 4; break;
        case NPC_GUARD: set = GUARD_LINES; n = 3; break;
        case NPC_SHOPKEEP: set = SHOP_LINES; n = 1; break;
        case NPC_INNKEEP: set = INN_LINES; n = 1; break;
        case NPC_PRIEST: set = PRIEST_LINES; n = 1; break;
        case NPC_ANIMAL: set = ANIMAL_LINES; n = 4; break;
        case NPC_QUEST: set = QUEST_LINES; n = 1; break;
        case NPC_KEEPER: set = KEEPER_LINES; n = 6; break;
        default: break;
    }
    return set[(unsigned)index % (unsigned)n];
}

const char* npcRoleName(int role) {
    switch (role) {
        case NPC_ELDER: return "Elder";
        case NPC_CHILD: return "Child";
        case NPC_GUARD: return "Guard";
        case NPC_SHOPKEEP: return "Shopkeeper";
        case NPC_INNKEEP: return "Innkeeper";
        case NPC_PRIEST: return "Priest";
        case NPC_ANIMAL: return "Animal";
        case NPC_QUEST: return "Villager";
        case NPC_KEEPER: return "Keeper";
        default: return "Villager";
    }
}

// ------------------------------------------------------------- the ferryman
// One sailor stands on the millpond dock all day, and on whichever dock the
// party last landed at. He is placed after everything else in the sector pass
// so no random villager can take his plank, and he is placed whether or not a
// village is loaded, because the dock is out on the water where no village is.
static void placeFerryman(std::vector<Npc>& out) {
    Dock d;
    if (!dockFind(false, d)) return;
    if (fabsf((float)d.tx + 0.5f - g.px) > 26.0f || fabsf((float)d.ty + 0.5f - g.py) > 26.0f) return;
    Npc n;
    n.id = 0x46e44ull;
    n.sprite = NS_FERRYMAN;
    n.face = FACE_RANGER;
    n.facing = 0;
    n.role = NPC_QUEST;
    n.quest = QUEST_TIDE;
    n.name = "Ferryman Bel";
    n.lineSet = 0;
    // The berth is a busy corner of the village and a villager is often standing
    // exactly where Bel would tie up. Step along the bank rather than vanishing:
    // an absent ferryman strands the whole tide quest, and which tile the cook
    // happens to occupy is not the player's problem.
    auto freeHere = [&](int tx, int ty) {
        if (!cellWalkable(tx, ty)) return false;
        for (const Npc& o : out)
            if (fabsf(o.x - (tx + 0.5f)) < 1.2f && fabsf(o.y - (ty + 0.5f)) < 1.2f) return false;
        return true;
    };
    // Prefer a free tile that still touches the water: Bel is a ferryman, and a
    // ferryman standing in the middle of the square is just a man with a boat.
    auto touchesWater = [&](int tx, int ty) {
        for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++)
            if (cellSailable(tx + dx, ty + dy)) return true;
        return false;
    };
    int bx = -1, by = -1;
    for (int r = 0; r <= 6 && bx < 0; r++) {
        for (int dy = -r; dy <= r && bx < 0; dy++) for (int dx = -r; dx <= r && bx < 0; dx++) {
            if (std::max(abs(dx), abs(dy)) != r) continue;
            if (freeHere(d.tx + dx, d.ty + dy) && touchesWater(d.tx + dx, d.ty + dy)) { bx = d.tx + dx; by = d.ty + dy; }
        }
    }
    for (int r = 0; r <= 6 && bx < 0; r++) {
        for (int dy = -r; dy <= r && bx < 0; dy++) for (int dx = -r; dx <= r && bx < 0; dx++) {
            if (std::max(abs(dx), abs(dy)) != r) continue;
            if (freeHere(d.tx + dx, d.ty + dy)) { bx = d.tx + dx; by = d.ty + dy; }
        }
    }
    if (bx < 0) return;
    n.x = n.homeX = bx + 0.5f;
    n.y = n.homeY = by + 0.5f;
    out.push_back(n);
}

void npcsLoadFor(float x, float y, bool force) {
    bool useVillage = g.villageLoaded && fabsf(x - g.village.cx) < 30 && fabsf(y - g.village.cy) < 30;
    int sx = (int)floorf(x / 32), sy = (int)floorf(y / 32);
    uint64_t key = useVillage
        ? (0x1000000000000ull | g.villageKey)
        : (0x2000000000000ull | (((uint64_t)(uint32_t)sx << 24) ^ (uint32_t)sy));
    if (!force && g.npcKey == key) return;

    g.npcs.clear();
    g.npcKey = key;
    uint32_t seed = useVillage ? g.village.seed : hash2i(sx, sy, g.worldSeed);

    auto add = [&](int sprite, int face, int role, float px, float py, const char* name, int lineSet,
                   int quest = -1) {
        int tx = (int)floorf(px), ty = (int)floorf(py);
        nearestWalkable(tx, ty, tx, ty);
        Npc n;
        n.id = (uint64_t)(g.npcs.size() + 1) * 7919ull ^ (uint64_t)seed;
        n.sprite = sprite; n.face = face; n.role = role; n.quest = quest;
        n.x = n.homeX = (float)tx + 0.5f;
        n.y = n.homeY = (float)ty + 0.5f;
        n.name = name; n.lineSet = lineSet;
        n.thinkT = 1.0f + hashf(tx, ty, seed) * 2.5f;
        g.npcs.push_back(n);
    };

    if (useVillage) {
        const Village& v = g.village;
        for (const Building& b : v.buildings) {
            int dtx = buildingDoorTx(b), dty = buildingDoorTy(b);
            if (b.kind == 5)
                add(NS_MERCHANT, FACE_SHOPKEEPER, NPC_SHOPKEEP, (float)(dtx - 2) + 0.5f, (float)dty + 0.5f,
                    v.startVillage ? "Mira the Trader" : "Village Trader", 0);
            else if (b.kind == 6)
                add(NS_MAYOR, FACE_INNKEEPER, NPC_INNKEEP, (float)(dtx + 2) + 0.5f, (float)dty + 0.5f,
                    v.startVillage ? "Bren the Innkeep" : "Innkeeper", 0);
            else if (b.kind == 7)
                add(NS_PRIEST, FACE_ELDER, NPC_PRIEST, (float)(dtx + 2) + 0.5f, (float)dty + 0.5f,
                    v.startVillage ? "Father Osric" : "Shrine Keeper", 0);
            else if (b.kind == 10)
                add(NS_SMITH, FACE_SMITH, NPC_SHOPKEEP, (float)(dtx - 2) + 0.5f, (float)dty + 0.5f,
                    v.startVillage ? "Aldwin the Smith" : "Village Smith", 0);
        }

        if (v.startVillage) {
            // Animals in their proper yards & patios
            add(NS_CHICKEN, -1,       NPC_ANIMAL,  -10.5f, 10.5f, "Hen",      0);
            add(NS_CHICKEN, -1,       NPC_ANIMAL,  -11.5f, 11.5f, "Hen",      1);
            add(NS_SHEEP,   -1,       NPC_ANIMAL,   11.5f, 10.5f, "Clover",   0);
            add(NS_SHEEP,   -1,       NPC_ANIMAL,   12.5f, 11.5f, "Woolly",   1);
            add(NS_COW,     -1,       NPC_ANIMAL,  -13.5f, 10.5f, "Daisy",    0);
            add(NS_CAT,     -1,       NPC_ANIMAL,   11.5f,  2.5f, "Inn Cat",  0);
            add(NS_DOG,     FACE_DOG, NPC_ANIMAL,    2.5f,  1.5f, "Town Dog", 0);
            add(NS_HORSE,   -1,       NPC_ANIMAL,   12.5f,  5.5f, "Dobbin",   0);
            add(NS_CHICKEN, -1,       NPC_ANIMAL,   -9.5f, 12.5f, "Biddy",    2);

            // Named Elmswick townsfolk & questgivers in their district spots
            add(NS_ELDER,     FACE_ELDER,     NPC_ELDER,     1.5f, -1.5f, "Old Maren",       0, QUEST_CROWN);
            add(NS_WOMAN,     FACE_WOMAN,     NPC_QUEST,    -2.5f,  1.5f, "Tamsin",          1, QUEST_LOCKET);
            add(NS_CHILD,     FACE_CHILD,     NPC_CHILD,    -1.5f,  3.5f, "Pip the Younger", 2);
            add(NS_GUARD,     FACE_GUARD,     NPC_GUARD,     2.5f, 10.5f, "Watchman Odo",    3);
            add(NS_VILLAGER1, FACE_VILLAGER1, NPC_VILLAGER, -6.5f,  1.5f, "Rook",            4);
            add(NS_VILLAGER2, FACE_VILLAGER2, NPC_VILLAGER,  6.5f,  3.5f, "Sable",           5);
            add(NS_VILLAGER3, FACE_VILLAGER3, NPC_VILLAGER, -8.5f, -5.5f, "Wren",            6);
            add(NS_FARMER,    FACE_SMITH,     NPC_QUEST,    -6.5f, 10.5f, "Farmer Hedrick",  0, QUEST_SLIMES);
            // Village 2.0 — the forge quarter and the road wardens
            add(NS_VILLAGER5, FACE_VILLAGER5, NPC_QUEST,   -13.5f, -1.5f, "Nell the Apprentice", 7, QUEST_DEEP);
            add(NS_RANGER,    FACE_RANGER,    NPC_VILLAGER, 15.5f, -1.5f, "Warden Yara",     8);
            add(NS_SOLDIER,   FACE_GUARD,     NPC_VILLAGER,-15.5f,  1.5f, "Corporal Fenn",   9);
            add(NS_VILLAGER6, FACE_VILLAGER6, NPC_VILLAGER, 11.5f,  6.5f, "Maren the Weaver",10);
            add(NS_VILLAGER7, FACE_VILLAGER7, NPC_VILLAGER, -2.5f, -8.5f, "Brother Cuthbert",11);
            add(NS_VILLAGER8, FACE_VILLAGER8, NPC_CHILD,    12.5f, -4.5f, "Little Tam",      2);
            add(NS_VILLAGER9, FACE_VILLAGER9, NPC_VILLAGER,-16.5f, -6.5f, "Old Geordie",     5);
            // The Seed's one living voice: the keeper at the Commons gate, in
            // an OpenRTP People1 walking slot no other villager uses.
            add(NS_KEEPER, FACE_KEEPER, NPC_KEEPER,
                (float)COMMONS_CX, (float)COMMONS_CY + 7.0f, "Sable, Keeper of the Commons",
                0, QUEST_COMMONS);
        } else {
            const int sprites[] = {
                NS_VILLAGER0, NS_VILLAGER1, NS_VILLAGER2, NS_VILLAGER3, NS_VILLAGER4,
                NS_VILLAGER5, NS_VILLAGER6, NS_VILLAGER7, NS_VILLAGER8, NS_VILLAGER9,
            };
            const int faces[] = {
                FACE_VILLAGER0, FACE_VILLAGER1, FACE_VILLAGER2, FACE_VILLAGER3, FACE_VILLAGER4,
                FACE_VILLAGER5, FACE_VILLAGER6, FACE_VILLAGER7, FACE_VILLAGER8, FACE_VILLAGER9,
            };
            const char* names[] = { "Elin", "Corin", "Mina", "Tomas", "Nessa", "Rook", "Sable", "Wren", "Tarin", "Lio" };
            int n = 7 + (int)(v.seed % 4);
            for (int i = 0; i < n; i++) {
                float ang = hashf(i, 11, v.seed) * 6.2831853f;
                float r = 2.5f + hashf(i, 13, v.seed) * 5.0f;
                float px = (float)v.cx + cosf(ang) * r;
                float py = (float)v.cy + sinf(ang) * r * 0.8f;
                int role = (i == 0) ? NPC_ELDER : (i == 1 ? NPC_CHILD : (i == 2 ? NPC_GUARD : NPC_VILLAGER));
                int person = (int)((v.seed + (uint32_t)i * 7u) % 10u);
                int sprite = (role == NPC_ELDER) ? NS_ELDER : (role == NPC_CHILD ? NS_CHILD : (role == NPC_GUARD ? NS_GUARD : sprites[person]));
                int face = (role == NPC_ELDER) ? FACE_ELDER : (role == NPC_CHILD ? FACE_CHILD : (role == NPC_GUARD ? FACE_GUARD : faces[person]));
                const char* name = role == NPC_ELDER ? "Elder Rowan" :
                                   (role == NPC_CHILD ? "Kip" :
                                   (role == NPC_GUARD ? "Guard Bram" : names[person]));
                add(sprite, face, role, px, py, name, i);
            }
            // The forge apprentice offers the deep-vault errand in every village.
            bool hasSmithy = false;
            for (const auto& b : v.buildings) if (b.kind == 10) { hasSmithy = true; break; }
            if (hasSmithy) {
                add(NS_VILLAGER5, FACE_VILLAGER5, NPC_QUEST,
                    (float)v.cx - 4.5f, (float)v.cy + 5.5f, "Forge Apprentice", 7, QUEST_DEEP);
            }
            add(NS_DOG, FACE_DOG, NPC_ANIMAL, (float)v.cx + 2.5f, (float)v.cy + 1.5f, "Village Dog", 0);
            switch (v.seed % 3u) {
                case 0: add(NS_COW, -1, NPC_ANIMAL, (float)v.cx - 3.5f, (float)v.cy + 2.5f, "Village Cow", 0); break;
                case 1: add(NS_HORSE, -1, NPC_ANIMAL, (float)v.cx - 3.5f, (float)v.cy + 2.5f, "Village Horse", 0); break;
                default: add(NS_SHEEP, -1, NPC_ANIMAL, (float)v.cx - 3.5f, (float)v.cy + 2.5f, "Village Sheep", 0); break;
            }
        }
    } else {
        uint32_t h = hash2i(sx, sy, g.worldSeed ^ 0x4e3c17u);
        if ((h % 100) < 25 && (x * x + y * y) > 45 * 45) {
            float px = sx * 32 + 8.0f + hashf(sx, sy, h) * 14.0f;
            float py = sy * 32 + 8.0f + hashf(sy, sx, h) * 14.0f;
            int tx = (int)floorf(px), ty = (int)floorf(py);
            if (cellWalkable(tx, ty))
                add(NS_RANGER, FACE_RANGER, NPC_VILLAGER, px, py, "Wandering Ranger", (int)(h % 8));
        }
    }
    // The ferryman keeps the dock. He is added last so nothing displaces him,
    // and he is on the water's side of the map, so he exists with or without
    // a village loaded.
    placeFerryman(g.npcs);
}

int npcNear(float x, float y, float radius, float* outDist) {
    int best = -1;
    float bestD = radius * radius;
    for (int i = 0; i < (int)g.npcs.size(); i++) {
        float dx = g.npcs[i].x - x, dy = g.npcs[i].y - y;
        float d = dx * dx + dy * dy;
        if (d < bestD) { bestD = d; best = i; }
    }
    if (outDist) *outDist = sqrtf(bestD);
    return best;
}

// ================================================================ chests
void chestsLoadFor(float x, float y) {
    gChestsInView.clear();
    const int SECTOR = 40;
    int sx = (int)floorf(x / SECTOR), sy = (int)floorf(y / SECTOR);
    for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
        int cx = sx + dx, cy = sy + dy;
        uint32_t h = hash2i(cx, cy, g.worldSeed ^ 0xc4e57u);
        if ((h % 100) >= 42) continue;
        int tx = cx * SECTOR + 4 + (int)(hashf(cx, cy, h) * (SECTOR - 8));
        int ty = cy * SECTOR + 4 + (int)(hashf(cy, cx, h + 7) * (SECTOR - 8));
        if (!cellWalkable(tx, ty)) continue;
        if (inVillage((float)tx + 0.5f, (float)ty + 0.5f)) continue;
        if (nearSpecialSite(tx, ty)) continue;
        Chest c;
        c.id = ((uint64_t)(uint32_t)cx << 32) ^ (uint32_t)cy ^ 0x9e37ull;
        c.tx = tx; c.ty = ty;
        int zl = zoneLevelAt((float)tx, (float)ty);
        int roll = (int)((h >> 8) % 100);
        if (roll < 36) { c.item = IT_POTION; c.amount = 1 + (int)((h >> 16) % 2); }
        else if (roll < 56) { c.gil = 25 + zl * 15 + (int)((h >> 16) % 80); }
        else if (roll < 70) { c.item = IT_ETHER; c.amount = 1; }
        else if (roll < 82) { c.item = IT_HIPOTION; c.amount = 1; }
        else if (roll < 92) { c.item = IT_PHOENIX; c.amount = 1; }
        else { c.item = (zl >= 6) ? IT_ELIXIR : IT_HIETHER; c.amount = 1; }
        gChestsInView.push_back(c);
    }
}

// ================================================================ the Open Water
// The ferryman needs four places: a landing on Elmswick's millpond, and three
// isles with a beach. The landing is found from the shore nearest the well, so
// it sits exactly where the pond is closest to the square and moves with the
// seed. The isles are NOT scavenged from the noise: a pond's worth of noise
// rarely leaves three islands big enough to stand on, and a fixed world that
// sometimes has no destination is a broken promise. They are built into the
// millpond instead, deterministically per seed, and then dressed like any
// other shore: sand ring, meadow crown, a ring of standing stones, a Tidewell.
// The nearest water tile to a land tile: the boat's slip.
static bool nearestSlip(int tx, int ty, int maxR, int& ox, int& oy) {
    for (int r = 1; r <= maxR; r++) {
        for (int dy = -r; dy <= r; dy++) {
            for (int dx = -r; dx <= r; dx++) {
                if (std::max(abs(dx), abs(dy)) != r) continue;
                if (sampleGround(tx + dx, ty + dy).kind == G_WATER) {
                    ox = tx + dx; oy = ty + dy;
                    return true;
                }
            }
        }
    }
    return false;
}

// ---- the three isles --------------------------------------------------------
// The isles are the authored ones in MILL_ISLES above. They are stamped into
// the terrain by elevationAt(), so ground, Survey, collision and the Journal
// already agree about them without anyone asking a second time.
static TideIsle isleLanding[TIDE_ISLANDS];
static int isleCount = 0;
static bool islesLaid = false;

// Stamps an isle's crown and sand skirt. A pure function of the authored list,
// so it is safe to call from inside generateGround().
static bool isleStamp(int tx, int ty, GroundTile& out) {
    for (int i = 0; i < TIDE_ISLANDS; i++) {
        float dx = (float)tx - MILL_ISLES[i].x, dy = (float)ty - MILL_ISLES[i].y;
        float r = MILL_ISLES[i].r;
        float r2 = dx * dx + dy * dy;
        if (r2 <= r * r) {
            out.kind = G_GRASS;
            out.biome = BI_MEADOW;
            out.deco = (uint8_t)(hash2i(tx, ty, g.worldSeed ^ 0x1515u) % 3 == 0 ? 2 : 0);
            return true;
        }
        // Sand skirt, but only where the pond has water to spare: a skirt that
        // reaches dry land would make the isle a peninsula and the boat a joke.
        if (r2 <= (r + 1.5f) * (r + 1.5f) && millpondT((float)tx, (float)ty) < 0.94f) {
            out.kind = G_BEACH;
            out.biome = BI_MEADOW;
            out.deco = 0;
            return true;
        }
    }
    return false;
}

// The dry crown (the sand skirt still counts as beach).
static bool onTidemillIsle(int tx, int ty) { return onMillIsle(tx, ty, 1.0f); }

// The water around an isle is a boat's business: no survey planks, no builds.
static bool nearTidemillIsle(int tx, int ty) { return onMillIsle(tx, ty, 9.0f); }

static void buildIslets() {
    if (islesLaid) return;
    islesLaid = true;
    isleCount = TIDE_ISLANDS;
    for (int i = 0; i < TIDE_ISLANDS; i++) {
        // The landing is the walkable crown tile nearest the town: the beach a
        // ferry would nose into.
        int bestX = (int)floorf(MILL_ISLES[i].x), bestY = (int)floorf(MILL_ISLES[i].y);
        float bestD = 1e9f;
        for (int oy = -8; oy <= 8; oy++) for (int ox = -8; ox <= 8; ox++) {
            int x = (int)floorf(MILL_ISLES[i].x) + ox, y = (int)floorf(MILL_ISLES[i].y) + oy;
            if (!onMillIsle(x, y)) continue;
            if (!cellWalkable(x, y)) continue;
            float dd = (float)x * x + (float)y * y;
            if (dd < bestD) { bestD = dd; bestX = x; bestY = y; }
        }
        isleLanding[i].tx = bestX;
        isleLanding[i].ty = bestY;
        isleLanding[i].well = true;
    }
}

void tideIslands() { buildIslets(); }
int tideIslandCount() { buildIslets(); return isleCount; }
bool tideIslandAt(int i, TideIsle& out) {
    buildIslets();
    if (i < 0 || i >= isleCount) return false;
    out = isleLanding[i];
    return true;
}
float tideIslandWellX(int i) { buildIslets(); return (i >= 0 && i < isleCount) ? isleLanding[i].tx + 0.5f : 0.0f; }
float tideIslandWellY(int i) { buildIslets(); return (i >= 0 && i < isleCount) ? isleLanding[i].ty + 0.5f : 0.0f; }
const char* tideIslandName(int i) {
    static const char* NAMES[TIDE_ISLANDS] = { "Tidewell Isle", "Gull Rock", "The Little Kettle" };
    return (i >= 0 && i < TIDE_ISLANDS) ? NAMES[i] : "an isle";
}

// The docks. Land-side docks are found from the shore nearest the well (the
// Elmswick millpond landing) and from each isle's own beach; afloat, the
// nearest dock is whichever of those the hull is closest to.
static bool dockFromLand(int wx, int wy, Dock& out) {
    int lx = 0, ly = 0;
    for (int r = 1; r <= 8; r++) {
        for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) {
            if (std::max(abs(dx), abs(dy)) != r) continue;
            int x = wx + dx, y = wy + dy;
            if (!cellWalkable(x, y)) continue;
            bool touches = false;
            for (int oy = -1; oy <= 1 && !touches; oy++) for (int ox = -1; ox <= 1; ox++) {
                if (sampleGround(x + ox, y + oy).kind == G_WATER) { touches = true; break; }
            }
            if (!touches) continue;
            lx = x; ly = y;
            int sx = 0, sy = 0;
            if (!nearestSlip(lx, ly, 3, sx, sy)) continue;
            out.valid = true; out.tx = lx; out.ty = ly; out.wx = sx; out.wy = sy;
            return true;
        }
    }
    return false;
}

bool dockFind(bool sailing, Dock& out) {
    out = Dock{};
    float bestD = 1e9f;
    auto consider = [&](const Dock& d) {
        if (!d.valid) return;
        float dx = (sailing ? gSailX : g.px) - (d.tx + 0.5f);
        float dy = (sailing ? gSailY : g.py) - (d.ty + 0.5f);
        float dd = dx * dx + dy * dy;
        if (dd < bestD) { bestD = dd; out = d; }
    };
    if (!sailing) {
        // The home dock sits on the millpond's north bank, a short walk from
        // the square: find the shore nearest the well, then the landing on it.
        int sx = 0, sy = 0;
        bool have = false;
        float bd = 1e9f;
        for (int y = -60; y <= 60; y++) for (int x = -60; x <= 60; x++) {
            if (sampleGround(x, y).kind != G_WATER) continue;
            float dd = (float)x * x + (float)y * y;
            if (dd < bd) { bd = dd; sx = x; sy = y; have = true; }
        }
        if (have) { Dock d; if (dockFromLand(sx, sy, d) && (d.tx - g.px) * (d.tx - g.px) + (d.ty - g.py) * (d.ty - g.py) < 24.0f * 24.0f) consider(d); }
        // island docks are reachable only from their own beach
        buildIslets();
        for (int i = 0; i < isleCount; i++) {
            Dock d;
            int sx2 = 0, sy2 = 0;
            if (nearestSlip(isleLanding[i].tx, isleLanding[i].ty, 14, sx2, sy2) && dockFromLand(sx2, sy2, d)) consider(d);
        }
    } else {
        int sx = 0, sy = 0;
        bool have = false;
        float bd = 1e9f;
        for (int y = -60; y <= 60; y++) for (int x = -60; x <= 60; x++) {
            if (sampleGround(x, y).kind != G_WATER) continue;
            float dd = (gSailX - x) * (gSailX - x) + (gSailY - y) * (gSailY - y);
            if (dd < bd) { bd = dd; sx = x; sy = y; have = true; }
        }
        if (have) { Dock d; if (dockFromLand(sx, sy, d)) consider(d); }
        buildIslets();
        for (int i = 0; i < isleCount; i++) {
            int sx2 = 0, sy2 = 0;
            if (!nearestSlip(isleLanding[i].tx, isleLanding[i].ty, 14, sx2, sy2)) continue;
            Dock d;
            if (dockFromLand(sx2, sy2, d)) consider(d);
        }
    }
    return out.valid;
}

bool landmarkNear(int tx, int ty, Landmark& out) {
    if (surveyedCavernNear(tx, ty, 6, out)) return true;
    if (abs(tx - BOSS_CASTLE_TX) <= 10 && abs(ty - BOSS_CASTLE_TY) <= 10) {
        out = { LK_CASTLE, BOSS_CASTLE_TX, BOSS_CASTLE_TY, 0xca571eu };
        return true;
    }
    if (abs(tx - LOCKET_SITE_TX) <= 8 && abs(ty - LOCKET_SITE_TY) <= 8) {
        out = { LK_STONES, LOCKET_SITE_TX, LOCKET_SITE_TY, 0x10c4e7u };
        return true;
    }
    if (abs(tx - TOWER_SITE_TX) <= 8 && abs(ty - TOWER_SITE_TY) <= 8) {
        out = { LK_TOWER, TOWER_SITE_TX, TOWER_SITE_TY, 0x703e2u };
        return true;
    }
    {
        int cx, cy;
        if (startCavernSite(cx, cy) && abs(tx - cx) <= 6 && abs(ty - cy) <= 5) {
            out = { LK_CAVERN, cx, cy, 0x57acu };
            return true;
        }
    }
    if (!proceduralLandmarkNear(tx, ty, 8, out)) return false;
    return true;
}

void landmarksInView(int x0, int y0, int x1, int y1, std::vector<Landmark>& out) {
    out.clear();
    auto maybeAdd = [&](LandmarkKind kind, int tx, int ty, uint32_t seed) {
        if (tx >= x0 - 6 && tx <= x1 + 6 && ty >= y0 - 8 && ty <= y1 + 8)
            out.push_back({ kind, tx, ty, seed });
    };
    // The three story sites and the two authored cavern mouths are always there.
    maybeAdd(LK_CASTLE, BOSS_CASTLE_TX, BOSS_CASTLE_TY, 0xca571eu);
    maybeAdd(LK_STONES, LOCKET_SITE_TX, LOCKET_SITE_TY, 0x10c4e7u);
    maybeAdd(LK_TOWER,  TOWER_SITE_TX,  TOWER_SITE_TY,  0x703e2u);
    {
        int cx, cy;
        if (startCavernSite(cx, cy)) maybeAdd(LK_CAVERN, cx, cy, 0x57acu);
    }
    Landmark northCave = northRoadCavern();
    maybeAdd(northCave.kind, northCave.tx, northCave.ty, northCave.seed);
    int fx, fy; frontierCavernSite(fx, fy);
    maybeAdd(LK_CAVERN, fx, fy, 0xf204u);

    // The three Tidemill Isles, each with a standing Tidewell at its heart.
    buildIslets();
    for (int i = 0; i < isleCount; i++) {
        int cx = isleLanding[i].tx, cy = isleLanding[i].ty;
        if (cx >= x0 - 10 && cx <= x1 + 10 && cy >= y0 - 10 && cy <= y1 + 10)
            out.push_back({ LK_STONES, isleLanding[i].tx, isleLanding[i].ty,
                            0x71de0000u + (uint32_t)i });
    }

    // Everything else is surveyed ground: caverns, ruins, towers, castles,
    // standing stones and camps, each guaranteed by its lattice cell.
    static std::vector<AtlasSite> sites;
    atlasSitesInRect(x0, y0, x1, y1, sites);
    for (const AtlasSite& site : sites) {
        if (site.kind == AS_VILLAGE) continue;              // villages draw themselves
        if (site.kind == AS_CAVERN) { maybeAdd(LK_CAVERN, site.tx, site.ty, site.seed); continue; }
        maybeAdd((LandmarkKind)(site.kind - 1), site.tx, site.ty, site.seed);
    }
}

bool inBossArena(float x, float y) {
    float dx = x - (float)BOSS_CASTLE_TX, dy = y - (float)BOSS_CASTLE_TY;
    return (dx * dx + dy * dy) < 16.0f * 16.0f;
}

// ================================================================ field monsters
EnemyKind rollBiomeEnemy(int tx, int ty, int zoneLevel) {
    // The six-entry local pools now mix the original cast with OpenRTP's
    // previously unused wildlife and lower-tier creatures.
    static const EnemyKind common[BI_COUNT][6] = {
        { EN_SLIME, EN_SLIME, EN_IMP, EN_BAT, EN_GOBLIN, EN_BANDIT },
        { EN_BAT, EN_IMP, EN_WOLF, EN_BAT, EN_FAERIE, EN_GOBLIN },
        { EN_HORNET, EN_SLIME, EN_HORNET, EN_WOLF, EN_FAERIE, EN_PORCUPINE },
        { EN_SLIME, EN_WISP, EN_BAT, EN_WISP, EN_PORCUPINE, EN_GOBLIN },
        { EN_WOLF, EN_IMP, EN_WOLF, EN_BAT, EN_BANDIT, EN_PORCUPINE },
        { EN_WOLF, EN_WISP, EN_WOLF, EN_WISP, EN_TIGER, EN_REAPER },
        { EN_HORNET, EN_IMP, EN_BAT, EN_IMP, EN_PORCUPINE, EN_GOBLIN }
    };
    static const EnemyKind frontier[BI_COUNT][4] = {
        { EN_WOLF, EN_ORC, EN_BANDIT, EN_TIGER },
        { EN_GHOST, EN_WOLF, EN_REAPER, EN_TIGER },
        { EN_WOLF, EN_HORNET, EN_GOBLIN, EN_TIGER },
        { EN_GHOST, EN_SKELETON, EN_SHADE, EN_NAGA },
        { EN_ORC, EN_SKELETON, EN_REAPER, EN_DREAD_KNIGHT },
        { EN_WISP, EN_GHOST, EN_TIGER, EN_SHADE },
        { EN_SKELETON, EN_ORC, EN_NAGA, EN_DRAKE }
    };
    static const EnemyKind far[BI_COUNT][3] = {
        { EN_NIGHT_RIDER, EN_DREAD_KNIGHT, EN_DRAKE },
        { EN_NIGHT_RIDER, EN_DRAKE, EN_ARCHFIEND },
        { EN_SERPENT, EN_DRAKE, EN_ARCHFIEND },
        { EN_SERPENT, EN_NAGA, EN_SHADE },
        { EN_GOLEM, EN_DREAD_KNIGHT, EN_ARCHFIEND },
        { EN_GOLEM, EN_ARCHFIEND, EN_DRAKE },
        { EN_SERPENT, EN_DRAKE, EN_ARCHFIEND }
    };
    auto biome = sampleGround(tx, ty).biome;
    auto h = hash2i(tx, ty, g.worldSeed ^ 0xbad1ceu);
    if (zoneLevel >= 12 && h % 3 != 0) return far[biome][(h >> 8) % 3];
    if (zoneLevel >= 3 && (h % 3 == 0 || zoneLevel >= 7))
        return frontier[biome][(h >> 8) % 4];
    return common[biome][(h >> 8) % 6];
}

std::vector<EnemyKind> buildEncounterGroup(EnemyKind primary, int tx, int ty, int zoneLevel) {
    std::vector<EnemyKind> group;
    group.push_back(primary);
    uint32_t h = hash2i(tx + g.stepCounter, ty + g.defeatedCount, g.worldSeed ^ 0x51a9u);
    int extraChance = (zoneLevel <= 1) ? 35 : (zoneLevel <= 3 ? 55 : 70);
    if ((int)(h % 100) < extraChance) {
        EnemyKind second = ((h >> 8) % 2 == 0) ? primary : rollBiomeEnemy(tx + 3, ty - 2, zoneLevel);
        group.push_back(second);
        if (zoneLevel >= 4 && (int)((h >> 16) % 100) < 35) {
            group.push_back(rollBiomeEnemy(tx - 2, ty + 5, zoneLevel));
        }
    }
    return group;
}

void fieldMonstersInView(float x, float y, std::vector<FieldMonster>& out) {
    out.clear();
    const int SECTOR = 24;
    int sx = (int)floorf(x / SECTOR), sy = (int)floorf(y / SECTOR);
    for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
        int cx = sx + dx, cy = sy + dy;
        uint32_t h = hash2i(cx, cy, g.worldSeed ^ 0xf11du);
        if ((h % 100) >= 85) continue;
        int count = 1 + (int)((h >> 8) % 2);
        for (int i = 0; i < count; i++) {
            float a = hashf(i, cx * 7 + 1, h) * 6.2831853f;
            float r = 3.0f + hashf(i, cy * 13 + 5, h) * (SECTOR * 0.38f);
            float mx = cx * SECTOR + SECTOR * 0.5f + cosf(a) * r;
            float my = cy * SECTOR + SECTOR * 0.5f + sinf(a) * r;
            if (mx * mx + my * my < 22.0f * 22.0f) continue;
            if (g.villageLoaded && inVillage(mx, my)) continue;
            int mtx = (int)floorf(mx), mty = (int)floorf(my);
            if (nearSpecialSite(mtx, mty)) continue;
            if (!cellWalkable(mtx, mty)) continue;
            uint64_t id = ((uint64_t)(uint32_t)cx << 40) ^ ((uint64_t)(uint32_t)cy << 8) ^ (uint64_t)(i + 1);
            if (g.defeatedMonsters.count(id)) continue;

            FieldMonster fm;
            fm.id = id;
            fm.anchorX = mx; fm.anchorY = my;
            fm.x = mx; fm.y = my;
            fm.phase = hashf(i, cx + cy + 99, h) * 6.2831853f;
            int zl = zoneLevelAt(mx, my);
            if (my > 18.0f && my < 56.0f && fabsf(mx) < 34.0f && ((h >> 12) % 100) < 68)
                fm.kind = EN_SLIME;
            else
                fm.kind = rollBiomeEnemy(mtx, mty, zl);
            out.push_back(fm);
        }
    }
}

int zoneLevelAt(float x, float y) {
    float dist = sqrtf(x * x + y * y);
    int zl = 1 + (int)(dist / 95.0f);
    // Each country has a readable minimum danger, while the existing distance
    // and party-level scaling still shapes its encounters and shop stock.
    zl = std::max(zl, regionAt(x, y) == 2 ? 4 : (regionAt(x, y) == 1 ? 2 : 1));
    int avg = partyLevel();
    return std::max(1, std::min(zl + (avg - 1) / 3, 26));
}

const char* zoneNameAt(float x, float y) {
    if (fabsf(x) < 11 && y > 15 && y < 24) return "Millbrook Crossing";
    if (inCommons(x, y) && g.villageLoaded) return "The Agents' Commons";
    if (inVillage(x, y) && g.villageLoaded) return g.village.name.c_str();
    if (inBossArena(x, y)) return "Castle of Elm";
    if (fabsf(x - LOCKET_SITE_TX) < 10.0f && fabsf(y - LOCKET_SITE_TY) < 10.0f) return "The Standing Stones";
    if (fabsf(x - TOWER_SITE_TX) < 9.0f && fabsf(y - TOWER_SITE_TY) < 9.0f) return "The Old Watchtower";
    Biome b = sampleGround((int)floorf(x), (int)floorf(y)).biome;
    switch (b) {
        case BI_FOREST: return "Briarwood";
        case BI_AMBER: return "Amberwood";
        case BI_FEN: return "The Mirefen";
        case BI_HEATH: return "The Stonelands";
        case BI_FROST: return "The Frozenreach";
        case BI_DUNES: return "The Sunscorch Flats";
        default: break;
    }
    if (x * x + y * y < 26 * 26) return "Meadow of First Steps";
    if (y < -40 && fabsf(x) < 30) return "The North Road";
    if (x < -120) return "The Howling West";
    if (x * x + y * y > 260 * 260) return "The Far Wilds";
    return "The Greenmere";
}

// ================================================================ navigation
void nearestWalkable(int tx, int ty, int& outTx, int& outTy) {
    if (cellWalkable(tx, ty)) { outTx = tx; outTy = ty; return; }
    for (int r = 1; r <= 8; r++) {
        for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) {
            if (std::max(abs(dx), abs(dy)) != r) continue;
            if (cellWalkable(tx + dx, ty + dy)) { outTx = tx + dx; outTy = ty + dy; return; }
        }
    }
    outTx = tx; outTy = ty;
}

namespace {
struct NavOpen { int x, y; float f; bool operator>(const NavOpen& o) const { return f > o.f; } };
// Click-to-walk is not a leash. The old search gave up at 44 tiles in each
// direction and 9000 expanded cells, so clicking a landmark across the meadow
// simply stopped half-way — "the map has an edge" from the player's side. The
// search may now range as far as the click demands, at the cost of walking.
constexpr int NAV_MAX_EXPAND = 60000;
constexpr int NAV_MAX_RADIUS = 160;
inline uint64_t navKey(int x, int y) { return ((uint64_t)(uint32_t)x << 32) | (uint32_t)y; }
}

bool findWalkPath(float fromX, float fromY, int toTx, int toTy, WalkPath& out) {
    out.clear();
    int sx = (int)floorf(fromX), sy = (int)floorf(fromY);
    int targetX = toTx, targetY = toTy;

    if (g.villageLoaded) {
        for (int i = 0; i < (int)g.village.buildings.size(); i++) {
            const Building& b = g.village.buildings[i];
            int x0 = buildingX0(b), x1 = x0 + b.w - 1;
            int y0 = buildingY0(b);
            if (toTx >= x0 && toTx <= x1 && toTy >= y0 && toTy <= b.ty) {
                out.doorBuilding = i;
                targetX = buildingDoorTx(b);
                targetY = buildingDoorTy(b);
                break;
            }
        }
    }
    if (out.doorBuilding < 0 && !cellWalkable(targetX, targetY))
        nearestWalkable(toTx, toTy, targetX, targetY);
    if (!cellWalkable(targetX, targetY)) return false;
    if (!cellWalkable(sx, sy)) nearestWalkable(sx, sy, sx, sy);
    if (sx == targetX && sy == targetY) {
        out.cells.push_back({ targetX, targetY });
        out.reachable = true;
        return true;
    }

    std::unordered_map<uint64_t, float> gScore;
    std::unordered_map<uint64_t, uint64_t> parent;
    std::priority_queue<NavOpen, std::vector<NavOpen>, std::greater<NavOpen>> open;
    std::vector<std::pair<int, int>> explored;

    auto hcost = [&](int x, int y) {
        float dx = (float)(x - targetX), dy = (float)(y - targetY);
        return sqrtf(dx * dx + dy * dy);
    };
    auto relax = [&](int x, int y, float gc, uint64_t from) {
        uint64_t k = navKey(x, y);
        auto it = gScore.find(k);
        if (it != gScore.end() && it->second <= gc + 1e-4f) return;
        gScore[k] = gc;
        parent[k] = from;
        open.push({ x, y, gc + hcost(x, y) });
    };
    relax(sx, sy, 0.0f, 0);
    bool found = false;
    int expanded = 0;
    const int dirs[8][2] = { { 1,0 },{ -1,0 },{ 0,1 },{ 0,-1 },{ 1,1 },{ 1,-1 },{ -1,1 },{ -1,-1 } };

    while (!open.empty() && expanded < NAV_MAX_EXPAND) {
        NavOpen cur = open.top(); open.pop();
        uint64_t ck = navKey(cur.x, cur.y);
        auto gIt = gScore.find(ck);
        if (gIt == gScore.end()) continue;
        if (cur.f > gIt->second + hcost(cur.x, cur.y) + 1e-3f) continue;
        if (cur.x == targetX && cur.y == targetY) { found = true; break; }
        explored.push_back({ cur.x, cur.y });
        expanded++;
        for (auto& d : dirs) {
            int nx = cur.x + d[0], ny = cur.y + d[1];
            if (abs(nx - sx) > NAV_MAX_RADIUS || abs(ny - sy) > NAV_MAX_RADIUS) continue;
            if (nx == sx && ny == sy) continue;
            bool diagonal = (d[0] != 0 && d[1] != 0);
            if (!cellWalkable(nx, ny)) continue;
            if (diagonal && (!cellWalkable(cur.x + d[0], cur.y) || !cellWalkable(cur.x, cur.y + d[1]))) continue;
            relax(nx, ny, gIt->second + (diagonal ? 1.4142f : 1.0f), ck);
        }
    }

    if (!found) {
        float bestD = 1e9f; int bx = sx, by = sy;
        for (auto& c : explored) {
            float dx = (float)(c.first - targetX), dy = (float)(c.second - targetY);
            float d = dx * dx + dy * dy;
            if (d < bestD) { bestD = d; bx = c.first; by = c.second; }
        }
        if (bx == sx && by == sy) return false;
        out.doorBuilding = -1;
        targetX = bx; targetY = by;
    }

    std::vector<std::pair<int, int>> rev;
    int cx = targetX, cy = targetY;
    rev.push_back({ cx, cy });
    int guard = 0;
    while (!(cx == sx && cy == sy) && guard++ < NAV_MAX_EXPAND) {
        auto it = parent.find(navKey(cx, cy));
        if (it == parent.end()) break;
        uint64_t k = it->second;
        cx = (int)(int32_t)(k >> 32); cy = (int)(int32_t)(k & 0xffffffffu);
        rev.push_back({ cx, cy });
    }
    std::reverse(rev.begin(), rev.end());
    if (!rev.empty() && rev.front().first == sx && rev.front().second == sy) rev.erase(rev.begin());
    if (rev.empty()) return false;
    out.cells = rev;
    out.reachable = true;
    return true;
}
