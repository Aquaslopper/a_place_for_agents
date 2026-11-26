// world.cpp — noise, biomes, village layout, field monsters
#include "world.hpp"
#include <cmath>
#include <cstdlib>
#include <limits>

// ------------------------------------------------------------ hashing
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

// ------------------------------------------------------------ terrain
// elevation in [0,1]; water < 0.36, beach < 0.40, grass/forest, rock hills > 0.68, snow > 0.80 or cold low areas
static float elevation(float x, float y, uint32_t seed) {
    float e = fbm(x * 0.018f, y * 0.018f, seed ^ 0x1eafu, 5);
    // continent-ish bias: raise noise contrast
    e = (e - 0.5f) * 1.35f + 0.5f;
    return e;
}
float moisture(float x, float y, uint32_t seed) {
    return fbm(x * 0.024f + 91.7f, y * 0.024f + 17.3f, seed ^ 0x5eed, 4);
}
static float temperature(float x, float y, uint32_t seed) {
    return fbm(x * 0.012f + 313.1f, y * 0.012f + 47.9f, seed ^ 0x7e47, 3);
}

bool inSpawnSafeZone(float x, float y) {
    float d = sqrtf(x * x + y * y);
    return d < 9.0f;
}

TileInfo sampleTile(int tx, int ty) {
    TileInfo out;
    float x = (float)tx, y = (float)ty;
    uint32_t seed = g.worldSeed;

    float e = elevation(x, y, seed);
    float m = moisture(x, y, seed);
    float t = temperature(x, y, seed);

    // spawn safe zone: gentle grass bowl
    float d0 = sqrtf(x * x + y * y);
    if (d0 < 10.0f) e = 0.47f + (e - 0.5f) * (d0 / 10.0f);

    if (e < 0.36f) {
        out.kind = (t < 0.30f) ? TK_ICE : TK_WATER;
        out.variant = 0;
    } else if (e < 0.40f) {
        out.kind = (t < 0.25f) ? TK_SNOW : TK_SAND;
    } else if (e < 0.68f) {
        if (t < 0.28f) {
            out.kind = TK_SNOW;            // frozen reaches
        } else {
            out.kind = TK_GRASS;
            out.variant = hash2i(tx, ty, seed ^ 0x9a11u) % 3;
            float d = hashf(tx, ty, seed ^ 0xc0ffeeu);
            if (m > 0.55f && d < 0.16f) { out.deco = 1; out.decoVariant = hash2i(tx, ty, seed ^ 0xdee) % 3; }
            else if (d > 0.985f) { out.deco = 2; out.decoVariant = hash2i(tx, ty, seed ^ 0xfee) % 3; }
        }
    } else if (e < 0.80f) {
        out.kind = TK_ROCK;
        if (t < 0.3f) out.kind = TK_SNOW;
    } else {
        out.kind = TK_ROCK;
    }
    return out;
}

bool tileWalkable(int tx, int ty) {
    TileInfo t = sampleTile(tx, ty);
    if (t.kind == TK_WATER || t.kind == TK_ICE || t.kind == TK_ROCK) return false;
    // village ground is walkable; water never is
    return true;
}

// ------------------------------------------------------------ villages
static const char* NAME_A[] = {
    "Bran", "Kes", "Dun", "Wych", "Tor", "Ash", "Raven", "Elm", "Kar", "Sten",
    "Hol", "Grim", "Mar", "Bel", "Thorn", "Karn", "Ul", "Wyn", "El", "Oak"
};
static const char* NAME_B[] = {
    "wick", "hollow", "fell", "ford", "by", "stead", "mere", "dale", "hurst",
    "brook", "gate", "field", "wood", "shaw", "port", "haven", "crest", "moor"
};

static float tileScoreForVillage(int tx, int ty) {
    // prefer mid-elevation grass far from steep rock and water
    TileInfo t = sampleTile(tx, ty);
    if (t.kind != TK_GRASS) return -1.0f;
    // check surroundings are not water
    for (int dy = -3; dy <= 3; dy += 3)
        for (int dx = -3; dx <= 3; dx += 3)
            if (sampleTile(tx + dx, ty + dy).kind == TK_WATER) return -1.0f;
    return 1.0f - fabsf(elevation((float)tx, (float)ty, g.worldSeed) - 0.5f);
}

std::string villageNameFor(uint32_t sx, uint32_t sy, uint32_t seed) {
    uint32_t h = hash2i((int)sx, (int)sy, seed ^ 0x7ab1e5u);
    const char* a = NAME_A[h % 20];
    const char* b = NAME_B[(h / 20) % 18];
    return std::string(a) + b;
}

void villageLoadFor(float x, float y) {
    const int SECTOR = 104;
    int sx = (int)floorf(x / SECTOR);
    int sy = (int)floorf(y / SECTOR);

    // check 3x3 sectors for a village; pick the closest one
    float bestDist = 1e9f;
    Village best;
    bool found = false;

    for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
        int cx = sx + dx, cy = sy + dy;
        uint32_t h = hash2i(cx, cy, g.worldSeed ^ 0x71a9eu);
        if ((h % 1000) / 1000.0f >= 0.30f) continue;   // 30% of sectors host a village
        uint32_t vs = h;
        // scan a spiral of candidate positions around sector center
        int baseX = cx * SECTOR + SECTOR / 2, baseY = cy * SECTOR + SECTOR / 2;
        int bx = baseX, by = baseY; bool ok = false;
        for (int r = 0; r < 46 && !ok; r += 2) {
            for (int a = 0; a < 32 && !ok; a++) {
                float ang = (float)a / 32.0f * 6.2831853f;
                int tx = baseX + (int)(cosf(ang) * r);
                int ty = baseY + (int)(sinf(ang) * r);
                if (tileScoreForVillage(tx, ty) > 0.0f) { bx = tx; by = ty; ok = true; }
            }
        }
        if (!ok) continue;
        float d = sqrtf((bx - x) * (bx - x) + (by - y) * (by - y));
        if (d < bestDist) {
            bestDist = d;
            best = Village{};
            best.sectorX = (uint32_t)cx; best.sectorY = (uint32_t)cy; best.vseed = vs;
            best.cx = (float)bx; best.cy = (float)by;
            best.name = villageNameFor((uint32_t)cx, (uint32_t)cy, g.worldSeed);
            best.valid = true;
            found = true;
        }
    }

    if (!found) { g.villageLoaded = false; return; }
    uint64_t key = ((uint64_t)best.sectorX << 32) | best.sectorY;
    if (g.villageLoaded && g.villageSectorKey == key) return;   // already loaded

    Village& v = best;
    uint32_t rs = v.vseed;
    int count = 5 + (rs % 4);            // 5..8 buildings
    // plaza center building roles: shop to the south-east, inn to north-west
    std::vector<std::pair<int, int>> slots;
    for (int i = 0; i < count; i++) {
        float ang = (float)i / (float)count * 6.2831853f + (float)(rs % 100) / 100.0f;
        int dist = 5 + (int)(hashf(i, 77, rs) * 3.0f);
        int hx = (int)(v.cx + cosf(ang) * dist);
        int hy = (int)(v.cy + sinf(ang) * dist * 0.75f);
        slots.push_back({hx, hy});
    }
    v.houses.clear();
    int shopIdx = -1, innIdx = -1;
    float bestShop = 1e9f, bestInn = 1e9f;
    for (int i = 0; i < (int)slots.size(); i++) {
        float dxs = slots[i].first - (v.cx + 4), dys = slots[i].second - (v.cy + 2);
        float dShop = dxs * dxs + dys * dys;
        float dxi = slots[i].first - (v.cx - 4), dyi = slots[i].second - (v.cy - 2);
        float dInn = dxi * dxi + dyi * dyi;
        if (dShop < bestShop) { bestShop = dShop; shopIdx = i; }
        if (dInn < bestInn)  { bestInn  = dInn;  innIdx  = i; }
    }
    for (int i = 0; i < (int)slots.size(); i++) {
        HouseInst h;
        h.tx = slots[i].first; h.ty = slots[i].second;
        if (i == shopIdx) { h.variant = -1; h.role = 1; }
        else if (i == innIdx) { h.variant = -2; h.role = 2; }
        else { h.variant = (int)(hashf(i, 31, rs) * 3.0f) % 3; h.role = 0; }
        v.houses.push_back(h);
    }
    auto sprinkle = [&](std::vector<std::pair<int,int>>& out, int n, uint32_t ch, float r0, float r1) {
        for (int i = 0; i < n; i++) {
            float ang = hashf(i, (int)ch, rs) * 6.2831853f;
            float r = r0 + hashf(i, (int)ch + 7, rs) * (r1 - r0);
            out.push_back({ (int)(v.cx + cosf(ang) * r), (int)(v.cy + sinf(ang) * r) });
        }
    };
    v.wells.clear(); sprinkle(v.wells, 1, 11, 0.0f, 1.2f);
    v.signs.clear(); sprinkle(v.signs, 1, 23, 3.2f, 4.4f);
    v.fences.clear(); sprinkle(v.fences, 6, 37, 4.5f, 7.0f);
    v.trees.clear(); sprinkle(v.trees, 4, 41, 6.0f, 10.0f);
    v.bushes.clear(); sprinkle(v.bushes, 5, 43, 2.0f, 8.0f);
    v.mushrooms.clear(); sprinkle(v.mushrooms, 3, 47, 2.0f, 8.0f);

    g.village = v;
    g.villageSectorKey = key;
    g.villageLoaded = true;
}

bool villageContains(float x, float y) {
    if (!g.villageLoaded) return false;
    float dx = x - g.village.cx, dy = y - g.village.cy;
    return (dx * dx + dy * dy) < 9.5f * 9.5f;
}

bool villageInteractAt(int tx, int ty, int* outRole, Village* outVillage) {
    if (!g.villageLoaded) return false;
    for (const HouseInst& h : g.village.houses) {
        // door is at h.tx, h.ty (anchor bottom-center); interaction tile is just below
        if (tx == h.tx && (ty == h.ty || ty == h.ty + 1)) {
            if (outRole) *outRole = h.role;
            if (outVillage) *outVillage = g.village;
            return true;
        }
    }
    return false;
}

// ------------------------------------------------------------ field monsters
static const EnemyKind PLAIN_TABLE[] = { EN_SLIME, EN_SLIME, EN_WOLF, EN_BAT };
static const EnemyKind FOREST_TABLE[] = { EN_BAT, EN_WISP, EN_WOLF, EN_SLIME };
static const EnemyKind SNOW_TABLE[] = { EN_WISP, EN_BAT, EN_WISP, EN_WOLF };

EnemyKind rollBiomeEnemy(int tx, int ty) {
    TileInfo t = sampleTile(tx, ty);
    float r = hashf(tx, ty, g.worldSeed ^ 0xbad1ceu);
    const EnemyKind* tbl = PLAIN_TABLE;
    int n = 4;
    float m = moisture((float)tx, (float)ty, g.worldSeed);
    if (t.kind == TK_SNOW) tbl = SNOW_TABLE;
    else if (m > 0.55f) tbl = FOREST_TABLE;
    return tbl[(int)(r * n) % n];
}

void fieldMonstersInView(float x, float y, std::vector<FieldMonster>& out) {
    out.clear();
    const int SECTOR = 26;
    int sx = (int)floorf(x / SECTOR), sy = (int)floorf(y / SECTOR);
    for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
        int cx = sx + dx, cy = sy + dy;
        uint32_t h = hash2i(cx, cy, g.worldSeed ^ 0xf11du);
        if ((h % 100) >= 62) continue;                  // 62% of sectors have monsters
        int count = 1 + (int)((h >> 8) % 2);            // 1..2
        for (int i = 0; i < count; i++) {
            float a = hashf(i, cx * 7 + 1, h) * 6.2831853f;
            float r = 4.0f + hashf(i, cy * 13 + 5, h) * (SECTOR * 0.42f);
            float mx = cx * SECTOR + SECTOR * 0.5f + cosf(a) * r;
            float my = cy * SECTOR + SECTOR * 0.5f + sinf(a) * r;
            if (inSpawnSafeZone(mx, my)) continue;
            TileInfo t = sampleTile((int)floorf(mx), (int)floorf(my));
            if (t.kind != TK_GRASS && t.kind != TK_SAND && t.kind != TK_SNOW) continue;
            uint64_t id = ((uint64_t)(uint32_t)cx << 40) ^ ((uint64_t)(uint32_t)cy << 8) ^ (uint64_t)i;
            if (g.defeatedMonsters.count(id)) continue;
            FieldMonster fm;
            fm.id = id;
            fm.anchorX = mx; fm.anchorY = my;
            fm.x = mx; fm.y = my;
            fm.phase = hashf(i, cx + cy + 99, h) * 6.2831853f;
            fm.kind = rollBiomeEnemy((int)floorf(mx), (int)floorf(my));
            fm.sectorX = cx; fm.sectorY = cy;
            // keep monsters inside 18 tiles of player
            float ddx = fm.x - x, ddy = fm.y - y;
            if (ddx * ddx + ddy * ddy < 18.0f * 18.0f) out.push_back(fm);
        }
    }
}

int zoneLevelAt(float x, float y) {
    float dist = sqrtf(x * x + y * y);
    int zl = 1 + (int)(dist / 150.0f);
    int partyAvg = 0; for (auto& hh : g.party) partyAvg += hh.level; partyAvg = partyAvg / 3;
    return std::max(1, std::min(zl + partyAvg / 2, 24));
}

const char* zoneNameAt(float x, float y) {
    float dist = sqrtf(x * x + y * y);
    float t = temperature(x, y, g.worldSeed);
    if (dist < 10.0f) return "The Meadow of First Steps";
    if (t < 0.28f) return "The Frozenreach";
    if (y < -120.0f) return "The Sunscorch Flats";
    if (x < -140.0f) return "The Howling West";
    if (dist > 320.0f) return "The Far Wilds";
    return "The Greenmere Wilds";
}
