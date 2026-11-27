// core.cpp — tables, assets, UI drawing, save/load, quests
#include "game.hpp"
#include "world.hpp"
#include "atlas.hpp"
#include "regions.hpp"
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <cstddef>

Game g;
float gAnimTime = 0.0f;

// ================================================================ items
static const ItemDef ITEMS[IT_COUNT] = {
    // consumables
    { "Potion",       "Restores 60 HP.",                       15,  IK_CONSUMABLE, 60,   ICON_POTION,   true,  KM_ANY },
    { "Hi-Potion",    "Restores 180 HP.",                      60,  IK_CONSUMABLE, 180,  ICON_HIPOTION, true,  KM_ANY },
    { "Mega Potion",  "Restores 400 HP.",                      180, IK_CONSUMABLE, 400,  ICON_HIPOTION, true,  KM_ANY },
    { "Ether",        "Restores 30 MP.",                       40,  IK_CONSUMABLE, 30,   ICON_ETHER,    true,  KM_ANY },
    { "Hi-Ether",     "Restores 90 MP.",                       120, IK_CONSUMABLE, 90,   ICON_ETHER,    true,  KM_ANY },
    { "Elixir",       "Fully restores HP, MP, and cures all.",400,  IK_CONSUMABLE, 9999, ICON_ELIXIR,   true,  KM_ANY },
    { "Antidote",     "Cures poison (or restores 20 HP).",     20,  IK_CONSUMABLE, 20,   ICON_FOOD,     true,  KM_ANY },
    { "Remedy",       "Cures poison and revives 30 HP.",       70,  IK_CONSUMABLE, 30,   ICON_REMEDY,   true,  KM_ANY },
    { "Phoenix Down", "Revives a fallen hero at half HP.",     80,  IK_CONSUMABLE, 1,    ICON_FEATHER,  true,  KM_ANY },
    { "Smoke Bomb",   "Escape any battle unscathed.",          45,  IK_CONSUMABLE, 0,    ICON_BOMB,     true,  KM_ANY },

    // weapons — knightly steel
    { "Bronze Sword", "A serviceable blade. ATK +5",          140,  IK_WEAPON,  5, ICON_SWORD0, true,  KM_ANY },
    { "Iron Sword",   "Honest steel. ATK +11",                380,  IK_WEAPON, 11, ICON_SWORD1, true,  KM_ANY },
    { "Steel Sword",  "Tempered in Elmswick's forge. ATK +16",620,  IK_WEAPON, 16, ICON_SWORD1, true,  KM_KNIGHT },
    { "Mythril Sword","Light as a whisper. ATK +19",          900,  IK_WEAPON, 19, ICON_SWORD2, true,  KM_ANY },
    { "Dragon Fang",  "It hums when monsters near. ATK +30", 2400,  IK_WEAPON, 30, ICON_SWORD2, false, KM_ANY },
    { "War Axe",      "Heavy iron that bites shields. ATK +22",1100,IK_WEAPON, 22, ICON_AXE,    true,  KM_KNIGHT },
    { "Halberd",      "Reach and ruin. ATK +26",              1600,  IK_WEAPON, 26, ICON_SPEAR,  true,  KM_KNIGHT },

    // weapons — arcane
    { "Oak Staff",    "Knotted heartwood. ATK +6, MAG +3",    160,  IK_WEAPON,  6, ICON_STAFF,  true,  KM_MAGE },
    { "Iron Rod",     "Cold to the touch. ATK +9, MAG +7",    460,  IK_WEAPON,  9, ICON_STAFF,  true,  KM_MAGE },
    { "Sage Staff",   "Whispers back. ATK +14, MAG +15",     1250,  IK_WEAPON, 14, ICON_STAFF,  true,  KM_MAGE },

    // weapons — devotional
    { "Brass Mace",   "Blessed weight. ATK +8, MAG +2",       240,  IK_WEAPON,  8, ICON_MACE,   true,  KM_CLERIC },
    { "Holy Mace",    "Strikes true against the dark. ATK +17, MAG +9", 1400, IK_WEAPON, 17, ICON_MACE, true, KM_CLERIC },

    // weapons — light & ranged
    { "Dagger",       "Quick and quiet. ATK +7",              120,  IK_WEAPON,  7, ICON_SWORD0, true,  KM_ANY },
    { "Hunter Bow",   "Pulled from a yew stave. ATK +13",     540,  IK_WEAPON, 13, ICON_BOW,    true,  KM_ANY },

    // armour
    { "Leather Vest", "Boiled leather. DEF +4",               110,  IK_ARMOR,  4, ICON_ARMOR0, true,  KM_ANY },
    { "Chain Mail",   "Heavy but honest. DEF +9",             340,  IK_ARMOR,  9, ICON_ARMOR1, true,  KM_ANY },
    { "Plate Mail",   "A walking wall. DEF +13",              700,  IK_ARMOR, 13, ICON_ARMOR1, true,  KM_KNIGHT },
    { "Mythril Mail", "Rings like a bell. DEF +15",           820,  IK_ARMOR, 15, ICON_ARMOR2, true,  KM_ANY },
    { "Dragon Scale", "Scaled like its namesake. DEF +24",   2100,  IK_ARMOR, 24, ICON_ARMOR3, false, KM_ANY },
    { "Silk Robe",    "Spellwoven silk. DEF +6, MAG +4",      480,  IK_ARMOR,  6, ICON_ROBE,   true,  KM_MAGE | KM_CLERIC },
    { "Archmage Robe","Threads of old power. DEF +14, MAG +10",1750,IK_ARMOR, 14, ICON_ROBE,   true,  KM_MAGE | KM_CLERIC },

    // shields & accessories
    { "Wooden Shield","A round of oak. DEF +2",                70,  IK_ACCESSORY, 2, ICON_SHIELD, true, KM_ANY },
    { "Iron Shield",  "Banded and true. DEF +5",              260,  IK_ACCESSORY, 5, ICON_SHIELD, true, KM_ANY },
    { "Kite Shield",  "Heraldic and hard. DEF +8",            640,  IK_ACCESSORY, 8, ICON_SHIELD, true, KM_KNIGHT },
    { "Power Ring",   "Strength hums in the band. ATK +6",    600,  IK_ACCESSORY, 6, ICON_RING,   true, KM_ANY },
    { "Mana Amulet",  "Thoughts flow easier. MAG +6",         600,  IK_ACCESSORY, 6, ICON_RING,   true, KM_ANY },
    { "War Ring",     "A soldier's oath. ATK +10",           1300,  IK_ACCESSORY,10, ICON_RING,   true, KM_ANY },
    { "Sage Ring",    "A scholar's focus. MAG +10",          1300,  IK_ACCESSORY,10, ICON_RING,   true, KM_ANY },
    { "Guard Amulet", "Steady as stone. DEF +8",              900,  IK_ACCESSORY, 8, ICON_RING,   true, KM_ANY },
    { "Swift Boots",  "The road blurs. SPD +5",              1100,  IK_ACCESSORY, 5, ICON_BOOTS,  true, KM_ANY },

    // key items
    { "Rusty Key",    "Opens the old western watchtower chest.",0,  IK_KEY, 0, ICON_KEY,  false, KM_ANY },
    { "Lost Locket",  "A silver locket from the standing stones.",0,IK_KEY, 0, ICON_RING, false, KM_ANY },
    { "Crown of Elm", "The old crown of the first village.",    0,  IK_KEY, 0, ICON_CROWN,false, KM_ANY },
    { "Star Metal",   "Ore from the deep vault. The forge wants it.",0, IK_KEY, 0, ICON_STAR, false, KM_ANY },
    { "Commonplace",  "The keeper's blank codex: where the next agent's work is written.",0, IK_KEY, 0, ICON_BOOK, false, KM_ANY },
};
const ItemDef& itemDef(int id) {
    static const ItemDef none{ "Nothing", "", 0, IK_KEY, 0, ICON_GEM, false, KM_ANY };
    if (id < 0 || id >= IT_COUNT) return none;
    return ITEMS[id];
}

// ================================================================ heroes
int heroAtk(const Hero& h) {
    int bonus = (h.weapon >= 0) ? itemDef(h.weapon).power : 0;
    if (h.accessory == IT_RING_POWER || h.accessory == IT_RING_WAR)
        bonus += itemDef(h.accessory).power;
    return h.atk + bonus;
}
int heroDef(const Hero& h) {
    int bonus = (h.armor >= 0) ? itemDef(h.armor).power : 0;
    if (h.accessory == IT_SHIELD_WOOD || h.accessory == IT_SHIELD_IRON ||
        h.accessory == IT_SHIELD_KITE || h.accessory == IT_AMULET_GUARD)
        bonus += itemDef(h.accessory).power;
    return h.def + bonus;
}
int heroMag(const Hero& h) {
    int bonus = 0;
    if (h.accessory == IT_AMULET_MANA || h.accessory == IT_RING_SAGE)
        bonus += itemDef(h.accessory).power;
    // Spellwoven armour and staves carry their own magic edge.
    if (h.armor == IT_ROBE_SILK) bonus += 4;
    else if (h.armor == IT_ROBE_ARCHMAGE) bonus += 10;
    if (h.weapon == IT_STAFF_OAK) bonus += 3;
    else if (h.weapon == IT_ROD_IRON) bonus += 7;
    else if (h.weapon == IT_STAFF_SAGE) bonus += 15;
    else if (h.weapon == IT_MACE_BRASS) bonus += 2;
    else if (h.weapon == IT_MACE_HOLY) bonus += 9;
    return h.mag + bonus;
}
int heroSpd(const Hero& h) {
    int bonus = (h.accessory == IT_BOOTS_SWIFT) ? itemDef(h.accessory).power : 0;
    return h.spd + bonus;
}
bool heroCanEquip(const Hero& h, int itemId) {
    if (itemId < 0 || itemId >= IT_COUNT) return false;
    const ItemDef& d = itemDef(itemId);
    if (d.kind != IK_WEAPON && d.kind != IK_ARMOR && d.kind != IK_ACCESSORY) return false;
    if (d.klassMask == KM_ANY) return true;
    return (d.klassMask & (1 << h.klass)) != 0;
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
    return std::max(1, sum / 3);
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
        h.poisoned = false;
        gained++;
    }
    return gained;
}

bool equipHeroItem(int heroIdx, int slot, int itemId) {
    if (heroIdx < 0 || heroIdx > 2 || slot < 0 || slot > 2) return false;
    if (itemId < 0 || itemId >= IT_COUNT) return false;
    ItemKind want = (slot == 0) ? IK_WEAPON : (slot == 1 ? IK_ARMOR : IK_ACCESSORY);
    if (itemDef(itemId).kind != want) return false;
    if (g.inventory[itemId] <= 0) return false;
    Hero& h = g.party[heroIdx];
    if (!heroCanEquip(h, itemId)) return false;
    int* eq = (slot == 0) ? &h.weapon : (slot == 1 ? &h.armor : &h.accessory);
    if (*eq == itemId) return false;
    if (*eq >= 0 && *eq < IT_COUNT) g.inventory[*eq]++;
    g.inventory[itemId]--;
    *eq = itemId;
    return true;
}

bool unequipHeroSlot(int heroIdx, int slot) {
    if (heroIdx < 0 || heroIdx > 2 || slot < 0 || slot > 2) return false;
    Hero& h = g.party[heroIdx];
    int* eq = (slot == 0) ? &h.weapon : (slot == 1 ? &h.armor : &h.accessory);
    if (*eq < 0 || *eq >= IT_COUNT) return false;
    g.inventory[*eq]++;
    *eq = -1;
    return true;
}

// ================================================================ spells
static const SpellDef SPELLS[KLASS_COUNT][6] = {
    // Knight (SCH_PHYS skills scale with heroAtk; Last Stand heals + guards)
    { { "Power Slash",  6, 24, false, false, SCH_PHYS, 1, 0 },
      { "Shield Bash", 10, 20, false, false, SCH_PHYS, 3, 3 },
      { "Whirlwind",   16, 22, false, true,  SCH_PHYS, 6, 0 },
      { "Last Stand",  18, 45, true,  false, SCH_HOLY, 10, 4 },
      { "", 0, 0, false, false, SCH_NONE, 99, 0 },
      { "", 0, 0, false, false, SCH_NONE, 99, 0 } },
    // Mage
    { { "Fire",         8, 34, false, false, SCH_FIRE,  1, 1 },
      { "Spark",        5, 24, false, false, SCH_STORM, 2, 2 },
      { "Frost",       12, 46, false, false, SCH_STORM, 4, 2 },
      { "Firaga",      22, 74, false, false, SCH_FIRE,  7, 1 },
      { "Stormcall",   28, 62, false, true,  SCH_STORM, 11, 2 },
      { "", 0, 0, false, false, SCH_NONE, 99, 0 } },
    // Cleric
    { { "Heal",         6, 62, true,  false, SCH_HOLY,  1, 4 },
      { "Cure All",    16, 48, true,  true,  SCH_HOLY,  3, 4 },
      { "Holy Bolt",   10, 38, false, false, SCH_HOLY,  5, 3 },
      { "Revive",      24,  1, true,  false, SCH_HOLY,  8, 4 },
      { "Sanctuary",   36, 120,true,  true,  SCH_HOLY,  12, 4 },
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
    { "Slime King",   420, 36, 16, 11, 900, 800, 1 << SCH_FIRE,   IT_CROWN,      100, 5 },
    // OpenRTP additions, introduced at the matching field-zone tier.
    { "Bandit",        54, 15,  6, 12,  45,  38, 1 << SCH_FIRE,   IT_SMOKE_BOMB,  15, 2 },
    { "Goblin",        43, 14,  5, 14,  42,  30, 1 << SCH_FIRE,   IT_ANTIDOTE,    20, 2 },
    { "Reaper",        76, 20,  9, 13,  84,  68, 1 << SCH_HOLY,   IT_ETHER,       18, 3 },
    { "Faerie",        38, 12,  4, 16,  45,  38, 1 << SCH_FIRE,   IT_POTION,      15, 2 },
    { "Drake",        178, 31, 18, 13, 250, 190, 1 << SCH_STORM,  IT_HIETHER,     18, 4 },
    { "Naga",         138, 27, 14, 15, 182, 135, 1 << SCH_STORM,  IT_HIETHER,     18, 4 },
    { "Shade",         84, 22,  9, 15,  98,  76, 1 << SCH_HOLY,   IT_REMEDY,      18, 3 },
    { "Dread Knight", 205, 33, 23, 11, 310, 250, 1 << SCH_HOLY,   IT_HIPOTION,    18, 4 },
    { "Archfiend",    280, 40, 24, 16, 445, 360, 1 << SCH_HOLY,   IT_ELIXIR,      10, 5 },
    { "Tiger",         92, 24, 10, 16, 113,  86, 1 << SCH_FIRE,   IT_POTION,      15, 3 },
    { "Porcupine",     62, 18, 13,  8,  76,  54, 1 << SCH_FIRE,   IT_ANTIDOTE,    14, 2 },
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
    "Ember of the Deep",
    "The Kept Light",
    "The Tidemill Islands",
};
static const char* QUEST_GOALS[QUEST_COUNT] = {
    "Thin out 6 slimes in the meadow for Farmer Hedrick.",
    "Find Tamsin's locket at the Standing Stones SE of Elmswick.",
    "Find Hollow Hill's Waystone; defeat the Slime King and return the Crown.",
    "Recover Star Metal from a cavern's deepest vault.",
    "Tend the Agents' Commons for its keeper: still the Wisp in the hedge, then light the brazier.",
    "Hire Bel's boat at the millpond, find the three island Tidewells, and sail home.",
};
const char* questTitle(int id) {
    if (id < 0 || id >= QUEST_COUNT) return "";
    return QUEST_TITLES[id];
}
const char* questGoalText(int id) {
    if (id < 0 || id >= QUEST_COUNT) return "";
    return QUEST_GOALS[id];
}

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

const char* dunThemeName(int theme) {
    switch (theme) {
        case TH_MOSSY:     return "Mossy Warren";
        case TH_ROOTED:    return "Rooted Hollow";
        case TH_AMBER:     return "Amber Vault";
        case TH_MIRE:      return "Mire Grotto";
        case TH_HEATH:     return "Stonevault";
        case TH_FROST:     return "Frostvault";
        case TH_SUNSCORCH: return "Sunscorch Delve";
        case TH_CRYPT:     return "Deep Crypt";
        default:           return "Cavern";
    }
}
MusicCue dunThemeMusic(int theme) {
    switch (theme) {
        case TH_MOSSY:     return MUS_WOOD;
        case TH_ROOTED:    return MUS_WOOD;
        case TH_AMBER:     return MUS_WOOD;
        case TH_MIRE:      return MUS_FEN;
        case TH_HEATH:     return MUS_PASS;
        case TH_FROST:     return MUS_PASS;
        case TH_SUNSCORCH: return MUS_DUNES;
        case TH_CRYPT:     return MUS_FEN;
        default:           return MUS_FEN;
    }
}

// ================================================================ audio helpers
void playSoundIf(Sound& s) {
    if (s.frameCount > 0) PlaySound(s);
}

// A save made afloat resumes on the bank: the hull is not saved (the sea is
// the same on every load) and a party that woke up mid-pond with no boat line
// under it would be a save that cannot be loaded sanely.
static void parkSavedParty() {
    if (!g.sailing) return;
    g.sailing = false;
    g.mode = MODE_OVERWORLD;
    int lx = 0, ly = 0;
    if (tideIslandCount() > 0) {
        // land at the nearest isle's beach so a save taken over open water
        // resumes somewhere the party can actually stand
        float best = 1e9f;
        for (int i = 0; i < tideIslandCount(); i++) {
            float dx = tideIslandWellX(i) - gSailX, dy = tideIslandWellY(i) - gSailY;
            float dd = dx * dx + dy * dy;
            if (dd < best) { best = dd; lx = (int)floorf(tideIslandWellX(i)); ly = (int)floorf(tideIslandWellY(i)); }
        }
    }
    if (lx == 0 && ly == 0) {
        // home landing: a few steps north of the millpond's bank
        lx = (int)floorf(g.px); ly = (int)floorf(g.py);
    }
    int wx = 0, wy = 0;
    bool found = false;
    for (int r = 0; r <= 6 && !found; r++) {
        for (int dy = -r; dy <= r && !found; dy++) for (int dx = -r; dx <= r; dx++) {
            if (std::max(abs(dx), abs(dy)) != r) continue;
            if (cellWalkable(lx + dx, ly + dy)) { wx = lx + dx; wy = ly + dy; found = true; break; }
        }
    }
    if (found) { g.px = wx + 0.5f; g.py = wy + 0.5f; }
}

void returnToTitle() {
    g.mode = MODE_TITLE;
    g.menuPage = 0;
    g.titleIndex = 0;
    g.fade = 1.0f;
    g.fadeDir = -1;
    refreshSaveExists();
}

// ================================================================ assets
static std::string resolveAssetsDir() {
    const char* env = getenv("APFA_ASSETS_DIR");
    if (env && env[0]) { std::string d = env; if (d.back() != '/') d += '/'; return d; }
    if (FileExists("assets/tiles/landscape.png")) return "assets/";
    if (FileExists("../assets/tiles/landscape.png")) return "../assets/";
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
    return IsAudioDeviceReady() ? LoadSound((dir + rel).c_str()) : Sound{};
}
bool loadAllAssets() {
    std::string d = resolveAssetsDir();
    Assets& a = g.assets;

    // --- terrain
    a.landscape = loadTex(d,"tiles/landscape.png");
    a.relief = loadTex(d,"tiles/relief.png");
    a.bridge = loadTex(d,"tiles/bridge.png");
    a.turnips = loadTex(d,"tiles/turnips.png");
    a.lilies = loadTex(d,"tiles/lilies.png");
    for (int i = 0; i < 3; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "tiles/tuft_%d.png", i);
        a.tuft[i] = loadTex(d, buf, false);
    }
    for (int i = 0; i < 2; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "tiles/flower_%d.png", i); a.flowers[i] = loadTex(d, buf, false);
    }
    a.brickFloor[0] = loadTex(d, "tiles/brick.png", false);
    a.brickFloor[1] = loadTex(d, "tiles/brick_dark.png", false);
    a.plankFloor = loadTex(d, "tiles/plank_floor.png", false);
    a.tileFloor = loadTex(d, "tiles/tile_floor.png", false);
    a.stoneFloor = loadTex(d, "tiles/stone_floor.png", false);
    a.carpet = loadTex(d, "tiles/carpet.png", false);
    a.pebble = loadTex(d, "tiles/pebble.png", false);
    // dungeon theme floors — all are existing 16×16 runtime tiles, already in ART_MANIFEST.md
    a.dungeonFloor = loadTex(d, "tiles/dungeon_floor.png", false);
    a.flagstoneFloor = loadTex(d, "tiles/flagstone.png", false);
    a.cobbleFloor = loadTex(d, "tiles/cobble.png", false);
    a.sandFloor = loadTex(d, "tiles/sand.png", false);
    a.tundraFloor = loadTex(d, "tiles/tundra.png", false);
    a.iceFloor = loadTex(d, "tiles/ice.png", false);
    a.snowFloor = loadTex(d, "tiles/snow.png", false);
    a.swampFloor = loadTex(d, "tiles/swamp.png", false);
    a.arcaneFloor = loadTex(d, "tiles/arcane.png", false);
    a.dirtFloor = loadTex(d, "tiles/dirt.png", false);
    a.iceCrack = loadTex(d, "tiles/ice_crack.png", false);
    // Additive native OpenRTP cave materials; old tiles remain available for crypts.
    const char* caveFloors[] = {"tiles/cave_floor_0.png", "tiles/cave_floor_1.png",
                                "tiles/cave_floor_2.png", "tiles/cave_floor_3.png"};
    const char* caveRocks[] = {"tiles/cave_rock_0.png", "tiles/cave_rock_1.png",
                               "tiles/cave_rock_2.png", "tiles/cave_rock_3.png"};
    const char* caveForms[] = {"sprites/cave_stalagmite.png", "sprites/cave_stalagmite_small.png",
                               "sprites/cave_crystal_blue.png", "sprites/cave_crystal_cluster.png",
                               "sprites/cave_rock_pile.png", "sprites/cave_boulder.png"};
    for (int i = 0; i < 4; ++i) {
        a.caveFloor[i] = loadTex(d, caveFloors[i], false);
        a.caveRock[i] = loadTex(d, caveRocks[i], false);
    }
    for (int i = 0; i < 6; ++i) a.caveFormation[i] = loadTex(d, caveForms[i], false);
    // per-theme floor atlas (each theme picks a distinct base tile; tint adds mood)
    a.dunFloor[TH_MOSSY]     = a.stoneFloor.id ? a.stoneFloor : a.dungeonFloor;
    a.dunFloor[TH_ROOTED]    = a.dirtFloor.id ? a.dirtFloor : a.stoneFloor;
    a.dunFloor[TH_AMBER]     = a.tileFloor.id ? a.tileFloor : a.stoneFloor;
    a.dunFloor[TH_MIRE]      = a.swampFloor.id ? a.swampFloor : a.dungeonFloor;
    a.dunFloor[TH_HEATH]     = a.cobbleFloor.id ? a.cobbleFloor : a.brickFloor[0];
    a.dunFloor[TH_FROST]     = a.iceFloor.id ? a.iceFloor : a.snowFloor;
    a.dunFloor[TH_SUNSCORCH] = a.sandFloor.id ? a.sandFloor : a.dungeonFloor;
    a.dunFloor[TH_CRYPT]     = a.flagstoneFloor.id ? a.flagstoneFloor : a.dungeonFloor;
    // per-theme wall tints reuse the brick atlas with theme colour; keep distinct warm/cold
    a.dunWall[TH_MOSSY]     = a.brickFloor[1];
    a.dunWall[TH_ROOTED]    = a.brickFloor[0];
    a.dunWall[TH_AMBER]     = a.brickFloor[0];
    a.dunWall[TH_MIRE]      = a.brickFloor[1];
    a.dunWall[TH_HEATH]     = a.brickFloor[0];
    a.dunWall[TH_FROST]     = a.brickFloor[1];
    a.dunWall[TH_SUNSCORCH] = a.brickFloor[0];
    a.dunWall[TH_CRYPT]     = a.brickFloor[1];

    const char* scenes[BI_COUNT] = {"meadow", "forest", "amber", "fen", "heath", "frost", "dunes"};
    for (int i=0; i<BI_COUNT; ++i)
        a.battlefield[i] = loadTex(d,(std::string("backdrops/")+scenes[i]+".png").c_str());
    a.castleBattlefield = loadTex(d,"backdrops/castle_field.png");

    // --- props
    a.amberOak = loadTex(d,"sprites/amber_oak.png");
    a.copperOak = loadTex(d,"sprites/copper_oak.png");
    a.willow = loadTex(d,"sprites/willow.png");
    a.yucca = loadTex(d,"sprites/yucca.png");
    a.reeds = loadTex(d,"sprites/reeds.png");
    a.crate = loadTex(d,"sprites/crate.png");
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
    a.tent = loadTex(d, "sprites/pavilion.png");
    a.ruinArch = loadTex(d, "sprites/ruin_arch.png");
    a.barrel = loadTex(d, "sprites/barrel.png");
    a.table = loadTex(d, "sprites/table.png");
    a.longtable = loadTex(d, "sprites/longtable.png", false);
    a.counter = loadTex(d, "sprites/counter.png");
    a.lantern = loadTex(d, "sprites/lantern.png");
    a.candle = loadTex(d, "sprites/candle.png");
    a.keg = loadTex(d, "sprites/keg.png");
    a.plate = loadTex(d, "sprites/plate.png");
    a.shelf = loadTex(d, "sprites/shelf.png", false);
    a.mailbox = loadTex(d, "sprites/mailbox.png", false);
    a.noticeboard = loadTex(d, "sprites/noticeboard.png", false);
    a.chestClosed = loadTex(d, "sprites/chest_closed.png");
    a.chestOpen = loadTex(d, "sprites/chest_open.png");
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
    a.smithy = loadTex(d, "sprites/smithy.png");
    a.forgeHearth = loadTex(d, "sprites/forge_hearth.png");
    a.forgeAnvil = loadTex(d, "sprites/forge_anvil.png");
    a.caveMouth = loadTex(d, "sprites/cave_mouth.png");
    a.crystalCluster = loadTex(d, "sprites/cave_crystal_cluster.png");
    a.statusStone[0] = loadTex(d, "sprites/commons_stone.png");
    a.statusStone[1] = loadTex(d, "sprites/commons_stone_tall.png");
    a.statusStone[2] = loadTex(d, "sprites/commons_stone_low.png");
    a.stairDown = loadTex(d, "sprites/stair_down.png");
    a.stairUp = loadTex(d, "sprites/stair_up.png");
    a.tidewell = loadTex(d, "sprites/tidewell.png");
    a.tidewellDry = loadTex(d, "sprites/tidewell_dry.png");
    a.tideStone = loadTex(d, "sprites/tide_stone.png");

    // --- heroes
    const char* heroes[3] = { "sprites/hero_knight.png", "sprites/hero_mage.png", "sprites/hero_cleric.png" };
    for (int i = 0; i < 3; i++) a.heroWalk[i] = loadTex(d, heroes[i]);
    a.boat = loadTex(d, "sprites/boat.png");
    const char* npcs[NS_KIND_COUNT] = {
        "sprites/npc_villager0.png", "sprites/npc_villager1.png", "sprites/npc_villager2.png",
        "sprites/npc_villager3.png", "sprites/npc_elder.png", "sprites/npc_mayor.png",
        "sprites/npc_woman.png", "sprites/npc_child.png", "sprites/npc_guard.png",
        "sprites/npc_soldier.png", "sprites/npc_knight.png", "sprites/npc_merchant.png",
        "sprites/npc_smith.png", "sprites/npc_priest.png", "sprites/npc_farmer.png",
        "sprites/npc_dog.png", "sprites/npc_cat.png", "sprites/npc_chicken.png",
        "sprites/npc_sheep.png",
        "sprites/npc_villager4.png", "sprites/npc_villager5.png", "sprites/npc_villager6.png",
        "sprites/npc_villager7.png", "sprites/npc_villager8.png", "sprites/npc_villager9.png",
        "sprites/npc_ranger.png", "sprites/npc_cow.png", "sprites/npc_horse.png",
        "sprites/npc_keeper.png", "sprites/npc_ferryman.png",
    };
    for (int i = 0; i < NS_KIND_COUNT; i++) a.npc[i] = loadTex(d, npcs[i]);

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
        "sprites/face_demon.png", "sprites/face_dog.png",
        "sprites/face_villager4.png", "sprites/face_villager5.png", "sprites/face_villager6.png",
        "sprites/face_villager7.png", "sprites/face_villager8.png", "sprites/face_villager9.png",
        "sprites/face_keeper.png",
    };
    for (int i = 0; i < FACE_COUNT; i++) a.face[i] = loadTex(d, faces[i]);

    // --- enemies, in EN_* order
    const char* enemies[EN_COUNT] = {
        "slime", "imp", "bat", "hornet", "wisp", "wolf", "skeleton", "ghost",
        "orc", "serpent", "golem", "night_rider", "slime_king",
        "bandit", "goblin", "reaper", "faerie", "drake", "naga", "shade",
        "dread_knight", "archfiend", "tiger", "porcupine",
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
        "icons/feather.png", "icons/food.png", "icons/book.png", "icons/icon_candle.png",
        "icons/gem.png", "icons/coin.png",
        "icons/sword_0.png", "icons/sword_1.png", "icons/sword_2.png",
        "icons/armor_0.png", "icons/armor_1.png", "icons/armor_2.png", "icons/armor_3.png",
        "icons/shield.png", "icons/ring.png", "icons/key.png", "icons/crown.png",
        "icons/staff.png", "icons/axe.png", "icons/spear.png", "icons/bow.png",
        "icons/mace.png", "icons/boots.png", "icons/robe.png", "icons/remedy.png",
        "icons/bomb.png", "icons/star_metal.png",
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

    loadScore(d);

    a.ok = true;
    return true;
}

// ================================================================ ui helpers
static Color lerpCol(Color a, Color b, float t) {
    auto c = [&](unsigned char x, unsigned char y) { return (unsigned char)(x + (y - x) * t); };
    return { c(a.r, b.r), c(a.g, b.g), c(a.b, b.b), c(a.a, b.a) };
}

void drawWindow(Rectangle r, bool dimInside) {
    int rx = (int)floorf(r.x), ry = (int)floorf(r.y);
    int rw = (int)roundf(r.width), rh = (int)roundf(r.height);
    DrawRectangle(rx + 2, ry + 3, rw, rh, Fade(BLACK, 0.42f));
    if (dimInside) {
        for (int i = 0; i < rh; i++) {
            float t = (float)i / std::max(1.0f, (float)rh - 1.0f);
            Color c = lerpCol(Color{ 28, 44, 118, 250 }, Color{ 10, 16, 52, 252 }, t);
            DrawRectangle(rx, ry + i, rw, 1, c);
        }
    } else {
        DrawRectangle(rx, ry, rw, rh, Color{ 14, 20, 56, 236 });
    }
    DrawRectangleLinesEx({ (float)rx, (float)ry, (float)rw, (float)rh }, 1.0f, Color{ 8, 10, 24, 255 });
    if (rw > 4 && rh > 4) {
        DrawRectangleLinesEx({ (float)(rx + 1), (float)(ry + 1), (float)(rw - 2), (float)(rh - 2) },
                             1.0f, Color{ 236, 240, 255, 255 });
    }
    if (rw > 6 && rh > 6) {
        DrawRectangleLinesEx({ (float)(rx + 2), (float)(ry + 2), (float)(rw - 4), (float)(rh - 4) },
                             1.0f, Color{ 96, 118, 208, 255 });
    }
}

void drawWindowBlue(Rectangle r) {
    int rx = (int)floorf(r.x), ry = (int)floorf(r.y);
    int rw = (int)roundf(r.width), rh = (int)roundf(r.height);
    DrawRectangle(rx + 2, ry + 3, rw, rh, Fade(BLACK, 0.42f));
    DrawRectangle(rx, ry, rw, rh, Color{ 16, 20, 48, 250 });
    DrawRectangleLinesEx({ (float)rx, (float)ry, (float)rw, (float)rh }, 1.0f, Color{ 8, 10, 24, 255 });
    if (rw > 4 && rh > 4) {
        DrawRectangleLinesEx({ (float)(rx + 1), (float)(ry + 1), (float)(rw - 2), (float)(rh - 2) },
                             1.0f, Color{ 255, 214, 130, 255 });
    }
    if (rw > 6 && rh > 6) {
        DrawRectangleLinesEx({ (float)(rx + 2), (float)(ry + 2), (float)(rw - 4), (float)(rh - 4) },
                             1.0f, Color{ 118, 96, 56, 255 });
    }
}

void drawPixelPanel(Rectangle r, Color fill, Color border) {
    int rx = (int)floorf(r.x), ry = (int)floorf(r.y);
    int rw = (int)roundf(r.width), rh = (int)roundf(r.height);
    if (rw <= 0 || rh <= 0) return;
    DrawRectangle(rx + 1, ry + 2, rw, rh, Fade(BLACK, 0.35f));
    DrawRectangle(rx, ry, rw, rh, fill);
    DrawRectangleLinesEx({ (float)rx, (float)ry, (float)rw, (float)rh }, 1.0f, Color{ 8, 10, 24, 255 });
    if (rw > 2 && rh > 2) {
        DrawRectangleLinesEx({ (float)(rx + 1), (float)(ry + 1), (float)(rw - 2), (float)(rh - 2) },
                             1.0f, border);
    }
}

void drawPixelPanel(Rectangle r, bool selected) {
    // Shared navy-and-gold selection language: bright navy + gold border
    // when selected, deep navy + steel-blue border otherwise.
    drawPixelPanel(r,
                   selected ? Color{ 56, 86, 164, 255 } : Color{ 26, 36, 78, 255 },
                   selected ? Color{ 255, 232, 150, 255 } : Color{ 90, 110, 160, 255 });
}

Vector2 measureText(const char* text, int size) {
    if (!text || !text[0]) return { 0.0f, (float)size };
    Font f = (size >= 32) ? g.assets.font32 : (size >= 24 ? g.assets.font24 : (size >= 16 ? g.assets.font16 : g.assets.font8));
    return MeasureTextEx(f, text, (float)size, 1.0f);
}

static void drawLine(const char* s, float x, float y, Color c, int size) {
    if (!s || !s[0]) return;
    Font f = (size >= 32) ? g.assets.font32 : (size >= 24 ? g.assets.font24 : (size >= 16 ? g.assets.font16 : g.assets.font8));
    float px = floorf(x);
    float py = floorf(y);
    if ((int)c.r + (int)c.g + (int)c.b >= 45 && c.a > 24) {
        unsigned char sa = (unsigned char)((int)c.a * 230 / 255);
        DrawTextEx(f, s, { px + 1.0f, py + 1.0f }, (float)size, 1.0f, Color{ 6, 8, 18, sa });
    }
    DrawTextEx(f, s, { px, py }, (float)size, 1.0f, c);
}

void drawText(const char* text, float x, float y, Color c, int size) {
    drawLine(text, x, y, c, size);
}
void drawTextCentered(const char* text, float cx, float y, Color c, int size) {
    Vector2 s = measureText(text, size);
    drawLine(text, floorf(cx - s.x * 0.5f), floorf(y), c, size);
}
void drawWorldLabel(const char* text, float cx, float y, Color c, int size) {
    if (!text || !text[0]) return;
    Vector2 s = measureText(text, size);
    int tx = (int)floorf(cx - s.x * 0.5f);
    int ty = (int)floorf(y);
    int pw = (int)ceilf(s.x) + 6;
    int ph = size + 4;
    unsigned char ba = (unsigned char)((int)c.a * 220 / 255);
    unsigned char la = (unsigned char)((int)c.a * 175 / 255);
    DrawRectangle(tx - 3, ty - 2, pw, ph, Color{ 10, 14, 28, ba });
    DrawRectangleLinesEx({ (float)(tx - 3), (float)(ty - 2), (float)pw, (float)ph },
                         1.0f, Color{ 112, 128, 196, la });
    drawLine(text, (float)tx, (float)ty, c, size);
}

void drawTextWrapped(const char* text, float x, float y, float maxW, float lineH, Color c, int size) {
    if (!text || !text[0]) return;
    std::string line;
    float cy = y;
    const char* p = text;
    while (*p) {
        if (*p == '\n') {
            if (!line.empty()) { drawText(line.c_str(), x, cy, c, size); line.clear(); }
            cy += lineH;
            p++;
            continue;
        }
        // grab next word (including leading spaces if mid-line)
        std::string word;
        while (*p == ' ') { word += *p++; }
        while (*p && *p != ' ' && *p != '\n') { word += *p++; }
        if (line.empty()) {
            // trim leading spaces on fresh line
            size_t first = word.find_first_not_of(' ');
            if (first != std::string::npos) word = word.substr(first);
            else continue;
            // if a single word is wider than maxW, split by character
            if (measureText(word.c_str(), size).x > maxW) {
                std::string chunk;
                for (char ch : word) {
                    std::string cand = chunk + ch;
                    if (!chunk.empty() && measureText(cand.c_str(), size).x > maxW) {
                        drawText(chunk.c_str(), x, cy, c, size);
                        cy += lineH;
                        chunk.clear();
                    }
                    chunk += ch;
                }
                line = chunk;
            } else {
                line = word;
            }
        } else {
            std::string cand = line + word;
            if (measureText(cand.c_str(), size).x > maxW) {
                drawText(line.c_str(), x, cy, c, size);
                cy += lineH;
                size_t first = word.find_first_not_of(' ');
                line = (first != std::string::npos) ? word.substr(first) : "";
            } else {
                line = cand;
            }
        }
    }
    if (!line.empty()) drawText(line.c_str(), x, cy, c, size);
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

void drawButton(const char* text, Rectangle r, int size, bool selected) {
    Vector2 m = getVirtualMouse();
    bool hover = CheckCollisionPointRec(m, r);
    if (hover) g.hotButtons++;
    bool hi = hover || selected;
    drawPixelPanel(r, hi ? Color{ 64, 74, 150, 255 } : Color{ 30, 38, 92, 255 },
                      hi ? Color{ 255, 214, 130, 255 } : Color{ 140, 150, 220, 255 });
    drawTextCentered(text, r.x + r.width / 2, r.y + r.height / 2 - size / 2.0f + 1,
                     hi ? WHITE : Color{ 214, 220, 250, 255 }, size);
}

bool uiButtonHit(Rectangle r) {
    Vector2 m = getVirtualMouse();
    if (CheckCollisionPointRec(m, r)) {
        g.hotButtons++;
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            playSoundIf(g.assets.confirm);
            return true;
        }
    }
    return false;
}

bool uiButtonAt(const char* text, Rectangle r, int size) {
    drawButton(text, r, size);
    return uiButtonHit(r);
}

void drawCursor(float x, float y, Color c) {
    // Blinks at ~2 Hz like an SNES menu cursor; steady when the caller wants
    // a static marker by passing through a frame where the blink is on.
    if (fmodf(g.totalTime * 2.0f, 1.0f) > 0.72f) return;
    DrawTriangle({ x, y }, { x, y + 8 }, { x + 7, y + 4 }, c);
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
void addItem(int itemId, int amount, bool silent) {
    if (itemId < 0 || itemId >= IT_COUNT || amount <= 0) return;
    g.inventory[itemId] += amount;
    if (!silent) {
        pushToast(std::string("Received ") + itemDef(itemId).name +
                  (amount > 1 ? (" x" + std::to_string(amount)) : ""));
    }
}
bool takeItem(int itemId, int amount) {
    if (itemId < 0 || itemId >= IT_COUNT || amount <= 0) return false;
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
        case IT_MEGAPOTION:if (h.ko || h.hp >= h.maxHp) return false;
                           h.hp = std::min(h.maxHp, h.hp + 400); break;
        case IT_ETHER:     if (h.ko || h.mp >= h.maxMp) return false;
                           h.mp = std::min(h.maxMp, h.mp + 30); break;
        case IT_HIETHER:   if (h.ko || h.mp >= h.maxMp) return false;
                           h.mp = std::min(h.maxMp, h.mp + 90); break;
        case IT_ELIXIR:    if (!h.ko && !h.poisoned && h.hp >= h.maxHp && h.mp >= h.maxMp) return false;
                           h.hp = h.maxHp; h.mp = h.maxMp; h.ko = false; h.poisoned = false; break;
        case IT_PHOENIX:   if (!h.ko) return false;
                           h.ko = false; h.poisoned = false; h.hp = std::max(1, h.maxHp / 2); break;
        case IT_ANTIDOTE:  if (h.ko || (!h.poisoned && h.hp >= h.maxHp)) return false;
                           if (h.poisoned) h.poisoned = false;
                           else h.hp = std::min(h.maxHp, h.hp + 20);
                           break;
        case IT_REMEDY:    if (h.ko || (!h.poisoned && h.hp >= h.maxHp)) return false;
                           h.poisoned = false;
                           h.hp = std::min(h.maxHp, h.hp + 30);
                           break;
        case IT_SMOKE_BOMB:
                           // Usable only mid-battle; battle.cpp intercepts this id.
                           return false;
        default: return false;
    }
    g.inventory[itemId]--;
    return true;
}

// ================================================================ save / load
constexpr int SAVE_MAX_IDS = 256;
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
        int32_t weapon, armor, accessory, ko, row, poisoned;
    } heroes[3];
    int32_t questState[QUEST_COUNT], questProgress[QUEST_COUNT], questGoal[QUEST_COUNT];
    uint32_t flags;                // bit0 bossDefeated, bit1 locketFound, bit2 towerChestOpened,
                                   // bit3 startVillageSeen, bit4 musicOn, bit5 waitBattle,
                                   // bit6 crtOn, bit7 hasBoat (older saves read as no boat),
                                   // bits 8-9 tideFound, bit10 waystone, bit11 frontier guardian,
                                   // bit12 regional-save marker, bit13 grandfathered far passage
    int32_t openedChestCount;
    uint64_t openedChests[SAVE_MAX_IDS];
    int32_t defeatedMonsterCount;
    uint64_t defeatedMonsters[SAVE_MAX_IDS];
    uint32_t checksum;
};
// Version 4 layout (22 items, 3 quests) — kept so pre-dungeon saves migrate.
struct SaveBlobV4 {
    uint32_t magic;
    uint32_t version;
    uint32_t worldSeed;
    float px, py;
    float runTime;
    int32_t gil, defeatedCount, slimesSlain;
    int32_t inventory[22];
    struct HS {
        int32_t klass, level, xp, hp, mp, maxHp, maxMp, atk, def, spd, mag;
        int32_t weapon, armor, accessory, ko, row, poisoned;
    } heroes[3];
    int32_t questState[3], questProgress[3], questGoal[3];
    uint32_t flags;
    int32_t openedChestCount;
    uint64_t openedChests[SAVE_MAX_IDS];
    int32_t defeatedMonsterCount;
    uint64_t defeatedMonsters[SAVE_MAX_IDS];
    uint32_t checksum;
};
// Version 5a layout: the four-quest save written before the Agents' Commons.
// The checksum cover is identical (it ends at the same fields), so an old blob
// is byte-for-byte readable as a SaveBlob once the missing quest row is zeroed.
struct SaveBlobV5 {
    uint32_t magic;
    uint32_t version;
    uint32_t worldSeed;
    float px, py;
    float runTime;
    int32_t gil, defeatedCount, slimesSlain;
    int32_t inventory[IT_COUNT];
    struct HS {
        int32_t klass, level, xp, hp, mp, maxHp, maxMp, atk, def, spd, mag;
        int32_t weapon, armor, accessory, ko, row, poisoned;
    } heroes[3];
    int32_t questState[4], questProgress[4], questGoal[4];
    uint32_t flags;
    int32_t openedChestCount;
    uint64_t openedChests[SAVE_MAX_IDS];
    int32_t defeatedMonsterCount;
    uint64_t defeatedMonsters[SAVE_MAX_IDS];
    uint32_t checksum;
};
// Everything up to the quest table is byte-identical; the only difference is
// the number of quest rows, and the old checksum covers exactly its own bytes.
static_assert(offsetof(SaveBlobV5, questState) == offsetof(SaveBlob, questState),
              "fields before the quest table must not move");
static_assert(sizeof(SaveBlobV5::HS) == sizeof(SaveBlob::HS), "hero rows must not move");
static_assert(sizeof(SaveBlobV5) != sizeof(SaveBlob), "old and new saves must not collide");

static const uint32_t SAVE_MAGIC = 0x41504641;   // 'APFA'
static const uint32_t SAVE_VERSION = 5;

template <typename T>
static uint32_t blobChecksumOf(const T& b) {
    uint32_t h = 2166136261u;
    const uint8_t* p = (const uint8_t*)&b;
    size_t n = offsetof(T, checksum);
    for (size_t i = 0; i < n; i++) { h ^= p[i]; h *= 16777619u; }
    return h;
}
static uint32_t blobChecksum(const SaveBlob& b) { return blobChecksumOf(b); }

// v4 ItemId -> v5 ItemId (old saves keep every item they owned).
static int migrateItemIdV4(int id) {
    static const int kMap[22] = {
        0, 1, 3, 4, 5, 6, 8, 10, 11, 13, 14,
        24, 25, 27, 28, 31, 32, 34, 35, 40, 41, 42
    };
    if (id < 0 || id >= 22) return -1;
    return kMap[id];
}
std::string saveFilePath() {
    const char* env = getenv("APFA_SAVE");
    if (env && env[0]) return env;
    return std::string(GetApplicationDirectory()) + "aplaceforagents_save.sav";
}
bool saveExists() { return FileExists(saveFilePath().c_str()); }
void refreshSaveExists() { g.saveExists = saveExists(); }

bool saveGame() {
    SaveBlob b{};
    b.magic = SAVE_MAGIC;
    b.version = SAVE_VERSION;
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
                        h.ko ? 1 : 0, h.row, h.poisoned ? 1 : 0 };
    }
    for (int q = 0; q < QUEST_COUNT; q++) {
        b.questState[q] = g.quests[q].state;
        b.questProgress[q] = g.quests[q].progress;
        b.questGoal[q] = g.quests[q].goal;
    }
    b.flags = 0;
    if (g.bossDefeated)     b.flags |= (1u << 0);
    if (g.locketFound)      b.flags |= (1u << 1);
    if (g.towerChestOpened) b.flags |= (1u << 2);
    if (g.startVillageSeen) b.flags |= (1u << 3);
    if (g.musicOn)          b.flags |= (1u << 4);
    if (g.waitBattle)       b.flags |= (1u << 5);
    if (g.crtOn)            b.flags |= (1u << 6);
    if (g.hasBoat)          b.flags |= (1u << 7);
    b.flags |= ((uint32_t)std::max(0, std::min(TIDE_ISLANDS, g.tideFound))) << 8;
    if (g.hasWaystone)      b.flags |= (1u << 10);
    if (g.frontierGuardian) b.flags |= (1u << 11);
    b.flags |= (1u << 12);          // distinguishes a new save from a pre-range v5 save
    if (g.legacyFarPass)    b.flags |= (1u << 13);

    int oc = 0;
    for (uint64_t id : g.openedChests) {
        if (oc >= SAVE_MAX_IDS) break;
        b.openedChests[oc++] = id;
    }
    b.openedChestCount = oc;

    int dm = 0;
    for (uint64_t id : g.defeatedMonsters) {
        if (dm >= SAVE_MAX_IDS) break;
        b.defeatedMonsters[dm++] = id;
    }
    b.defeatedMonsterCount = dm;

    b.checksum = blobChecksum(b);
    bool ok = SaveFileData(saveFilePath().c_str(), &b, sizeof(b));
    if (ok) g.saveExists = true;
    return ok;
}

bool loadGame() {
    int size = 0;
    unsigned char* data = LoadFileData(saveFilePath().c_str(), &size);
    if (!data) return false;

    SaveBlob b{};
    bool fromV4 = false;
    if (size == (int)sizeof(SaveBlob)) {
        memcpy(&b, data, sizeof(b));
        UnloadFileData(data);
        if (b.magic != SAVE_MAGIC || b.version != SAVE_VERSION || blobChecksum(b) != b.checksum)
            return false;
    } else if (size == (int)sizeof(SaveBlobV5)) {
        // Saves written before the Agents' Commons: same fields, one fewer
        // quest row. Copy the covered bytes and open the new errand fresh.
        SaveBlobV5 o;
        memcpy(&o, data, sizeof(o));
        UnloadFileData(data);
        if (o.magic != SAVE_MAGIC || o.version != SAVE_VERSION || blobChecksumOf(o) != o.checksum)
            return false;
        memcpy(&b, &o, offsetof(SaveBlob, questState));          // magic .. heroes
        memcpy(b.inventory, o.inventory, sizeof(b.inventory) < sizeof(o.inventory) ? sizeof(b.inventory) : sizeof(o.inventory));
        for (int q = 0; q < 4; q++) {
            b.questState[q] = o.questState[q];
            b.questProgress[q] = o.questProgress[q];
            b.questGoal[q] = o.questGoal[q];
        }
        b.flags = o.flags;
        b.openedChestCount = o.openedChestCount;
        memcpy(b.openedChests, o.openedChests, sizeof(b.openedChests));
        b.defeatedMonsterCount = o.defeatedMonsterCount;
        memcpy(b.defeatedMonsters, o.defeatedMonsters, sizeof(b.defeatedMonsters));
        b.questState[QUEST_COMMONS] = QS_OFFERED;
        b.questProgress[QUEST_COMMONS] = 0;
        b.questGoal[QUEST_COMMONS] = 1;
    } else if (size == (int)sizeof(SaveBlobV4)) {
        // Pre-dungeon save: migrate inventory & quests to the v5 layout.
        SaveBlobV4 o;
        memcpy(&o, data, sizeof(o));
        UnloadFileData(data);
        if (o.magic != SAVE_MAGIC || o.version != 4 || blobChecksumOf(o) != o.checksum)
            return false;
        fromV4 = true;
        b.magic = SAVE_MAGIC;
        b.version = SAVE_VERSION;
        b.worldSeed = o.worldSeed;
        b.px = o.px; b.py = o.py; b.runTime = o.runTime;
        b.gil = o.gil; b.defeatedCount = o.defeatedCount; b.slimesSlain = o.slimesSlain;
        for (int i = 0; i < 22; i++) {
            int mapped = migrateItemIdV4(i);
            if (mapped >= 0) b.inventory[mapped] = o.inventory[i];
        }
        for (int i = 0; i < 3; i++) {
            const SaveBlobV4::HS& s = o.heroes[i];
            SaveBlob::HS& d = b.heroes[i];
            d.klass = s.klass; d.level = s.level; d.xp = s.xp;
            d.hp = s.hp; d.mp = s.mp; d.maxHp = s.maxHp; d.maxMp = s.maxMp;
            d.atk = s.atk; d.def = s.def; d.spd = s.spd; d.mag = s.mag;
            d.weapon    = migrateItemIdV4(s.weapon);
            d.armor     = migrateItemIdV4(s.armor);
            d.accessory = migrateItemIdV4(s.accessory);
            d.ko = s.ko; d.row = s.row; d.poisoned = s.poisoned;
        }
        for (int q = 0; q < 3; q++) {
            b.questState[q] = o.questState[q];
            b.questProgress[q] = o.questProgress[q];
            b.questGoal[q] = o.questGoal[q];
        }
        b.questState[QUEST_DEEP] = QS_OFFERED;   // new quest, fresh in the log
        b.questProgress[QUEST_DEEP] = 0;
        b.questGoal[QUEST_DEEP] = 1;
        b.questState[QUEST_COMMONS] = QS_OFFERED; // and the keeper's errand
        b.questProgress[QUEST_COMMONS] = 0;
        b.questGoal[QUEST_COMMONS] = 1;
        b.flags = o.flags;
        b.openedChestCount = o.openedChestCount;
        memcpy(b.openedChests, o.openedChests, sizeof(b.openedChests));
        b.defeatedMonsterCount = o.defeatedMonsterCount;
        memcpy(b.defeatedMonsters, o.defeatedMonsters, sizeof(b.defeatedMonsters));
        b.checksum = blobChecksum(b);
    } else {
        UnloadFileData(data);
        return false;
    }
    (void)fromV4;

    g.worldSeed = b.worldSeed;
    g.px = b.px; g.py = b.py;
    g.runTime = b.runTime;
    g.gil = std::max(0, (int)b.gil);
    g.defeatedCount = std::max(0, (int)b.defeatedCount);
    g.slimesSlain = std::max(0, (int)b.slimesSlain);
    for (int i = 0; i < IT_COUNT; i++) g.inventory[i] = std::max(0, (int)b.inventory[i]);
    for (int i = 0; i < 3; i++) {
        Hero& h = g.party[i];
        h.klass = std::max(0, std::min(KLASS_COUNT - 1, (int)b.heroes[i].klass));
        h.level = std::max(1, (int)b.heroes[i].level);
        h.xp = std::max(0, (int)b.heroes[i].xp);
        h.maxHp = std::max(1, (int)b.heroes[i].maxHp);
        h.maxMp = std::max(0, (int)b.heroes[i].maxMp);
        h.hp = std::max(0, std::min(h.maxHp, (int)b.heroes[i].hp));
        h.mp = std::max(0, std::min(h.maxMp, (int)b.heroes[i].mp));
        h.atk = std::max(1, (int)b.heroes[i].atk);
        h.def = std::max(1, (int)b.heroes[i].def);
        h.spd = std::max(1, (int)b.heroes[i].spd);
        h.mag = std::max(1, (int)b.heroes[i].mag);
        h.weapon = (b.heroes[i].weapon >= -1 && b.heroes[i].weapon < IT_COUNT) ? b.heroes[i].weapon : -1;
        h.armor = (b.heroes[i].armor >= -1 && b.heroes[i].armor < IT_COUNT) ? b.heroes[i].armor : -1;
        h.accessory = (b.heroes[i].accessory >= -1 && b.heroes[i].accessory < IT_COUNT) ? b.heroes[i].accessory : -1;
        h.ko = b.heroes[i].ko != 0 || h.hp == 0;
        h.row = b.heroes[i].row;
        h.poisoned = b.heroes[i].poisoned != 0;
    }
    for (int q = 0; q < QUEST_COUNT; q++) {
        g.quests[q].state = std::max(0, std::min((int)QS_DONE, (int)b.questState[q]));
        g.quests[q].progress = std::max(0, (int)b.questProgress[q]);
        g.quests[q].goal = std::max(0, (int)b.questGoal[q]);
    }
    g.quests[QUEST_SLIMES].goal = 6;
    g.quests[QUEST_LOCKET].goal = 1;
    g.quests[QUEST_CROWN].goal = 1;
    g.quests[QUEST_DEEP].goal = 1;
    g.quests[QUEST_COMMONS].goal = 1;
    g.quests[QUEST_TIDE].goal = TIDE_ISLANDS;
    if (g.quests[QUEST_TIDE].state == QS_UNKNOWN) {   // a save older than the water
        g.quests[QUEST_TIDE].state = QS_OFFERED;
        g.quests[QUEST_TIDE].progress = std::min(g.tideFound, TIDE_ISLANDS);
    }

    g.bossDefeated     = (b.flags & (1u << 0)) != 0;
    g.locketFound      = (b.flags & (1u << 1)) != 0;
    g.towerChestOpened = (b.flags & (1u << 2)) != 0;
    g.startVillageSeen = (b.flags & (1u << 3)) != 0;
    g.musicOn          = (b.flags & (1u << 4)) != 0;
    g.waitBattle       = (b.flags & (1u << 5)) != 0;
    g.crtOn            = (b.flags & (1u << 6)) != 0;
    g.hasBoat          = (b.flags & (1u << 7)) != 0;
    g.tideFound        = std::max(0, std::min(TIDE_ISLANDS, (int)((b.flags >> 8) & 3u)));
    g.sailing          = false;
    g.travelMap        = false;
    g.travelContext    = -1;
    g.hasWaystone      = (b.flags & (1u << 10)) != 0;
    g.frontierGuardian = (b.flags & (1u << 11)) != 0;
    g.legacyFarPass    = (b.flags & (1u << 13)) != 0;
    if (!(b.flags & (1u << 12))) {
        // Old games had no ranges. Never strand a returning party or take away
        // the route it already earned; this does not complete the Crown quest.
        int oldCountry = regionAt(g.px, g.py);
        if (oldCountry >= 1 || g.bossDefeated || itemCount(IT_STAR_METAL) > 0 ||
            g.quests[QUEST_DEEP].progress > 0 || g.quests[QUEST_DEEP].state == QS_DONE)
            g.hasWaystone = true;
        if (oldCountry >= 2 && !g.bossDefeated) g.legacyFarPass = true;
    }

    g.openedChests.clear();
    int oc = std::max(0, std::min(SAVE_MAX_IDS, (int)b.openedChestCount));
    for (int i = 0; i < oc; i++) g.openedChests.insert(b.openedChests[i]);

    g.defeatedMonsters.clear();
    int dm = std::max(0, std::min(SAVE_MAX_IDS, (int)b.defeatedMonsterCount));
    for (int i = 0; i < dm; i++) g.defeatedMonsters.insert(b.defeatedMonsters[i]);

    g.villageLoaded = false;
    g.villageKey = ~0ull;
    g.npcKey = ~0ull;
    g.npcs.clear();
    g.walkQueue.clear();
    g.hasMoveTarget = false;
    g.walkDoorBuilding = -1;
    g.walkTargetNpc = -1;
    g.walkTargetChest = 0;
    g.walkTargetInteract = false;
    g.encounterGrace = 3.0f;
    g.stepDist = 0.0f;
    parkSavedParty();   // a save can be read before the mode is set; be safe
    if (g.sailing) { g.sailing = false; }
    g.mode = MODE_OVERWORLD;
    if (g.hasBoat && cellSailable((int)floorf(g.px), (int)floorf(g.py))) {
        // The save was written afloat; parkSavedParty() has already moved the
        // party onto a bank, so this is only a belt-and-braces check.
        int x = (int)floorf(g.px), y = (int)floorf(g.py);
        nearestWalkable(x, y, x, y);
        if (cellWalkable(x, y)) { g.px = x + 0.5f; g.py = y + 0.5f; }
    }
    // Keep older saves playable if the new river, cliff or grove covers the
    // saved position. The save layout and quest/chest IDs haven't changed.
    villageLoadAt(g.px,g.py);
    if (positionBlocked(g.px,g.py,0.25f)) {
        int x=(int)floorf(g.px), y=(int)floorf(g.py);
        nearestWalkable(x,y,x,y);
        if (cellWalkable(x,y)) { g.px=x+0.5f; g.py=y+0.5f; }
        else { g.px=0.5f; g.py=4.5f; }
        villageLoadAt(g.px,g.py);
    }
    return true;
}

void newGameState(uint32_t seed) {
    g.worldSeed = seed;
    atlasResetRegion();
    g.px = 0.5f; g.py = 4.5f;      // just south of the Elmswick plaza
    g.facing = 0;
    g.sideFlip = false;
    g.runTime = 0;
    g.gil = 80;
    g.defeatedCount = 0;
    g.slimesSlain = 0;
    memset(g.inventory, 0, sizeof(g.inventory));
    g.inventory[IT_POTION] = 3;
    g.inventory[IT_ETHER] = 1;
    g.inventory[IT_ANTIDOTE] = 1;
    g.inventory[IT_PHOENIX] = 1;
    // "You are three, and you have one sword." — Aldric starts with the Bronze Sword.
    Hero base[3] = {
        { KLASS_KNIGHT, 1, 0, 42, 8,  42, 8,   7, 8,  8, 2, IT_SWORD_BRONZE, -1, -1, false, 0, false },
        { KLASS_MAGE,   1, 0, 26, 18, 26, 18,  6, 4,  7, 9, -1,              -1, -1, false, 1, false },
        { KLASS_CLERIC, 1, 0, 32, 16, 32, 16,  7, 6,  9, 6, -1,              -1, -1, false, 1, false },
    };
    memcpy(g.party, base, sizeof(base));
    for (int q = 0; q < QUEST_COUNT; q++) { g.quests[q] = Quest{}; }
    g.quests[QUEST_SLIMES] = { QS_OFFERED, 0, 6 };
    g.quests[QUEST_LOCKET] = { QS_OFFERED, 0, 1 };
    g.quests[QUEST_CROWN]  = { QS_OFFERED, 0, 1 };
    g.quests[QUEST_DEEP]   = { QS_OFFERED, 0, 1 };
    g.quests[QUEST_COMMONS] = { QS_OFFERED, 0, 1 };
    g.quests[QUEST_TIDE] = { QS_OFFERED, 0, TIDE_ISLANDS };

    g.defeatedMonsters.clear();
    g.openedChests.clear();
    g.villageLoaded = false;
    g.villageKey = ~0ull;
    g.npcKey = ~0ull;
    g.npcs.clear();
    g.hasMoveTarget = false;
    g.walkQueue.clear();
    g.walkDoorBuilding = -1;
    g.walkTargetNpc = -1;
    g.walkTargetChest = 0;
    g.walkTargetInteract = false;
    g.encounterGrace = 4.0f;
    g.stepCounter = 0;
    g.stepDist = 0.0f;
    g.bossDefeated = false;
    g.locketFound = false;
    g.towerChestOpened = false;
    g.hasWaystone = false;
    g.frontierGuardian = false;
    g.legacyFarPass = false;
    g.travelMap = false;
    g.travelContext = -1;
    g.startVillageSeen = false;
    g.hasBoat = false;
    g.sailing = false;
    g.tideFound = 0;
    g.surfT = 0;
    g.menuPage = 0;
    g.journalTab = 0;
    g.commonsJournalSel = 0;
    g.menuReturn = MODE_OVERWORLD;
    g.battleReturn = MODE_OVERWORLD;
    g.shopForge = false;
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
