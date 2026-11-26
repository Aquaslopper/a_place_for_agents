// core.cpp — items, enemies, heroes, assets, save/load, UI helpers
#include "game.hpp"
#include "world.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <cstddef>

Game g;

// ------------------------------------------------------------ tables
static const ItemDef ITEMS[IT_COUNT] = {
    { "Potion",        12,  IK_CONSUMABLE, 60,  "Restore 60 HP" },
    { "Ether",         16,  IK_CONSUMABLE, 40,  "Restore 40 MP" },
    { "Hi-Potion",     45,  IK_CONSUMABLE, 160, "Restore 160 HP" },
    { "Elixir",        150, IK_CONSUMABLE, 9999,"Fully restore one hero" },
    { "Bronze Sword",  120, IK_WEAPON, 4,  "ATK +4" },
    { "Iron Sword",    320, IK_WEAPON, 9,  "ATK +9" },
    { "Mythril Sword", 700, IK_WEAPON, 15, "ATK +15" },
    { "Leather Armor", 90,  IK_ARMOR,  3,  "DEF +3" },
    { "Chain Mail",    260, IK_ARMOR,  6,  "DEF +6" },
    { "Mythril Mail",  520, IK_ARMOR,  10, "DEF +10" },
};
const ItemDef& itemDef(int id) { return ITEMS[id]; }

static const EnemyDef ENEMIES[EN_COUNT] = {
    { "Slime", 22, 6,  2, 6,  18, 12 },
    { "Wolf",  30, 9,  3, 11, 26, 18 },
    { "Bat",   16, 7,  2, 14, 15, 10 },
    { "Wisp",  26, 11, 2, 9,  32, 22 },
    { "Golem", 68, 13, 8, 4,  70, 60 },
};
const EnemyDef& enemyDef(int kind) { return ENEMIES[kind]; }

int heroAtk(const Hero& h) {
    int bonus = (h.weapon >= 0) ? ITEMS[h.weapon].power : 0;
    return h.atk + bonus;
}
int heroDef(const Hero& h) {
    int bonus = (h.armor >= 0) ? ITEMS[h.armor].power : 0;
    return h.def + bonus;
}
int heroLevel(const Hero& h) { return h.level; }
int xpToNext(const Hero& h) {
    return (int)(30.0f + 45.0f * powf((float)h.level, 1.5f));
}
const char* klassName(int klass) {
    switch (klass) {
        case KLASS_KNIGHT: return "Knight";
        case KLASS_MAGE:   return "Mage";
        case KLASS_CLERIC: return "Cleric";
        default:           return "Hero";
    }
}
const char* heroName(int klass) {
    switch (klass) {
        case KLASS_KNIGHT: return "Aldric";
        case KLASS_MAGE:   return "Iris";
        case KLASS_CLERIC: return "Pip";
        default:           return "Hero";
    }
}

// ------------------------------------------------------------ UI helpers
static Color lerpColor(Color a, Color b, float t) {
    return { (unsigned char)(a.r + (b.r - a.r) * t), (unsigned char)(a.g + (b.g - a.g) * t),
             (unsigned char)(a.b + (b.b - a.b) * t), (unsigned char)(a.a + (b.a - a.a) * t) };
}
Color lerpColorPub(Color a, Color b, float t) { return lerpColor(a, b, t); }

void drawDuskGradient(int w, int h, Color top, Color bottom) {
    for (int y = 0; y < h; y++) {
        Color c = lerpColor(top, bottom, (float)y / (float)(h - 1));
        DrawRectangle(0, y, w, 1, c);
    }
}

void drawPanel(Rectangle r) {
    // semi-opaque indigo body with double white border — the classic JRPG window
    DrawRectangleRounded(r, 0.12f, 6, Color{ 16, 20, 56, 232 });
    DrawRectangleRoundedLinesEx({ r.x - 1, r.y - 1, r.width + 2, r.height + 2 }, 0.12f, 6, 2.0f, Color{ 238, 240, 255, 255 });
    DrawRectangleRoundedLinesEx({ r.x + 1.5f, r.y + 1.5f, r.width - 3, r.height - 3 }, 0.10f, 6, 1.0f, Color{ 88, 104, 198, 255 });
}

bool uiButton(const char* text, Rectangle r, int fontSize) {
    Vector2 m = getVirtualMouse();
    // convert to virtual coords assuming RT is drawn fullscreen-letterboxed: handled by caller transform — we use virtual directly
    bool hover = CheckCollisionPointRec(m, r);
    Color fill = hover ? Color{ 56, 64, 140, 255 } : Color{ 36, 40, 96, 255 };
    DrawRectangleRounded(r, 0.25f, 4, fill);
    DrawRectangleRoundedLinesEx(r, 0.25f, 4, 1.5f, hover ? WHITE : Color{ 150, 158, 230, 255 });
    int fs = fontSize;
    Font f = (fs >= 16) ? g.assets.font16 : g.assets.font8;
    Vector2 ts = MeasureTextEx(f, text, (float)fs, 1.0f);
    Color tc = hover ? WHITE : Color{ 212, 218, 255, 255 };
    if (hover) tc = WHITE;
    DrawTextEx(f, text, { r.x + r.width / 2 - ts.x / 2, r.y + r.height / 2 - ts.y / 2 }, (float)fs, 1.0f, tc);
    if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        PlaySound(g.assets.confirm);
        return true;
    }
    return false;
}

void pushToast(const std::string& msg) {
    g.toast = msg;
    g.toastT = 2.6f;
}

std::string formatGil(int gil) {
    char buf[64];
    if (gil >= 10000) snprintf(buf, sizeof(buf), "%dg", gil);
    else snprintf(buf, sizeof(buf), "%d G", gil);
    return buf;
}

// ------------------------------------------------------------ asset loading
static std::string resolveAssetsDir() {
    const char* env = getenv("AGENTS_ASSETS_DIR");
    if (env && env[0]) return std::string(env);
    // assets/ next to the working directory, else next to the executable
    if (FileExists("assets/title_bg.png")) return "assets/";
    std::string exe = GetApplicationDirectory();
    return exe + "assets/";
}

static Texture2D loadTex(const std::string& dir, const char* rel) {
    std::string path = dir + rel;
    Texture2D t = LoadTexture(path.c_str());
    if (t.id == 0) TraceLog(LOG_ERROR, "failed to load texture: %s", path.c_str());
    SetTextureFilter(t, TEXTURE_FILTER_POINT);
    SetTextureWrap(t, TEXTURE_WRAP_CLAMP);
    return t;
}
static Sound loadSnd(const std::string& dir, const char* rel) {
    std::string path = dir + rel;
    return LoadSound(path.c_str());
}
static Music loadMus(const std::string& dir, const char* rel) {
    std::string path = dir + rel;
    return LoadMusicStream(path.c_str());
}

static Texture2D makeCloudTexture() {
    Image img = GenImageColor(96, 40, BLANK);
    ImageDrawCircle(&img, 22, 26, 14, Color{ 255, 255, 255, 220 });
    ImageDrawCircle(&img, 42, 18, 17, Color{ 255, 255, 255, 230 });
    ImageDrawCircle(&img, 62, 24, 15, Color{ 255, 255, 255, 220 });
    ImageDrawCircle(&img, 76, 28, 10, Color{ 255, 255, 255, 200 });
    ImageDrawRectangle(&img, 12, 26, 72, 12, Color{ 255, 255, 255, 200 });
    Texture2D t = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(t, TEXTURE_FILTER_BILINEAR);
    return t;
}

bool loadAllAssets() {
    std::string dir = resolveAssetsDir();
    Assets& a = g.assets;
    for (int i = 0; i < 3; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "tiles/grass_%d.png", i);  a.grass[i] = loadTex(dir, buf);
        snprintf(buf, sizeof(buf), "tiles/tuft_%d.png", i);   a.tuft[i] = loadTex(dir, buf);
        snprintf(buf, sizeof(buf), "tiles/flower_%d.png", i); a.flower[i] = loadTex(dir, buf);
        snprintf(buf, sizeof(buf), "sprites/tree_%d.png", i); a.tree[i] = loadTex(dir, buf);
        snprintf(buf, sizeof(buf), "sprites/pine_%d.png", i); a.pine[i] = loadTex(dir, buf);
        snprintf(buf, sizeof(buf), "sprites/rock_%d.png", i); a.rock[i] = loadTex(dir, buf);
        snprintf(buf, sizeof(buf), "sprites/house_%d.png", i);a.house[i] = loadTex(dir, buf);
        snprintf(buf, sizeof(buf), "icons/sword_%d.png", i);  a.icon[IT_SWORD_BRONZE + i] = loadTex(dir, buf);
        snprintf(buf, sizeof(buf), "icons/armor_%d.png", i);  a.icon[IT_ARMOR_LEATHER + i] = loadTex(dir, buf);
    }
    a.sand = loadTex(dir, "tiles/sand.png");
    a.dirt = loadTex(dir, "tiles/dirt.png");
    a.snow = loadTex(dir, "tiles/snow.png");
    a.water[0] = loadTex(dir, "tiles/water_0.png");
    a.water[1] = loadTex(dir, "tiles/water_1.png");
    a.rockHill = loadTex(dir, "tiles/rock_hill.png");
    a.ice = loadTex(dir, "tiles/ice.png");
    a.cobble = loadTex(dir, "tiles/cobble.png");
    a.deadTree = loadTex(dir, "sprites/dead_tree.png");
    a.bush = loadTex(dir, "sprites/bush.png");
    a.well = loadTex(dir, "sprites/well.png");
    a.sign = loadTex(dir, "sprites/sign.png");
    a.fence = loadTex(dir, "sprites/fence.png");
    a.mushroom = loadTex(dir, "sprites/mushroom.png");
    a.shop = loadTex(dir, "sprites/shop.png");
    a.inn = loadTex(dir, "sprites/inn.png");
    a.castle = loadTex(dir, "sprites/castle.png");
    a.heroWalk[KLASS_KNIGHT] = loadTex(dir, "sprites/hero_knight.png");
    a.heroWalk[KLASS_MAGE]   = loadTex(dir, "sprites/hero_mage.png");
    a.heroWalk[KLASS_CLERIC] = loadTex(dir, "sprites/hero_cleric.png");
    a.face[KLASS_KNIGHT] = loadTex(dir, "sprites/face_knight.png");
    a.face[KLASS_MAGE]   = loadTex(dir, "sprites/face_mage.png");
    a.face[KLASS_CLERIC] = loadTex(dir, "sprites/face_cleric.png");
    for (int i = 0; i < EN_COUNT; i++) {
        const char* files[EN_COUNT] = { "sprites/slime.png", "sprites/wolf.png", "sprites/bat.png", "sprites/wisp.png", "sprites/golem.png" };
        a.enemy[i] = loadTex(dir, files[i]);
    }
    a.icon[IT_POTION]   = loadTex(dir, "icons/potion.png");
    a.icon[IT_ETHER]    = loadTex(dir, "icons/ether.png");
    a.icon[IT_HIPOTION] = loadTex(dir, "icons/hi_potion.png");
    a.icon[IT_ELIXIR]   = loadTex(dir, "icons/elixir.png");
    a.coinIcon          = loadTex(dir, "icons/coin.png");
    a.fxSlash   = loadTex(dir, "fx/slash.png");
    a.fxImpact  = loadTex(dir, "fx/impact.png");
    a.fxFireball= loadTex(dir, "fx/fireball.png");
    a.fxSpark   = loadTex(dir, "fx/spark.png");
    a.fxSparkle = loadTex(dir, "fx/sparkle.png");
    a.fxPoof    = loadTex(dir, "fx/poof.png");
    a.titleBg   = loadTex(dir, "title_bg.png");
    a.cloud     = makeCloudTexture();

    // fonts
    std::string fontPath = dir + "fonts/PressStart2P-Regular.ttf";
    if (FileExists(fontPath.c_str())) {
        a.font8  = LoadFontEx(fontPath.c_str(), 8,  nullptr, 0);
        a.font16 = LoadFontEx(fontPath.c_str(), 16, nullptr, 0);
        a.font32 = LoadFontEx(fontPath.c_str(), 32, nullptr, 0);
        SetTextureFilter(a.font8.texture, TEXTURE_FILTER_POINT);
        SetTextureFilter(a.font16.texture, TEXTURE_FILTER_POINT);
        SetTextureFilter(a.font32.texture, TEXTURE_FILTER_POINT);
    } else {
        a.font8 = GetFontDefault(); a.font16 = GetFontDefault(); a.font32 = GetFontDefault();
    }

    a.cursor    = loadSnd(dir, "audio/cursor.ogg");
    a.confirm   = loadSnd(dir, "audio/confirm.ogg");
    a.cancel    = loadSnd(dir, "audio/cancel.ogg");
    a.sword     = loadSnd(dir, "audio/sword.ogg");
    a.hit       = loadSnd(dir, "audio/hit.ogg");
    a.spell     = loadSnd(dir, "audio/spell.ogg");
    a.heal      = loadSnd(dir, "audio/heal.ogg");
    a.coin      = loadSnd(dir, "audio/coin.ogg");
    a.potion    = loadSnd(dir, "audio/potion.ogg");
    a.door      = loadSnd(dir, "audio/door.ogg");
    a.encounter = loadSnd(dir, "audio/encounter.ogg");
    a.levelup   = loadSnd(dir, "audio/levelup.ogg");
    a.flee      = loadSnd(dir, "audio/flee.ogg");

    a.title    = loadMus(dir, "audio/title.ogg");
    a.field    = loadMus(dir, "audio/field.ogg");
    a.battle   = loadMus(dir, "audio/battle.ogg");
    a.victory  = loadMus(dir, "audio/victory.ogg");
    a.gameover = loadMus(dir, "audio/gameover.ogg");

    if (a.title.frameCount > 0)   { SetMusicVolume(a.title, 0.55f); }
    if (a.field.frameCount > 0)   { SetMusicVolume(a.field, 0.5f); }
    if (a.battle.frameCount > 0)  { SetMusicVolume(a.battle, 0.55f); }
    if (a.victory.frameCount > 0) { SetMusicVolume(a.victory, 0.7f); }
    if (a.gameover.frameCount > 0){ SetMusicVolume(a.gameover, 0.7f); }

    a.ok = true;
    return true;
}

void unloadAllAssets() {
    // process-lifetime cache; raylib cleans up at exit anyway
}


bool applyConsumable(int itemId, int heroIdx) {
    if (heroIdx < 0 || heroIdx > 2) return false;
    Hero& h = g.party[heroIdx];
    const ItemDef& d = itemDef(itemId);
    if (d.kind != IK_CONSUMABLE) return false;
    if (g.inventory[itemId] <= 0) return false;
    if (h.ko && itemId != IT_ELIXIR) return false;   // only elixir revives
    switch (itemId) {
        case IT_POTION:   h.hp = std::min(h.maxHp, h.hp + 60); break;
        case IT_HIPOTION: h.hp = std::min(h.maxHp, h.hp + 160); break;
        case IT_ETHER:    h.mp = std::min(h.maxMp, h.mp + 40); break;
        case IT_ELIXIR:   h.hp = h.maxHp; h.mp = h.maxMp; h.ko = false; break;
        default: return false;
    }
    g.inventory[itemId]--;
    if (itemId == IT_ELIXIR) h.ko = false;
    return true;
}

// ------------------------------------------------------------ save / load
struct SaveBlob {
    uint32_t magic;        // 'APFA'
    uint32_t version;      // 1
    uint32_t worldSeed;
    float px, py;
    uint32_t runTime;
    int32_t  gil;
    int32_t  defeatedCount;
    int32_t  inventory[IT_COUNT];
    struct {
        int32_t klass, level, xp, hp, mp, maxHp, maxMp, atk, def, spd, weapon, armor, ko;
    } heroes[3];
    uint32_t checksum;
};
static const uint32_t SAVE_MAGIC = 0x41465041; // 'APFA'
static const uint32_t SAVE_VERSION = 2;

static uint32_t blobChecksum(const SaveBlob& b) {
    uint32_t h = 2166136261u;
    const uint8_t* p = (const uint8_t*)&b;
    size_t n = offsetof(SaveBlob, checksum);
    for (size_t i = 0; i < n; i++) { h ^= p[i]; h *= 16777619u; }
    return h;
}
std::string saveFilePath() {
    return std::string(GetApplicationDirectory()) + "aplaceforagents_save.sav";
}
bool saveExists() { return FileExists(saveFilePath().c_str()); }

bool saveGame() {
    SaveBlob b{};
    b.magic = SAVE_MAGIC; b.version = SAVE_VERSION;
    b.worldSeed = g.worldSeed;
    b.px = g.px; b.py = g.py;
    b.runTime = g.runTime;
    b.gil = g.gil;
    b.defeatedCount = g.defeatedCount;
    memcpy(b.inventory, g.inventory, sizeof(b.inventory));
    for (int i = 0; i < 3; i++) {
        b.heroes[i].klass = g.party[i].klass;
        b.heroes[i].level = g.party[i].level;
        b.heroes[i].xp = g.party[i].xp;
        b.heroes[i].hp = g.party[i].hp; b.heroes[i].mp = g.party[i].mp;
        b.heroes[i].maxHp = g.party[i].maxHp; b.heroes[i].maxMp = g.party[i].maxMp;
        b.heroes[i].atk = g.party[i].atk; b.heroes[i].def = g.party[i].def; b.heroes[i].spd = g.party[i].spd;
        b.heroes[i].weapon = g.party[i].weapon; b.heroes[i].armor = g.party[i].armor;
        b.heroes[i].ko = g.party[i].ko ? 1 : 0;
    }
    b.checksum = blobChecksum(b);
    bool ok = SaveFileData(saveFilePath().c_str(), &b, sizeof(b));
    return ok;
}

bool loadGame() {
    int size = 0;
    unsigned char* data = LoadFileData(saveFilePath().c_str(), &size);
    if (!data) return false;
    if (size != (int)sizeof(SaveBlob)) { UnloadFileData(data); return false; }
    SaveBlob b;
    memcpy(&b, data, sizeof(b));
    UnloadFileData(data);
    if (b.magic != SAVE_MAGIC || blobChecksum(b) != b.checksum) return false;
    g.worldSeed = b.worldSeed;
    g.px = b.px; g.py = b.py;
    g.runTime = b.runTime;
    g.gil = b.gil;
    g.defeatedCount = b.defeatedCount;
    memcpy(g.inventory, b.inventory, sizeof(g.inventory));
    for (int i = 0; i < 3; i++) {
        g.party[i].klass = b.heroes[i].klass;
        g.party[i].level = b.heroes[i].level;
        g.party[i].xp = b.heroes[i].xp;
        g.party[i].hp = b.heroes[i].hp; g.party[i].mp = b.heroes[i].mp;
        g.party[i].maxHp = b.heroes[i].maxHp; g.party[i].maxMp = b.heroes[i].maxMp;
        g.party[i].atk = b.heroes[i].atk; g.party[i].def = b.heroes[i].def; g.party[i].spd = b.heroes[i].spd;
        g.party[i].weapon = b.heroes[i].weapon; g.party[i].armor = b.heroes[i].armor;
        g.party[i].ko = b.heroes[i].ko != 0;
    }
    g.defeatedMonsters.clear();
    g.villageLoaded = false;
    return true;
}

void newGameState(uint32_t seed) {
    g.worldSeed = seed;
    g.px = 0; g.py = 2;
    g.runTime = 0;
    g.gil = 60;
    g.defeatedCount = 0;
    memset(g.inventory, 0, sizeof(g.inventory));
    g.inventory[IT_POTION] = 3;
    g.inventory[IT_ETHER] = 1;
    Hero base[3] = {
        { KLASS_KNIGHT, 1, 0, 34, 6,  34, 6,  9, 7, 8, -1, -1, false },
        { KLASS_MAGE,   1, 0, 22, 14, 22, 14, 6, 3, 7, -1, -1, false },
        { KLASS_CLERIC, 1, 0, 26, 12, 26, 12, 7, 5, 9, -1, -1, false },
    };
    memcpy(g.party, base, sizeof(base));
    g.defeatedMonsters.clear();
    g.villageLoaded = false;
    g.hasMoveTarget = false;
    g.encounterGrace = 2.0f;
    g.stepCounter = 0;
}

int levelUpCheck(Hero& h) {
    int gained = 0;
    while (h.xp >= xpToNext(h)) {
        h.xp -= xpToNext(h);
        h.level++;
        h.maxHp += 8 + (int)hashf(h.level, h.klass, 0x1e9) * 6;
        h.maxMp += 3 + (int)hashf(h.level, h.klass, 0x2e9) * 4;
        h.atk += 2; h.def += (h.klass == KLASS_KNIGHT ? 3 : 2); h.spd += 1;
        h.hp = h.maxHp; h.mp = h.maxMp;
        gained++;
    }
    return gained;
}
