// game.hpp — shared state and types for "A Place for Agents"
//
// An endless SNES-style JRPG.  The hero starts in a village, the world keeps
// generating around them, and everything is drawn with adapted OpenRTP art.
#pragma once

#include "raylib.h"
#include <string>
#include <vector>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <climits>
#include <cstdio>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>

constexpr int BASE_VW = 512;     // minimum virtual framebuffer dimensions (16:9 base)
constexpr int BASE_VH = 288;
constexpr int VRES_W = 512;      // reference 16:9 layout width
constexpr int VRES_H = 288;      // reference 16:9 layout height
constexpr int TILE = 16;

// ============================================================== items
// klassMask: bit0 knight, bit1 mage, bit2 cleric; 0 = any class may equip.
enum KlassMask { KM_ANY = 0, KM_KNIGHT = 1, KM_MAGE = 2, KM_CLERIC = 4 };
enum ItemId {
    // consumables
    IT_POTION = 0, IT_HIPOTION, IT_MEGAPOTION,
    IT_ETHER, IT_HIETHER, IT_ELIXIR,
    IT_ANTIDOTE, IT_REMEDY,
    IT_PHOENIX, IT_SMOKE_BOMB,
    // weapons — knightly steel
    IT_SWORD_BRONZE, IT_SWORD_IRON, IT_SWORD_STEEL,
    IT_SWORD_MYTHRIL, IT_SWORD_DRAGON,
    IT_WAR_AXE, IT_HALBERD,
    // weapons — arcane
    IT_STAFF_OAK, IT_ROD_IRON, IT_STAFF_SAGE,
    // weapons — devotional
    IT_MACE_BRASS, IT_MACE_HOLY,
    // weapons — light & ranged (any class)
    IT_DAGGER, IT_HUNTER_BOW,
    // armour
    IT_ARMOR_LEATHER, IT_ARMOR_CHAIN, IT_ARMOR_PLATE,
    IT_ARMOR_MYTHRIL, IT_ARMOR_DRAGON,
    IT_ROBE_SILK, IT_ROBE_ARCHMAGE,
    // shields & accessories
    IT_SHIELD_WOOD, IT_SHIELD_IRON, IT_SHIELD_KITE,
    IT_RING_POWER, IT_AMULET_MANA,
    IT_RING_WAR, IT_RING_SAGE, IT_AMULET_GUARD, IT_BOOTS_SWIFT,
    // key items
    IT_RUSTY_KEY, IT_LOCKET, IT_CROWN, IT_STAR_METAL, IT_CODEX,
    IT_COUNT
};
enum ItemKind { IK_CONSUMABLE, IK_WEAPON, IK_ARMOR, IK_ACCESSORY, IK_KEY };
struct ItemDef {
    const char* name;
    const char* desc;
    int price;
    ItemKind kind;
    int power;          // heal amount / attack or defence or magic bonus
    int icon;           // index into the loaded icon atlas
    bool sells;
    uint8_t klassMask;  // KM_* bits; 0 = any class
};

const ItemDef& itemDef(int id);

// ============================================================== heroes
enum Klass { KLASS_KNIGHT = 0, KLASS_MAGE = 1, KLASS_CLERIC = 2, KLASS_COUNT };
struct Hero {
    int klass = 0;
    int level = 1, xp = 0;
    int hp = 1, mp = 0, maxHp = 1, maxMp = 0;
    int atk = 1, def = 1, spd = 1, mag = 1;
    int weapon = -1, armor = -1, accessory = -1;
    bool ko = false;
    int row = 0;                       // 0 front, 1 back
    bool poisoned = false;
};
int  heroAtk(const Hero& h);
int  heroDef(const Hero& h);
int  heroMag(const Hero& h);
int  heroSpd(const Hero& h);
bool heroCanEquip(const Hero& h, int itemId);
int  xpToNext(const Hero& h);
const char* klassName(int klass);
const char* heroName(int klass);
int  levelUpCheck(Hero& h);
int  partyLevel();
bool equipHeroItem(int heroIdx, int slot, int itemId);
bool unequipHeroSlot(int heroIdx, int slot);

// ============================================================== spells
enum SpellSchool { SCH_NONE = 0, SCH_FIRE, SCH_STORM, SCH_HOLY, SCH_PHYS };
struct SpellDef {
    const char* name;
    int cost;
    int power;
    bool ally;       // targets the party
    bool aoe;
    int school;
    int level;       // hero level required
    int fx;          // 0 slash, 1 fireball, 2 spark, 3 sparkle, 4 heal
};
struct SpellList { const SpellDef* spells; int count; };
SpellList spellsFor(int klass, int level);

// ============================================================== enemies
enum EnemyKind {
    EN_SLIME = 0, EN_IMP, EN_BAT, EN_HORNET, EN_WISP, EN_WOLF, EN_SKELETON,
    EN_GHOST, EN_ORC, EN_SERPENT, EN_GOLEM, EN_NIGHT_RIDER, EN_SLIME_KING,
    // Additional OpenRTP field and battle sprites; keep the original IDs stable.
    EN_BANDIT, EN_GOBLIN, EN_REAPER, EN_FAERIE, EN_DRAKE, EN_NAGA, EN_SHADE,
    EN_DREAD_KNIGHT, EN_ARCHFIEND, EN_TIGER, EN_PORCUPINE,
    EN_COUNT
};
struct EnemyDef {
    const char* name;
    int hp, atk, def, spd, xp, gil;
    int weak;        // bitmask of SpellSchool it is weak to (1<<school)
    int drop;        // ItemId or -1
    int dropChance;  // percent
    int tier;        // 1..5, where it starts appearing
};
const EnemyDef& enemyDef(int kind);
EnemyKind rollBiomeEnemy(int tx, int ty, int zoneLevel);
std::vector<EnemyKind> buildEncounterGroup(EnemyKind primary, int tx, int ty, int zoneLevel);

// ============================================================== assets
enum IconId {
    ICON_POTION = 0, ICON_HIPOTION, ICON_ETHER, ICON_ELIXIR, ICON_FEATHER, ICON_FOOD,
    ICON_BOOK, ICON_CANDLE, ICON_GEM, ICON_COIN,
    ICON_SWORD0, ICON_SWORD1, ICON_SWORD2,
    ICON_ARMOR0, ICON_ARMOR1, ICON_ARMOR2, ICON_ARMOR3,
    ICON_SHIELD, ICON_RING, ICON_KEY, ICON_CROWN,
    // dungeon & equipment expansion
    ICON_STAFF, ICON_AXE, ICON_SPEAR, ICON_BOW, ICON_MACE,
    ICON_BOOTS, ICON_ROBE, ICON_REMEDY, ICON_BOMB, ICON_STAR,
    ICON_COUNT
};

enum Biome : uint8_t { BI_MEADOW, BI_FOREST, BI_AMBER, BI_FEN, BI_HEATH, BI_FROST, BI_DUNES, BI_COUNT };
enum MusicCue { MUS_NONE = -1, MUS_TITLE, MUS_VILLAGE, MUS_FIELD, MUS_WOOD, MUS_FEN,
    MUS_PASS, MUS_DUNES, MUS_HEARTH, MUS_CHAPEL, MUS_BATTLE, MUS_BOSS, MUS_VICTORY,
    MUS_GAMEOVER, MUS_COUNT };

enum DunTheme : uint8_t { TH_MOSSY=0, TH_ROOTED, TH_AMBER, TH_MIRE, TH_HEATH, TH_FROST, TH_SUNSCORCH, TH_CRYPT, TH_COUNT };
const char* dunThemeName(int theme);
MusicCue dunThemeMusic(int theme);

enum NpcSprite { NS_VILLAGER0, NS_VILLAGER1, NS_VILLAGER2, NS_VILLAGER3, NS_ELDER, NS_MAYOR,
    NS_WOMAN, NS_CHILD, NS_GUARD, NS_SOLDIER, NS_KNIGHT, NS_MERCHANT, NS_SMITH,
    NS_PRIEST, NS_FARMER, NS_DOG, NS_CAT, NS_CHICKEN, NS_SHEEP,
    NS_VILLAGER4, NS_VILLAGER5, NS_VILLAGER6, NS_VILLAGER7, NS_VILLAGER8, NS_VILLAGER9,
    NS_RANGER, NS_COW, NS_HORSE, NS_KEEPER, NS_FERRYMAN, NS_KIND_COUNT };

struct Assets {
    Texture2D landscape{}, relief{}, bridge{}, lilies{}, turnips{};
    // terrain
    Texture2D brickFloor[2]{};
    Texture2D plankFloor{}, tileFloor{}, stoneFloor{}, carpet{}, stoolTex{};
    Texture2D tuft[3]{}, flowers[2]{}, pebble{};
    // dungeon floors — one per theme plus shared overlays (tiles/ 16×16, validator-legal)
    Texture2D caveFloor[4]{}, caveRock[4]{}, caveFormation[6]{};
    Texture2D dunFloor[TH_COUNT]{};
    Texture2D dunWall[TH_COUNT]{};
    Texture2D dungeonFloor{}, flagstoneFloor{}, cobbleFloor{}, sandFloor{}, tundraFloor{}, iceFloor{}, snowFloor{}, swampFloor{}, arcaneFloor{}, dirtFloor{};
    Texture2D iceCrack{};
    // objects
    Texture2D tree[3]{}, treeSnow{}, pine[3]{}, deadTree{}, amberOak{}, copperOak{}, willow{}, reeds{}, yucca{}, crate{};
    Texture2D rock[3]{}, boulder{}, crystal{}, crystalCluster{};
    Texture2D bush[2]{}, mushroom{}, plant{}, urn{};
    Texture2D well{}, sign{}, bench{}, torch{}, campfire{}, cauldron{}, bucket{}, flowerbed{};
    Texture2D fence{}, fencePost{}, fenceGate{}, ladder{};
    Texture2D statue{}, pillar{}, ruinShrine{}, ruinArch{}, tent{}, statusStone[3]{};
    Texture2D barrel{}, table{}, longtable{}, counter{}, lantern{}, candle{}, keg{}, plate{}, shelf{};
    Texture2D forgeHearth{}, forgeAnvil{};
    // the Tidemill Isles: a stone basin with a standing stone behind it
    Texture2D tidewell{}, tidewellDry{}, tideStone{};
    Texture2D mailbox{}, noticeboard{};
    Texture2D chestClosed{}, chestOpen{}, stump{}, bed{};
    Texture2D cloud{};
    // buildings
    Texture2D house[5]{}, shop{}, inn{}, temple{}, manor{}, castle{}, tower{}, gate{}, smithy{};
    // dungeon entrance & stairs
    Texture2D caveMouth{}, stairDown{}, stairUp{};
    // the ferry's boat (src/sail.cpp) — native 72x96 character sheet, blue hull
    Texture2D boat{};
    // characters
    Texture2D heroWalk[3]{};
    Texture2D npc[NS_KIND_COUNT]{};
    Texture2D face[32]{};          // portraits addressed by index
    Texture2D battlefield[BI_COUNT]{}, castleBattlefield{};
    Texture2D enemy[EN_COUNT]{}, enemyBig[EN_COUNT]{};
    Texture2D icon[ICON_COUNT]{};
    // ui
    Texture2D window{}, windowBody{};
    Texture2D titleBg{}, titleAtlas{}, gameoverPlate{}, dawn{}, night{};
    // fx
    Texture2D fxSlash{}, fxSpark{}, fxSparkle{}, fxImpact{}, fxPoof{}, fxFireball{}, fxArrow{};
    Font font8{}, font16{}, font24{}, font32{};
    Sound cursor{}, confirm{}, cancel{}, sword{}, hit{}, spell{}, heal{}, coin{},
          potion{}, door{}, encounter{}, levelup{}, flee{}, chime{}, treasure{};
    Music score[MUS_COUNT]{};
    bool ok = false;
};
bool loadAllAssets();

// face indices
enum FaceId {
    FACE_KNIGHT = 0, FACE_MAGE, FACE_CLERIC, FACE_KNIGHT_BIG,
    FACE_VILLAGER0, FACE_VILLAGER1, FACE_VILLAGER2, FACE_VILLAGER3,
    FACE_ELDER, FACE_MAYOR, FACE_WOMAN, FACE_CHILD,
    FACE_SMITH, FACE_BARD, FACE_SHOPKEEPER, FACE_INNKEEPER, FACE_GUARD, FACE_RANGER,
    FACE_SLIME, FACE_GHOST, FACE_SKELETON, FACE_DEMON, FACE_DOG,
    FACE_VILLAGER4, FACE_VILLAGER5, FACE_VILLAGER6, FACE_VILLAGER7, FACE_VILLAGER8, FACE_VILLAGER9,
    FACE_KEEPER,
    FACE_COUNT
};

// ============================================================== world
enum GroundKind : uint8_t {
    G_GRASS = 0, G_GRASS_DARK, G_DIRT, G_SAND, G_WATER, G_DEEP, G_ROCK, G_SNOW,
    G_ICE, G_SWAMP, G_COBBLE, G_FLOOR, G_AMBER, G_HEATH, G_BEACH, G_SOIL,
    G_BRIDGE_NS, G_BRIDGE_EW, G_LEAVES, G_RIPPLES, G_COUNT
};
struct GroundTile {
    Biome biome = BI_MEADOW;
    GroundKind kind = G_GRASS;
    uint8_t variant = 0;
    uint8_t deco = 0;      // 0 none, 1 tuft, 2 flowers, 3 pebbles, 4 lilies, 5 crops
    uint8_t decoVariant = 0;
};
GroundTile sampleGround(int tx, int ty);

// props that live on the ground layer, y-sorted with the player
enum PropKind : uint8_t {
    P_NONE = 0, P_TREE0, P_TREE1, P_TREE2, P_PINE0, P_PINE1, P_PINE2, P_DEADTREE,
    P_TREE_SNOW, P_ROCK0, P_ROCK1, P_ROCK2, P_BOULDER, P_CRYSTAL, P_BUSH, P_BUSH_PLAIN,
    P_MUSHROOM, P_PLANT, P_URN, P_STUMP, P_AMBER_OAK, P_COPPER_OAK, P_REEDS, P_YUCCA, P_WILLOW, P_COUNT
};
struct WorldProp { PropKind kind = P_NONE; uint8_t variant = 0; };

struct Village;
struct Building {
    int kind = 0;         // 0..4 house variants, 5 shop, 6 inn, 7 temple, 8 manor,
                          // 9 castle (landmark use), 10 smithy/forge
    int tx = 0, ty = 0;   // anchor tile: footprint bottom-centre, door below
    int w = 4, h = 3;     // footprint in tiles
    int sprite = 0;       // texture selector
};

struct Village {
    bool valid = false;
    int sectorX = 0, sectorY = 0;
    uint32_t seed = 0;
    int cx = 0, cy = 0;                 // plaza centre in tiles
    std::string name;
    std::vector<Building> buildings;
    struct Deco { int kind; float x, y; int variant; };
    std::vector<Deco> decos;
    std::vector<std::pair<int, int>> paths;               // road tiles (dirt)
    std::vector<std::pair<int, int>> plaza;               // cobbled tiles
    std::vector<std::pair<int, int>> court;               // cut-stone flagstones
    std::vector<std::pair<int, int>> grassDark;           // shaded woodland/orchard turf
    std::vector<std::pair<int, int>> soil;                // tilled garden rows (with crops)
    std::vector<std::pair<int, int>> flowers;             // village flower patches
    std::vector<std::pair<int, int>> rocks;               // cliff/escarpment cells framing village
    bool startVillage = false;
};

void villageLoadAt(float x, float y);
bool inVillage(float x, float y);
bool nearStartVillage(float x, float y);

// collision + navigation
bool cellWalkable(int tx, int ty);
bool cellSailable(int tx, int ty);   // shallow water: a hull may cross it
bool positionBlocked(float x, float y, float r);
struct WalkPath {
    std::vector<std::pair<int, int>> cells;
    int doorBuilding = -1;
    bool reachable = false;
    void clear() { cells.clear(); doorBuilding = -1; reachable = false; }
};
bool findWalkPath(float fromX, float fromY, int toTx, int toTy, WalkPath& out);
void nearestWalkable(int tx, int ty, int& outTx, int& outTy);

// npcs
enum NpcRole { NPC_VILLAGER, NPC_ELDER, NPC_CHILD, NPC_GUARD, NPC_SHOPKEEP, NPC_INNKEEP,
               NPC_PRIEST, NPC_ANIMAL, NPC_QUEST, NPC_KEEPER };
struct Npc {
    uint64_t id = 0;
    int sprite = 0;         // index into assets.npc
    int face = FACE_VILLAGER0;
    float x = 0, y = 0;     // tile units (tile center)
    float homeX = 0, homeY = 0;
    float dx = 0, dy = 0;
    float thinkT = 0;
    int facing = 0;         // 0 down, 1 up, 2 side
    bool sideFlip = false;
    float animT = 0;
    bool moving = false;
    int role = NPC_VILLAGER;
    int quest = -1;
    std::string name;
    int lineSet = 0;
};
void npcsLoadFor(float x, float y, bool force = false);
int  npcNear(float x, float y, float radius, float* outDist = nullptr);
const char* npcLine(const Npc& npc, int index);

// chests
struct Chest {
    uint64_t id = 0;
    int tx = 0, ty = 0;
    int item = -1, amount = 1, gil = 0;
};
void chestsLoadFor(float x, float y);
extern std::vector<Chest> gChestsInView;

// landmarks
enum LandmarkKind { LK_NONE = 0, LK_RUINS, LK_TOWER, LK_CASTLE, LK_STONES, LK_CAMP, LK_CAVERN };
struct Landmark { LandmarkKind kind = LK_NONE; int tx = 0, ty = 0; uint32_t seed = 0; };
bool landmarkNear(int tx, int ty, Landmark& out);
bool inBossArena(float x, float y);

// dungeon entrances (world.cpp) — deterministic cavern mouths in the wilds
bool startCavernSite(int& outTx, int& outTy);              // Hollow Hill, always present

// field monsters
struct FieldMonster {
    uint64_t id = 0;
    EnemyKind kind = EN_SLIME;
    float x = 0, y = 0;
    float anchorX = 0, anchorY = 0, phase = 0;
};
void fieldMonstersInView(float x, float y, std::vector<FieldMonster>& out);
int  zoneLevelAt(float x, float y);
const char* zoneNameAt(float x, float y);
float moistureAt(float x, float y);

// noise helpers
uint32_t hash2i(int x, int y, uint32_t seed);
float hashf(int x, int y, uint32_t seed);
float smoothNoise(float x, float y, uint32_t seed);
float fbm(float x, float y, uint32_t seed, int octaves);

// ============================================================== quests
enum QuestId { QUEST_SLIMES = 0, QUEST_LOCKET, QUEST_CROWN, QUEST_DEEP, QUEST_COMMONS, QUEST_TIDE, QUEST_COUNT };
enum QuestState { QS_UNKNOWN = 0, QS_OFFERED, QS_ACTIVE, QS_DONE };
struct Quest {
    int state = QS_UNKNOWN;
    int progress = 0;
    int goal = 0;
};
const char* questTitle(int id);
const char* questGoalText(int id);

// ============================================================== interior
enum RoomKind { ROOM_SHOP = 0, ROOM_INN, ROOM_TEMPLE, ROOM_HOUSE, ROOM_FORGE };
struct RoomProp { int tex; float x, y; bool solid; float w, h; };
struct Room {
    RoomKind kind = ROOM_SHOP;
    int w = 15, h = 11;
    int buildingIndex = -1;
    std::string title;
    std::vector<RoomProp> props;
    float exitX = 7.5f, exitY = 9.0f;
    float npcX = 7.5f, npcY = 3.6f;
};
void roomBuild(Room& room, const Building& b, const Village& v);
int  roomPropNear(const Room& room, float x, float y, float r);
bool roomBlocked(const Room& room, float x, float y, float r);

// ============================================================== modes
enum Mode {
    MODE_TITLE = 0, MODE_OVERWORLD, MODE_BATTLE, MODE_MENU, MODE_DIALOG,
    MODE_ROOM, MODE_GAMEOVER, MODE_INTRO, MODE_DUNGEON, MODE_SAIL
};

// ============================================================== dialog
struct DialogLine {
    int face = -1;
    std::string speaker;
    std::string text;
};
enum DialogAction {
    DLG_NONE = 0, DLG_SHOP, DLG_INN, DLG_TEMPLE, DLG_START_QUEST_SLIMES,
    DLG_FINISH_QUEST_SLIMES, DLG_START_QUEST_LOCKET, DLG_FINISH_QUEST_LOCKET,
    DLG_START_QUEST_CROWN, DLG_GIVE_KEY, DLG_HEAL, DLG_CROWN_REWARD,
    DLG_BOSS_SLIME_KING, DLG_SAVE_TIP,
    DLG_START_QUEST_DEEP, DLG_FINISH_QUEST_DEEP,
    DLG_START_QUEST_COMMONS, DLG_FINISH_QUEST_COMMONS,
    DLG_BUY_BOAT, DLG_SAIL_TIDE
};
void startDialog(const std::vector<DialogLine>& lines, DialogAction action = DLG_NONE);

// ============================================================== context
struct Game {
    Mode mode = MODE_TITLE;
    Assets assets;
    RenderTexture2D rt{};
    RenderTexture2D upscaleRt{};
    int vw = BASE_VW;              // active virtual framebuffer width (>= 512)
    int vh = BASE_VH;              // active virtual framebuffer height (>= 288)
    int winRestoreW = 1024;
    int winRestoreH = 576;
    int winRestoreX = 80;
    int winRestoreY = 60;
    bool borderlessFs = false;

    uint32_t worldSeed = 0;
    float runTime = 0;             // seconds of play this run
    float totalTime = 0;           // monotonic, for animations

    // party & inventory
    Hero party[3];
    int gil = 0;
    int inventory[IT_COUNT]{};
    int defeatedCount = 0;
    int slimesSlain = 0;
    Quest quests[QUEST_COUNT];

    // player on the field
    float px = 0, py = 0;          // tile units (centre of the hero)
    int facing = 0;                // 0 down, 1 up, 2 side
    bool sideFlip = false;
    bool moving = false;
    float walkAnimT = 0;
    float encounterGrace = 0;
    int stepCounter = 0;
    float stepDist = 0.0f;
    std::vector<std::pair<int, int>> walkQueue;
    int walkDoorBuilding = -1;
    int walkTargetNpc = -1;
    uint64_t walkTargetChest = 0;
    bool walkTargetInteract = false;
    bool hasMoveTarget = false;
    float clickFlashT = 0, clickFlashX = 0, clickFlashY = 0;
    float blockedFx = 0;
    float interactT = 0;           // brief hop when interacting

    // village cache
    Village village;
    bool villageLoaded = false;
    uint64_t villageKey = ~0ull;
    bool startVillageSeen = false;

    // npc cache
    uint64_t npcKey = ~0ull;
    std::vector<Npc> npcs;

    // ---- the Open Water (src/sail.cpp) --------------------------------------
    // The boat is a ferryman's promise, not an item: once he has been paid you
    // can board at any dock. Boarding swaps the whole overworld for a sail on
    // the water; stepping onto a beach swaps it back.
    bool hasBoat = false;          // the ferryman has sold you the boat (flag bit7)
    bool sailing = false;          // true while the party is afloat
    float boatTrail = 0;           // seconds since the last landing (for the wake)
    int tideFound = 0;             // islands' Tidewells found, 0..3 (quest progress)
    int tideMask = 0;              // bit i = isle i's Tidewell has been lit (not saved: rebuildable)
    float surfT = 0;               // animation clock for the shoreline

    // chests & world flags
    std::unordered_set<uint64_t> openedChests;
    std::unordered_set<uint64_t> defeatedMonsters;
    bool bossDefeated = false;
    bool locketFound = false;
    bool towerChestOpened = false;
    // Kept in unused bits of the existing save flags, not in a new save row.
    bool hasWaystone = false;       // Hollow Hill's guardian opened the inner passes
    bool frontierGuardian = false; // the Far Marches' guardian has been beaten
    bool legacyFarPass = false;     // an older save already stood beyond the Crown range
    bool travelMap = false;        // transient miniature view of the same overworld
    int travelContext = -1;        // -1 unset, 0 place close-up, 1 between places

    // interior
    Room room;
    float roomPx = 0, roomPy = 0;
    int roomFacing = 1;
    bool roomSideFlip = false;
    bool roomMoving = false;
    float roomAnimT = 0;
    bool roomHasTarget = false;
    float roomTargetX = 0, roomTargetY = 0;
    bool roomClickTalk = false;
    bool roomClickExit = false;
    int roomNpcSprite = 0;
    int roomNpcFace = FACE_SHOPKEEPER;
    std::string roomNpcName;

    // dialog
    std::vector<DialogLine> dialog;
    int dialogIndex = 0;
    float dialogReveal = 0;
    int dialogLastBlip = -1;
    DialogAction dialogAction = DLG_NONE;
    Mode dialogReturn = MODE_OVERWORLD;

    // shop
    int shopTab = 0;               // 0 buy, 1 sell
    int shopIndex = 0;
    int shopScroll = 0;
    std::string shopTitle;
    std::string innTitle;
    bool shopForge = false;        // true = smithy stock (equipment only)

    // inn
    int innCost = 20;
    int innChoice = 0;             // 0 rest, 1 leave

    // menu
    Mode menuReturn = MODE_OVERWORLD;
    int menuPage = 0;              // 0 party, 1 items, 2 equip, 3 quests, 4 options, 5 shop, 6 inn
    int menuIndex = 0;
    int menuItem = -1;
    int menuUseHero = 0;
    int equipHero = 0, equipSlot = 0, equipIndex = 0;
    int optionsIndex = 0;
    int journalTab = 0;            // Quests page: 0 = quests, 1 = the Commons, 2 = the Survey
    std::string regionName;        // name of the country underfoot (the Survey names it)
    float regionBannerT = 0;       // seconds left on the region banner
    int commonsJournalSel = 0;     // first chronicle row shown on the Commons page
    float commonsFlameT = 0;       // transient: flame lighting after the keeper's errand
    MusicCue audition = MUS_VILLAGE;  // sound room only; never part of a save
    bool waitBattle = true;        // true = ATB pauses in menus (WAIT), false = ACTIVE
    bool musicOn = true;
    bool crtOn = false;            // optional, restrained CRT treatment over the pixel-perfect image

    // title
    int titleIndex = 0;
    float titleT = 0;
    bool saveExists = false;

    // camera & effects
    float camX = 0, camY = 0;
    // The field is framed as map pieces (src/pieces.cpp): the camera sits on the
    // piece the party is in and slides to the next when they cross a seam.
    int camPieceX = INT_MIN, camPieceY = INT_MIN;
    float camSlideT = 0.0f, camSlideDur = 0.0f;
    float camSlideFromX = 0.0f, camSlideFromY = 0.0f;
    Mode battleReturn = MODE_OVERWORLD;   // where a battle sends you on victory
    float shakeT = 0, shakeMag = 0;
    float fade = 0;
    int fadeDir = 0;
    bool quitRequested = false;

    // clouds
    float cloudX[6]{}, cloudY[6]{}, cloudScale[6]{};

    // toast
    std::string toast;
    float toastT = 0;

    // intro
    float introT = 0;

    bool showHelp = true;
    bool debugMode = false;
    int hotButtons = 0;            // reset each frame; >0 suppresses click-to-move
};

extern Game g;
extern float gAnimTime;
Vector2 getVirtualMouse();
Rectangle getPresentationRect();
void toggleFullscreenClean();
void pushToast(const std::string& msg);
void shakeScreen(float mag, float t);
void returnToTitle();
void loadScore(const std::string& dir);
void updateScore(float dt);
MusicCue playingCue();
const char* cueTitle(MusicCue cue);
void unloadScore();

// ============================================================== the Open Water
// world.cpp owns the shoreline; sail.cpp owns everything that happens on it.
float shoreRadius();                       // this seed's mean distance to the sea
bool  inOpenSea(float x, float y);         // past the coast: deep, and no hull goes there
struct Dock {
    bool valid = false;
    int tx = 0, ty = 0;      // the plank the party stands on
    int wx = 0, wy = 0;      // the first water tile the boat slips into
};
bool dockFind(bool sailing, Dock& out);    // nearest dock; afloat = nearest harbour
constexpr int TIDE_ISLANDS = 3;            // islands that carry a Tidewell
struct TideIsle { int tx = 0, ty = 0; bool well = false; };
void tideIslands();
int  tideIslandCount();
bool tideIslandAt(int i, TideIsle& out);
const char* tideIslandName(int i);
float tideIslandWellX(int i), tideIslandWellY(int i);

// sail.cpp
bool sailCanBoard();                       // standing beside open water, boat in hand
void sailEnter();                          // push off from the nearest bank
void sailExit();                           // step ashore onto the nearest beach
void sailUpdate(float dt);                 // movement, wake, landings, encounters
void sailDraw();                           // the water, the boat, the shore
void sailHud();                            // bow marker + landing prompt
bool sailPrompt(const char** text);        // "SPACE: land at ..." style prompt
void sailInteract();                       // land, or found a Tidewell
bool tideTryInteract();                    // light a Tidewell the party is standing at
bool tidePrompt(const char** text);
const char* sailZoneName();                // the name of the water under the hull
extern float gSailX, gSailY, gSailFacing;

// ============================================================== ui helpers
void drawWindow(Rectangle r, bool dimInside = true);
void drawWindowBlue(Rectangle r);
// One UI finish (docs/ART_BIBLE.md A6): square-corner navy panel with a
// 1px dark outline and a 1px inner border — gold when selected, steel-blue
// otherwise. This is the ONLY panel primitive for menus/buttons/cards;
// rounded/pill/glass drawing is banned (no DrawRectangleRounded* in src/).
void drawPixelPanel(Rectangle r, Color fill, Color border);
void drawPixelPanel(Rectangle r, bool selected);
void drawText(const char* text, float x, float y, Color c, int size = 8);
void drawTextCentered(const char* text, float cx, float y, Color c, int size = 8);
void drawTextWrapped(const char* text, float x, float y, float maxW, float lineH, Color c, int size = 8);
void drawWorldLabel(const char* text, float cx, float y, Color c, int size = 8);
Vector2 measureText(const char* text, int size = 8);
void drawBar(float x, float y, float w, float h, float frac, Color fill, Color back);
void drawItemIcon(int itemId, float x, float y, float size = 16);
std::string formatGil(int gil);
void drawButton(const char* text, Rectangle r, int size = 8, bool selected = false);
bool uiButtonAt(const char* text, Rectangle r, int size = 8);
bool uiButtonHit(Rectangle r);
// SNES menu cursor: a small right-pointing triangle, blinking like the
// originals. Drawn just left of the selected row.
void drawCursor(float x, float y, Color c = Color{ 255, 232, 150, 255 });

// ============================================================== save / load
bool saveGame();
bool loadGame();
bool saveExists();
void refreshSaveExists();
void newGameState(uint32_t seed);
bool applyConsumable(int itemId, int heroIdx);
void addItem(int itemId, int amount, bool silent = false);
bool takeItem(int itemId, int amount = 1);
int  itemCount(int itemId);
void startNewAdventure();

// ============================================================== quest helpers
void questSetState(int id, int state);
bool questActive(int id);
void questAdvance(int id, int amount = 1);

// ============================================================== module entry points
// overworld.cpp
void overworldUpdate(float dt);
void overworldSnapCamera();
void overworldInteract();
void pulseBlocked();
const char* interactionPrompt();
extern std::vector<FieldMonster> gFieldMonsters;

// render.cpp
void updateClouds(float dt);
void drawWorld();
void drawFieldMonsters();
void drawHUD();
void drawDialogBox();
void drawTitleScreen();
void drawGameOverScreen();
void drawIntroScreen();
void drawMenuScreen();
void drawToast();
void drawTransitionFx();

// dialog.cpp
void dialogUpdate(float dt);
void resolveDialogAction(DialogAction a);
void menuUpdate(float dt);

// room.cpp
void enterBuilding(int buildingIndex);
void exitRoom();
void roomUpdate(float dt);
void roomDraw();

// dungeon.cpp — procedurally generated cavern dungeons
void enterDungeon(int siteTx, int siteTy, bool startCavern);
void exitDungeon();
void dungeonUpdate(float dt);
void dungeonDraw();
const char* dungeonPrompt();          // bottom interaction hint, or nullptr
bool dungeonActive();                 // true while MODE_DUNGEON state is live
void dungeonLeaveToOverworld();       // climb out (restores field position)
int dungeonTheme();                   // TH_* or -1 when not in a dungeon
const char* dungeonThemeName();       // e.g. "Mossy Warren" for HUD
const char* dungeonFullName();        // e.g. "Gloam Warrens — Mire Grotto"
MusicCue dungeonMusicCue();            // theme-aware dungeon music

// commons.cpp — the Agents' Commons east of Elmswick (see src/commons.hpp)
bool commonsTryInteract();            // field interaction; true when consumed
bool inCommonsArea(float x, float y); // zone/overwrite for the HUD and music
void commonsTick(float dt);           // lighting + journal state, called each field frame

// sound helper
void playSoundIf(Sound& s);
