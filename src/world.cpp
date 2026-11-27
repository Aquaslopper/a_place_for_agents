// world.cpp — noise, biomes, villages, props, npcs, chests, landmarks, A*
#include "world.hpp"
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <queue>
#include <limits>
#include <string>
#include <algorithm>
#include <set>

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
static float elevationAt(float x, float y, uint32_t seed) {
    float e = fbm(x * 0.016f, y * 0.016f, seed ^ 0x1eafu, 5);
    return (e - 0.5f) * 1.4f + 0.5f;
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

static void flattenSpecialSites(float x, float y, float& e, float& t, float& m) {
    flattenSite(x, y, 0.0f, 0.0f, 22.0f, 30.0f, e, t, m, 0.52f, 0.55f, 0.58f);
    flattenSite(x, y, (float)BOSS_CASTLE_TX, (float)BOSS_CASTLE_TY, 12.0f, 19.0f,
                e, t, m, 0.54f, 0.27f, 0.48f);
    flattenSite(x, y, (float)LOCKET_SITE_TX, (float)LOCKET_SITE_TY, 8.0f, 14.0f,
                e, t, m, 0.52f, 0.54f, 0.52f);
    flattenSite(x, y, (float)TOWER_SITE_TX, (float)TOWER_SITE_TY, 7.0f, 13.0f,
                e, t, m, 0.53f, 0.50f, 0.50f);
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
    if (y <= -15 && y >= BOSS_CASTLE_TY + 7) {
        float u = (float)(-15 - y) / (float)(-15 - (BOSS_CASTLE_TY + 7));
        int bend = (int)roundf(sinf(3.14159265f * u) * (4.5f * sinf(u * 12.566f) + 2.5f * sinf(u * 6.283f)));
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

static bool nearTrail(int tx, int ty, int r) {
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++)
            if (trailAt(tx + dx, ty + dy) != 0) return true;
    return false;
}

static GroundTile generateGround(int tx, int ty) {
    GroundTile out;
    float x = (float)tx, y = (float)ty;
    uint32_t seed = g.worldSeed;
    float e = elevationAt(x, y, seed), t = temperatureAt(x, y, seed), m = moistureAt(x, y);
    flattenSpecialSites(x, y, e, t, m);
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
        out.kind = wet ? (trail == 1 ? G_BRIDGE_NS : G_BRIDGE_EW) : G_DIRT;
        return out;
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
    return out;
}

GroundTile sampleGround(int tx, int ty) {
    struct Entry { int x = 0, y = 0; uint32_t seed = 0; bool valid = false; GroundTile tile; };
    static Entry cache[8192];
    Entry& c = cache[((uint32_t)ty * 131u + (uint32_t)tx) & 8191];
    if (!c.valid || c.x != tx || c.y != ty || c.seed != g.worldSeed)
        c = { tx, ty, g.worldSeed, true, generateGround(tx, ty) };
    return c.tile;
}

static bool groundWalkable(GroundKind k) {
    return k != G_WATER && k != G_DEEP && k != G_ROCK;
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
            return true;
        default:
            return false; // VD_PLANT, VD_BUSH, VD_FLOWER_BUSH, VD_LADDER stay walkable/brushable
    }
}

float villageDecoCenterX(const Village::Deco& d) {
    int dx = (int)floorf(d.x + 0.5f);
    if (d.kind == VD_WELL || d.kind == VD_BENCH || d.kind == VD_FLOWERBED || d.kind == VD_FENCE_GATE) {
        // 2-tile wide [dx-1 .. dx]: exact pixel midpoint is dx * TILE
        return (float)(dx * TILE);
    }
    // 1-tile [dx] or 3-tile [dx-1 .. dx+1]: exact pixel midpoint is (dx + 0.5) * TILE
    return ((float)dx + 0.5f) * (float)TILE;
}

// ================================================================ landmarks
static bool proceduralLandmark(int sx, int sy, Landmark& out) {
    const int sector = 128;
    uint32_t h = hash2i(sx, sy, g.worldSeed ^ 0x1a4d4u);
    if ((h % 100) >= 50) return false;
    int lx = sx * sector + sector / 2 + (int)(hashf(sx, sy, h) * 40) - 20;
    int ly = sy * sector + sector / 2 + (int)(hashf(sy, sx, h) * 40) - 20;
    if ((float)lx * lx + (float)ly * ly < 42.0f * 42.0f) return false;
    if ((abs(lx - BOSS_CASTLE_TX) < 20 && abs(ly - BOSS_CASTLE_TY) < 20) ||
        (abs(lx - LOCKET_SITE_TX) < 16 && abs(ly - LOCKET_SITE_TY) < 16) ||
        (abs(lx - TOWER_SITE_TX) < 15 && abs(ly - TOWER_SITE_TY) < 15)) return false;
    if (g.villageLoaded) {
        float dx = (float)lx - g.village.cx, dy = (float)ly - g.village.cy;
        if (dx * dx + dy * dy < 24.0f * 24.0f) return false;
    }
    GroundKind ground = sampleGround(lx, ly).kind;
    if (!groundWalkable(ground) || ground == G_ICE) return false;
    out = { (LandmarkKind)(1 + (h >> 6) % 5), lx, ly, h };
    return true;
}

static bool proceduralLandmarkNear(int tx, int ty, int radius, Landmark& out) {
    const int sector = 128;
    int sx = (int)floorf((float)tx / sector), sy = (int)floorf((float)ty / sector);
    uint32_t h = hash2i(sx, sy, g.worldSeed ^ 0x1a4d4u);
    if ((h % 100) >= 50) return false;
    int lx = sx * sector + sector / 2 + (int)(hashf(sx, sy, h) * 40) - 20;
    int ly = sy * sector + sector / 2 + (int)(hashf(sy, sx, h) * 40) - 20;
    if (abs(tx - lx) > radius || abs(ty - ly) > radius) return false;
    return proceduralLandmark(sx, sy, out);
}

void expandLandmarkParts(const Landmark& lm, std::vector<LandmarkPart>& out) {
    out.clear();
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

bool cellWalkable(int tx, int ty) {
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

PropKind propAt(int tx, int ty) {
    if (g.villageLoaded && inVillage((float)tx + 0.5f, (float)ty + 0.5f)) return P_NONE;

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

    Landmark lm;
    if (proceduralLandmarkNear(tx, ty, 5, lm)) return P_NONE;
    if (nearTrail(tx, ty, 1)) return P_NONE;

    GroundTile gt = sampleGround(tx, ty);
    if (gt.kind == G_DIRT || gt.kind == G_BRIDGE_NS || gt.kind == G_BRIDGE_EW ||
        gt.kind == G_COBBLE || gt.kind == G_FLOOR || gt.kind == G_SOIL ||
        gt.kind == G_WATER || gt.kind == G_DEEP || gt.kind == G_ICE)
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

std::string compassHint(float fromX, float fromY, float toX, float toY) {
    float dx = toX - fromX, dy = toY - fromY;
    int dist = (int)roundf(sqrtf(dx * dx + dy * dy));
    if (dist < 6) return "here";
    float ang = atan2f(dy, dx) * 180.0f / 3.14159265f;
    const char* dir = "E";
    if (ang >= -22.5f && ang < 22.5f) dir = "E";
    else if (ang >= 22.5f && ang < 67.5f) dir = "SE";
    else if (ang >= 67.5f && ang < 112.5f) dir = "S";
    else if (ang >= 112.5f && ang < 157.5f) dir = "SW";
    else if (ang >= 157.5f || ang < -157.5f) dir = "W";
    else if (ang >= -157.5f && ang < -112.5f) dir = "NW";
    else if (ang >= -112.5f && ang < -67.5f) dir = "N";
    else dir = "NE";
    char buf[32];
    snprintf(buf, sizeof(buf), "%s ~%d", dir, dist);
    return buf;
}

// ================================================================ villages
static bool siteFit(int cx, int cy, int R) {
    for (int dy = -R; dy <= R; dy++) {
        for (int dx = -R; dx <= R; dx++) {
            if (dx * dx + dy * dy > R * R) continue;
            GroundTile t = sampleGround(cx + dx, cy + dy);
            if (t.kind == G_WATER || t.kind == G_DEEP || t.kind == G_ROCK || t.kind == G_ICE ||
                t.kind == G_SWAMP || t.kind == G_BRIDGE_NS || t.kind == G_BRIDGE_EW || t.kind == G_DIRT)
                return false;
        }
    }
    return true;
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
        { -15, 6 }, { -12, 13 }, { 12, 14 }
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
        { -3, 12 }, { 3, 12 }
    };
    for (auto [tx, ty] : flowerSpots) {
        if (!cellReservedInVillage(v, tx, ty, 0)) v.flowers.push_back({ tx, ty });
    }

    dedupCells(v.paths);
    dedupCells(v.plaza);
    dedupCells(v.court);
    dedupCells(v.grassDark);
    dedupCells(v.soil);
    dedupCells(v.flowers);
    dedupCells(v.rocks);
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

    // 5-tile wide service buildings (shop, inn, temple) + staggered residential cottages
    Building shop{ 5, v.cx + 7 * side, v.cy + (plan == 0 ? 0 : -2), 5, 3, 5 };
    Building inn{ 6, v.cx - 7 * side, v.cy + (plan == 1 ? 3 : 1), 5, 3, 6 };
    Building temple{ 7, v.cx + (plan == 2 ? -2 * side : 0), v.cy - 9, 5, 4, 7 };
    v.buildings.push_back(shop);
    v.buildings.push_back(inn);
    v.buildings.push_back(temple);

    const std::pair<int, int> slots[] = {
        { -9 * side, -6 }, { 9 * side, -6 }, { -10 * side, 6 },
        { 8 * side, 7 },   { -4 * side, 8 }, { 4 * side, 9 }
    };
    int targetHouses = 3 + (int)(rs % 3);
    for (int i = 0; i < 6 && (int)v.buildings.size() < 3 + targetHouses; i++) {
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

    // Orthogonal L-shaped lanes to every doorstep + 4 cardinal village entrances
    for (size_t i = 0; i < v.buildings.size(); i++) {
        const Building& b = v.buildings[i];
        int dx = buildingDoorTx(b), dy = buildingDoorTy(b);
        paveStreetL(v.paths, v.cx, v.cy, dx, dy, (i % 2) == 0);
        v.paths.push_back({ dx - 1, dy });
        v.paths.push_back({ dx, dy });
    }
    paveStreetL(v.paths, v.cx, v.cy + 3, v.cx, v.cy + 16, false);
    paveStreetL(v.paths, v.cx - 4, v.cy, v.cx - 16, v.cy, true);
    paveStreetL(v.paths, v.cx + 4, v.cy, v.cx + 16, v.cy, true);

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

    const int SECTOR = 96;
    int sx = (int)floorf(x / SECTOR), sy = (int)floorf(y / SECTOR);
    struct Site { int sx, sy, x, y; uint32_t seed; };
    static std::vector<Site> sites;
    static int lastX, lastY;
    static uint32_t lastSeed;
    static bool surveyed = false;
    if (!surveyed || sx != lastX || sy != lastY || lastSeed != g.worldSeed) {
        surveyed = true; lastX = sx; lastY = sy; lastSeed = g.worldSeed; sites.clear();
        for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) {
            int cx = sx + dx, cy = sy + dy;
            uint32_t h = hash2i(cx, cy, g.worldSeed ^ 0x71a9eu);
            if (h % 1000 >= 320) continue;
            int bx = cx * SECTOR + SECTOR / 2, by = cy * SECTOR + SECTOR / 2;
            bool ok = false;
            for (int r = 0; r < 40 && !ok; r += 3) for (int a = 0; a < (r ? 16 : 1) && !ok; ++a) {
                float ang = a * (6.2831853f / 16);
                int tx = bx + (int)(cosf(ang) * r), ty = by + (int)(sinf(ang) * r);
                if (tx * (float)tx + ty * (float)ty < 50 * 50 ||
                    (abs(tx - BOSS_CASTLE_TX) < 29 && abs(ty - BOSS_CASTLE_TY) < 29) ||
                    (abs(tx - LOCKET_SITE_TX) < 23 && abs(ty - LOCKET_SITE_TY) < 23) ||
                    (abs(tx - TOWER_SITE_TX) < 23 && abs(ty - TOWER_SITE_TY) < 23)) continue;
                if (siteFit(tx, ty, 15)) { sites.push_back({ cx, cy, tx, ty, h }); ok = true; }
            }
        }
    }
    Village best;
    bool found = false;
    int bestDist = 48;
    for (const auto& s : sites) {
        int d = std::max(abs(s.x - (int)floorf(x)), abs(s.y - (int)floorf(y)));
        if (d >= bestDist) continue;
        bestDist = d; found = true; best.valid = true; best.cx = s.x; best.cy = s.y;
        best.sectorX = s.sx; best.sectorY = s.sy; best.seed = s.seed;
    }
    if (!found) { g.villageLoaded = false; g.villageKey = ~0ull; return; }

    uint64_t key = ((uint64_t)(uint32_t)best.sectorX << 32) | (uint32_t)best.sectorY;
    if (g.villageLoaded && g.villageKey == key) return;
    best.name = villageNameFor(best.sectorX, best.sectorY, g.worldSeed);
    layoutVillage(best);
    g.village = best;
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
};
static const char* ELDER_LINES[] = {
    "You have the look of someone the road has been waiting for.",
    "Old kings built towers to be remembered. The towers remember them better than we do.",
    "When the moon is fat, the wisps walk the frozen reach. Keep your ether close.",
};
static const char* CHILD_LINES[] = {
    "Are you an adventurer? Do you have a SWORD? Can I see it?",
    "I caught a slime once! It was mostly jelly. Mum made me put it back.",
    "The well echoes if you shout into it. The echo knows your name.",
};
static const char* GUARD_LINES[] = {
    "Keep your blade sheathed in town and we'll get along fine.",
    "North road climbs to the old castle. West lies a ruined watchtower.",
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

const char* npcLine(const Npc& npc, int index) {
    if (npc.sprite == NS_CAT) return "The cat blinks slowly, then looks away.";
    if (npc.sprite == NS_CHICKEN) return "Cluck! She goes back to the turnips.";
    if (npc.sprite == NS_SHEEP) return SHEEP_LINES[(unsigned)index % 2u];
    const char** set = VILLAGER_LINES;
    int n = 8;
    switch (npc.role) {
        case NPC_ELDER: set = ELDER_LINES; n = 3; break;
        case NPC_CHILD: set = CHILD_LINES; n = 3; break;
        case NPC_GUARD: set = GUARD_LINES; n = 2; break;
        case NPC_SHOPKEEP: set = SHOP_LINES; n = 1; break;
        case NPC_INNKEEP: set = INN_LINES; n = 1; break;
        case NPC_PRIEST: set = PRIEST_LINES; n = 1; break;
        case NPC_ANIMAL: set = ANIMAL_LINES; n = 4; break;
        case NPC_QUEST: set = QUEST_LINES; n = 1; break;
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
        default: return "Villager";
    }
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
        }

        if (v.startVillage) {
            // Animals in their proper yards & patios
            add(NS_CHICKEN, -1,       NPC_ANIMAL,  -10.5f, 10.5f, "Hen",      0);
            add(NS_CHICKEN, -1,       NPC_ANIMAL,  -11.5f, 11.5f, "Hen",      1);
            add(NS_SHEEP,   -1,       NPC_ANIMAL,   11.5f, 10.5f, "Clover",   0);
            add(NS_SHEEP,   -1,       NPC_ANIMAL,   12.5f, 11.5f, "Woolly",   1);
            add(NS_CAT,     -1,       NPC_ANIMAL,   11.5f,  2.5f, "Inn Cat",  0);
            add(NS_DOG,     FACE_DOG, NPC_ANIMAL,    2.5f,  1.5f, "Town Dog", 0);

            // Named Elmswick townsfolk & questgivers in their district spots
            add(NS_ELDER,     FACE_ELDER,     NPC_ELDER,     1.5f, -1.5f, "Old Maren",       0, QUEST_CROWN);
            add(NS_WOMAN,     FACE_WOMAN,     NPC_QUEST,    -2.5f,  1.5f, "Tamsin",          1, QUEST_LOCKET);
            add(NS_CHILD,     FACE_CHILD,     NPC_CHILD,    -1.5f,  3.5f, "Pip the Younger", 2);
            add(NS_GUARD,     FACE_GUARD,     NPC_GUARD,     2.5f, 10.5f, "Watchman Odo",    3);
            add(NS_VILLAGER1, FACE_VILLAGER1, NPC_VILLAGER, -6.5f,  1.5f, "Rook",            4);
            add(NS_VILLAGER2, FACE_VILLAGER2, NPC_VILLAGER,  6.5f,  3.5f, "Sable",           5);
            add(NS_VILLAGER3, FACE_VILLAGER3, NPC_VILLAGER, -8.5f, -5.5f, "Wren",            6);
            add(NS_FARMER,    FACE_SMITH,     NPC_QUEST,    -6.5f, 10.5f, "Farmer Hedrick",  0, QUEST_SLIMES);
        } else {
            const int sprites[] = { NS_VILLAGER0, NS_VILLAGER1, NS_VILLAGER2, NS_VILLAGER3 };
            const int faces[] = { FACE_VILLAGER0, FACE_VILLAGER1, FACE_VILLAGER2, FACE_VILLAGER3 };
            const char* names[] = { "Elder Rowan", "Kip", "Guard Bram", "Elin", "Corin", "Mina" };
            int n = 5 + (int)(v.seed % 3);
            for (int i = 0; i < n; i++) {
                float ang = hashf(i, 11, v.seed) * 6.2831853f;
                float r = 2.5f + hashf(i, 13, v.seed) * 5.0f;
                float px = (float)v.cx + cosf(ang) * r;
                float py = (float)v.cy + sinf(ang) * r * 0.8f;
                int role = (i == 0) ? NPC_ELDER : (i == 1 ? NPC_CHILD : (i == 2 ? NPC_GUARD : NPC_VILLAGER));
                int sprite = (role == NPC_ELDER) ? NS_ELDER : (role == NPC_CHILD ? NS_CHILD : (role == NPC_GUARD ? NS_GUARD : sprites[i % 4]));
                int face = (role == NPC_ELDER) ? FACE_ELDER : (role == NPC_CHILD ? FACE_CHILD : (role == NPC_GUARD ? FACE_GUARD : faces[i % 4]));
                add(sprite, face, role, px, py, names[i % 6], i);
            }
            add(NS_DOG, FACE_DOG, NPC_ANIMAL, (float)v.cx + 2.5f, (float)v.cy + 1.5f, "Village Dog", 0);
            if ((v.seed & 1) == 0)
                add(NS_SHEEP, -1, NPC_ANIMAL, (float)v.cx - 3.5f, (float)v.cy + 2.5f, "Village Sheep", 0);
        }
    } else {
        uint32_t h = hash2i(sx, sy, g.worldSeed ^ 0x4e3c17u);
        if ((h % 100) < 25 && (x * x + y * y) > 45 * 45) {
            float px = sx * 32 + 8.0f + hashf(sx, sy, h) * 14.0f;
            float py = sy * 32 + 8.0f + hashf(sy, sx, h) * 14.0f;
            int tx = (int)floorf(px), ty = (int)floorf(py);
            if (cellWalkable(tx, ty))
                add(NS_VILLAGER3, FACE_RANGER, NPC_VILLAGER, px, py, "Wandering Ranger", (int)(h % 8));
        }
    }
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

bool landmarkNear(int tx, int ty, Landmark& out) {
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
    if (!proceduralLandmarkNear(tx, ty, 8, out)) return false;
    return true;
}

void landmarksInView(int x0, int y0, int x1, int y1, std::vector<Landmark>& out) {
    out.clear();
    auto maybeAdd = [&](LandmarkKind kind, int tx, int ty, uint32_t seed) {
        if (tx >= x0 - 6 && tx <= x1 + 6 && ty >= y0 - 8 && ty <= y1 + 8)
            out.push_back({ kind, tx, ty, seed });
    };
    maybeAdd(LK_CASTLE, BOSS_CASTLE_TX, BOSS_CASTLE_TY, 0xca571eu);
    maybeAdd(LK_STONES, LOCKET_SITE_TX, LOCKET_SITE_TY, 0x10c4e7u);
    maybeAdd(LK_TOWER,  TOWER_SITE_TX,  TOWER_SITE_TY,  0x703e2u);

    const int SECTOR = 128;
    int sx0 = (int)floorf((float)(x0 - 8) / SECTOR);
    int sx1 = (int)floorf((float)(x1 + 8) / SECTOR);
    int sy0 = (int)floorf((float)(y0 - 8) / SECTOR);
    int sy1 = (int)floorf((float)(y1 + 8) / SECTOR);
    for (int sy = sy0; sy <= sy1; sy++) {
        for (int sx = sx0; sx <= sx1; sx++) {
            Landmark lm;
            if (proceduralLandmark(sx, sy, lm))
                maybeAdd(lm.kind, lm.tx, lm.ty, lm.seed);
        }
    }
}

bool inBossArena(float x, float y) {
    float dx = x - (float)BOSS_CASTLE_TX, dy = y - (float)BOSS_CASTLE_TY;
    return (dx * dx + dy * dy) < 16.0f * 16.0f;
}

// ================================================================ field monsters
EnemyKind rollBiomeEnemy(int tx, int ty, int zoneLevel) {
    static const EnemyKind common[BI_COUNT][4] = {
        { EN_SLIME, EN_SLIME, EN_IMP, EN_BAT }, { EN_BAT, EN_IMP, EN_WOLF, EN_BAT },
        { EN_HORNET, EN_SLIME, EN_HORNET, EN_WOLF }, { EN_SLIME, EN_WISP, EN_BAT, EN_WISP },
        { EN_WOLF, EN_IMP, EN_WOLF, EN_BAT }, { EN_WOLF, EN_WISP, EN_WOLF, EN_WISP },
        { EN_HORNET, EN_IMP, EN_BAT, EN_IMP }
    };
    static const EnemyKind frontier[BI_COUNT][2] = {
        { EN_WOLF, EN_ORC }, { EN_GHOST, EN_WOLF }, { EN_WOLF, EN_HORNET }, { EN_GHOST, EN_SKELETON },
        { EN_ORC, EN_SKELETON }, { EN_WISP, EN_GHOST }, { EN_SKELETON, EN_ORC }
    };
    static const EnemyKind far[BI_COUNT] = {
        EN_NIGHT_RIDER, EN_NIGHT_RIDER, EN_SERPENT, EN_SERPENT, EN_GOLEM, EN_GOLEM, EN_SERPENT
    };
    auto biome = sampleGround(tx, ty).biome;
    auto h = hash2i(tx, ty, g.worldSeed ^ 0xbad1ceu);
    if (zoneLevel >= 12 && h % 3 != 0) return far[biome];
    if (zoneLevel >= 3 && (h % 3 == 0 || zoneLevel >= 7)) return frontier[biome][(h >> 8) % 2];
    return common[biome][(h >> 8) % 4];
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
    int avg = partyLevel();
    return std::max(1, std::min(zl + (avg - 1) / 3, 26));
}

const char* zoneNameAt(float x, float y) {
    if (fabsf(x) < 11 && y > 15 && y < 24) return "Millbrook Crossing";
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
constexpr int NAV_MAX_EXPAND = 9000;
constexpr int NAV_MAX_RADIUS = 44;
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
