// world.cpp — noise, biomes, villages, props, npcs, chests, landmarks, A*
#include "world.hpp"
#include <cmath>
#include <cstdlib>
#include <queue>
#include <limits>
#include <string>
#include <algorithm>


// ================================================================ hashing
uint32_t hash2i(int x, int y, uint32_t seed) {
    uint32_t h = seed
        ^ (uint32_t)x * 2654435761u
        ^ (uint32_t)y * 2246822519u
        ^ (uint32_t)(x * 374761393 + y * 668265263) * 3266489917u;
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
    t += (y < 0 ? 0.06f : -0.04f);
    return t;
}
float moistureAt(float x, float y) {
    return fbm(x * 0.022f + 91.7f, y * 0.022f + 17.3f, g.worldSeed ^ 0x5eedu, 4);
}

// The meadow around the origin is always kind ground, so the opening village
// can exist no matter what seed the world rolled.
static void flattenStart(float x, float y, float& e, float& t, float& m) {
    float d = sqrtf(x * x + y * y);
    // The meadow of first steps is guaranteed dry, temperate grassland out to
    // 34 tiles so no seed can fence the opening village in with water or peaks.
    if (d < 34.0f) {
        float k = (d <= 26.0f) ? 1.0f : 1.0f - (d - 26.0f) / 8.0f;
        k = k * k * (3.0f - 2.0f * k);
        e = e * (1.0f - k) + 0.52f * k;
        t = t * (1.0f - k) + 0.55f * k;
        m = m * (1.0f - k) + 0.58f * k;
    }
    // hard floor on elevation in the protected meadow: never water, never rock
    if (d < 30.0f && e < 0.47f) e = 0.47f;
    if (d < 30.0f && e > 0.62f) e = 0.62f;
}

GroundTile sampleGround(int tx, int ty) {
    GroundTile out;
    float x = (float)tx, y = (float)ty;
    uint32_t seed = g.worldSeed;
    float e = elevationAt(x, y, seed);
    float t = temperatureAt(x, y, seed);
    float m = moistureAt(x, y);
    flattenStart(x, y, e, t, m);

    uint32_t h = hash2i(tx, ty, seed ^ 0x9a11u);
    out.variant = (uint8_t)(h % 4);

    if (e < 0.355f) {
        out.kind = (t < 0.24f) ? G_ICE : ((e < 0.31f) ? G_DEEP : G_WATER);
        return out;
    }
    if (e < 0.40f) {
        out.kind = (t < 0.22f) ? G_SNOW : G_SAND;
        return out;
    }
    if (e > 0.72f) {
        out.kind = (e > 0.80f && t < 0.55f) ? G_SNOW : G_ROCK;
        return out;
    }
    if (t < 0.26f) { out.kind = G_SNOW; }
    else if (m > 0.62f && t > 0.30f) { out.kind = (e < 0.44f) ? G_SWAMP : G_GRASS_DARK; }
    else out.kind = G_GRASS;

    // sparse ground decoration
    float d = hashf(tx, ty, seed ^ 0xc0ffeeu);
    if (out.kind == G_GRASS || out.kind == G_GRASS_DARK) {
        if (d < 0.024f) { out.deco = 1; out.decoVariant = (uint8_t)(h % 4); }
        else if (d < 0.038f) { out.deco = 2; out.decoVariant = (uint8_t)((h >> 5) % 2); }
    } else if (out.kind == G_SNOW) {
        if (d < 0.03f) { out.deco = 1; out.decoVariant = 3; }
    } else if (out.kind == G_ROCK) {
        if (d < 0.10f) { out.deco = 3; }
    }
    return out;
}

static bool groundWalkable(GroundKind k) {
    return k != G_WATER && k != G_DEEP && k != G_ROCK;
}

bool cellWalkable(int tx, int ty) {
    GroundTile gt = sampleGround(tx, ty);
    if (!groundWalkable(gt.kind)) return false;
    PropKind p = propAt(tx, ty);
    if (p == P_TREE0 || p == P_TREE1 || p == P_TREE2 || p == P_PINE0 || p == P_PINE1 ||
        p == P_PINE2 || p == P_DEADTREE || p == P_TREE_SNOW || p == P_ROCK0 || p == P_ROCK1 ||
        p == P_BOULDER)
        return false;
    if (g.villageLoaded) {
        for (const Building& b : g.village.buildings) {
            int x0 = b.tx - b.w / 2, y0 = b.ty - b.h + 1;
            if (tx >= x0 && tx <= x0 + b.w - 1 && ty >= y0 && ty <= b.ty) return false;
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
    return dx * dx + dy * dy < 17.0f * 17.0f;
}
bool nearStartVillage(float x, float y) {
    return (x * x + y * y) < 30.0f * 30.0f;
}

// ================================================================ props
PropKind propAt(int tx, int ty) {
    if (!g.villageLoaded || !inVillage((float)tx + 0.5f, (float)ty + 0.5f)) {
        GroundTile gt = sampleGround(tx, ty);
        if (gt.kind == G_WATER || gt.kind == G_DEEP || gt.kind == G_ICE) return P_NONE;
        uint32_t h = hash2i(tx, ty, g.worldSeed ^ 0x77ee5u);
        float hf = (float)h / 4294967295.0f;
        float m = moistureAt((float)tx, (float)ty);
        // never block the meadow where the game starts
        bool meadow = (tx * tx + ty * ty) < 24 * 24;
        switch (gt.kind) {
            case G_GRASS:
            case G_GRASS_DARK:
                if (!meadow && m > 0.52f && hf < 0.20f)
                    return (hf < 0.03f) ? P_PINE0 : (hf < 0.10f ? P_TREE0 : P_TREE1);
                if (hf > 0.985f) return P_ROCK0;
                if (hf > 0.960f) return P_BUSH;
                if (hf < 0.018f) return P_BUSH_PLAIN;
                if (hf > 0.945f && hf <= 0.960f) return P_MUSHROOM;
                if (hf > 0.940f && hf <= 0.945f) return P_PLANT;
                return P_NONE;
            case G_SNOW:
                if (hf < 0.16f) return (hf < 0.06f) ? P_PINE2 : P_TREE_SNOW;
                if (hf > 0.975f) return P_ROCK2;
                return P_NONE;
            case G_ROCK:
                if (hf < 0.14f) return P_BOULDER;
                return P_NONE;
            case G_SAND:
                if (hf < 0.03f) return P_ROCK1;
                if (hf > 0.99f) return P_CRYSTAL;
                return P_NONE;
            case G_SWAMP:
                if (hf < 0.12f) return P_MUSHROOM;
                if (hf < 0.16f) return P_DEADTREE;
                return P_NONE;
            default:
                return P_NONE;
        }
    }
    // inside a village: the village's own deco list decides
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
        case VD_CRATE: t = &g.assets.keg; break;
        case VD_TABLE: t = &g.assets.table; break;
        case VD_BUCKET: t = &g.assets.bucket; break;
        default: break;
    }
    tex = t;
    w = t ? t->width : 0;
    h = t ? t->height : 0;
}

// ================================================================ names
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

// ================================================================ villages
static bool siteFit(int cx, int cy, int R) {
    for (int dy = -R; dy <= R; dy++) {
        for (int dx = -R; dx <= R; dx++) {
            if (dx * dx + dy * dy > R * R) continue;
            GroundTile t = sampleGround(cx + dx, cy + dy);
            if (t.kind == G_WATER || t.kind == G_DEEP || t.kind == G_ROCK || t.kind == G_ICE ||
                t.kind == G_SWAMP)
                return false;
        }
    }
    return true;
}

// --------------------------------------------------------------------------
// The starting village.  Hand-authored on one seed-independent plan so the
// opening scene is always the same warm, walkable little town: cobbled plaza
// with a well, a shop and an inn on either side of the road, a temple, houses
// with fences and gardens, and the meadow gate to the wide world in the south.
// --------------------------------------------------------------------------
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
    addHouse(1, 12, -9);
    addHouse(2, -12, 5);
    addHouse(3, 12, 4);
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
    // roads from the plaza to every door
    for (const Building& b : v.buildings) road(0, 0, b.tx, b.ty + 1);
    // the gate road running south out of town
    road(0, 4, 0, 16);
    road(0, -4, 30, -4);

    auto deco = [&](int kind, float x, float y, int variant = 0) {
        v.decos.push_back({ kind, x, y, variant });
    };
    deco(VD_WELL, 0.0f, 1.0f);
    deco(VD_SIGN, 4.0f, 5.0f);
    deco(VD_SIGN, -3.0f, 6.0f);
    deco(VD_BENCH, -3.0f, 2.0f);
    deco(VD_BENCH, 3.0f, 2.0f);
    deco(VD_FLOWERBED, -5.0f, -2.0f);
    deco(VD_FLOWERBED, 5.0f, -2.0f);
    deco(VD_TORCH, -6.0f, 3.0f);
    deco(VD_TORCH, 6.0f, 3.0f);
    deco(VD_STATUE, -2.0f, -6.0f);
    deco(VD_PLANT, -10.0f, 3.0f);
    deco(VD_PLANT, 10.0f, 2.0f);
    deco(VD_BARREL, -10.0f, -2.0f);
    deco(VD_BARREL, 10.0f, -2.0f);
    deco(VD_CRATE, 7.0f, 5.0f);
    deco(VD_BUCKET, 1.0f, 3.0f);
    deco(VD_CAMPFIRE, -7.0f, 8.0f);
    deco(VD_CAULDRON, 8.0f, 8.0f);
    deco(VD_LADDER, -13.0f, -5.0f);
    deco(VD_TABLE, -6.0f, 4.0f);
    deco(VD_FENCE_H, -14.0f, 8.0f);
    deco(VD_FENCE_H, -13.0f, 8.0f);
    deco(VD_FENCE_H, 13.0f, 7.0f);
    deco(VD_FENCE_V, -14.0f, 9.0f);
    deco(VD_FENCE_V, 14.0f, 8.0f);
}

// Generated villages --------------------------------------------------------
static void layoutVillage(Village& v) {
    uint32_t rs = v.seed;
    int count = 5 + (rs % 4);
    v.buildings.clear();
    for (int i = 0; i < count; i++) {
        float ang = (float)i / (float)count * 6.2831853f + (float)(rs % 100) / 100.0f;
        int dist = 6 + (int)(hashf(i, 77, rs) * 3.0f);
        int hx = v.cx + (int)(cosf(ang) * dist);
        int hy = v.cy + (int)(sinf(ang) * dist * 0.8f);
        Building b;
        b.w = 4; b.h = 3;
        b.tx = hx; b.ty = hy;
        b.kind = (int)(hashf(i, 31, rs) * 5.0f) % 5;
        v.buildings.push_back(b);
    }
    if (!v.buildings.empty()) {
        v.buildings[0].kind = 5;                       // shop
        v.buildings[0].tx = v.cx + 7; v.buildings[0].ty = v.cy + 1;
        if (v.buildings.size() > 1) {
            v.buildings[1].kind = 6;                   // inn
            v.buildings[1].tx = v.cx - 7; v.buildings[1].ty = v.cy + 1;
        }
        if (v.buildings.size() > 2) {
            v.buildings[2].kind = 7;                   // temple
            v.buildings[2].w = 5; v.buildings[2].h = 4;
            v.buildings[2].tx = v.cx; v.buildings[2].ty = v.cy - 8;
        }
    }
    for (int i = 0; i < (int)v.buildings.size(); i++) {
        Building& b = v.buildings[i];
        b.sprite = b.kind;
        if (b.kind == 7) { b.w = 5; b.h = 4; }
    }
    v.plaza.clear();
    for (int ty = -3; ty <= 3; ty++)
        for (int tx = -4; tx <= 4; tx++) {
            float dx = (float)tx / 4.0f, dy = (float)ty / 3.0f;
            if (dx * dx + dy * dy <= 1.0f) v.plaza.push_back({ v.cx + tx, v.cy + ty });
        }
    v.paths.clear();
    auto road = [&](int x0, int y0, int x1, int y1) {
        int dx = abs(x1 - x0), dy = abs(y1 - y0);
        int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
        int err = dx - dy, x = x0, y = y0;
        for (int guard = 0; guard < 96; guard++) {
            v.paths.push_back({ x, y });
            if (x == x1 && y == y1) break;
            int e2 = 2 * err;
            if (e2 > -dy) { err -= dy; x += sx; }
            if (e2 < dx) { err += dx; y += sy; }
        }
    };
    for (const Building& b : v.buildings) road(v.cx, v.cy, b.tx, b.ty + 1);
    v.decos.clear();
    auto deco = [&](int kind, float x, float y, int variant = 0) {
        v.decos.push_back({ kind, x, y, variant });
    };
    deco(VD_WELL, (float)v.cx, (float)v.cy + 1.0f);
    deco(VD_SIGN, (float)v.cx + 2.0f, (float)v.cy + 3.0f);
    deco(VD_BENCH, (float)v.cx - 2.0f, (float)v.cy + 2.0f);
    deco(VD_FLOWERBED, (float)v.cx + 3.0f, (float)v.cy - 1.0f);
    for (int i = 0; i < 6; i++) {
        float a = hashf(i, 5, rs) * 6.2831853f;
        float r = 4.0f + hashf(i, 9, rs) * 6.0f;
        int kind = (i % 3 == 0) ? VD_BARREL : (i % 3 == 1 ? VD_PLANT : VD_TORCH);
        deco(kind, v.cx + cosf(a) * r, v.cy + sinf(a) * r * 0.8f);
    }
}

void villageLoadAt(float x, float y) {
    // The starting village always wins near the origin.
    if (nearStartVillage(x, y)) {
        if (g.villageLoaded && g.village.startVillage) return;
        buildStartVillage(g.village, g.worldSeed);
        g.villageLoaded = true;
        g.villageKey = 0x5157454cu;      // 'SWEL'
        return;
    }

    const int SECTOR = 96;
    int sx = (int)floorf(x / SECTOR), sy = (int)floorf(y / SECTOR);
    int bestDist = 1 << 30;
    Village best;
    bool found = false;

    for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
        int cx = sx + dx, cy = sy + dy;
        uint32_t h = hash2i(cx, cy, g.worldSeed ^ 0x71a9eu);
        if ((h % 1000) / 1000.0f >= 0.26f) continue;
        int baseX = cx * SECTOR + SECTOR / 2, baseY = cy * SECTOR + SECTOR / 2;
        if (baseX * baseX + baseY * baseY < 60 * 60) continue;    // keep clear of Elmswick
        int bx = baseX, by = baseY;
        bool ok = false;
        for (int r = 0; r < 40 && !ok; r += 2) {
            for (int a = 0; a < 24 && !ok; a++) {
                float ang = (float)a / 24.0f * 6.2831853f;
                int tx = baseX + (int)(cosf(ang) * r);
                int ty = baseY + (int)(sinf(ang) * r);
                if (siteFit(tx, ty, 11)) { bx = tx; by = ty; ok = true; }
            }
        }
        if (!ok) continue;
        int d = abs(bx - (int)x) + abs(by - (int)y);
        if (d < bestDist) {
            bestDist = d;
            best = Village{};
            best.valid = true;
            best.sectorX = (uint32_t)cx; best.sectorY = (uint32_t)cy;
            best.seed = h;
            best.cx = bx; best.cy = by;
            best.name = villageNameFor(cx, cy, g.worldSeed);
            found = true;
        }
    }
    if (!found) { g.villageLoaded = false; g.villageKey = ~0ull; return; }

    uint64_t key = ((uint64_t)best.sectorX << 32) | best.sectorY;
    if (g.villageLoaded && g.villageKey == key) return;
    layoutVillage(best);
    g.village = best;
    g.villageKey = key;
    g.villageLoaded = true;
}

// ================================================================ npcs
static const char* VILLAGER_LINES[] = {
    "The wilds hum tonight. The trees keep secrets that aren't yours.",
    "They say a castle waits beyond the peaks. Nobody walks that far and back.",
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
    "East road's clear. West road has wolves. Your call.",
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
    "Woof.", "Moo.", "Cluck.", "...meow.",
};
static const char* QUEST_LINES[] = {
    "Those slimes are eating my turnips. Thin them out and I'll make it worth your while.",
};

const char* npcLine(const Npc& npc, int index) {
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

// NPC sprite indices (see import_openrtp.py order)
enum NpcSprite {
    NS_VILLAGER0 = 0, NS_VILLAGER1, NS_VILLAGER2, NS_VILLAGER3, NS_ELDER, NS_MAYOR,
    NS_WOMAN, NS_CHILD, NS_GUARD, NS_SOLDIER, NS_KNIGHT, NS_MERCHANT, NS_SMITH,
    NS_PRIEST, NS_FARMER, NS_KIND_COUNT
};

void npcsLoadFor(float x, float y, bool force) {
    bool useVillage = g.villageLoaded && inVillage(x, y);
    uint64_t key = useVillage ? (0x10000ull | g.villageKey) : 0ull;
    if (!force && key != 0 && g.npcKey == key && !g.npcs.empty()) return;

    g.npcs.clear();
    g.npcKey = key;
    uint32_t seed = useVillage ? g.village.seed : hash2i((int)floorf(x / 32), (int)floorf(y / 32), g.worldSeed);

    auto add = [&](int sprite, int face, int role, float px, float py, const char* name, int lineSet) {
        Npc n;
        n.id = (uint64_t)(g.npcs.size() + 1) * 7919ull ^ (uint64_t)seed;
        n.sprite = sprite; n.face = face; n.role = role;
        n.x = n.homeX = px; n.y = n.homeY = py;
        n.name = name; n.lineSet = lineSet;
        n.thinkT = hashf((int)px, (int)py, seed) * 2.0f;
        g.npcs.push_back(n);
    };

    if (useVillage) {
        const Village& v = g.village;
        // shopkeeper and innkeeper stand beside their doors
        for (const Building& b : v.buildings) {
            if (b.kind == 5) add(NS_MERCHANT, FACE_SHOPKEEPER, NPC_SHOPKEEP, (float)b.tx - 1.4f, (float)b.ty + 1.2f, "Mira the Trader", 0);
            else if (b.kind == 6) add(NS_WOMAN, FACE_INNKEEPER, NPC_INNKEEP, (float)b.tx + 1.4f, (float)b.ty + 1.2f, "Bren the Innkeep", 0);
            else if (b.kind == 7) add(NS_PRIEST, FACE_ELDER, NPC_PRIEST, (float)b.tx + 1.6f, (float)b.ty + 1.4f, "Father Osric", 0);
        }
        // townfolk wandering the plaza
        const int sprites[] = { NS_VILLAGER0, NS_VILLAGER1, NS_VILLAGER2, NS_VILLAGER3 };
        const int faces[] = { FACE_VILLAGER0, FACE_VILLAGER1, FACE_VILLAGER2, FACE_VILLAGER3 };
        int n = v.startVillage ? 7 : 4 + (int)(v.seed % 3);
        for (int i = 0; i < n; i++) {
            float ang = hashf(i, 11, v.seed) * 6.2831853f;
            float r = 2.0f + hashf(i, 13, v.seed) * 7.0f;
            float px = (float)v.cx + cosf(ang) * r;
            float py = (float)v.cy + sinf(ang) * r * 0.8f;
            int role = (i == 0) ? NPC_ELDER : (i == 1 ? NPC_CHILD : (i == 2 ? NPC_GUARD : NPC_VILLAGER));
            int sprite = (role == NPC_ELDER) ? NS_ELDER : (role == NPC_CHILD ? NS_CHILD : (role == NPC_GUARD ? NS_GUARD : sprites[i % 4]));
            int face = (role == NPC_ELDER) ? FACE_ELDER : (role == NPC_CHILD ? FACE_CHILD : (role == NPC_GUARD ? FACE_GUARD : faces[i % 4]));
            const char* names[] = { "Old Maren", "Pip the Younger", "Watchman Odo", "Tamsin", "Rook", "Sable", "Wren" };
            add(sprite, face, role, px, py, names[i % 7], i);
        }
        // a farmer out past the fence with a small request
        add(NS_FARMER, FACE_VILLAGER0, NPC_QUEST, (float)v.cx - 5.5f, (float)v.cy + 7.5f, "Farmer Hedrick", 0);
        // animals
        add(NS_VILLAGER0 - 12 + 14, FACE_VILLAGER0, NPC_ANIMAL, (float)v.cx + 3.0f, (float)v.cy - 4.0f, "Town Dog", 1);
    } else {
        // lone wanderers out in the wilds (rare, and never in the deep snow)
        int sx = (int)floorf(x / 32), sy = (int)floorf(y / 32);
        uint32_t h = hash2i(sx, sy, g.worldSeed ^ 0x4e3c17u);
        if ((h % 100) < 18 && (x * x + y * y) > 60 * 60) {
            float px = sx * 32 + 8.0f + hashf(sx, sy, h) * 14.0f;
            float py = sy * 32 + 8.0f + hashf(sy, sx, h) * 14.0f;
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
        if ((h % 100) >= 34) continue;
        int tx = cx * SECTOR + 4 + (int)(hashf(cx, cy, h) * (SECTOR - 8));
        int ty = cy * SECTOR + 4 + (int)(hashf(cy, cx, h + 7) * (SECTOR - 8));
        if (!cellWalkable(tx, ty)) continue;
        if (inVillage((float)tx, (float)ty)) continue;
        Chest c;
        c.id = ((uint64_t)(uint32_t)cx << 32) ^ (uint32_t)cy ^ 0x9e37ull;
        c.tx = tx; c.ty = ty;
        int roll = (int)((h >> 8) % 100);
        if (roll < 42) { c.item = IT_POTION; c.amount = 1 + (int)((h >> 16) % 2); }
        else if (roll < 62) { c.gil = 20 + (int)((h >> 16) % 90); }
        else if (roll < 76) { c.item = IT_ETHER; c.amount = 1; }
        else if (roll < 88) { c.item = IT_HIPOTION; c.amount = 1; }
        else { c.item = IT_PHOENIX; c.amount = 1; }
        gChestsInView.push_back(c);
    }
}

// ================================================================ landmarks
bool landmarkNear(int tx, int ty, Landmark& out) {
    const int SECTOR = 128;
    int sx = (int)floorf((float)tx / SECTOR), sy = (int)floorf((float)ty / SECTOR);
    uint32_t h = hash2i(sx, sy, g.worldSeed ^ 0x1a4d4u);
    if ((h % 100) >= 55) return false;
    int lx = sx * SECTOR + SECTOR / 2 + (int)(hashf(sx, sy, h) * 40) - 20;
    int ly = sy * SECTOR + SECTOR / 2 + (int)(hashf(sy, sx, h) * 40) - 20;
    if (abs(lx - tx) > 8 || abs(ly - ty) > 8) return false;
    out.kind = (LandmarkKind)(1 + (h >> 6) % 5);
    out.tx = lx; out.ty = ly;
    out.seed = h;
    return true;
}

bool inBossArena(float x, float y) {
    // the castle sits far to the north; its gate is the final arena
    int csx = 0, csy = -3;
    int cx = csx * 128 + 64, cy = csy * 128 + 64;
    float dx = x - cx, dy = y - cy;
    return (dx * dx + dy * dy) < 26.0f * 26.0f;
}

// ================================================================ field monsters
static const EnemyKind TIER1[] = { EN_SLIME, EN_SLIME, EN_IMP, EN_BAT };
static const EnemyKind TIER2[] = { EN_SLIME, EN_IMP, EN_BAT, EN_HORNET, EN_WISP };
static const EnemyKind TIER3[] = { EN_WISP, EN_WOLF, EN_SKELETON, EN_GHOST, EN_ORC };
static const EnemyKind TIER4[] = { EN_ORC, EN_SERPENT, EN_GOLEM, EN_NIGHT_RIDER };

EnemyKind rollBiomeEnemy(int tx, int ty, int zoneLevel) {
    GroundTile gt = sampleGround(tx, ty);
    uint32_t h = hash2i(tx, ty, g.worldSeed ^ 0xbad1ceu);
    const EnemyKind* table = TIER1;
    int n = 4;
    if (zoneLevel >= 12) { table = TIER4; }
    else if (zoneLevel >= 7) { table = TIER3; }
    else if (zoneLevel >= 3) { table = TIER2; }
    // snow favours wisps and ghosts, water edges favour slimes
    if ((gt.kind == G_SNOW || gt.kind == G_ICE) && zoneLevel >= 3) { table = TIER3; n = 5; }
    if (gt.kind == G_SWAMP && zoneLevel >= 3) { table = TIER2; n = 5; }
    return table[h % (uint32_t)n];
}

void fieldMonstersInView(float x, float y, std::vector<FieldMonster>& out) {
    out.clear();
    const int SECTOR = 24;
    int sx = (int)floorf(x / SECTOR), sy = (int)floorf(y / SECTOR);
    int zl = zoneLevelAt(x, y);
    for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
        int cx = sx + dx, cy = sy + dy;
        uint32_t h = hash2i(cx, cy, g.worldSeed ^ 0xf11du);
        if ((h % 100) >= 78) continue;
        int count = 1 + (int)((h >> 8) % 2);
        for (int i = 0; i < count; i++) {
            float a = hashf(i, cx * 7 + 1, h) * 6.2831853f;
            float r = 3.0f + hashf(i, cy * 13 + 5, h) * (SECTOR * 0.40f);
            float mx = cx * SECTOR + SECTOR * 0.5f + cosf(a) * r;
            float my = cy * SECTOR + SECTOR * 0.5f + sinf(a) * r;
            if (mx * mx + my * my < 26.0f * 26.0f) continue;      // never in Elmswick
            GroundTile t = sampleGround((int)floorf(mx), (int)floorf(my));
            if (t.kind == G_WATER || t.kind == G_DEEP || t.kind == G_ROCK || t.kind == G_ICE) continue;
            if (propAt((int)floorf(mx), (int)floorf(my)) != P_NONE) continue;
            uint64_t id = ((uint64_t)(uint32_t)cx << 40) ^ ((uint64_t)(uint32_t)cy << 8) ^ (uint64_t)i;
            if (g.defeatedMonsters.count(id)) continue;
            float ddx = mx - x, ddy = my - y;

            FieldMonster fm;
            fm.id = id;
            fm.anchorX = mx; fm.anchorY = my;
            fm.x = mx; fm.y = my;
            fm.phase = hashf(i, cx + cy + 99, h) * 6.2831853f;
            fm.kind = rollBiomeEnemy((int)floorf(mx), (int)floorf(my), zl);
            out.push_back(fm);
        }
    }
}

int zoneLevelAt(float x, float y) {
    float dist = sqrtf(x * x + y * y);
    int zl = 1 + (int)(dist / 110.0f);
    int avg = partyLevel();
    return std::max(1, std::min(zl + avg / 3, 26));
}

const char* zoneNameAt(float x, float y) {
    if (inVillage(x, y) && g.villageLoaded) return g.village.name.c_str();
    float dist = sqrtf(x * x + y * y);
    GroundTile t = sampleGround((int)floorf(x), (int)floorf(y));
    float temp = temperatureAt(x, y, g.worldSeed);
    if (dist < 26.0f) return "The Meadow of First Steps";
    if (t.kind == G_SNOW || t.kind == G_ICE || temp < 0.26f) return "The Frozenreach";
    if (t.kind == G_SAND) return "The Sunscorch Flats";
    if (t.kind == G_SWAMP) return "The Mirefen";
    if (t.kind == G_ROCK) return "The Stonelands";
    if (y < -160.0f) return "The North Road";
    if (x < -180.0f) return "The Howling West";
    if (dist > 300.0f) return "The Far Wilds";
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

    // clicking a building aims for the doorstep and remembers the door
    if (g.villageLoaded) {
        for (int i = 0; i < (int)g.village.buildings.size(); i++) {
            const Building& b = g.village.buildings[i];
            if (abs(toTx - b.tx) <= b.w / 2 + 1 && toTy >= b.ty - b.h && toTy <= b.ty + 1) {
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
    auto relax = [&](int x, int y, float g, uint64_t from) {
        uint64_t k = navKey(x, y);
        auto it = gScore.find(k);
        if (it != gScore.end() && it->second <= g + 1e-4f) return;
        gScore[k] = g;
        parent[k] = from;
        open.push({ x, y, g + hcost(x, y) });
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
