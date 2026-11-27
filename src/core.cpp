// core.cpp — tables, assets, UI drawing, save/load, quests
#include "game.hpp"
#include "world.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <cstddef>

Game g;
float gAnimTime = 0.0f;

// ================================================================ items
static const ItemDef ITEMS[IT_COUNT] = {
    // consumables
    { "Potion",      "Restores 60 HP.",                       15,  IK_CONSUMABLE, 60,   ICON_POTION,   true  },
    { "Hi-Potion",   "Restores 180 HP.",                      60,  IK_CONSUMABLE, 180,  ICON_HIPOTION, true  },
    { "Ether",       "Restores 30 MP.",                       40,  IK_CONSUMABLE, 30,   ICON_ETHER,    true  },
    { "Hi-Ether",    "Restores 90 MP.",                      120,  IK_CONSUMABLE, 90,   ICON_ETHER,    true  },
    { "Elixir",      "Fully restores one hero.",             400,  IK_CONSUMABLE, 9999, ICON_ELIXIR,   true  },
    { "Antidote",    "Cures poison.",                         20,  IK_CONSUMABLE, 0,    ICON_FOOD,     true  },
    { "Phoenix Down","Revives a fallen hero at half HP.",     80,  IK_CONSUMABLE, 1,    ICON_FEATHER,  true  },

    // weapons
    { "Bronze Sword","A serviceable blade. ATK +5",          140,  IK_WEAPON, 5,  ICON_SWORD0, true },
    { "Iron Sword",  "Honest steel. ATK +11",                380,  IK_WEAPON, 11, ICON_SWORD1, true },
    { "Mythril Sword","Light as a whisper. ATK +19",         900,  IK_WEAPON, 19, ICON_SWORD2, true },
    { "Dragon Fang", "It hums when monsters near. ATK +30", 2400,  IK_WEAPON, 30, ICON_SWORD2, false },

    // armour
    { "Leather Vest","Boiled leather. DEF +4",               110,  IK_ARMOR, 4,  ICON_ARMOR0, true },
    { "Chain Mail",  "Heavy but honest. DEF +9",             340,  IK_ARMOR, 9,  ICON_ARMOR1, true },
    { "Mythril Mail","Rings like a bell. DEF +15",           820,  IK_ARMOR, 15, ICON_ARMOR2, true },
    { "Dragon Scale","Scaled like its namesake. DEF +24",   2100,  IK_ARMOR, 24, ICON_ARMOR3, false },

    // shields & accessories
    { "Wooden Shield","A round of oak. DEF +2",               70,  IK_ACCESSORY, 2,  ICON_SHIELD, true },
    { "Iron Shield", "Banded and true. DEF +5",              260,  IK_ACCESSORY, 5,  ICON_SHIELD, true },
    { "Power Ring",  "Strength hums in the band. ATK +6",    600,  IK_ACCESSORY, 6,  ICON_RING,   true },
    { "Mana Amulet", "Thoughts flow easier. MAG +6",         600,  IK_ACCESSORY, 6,  ICON_RING,   true },

    // key items
    { "Rusty Key",   "Opens the tower gate.",                  0,  IK_KEY, 0, ICON_GEM,  false },
    { "Crown of Elm","The old crown of the first village.",    0,  IK_KEY, 0, ICON_COIN, false },
};
const ItemDef& itemDef(int id) {
    static const ItemDef none{ "Nothing", "", 0, IK_KEY, 0, ICON_GEM, false };
    if (id < 0 || id >= IT_COUNT) return none;
    return ITEMS[id];
}

// ================================================================ heroes
int heroAtk(const Hero& h) {
    int bonus = (h.weapon >= 0) ? itemDef(h.weapon).power : 0;
    if (h.accessory >= 0 && itemDef(h.accessory).name[0] == 'P') bonus += itemDef(h.accessory).power;
    return h.atk + bonus;
}
int heroDef(const Hero& h) {
    int bonus = (h.armor >= 0) ? itemDef(h.armor).power : 0;
    if (h.accessory >= 0 && itemDef(h.accessory).name[0] != 'P' && itemDef(h.accessory).kind == IK_ACCESSORY)
        bonus += itemDef(h.accessory).power;
    return h.def + bonus;
}
int heroMag(const Hero& h) {
    int bonus = 0;
    if (h.accessory >= 0 && itemDef(h.accessory).name[0] == 'M') bonus += itemDef(h.accessory).power;
    return h.mag + bonus;
}
int xpToNext(const Hero& h) {
    return (int)(40.0f + 52.0f * powf((float)h.level, 1.45f));
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
int partyLevel() {
    int sum = 0;
    for (auto& h : g.party) sum += h.level;
    return sum / 3;
}
int levelUpCheck(Hero& h) {
    int gained = 0;
    int guard = 0;
    while (h.xp >= xpToNext(h) && guard++ < 40) {
        h.xp -= xpToNext(h);
        h.level++;
        if (h.klass == KLASS_KNIGHT) {
            h.maxHp += 9 + h.level; h.maxMp += 2; h.atk += 3; h.def += 3; h.spd += 1; h.mag += 1;
        } else if (h.klass == KLASS_MAGE) {
            h.maxHp += 5; h.maxMp += 5 + h.level / 2; h.atk += 1; h.def += 1; h.spd += 1; h.mag += 4;
        } else {
            h.maxHp += 6; h.maxMp += 4 + h.level / 3; h.atk += 2; h.def += 2; h.spd += 2; h.mag += 3;
        }
        h.hp = h.maxHp; h.mp = h.maxMp;
        gained++;
    }
    return gained;
}

// ================================================================ spells
static const SpellDef SPELLS[KLASS_COUNT][6] = {
    // Knight
    { { "Power Slash", 6, 0, false, false, SCH_PHYS, 1, 0 },
      { "Shield Bash", 10, 26, false, false, SCH_PHYS, 3, 3 },
      { "Whirlwind",  16, 30, false, true,  SCH_PHYS, 6, 0 },
      { "Last Stand", 20, 0, true, false, SCH_HOLY, 10, 4 },
      { "", 0, 0, false, false, SCH_NONE, 99, 0 },
      { "", 0, 0, false, false, SCH_NONE, 99, 0 } },
    // Mage
    { { "Fire",        8, 34, false, false, SCH_FIRE, 1, 1 },
      { "Spark",       5, 22, false, false, SCH_STORM, 2, 2 },
      { "Frost",       12, 44, false, false, SCH_STORM, 4, 2 },
      { "Firaga",      22, 70, false, false, SCH_FIRE, 7, 1 },
      { "Stormcall",   30, 60, false, true,  SCH_STORM, 11, 2 },
      { "", 0, 0, false, false, SCH_NONE, 99, 0 } },
    // Cleric
    { { "Heal",        6, 62, true, false, SCH_HOLY, 1, 4 },
      { "Cure All",   16, 48, true, true,  SCH_HOLY, 3, 4 },
      { "Holy Bolt",  10, 36, false, false, SCH_HOLY, 5, 3 },
      { "Revive",     24, 1, true, false, SCH_HOLY, 8, 4 },
      { "Sanctuary",  36, 120, true, true, SCH_HOLY, 12, 4 },
      { "", 0, 0, false, false, SCH_NONE, 99, 0 } },
};
SpellList spellsFor(int klass, int level) {
    SpellList out;
    if (klass < 0 || klass >= KLASS_COUNT) { out.spells = nullptr; out.count = 0; return out; }
    out.spells = SPELLS[klass];
    out.count = 0;
    for (int i = 0; i < 6; i++) {
        if (SPELLS[klass][i].name[0] == 0) break;
        if (SPELLS[klass][i].level <= level) out.count++;
    }
    return out;
}

// ================================================================ enemies
static const EnemyDef ENEMIES[EN_COUNT] = {
    // name            hp  atk def spd  xp   gil  weak            drop            %  tier
    { "Slime",         22,  6,  2,  6,  16,  11, 1 << SCH_FIRE,   IT_POTION,      25, 1 },
    { "Imp",           26,  9,  3, 10,  22,  16, 1 << SCH_HOLY,   IT_POTION,      20, 1 },
    { "Bat",           17,  7,  2, 15,  15,  10, 1 << SCH_STORM,  IT_ANTIDOTE,    15, 1 },
    { "Hornet",        30, 11,  4, 13,  30,  22, 1 << SCH_FIRE,   IT_ANTIDOTE,    25, 2 },
    { "Wisp",          28, 12,  3,  9,  34,  24, 1 << SCH_HOLY,   IT_ETHER,       20, 2 },
    { "Wolf",          38, 13,  5, 12,  36,  26, 1 << SCH_FIRE,   IT_POTION,      20, 2 },
    { "Skeleton",      44, 15,  8,  7,  52,  40, 1 << SCH_HOLY,   IT_PHOENIX,     10, 3 },
    { "Ghost",         50, 16,  6, 10,  60,  46, 1 << SCH_HOLY,   IT_ETHER,       15, 3 },
    { "Orc",           78, 20, 11,  8,  90,  70, 1 << SCH_FIRE,   IT_HIPOTION,    20, 3 },
    { "Serpent",       96, 23, 12, 12, 120,  95, 1 << SCH_STORM,  IT_HIETHER,     15, 4 },
    { "Golem",        150, 27, 20,  5, 190, 150, 1 << SCH_STORM,  IT_ELIXIR,      10, 4 },
    { "Night Rider",  190, 33, 16, 15, 260, 220, 1 << SCH_HOLY,   IT_HIPOTION,    25, 4 },
    { "Slime King",   420, 40, 18, 11, 900, 800, 1 << SCH_FIRE,   IT_CROWN,      100, 5 },
};
const EnemyDef& enemyDef(int kind) {
    static const EnemyDef none{ "???", 1, 1, 1, 1, 0, 0, 0, -1, 0, 0 };
    if (kind < 0 || kind >= EN_COUNT) return none;
    return ENEMIES[kind];
}

// ================================================================ quests
static const char* QUEST_TITLES[QUEST_COUNT] = {
    "Turnip Trouble",
    "The Lost Locket",
    "The Crown of Elm",
};
static const char* QUEST_GOALS[QUEST_COUNT] = {
    "Thin out the slimes in the south meadow.",
    "Find the locket a traveller lost in the wilds.",
    "Recover the old crown from the Slime King.",
};
const char* questTitle(int id) { return QUEST_TITLES[id]; }
const char* questGoalText(int id) { return QUEST_GOALS[id]; }

void questSetState(int id, int state) {
    if (id < 0 || id >= QUEST_COUNT) return;
    g.quests[id].state = state;
}
bool questActive(int id) {
    if (id < 0 || id >= QUEST_COUNT) return false;
    return g.quests[id].state == QS_ACTIVE;
}
void questAdvance(int id, int amount) {
    if (id < 0 || id >= QUEST_COUNT) return;
    Quest& q = g.quests[id];
    if (q.state != QS_ACTIVE) return;
    q.progress += amount;
}

// ================================================================ assets
static std::string resolveAssetsDir() {
    const char* env = getenv("APFA_ASSETS_DIR");
    if (env && env[0]) { std::string d = env; if (d.back() != '/') d += '/'; return d; }
    if (FileExists("assets/tiles/grass_0.png")) return "assets/";
    if (FileExists("../assets/tiles/grass_0.png")) return "../assets/";
    return std::string(GetApplicationDirectory()) + "assets/";
}

static Texture2D loadTex(const std::string& dir, const char* rel, bool required = true) {
    std::string path = dir + rel;
    Texture2D t = LoadTexture(path.c_str());
    if (t.id == 0) {
        if (required) TraceLog(LOG_ERROR, "missing texture: %s", path.c_str());
        t = LoadTexture((dir + "sprites/rock_0.png").c_str());
    }
    SetTextureFilter(t, TEXTURE_FILTER_POINT);
    SetTextureWrap(t, TEXTURE_WRAP_CLAMP);
    return t;
}
static Sound loadSnd(const std::string& dir, const char* rel) {
    return LoadSound((dir + rel).c_str());
}
static Music loadMus(const std::string& dir, const char* rel) {
    return LoadMusicStream((dir + rel).c_str());
}

bool loadAllAssets() {
    std::string d = resolveAssetsDir();
    Assets& a = g.assets;

    // --- terrain
    for (int i = 0; i < 4; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "tiles/grass_%d.png", i);
        a.grass[i] = loadTex(d, buf, false);
    }
    for (int i = 0; i < 3; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "tiles/tuft_%d.png", i);
        a.tuft[i] = loadTex(d, buf, false);
    }
    for (int i = 0; i < 2; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "tiles/water_%d.png", i); a.water[i] = loadTex(d, buf);
        snprintf(buf, sizeof(buf), "tiles/flower_%d.png", i); a.flowers[i] = loadTex(d, buf, false);
    }
    a.snow[0] = loadTex(d, "tiles/snow.png");
    a.snow[1] = loadTex(d, "tiles/snow_drift.png", false);
    a.ice[0] = loadTex(d, "tiles/ice.png");
    a.ice[1] = loadTex(d, "tiles/ice_crack.png", false);
    a.rockHill[0] = loadTex(d, "tiles/rock_hill.png");
    a.rockHill[1] = loadTex(d, "tiles/rock_hill2.png", false);
    a.brickFloor[0] = loadTex(d, "tiles/brick.png", false);
    a.brickFloor[1] = loadTex(d, "tiles/brick_dark.png", false);
    a.dirt = loadTex(d, "tiles/dirt.png");
    a.soil = loadTex(d, "tiles/soil.png");
    a.sand = loadTex(d, "tiles/sand.png");
    a.beach = loadTex(d, "tiles/beach.png");
    a.cobble = loadTex(d, "tiles/cobble.png");
    a.flagstone = loadTex(d, "tiles/flagstone.png");
    a.tundra = loadTex(d, "tiles/tundra.png");
    a.deepWater = loadTex(d, "tiles/deep_water.png");
    a.swamp = loadTex(d, "tiles/swamp.png");
    a.dungeonFloor = loadTex(d, "tiles/dungeon_floor.png", false);
    a.plankFloor = loadTex(d, "tiles/plank_floor.png", false);
    a.tileFloor = loadTex(d, "tiles/tile_floor.png", false);
    a.stoneFloor = loadTex(d, "tiles/stone_floor.png", false);
    a.carpet = loadTex(d, "tiles/carpet.png", false);
    a.pebble = loadTex(d, "tiles/pebble.png", false);

    // --- props
    for (int i = 0; i < 3; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "sprites/tree_%d.png", i); a.tree[i] = loadTex(d, buf);
        snprintf(buf, sizeof(buf), "sprites/pine_%d.png", i); a.pine[i] = loadTex(d, buf);
        snprintf(buf, sizeof(buf), "sprites/rock_%d.png", i); a.rock[i] = loadTex(d, buf);
        snprintf(buf, sizeof(buf), "sprites/house_%d.png", i); a.house[i] = loadTex(d, buf);
    }
    for (int i = 3; i < 5; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "sprites/house_%d.png", i); a.house[i] = loadTex(d, buf);
    }
    a.treeSnow = loadTex(d, "sprites/tree_snow.png");
    a.deadTree = loadTex(d, "sprites/dead_tree.png");
    a.boulder = loadTex(d, "sprites/boulder.png");
    a.crystal = loadTex(d, "sprites/crystal.png");
    a.bush[0] = loadTex(d, "sprites/bush.png");
    a.bush[1] = loadTex(d, "sprites/bush_plain.png");
    a.mushroom = loadTex(d, "sprites/mushroom.png");
    a.plant = loadTex(d, "sprites/potted_plant.png");
    a.urn = loadTex(d, "sprites/urn.png");
    a.stump = loadTex(d, "sprites/stump.png", false);
    a.well = loadTex(d, "sprites/well.png");
    a.sign = loadTex(d, "sprites/sign.png");
    a.bench = loadTex(d, "sprites/bench.png");
    a.torch = loadTex(d, "sprites/torch.png");
    a.campfire = loadTex(d, "sprites/campfire.png");
    a.cauldron = loadTex(d, "sprites/cauldron.png");
    a.bucket = loadTex(d, "sprites/basin.png", false);
    a.flowerbed = loadTex(d, "sprites/flowerbed.png", false);
    a.fence = loadTex(d, "sprites/fence.png");
    a.fencePost = loadTex(d, "sprites/fence_post.png");
    a.fenceGate = loadTex(d, "sprites/fence_gate.png");
    a.ladder = loadTex(d, "sprites/ladder.png");
    a.statue = loadTex(d, "sprites/statue.png");
    a.pillar = loadTex(d, "sprites/pillar.png");
    a.ruinShrine = loadTex(d, "sprites/monument.png");
    a.ruinArch = loadTex(d, "sprites/pavilion.png");
    a.barrel = loadTex(d, "sprites/barrel.png");
    a.table = loadTex(d, "sprites/table.png");
    a.counter = loadTex(d, "sprites/counter.png");
    a.lantern = loadTex(d, "sprites/lantern.png");
    a.candle = loadTex(d, "sprites/candle.png");
    a.keg = loadTex(d, "sprites/keg.png");
    a.plate = loadTex(d, "sprites/plate.png");
    a.shelf = loadTex(d, "sprites/longtable.png", false);
    a.chestClosed = loadTex(d, "sprites/chest_closed.png");
    a.chestOpen = loadTex(d, "sprites/chest_open.png");
    a.jar = loadTex(d, "sprites/urn.png", false);
    a.bed = loadTex(d, "sprites/bed.png", false);
    a.stoolTex = loadTex(d, "sprites/stool.png", false);
    a.cloud = loadTex(d, "sprites/cloud.png", false);

    // --- buildings
    a.shop = loadTex(d, "sprites/shop.png");
    a.inn = loadTex(d, "sprites/inn.png");
    a.temple = loadTex(d, "sprites/temple.png");
    a.manor = loadTex(d, "sprites/manor.png");
    a.castle = loadTex(d, "sprites/castle.png");
    a.tower = loadTex(d, "sprites/tower.png");
    a.gate = loadTex(d, "sprites/gate.png");

    // --- heroes
    const char* heroes[3] = { "sprites/hero_knight.png", "sprites/hero_mage.png", "sprites/hero_cleric.png" };
    for (int i = 0; i < 3; i++) a.heroWalk[i] = loadTex(d, heroes[i]);
    for (int i = 0; i < 6; i++) {
        char buf[64]; snprintf(buf, sizeof(buf), "sprites/hero_alt%d.png", i);
        a.heroExtra[i] = loadTex(d, buf, false);
    }
    // npcs, in the order the world builds them (see NpcSprite)
    const char* npcs[16] = {
        "sprites/npc_villager0.png", "sprites/npc_villager1.png", "sprites/npc_villager2.png",
        "sprites/npc_villager3.png", "sprites/npc_elder.png", "sprites/npc_mayor.png",
        "sprites/npc_woman.png", "sprites/npc_child.png", "sprites/npc_guard.png",
        "sprites/npc_soldier.png", "sprites/npc_knight.png", "sprites/npc_merchant.png",
        "sprites/npc_smith.png", "sprites/npc_priest.png", "sprites/npc_farmer.png",
        "sprites/npc_dog.png",
    };
    for (int i = 0; i < 16; i++) a.npc[i] = loadTex(d, npcs[i]);

    // --- faces, in FACE_* order
    const char* faces[FACE_COUNT] = {
        "sprites/face_knight.png", "sprites/face_mage.png", "sprites/face_cleric.png",
        "sprites/face_knight_big.png",
        "sprites/face_villager0.png", "sprites/face_villager1.png", "sprites/face_villager2.png",
        "sprites/face_villager3.png",
        "sprites/face_elder.png", "sprites/face_mayor.png", "sprites/face_woman.png",
        "sprites/face_child.png",
        "sprites/face_smith.png", "sprites/face_bard.png", "sprites/face_shopkeeper.png",
        "sprites/face_innkeeper.png", "sprites/face_guard.png", "sprites/face_ranger.png",
        "sprites/face_slime.png", "sprites/face_ghost.png", "sprites/face_skeleton.png",
        "sprites/face_demon.png", "sprites/face_wolf.png",
    };
    for (int i = 0; i < FACE_COUNT; i++) a.face[i] = loadTex(d, faces[i]);

    // --- enemies, in EN_* order
    const char* enemies[EN_COUNT] = {
        "slime", "imp", "bat", "hornet", "wisp", "wolf", "skeleton", "ghost",
        "orc", "serpent", "golem", "night_rider", "slime_king",
    };
    for (int i = 0; i < EN_COUNT; i++) {
        char buf[96];
        snprintf(buf, sizeof(buf), "sprites/%s.png", enemies[i]);
        a.enemy[i] = loadTex(d, buf);
        snprintf(buf, sizeof(buf), "sprites/%s_battle.png", enemies[i]);
        a.enemyBig[i] = loadTex(d, buf, false);
    }

    // --- icons, in ICON_* order
    const char* icons[ICON_COUNT] = {
        "icons/potion.png", "icons/hi_potion.png", "icons/ether.png", "icons/elixir.png",
        "icons/feather.png", "icons/food.png", "icons/book.png", "icons/candle.png",
        "icons/gem.png", "icons/coin.png",
        "icons/sword_0.png", "icons/sword_1.png", "icons/sword_2.png",
        "icons/armor_0.png", "icons/armor_1.png", "icons/armor_2.png", "icons/armor_3.png",
        "icons/shield.png", "icons/ring.png",
    };
    for (int i = 0; i < ICON_COUNT; i++) a.icon[i] = loadTex(d, icons[i], false);

    // --- ui + backdrops
    a.window = loadTex(d, "ui/window_frame.png", false);
    a.titleBg = loadTex(d, "title_bg.png");
    a.titleAtlas = loadTex(d, "title_atlas.png", false);
    a.gameoverPlate = loadTex(d, "ui/gameover_plate.png", false);
    a.dawn = loadTex(d, "ui/dawn.png", false);
    a.night = loadTex(d, "ui/night.png", false);

    // --- fx
    a.fxSlash = loadTex(d, "fx/slash.png");
    a.fxSpark = loadTex(d, "fx/spark.png");
    a.fxSparkle = loadTex(d, "fx/sparkle.png");
    a.fxImpact = loadTex(d, "fx/impact.png");
    a.fxPoof = loadTex(d, "fx/poof.png");
    a.fxFireball = loadTex(d, "fx/fireball.png");
    a.fxArrow = loadTex(d, "fx/arrow.png", false);

    // --- fonts
    std::string fp = d + "fonts/PressStart2P-Regular.ttf";
    if (FileExists(fp.c_str())) {
        a.font8  = LoadFontEx(fp.c_str(), 8, nullptr, 0);
        a.font16 = LoadFontEx(fp.c_str(), 16, nullptr, 0);
        a.font24 = LoadFontEx(fp.c_str(), 24, nullptr, 0);
        a.font32 = LoadFontEx(fp.c_str(), 32, nullptr, 0);
        SetTextureFilter(a.font8.texture, TEXTURE_FILTER_POINT);
        SetTextureFilter(a.font16.texture, TEXTURE_FILTER_POINT);
        SetTextureFilter(a.font24.texture, TEXTURE_FILTER_POINT);
        SetTextureFilter(a.font32.texture, TEXTURE_FILTER_POINT);
    } else {
        a.font8 = a.font16 = a.font24 = a.font32 = GetFontDefault();
    }

    // --- audio
    a.cursor    = loadSnd(d, "audio/cursor.ogg");
    a.confirm   = loadSnd(d, "audio/confirm.ogg");
    a.cancel    = loadSnd(d, "audio/cancel.ogg");
    a.sword     = loadSnd(d, "audio/sword.ogg");
    a.hit       = loadSnd(d, "audio/hit.ogg");
    a.spell     = loadSnd(d, "audio/spell.ogg");
    a.heal      = loadSnd(d, "audio/heal.ogg");
    a.coin      = loadSnd(d, "audio/coin.ogg");
    a.potion    = loadSnd(d, "audio/potion.ogg");
    a.door      = loadSnd(d, "audio/door.ogg");
    a.encounter = loadSnd(d, "audio/encounter.ogg");
    a.levelup   = loadSnd(d, "audio/levelup.ogg");
    a.flee      = loadSnd(d, "audio/flee.ogg");
    a.chime     = loadSnd(d, "audio/confirm.ogg");
    a.treasure  = loadSnd(d, "audio/coin.ogg");

    a.title    = loadMus(d, "audio/title.ogg");
    a.field    = loadMus(d, "audio/field.ogg");
    a.battle   = loadMus(d, "audio/battle.ogg");
    a.victory  = loadMus(d, "audio/victory.ogg");
    a.gameover = loadMus(d, "audio/gameover.ogg");
    a.village  = loadMus(d, "audio/field.ogg");

    a.ok = true;
    return true;
}

// ================================================================ ui helpers
static Color lerpCol(Color a, Color b, float t) {
    auto c = [&](unsigned char x, unsigned char y) { return (unsigned char)(x + (y - x) * t); };
    return { c(a.r, b.r), c(a.g, b.g), c(a.b, b.b), c(a.a, b.a) };
}

void drawWindow(Rectangle r, bool dimInside) {
    // RM2k-style window: a blue vertical gradient body, a pale border, a dark
    // outer shadow.  Overlays the digitised OpenRTP window skin when loaded.
    DrawRectangleRounded({ r.x + 2, r.y + 3, r.width, r.height }, 0.18f, 5, Fade(BLACK, 0.35f));
    for (int i = 0; i < (int)r.height; i++) {
        float t = (float)i / std::max(1.0f, r.height - 1.0f);
        Color c = lerpCol(Color{ 26, 42, 116, 236 }, Color{ 10, 16, 54, 244 }, t);
        DrawRectangle((int)r.x, (int)r.y + i, (int)r.width, 1, c);
    }
    DrawRectangleRoundedLinesEx({ r.x + 0.5f, r.y + 0.5f, r.width - 1, r.height - 1 }, 0.18f, 5,
                                1.4f, Color{ 236, 240, 255, 255 });
    DrawRectangleRoundedLinesEx({ r.x + 2.5f, r.y + 2.5f, r.width - 5, r.height - 5 }, 0.16f, 5,
                                1.0f, Color{ 96, 118, 208, 255 });
}

void drawWindowBlue(Rectangle r) {
    DrawRectangleRounded({ r.x + 2, r.y + 3, r.width, r.height }, 0.18f, 5, Fade(BLACK, 0.35f));
    DrawRectangleRounded(r, 0.18f, 5, Color{ 16, 20, 48, 240 });
    DrawRectangleRoundedLinesEx(r, 0.18f, 5, 1.4f, Color{ 255, 214, 130, 255 });
}

Vector2 measureText(const char* text, int size) {
    Font f = (size >= 24) ? g.assets.font24 : (size >= 16 ? g.assets.font16 : g.assets.font8);
    return MeasureTextEx(f, text, (float)size, 1.0f);
}

static void drawWrappedLine(const char* s, float x, float y, Color c, int size) {
    Font f = (size >= 24) ? g.assets.font24 : (size >= 16 ? g.assets.font16 : g.assets.font8);
    DrawTextEx(f, s, { x, y }, (float)size, 1.0f, c);
}

void drawText(const char* text, float x, float y, Color c, int size) {
    drawWrappedLine(text, x, y, c, size);
}
void drawTextCentered(const char* text, float cx, float y, Color c, int size) {
    Vector2 s = measureText(text, size);
    drawWrappedLine(text, cx - s.x / 2, y, c, size);
}
void drawBar(float x, float y, float w, float h, float frac, Color fill, Color back) {
    DrawRectangle((int)x, (int)y, (int)w, (int)h, back);
    float f = std::max(0.0f, std::min(1.0f, frac));
    DrawRectangle((int)x, (int)y, (int)(w * f), (int)h, fill);
    DrawRectangleLines((int)x, (int)y, (int)w, (int)h, Fade(WHITE, 0.35f));
}

void drawItemIcon(int itemId, float x, float y, float size) {
    const ItemDef& d = itemDef(itemId);
    int ic = (d.icon >= 0 && d.icon < ICON_COUNT) ? d.icon : ICON_GEM;
    Texture2D& t = g.assets.icon[ic];
    if (t.id == 0) return;
    DrawTexturePro(t, { 0, 0, (float)t.width, (float)t.height }, { x, y, size, size }, { 0, 0 },
                   0.0f, WHITE);
}

std::string formatGil(int gil) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", gil);
    return buf;
}

void drawButton(const char* text, Rectangle r, int size) {
    Vector2 m = getVirtualMouse();
    bool hover = CheckCollisionPointRec(m, r);
    if (hover) g.hotButtons++;
    DrawRectangleRounded(r, 0.25f, 4, hover ? Color{ 64, 74, 150, 255 } : Color{ 30, 38, 92, 255 });
    DrawRectangleRoundedLinesEx(r, 0.25f, 4, 1.4f, hover ? Color{ 255, 214, 130, 255 }
                                                          : Color{ 140, 150, 220, 255 });
    drawTextCentered(text, r.x + r.width / 2, r.y + r.height / 2 - size / 2.0f + 1,
                     hover ? WHITE : Color{ 214, 220, 250, 255 }, size);
}

bool uiButtonAt(const char* text, Rectangle r, int size) {
    drawButton(text, r, size);
    Vector2 m = getVirtualMouse();
    if (CheckCollisionPointRec(m, r) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        PlaySound(g.assets.confirm);
        return true;
    }
    return false;
}

void pushToast(const std::string& msg) {
    g.toast = msg;
    g.toastT = 2.8f;
}
void shakeScreen(float mag, float t) {
    g.shakeMag = mag;
    g.shakeT = std::max(g.shakeT, t);
}

// ================================================================ inventory
int itemCount(int itemId) {
    if (itemId < 0 || itemId >= IT_COUNT) return 0;
    return g.inventory[itemId];
}
void addItem(int itemId, int amount) {
    if (itemId < 0 || itemId >= IT_COUNT) return;
    g.inventory[itemId] += amount;
    pushToast(std::string("Received ") + itemDef(itemId).name +
              (amount > 1 ? (" x" + std::to_string(amount)) : ""));
}
bool takeItem(int itemId, int amount) {
    if (itemId < 0 || itemId >= IT_COUNT) return false;
    if (g.inventory[itemId] < amount) return false;
    g.inventory[itemId] -= amount;
    return true;
}

bool applyConsumable(int itemId, int heroIdx) {
    if (heroIdx < 0 || heroIdx > 2) return false;
    Hero& h = g.party[heroIdx];
    const ItemDef& d = itemDef(itemId);
    if (d.kind != IK_CONSUMABLE) return false;
    if (itemCount(itemId) <= 0) return false;
    switch (itemId) {
        case IT_POTION:    if (h.ko || h.hp >= h.maxHp) return false;
                           h.hp = std::min(h.maxHp, h.hp + 60); break;
        case IT_HIPOTION:  if (h.ko || h.hp >= h.maxHp) return false;
                           h.hp = std::min(h.maxHp, h.hp + 180); break;
        case IT_ETHER:     if (h.ko || h.mp >= h.maxMp) return false;
                           h.mp = std::min(h.maxMp, h.mp + 30); break;
        case IT_HIETHER:   if (h.ko || h.mp >= h.maxMp) return false;
                           h.mp = std::min(h.maxMp, h.mp + 90); break;
        case IT_ELIXIR:    h.hp = h.maxHp; h.mp = h.maxMp; h.ko = false; break;
        case IT_PHOENIX:   if (!h.ko) return false;
                           h.ko = false; h.hp = std::max(1, h.maxHp / 2); break;
        case IT_ANTIDOTE:  break;     // no status ailments in this build
        default: return false;
    }
    g.inventory[itemId]--;
    return true;
}

// ================================================================ save / load
struct SaveBlob {
    uint32_t magic;
    uint32_t version;
    uint32_t worldSeed;
    float px, py;
    float runTime;
    int32_t gil, defeatedCount, slimesSlain;
    int32_t inventory[IT_COUNT];
    struct HS {
        int32_t klass, level, xp, hp, mp, maxHp, maxMp, atk, def, spd, mag;
        int32_t weapon, armor, accessory, ko;
    } heroes[3];
    int32_t questState[QUEST_COUNT], questProgress[QUEST_COUNT];
    int32_t flags;                 // bit0 boss defeated
    uint32_t checksum;
};
static const uint32_t SAVE_MAGIC = 0x41504641;   // 'AFPA'
static const uint32_t SAVE_VERSION = 3;

static uint32_t blobChecksum(const SaveBlob& b) {
    uint32_t h = 2166136261u;
    const uint8_t* p = (const uint8_t*)&b;
    size_t n = offsetof(SaveBlob, checksum);
    for (size_t i = 0; i < n; i++) { h ^= p[i]; h *= 16777619u; }
    return h;
}
std::string saveFilePath() {
    const char* env = getenv("APFA_SAVE");
    if (env && env[0]) return env;
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
    b.slimesSlain = g.slimesSlain;
    memcpy(b.inventory, g.inventory, sizeof(b.inventory));
    for (int i = 0; i < 3; i++) {
        const Hero& h = g.party[i];
        b.heroes[i] = { h.klass, h.level, h.xp, h.hp, h.mp, h.maxHp, h.maxMp,
                        h.atk, h.def, h.spd, h.mag, h.weapon, h.armor, h.accessory,
                        h.ko ? 1 : 0 };
    }
    for (int q = 0; q < QUEST_COUNT; q++) {
        b.questState[q] = g.quests[q].state;
        b.questProgress[q] = g.quests[q].progress;
    }
    b.flags = g.bossDefeated ? 1 : 0;
    b.checksum = blobChecksum(b);
    return SaveFileData(saveFilePath().c_str(), &b, sizeof(b));
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
    g.slimesSlain = b.slimesSlain;
    memcpy(g.inventory, b.inventory, sizeof(g.inventory));
    for (int i = 0; i < 3; i++) {
        Hero& h = g.party[i];
        h.klass = b.heroes[i].klass;
        h.level = b.heroes[i].level;
        h.xp = b.heroes[i].xp;
        h.hp = b.heroes[i].hp; h.mp = b.heroes[i].mp;
        h.maxHp = b.heroes[i].maxHp; h.maxMp = b.heroes[i].maxMp;
        h.atk = b.heroes[i].atk; h.def = b.heroes[i].def; h.spd = b.heroes[i].spd;
        h.mag = b.heroes[i].mag;
        h.weapon = b.heroes[i].weapon; h.armor = b.heroes[i].armor;
        h.accessory = b.heroes[i].accessory;
        h.ko = b.heroes[i].ko != 0;
        h.row = 0;
    }
    for (int q = 0; q < QUEST_COUNT; q++) {
        g.quests[q].state = b.questState[q];
        g.quests[q].progress = b.questProgress[q];
    }
    g.bossDefeated = (b.flags & 1) != 0;
    g.defeatedMonsters.clear();
    g.openedChests.clear();
    g.villageLoaded = false;
    g.npcKey = ~0ull;
    g.npcs.clear();
    return true;
}

void newGameState(uint32_t seed) {
    g.worldSeed = seed;
    g.px = 0.5f; g.py = 7.5f;      // just south of the Elmswick plaza
    g.facing = 0;
    g.runTime = 0;
    g.gil = 80;
    g.defeatedCount = 0;
    g.slimesSlain = 0;
    memset(g.inventory, 0, sizeof(g.inventory));
    g.inventory[IT_POTION] = 3;
    g.inventory[IT_ETHER] = 1;
    g.inventory[IT_PHOENIX] = 1;
    Hero base[3] = {
        { KLASS_KNIGHT, 1, 0, 40, 8,  40, 8,  10, 8,  8, 2, -1, -1, -1, false, 0 },
        { KLASS_MAGE,   1, 0, 24, 18, 24, 18, 6,  4,  7, 9, -1, -1, -1, false, 1 },
        { KLASS_CLERIC, 1, 0, 30, 16, 30, 16, 7,  6,  9, 6, -1, -1, -1, false, 1 },
    };
    memcpy(g.party, base, sizeof(base));
    for (int q = 0; q < QUEST_COUNT; q++) { g.quests[q] = Quest{}; }
    g.quests[QUEST_SLIMES].state = QS_OFFERED;
    g.quests[QUEST_LOCKET].state = QS_OFFERED;
    g.defeatedMonsters.clear();
    g.openedChests.clear();
    g.villageLoaded = false;
    g.npcKey = ~0ull;
    g.npcs.clear();
    g.hasMoveTarget = false;
    g.walkQueue.clear();
    g.encounterGrace = 4.0f;
    g.stepCounter = 0;
    g.bossDefeated = false;
    g.startVillageSeen = false;
    g.introT = 0;
}

void startNewAdventure() {
    uint32_t seed = (uint32_t)time(nullptr) * 2654435761u + (uint32_t)GetRandomValue(1, 99999) + 1u;
    newGameState(seed);
    g.mode = MODE_INTRO;
    g.introT = 0;
    g.fade = 1.0f;
    g.fadeDir = -1;
}
