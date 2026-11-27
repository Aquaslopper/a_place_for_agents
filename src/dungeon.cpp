// dungeon.cpp — procedurally generated cavern dungeons
//
// A stable BSP backbone holds stairs, encounters and treasure. Natural caverns
// grow an irregular, connected rock envelope around it after placement; crypts
// retain their masonry. Both layers are deterministic for (seed, site, floor).
#include "game.hpp"
#include "world.hpp"
#include "battle.hpp"
#include <cstdio>

namespace {

// ================================================================ types
enum DunTile : uint8_t { DT_WALL = 0, DT_FLOOR = 1 };

// dungeon prop texture ids (mapped onto the shared asset sheet) — diversified per theme
enum DunPropTex {
    DP_BARREL = 0, DP_CRATE, DP_PILLAR, DP_STATUE, DP_URN, DP_TORCH,
    DP_CANDLE, DP_LANTERN, DP_TABLE, DP_LONGTABLE, DP_STOOL, DP_KEG,
    DP_SHELF, DP_PLANT, DP_CRYSTAL, DP_MUSHROOM,
    DP_BOULDER, DP_REEDS, DP_BUSH, DP_STUMP,
    DP_CAVE_FORM0, DP_CAVE_FORM1, DP_CAVE_FORM2, DP_CAVE_FORM3,
    DP_CAVE_FORM4, DP_CAVE_FORM5, DP_COUNT
};

struct DunChest { int tx, ty; int item; int amount; int gil; uint64_t id; };
struct DunMob   { uint64_t id; EnemyKind kind; float x, y, ax, ay, phase; };
struct DunProp  { int tex; float x, y; bool solid; float w, h; };
struct Room     { int x, y, w, h; };

struct DunFloor {
    int w = 0, h = 0;
    std::vector<uint8_t> tiles;
    int entryTx = 0, entryTy = 0;
    int downTx = -1, downTy = -1;
    std::vector<DunChest> chests;
    std::vector<DunMob>   mobs;
    // Includes defeated spawn sites so weathering never follows the kill log.
    std::vector<std::pair<int, int>> mobAnchors;
    std::vector<DunProp>  props;
    std::vector<std::pair<int, int>> sconces;   // wall cells holding a torch
};

struct Dungeon {
    bool active = false;
    int siteTx = 0, siteTy = 0;
    uint32_t seed = 0;
    bool startCavern = false;
    int floor = 0, floors = 3;
    int zoneBase = 1;
    int theme = TH_MOSSY;
    std::string name;
    std::string themeName;
    DunFloor F;
    std::unordered_map<int, std::vector<uint8_t>> explored;
    float px = 0, py = 0;               // tile units
    int facing = 0; bool sideFlip = false;
    float animT = 0; bool moving = false;
    float stepDist = 0; int stepCounter = 0;
    float encounterGrace = 0;
    float camX = 0, camY = 0;
    std::vector<std::pair<int, int>> path;
    bool pendingInteract = false;
    uint64_t pendingChestId = 0;
    float clickFlashT = 0, clickFlashX = 0, clickFlashY = 0;
};
Dungeon D;

// ================================================================ rng
struct Rng {
    uint32_t s;
    explicit Rng(uint32_t seed) : s(seed ? seed : 0x9e3779b9u) {}
    uint32_t next() {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return s;
    }
    int range(int lo, int hi) {                 // inclusive
        if (hi <= lo) return lo;
        return lo + (int)(next() % (uint32_t)(hi - lo + 1));
    }
    float f() { return (next() & 0xffffff) / 16777216.0f; }
    bool chance(int percent) { return (int)(next() % 100u) < percent; }
};

// ================================================================ small helpers
inline int dIdx(int x, int y, int w) { return y * w + x; }
bool inBounds(int tx, int ty) { return tx >= 0 && ty >= 0 && tx < D.F.w && ty < D.F.h; }
bool cellOpen(int tx, int ty) { return inBounds(tx, ty) && D.F.tiles[dIdx(tx, ty, D.F.w)] == DT_FLOOR; }
bool cellOpenIn(const DunFloor& f, int tx, int ty) {
    return tx >= 0 && ty >= 0 && tx < f.w && ty < f.h &&
           f.tiles[dIdx(tx, ty, f.w)] == DT_FLOOR;
}

void texFootLocal(Texture2D t, float cx, float footY, Color tint = WHITE, float scale = 1.0f) {
    float w = t.width * scale, h = t.height * scale;
    DrawTexturePro(t, { 0, 0, (float)t.width, (float)t.height },
                   { floorf(cx - w * 0.5f), floorf(footY - h), w, h },
                   { 0, 0 }, 0.0f, tint);
}

Texture2D* dunPropTexture(int tex) {
    if (tex >= DP_CAVE_FORM0 && tex <= DP_CAVE_FORM5)
        return &g.assets.caveFormation[tex - DP_CAVE_FORM0];
    switch (tex) {
        case DP_BARREL:    return &g.assets.barrel;
        case DP_CRATE:     return &g.assets.crate;
        case DP_PILLAR:    return &g.assets.pillar;
        case DP_STATUE:    return &g.assets.statue;
        case DP_URN:       return &g.assets.urn;
        case DP_TORCH:     return &g.assets.torch;
        case DP_CANDLE:    return &g.assets.candle;
        case DP_LANTERN:   return &g.assets.lantern;
        case DP_TABLE:     return &g.assets.table;
        case DP_LONGTABLE: return g.assets.longtable.id ? &g.assets.longtable : &g.assets.table;
        case DP_STOOL:     return &g.assets.stoolTex;
        case DP_KEG:       return g.assets.keg.id ? &g.assets.keg : &g.assets.barrel;
        case DP_SHELF:     return g.assets.shelf.id ? &g.assets.shelf : &g.assets.counter;
        case DP_PLANT:     return &g.assets.plant;
        case DP_CRYSTAL:   return &g.assets.crystal;
        case DP_MUSHROOM:  return &g.assets.mushroom;
        case DP_BOULDER:   return &g.assets.boulder;
        case DP_REEDS:     return &g.assets.reeds;
        case DP_BUSH:      return &g.assets.bush[0];
        case DP_STUMP:     return g.assets.stump.id ? &g.assets.stump : &g.assets.bush[1];
        default:           return &g.assets.barrel;
    }
}
bool dunPropSolid(int tex) {
    switch (tex) {
        case DP_TORCH: case DP_CANDLE: case DP_LANTERN: case DP_MUSHROOM:
        case DP_REEDS: case DP_BUSH:
        case DP_CAVE_FORM0: case DP_CAVE_FORM1: case DP_CAVE_FORM2:
        case DP_CAVE_FORM3: case DP_CAVE_FORM4: case DP_CAVE_FORM5:
            return false;
        default:
            return true;
    }
}

// per-theme visuals: background, fog, torch glow and wall tints
struct ThemeVis {
    Color clear;
    Color fog;
    Color torchGlow;
    Color wallLight;
    Color wallDark;
    Color sconceTint;
};
ThemeVis themeVisual(int th) {
    switch (th) {
        case TH_MOSSY:     return { {  8, 12, 14, 255}, {  4, 10, 10, 255}, {255,170, 60,255}, {168,176,156,255}, { 58, 62, 48,255}, WHITE };
        case TH_ROOTED:    return { { 10,  9, 14, 255}, {  8,  8, 14, 255}, {255,180, 70,255}, {158,146,132,255}, { 52, 42, 38,255}, Color{255,210,140,255} };
        case TH_AMBER:     return { { 14, 12,  8, 255}, { 16, 12,  6, 255}, {255,200, 90,255}, {192,178,142,255}, { 70, 58, 42,255}, Color{255,220,160,255} };
        case TH_MIRE:      return { {  6, 10, 12, 255}, {  6, 16, 14, 255}, {150,255,130,255}, {120,148,132,255}, { 36, 52, 44,255}, Color{170,255,150,255} };
        case TH_HEATH:     return { { 12, 12, 16, 255}, { 14, 14, 20, 255}, {255,190, 80,255}, {172,168,160,255}, { 58, 54, 62,255}, WHITE };
        case TH_FROST:     return { {  8, 12, 22, 255}, { 10, 14, 26, 255}, {120,180,255,255}, {160,180,210,255}, { 38, 48, 72,255}, Color{180,220,255,255} };
        case TH_SUNSCORCH: return { { 18, 14, 10, 255}, { 22, 16, 10, 255}, {255,160, 60,255}, {188,172,144,255}, { 64, 52, 38,255}, Color{255,180, 80,255} };
        case TH_CRYPT:     return { { 10,  8, 14, 255}, { 12,  8, 16, 255}, {200,140,255,255}, {148,138,168,255}, { 42, 38, 52,255}, Color{210,180,255,255} };
        default:           return { {  8,  8, 14, 255}, {  4,  4, 10, 255}, {255,170, 60,255}, {168,156,176,255}, { 58, 52, 72,255}, WHITE };
    }
}
int themeForSite(int siteTx, int siteTy, uint32_t seed) {
    Biome b = sampleGround(siteTx, siteTy).biome;
    int th = TH_CRYPT;
    switch (b) {
        case BI_MEADOW: th = TH_MOSSY; break;
        case BI_FOREST: th = TH_ROOTED; break;
        case BI_AMBER:  th = TH_AMBER; break;
        case BI_FEN:    th = TH_MIRE; break;
        case BI_HEATH:  th = TH_HEATH; break;
        case BI_FROST:  th = TH_FROST; break;
        case BI_DUNES:  th = TH_SUNSCORCH; break;
        default: break;
    }
    // far wilds sometimes collapse into a true crypt regardless of surface
    int zl = zoneLevelAt((float)siteTx + 0.5f, (float)siteTy + 1.5f);
    if (zl >= 10 && (seed % 5u) == 0u) th = TH_CRYPT;
    if (zl >= 14 && (seed % 3u) == 0u) th = TH_CRYPT;
    return th;
}

bool dunBlocked(float x, float y, float r) {
    const float off[4][2] = { { -r, -r }, { r, -r }, { -r, r }, { r, r } };
    for (auto& o : off) {
        int tx = (int)floorf(x + o[0]), ty = (int)floorf(y + o[1]);
        if (!cellOpen(tx, ty)) return true;
    }
    for (const auto& p : D.F.props) {
        if (!p.solid) continue;
        if (fabsf(p.x - x) < p.w * 0.45f + r && fabsf(p.y - y) < p.h * 0.42f + r)
            return true;
    }
    return false;
}

// ================================================================ content tables
const char* cavernNameFor(uint32_t seed) {
    // theme-tinted naming: each theme has its own 8-name table, seeded deterministically
    static const char* kMossy[] = {"Whispering Warren","Mould Hollow","Rootbreak Delve","Green Gloam","Bramble Maw","Mossvault","Thorn Warren","Fern Hollow"};
    static const char* kRooted[] = {"Hollow Hill","Rooted Hollow","Briar Delve","Hollowed Copse","Burrowed Hollow","Deepwood Cut","Wormroot","The Undergrove"};
    static const char* kAmber[] = {"Amber Vault","Emberdeep Hollow","Sunleaf Delve","Golden Warren","Amber Grotto","The Gilded Cut","Leafvault","Hollow of Amber"};
    static const char* kMire[] = {"Murkfall Shaft","The Sunken Vault","Bilge Hollow","Fen Grotto","Mire Warren","Drowned Vault","Silt Hollow","The Reed Cut"};
    static const char* kHeath[] = {"Stonevault","The Stone Cut","Heath Barrow","Windcut Hollow","Grit Warren","Stone Maw","Heathvault","Rook's Hollow"};
    static const char* kFrost[] = {"Frostvault","Rime Hollow","Ice Maw","Snowcut Warren","Hoarfrost Delve","The White Vault","Chill Warren","Glacier Hollow"};
    static const char* kSun[] = {"Cinder Delve","Sunscorch Cut","Dustvault","Ember Maw","Sand Warren","Sun Hollow","Scorchvault","The Dry Cut"};
    static const char* kCrypt[] = {"The Bone Gallery","Gloam Warrens","Emberdeep","Whispering Cavern","Murkfall Shaft","The Sunken Vault","Rook's Hollow","Cinder Delve"};
    int th = D.theme;
    const char** tab = kCrypt;
    if (th==TH_MOSSY) tab=kMossy; else if (th==TH_ROOTED) tab=kRooted; else if (th==TH_AMBER) tab=kAmber;
    else if (th==TH_MIRE) tab=kMire; else if (th==TH_HEATH) tab=kHeath; else if (th==TH_FROST) tab=kFrost;
    else if (th==TH_SUNSCORCH) tab=kSun;
    return tab[(seed >> 8) % 8];
}

EnemyKind pickDungeonEnemy(Rng& rng, int zl) {
    // theme-weighted pools — each cavern feels like its surface biome, tier-filtered by depth
    static const EnemyKind kMossy[] = { EN_SLIME, EN_GOBLIN, EN_BANDIT, EN_WOLF, EN_PORCUPINE, EN_SKELETON, EN_SKELETON, EN_BAT };
    static const EnemyKind kRooted[] = { EN_BAT, EN_WOLF, EN_FAERIE, EN_GOBLIN, EN_PORCUPINE, EN_SKELETON, EN_TIGER };
    static const EnemyKind kAmber[] = { EN_HORNET, EN_FAERIE, EN_WOLF, EN_PORCUPINE, EN_GOBLIN, EN_TIGER, EN_DRAKE };
    static const EnemyKind kMire[] = { EN_WISP, EN_SLIME, EN_SKELETON, EN_GHOST, EN_SHADE, EN_SERPENT, EN_NAGA };
    static const EnemyKind kHeath[] = { EN_SKELETON, EN_ORC, EN_REAPER, EN_SHADE, EN_BANDIT, EN_GOLEM, EN_PORCUPINE };
    static const EnemyKind kFrost[] = { EN_WISP, EN_WOLF, EN_SKELETON, EN_GOLEM, EN_TIGER, EN_SHADE, EN_REAPER };
    static const EnemyKind kSun[] = { EN_HORNET, EN_ORC, EN_SERPENT, EN_NAGA, EN_DRAKE, EN_BANDIT, EN_IMP };
    static const EnemyKind kCrypt[] = { EN_SKELETON, EN_GHOST, EN_SHADE, EN_REAPER, EN_NIGHT_RIDER, EN_DREAD_KNIGHT, EN_ARCHFIEND, EN_GOLEM, EN_SERPENT };
    static const EnemyKind kGeneric[] = { EN_SLIME, EN_BAT, EN_IMP, EN_GOBLIN, EN_BANDIT, EN_WOLF, EN_PORCUPINE, EN_SKELETON, EN_GHOST, EN_ORC, EN_SHADE, EN_REAPER, EN_TIGER, EN_NIGHT_RIDER, EN_DREAD_KNIGHT, EN_GOLEM, EN_ARCHFIEND, EN_SERPENT };
    const EnemyKind* pool = kGeneric; int n = 18;
    switch (D.theme) {
        case TH_MOSSY: pool=kMossy; n=8; break;
        case TH_ROOTED: pool=kRooted; n=7; break;
        case TH_AMBER: pool=kAmber; n=7; break;
        case TH_MIRE: pool=kMire; n=7; break;
        case TH_HEATH: pool=kHeath; n=7; break;
        case TH_FROST: pool=kFrost; n=7; break;
        case TH_SUNSCORCH: pool=kSun; n=7; break;
        case TH_CRYPT: pool=kCrypt; n=9; break;
        default: break;
    }
    int maxTier = std::max(1, std::min(5, 1 + (zl + D.floor) / 3));
    int minTier = std::max(1, maxTier - 2);
    int candidates[18], c = 0;
    for (int i = 0; i < n; i++) {
        int t = enemyDef(pool[i]).tier;
        if (t >= minTier && t <= maxTier) candidates[c++] = i;
    }
    // fallback to generic if theme filter was too narrow for this depth
    if (c == 0) {
        pool = kGeneric; n = 18;
        for (int i = 0; i < n; i++) {
            int t = enemyDef(pool[i]).tier;
            if (t >= minTier && t <= maxTier) candidates[c++] = i;
        }
    }
    if (c == 0) return EN_BAT;
    return pool[candidates[(int)(rng.next() % (uint32_t)c)]];
}

// Loot pick is driven by a PRE-DRAWN roll so every placement consumes the
// same number of rng draws no matter what the party is carrying — layout must
// never shift with inventory.
int rollDungeonLoot(uint32_t pick, bool vault) {
    if (vault) {
        static const int kVault[] = {
            IT_SWORD_MYTHRIL, IT_ARMOR_MYTHRIL, IT_ROBE_ARCHMAGE, IT_HALBERD,
            IT_STAFF_SAGE, IT_MACE_HOLY, IT_ELIXIR, IT_RING_WAR, IT_RING_SAGE, IT_MEGAPOTION
        };
        return kVault[pick % 10u];
    }
    if (D.floor <= 0) {
        static const int kShallow[] = {
            IT_POTION, IT_POTION, IT_ANTIDOTE, IT_ETHER, IT_PHOENIX, IT_DAGGER, IT_SWORD_BRONZE
        };
        return kShallow[pick % 7u];
    }
    if (D.floor == 1) {
        static const int kMid[] = {
            IT_HIPOTION, IT_ETHER, IT_HIETHER, IT_REMEDY, IT_SMOKE_BOMB, IT_SWORD_STEEL,
            IT_ARMOR_CHAIN, IT_SHIELD_KITE, IT_ROBE_SILK, IT_MACE_BRASS
        };
        return kMid[pick % 10u];
    }
    static const int kDeep[] = {
        IT_MEGAPOTION, IT_HIETHER, IT_ELIXIR, IT_REMEDY, IT_PHOENIX, IT_WAR_AXE,
        IT_ARMOR_PLATE, IT_ROBE_ARCHMAGE, IT_AMULET_GUARD, IT_BOOTS_SWIFT, IT_HUNTER_BOW
    };
    return kDeep[pick % 11u];
}

int pickThemeQuietProp(Rng& rng) {
    switch (D.theme) {
        case TH_MOSSY:     return rng.chance(50) ? DP_MUSHROOM : (rng.chance(50) ? DP_BUSH : DP_STUMP);
        case TH_ROOTED:    return rng.chance(40) ? DP_MUSHROOM : (rng.chance(60) ? DP_STUMP : DP_BUSH);
        case TH_AMBER:     return rng.chance(60) ? DP_PLANT : DP_MUSHROOM;
        case TH_MIRE:      return rng.chance(55) ? DP_REEDS : (rng.chance(50) ? DP_MUSHROOM : DP_BUSH);
        case TH_HEATH:     return rng.chance(50) ? DP_BOULDER : DP_CRYSTAL;
        case TH_FROST:     return rng.chance(70) ? DP_CRYSTAL : DP_BOULDER;
        case TH_SUNSCORCH: return rng.chance(60) ? DP_URN : DP_BOULDER;
        case TH_CRYPT:     return rng.chance(60) ? DP_URN : DP_CRYSTAL;
        default:           return rng.chance(50) ? DP_URN : DP_MUSHROOM;
    }
}
int pickThemeFurnProp(Rng& rng) {
    switch (D.theme) {
        case TH_MOSSY:     { static const int k[]={DP_BARREL,DP_CRATE,DP_KEG,DP_TABLE,DP_STOOL,DP_BUSH,DP_STUMP}; return k[rng.next()%7]; }
        case TH_ROOTED:    { static const int k[]={DP_BARREL,DP_CRATE,DP_TABLE,DP_STUMP,DP_MUSHROOM,DP_PLANT,DP_BUSH}; return k[rng.next()%7]; }
        case TH_AMBER:     { static const int k[]={DP_BARREL,DP_CRATE,DP_TABLE,DP_SHELF,DP_PLANT,DP_URN,DP_CRYSTAL}; return k[rng.next()%7]; }
        case TH_MIRE:      { static const int k[]={DP_BARREL,DP_CRATE,DP_KEG,DP_URN,DP_REEDS,DP_MUSHROOM,DP_PLANT}; return k[rng.next()%7]; }
        case TH_HEATH:     { static const int k[]={DP_BARREL,DP_CRATE,DP_TABLE,DP_STOOL,DP_SHELF,DP_BOULDER,DP_STATUE}; return k[rng.next()%7]; }
        case TH_FROST:     { static const int k[]={DP_CRATE,DP_BARREL,DP_CRYSTAL,DP_BOULDER,DP_TABLE,DP_LANTERN,DP_URN}; return k[rng.next()%7]; }
        case TH_SUNSCORCH: { static const int k[]={DP_BARREL,DP_CRATE,DP_URN,DP_KEG,DP_TABLE,DP_BOULDER,DP_PILLAR}; return k[rng.next()%7]; }
        case TH_CRYPT:     { static const int k[]={DP_PILLAR,DP_STATUE,DP_URN,DP_CRATE,DP_TABLE,DP_SHELF,DP_CANDLE}; return k[rng.next()%7]; }
        default:           { static const int k[]={DP_BARREL,DP_CRATE,DP_KEG,DP_TABLE,DP_STOOL,DP_SHELF,DP_PLANT}; return k[rng.next()%7]; }
    }
}
int pickThemeVaultProp(Rng& rng) {
    switch (D.theme) {
        case TH_FROST: return DP_CRYSTAL;
        case TH_MIRE:  return rng.chance(50) ? DP_STATUE : DP_URN;
        case TH_HEATH: return DP_STATUE;
        case TH_SUNSCORCH: return DP_URN;
        case TH_CRYPT: return rng.chance(40) ? DP_STATUE : DP_PILLAR;
        default: return rng.chance(30) ? DP_STATUE : DP_LONGTABLE;
    }
}

// ================================================================ generation
void addProp(DunFloor& f, int tex, int tx, int ty, float w = 0.85f, float h = 0.75f) {
    if (!cellOpenIn(f, tx, ty)) return;
    if ((tx == f.entryTx && ty == f.entryTy) || (tx == f.downTx && ty == f.downTy)) return;
    for (const auto& other : f.props)
        if (fabsf(other.x - (tx + 0.5f)) < 1.0f && fabsf(other.y - (ty + 0.5f)) < 1.0f) return;
    // Keep furniture away from doorways and narrow corridor mouths.
    if (dunPropSolid(tex)) {
        for (int y = ty - 1; y <= ty + 1; ++y)
            for (int x = tx - 1; x <= tx + 1; ++x)
                if (!cellOpenIn(f, x, y)) return;
    }
    DunProp p;
    p.tex = tex;
    p.x = tx + 0.5f; p.y = ty + 0.5f;
    p.solid = dunPropSolid(tex);
    p.w = w; p.h = h;
    f.props.push_back(p);
}

void bspCarve(Rng& rng, std::vector<uint8_t>& grid, int w, int h,
              int x, int y, int rw, int rh, int depth, std::vector<Room>& rooms) {
    bool canSplitH = rh >= 16;                 // room + min partition
    bool canSplitV = rw >= 22;
    if (depth >= 5 || (!canSplitH && !canSplitV)) {
        int pad = 1;
        int roomW = rng.range(5, std::max(5, rw - pad * 2));
        int roomH = rng.range(4, std::max(4, rh - pad * 2));
        int rx = x + rng.range(pad, std::max(pad, rw - roomW - pad));
        int ry = y + rng.range(pad, std::max(pad, rh - roomH - pad));
        Room r{ rx, ry, roomW, roomH };
        for (int yy = ry; yy < ry + roomH; yy++)
            for (int xx = rx; xx < rx + roomW; xx++)
                if (xx > 0 && yy > 0 && xx < w - 1 && yy < h - 1)
                    grid[dIdx(xx, yy, w)] = DT_FLOOR;
        rooms.push_back(r);
        return;
    }
    bool horiz;
    if (canSplitH && canSplitV) horiz = rh >= rw ? true : rng.chance(55);
    else horiz = canSplitH;
    if (horiz) {
        int cut = rng.range(8, rh - 8);
        bspCarve(rng, grid, w, h, x, y, rw, cut, depth + 1, rooms);
        bspCarve(rng, grid, w, h, x, y + cut, rw, rh - cut, depth + 1, rooms);
    } else {
        int cut = rng.range(11, rw - 11);
        bspCarve(rng, grid, w, h, x, y, cut, rh, depth + 1, rooms);
        bspCarve(rng, grid, w, h, x + cut, y, rw - cut, rh, depth + 1, rooms);
    }
}

void carveCorridor(std::vector<uint8_t>& grid, int w, int h, int x0, int y0, int x1, int y1) {
    auto stamp = [&](int x, int y) {
        if (x > 0 && y > 0 && x < w - 1 && y < h - 1)
            grid[dIdx(x, y, w)] = DT_FLOOR;
    };
    int sx = (x0 <= x1) ? 1 : -1;
    for (int x = x0; x != x1 + sx; x += sx) { stamp(x, y0); stamp(x, y0 + 1); }
    int sy = (y0 <= y1) ? 1 : -1;
    for (int y = y0; y != y1 + sy; y += sy) { stamp(x1, y); stamp(x1 + 1, y); }
}

void roomCenter(const Room& r, int& cx, int& cy) {
    cx = r.x + r.w / 2;
    cy = r.y + r.h / 2;
}

uint64_t chestIdAt(int tx, int ty, int floor) {
    return (1ull << 62) ^
           ((uint64_t)(uint32_t)D.siteTx << 40) ^
           ((uint64_t)(uint32_t)D.siteTy << 24) ^
           ((uint64_t)(uint32_t)(floor + 1) << 12) ^
           (uint64_t)hash2i(tx, ty, D.seed ^ 0xC4EAu);
}
uint64_t mobIdAt(int tx, int ty, int floor, int salt) {
    return (1ull << 63) ^
           ((uint64_t)(uint32_t)D.siteTx << 40) ^
           ((uint64_t)(uint32_t)D.siteTy << 24) ^
           ((uint64_t)(uint32_t)(floor + 1) << 12) ^
           (uint64_t)hash2i(tx + salt * 131, ty - salt * 17, D.seed ^ 0xF00Du);
}

// Weather the old traversable network only after placing its contents. The
// old placement RNG and objective coordinates must remain untouched: saves
// store chest and monster IDs derived from those coordinates, not a version.
void weatherCavern(DunFloor& f) {
    if (D.theme == TH_CRYPT) return;
    const uint32_t salt = D.seed ^ (0x8128d91u * (uint32_t)(D.floor + 1));
    const std::vector<uint8_t> original = f.tiles;
    // Break up the square BSP outlines with shallow notches. Never cut near
    // an objective, creature or existing prop. Reject the entire erosion pass
    // if any formerly walkable cell becomes isolated behind a constriction.
    for (int y = 2; y < f.h - 2; ++y) for (int x = 2; x < f.w - 2; ++x) {
        if (original[dIdx(x,y,f.w)] != DT_FLOOR) continue;
        int open = (original[dIdx(x-1,y,f.w)] == DT_FLOOR) + (original[dIdx(x+1,y,f.w)] == DT_FLOOR) +
                   (original[dIdx(x,y-1,f.w)] == DT_FLOOR) + (original[dIdx(x,y+1,f.w)] == DT_FLOOR);
        if (open != 3 || hash2i(x,y,salt ^ 0x947u) % 100u >= 28u) continue;
        auto close = [x,y](int px,int py) { return abs(x-px) <= 1 && abs(y-py) <= 1; };
        bool reserved = close(f.entryTx,f.entryTy) || (f.downTx >= 0 && close(f.downTx,f.downTy));
        for (const auto& c : f.chests) if (close(c.tx,c.ty)) reserved = true;
        for (const auto& m : f.mobAnchors) if (close(m.first,m.second)) reserved = true;
        for (const auto& p : f.props) if (close((int)p.x,(int)p.y)) reserved = true;
        if (!reserved) f.tiles[dIdx(x,y,f.w)] = DT_WALL;
    }
    auto connected = [&]() {
        std::vector<uint8_t> seen(f.tiles.size(), 0);
        std::vector<int> q{dIdx(f.entryTx, f.entryTy, f.w)};
        seen[q[0]] = 1;
        const int dirs[4][2] = {{1,0}, {-1,0}, {0,1}, {0,-1}};
        for (size_t i = 0; i < q.size(); ++i) for (const auto& dir : dirs) {
            int x = q[i] % f.w + dir[0], y = q[i] / f.w + dir[1];
            if (!cellOpenIn(f,x,y) || dunBlocked(x+0.5f,y+0.5f,0.28f)) continue;
            int idx = dIdx(x,y,f.w);
            if (!seen[idx]) { seen[idx] = 1; q.push_back(idx); }
        }
        for (int y = 0; y < f.h; ++y) for (int x = 0; x < f.w; ++x)
            if (cellOpenIn(f,x,y) && !dunBlocked(x+0.5f,y+0.5f,0.28f) && !seen[dIdx(x,y,f.w)])
                return false;
        return true;
    };
    if (!connected()) f.tiles = original;
    const std::vector<uint8_t> notched = f.tiles;
    // Each added cell touches the previous pass, so the weathered envelope
    // grows into connected alcoves instead of generating isolated islands.
    for (int pass = 0; pass < 2; ++pass) {
        std::vector<uint8_t> next = f.tiles;
        for (int y = 2; y < f.h - 2; ++y) for (int x = 2; x < f.w - 2; ++x) {
            if (cellOpenIn(f, x, y) || (original[dIdx(x,y,f.w)] == DT_FLOOR &&
                                         notched[dIdx(x,y,f.w)] == DT_WALL)) continue;
            int cardinal = cellOpenIn(f,x-1,y) + cellOpenIn(f,x+1,y) +
                           cellOpenIn(f,x,y-1) + cellOpenIn(f,x,y+1);
            if (!cardinal) continue;
            int near = cardinal + cellOpenIn(f,x-1,y-1) + cellOpenIn(f,x+1,y-1) +
                       cellOpenIn(f,x-1,y+1) + cellOpenIn(f,x+1,y+1);
            // The coarse field makes bulges rather than single-pixel noise;
            // a finer field adds small, fractured outlines.
            unsigned coarse = hash2i(x / 3, y / 3, salt + pass * 97u) % 100u;
            unsigned fine = hash2i(x, y, salt + pass * 337u) % 100u;
            int threshold = (pass == 0 ? 52 : 24) + (near >= 4 ? 18 : 0);
            if ((int)((coarse * 2 + fine) / 3) < threshold)
                next[dIdx(x, y, f.w)] = DT_FLOOR;
        }
        f.tiles.swap(next);
    }

    // An old solid prop may stand at the mouth of a newly opened pocket.
    // Keep only new cells reachable with the same clearance as movement.
    std::vector<uint8_t> seen(f.tiles.size(), 0);
    std::vector<int> q{dIdx(f.entryTx, f.entryTy, f.w)};
    seen[q[0]] = 1;
    const int dirs[4][2] = {{1,0}, {-1,0}, {0,1}, {0,-1}};
    for (size_t i = 0; i < q.size(); ++i) {
        int x = q[i] % f.w, y = q[i] / f.w;
        for (const auto& dir : dirs) {
            int nx = x + dir[0], ny = y + dir[1];
            if (!cellOpenIn(f, nx, ny) || dunBlocked(nx + 0.5f, ny + 0.5f, 0.28f)) continue;
            int idx = dIdx(nx, ny, f.w);
            if (!seen[idx]) { seen[idx] = 1; q.push_back(idx); }
        }
    }
    for (size_t i = 0; i < f.tiles.size(); ++i)
        if (original[i] == DT_WALL && !seen[i]) f.tiles[i] = DT_WALL;

    // Natural caves may have traces of visitors, but not a room full of
    // tables and shelves. Removing these after placement does not alter IDs.
    f.props.erase(std::remove_if(f.props.begin(), f.props.end(), [](const DunProp& p) {
        return p.tex == DP_TABLE || p.tex == DP_LONGTABLE || p.tex == DP_STOOL ||
               p.tex == DP_SHELF || p.tex == DP_PLANT || p.tex == DP_PILLAR;
    }), f.props.end());

    // Rebuild wall lights against the expanded boundary, not the buried BSP
    // edges. Natural caverns are sparsely lit; the party carries a lantern.
    f.sconces.clear();
    for (int y = 1; y < f.h - 1; ++y) for (int x = 1; x < f.w - 1; ++x) {
        if (cellOpenIn(f, x, y) || !cellOpenIn(f, x, y + 1)) continue;
        if (hash2i(x, y, salt ^ 0x731u) % 37u == 0u)
            f.sconces.push_back({x, y});
    }

    // Lumps and dripstone sit at rock margins, where they frame passages
    // rather than block them. They are visual dressing, never collision.
    for (int y = 2; y < f.h - 2; ++y) for (int x = 2; x < f.w - 2; ++x) {
        if (!cellOpenIn(f, x, y)) continue;
        if (cellOpenIn(f,x-1,y) && cellOpenIn(f,x+1,y) &&
            cellOpenIn(f,x,y-1) && cellOpenIn(f,x,y+1)) continue;
        uint32_t h = hash2i(x, y, salt ^ 0xa121u);
        if (h % 13u != 0u) continue;
        auto close = [x,y](int px, int py) { return abs(x-px) <= 1 && abs(y-py) <= 1; };
        if (close(f.entryTx, f.entryTy) || (f.downTx >= 0 && close(f.downTx, f.downTy))) continue;
        bool reserved = false;
        for (const auto& c : f.chests) if (close(c.tx, c.ty)) { reserved = true; break; }
        for (const auto& m : f.mobAnchors) if (close(m.first, m.second)) { reserved = true; break; }
        if (reserved) continue;
        // Temperate caves favour dripstone and fallen rock; wet and icy
        // galleries also expose crystals. Large boulders are rarer.
        static const int dry[] = {0, 1, 4, 0, 1, 5};
        static const int wet[] = {0, 1, 2, 3, 2, 5};
        int choice = (int)((h >> 8) % 6u);
        int form = (D.theme == TH_FROST || D.theme == TH_MIRE) ? wet[choice] : dry[choice];
        addProp(f, DP_CAVE_FORM0 + form, x, y, 0.5f, 0.5f);
    }
}

void generateFloor() {
    Rng rng(D.seed ^ (0x51f15eu * (uint32_t)(D.floor + 1)) ^ (D.startCavern ? 0xa11u : 0u));
    DunFloor f;
    f.w = 56; f.h = 40;
    f.tiles.assign((size_t)f.w * f.h, DT_WALL);

    std::vector<Room> rooms;
    bspCarve(rng, f.tiles, f.w, f.h, 1, 1, f.w - 2, f.h - 2, 0, rooms);
    if (rooms.size() < 2) {                    // degenerate fallback: two halls + link
        rooms = { Room{ 4, 4, 12, 8 }, Room{ 34, 24, 14, 9 } };
        f.tiles.assign((size_t)f.w * f.h, DT_WALL);
        for (auto& r : rooms)
            for (int yy = r.y; yy < r.y + r.h; yy++)
                for (int xx = r.x; xx < r.x + r.w; xx++)
                    f.tiles[dIdx(xx, yy, f.w)] = DT_FLOOR;
        carveCorridor(f.tiles, f.w, f.h, 10, 8, 41, 28);
    }

    // Connect sibling rooms in BSP order (tree connectivity), then punch loops.
    for (size_t i = 1; i < rooms.size(); i++) {
        int ax, ay, bx, by;
        roomCenter(rooms[i - 1], ax, ay);
        roomCenter(rooms[i], bx, by);
        carveCorridor(f.tiles, f.w, f.h, ax, ay, bx, by);
    }
    int loops = 1 + (int)(rng.next() % 3u);
    // theme nudges connectivity: mire is loopier, crypt is more linear, frost is airy
    if (D.theme == TH_MIRE) loops = 2 + (int)(rng.next() % 2u);
    else if (D.theme == TH_CRYPT) loops = 1 + (int)(rng.next() % 2u);
    else if (D.theme == TH_FROST) loops = std::max(1, loops - (rng.chance(50)?1:0));
    for (int i = 0; i < loops && rooms.size() > 3; i++) {
        int a = (int)(rng.next() % (uint32_t)rooms.size());
        int b = (a + 1 + (int)(rng.next() % (uint32_t)(rooms.size() - 1))) % (int)rooms.size();
        int ax, ay, bx, by;
        roomCenter(rooms[a], ax, ay);
        roomCenter(rooms[b], bx, by);
        if (abs(ax - bx) + abs(ay - by) > 14)
            carveCorridor(f.tiles, f.w, f.h, ax, ay, bx, by);
    }

    // Entry = room nearest the south-west; stairs-down = farthest floor walk
    // from the entry (BFS), so the descent is always a proper journey.
    int entryRoom = 0;
    int bestScore = 1 << 30;
    for (int i = 0; i < (int)rooms.size(); i++) {
        int cx, cy; roomCenter(rooms[i], cx, cy);
        int score = (cx - 6) * (cx - 6) + (cy - (f.h - 6)) * (cy - (f.h - 6));
        if (score < bestScore) { bestScore = score; entryRoom = i; }
    }
    int ex, ey;
    roomCenter(rooms[entryRoom], ex, ey);
    f.entryTx = ex; f.entryTy = ey;

    std::vector<int> dist((size_t)f.w * f.h, -1);
    std::vector<int> queue;
    queue.push_back(dIdx(ex, ey, f.w));
    dist[dIdx(ex, ey, f.w)] = 0;
    int farIdx = dIdx(ex, ey, f.w);
    const int dirs[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
    for (size_t qi = 0; qi < queue.size(); qi++) {
        int idx = queue[qi];
        int x = idx % f.w, y = idx / f.w;
        if (dist[idx] > dist[farIdx]) farIdx = idx;
        for (auto& d : dirs) {
            int nx = x + d[0], ny = y + d[1];
            if (!cellOpenIn(f, nx, ny)) continue;
            int nidx = dIdx(nx, ny, f.w);
            if (dist[nidx] >= 0) continue;
            dist[nidx] = dist[idx] + 1;
            queue.push_back(nidx);
        }
    }
    // Unreachable floor (shouldn't happen with BSP) becomes wall.
    for (int i = 0; i < (int)dist.size(); i++)
        if (f.tiles[i] == DT_FLOOR && dist[i] < 0) f.tiles[i] = DT_WALL;

    if (D.floor < D.floors - 1) {
        f.downTx = farIdx % f.w;
        f.downTy = farIdx / f.w;
    }

    D.F = std::move(f);                        // helpers below read D.F
    DunFloor& F = D.F;

    // --- wall sconces: torches on wall cells that face the floor (never block)
    // sconce density is theme-tinted: mire glows more, frost glints, heath is sparser
    int sconceMod = 9;
    if (D.theme == TH_MIRE) sconceMod = 7;
    else if (D.theme == TH_FROST) sconceMod = 8;
    else if (D.theme == TH_HEATH) sconceMod = 10;
    else if (D.theme == TH_SUNSCORCH) sconceMod = 11;
    for (int y = 1; y < F.h - 1; y++) {
        for (int x = 1; x < F.w - 1; x++) {
            if (F.tiles[dIdx(x, y, F.w)] != DT_WALL) continue;
            bool faceFloor = cellOpen(x, y + 1) || cellOpen(x + 1, y) || cellOpen(x - 1, y);
            if (!faceFloor) continue;
            if (hash2i(x, y, D.seed ^ ((uint32_t)D.floor * 131u)) % (uint32_t)sconceMod == 0u)
                F.sconces.push_back({ x, y });
        }
    }
    // theme-specific accent: frost gets crystal shards, mire gets reed tufts along watery seams
    if (D.theme == TH_FROST || D.theme == TH_MIRE || D.theme == TH_SUNSCORCH) {
        for (int y = 1; y < F.h - 1; y++) for (int x = 1; x < F.w - 1; x++) {
            if (!cellOpen(x, y)) continue;
            if ((hash2i(x, y, D.seed ^ 0xA5u) % 64u) != 0u) continue;
            if (D.theme == TH_FROST) addProp(F, DP_CRYSTAL, x, y, 0.6f, 0.55f);
            else if (D.theme == TH_MIRE && rng.chance(60)) addProp(F, DP_REEDS, x, y, 0.7f, 0.45f);
            else if (D.theme == TH_SUNSCORCH && rng.chance(50)) addProp(F, DP_BOULDER, x, y, 0.7f, 0.5f);
        }
    }

    // --- room dressing (some empty, some hoarded, treasure off the main path)
    for (int ri = 0; ri < (int)rooms.size(); ri++) {
        const Room& r = rooms[ri];
        if (ri == entryRoom) continue;
        int roll = (int)(rng.next() % 6u);
        int cx, cy; roomCenter(r, cx, cy);
        if (roll <= 1) {
            if (rng.chance(70)) {              // empty chamber: quiet dressing — theme picks the clutter
                int tex = pickThemeQuietProp(rng);
                int px_ = rng.range(r.x, r.x + r.w - 1);
                int py_ = rng.range(r.y, r.y + r.h - 1);
                if (!(px_ == cx && py_ == cy)) addProp(F, tex, px_, py_);
            }
        } else if (roll <= 3) {
            int count = rng.range(1, 3);       // stores & furniture — theme stock
            for (int i = 0; i < count; i++) {
                int tex = pickThemeFurnProp(rng);
                int px_ = rng.range(r.x, r.x + r.w - 1);
                int py_ = rng.range(r.y, r.y + r.h - 1);
                if (px_ == cx && py_ == cy) continue;
                addProp(F, tex, px_, py_);
            }
        } else {                               // guarded hall
            // Crypt themed halls keep true pillared colonnades; others improvise
            bool pillarTheme = (D.theme == TH_CRYPT || D.theme == TH_HEATH || D.theme == TH_AMBER);
            if (pillarTheme && r.w >= 7 && r.h >= 5) {
                addProp(F, DP_PILLAR, r.x + 1, r.y + 1);
                addProp(F, DP_PILLAR, r.x + r.w - 2, r.y + 1);
                if (r.h >= 6) {
                    addProp(F, DP_PILLAR, r.x + 1, r.y + r.h - 2);
                    addProp(F, DP_PILLAR, r.x + r.w - 2, r.y + r.h - 2);
                }
            } else if (r.w >= 6 && r.h >= 5) {
                // alternate theme dressing: boulders for heath, crystal for frost, etc.
                if (D.theme == TH_FROST) {
                    addProp(F, DP_CRYSTAL, r.x+1, r.y+1, 0.7f, 0.6f);
                    addProp(F, DP_CRYSTAL, r.x+r.w-2, r.y+1, 0.7f, 0.6f);
                } else if (D.theme == TH_SUNSCORCH) {
                    addProp(F, DP_URN, r.x+1, r.y+1);
                    addProp(F, DP_URN, r.x+r.w-2, r.y+1);
                } else if (D.theme == TH_HEATH) {
                    addProp(F, DP_BOULDER, r.x+1, r.y+1, 0.7f, 0.55f);
                    addProp(F, DP_BOULDER, r.x+r.w-2, r.y+1, 0.7f, 0.55f);
                } else if (r.w >= 7 && r.h >= 5) {
                    addProp(F, DP_PILLAR, r.x + 1, r.y + 1);
                    addProp(F, DP_PILLAR, r.x + r.w - 2, r.y + 1);
                }
            }
            if (rng.chance(55)) {
                int px_ = rng.range(r.x, r.x + r.w - 1);
                int py_ = rng.range(r.y, r.y + r.h - 1);
                if (!(px_ == cx && py_ == cy)) {
                    int vaultTex = pickThemeVaultProp(rng);
                    addProp(F, vaultTex, px_, py_, 1.3f, 0.9f);
                }
            }
        }
    }

    // Validate the dressed floor using the SAME body clearance as movement.
    // If a furnishing cuts a route, remove it; scenery must never gate stairs
    // or the quest vault. The geometry and decisions remain seed-only.
    auto allFloorReachable = [&]() {
        std::vector<uint8_t> seen(F.tiles.size(), 0);
        std::vector<int> q{dIdx(F.entryTx, F.entryTy, F.w)};
        seen[q.front()] = 1;
        for (size_t i = 0; i < q.size(); ++i) {
            int x = q[i] % F.w, y = q[i] / F.w;
            for (const auto& dir : dirs) {
                int nx = x + dir[0], ny = y + dir[1];
                if (!cellOpen(nx, ny) || dunBlocked(nx + 0.5f, ny + 0.5f, 0.28f)) continue;
                int idx = dIdx(nx, ny, F.w);
                if (!seen[idx]) { seen[idx] = 1; q.push_back(idx); }
            }
        }
        for (int y = 0; y < F.h; ++y)
            for (int x = 0; x < F.w; ++x)
                if (cellOpen(x, y) && !dunBlocked(x + 0.5f, y + 0.5f, 0.28f) &&
                    !seen[dIdx(x, y, F.w)]) return false;
        return true;
    };
    while (!allFloorReachable()) {
        auto it = std::find_if(F.props.rbegin(), F.props.rend(),
                               [](const DunProp& p) { return p.solid; });
        if (it == F.props.rend()) break;
        F.props.erase(std::next(it).base());
    }
    // The farthest tile is a reserved objective, even on the vault floor.
    int objectiveX = farIdx % F.w, objectiveY = farIdx / F.w;
    F.props.erase(std::remove_if(F.props.begin(), F.props.end(), [&](const DunProp& p) {
        return p.solid && fabsf(p.x - (objectiveX + 0.5f)) < p.w * 0.45f + 0.28f &&
               fabsf(p.y - (objectiveY + 0.5f)) < p.h * 0.42f + 0.28f;
    }), F.props.end());

    // --- treasure: dead ends first (best rewards off the beaten path)
    std::vector<std::pair<int, int>> deadEnds;
    for (int y = 1; y < F.h - 1; y++) {
        for (int x = 1; x < F.w - 1; x++) {
            if (!cellOpen(x, y)) continue;
            int open = 0;
            if (cellOpen(x + 1, y)) open++;
            if (cellOpen(x - 1, y)) open++;
            if (cellOpen(x, y + 1)) open++;
            if (cellOpen(x, y - 1)) open++;
            if (open == 1) deadEnds.push_back({ x, y });
        }
    }
    int chestTarget = 3 + (D.floor > 0 ? 1 : 0) + (int)(rng.next() % 2u);
    if (D.floor == D.floors - 1) chestTarget += 1;
    auto placeChest = [&](int tx, int ty, bool vault) {
        if (!cellOpen(tx, ty)) return;
        if (tx == F.entryTx && ty == F.entryTy) return;
        if (tx == F.downTx && ty == F.downTy) return;
        for (auto& c : F.chests) if (c.tx == tx && c.ty == ty) return;
        for (auto& p : F.props) if ((int)(p.x - 0.5f) == tx && (int)(p.y - 0.5f) == ty && p.solid) return;
        // Always consume the same three draws — layout never follows inventory.
        bool gilRoll = rng.chance(vault ? 20 : 34);
        bool amtRoll = rng.chance(30);
        uint32_t pick = rng.next() & 0xffu;
        bool starEligible = vault && D.startCavern && D.floor == D.floors - 1 &&
                            itemCount(IT_STAR_METAL) == 0 &&
                            g.quests[QUEST_DEEP].progress == 0;
        DunChest c;
        c.tx = tx; c.ty = ty;
        c.item = -1; c.amount = 1; c.gil = 0;
        if (!starEligible && gilRoll) {
            c.gil = (vault ? 160 : 30) + D.floor * 40 + (int)(pick % 60u);
        } else {
            c.item = starEligible ? IT_STAR_METAL : rollDungeonLoot(pick, vault);
            c.amount = (itemDef(c.item).kind == IK_CONSUMABLE && amtRoll) ? 2 : 1;
        }
        c.id = chestIdAt(tx, ty, D.floor);
        F.chests.push_back(c);
    };

    // The deep vault on the final floor claims the farthest chamber FIRST,
    // so the story treasure can never be crowded out of its own hoard.
    int guard = 0;
    int placedMobs = 0;
    std::unordered_set<uint64_t> mobSites;
    if (D.floor == D.floors - 1) {
        int vx = F.entryTx, vy = F.entryTy;
        int farDist = -1;
        for (int y = 1; y < F.h - 1; y++)
            for (int x = 1; x < F.w - 1; x++) {
                int idx = dIdx(x, y, F.w);
                if (dist[idx] > farDist && cellOpen(x, y) &&
                    !(x == F.entryTx && y == F.entryTy)) {
                    farDist = dist[idx]; vx = x; vy = y;
                }
            }
        placeChest(vx, vy, true);
        for (int k = 0; k < 2; k++) {          // two guards for the hoard
            int gx = vx + (k == 0 ? -1 : 1), gy = vy + 1;
            if (!cellOpen(gx, gy)) { gx = vx; gy = vy - 1; }
            if (!cellOpen(gx, gy) || dunBlocked(gx + 0.5f, gy + 0.5f, 0.28f)) continue;
            // Draw first, then consult the kill log — a dead guard must not
            // shift the rng stream the rest of the floor is built from.
            EnemyKind kind = pickDungeonEnemy(rng, D.zoneBase + D.floor * 2 + 2);
            float phase = rng.f() * 6.28f;
            ++placedMobs;
            F.mobAnchors.push_back({gx, gy});
            if (g.defeatedMonsters.count(mobIdAt(gx, gy, D.floor, 90 + k))) continue;
            DunMob m;
            m.id = mobIdAt(gx, gy, D.floor, 90 + k);
            m.kind = kind;
            m.x = m.ax = gx + 0.5f; m.y = m.ay = gy + 0.5f;
            m.phase = phase;
            F.mobs.push_back(m);
        }
    }

    for (auto& de : deadEnds) {                // dead-end hoards
        if ((int)F.chests.size() >= chestTarget) break;
        if (rng.chance(65)) placeChest(de.first, de.second, false);
    }
    while ((int)F.chests.size() < chestTarget && guard++ < 40) {
        const Room& r = rooms[(int)(rng.next() % (uint32_t)rooms.size())];
        placeChest(rng.range(r.x, r.x + r.w - 1), rng.range(r.y, r.y + r.h - 1), false);
    }

    // --- roaming inhabitants — theme nudges density: crypt/frost a touch lonelier, mire busier
    int mobCount = std::min(3 + D.floor, 6);
    if (D.theme == TH_MIRE) mobCount = std::min(4 + D.floor, 7);
    else if (D.theme == TH_FROST) mobCount = std::max(2, std::min(2 + D.floor, 5));
    else if (D.theme == TH_HEATH) mobCount = std::min(4 + D.floor, 7);
    else if (D.theme == TH_CRYPT) mobCount = std::min(3 + D.floor, 6);
    guard = 0;
    while (placedMobs < mobCount && guard++ < 60) {
        const Room& r = rooms[(int)(rng.next() % (uint32_t)rooms.size())];
        int mx = rng.range(r.x, r.x + r.w - 1);
        int my = rng.range(r.y, r.y + r.h - 1);
        if (!cellOpen(mx, my) || dunBlocked(mx + 0.5f, my + 0.5f, 0.28f)) continue;
        if (mx == F.entryTx && my == F.entryTy) continue;
        if (mx == F.downTx && my == F.downTy) continue;
        int ddx = mx - F.entryTx, ddy = my - F.entryTy;
        if (ddx * ddx + ddy * ddy < 36) continue;
        bool onChest = false;
        for (auto& c : F.chests) if (c.tx == mx && c.ty == my) onChest = true;
        if (onChest) continue;
        // Full draws before the kill-log check so visits stay aligned.
        EnemyKind kind = pickDungeonEnemy(rng, D.zoneBase + D.floor * 2);
        float phase = rng.f() * 6.28f;
        uint64_t id = mobIdAt(mx, my, D.floor, 0);
        if (!mobSites.insert(id).second) continue;
        ++placedMobs;
        F.mobAnchors.push_back({mx, my});
        if (g.defeatedMonsters.count(id)) continue;
        DunMob m;
        m.id = id;
        m.kind = kind;
        m.x = m.ax = mx + 0.5f; m.y = m.ay = my + 0.5f;
        m.phase = phase;
        F.mobs.push_back(m);
    }

    weatherCavern(F);
    D.encounterGrace = 2.0f;
}

// ================================================================ pathfinding
bool dunFindPath(int sx, int sy, int tx, int ty, std::vector<std::pair<int, int>>& out) {
    out.clear();
    if (!cellOpen(tx, ty) || !cellOpen(sx, sy) || dunBlocked(tx + 0.5f, ty + 0.5f, 0.28f)) return false;
    int w = D.F.w, h = D.F.h;
    std::vector<int> prev((size_t)w * h, -1);
    std::vector<int> queue;
    int start = dIdx(sx, sy, w), goal = dIdx(tx, ty, w);
    queue.push_back(start);
    prev[start] = start;
    const int dirs[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
    for (size_t qi = 0; qi < queue.size() && prev[goal] < 0; qi++) {
        int idx = queue[qi];
        int x = idx % w, y = idx / w;
        for (auto& d : dirs) {
            int nx = x + d[0], ny = y + d[1];
            if (!cellOpen(nx, ny) || dunBlocked(nx + 0.5f, ny + 0.5f, 0.28f)) continue;
            int nidx = dIdx(nx, ny, w);
            if (prev[nidx] >= 0) continue;
            prev[nidx] = idx;
            queue.push_back(nidx);
        }
    }
    if (prev[goal] < 0) return false;
    int idx = goal;
    while (idx != start) {
        out.push_back({ idx % w, idx / w });
        idx = prev[idx];
    }
    std::reverse(out.begin(), out.end());
    return !out.empty();
}

// ================================================================ transitions
void revealDungeon() {
    auto& seen = D.explored[D.floor];
    seen.resize(D.F.tiles.size(), 0);
    int px = (int)floorf(D.px), py = (int)floorf(D.py);
    for (int y = py - 6; y <= py + 6; ++y)
        for (int x = px - 6; x <= px + 6; ++x) {
            int dx = x - px, dy = y - py;
            if (!inBounds(x, y) || dx * dx + dy * dy > 36) continue;
            // Reveal only what the lantern can see, not rooms through walls.
            int steps = std::max(abs(dx), abs(dy));
            bool visible = true;
            for (int i = 1; i < steps; ++i) {
                int rx = (int)roundf(px + dx * (float)i / steps);
                int ry = (int)roundf(py + dy * (float)i / steps);
                if (!cellOpen(rx, ry)) { visible = false; break; }
            }
            if (visible) seen[dIdx(x, y, D.F.w)] = 1;
        }
}

void generateAndEnterFloor(bool fromBelow = false) {
    generateFloor();
    D.px = D.F.entryTx + 0.5f;
    D.py = D.F.entryTy + 0.5f;
    if (fromBelow && D.F.downTx >= 0) {
        D.px = D.F.downTx + 0.5f;
        D.py = D.F.downTy + 0.5f;
    }
    revealDungeon();
    D.facing = 0; D.sideFlip = false;
    D.path.clear();
    D.pendingInteract = false;
    D.clickFlashT = 0;
    D.stepDist = 0;
    // Snap the camera onto the new floor.
    float tx = D.px * TILE - g.vw * 0.5f;
    float ty = D.py * TILE - g.vh * 0.5f;
    float maxX = (float)D.F.w * TILE - g.vw;
    float maxY = (float)D.F.h * TILE - g.vh;
    D.camX = (maxX > 0) ? std::max(0.0f, std::min(tx, maxX)) : tx;
    D.camY = (maxY > 0) ? std::max(0.0f, std::min(ty, maxY)) : ty;
}

} // namespace

// ================================================================ public API
bool dungeonActive() { return D.active; }

void enterDungeon(int siteTx, int siteTy, bool startCavern) {
    D = Dungeon{};
    D.active = true;
    D.siteTx = siteTx; D.siteTy = siteTy;
    D.startCavern = startCavern;
    D.seed = g.worldSeed ^ hash2i(siteTx, siteTy, 0xD0A6Eu) ^ (startCavern ? 0x57ACu : 0u);
    // theme is derived from surface biome + seed so every mouth has a stable identity
    if (startCavern) D.theme = TH_MOSSY;
    else D.theme = themeForSite(siteTx, siteTy, D.seed);
    D.themeName = dunThemeName(D.theme);
    // theme can nudge depth: heath/crypt feel deeper, frost/sunscorch a touch shallower
    int baseFloors = startCavern ? 4 : (2 + (int)((D.seed >> 9) & 1u));
    if (!startCavern) {
        if (D.theme == TH_HEATH || D.theme == TH_CRYPT) baseFloors = std::min(4, baseFloors + (D.seed & 1u ? 1 : 0));
        else if (D.theme == TH_FROST) baseFloors = std::max(2, baseFloors - (int)(D.seed & 1u));
    }
    D.floors = baseFloors;
    D.floor = 0;
    // keep Hollow Hill's historic name; other caverns get a theme-tinted title
    {
        const char* base = startCavern ? "Hollow Hill" : cavernNameFor(D.seed);
        D.name = base;
        // fullName for HUD includes theme; keep D.name as short base for logs/saves
    }
    D.zoneBase = std::max(1, zoneLevelAt((float)siteTx + 0.5f, (float)siteTy + 1.5f));
    generateAndEnterFloor();
    g.mode = MODE_DUNGEON;
    g.fade = 1.0f; g.fadeDir = -1;
    g.encounterGrace = 3.0f;
    playSoundIf(g.assets.door);
}

void exitDungeon() {
    if (!D.active) return;
    D.active = false;
    D.path.clear();
    g.mode = MODE_OVERWORLD;
    g.fade = 1.0f; g.fadeDir = -1;
    g.encounterGrace = 3.5f;
    overworldSnapCamera();
    playSoundIf(g.assets.door);
}

void dungeonLeaveToOverworld() { exitDungeon(); }

// ================================================================ interaction
static void giveChestLoot(const DunChest& c) {
    if (c.gil > 0) {
        g.gil += c.gil;
        char buf[64];
        snprintf(buf, sizeof(buf), "Found %d gil!", c.gil);
        pushToast(buf);
    } else if (c.item >= 0) {
        addItem(c.item, c.amount);
        const ItemDef& d = itemDef(c.item);
        char buf[96];
        if (c.amount > 1) snprintf(buf, sizeof(buf), "Found %s x%d!", d.name, c.amount);
        else snprintf(buf, sizeof(buf), "Found %s!", d.name);
        pushToast(buf);
        if (c.item == IT_STAR_METAL) {
            if (questActive(QUEST_DEEP)) {
                g.quests[QUEST_DEEP].progress = 1;
                playSoundIf(g.assets.levelup);
                std::vector<DialogLine> lines;
                lines.push_back({ FACE_MAGE, "Iris", "Cold to the touch — and it hums like a struck bell." });
                lines.push_back({ FACE_CLERIC, "Pip", "Star Metal! Nell at the forge will want to see this." });
                startDialog(lines);
            } else {
                playSoundIf(g.assets.treasure);
            }
        }
    }
}

static void dungeonInteract(bool silent, uint64_t chestId = 0) {
    // 1. chests
    for (const auto& c : D.F.chests) {
        if (chestId && c.id != chestId) continue;
        float dx = (c.tx + 0.5f) - D.px, dy = (c.ty + 0.5f) - D.py;
        if (dx * dx + dy * dy > 1.7f * 1.7f) continue;
        if (g.openedChests.count(c.id)) continue;
        g.openedChests.insert(c.id);
        giveChestLoot(c);
        playSoundIf(g.assets.treasure);
        return;
    }
    if (chestId) return; // a targeted click must never activate nearby stairs
    // 2. stairs down
    if (D.F.downTx >= 0) {
        float dx = (D.F.downTx + 0.5f) - D.px, dy = (D.F.downTy + 0.5f) - D.py;
        if (dx * dx + dy * dy < 1.3f * 1.3f) {
            D.floor++;
            generateAndEnterFloor();
            char buf[64];
            snprintf(buf, sizeof(buf), "%s — B%d", D.name.c_str(), D.floor + 1);
            pushToast(buf);
            playSoundIf(g.assets.door);
            g.fade = 0.6f; g.fadeDir = -1;
            return;
        }
    }
    // 3. stairs up / the way out
    {
        float dx = (D.F.entryTx + 0.5f) - D.px, dy = (D.F.entryTy + 0.5f) - D.py;
        if (dx * dx + dy * dy < 1.3f * 1.3f) {
            if (D.floor == 0) {
                exitDungeon();
            } else {
                int target = D.floor - 1;
                D.floor = target;
                generateAndEnterFloor(true);
                char buf[64];
                snprintf(buf, sizeof(buf), "%s — B%d", D.name.c_str(), D.floor + 1);
                pushToast(buf);
                playSoundIf(g.assets.door);
                g.fade = 0.6f; g.fadeDir = -1;
            }
            return;
        }
    }
    if (!silent) pushToast("The dark listens.");
}

const char* dungeonPrompt() {
    if (!D.active || g.mode != MODE_DUNGEON) return nullptr;
    static char buf[96];
    for (const auto& c : D.F.chests) {
        float dx = (c.tx + 0.5f) - D.px, dy = (c.ty + 0.5f) - D.py;
        if (dx * dx + dy * dy < 1.7f * 1.7f && !g.openedChests.count(c.id))
            return "SPACE: open chest";
    }
    if (D.F.downTx >= 0) {
        float dx = (D.F.downTx + 0.5f) - D.px, dy = (D.F.downTy + 0.5f) - D.py;
        if (dx * dx + dy * dy < 1.3f * 1.3f) return "SPACE: descend deeper";
    }
    {
        float dx = (D.F.entryTx + 0.5f) - D.px, dy = (D.F.entryTy + 0.5f) - D.py;
        if (dx * dx + dy * dy < 1.3f * 1.3f)
            return D.floor == 0 ? "SPACE: climb back to daylight" : "SPACE: climb up";
    }
    snprintf(buf, sizeof(buf), "B%d  |  WASD move  |  SPACE inspect  |  ESC menu", D.floor + 1);
    return buf;
}

// ================================================================ update
static void updateDungeonCamera(float dt) {
    float targetX = D.px * TILE - g.vw * 0.5f;
    float targetY = D.py * TILE - g.vh * 0.5f;
    float maxX = (float)D.F.w * TILE - g.vw;
    float maxY = (float)D.F.h * TILE - g.vh;
    if (maxX > 0) targetX = std::max(0.0f, std::min(targetX, maxX));
    else targetX = maxX * 0.5f;
    if (maxY > 0) targetY = std::max(0.0f, std::min(targetY, maxY));
    else targetY = maxY * 0.5f;
    float k = 1.0f - powf(0.0004f, dt);
    D.camX += (targetX - D.camX) * k;
    D.camY += (targetY - D.camY) * k;
    if (!std::isfinite(D.camX)) D.camX = targetX;
    if (!std::isfinite(D.camY)) D.camY = targetY;
}

static void dungeonStartBattle(EnemyKind primary, uint64_t monsterId = 0) {
    Rng rng(hash2i(D.stepCounter, g.defeatedCount, D.seed ^ 0xBAD1u));
    std::vector<EnemyKind> group;
    group.push_back(primary);
    int zl = D.zoneBase + D.floor * 2;
    if (zl >= 3 && rng.chance(55)) group.push_back(pickDungeonEnemy(rng, zl));
    if (zl >= 6 && rng.chance(35)) group.push_back(pickDungeonEnemy(rng, zl));
    battleStart(group, zl, monsterId);
    D.encounterGrace = 3.0f;
    g.encounterGrace = 3.0f;
    D.path.clear();
    D.pendingInteract = false;
}

static void dungeonStepPath(float dt) {
    if (D.path.empty()) { D.moving = false; return; }
    auto [tx, ty] = D.path.front();
    float cx = tx + 0.5f, cy = ty + 0.5f;
    float dx = cx - D.px, dy = cy - D.py;
    float dist = sqrtf(dx * dx + dy * dy);
    float speed = (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) ? 7.2f : 4.4f;
    float stepLen = speed * dt;
    if (fabsf(dx) > fabsf(dy) && dist > 0.01f) {
        D.facing = 2; D.sideFlip = dx < 0;
    } else if (dist > 0.01f) {
        D.facing = dy > 0 ? 0 : 1;
    }
    if ((dist <= stepLen || dist < 0.001f) && !dunBlocked(cx, cy, 0.28f)) {
        D.px = cx; D.py = cy;
        D.path.erase(D.path.begin());
        D.animT += dt; D.moving = true;
        return;
    }
    float nx = D.px + dx / dist * stepLen;
    float ny = D.py + dy / dist * stepLen;
    if (!dunBlocked(nx, ny, 0.28f)) {
        D.px = nx; D.py = ny;
        D.moving = true;
        D.animT += dt;
    } else {
        D.path.clear();
        D.moving = false;
        pulseBlocked();
    }
}

static void dungeonClickMove() {
    if (g.hotButtons > 0) return;
    Vector2 m = getVirtualMouse();
    if (m.x < 0 || m.x >= g.vw || m.y < 0 || m.y >= g.vh) return;
    if (!IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) return;
    // HUD panels consume clicks instead of moving to hidden world tiles.
    if (m.y < 27 || m.y >= g.vh - 62 ||
        (m.x >= g.vw - D.F.w * 2 - 15 && m.y >= 27 && m.y <= 42 + D.F.h * 2)) return;
    float wx = (m.x + D.camX) / TILE;
    float wy = (m.y + D.camY) / TILE;
    int tx = (int)floorf(wx), ty = (int)floorf(wy);
    // Clicking a chest walks up to it, then opens it.
    for (size_t i = 0; i < D.F.chests.size(); i++) {
        const DunChest& c = D.F.chests[i];
        if (c.tx == tx && c.ty == ty) {
            D.path.clear();
            D.pendingInteract = false;
            float dx = c.tx + 0.5f - D.px, dy = c.ty + 0.5f - D.py;
            if (dx * dx + dy * dy <= 1.7f * 1.7f) {
                dungeonInteract(true, c.id);
                return;
            }
            // Try every approach and take the shortest reachable one, rather
            // than picking the first floor cell (which may contain furniture).
            const int dirs[4][2] = {{0, 1}, {0, -1}, {1, 0}, {-1, 0}};
            for (const auto& dir : dirs) {
                std::vector<std::pair<int, int>> path;
                if (dunFindPath((int)floorf(D.px), (int)floorf(D.py),
                                c.tx + dir[0], c.ty + dir[1], path) &&
                    (D.path.empty() || path.size() < D.path.size())) D.path = std::move(path);
            }
            if (!D.path.empty()) {
                D.pendingInteract = true;
                D.pendingChestId = c.id;
                D.clickFlashT = 0.35f;
                D.clickFlashX = c.tx + 0.5f; D.clickFlashY = c.ty + 0.5f;
            } else pulseBlocked();
            return;
        }
    }
    std::vector<std::pair<int, int>> p;
    if (tx == (int)floorf(D.px) && ty == (int)floorf(D.py)) return;   // clicked own cell
    if (dunFindPath((int)floorf(D.px), (int)floorf(D.py), tx, ty, p)) {
        D.path = p;
        D.pendingInteract = false;
        D.clickFlashT = 0.4f;
        D.clickFlashX = tx + 0.5f; D.clickFlashY = ty + 0.5f;
    } else {
        pulseBlocked();
    }
}

void dungeonUpdate(float dt) {
    gAnimTime += dt;
    g.totalTime += dt;
    g.runTime += dt;
    if (g.toastT > 0) g.toastT = std::max(0.0f, g.toastT - dt);
    if (g.clickFlashT > 0) g.clickFlashT = std::max(0.0f, g.clickFlashT - dt);
    if (g.blockedFx > 0) g.blockedFx = std::max(0.0f, g.blockedFx - dt * 2.0f);
    if (g.shakeT > 0) g.shakeT = std::max(0.0f, g.shakeT - dt);
    if (!D.active) return;
    g.encounterGrace = std::max(0.0f, g.encounterGrace - dt);
    D.clickFlashT = std::max(0.0f, D.clickFlashT - dt);
    D.F.mobs.erase(std::remove_if(D.F.mobs.begin(), D.F.mobs.end(),
        [](const DunMob& m) { return g.defeatedMonsters.count(m.id) != 0; }), D.F.mobs.end());

    int kx = 0, ky = 0;
    if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) kx -= 1;
    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) kx += 1;
    if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)) ky -= 1;
    if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)) ky += 1;

    D.moving = false;
    if (kx != 0 || ky != 0) {
        D.path.clear();
        D.pendingInteract = false;
        if (kx != 0 && ky != 0) kx = 0;              // cardinal only, like the SNES
        if (kx < 0) { D.facing = 2; D.sideFlip = true; }
        else if (kx > 0) { D.facing = 2; D.sideFlip = false; }
        else if (ky < 0) D.facing = 1;
        else D.facing = 0;
        float speed = (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) ? 7.2f : 4.4f;
        float prevX = D.px, prevY = D.py;
        if (kx != 0) {
            float nx = D.px + kx * speed * dt;
            if (!dunBlocked(nx, D.py, 0.28f)) D.px = nx;
        } else {
            float ny = D.py + ky * speed * dt;
            if (!dunBlocked(D.px, ny, 0.28f)) D.py = ny;
        }
        if (fabsf(D.px - prevX) + fabsf(D.py - prevY) > 0.0001f) {
            D.moving = true;
            D.animT += dt;
        } else {
            pulseBlocked();
        }
    } else if (!D.path.empty()) {
        dungeonStepPath(dt);
        if (D.path.empty() && D.pendingInteract) {
            D.pendingInteract = false;
            dungeonInteract(true, D.pendingChestId);
            return; // interaction can replace the floor or enter a dialogue
        }
    }

    revealDungeon();

    // wandering residents
    for (auto& m : D.F.mobs) {
        float ph = m.phase + g.totalTime;
        float cx = m.ax + sinf(ph * 0.7f) * 0.65f;
        float cy = m.ay + cosf(ph * 0.9f) * 0.45f;
        if (!dunBlocked(cx, cy, 0.22f)) { m.x = cx; m.y = cy; }
        else { m.x = m.ax; m.y = m.ay; }
        if (g.encounterGrace > 0.0f) continue;
        float dx = D.px - m.x, dy = D.py - m.y;
        if (dx * dx + dy * dy < 0.72f * 0.72f) {
            dungeonStartBattle(m.kind, m.id);
            return;
        }
    }

    // step encounters (the floor bites back)
    if (D.moving) {
        D.stepDist += dt * 4.4f;
        if (D.encounterGrace > 0) D.encounterGrace = std::max(0.0f, D.encounterGrace - dt);
        else if (g.encounterGrace <= 0 && D.stepDist >= 5.0f) {
            D.stepDist -= 5.0f;
            D.stepCounter++;
            int zl = D.zoneBase + D.floor * 2;
            float chance = std::min(0.30f, 0.13f + 0.012f * (float)zl);
            float roll = hashf(D.stepCounter * 17, (int)(D.px + D.py) * 19 + g.defeatedCount * 7,
                               D.seed ^ 0x3ec1u);
            if (roll < chance) {
                Rng rng(D.seed ^ (uint32_t)D.stepCounter * 2654435761u);
                dungeonStartBattle(pickDungeonEnemy(rng, zl));
                return;
            }
        }
    } else {
        D.stepDist = std::max(0.0f, D.stepDist - dt * 2.0f);
        if (D.encounterGrace > 0) D.encounterGrace = std::max(0.0f, D.encounterGrace - dt * 0.4f);
    }

    if (g.mode != MODE_DUNGEON) return;
    updateDungeonCamera(dt);

    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_TAB)) {
        g.menuReturn = MODE_DUNGEON;
        g.mode = MODE_MENU;
        g.menuPage = 0; g.menuIndex = 0; g.journalTab = 0; g.commonsJournalSel = 0;
        playSoundIf(g.assets.confirm);
        return;
    }
    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_E) || IsKeyPressed(KEY_ENTER)) {
        D.path.clear();
        D.pendingInteract = false;
        dungeonInteract(false);
        return;
    }
    dungeonClickMove();
}

// ================================================================ draw
void dungeonDraw() {
    float shakeX = 0, shakeY = 0;
    if (g.shakeT > 0) {
        shakeX = sinf(g.totalTime * 60.0f) * g.shakeMag;
        shakeY = cosf(g.totalTime * 47.0f) * g.shakeMag;
    }
    float camX = roundf(D.camX - shakeX);
    float camY = roundf(D.camY - shakeY);
    ThemeVis vis = themeVisual(D.theme);
    ClearBackground(vis.clear);

    int x0 = (int)floorf(camX / TILE) - 1;
    int y0 = (int)floorf(camY / TILE) - 1;
    int x1 = x0 + g.vw / TILE + 3;
    int y1 = y0 + g.vh / TILE + 3;

    Texture2D floorTex = g.assets.dunFloor[D.theme];
    if (floorTex.id == 0) floorTex = g.assets.stoneFloor.id ? g.assets.stoneFloor : g.assets.tileFloor;
    Texture2D brickTex = g.assets.dunWall[D.theme];
    if (brickTex.id == 0) brickTex = g.assets.brickFloor[1].id ? g.assets.brickFloor[1] : g.assets.brickFloor[0];

    // Mineral tints preserve a shared rock material while making the seven
    // natural biomes distinct. The Deep Crypt keeps its existing masonry.
    const Color caveTint[TH_COUNT] = {
        {175,190,158,255}, {184,169,151,255}, {220,192,150,255},
        {143,182,165,255}, {183,186,181,255}, {168,196,222,255},
        {226,194,153,255}, WHITE
    };
    bool natural = D.theme != TH_CRYPT && g.assets.caveFloor[0].id && g.assets.caveRock[0].id;
    // --- ground layer
    for (int ty = y0; ty <= y1; ty++) {
        for (int tx = x0; tx <= x1; tx++) {
            float sx = tx * TILE - camX, sy = ty * TILE - camY;
            if (natural && inBounds(tx, ty)) {
                uint32_t material = hash2i(tx, ty, D.seed ^ (uint32_t)D.floor * 1877u);
                Color tint = caveTint[D.theme];
                if (cellOpen(tx, ty)) {
                    Texture2D t = g.assets.caveFloor[material % 4u];
                    if (!t.id) t = g.assets.caveFloor[0];
                    DrawTexturePro(t, {0,0,(float)t.width,(float)t.height},
                                   {sx,sy,TILE,TILE}, {0,0}, 0, tint);
                    // Narrow contact shadows make the boundary read as rock
                    // overhanging a floor, without changing collision.
                    if (!cellOpen(tx, ty-1))
                        DrawRectangle((int)sx, (int)sy, TILE, 3, Fade(BLACK, 0.32f));
                    if (!cellOpen(tx-1, ty))
                        DrawRectangle((int)sx, (int)sy, 2, TILE, Fade(BLACK, 0.18f));
                    if (D.theme == TH_FROST && material % 19u == 0u && g.assets.iceCrack.id)
                        DrawTexturePro(g.assets.iceCrack, {0,0,(float)g.assets.iceCrack.width,(float)g.assets.iceCrack.height},
                                       {sx,sy,TILE,TILE}, {0,0}, 0, Fade(WHITE,0.45f));
                } else {
                    bool exposed = cellOpen(tx,ty+1) || cellOpen(tx-1,ty) || cellOpen(tx+1,ty) || cellOpen(tx,ty-1);
                    Texture2D t = g.assets.caveRock[material % 4u];
                    if (!t.id) t = g.assets.caveRock[0];
                    Color rock = exposed ? tint : Color{(unsigned char)(tint.r * 0.54f),
                        (unsigned char)(tint.g * 0.54f), (unsigned char)(tint.b * 0.54f), 255};
                    DrawTexturePro(t, {0,0,(float)t.width,(float)t.height},
                                   {sx,sy,TILE,TILE}, {0,0}, 0, rock);
                    if (cellOpen(tx,ty+1))
                        DrawRectangle((int)sx, (int)sy + 14, TILE, 2, Fade(BLACK, 0.35f));
                }
            } else if (cellOpen(tx, ty)) {
                if (floorTex.id)
                    DrawTexturePro(floorTex, { 0, 0, (float)floorTex.width, (float)floorTex.height },
                                   { sx, sy, TILE, TILE }, { 0, 0 }, 0.0f, WHITE);
                else
                    DrawRectangle((int)sx, (int)sy, TILE, TILE, Color{ 84, 76, 92, 255 });
                if (((tx + ty) & 1) == 0)
                    DrawRectangle((int)sx, (int)sy, TILE, TILE, Fade(Color{ 0, 0, 8, 255 }, 0.08f));
                // theme-tinted floor variance: cracks, moss tufts, sand ripples drawn as subtle overlay
                uint32_t fh = hash2i(tx, ty, D.seed ^ (uint32_t)D.theme * 7919u);
                if ((fh % 23u) == 0u && g.assets.iceCrack.id && D.theme == TH_FROST) {
                    DrawTexturePro(g.assets.iceCrack, {0,0,(float)g.assets.iceCrack.width,(float)g.assets.iceCrack.height},
                                   {sx,sy,TILE,TILE},{0,0},0.0f, Fade(WHITE,0.35f));
                } else if ((fh % 31u)==0u && g.assets.pebble.id && (D.theme==TH_HEATH || D.theme==TH_MOSSY)) {
                    DrawTexturePro(g.assets.pebble, {0,0,(float)g.assets.pebble.width,(float)g.assets.pebble.height},
                                   {sx+2,sy+4,12,8},{0,0},0.0f, Fade(WHITE,0.28f));
                } else if ((fh % 29u)==0u && g.assets.tundraFloor.id && D.theme==TH_HEATH) {
                    DrawTexturePro(g.assets.tundraFloor,{0,0,(float)g.assets.tundraFloor.width,(float)g.assets.tundraFloor.height},
                                   {sx,sy,TILE,TILE},{0,0},0.0f, Fade(Color{200,190,170,255},0.18f));
                }
            } else if (inBounds(tx, ty)) {
                bool face = cellOpen(tx, ty + 1);
                if (face && brickTex.id) {
                    // south-facing wall face catches theme light
                    DrawTexturePro(brickTex, { 0, 0, (float)brickTex.width, (float)brickTex.height },
                                   { sx, sy, TILE, TILE }, { 0, 0 }, 0.0f,
                                   vis.wallLight);
                    DrawRectangle((int)sx, (int)sy, TILE, 3, Fade(vis.wallLight, 0.95f));
                    DrawRectangle((int)sx, (int)(sy + TILE - 4), TILE, 4, vis.wallDark);
                } else if (brickTex.id) {
                    DrawTexturePro(brickTex, { 0, 0, (float)brickTex.width, (float)brickTex.height },
                                   { sx, sy, TILE, TILE }, { 0, 0 }, 0.0f,
                                   vis.wallDark);
                } else {
                    DrawRectangle((int)sx, (int)sy, TILE, TILE, vis.wallDark);
                }
            } else {
                DrawRectangle((int)sx, (int)sy, TILE, TILE, Fade(vis.clear, 0.9f));
            }
        }
    }

    // Sconces are fixtures, not invisible light sources — tinted per theme
    for (const auto& sc : D.F.sconces) {
        if (!g.assets.torch.id) break;
        float cx = (sc.first + 0.5f) * TILE - camX;
        float fy = (sc.second + 1.0f) * TILE - camY;
        // frost shows crystal sparkle, mire uses lantern green, sunscorch warm brazier
        Texture2D sconceTex = g.assets.torch;
        Color sconceTint = vis.sconceTint;
        if (D.theme == TH_FROST && g.assets.crystal.id) { sconceTex = g.assets.crystal; sconceTint = Color{180,220,255,255}; }
        else if (D.theme == TH_MIRE && g.assets.lantern.id) { sconceTex = g.assets.lantern; sconceTint = Color{170,255,150,255}; }
        else if (D.theme == TH_SUNSCORCH && g.assets.candle.id) { sconceTex = g.assets.candle; sconceTint = Color{255,200,120,255}; }
        texFootLocal(sconceTex, cx, fy, sconceTint);
    }

    // --- stair decals (drawn on the floor, revealed by torchlight)
    if (g.assets.stairUp.id) {
        float sx = D.F.entryTx * TILE - camX, sy = D.F.entryTy * TILE - camY;
        DrawTexturePro(g.assets.stairUp, { 0, 0, (float)g.assets.stairUp.width, (float)g.assets.stairUp.height },
                       { sx, sy, TILE, TILE }, { 0, 0 }, 0.0f, WHITE);
    }
    if (D.F.downTx >= 0 && g.assets.stairDown.id) {
        float sx = D.F.downTx * TILE - camX, sy = D.F.downTy * TILE - camY;
        DrawTexturePro(g.assets.stairDown, { 0, 0, (float)g.assets.stairDown.width, (float)g.assets.stairDown.height },
                       { sx, sy, TILE, TILE }, { 0, 0 }, 0.0f, WHITE);
        float pulse = 0.5f + 0.5f * sinf(g.totalTime * 3.0f);
        DrawCircleGradient((int)(sx + 8), (int)(sy + 8), 14 + pulse * 4,
                           Fade(Color{ 120, 160, 255, 255 }, 0.20f), BLANK);
    }

    // --- click flash
    if (D.clickFlashT > 0) {
        float sx = D.clickFlashX * TILE - camX, sy = D.clickFlashY * TILE - camY;
        DrawRectangleLines((int)(sx - 6), (int)(sy - 6), 12, 12,
                           Fade(Color{ 255, 232, 150, 255 }, D.clickFlashT));
    }

    // --- Y-sorted actors & props
    struct Item { float footY; int kind; int idx; };
    static std::vector<Item> list;
    list.clear();
    for (int i = 0; i < (int)D.F.props.size(); i++)
        list.push_back({ D.F.props[i].y * TILE, 0, i });
    for (int i = 0; i < (int)D.F.chests.size(); i++)
        list.push_back({ (D.F.chests[i].ty + 1.0f) * TILE, 1, i });
    for (int i = 0; i < (int)D.F.mobs.size(); i++)
        list.push_back({ (D.F.mobs[i].y + 0.35f) * TILE, 2, i });
    list.push_back({ (D.py + 0.35f) * TILE, 3, 0 });
    std::stable_sort(list.begin(), list.end(),
                     [](const Item& a, const Item& b) { return a.footY < b.footY; });

    for (const auto& it : list) {
        if (it.kind == 0) {
            const DunProp& p = D.F.props[it.idx];
            Texture2D* t = dunPropTexture(p.tex);
            if (!t || t->id == 0) continue;
            float cx = p.x * TILE - camX;
            float fy = (p.y + 0.45f) * TILE - camY;
            if (p.tex != DP_MUSHROOM && p.tex != DP_CANDLE && p.tex != DP_LANTERN)
                DrawEllipse((int)cx, (int)fy - 1, t->width * 0.28f, 2.5f, Fade(BLACK, 0.22f));
            texFootLocal(*t, cx, fy);
            if (p.tex == DP_TORCH || p.tex == DP_LANTERN) {
                float pulse = 0.5f + 0.5f * sinf(g.totalTime * 5.0f + p.x);
                DrawCircleGradient((int)cx, (int)(fy - t->height * 0.6f), 18 + pulse * 5,
                                   Fade(Color{ 255, 170, 60, 255 }, 0.20f), BLANK);
            }
        } else if (it.kind == 1) {
            const DunChest& c = D.F.chests[it.idx];
            bool opened = g.openedChests.count(c.id) > 0;
            Texture2D& t = opened ? g.assets.chestOpen : g.assets.chestClosed;
            float cx = (c.tx + 0.5f) * TILE - camX;
            float fy = (c.ty + 1.0f) * TILE - camY;
            DrawEllipse((int)cx, (int)fy - 1, 6.0f, 2.5f, Fade(BLACK, 0.25f));
            texFootLocal(t, cx, fy, opened ? WHITE : Color{ 255, 240, 200, 255 });
            if (!opened) {
                float pulse = 0.5f + 0.5f * sinf(g.totalTime * 4.0f + (float)c.tx);
                DrawCircleGradient((int)cx, (int)(fy - 8), 12 + pulse * 3,
                                   Fade(Color{ 255, 220, 120, 255 }, 0.22f), BLANK);
            }
        } else if (it.kind == 2) {
            const DunMob& m = D.F.mobs[it.idx];
            Texture2D& t = g.assets.enemy[m.kind];
            if (t.id == 0) continue;
            float ph = m.phase + g.totalTime;
            float bob = sinf(ph * 3.0f) * 1.5f;
            float sx = m.x * TILE - camX;
            float sy = (m.y + 0.35f) * TILE - camY;
            DrawEllipse((int)sx, (int)sy - 1, t.width * 0.28f, 3.0f, Fade(BLACK, 0.25f));
            texFootLocal(t, sx, sy + bob, WHITE, 0.62f);
            float dx = D.px - m.x, dy = D.py - m.y;
            if (dx * dx + dy * dy < 9.0f && fmodf(g.totalTime * 2.0f, 1.0f) < 0.6f)
                drawWorldLabel("!", sx, sy - t.height * 0.62f - 13,
                               Color{ 255, 110, 110, 255 }, 8);
        } else {
            Texture2D& sheet = g.assets.heroWalk[std::max(0, std::min(2, g.party[0].klass))];
            int row = (D.facing == 0) ? 0 : (D.facing == 1 ? 1 : 2);
            int frame = D.moving ? (1 + ((int)(D.animT * 7.0f) & 1)) : 0;
            float cx = D.px * TILE - camX;
            float fy = (D.py + 0.35f) * TILE - camY;
            DrawEllipse((int)cx, (int)fy - 1, 5.5f, 2.5f, Fade(BLACK, 0.28f));
            Rectangle src{ (float)(frame * 24), (float)(row * 32), 24, 32 };
            if (D.facing == 2 && D.sideFlip) src.width = -24;
            DrawTexturePro(sheet, src, { floorf(cx - 12.0f), floorf(fy - 32.0f), 24, 32 },
                           { 0, 0 }, 0.0f, WHITE);
        }
    }

    // --- darkness & torchlight — theme tints the fog and every glow
    DrawRectangle(0, 0, g.vw, g.vh, Fade(vis.fog, 0.34f));
    BeginBlendMode(BLEND_ADDITIVE);
    for (auto& sc : D.F.sconces) {
        float cx = (sc.first + 0.5f) * TILE - camX;
        float cy = (sc.second + 1.0f) * TILE - camY;
        if (cx < -40 || cx > g.vw + 40 || cy < -40 || cy > g.vh + 40) continue;
        float pulse = 0.5f + 0.5f * sinf(g.totalTime * 5.0f + (float)(sc.first * 7 + sc.second));
        DrawCircleGradient((int)cx, (int)cy, 30 + pulse * 7,
                           Fade(vis.torchGlow, 0.32f), BLANK);
    }
    for (auto& p : D.F.props) {
        if (p.tex != DP_TORCH && p.tex != DP_LANTERN && p.tex != DP_CANDLE && p.tex != DP_CRYSTAL) continue;
        float cx = p.x * TILE - camX;
        float cy = (p.y + 0.2f) * TILE - camY;
        float pulse = 0.5f + 0.5f * sinf(g.totalTime * 5.5f + p.x * 3.0f);
        Color glow = vis.torchGlow;
        if (p.tex == DP_CRYSTAL) glow = Color{160,210,255,255};
        DrawCircleGradient((int)cx, (int)cy, 26 + pulse * 6,
                           Fade(glow, 0.30f), BLANK);
    }
    {
        // the party's own lantern
        float cx = D.px * TILE - camX;
        float cy = (D.py + 0.1f) * TILE - camY;
        DrawCircleGradient((int)cx, (int)cy, 66, Fade(vis.torchGlow, 0.13f), BLANK);
    }
    if (D.F.downTx >= 0) {
        float cx = (D.F.downTx + 0.5f) * TILE - camX;
        float cy = (D.F.downTy + 0.5f) * TILE - camY;
        DrawCircleGradient((int)cx, (int)cy, 26, Fade(Color{ 110, 150, 255, 255 }, 0.16f), BLANK);
    }
    EndBlendMode();

    // --- HUD — show cavern base name plus theme for at-a-glance variety
    char header[128];
    snprintf(header, sizeof(header), "%s — %s  B%d", D.name.c_str(), D.themeName.c_str(), D.floor + 1);
    Vector2 tsz = measureText(header, 8);
    float tw = std::max(170.0f, tsz.x + 24.0f);
    drawWindow({ floorf(g.vw * 0.5f - tw * 0.5f), 5.0f, tw, 21.0f });
    drawTextCentered(header, g.vw * 0.5f, 12.0f, Color{ 255, 236, 176, 255 }, 8);

    // Survey map: discovered passages persist while revisiting this cavern.
    const int mapX = g.vw - D.F.w * 2 - 11, mapY = 38;
    drawWindow({(float)mapX - 4, (float)mapY - 11, (float)D.F.w * 2 + 8, (float)D.F.h * 2 + 15});
    drawText("SURVEY", (float)mapX, (float)mapY - 7, Color{176, 192, 221, 255}, 8);
    auto found = D.explored.find(D.floor);
    if (found != D.explored.end()) {
        const auto& seen = found->second;
        auto known = [&](int x, int y) {
            return inBounds(x, y) && seen[dIdx(x, y, D.F.w)] != 0;
        };
        for (int y = 0; y < D.F.h; ++y)
            for (int x = 0; x < D.F.w; ++x)
                if (known(x, y)) DrawRectangle(mapX + x * 2, mapY + y * 2, 2, 2,
                    cellOpen(x, y) ? Color{97, 108, 133, 255} : Color{34, 40, 61, 255});
        auto marker = [&](int x, int y, Color color) {
            if (known(x, y)) DrawRectangle(mapX + x * 2, mapY + y * 2, 3, 3, color);
        };
        marker(D.F.entryTx, D.F.entryTy, Color{112, 220, 195, 255});
        marker(D.F.downTx, D.F.downTy, Color{125, 171, 255, 255});
        for (const auto& chest : D.F.chests)
            if (!g.openedChests.count(chest.id)) marker(chest.tx, chest.ty, Color{255, 211, 115, 255});
        marker((int)floorf(D.px), (int)floorf(D.py), WHITE);
    }

    if (g.mode == MODE_DUNGEON) {
        // Compact field vitals remain visible between encounters.
        int left = (g.vw - 338) / 2, top = g.vh - 59;
        drawWindow({(float)left, (float)top, 338, 32});
        for (int i = 0; i < 3; ++i) {
            const Hero& hero = g.party[i];
            float x = (float)left + 7 + i * 110;
            drawText(heroName(hero.klass), x, (float)top + 5,
                     hero.ko ? Color{255, 127, 127, 255} : WHITE, 8);
            if (hero.ko || hero.poisoned)
                drawText(hero.ko ? "KO" : "PSN", x + 76, (float)top + 5,
                         Color{224, 156, 218, 255}, 8);
            drawText("HP", x, (float)top + 18, Color{167, 203, 180, 255}, 8);
            drawBar(x + 20, (float)top + 18, 43, 5, (float)hero.hp / std::max(1, hero.maxHp),
                    Color{101, 214, 146, 255}, Color{22, 30, 45, 255});
            drawText("MP", x + 67, (float)top + 18, Color{157, 186, 223, 255}, 8);
            drawBar(x + 87, (float)top + 18, 14, 5, (float)hero.mp / std::max(1, hero.maxMp),
                    Color{111, 163, 246, 255}, Color{22, 30, 45, 255});
        }
        const char* hint = dungeonPrompt();
        if (!hint) hint = "ESC: menu";
        float hw = measureText(hint, 8).x + 20.0f;
        drawWindowBlue({ floorf(g.vw * 0.5f - hw * 0.5f), (float)g.vh - 22.0f, hw, 18.0f });
        drawTextCentered(hint, g.vw * 0.5f, (float)g.vh - 17.0f, Color{ 235, 242, 255, 245 }, 8);
        drawToast();
    }
}

// -- theme accessors for HUD and music
int dungeonTheme() { return D.active ? D.theme : -1; }
const char* dungeonThemeName() { return D.active ? D.themeName.c_str() : ""; }
const char* dungeonFullName() {
    static char buf[128];
    if (!D.active) return "";
    snprintf(buf, sizeof(buf), "%s — %s", D.name.c_str(), D.themeName.c_str());
    return buf;
}
MusicCue dungeonMusicCue() {
    if (!D.active) return MUS_FEN;
    return dunThemeMusic(D.theme);
}
