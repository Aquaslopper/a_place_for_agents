// world.cpp — noise, biomes, villages, props, npcs, chests, landmarks, A*
#include "world.hpp"
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <queue>
#include <limits>
#include <string>
#include <algorithm>

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
    // colder far to the north, warmer to the south
    t += (y < 0 ? -0.05f : 0.04f);
    return t;
}
float moistureAt(float x, float y) {
    return fbm(x * 0.022f + 91.7f, y * 0.022f + 17.3f, g.worldSeed ^ 0x5eedu, 4);
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

// Keep Elmswick and the three quest landmark clearings always walkable on any seed.
static void flattenSpecialSites(float x, float y, float& e, float& t, float& m) {
    // Elmswick meadow
    flattenSite(x, y, 0.0f, 0.0f, 26.0f, 34.0f, e, t, m, 0.52f, 0.55f, 0.58f);
    // Northern Castle courtyard
    flattenSite(x, y, (float)BOSS_CASTLE_TX, (float)BOSS_CASTLE_TY, 12.0f, 19.0f,
                e, t, m, 0.54f, 0.27f, 0.48f);
    // Standing Stones (Lost Locket)
    flattenSite(x, y, (float)LOCKET_SITE_TX, (float)LOCKET_SITE_TY, 8.0f, 14.0f,
                e, t, m, 0.52f, 0.54f, 0.52f);
    // Old Watchtower (Rusty Key)
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

// A river, an orchard road, and three readable journeys out of town. Climate
// still controls the far world; nearby story sites have distinctive catchments
// on *every* seed. Biome is retained even on a road, bridge or patch of snow.
static Biome climate(int tx, int ty, float e, float t, float m, float patch) {
    auto basin = [&](int cx, int cy, float rx, float ry) {
        float dx = (tx-cx)/rx, dy = (ty-cy)/ry;
        return dx*dx + dy*dy < 0.86f + 0.28f*patch;
    };
    if (abs(tx) < 18 && ty > -17 && ty < 19) return BI_MEADOW;
    if (basin(32, 39, 30, 23)) return BI_AMBER;
    if (basin(-44, -11, 30, 19)) return BI_FEN;
    if (basin(-26, 64, 30, 21)) return BI_DUNES;
    if (basin(0, -113, 40, 29)) return BI_FROST;
    if (basin(0, -80, 32, 19)) return BI_HEATH;
    if (basin(0, -43, 26, 22)) return BI_FOREST;
    if (tx*float(tx) + ty*float(ty) < 26*26) return BI_MEADOW;
    if (t < 0.33f || (t < 0.50f && e > 0.78f)) return BI_FROST;
    if (t > 0.58f && m < 0.47f) return BI_DUNES;
    if (m > 0.56f && e < 0.47f) return BI_FEN;
    if (e > 0.64f) return BI_HEATH;
    if (t > 0.48f && m < 0.40f) return BI_AMBER;
    if (m > 0.54f) return BI_FOREST;
    return BI_MEADOW;
}

// 0 = no trail, 1 = N/S planks, 2 = E/W planks. Two cells wide even on
// bends. Trails reach the *approach* of each quest object, not just its sector.
static int trailAt(int x, int y) {
    if (y <= -15 && y >= BOSS_CASTLE_TY+7) {
        float u = (float)(-15-y)/(-15-BOSS_CASTLE_TY-7);
        int bend = (int)roundf(sinf(3.14159f*u)*(5*sinf(u*13.f)+3*sinf(u*7.f)));
        if (x==bend || x==bend+1) return 1;
    }
    if ((x==0 || x==1) && y>=11 && y<=25) return 1;
    if (y>=25 && y<=64) {  // the dry road forks off on the south bank
        float u=(y-25)/39.f;
        int bend=(int)roundf(-26.f*fade(u));
        if (x==bend || x==bend+1) return 1;
    }
    if (x >= 0 && x <= LOCKET_SITE_TX) {
        float u = (float)x / LOCKET_SITE_TX;
        int cy = 24 + (int)roundf(24*fade(u) + 2*sinf(u*12.56f)*sinf(u*3.14f));
        if (y == cy || y == cy+1) return 2;
    }
    if (x <= -8 && x >= TOWER_SITE_TX) {
        float u = (float)(-x-8) / (-TOWER_SITE_TX-8);
        int cy = (int)roundf((TOWER_CHEST_TY+1)*fade(u) + 2*sinf(u*9.42f));
        if (y == cy || y == cy+1) return 2;
    }
    return 0;
}

static GroundTile generateGround(int tx, int ty) {
    GroundTile out;
    float x = (float)tx, y = (float)ty;
    uint32_t seed = g.worldSeed;
    float e = elevationAt(x,y,seed), t = temperatureAt(x,y,seed), m = moistureAt(x,y);
    flattenSpecialSites(x,y,e,t,m);
    float patch = smoothNoise(x*0.095f, y*0.095f, seed ^ 0x4921u);
    uint32_t h = hash2i(tx,ty,seed ^ 0x9a11u);
    out.biome = climate(tx,ty,e,t,m,patch);
    out.variant = (uint8_t)((h >> 8) % 3);
    const GroundKind soils[] = {G_GRASS, G_GRASS_DARK, G_AMBER, G_SWAMP, G_HEATH, G_SNOW, G_SAND};
    out.kind = soils[out.biome];
    // Composition within a biome: pools in reed beds, bare clearings under
    // amber canopies, wind-scoured flats backed by raised sandstone shelves.
    if (out.biome==BI_AMBER && patch>0.56f) out.kind=G_LEAVES;
    if (out.biome==BI_DUNES && patch>0.47f) out.kind=G_RIPPLES;
    if (out.biome==BI_FOREST && patch<0.38f) out.kind=G_GRASS;
    if (out.biome==BI_FROST) {
        if (patch<0.25f && e<0.55f) out.kind=G_ICE;
        else if (patch>0.66f && !nearSpecialSite(tx,ty)) out.kind=G_ROCK;
    } else if (out.biome==BI_FEN) {
        if (patch<0.23f || e<0.28f) out.kind=G_WATER;
        else if (patch>0.63f) out.kind=G_GRASS_DARK;
    } else if (e<0.325f) out.kind=e<0.265f ? G_DEEP : G_WATER;
    else if (e<0.368f && out.biome!=BI_DUNES) out.kind=G_BEACH;
    else if (e>0.755f || (out.biome==BI_HEATH && patch>0.68f) ||
             (out.biome==BI_DUNES && patch>0.71f)) out.kind=G_ROCK;

    // The Millbrook winds past Elmswick's south gardens. Crossings are
    // structural (walkable bridge cells), not a picture painted over water.
    int sector = (int)floorf((x + 32) / 64);
    int ford = sector * 64 + (sector ? (int)(hash2i(sector, 3, seed) % 19) - 9 : 0);
    float fx = tx == ford+1 ? (float)ford : x;  // level both abutments
    float brook = 17.f + 1.8f*sinf(fx*0.14f) + std::min(1.f, fabsf(fx)/60.f)*
                  (smoothNoise(fx*0.021f, 31.7f, seed^0x7368u)-0.5f)*24.f;
    float bank = fabsf(y-brook);
    if (bank < 2.15f && !nearSpecialSite(tx,ty)) out.kind = G_WATER;
    int trail = trailAt(tx,ty);
    if ((tx == ford || tx == ford+1) && bank < 5.0f) trail = 1;
    if (trail) {
        bool wet = out.kind == G_WATER || out.kind == G_DEEP;
        out.kind = wet ? (trail == 1 ? G_BRIDGE_NS : G_BRIDGE_EW) : G_DIRT;
        return out;
    }
    if (abs(tx-BOSS_CASTLE_TX) <= 6 && ty >= BOSS_CASTLE_TY-4 && ty <= BOSS_CASTLE_TY+7) {
        out.kind = (abs(tx-BOSS_CASTLE_TX)<=2 && ty>BOSS_CASTLE_TY) ? G_COBBLE : G_FLOOR;
        return out;
    }
    if (nearSpecialSite(tx,ty) && (out.kind == G_ROCK || out.kind == G_WATER || out.kind == G_DEEP))
        out.kind = soils[out.biome];
    if (((tx>=-13 && tx<=-8) || (tx>=9 && tx<=13)) && ty>=10 && ty<=12) {
        out.kind = G_SOIL; out.deco = 5; return out;
    }
    float d = (float)(h & 0xffff)/65536.f;
    out.decoVariant = (uint8_t)((h>>16)%3);
    if (out.kind == G_GRASS || out.kind == G_GRASS_DARK) {
        if (patch > 0.57f && d < 0.15f) out.deco = 2;
        else if (patch > 0.32f && d < 0.11f) out.deco = 1;
    } else if (out.kind == G_WATER && (out.biome == BI_FEN || bank < 2.2f) && d < 0.085f) out.deco = 4;
    else if ((out.kind == G_HEATH || out.kind == G_BEACH || out.kind == G_SAND) && d < 0.055f) out.deco = 3;
    return out;
}

GroundTile sampleGround(int tx, int ty) {
    // A bounded direct-mapped cache: sampling never depends on the camera,
    // loaded village or visit order. Shared by navigation, painting and music.
    struct Entry { int x=0,y=0; uint32_t seed=0; bool valid=false; GroundTile tile; };
    static Entry cache[8192];
    Entry& c = cache[((uint32_t)ty*131u + (uint32_t)tx) & 8191];
    if (!c.valid || c.x!=tx || c.y!=ty || c.seed!=g.worldSeed)
        c = {tx,ty,g.worldSeed,true,generateGround(tx,ty)};
    return c.tile;
}
static bool groundWalkable(GroundKind k) {
    return k != G_WATER && k != G_DEEP && k != G_ROCK;
}

bool villageDecoSolid(int kind) {
    switch (kind) {
        case VD_SHADE_TREE:
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
        case VD_TEMPLE_PILLAR:
        case VD_CRATE:
        case VD_TABLE:
        case VD_BUCKET:
        case VD_FLOWERBED:
            return true;
        default:
            return false;
    }
}

// Resolve a procedural landmark identically for rendering, collision and props.
// Sector candidates are intentionally independent of visit order.
static bool proceduralLandmark(int sx, int sy, Landmark& out) {
    const int sector = 128;
    uint32_t h = hash2i(sx, sy, g.worldSeed ^ 0x1a4d4u);
    if ((h % 100) >= 55) return false;
    int lx = sx * sector + sector / 2 + (int)(hashf(sx, sy, h) * 40) - 20;
    int ly = sy * sector + sector / 2 + (int)(hashf(sy, sx, h) * 40) - 20;
    if ((float)lx * lx + (float)ly * ly < 42.0f * 42.0f) return false;
    // Leave room for the full story-site clearings, not just their centres.
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
    // Candidates are at least 44 tiles from a sector edge; a single sector
    // suffices for the small footprint/clearing radii used here.
    uint32_t h = hash2i(sx, sy, g.worldSeed ^ 0x1a4d4u);
    if ((h % 100) >= 55) return false;
    int lx = sx * sector + sector / 2 + (int)(hashf(sx, sy, h) * 40) - 20;
    int ly = sy * sector + sector / 2 + (int)(hashf(sy, sx, h) * 40) - 20;
    if (abs(tx - lx) > radius || abs(ty - ly) > radius) return false;
    return proceduralLandmark(sx, sy, out);
}

static bool landmarkCellBlocked(int tx, int ty) {
    // Northern Castle keep (anchored at BOSS_CASTLE_TX, BOSS_CASTLE_TY)
    if (tx >= BOSS_CASTLE_TX - 2 && tx <= BOSS_CASTLE_TX + 2 &&
        ty >= BOSS_CASTLE_TY - 3 && ty <= BOSS_CASTLE_TY)
        return true;
    // Standing Stones centre statue
    if (tx == LOCKET_SITE_TX && ty == LOCKET_SITE_TY) return true;
    // Old Watchtower base
    if (tx >= TOWER_SITE_TX - 1 && tx <= TOWER_SITE_TX + 1 &&
        ty >= TOWER_SITE_TY - 1 && ty <= TOWER_SITE_TY)
        return true;
    Landmark lm;
    if (proceduralLandmarkNear(tx, ty, 4, lm)) {
        int dx = tx - lm.tx, dy = ty - lm.ty;
        switch (lm.kind) {
            case LK_TOWER: return abs(dx) <= 1 && dy >= -1 && dy <= 0;
            case LK_CASTLE: return abs(dx) <= 2 && dy >= -3 && dy <= 0;
            case LK_STONES: return dy == 0 && (dx == 0 || abs(dx) == 2);
            case LK_CAMP: return dy == 0 && abs(dx) <= 1;
            case LK_RUINS: return dy == 0 && abs(dx) <= 2;
            default: break;
        }
    }
    return false;
}

bool cellWalkable(int tx, int ty) {
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
            int x0 = b.tx - b.w / 2, y0 = b.ty - b.h + 1;
            if (tx >= x0 && tx <= x0 + b.w - 1 && ty >= y0 && ty <= b.ty) return false;
        }
        for (const auto& d : g.village.decos) {
            if (!villageDecoSolid(d.kind)) continue;
            int dx = (int)floorf(d.x + 0.5f), dy = (int)floorf(d.y + 0.5f);
            if (d.kind == VD_WELL || d.kind == VD_FLOWERBED || d.kind == VD_BENCH) {
                // 2-tile wide street furniture centered on d.x
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

bool inVillage(float x, float y) {
    if (!g.villageLoaded) return false;
    float dx = x - g.village.cx, dy = y - g.village.cy;
    return dx * dx + dy * dy < 18.0f * 18.0f;
}
bool nearStartVillage(float x, float y) {
    return (x * x + y * y) < 30.0f * 30.0f;
}

// ================================================================ props
// A local priority test keeps tree trunks apart without depending on generation
// order or chunk boundaries. Other, smaller plants may grow between them.
static bool treeCandidate(int tx, int ty) {
    uint32_t priority = hash2i(tx, ty, g.worldSeed ^ 0x4f3e57u);
    if (priority > 0x70000000u) return false;
    for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) {
        if (!dx && !dy) continue;
        uint32_t other = hash2i(tx + dx, ty + dy, g.worldSeed ^ 0x4f3e57u);
        if (other < priority || (other == priority && (dy < 0 || (dy == 0 && dx < 0))))
            return false;
    }
    return true;
}

PropKind propAt(int tx, int ty) {
    if (g.villageLoaded && inVillage((float)tx + 0.5f, (float)ty + 0.5f)) return P_NONE;
    if (nearSpecialSite(tx, ty)) return P_NONE;
    Landmark lm;
    if (proceduralLandmarkNear(tx, ty, 4, lm)) return P_NONE;
    GroundTile gt = sampleGround(tx, ty);
    uint32_t h = hash2i(tx, ty, g.worldSeed ^ 0x77ee5u);
    float hf = (float)h / 4294967295.0f;
    float grove = smoothNoise(tx * 0.065f, ty * 0.065f, g.worldSeed ^ 0x61a4u);
    switch (gt.kind) {
        case G_GRASS:
        case G_GRASS_DARK:
            if (grove > (gt.biome == BI_FOREST ? 0.17f : 0.54f) && treeCandidate(tx, ty)) {
                if (gt.biome == BI_FOREST && ((h>>8)%3)) return (PropKind)(P_PINE0 + h%3);
                return (PropKind)(P_TREE0 + h%3);
            }
            if (hf < 0.012f) return P_ROCK0;
            if (hf < 0.018f + grove * 0.046f) return P_BUSH_PLAIN;
            if (hf > 0.993f && grove > 0.48f) return P_STUMP;
            if (hf > 0.96f && gt.biome == BI_FOREST) return P_MUSHROOM;
            if (hf > 0.982f && grove < 0.50f) return P_BUSH;
            break;
        case G_AMBER:
        case G_LEAVES:
            if (grove > 0.21f && treeCandidate(tx,ty)) return (h%4 ? P_AMBER_OAK : P_COPPER_OAK);
            if (hf < 0.022f) return P_MUSHROOM;
            if (hf > 0.986f) return P_STUMP;
            break;
        case G_SNOW:
            if (grove > 0.33f && treeCandidate(tx,ty)) return P_TREE_SNOW;
            if (hf > 0.986f) return P_CRYSTAL;
            break;
        case G_HEATH:
            if (grove > 0.44f && treeCandidate(tx,ty)) return P_PINE2;
            if (hf > 0.974f) return P_ROCK2;
            break;
        case G_ROCK:
            if (hf < 0.045f) return P_BOULDER;
            if (hf > 0.987f) return P_CRYSTAL;
            break;
        case G_SAND:
        case G_RIPPLES:
            if (hf < 0.035f && grove > 0.32f) return P_YUCCA;
            if (hf > 0.986f) return P_ROCK1;
            break;
        case G_SWAMP:
            if (grove > 0.42f && treeCandidate(tx,ty)) return h%3 ? P_WILLOW : P_DEADTREE;
            if (hf < 0.09f + grove*0.08f) return P_REEDS;
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
        case VD_GATE: t = &g.assets.gate; break;
        case VD_TEMPLE_PILLAR: t = &g.assets.pillar; break;
        case VD_HOUSE: t = (variant >= 0 && variant < 5) ? &g.assets.house[variant] : &g.assets.house[0]; break;
        case VD_SHADE_TREE: {
            switch (sampleGround(g.village.cx, g.village.cy).biome) {
                case BI_AMBER: t = variant%3 ? &g.assets.amberOak : &g.assets.copperOak; break;
                case BI_FOREST: case BI_HEATH: t = &g.assets.pine[variant%3]; break;
                case BI_FROST: t = &g.assets.treeSnow; break;
                case BI_FEN: t = &g.assets.willow; break;
                case BI_DUNES: t = &g.assets.yucca; break;
                default: t = &g.assets.tree[variant%3]; break;
            }
            break;
        }
        case VD_CRATE: t = &g.assets.crate; break;
        case VD_TABLE: t = &g.assets.table; break;
        case VD_BUCKET: t = &g.assets.bucket; break;
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
    // atan2(dy, dx): 0=E, +pi/2=S, -pi/2=N, pi=W
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
    int ax0 = a.tx - a.w / 2 - pad, ax1 = a.tx - a.w / 2 + a.w - 1 + pad;
    int ay0 = a.ty - a.h + 1 - pad, ay1 = a.ty + 1 + pad;
    int bx0 = b.tx - b.w / 2,       bx1 = b.tx - b.w / 2 + b.w - 1;
    int by0 = b.ty - b.h + 1,       by1 = b.ty + 1;
    return !(ax1 < bx0 || ax0 > bx1 || ay1 < by0 || ay0 > by1);
}

static bool decoHitsBuildingOrDoor(const Village& v, float dx, float dy) {
    int tx = (int)floorf(dx + 0.5f), ty = (int)floorf(dy + 0.5f);
    for (const Building& b : v.buildings) {
        int x0 = b.tx - b.w / 2 - 1, x1 = b.tx - b.w / 2 + b.w;
        int y0 = b.ty - b.h, y1 = b.ty + 2;
        if (tx >= x0 && tx <= x1 && ty >= y0 && ty <= y1) return true;
    }
    return false;
}

void buildStartVillage(Village& v, uint32_t seed) {
    v = Village{};
    v.valid = true;
    v.startVillage = true;
    v.cx = 0; v.cy = 0;
    v.name = "Elmswick";
    v.seed = seed ^ 0x5171u;

    auto addHouse = [&](int kind, int tx, int ty) {
        Building b;
        b.kind = kind;
        b.tx = tx; b.ty = ty;
        b.w = (kind == 7) ? 5 : 4;
        b.h = (kind == 7) ? 4 : 3;
        b.sprite = kind;
        v.buildings.push_back(b);
    };
    addHouse(5, -8, -1);      // shop, west of the plaza
    addHouse(6, 8, -1);       // inn, east of the plaza
    addHouse(7, 0, -11);      // temple, north
    addHouse(0, -12, -8);
    addHouse(1, 12, -8);
    addHouse(2, -12, 5);
    addHouse(3, 12, 5);
    addHouse(4, -5, 10);
    addHouse(0, 6, 10);

    // plaza cobbles
    for (int ty = -4; ty <= 4; ty++)
        for (int tx = -5; tx <= 5; tx++) {
            float dx = (float)tx / 5.0f, dy = (float)ty / 4.0f;
            if (dx * dx + dy * dy <= 1.0f) v.plaza.push_back({ tx, ty });
        }

    auto road = [&](int x0, int y0, int x1, int y1) {
        int dx = abs(x1 - x0), dy = abs(y1 - y0);
        int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
        int err = dx - dy, x = x0, y = y0;
        for (int guard = 0; guard < 96; guard++) {
            v.paths.push_back({ x, y });
            v.paths.push_back({ x + 1, y });
            if (x == x1 && y == y1) break;
            int e2 = 2 * err;
            if (e2 > -dy) { err -= dy; x += sx; }
            if (e2 < dx) { err += dx; y += sy; }
        }
    };
    // clean roads from the plaza to every building doorstep
    road(-5, 0, -8, 0);
    road(5, 0, 8, 0);
    road(0, -4, 0, -10);
    road(-8, 0, -12, 6);
    road(8, 0, 12, 6);
    road(-8, -5, -12, -7);
    road(8, -5, 12, -7);
    road(0, 4, -5, 11);
    road(0, 4, 6, 11);
    // the gate road running south out of town, and north path around the temple
    road(0, 4, 0, 17);
    road(1, -8, 4, -10);
    road(4, -10, 4, -15);
    road(4, -15, 0, -16);

    auto deco = [&](int kind, float x, float y, int variant = 0) {
        v.decos.push_back({ kind, x, y, variant });
    };
    deco(VD_WELL, 0.0f, 0.0f);
    deco(VD_BUCKET, 2.0f, 0.0f);
    deco(VD_SIGN, 3.0f, 5.0f);
    deco(VD_SIGN, -3.0f, 6.0f);
    deco(VD_BENCH, -3.0f, 2.0f);
    deco(VD_BENCH, 3.0f, 2.0f);
    deco(VD_FLOWERBED, -4.0f, -3.0f);
    deco(VD_FLOWERBED, 5.0f, -3.0f);
    deco(VD_TORCH, -6.0f, 3.0f);
    deco(VD_TORCH, 6.0f, 3.0f);
    deco(VD_STATUE, -3.0f, -6.0f);
    deco(VD_STATUE, 3.0f, -6.0f);
    deco(VD_PLANT, -10.0f, 2.0f);
    deco(VD_PLANT, 10.0f, 2.0f);
    deco(VD_BARREL, -11.0f, -1.0f);
    deco(VD_BARREL, 11.0f, -1.0f);
    deco(VD_CRATE, 8.0f, 4.0f);
    deco(VD_CAMPFIRE, -8.0f, 8.0f);
    deco(VD_CAULDRON, 9.0f, 8.0f);
    deco(VD_TABLE, -7.0f, 4.0f);
    deco(VD_FENCE_H, -14.0f, 8.0f);
    deco(VD_FENCE_H, -13.0f, 8.0f);
    deco(VD_FENCE_H, 13.0f, 8.0f);
    deco(VD_FENCE_V, -14.0f, 9.0f);
    deco(VD_FENCE_V, 14.0f, 9.0f);
    // Shelter belts, kitchen plots and two shady places to stop by the well.
    for (const auto& p : std::vector<std::pair<int,int>>{{-15,-3},{-14,1},{-9,-6},{-5,-5},
             {5,-5},{9,-7},{14,0},{15,3},{-9,8},{9,9},{-15,12},{14,12},{-5,14},{6,14}})
        deco(VD_SHADE_TREE, (float)p.first, (float)p.second, (abs(p.first)+abs(p.second))%3);
    for (int x = -13; x <= -9; ++x) deco(VD_FENCE_H, (float)x, 13);
    for (int x = 10; x <= 13; ++x) deco(VD_FENCE_H, (float)x, 13);
}

// Generated villages --------------------------------------------------------
static void layoutVillage(Village& v) {
    uint32_t rs = v.seed;
    v.buildings.clear();

    // A market lane, a hill court or a compact green; mirror/stagger the
    // services without moving any door off its own road.
    int plan = rs % 3, side = (rs & 8) ? 1 : -1;
    Building shop{ 5, v.cx + 7*side, v.cy + (plan == 0 ? 1 : -3), 4, 3, 5 };
    Building inn{ 6, v.cx - 7*side, v.cy + (plan == 1 ? 4 : 0), 4, 3, 6 };
    Building temple{ 7, v.cx + (plan == 2 ? -3*side : 0), v.cy - 8, 5, 4, 7 };
    v.buildings.push_back(shop);
    v.buildings.push_back(inn);
    v.buildings.push_back(temple);

    int targetHouses = 3 + (int)(rs % 3);
    for (int i = 0; i < targetHouses * 3 && (int)v.buildings.size() < 3 + targetHouses; i++) {
        float ang = hashf(i, 71, rs) * 6.2831853f;
        int dist = 8 + (int)(hashf(i, 77, rs) * 4.0f);
        Building b;
        b.w = 4; b.h = 3;
        b.tx = v.cx + (int)(cosf(ang) * dist);
        b.ty = v.cy + (int)(sinf(ang) * dist * 0.85f);
        b.kind = (int)(hashf(i, 31, rs) * 5.0f) % 5;
        b.sprite = b.kind;
        // Never overlap plaza or other buildings
        if (abs(b.tx - v.cx) <= 5 && abs(b.ty - v.cy) <= 4) continue;
        bool bad = false;
        for (const auto& ex : v.buildings) {
            if (buildingsOverlap(b, ex, 2)) { bad = true; break; }
        }
        if (!bad) v.buildings.push_back(b);
    }

    v.plaza.clear();
    for (int y = -4; y <= 4; ++y) for (int x = -5; x <= 5; ++x) {
        bool paved = plan == 0 ? abs(y) <= 1 && abs(x) <= 5 :        // market street
                     plan == 1 ? abs(y) <= 3 && abs(x) <= 3 :        // temple court
                                 x*x/16.f + y*y/9.f <= 1.f;          // village green
        if (paved) v.plaza.emplace_back(v.cx+x,v.cy+y);
    }

    v.paths.clear();
    auto road = [&](int x0, int y0, int x1, int y1) {
        int dx = abs(x1 - x0), dy = abs(y1 - y0);
        int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
        int err = dx - dy, x = x0, y = y0;
        for (int guard = 0; guard < 96; guard++) {
            v.paths.push_back({ x, y });
            v.paths.push_back({ x+1, y });
            if (x == x1 && y == y1) break;
            int e2 = 2 * err;
            if (e2 > -dy) { err -= dy; x += sx; }
            if (e2 < dx) { err += dx; y += sy; }
        }
    };
    for (const Building& b : v.buildings) road(v.cx, v.cy, b.tx, b.ty + 1);

    v.decos.clear();
    auto tryDeco = [&](int kind, float x, float y, int variant = 0) {
        if (decoHitsBuildingOrDoor(v, x, y)) return;
        v.decos.push_back({ kind, x, y, variant });
    };
    tryDeco(VD_WELL, (float)v.cx, (float)v.cy);
    tryDeco(VD_SIGN, (float)v.cx + 2.0f, (float)v.cy + 3.0f);
    tryDeco(VD_BENCH, (float)v.cx - 2.0f, (float)v.cy + 2.0f);
    tryDeco(VD_FLOWERBED, (float)v.cx + 3.0f, (float)v.cy - 2.0f);
    for (int i = 0; i < 6; i++) {
        float a = hashf(i, 5, rs) * 6.2831853f;
        float r = 4.5f + hashf(i, 9, rs) * 5.0f;
        int kind = (i % 3 == 0) ? VD_BARREL : (i % 3 == 1 ? VD_PLANT : VD_TORCH);
        tryDeco(kind, floorf(v.cx + cosf(a) * r), floorf(v.cy + sinf(a) * r * 0.8f));
    }
    for (int i=0; i<12; ++i) {
        int dx = (int)(hash2i(i,3,rs)%25)-12, dy = (int)(hash2i(i,7,rs)%23)-11;
        if (dx*dx+dy*dy < 35) continue;
        int x=v.cx+dx, y=v.cy+dy;
        if (std::find(v.paths.begin(),v.paths.end(),std::make_pair(x,y)) != v.paths.end()) continue;
        bool busy=false;
        for (const auto& d : v.decos) if (fabsf(d.x-x)<2 && fabsf(d.y-y)<2) busy=true;
        if (!busy) tryDeco(VD_SHADE_TREE, (float)x, (float)y, i%3);
    }
}

void villageLoadAt(float x, float y) {
    if (nearStartVillage(x, y)) {
        if (g.villageLoaded && g.village.startVillage) return;
        buildStartVillage(g.village, g.worldSeed);
        g.villageLoaded = true;
        g.villageKey = 0x5157454cu;      // 'SWEL'
        return;
    }

    const int SECTOR = 96;
    int sx = (int)floorf(x / SECTOR), sy = (int)floorf(y / SECTOR);
    // A site survey can sample thousands of tiles. Cache the nine candidates
    // for this neighbourhood, including rejected sites, not just the winner.
    struct Site { int sx,sy,x,y; uint32_t seed; };
    static std::vector<Site> sites;
    static int lastX, lastY;
    static uint32_t lastSeed;
    static bool surveyed = false;
    if (!surveyed || sx!=lastX || sy!=lastY || lastSeed!=g.worldSeed) {
        surveyed=true; lastX=sx; lastY=sy; lastSeed=g.worldSeed; sites.clear();
        for (int dy=-1; dy<=1; ++dy) for (int dx=-1; dx<=1; ++dx) {
            int cx=sx+dx, cy=sy+dy;
            uint32_t h=hash2i(cx,cy,g.worldSeed^0x71a9eu);
            if (h%1000 >= 280) continue;
            int bx=cx*SECTOR+SECTOR/2, by=cy*SECTOR+SECTOR/2;
            bool ok=false;
            for (int r=0; r<40 && !ok; r+=3) for (int a=0; a<(r?16:1) && !ok; ++a) {
                float ang=a*(6.2831853f/16);
                int tx=bx+(int)(cosf(ang)*r), ty=by+(int)(sinf(ang)*r);
                if (tx*(float)tx+ty*(float)ty < 50*50 ||
                    (abs(tx-BOSS_CASTLE_TX)<29 && abs(ty-BOSS_CASTLE_TY)<29) ||
                    (abs(tx-LOCKET_SITE_TX)<23 && abs(ty-LOCKET_SITE_TY)<23) ||
                    (abs(tx-TOWER_SITE_TX)<23 && abs(ty-TOWER_SITE_TY)<23)) continue;
                if (siteFit(tx,ty,15)) { sites.push_back({cx,cy,tx,ty,h}); ok=true; }
            }
        }
    }
    Village best;
    bool found=false;
    int bestDist=48; // no need to load/render a town a screen beyond the horizon
    for (const auto& s : sites) {
        int d=std::max(abs(s.x-(int)floorf(x)), abs(s.y-(int)floorf(y)));
        if (d>=bestDist) continue;
        bestDist=d; found=true; best.valid=true; best.cx=s.x; best.cy=s.y;
        best.sectorX=s.sx; best.sectorY=s.sy; best.seed=s.seed;
    }
    if (!found) { g.villageLoaded = false; g.villageKey = ~0ull; return; }

    uint64_t key = ((uint64_t)(uint32_t)best.sectorX << 32) | (uint32_t)best.sectorY;
    if (g.villageLoaded && g.villageKey == key) return;
    best.name=villageNameFor(best.sectorX,best.sectorY,g.worldSeed);
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
static const char* QUEST_LINES[] = {
    "Those slimes are eating my turnips. Thin them out and I'll make it worth your while.",
};

const char* npcLine(const Npc& npc, int index) {
    if (npc.sprite==NS_CAT) return "The cat blinks slowly, then looks away.";
    if (npc.sprite==NS_CHICKEN) return "Cluck! She goes back to the turnips.";
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
    bool useVillage = g.villageLoaded && fabsf(x-g.village.cx)<30 && fabsf(y-g.village.cy)<30;
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
            if (b.kind == 5)
                add(NS_MERCHANT, FACE_SHOPKEEPER, NPC_SHOPKEEP, (float)b.tx - 2.0f, (float)b.ty + 1.5f,
                    v.startVillage ? "Mira the Trader" : "Village Trader", 0);
            else if (b.kind == 6)
                add(NS_MAYOR, FACE_INNKEEPER, NPC_INNKEEP, (float)b.tx + 2.0f, (float)b.ty + 1.5f,
                    v.startVillage ? "Bren the Innkeep" : "Innkeeper", 0);
            else if (b.kind == 7)
                add(NS_PRIEST, FACE_ELDER, NPC_PRIEST, (float)b.tx + 2.5f, (float)b.ty + 1.5f,
                    v.startVillage ? "Father Osric" : "Shrine Keeper", 0);
        }

        if (v.startVillage) {
            add(NS_CHICKEN, -1, NPC_ANIMAL, -10.5f,11.5f,"Hen",0);
            add(NS_CHICKEN, -1, NPC_ANIMAL, -12.5f,12.5f,"Hen",1);
            add(NS_CAT, -1, NPC_ANIMAL, 8.5f,2.5f,"Inn Cat",0);
            // Elmswick's named residents & quest givers placed cleanly around the town
            add(NS_ELDER,     FACE_ELDER,     NPC_ELDER,    -2.5f, -2.5f, "Old Maren",      0, QUEST_CROWN);
            add(NS_WOMAN,     FACE_WOMAN,     NPC_QUEST,     3.5f, -1.5f, "Tamsin",         1, QUEST_LOCKET);
            add(NS_CHILD,     FACE_CHILD,     NPC_CHILD,    -2.5f,  3.5f, "Pip the Younger",2);
            add(NS_GUARD,     FACE_GUARD,     NPC_GUARD,     2.5f,  7.5f, "Watchman Odo",   3);
            add(NS_VILLAGER1, FACE_VILLAGER1, NPC_VILLAGER, -6.5f,  2.5f, "Rook",           4);
            add(NS_VILLAGER2, FACE_VILLAGER2, NPC_VILLAGER,  6.5f,  1.5f, "Sable",          5);
            add(NS_VILLAGER3, FACE_VILLAGER3, NPC_VILLAGER,  2.5f, -5.5f, "Wren",           6);
            add(NS_FARMER,    FACE_SMITH,     NPC_QUEST,    -2.5f, 12.5f, "Farmer Hedrick", 0, QUEST_SLIMES);
            add(NS_DOG,       FACE_DOG,      NPC_ANIMAL,    2.5f,  1.5f, "Town Dog",       0);
        } else {
            const int sprites[] = { NS_VILLAGER0, NS_VILLAGER1, NS_VILLAGER2, NS_VILLAGER3 };
            const int faces[] = { FACE_VILLAGER0, FACE_VILLAGER1, FACE_VILLAGER2, FACE_VILLAGER3 };
            const char* names[] = { "Elder Rowan", "Kip", "Guard Bram", "Elin", "Corin", "Mina" };
            int n = 4 + (int)(v.seed % 3);
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
        }
    } else {
        uint32_t h = hash2i(sx, sy, g.worldSeed ^ 0x4e3c17u);
        if ((h % 100) < 20 && (x * x + y * y) > 45 * 45) {
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
        if ((h % 100) >= 36) continue;
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

// ================================================================ landmarks
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
        if (tx >= x0 - 6 && tx <= x1 + 6 && ty >= y0 - 6 && ty <= y1 + 8)
            out.push_back({ kind, tx, ty, seed });
    };
    // Deterministic story landmarks
    maybeAdd(LK_CASTLE, BOSS_CASTLE_TX, BOSS_CASTLE_TY, 0xca571eu);
    maybeAdd(LK_STONES, LOCKET_SITE_TX, LOCKET_SITE_TY, 0x10c4e7u);
    maybeAdd(LK_TOWER,  TOWER_SITE_TX,  TOWER_SITE_TY,  0x703e2u);

    // Procedural sector landmarks
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
// Habitat survives paving: on a frozen road the encounter still comes from
// the tundra, not the generic dirt table. Keep early pools approachable and
// reserve Serpents, Golems and Riders for the distant, higher-level frontier.
EnemyKind rollBiomeEnemy(int tx, int ty, int zoneLevel) {
    static const EnemyKind common[BI_COUNT][4] = {
        {EN_SLIME, EN_SLIME, EN_IMP, EN_BAT}, {EN_BAT, EN_IMP, EN_WOLF, EN_BAT},
        {EN_HORNET, EN_SLIME, EN_HORNET, EN_WOLF}, {EN_SLIME, EN_WISP, EN_BAT, EN_WISP},
        {EN_WOLF, EN_IMP, EN_WOLF, EN_BAT}, {EN_WOLF, EN_WISP, EN_WOLF, EN_WISP},
        {EN_HORNET, EN_IMP, EN_BAT, EN_IMP}
    };
    static const EnemyKind frontier[BI_COUNT][2] = {
        {EN_WOLF, EN_ORC}, {EN_GHOST, EN_WOLF}, {EN_WOLF, EN_HORNET}, {EN_GHOST, EN_SKELETON},
        {EN_ORC, EN_SKELETON}, {EN_WISP, EN_GHOST}, {EN_SKELETON, EN_ORC}
    };
    static const EnemyKind far[BI_COUNT] = {
        EN_NIGHT_RIDER, EN_NIGHT_RIDER, EN_SERPENT, EN_SERPENT, EN_GOLEM, EN_GOLEM, EN_SERPENT
    };
    auto biome = sampleGround(tx, ty).biome;
    auto h = hash2i(tx, ty, g.worldSeed ^ 0xbad1ceu);
    if (zoneLevel >= 12 && h%3 != 0) return far[biome];
    if (zoneLevel >= 3 && (h%3 == 0 || zoneLevel >= 7)) return frontier[biome][(h>>8)%2];
    return common[biome][(h>>8)%4];
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
        if ((h % 100) >= 80) continue;
        int count = 1 + (int)((h >> 8) % 2);
        for (int i = 0; i < count; i++) {
            float a = hashf(i, cx * 7 + 1, h) * 6.2831853f;
            float r = 3.0f + hashf(i, cy * 13 + 5, h) * (SECTOR * 0.38f);
            float mx = cx * SECTOR + SECTOR * 0.5f + cosf(a) * r;
            float my = cy * SECTOR + SECTOR * 0.5f + sinf(a) * r;
            if (mx * mx + my * my < 22.0f * 22.0f) continue;      // never inside Elmswick
            if (g.villageLoaded && inVillage(mx, my)) continue;   // never inside any village
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
            // South meadow near Elmswick favours slimes so Turnip Trouble is fun to complete
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
    if (fabsf(x)<11 && y>14 && y<23) return "Millbrook Crossing";
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
    if (x*x+y*y < 26*26) return "Meadow of First Steps";
    if (y < -40 && fabsf(x) < 30) return "The North Road";
    if (x < -120) return "The Howling West";
    if (x*x+y*y > 260*260) return "The Far Wilds";
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
            int x0 = b.tx - b.w / 2, x1 = x0 + b.w - 1;
            int y0 = b.ty - b.h + 1;
            if (toTx >= x0 && toTx <= x1 && toTy >= y0 && toTy <= b.ty) {
                out.doorBuilding = i;
                targetX = b.tx; targetY = b.ty + 1;
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
