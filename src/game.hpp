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
#include <cstdio>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <vector>
#include <string>

constexpr int VRES_W = 512;      // virtual framebuffer, letterboxed to the window
constexpr int VRES_H = 288;
constexpr int TILE = 16;

// ============================================================== items
enum ItemId {
    IT_POTION = 0, IT_HIPOTION, IT_ETHER, IT_HIETHER, IT_ELIXIR, IT_ANTIDOTE, IT_PHOENIX,
    IT_SWORD_BRONZE, IT_SWORD_IRON, IT_SWORD_MYTHRIL, IT_SWORD_DRAGON,
    IT_ARMOR_LEATHER, IT_ARMOR_CHAIN, IT_ARMOR_MYTHRIL, IT_ARMOR_DRAGON,
    IT_SHIELD_WOOD, IT_SHIELD_IRON,
    IT_RING_POWER, IT_AMULET_MANA,
    IT_RUSTY_KEY, IT_CROWN,
    IT_COUNT
};
enum ItemKind { IK_CONSUMABLE, IK_WEAPON, IK_ARMOR, IK_ACCESSORY, IK_KEY };
struct ItemDef {
    const char* name;
    const char* desc;
    int price;
    ItemKind kind;
    int power;          // heal amount / attack or defence bonus
    int icon;           // index into the loaded icon atlas
    bool sells;
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
};
int  heroAtk(const Hero& h);
int  heroDef(const Hero& h);
int  heroMag(const Hero& h);
int  xpToNext(const Hero& h);
const char* klassName(int klass);
const char* heroName(int klass);
int  levelUpCheck(Hero& h);
int  partyLevel();

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
    EN_COUNT
};
struct EnemyDef {
    const char* name;
    int hp, atk, def, spd, xp, gil;
    int weak;        // bitmask of SpellSchool it is weak to (1<<school)
    int drop;        // ItemId or -1
    int dropChance;  // percent
    int tier;        // 1..4, where it starts appearing
};
const EnemyDef& enemyDef(int kind);
EnemyKind rollBiomeEnemy(int tx, int ty, int zoneLevel);

// ============================================================== assets
enum IconId {
    ICON_POTION = 0, ICON_HIPOTION, ICON_ETHER, ICON_ELIXIR, ICON_FEATHER, ICON_FOOD,
    ICON_BOOK, ICON_CANDLE, ICON_GEM, ICON_COIN,
    ICON_SWORD0, ICON_SWORD1, ICON_SWORD2,
    ICON_ARMOR0, ICON_ARMOR1, ICON_ARMOR2, ICON_ARMOR3,
    ICON_SHIELD, ICON_RING,
    ICON_COUNT
};

struct Assets {
    // terrain
    Texture2D grass[4]{}, grassTall{}, dirt{}, soil{}, sand{}, beach{}, cobble{}, flagstone{},
              rockHill[2]{}, brickFloor[2]{}, snow[2]{}, tundra{}, ice[2]{}, water[2]{},
              deepWater{}, swamp{}, dungeonFloor{};
    Texture2D plankFloor{}, tileFloor{}, stoneFloor{}, carpet{}, stoolTex{};
    Texture2D tuft[3]{}, flowers[2]{}, pebble{};
    // objects
    Texture2D tree[3]{}, treeSnow{}, pine[3]{}, deadTree{};
    Texture2D rock[3]{}, boulder{}, crystal{};
    Texture2D bush[2]{}, mushroom{}, plant{}, urn{};
    Texture2D well{}, sign{}, bench{}, torch{}, campfire{}, cauldron{}, bucket{}, flowerbed{};
    Texture2D fence{}, fencePost{}, fenceGate{}, ladder{};
    Texture2D statue{}, pillar{}, ruinShrine{}, ruinArch{};
    Texture2D barrel{}, table{}, counter{}, lantern{}, candle, keg{}, plate{}, shelf{};
    Texture2D chestClosed{}, chestOpen{}, jar{}, stump{}, bed{};
    Texture2D cloud{};
    // buildings
    Texture2D house[5]{}, shop{}, inn{}, temple{}, manor{}, castle{}, tower{}, gate{};
    // characters
    Texture2D heroWalk[3]{};
    Texture2D heroExtra[6]{};
    Texture2D npc[16]{};
    Texture2D face[32]{};          // portraits addressed by index
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
    Music title{}, field{}, battle{}, victory{}, gameover{}, village{};
    bool ok = false;
};
bool loadAllAssets();

// face indices
enum FaceId {
    FACE_KNIGHT = 0, FACE_MAGE, FACE_CLERIC, FACE_KNIGHT_BIG,
    FACE_VILLAGER0, FACE_VILLAGER1, FACE_VILLAGER2, FACE_VILLAGER3,
    FACE_ELDER, FACE_MAYOR, FACE_WOMAN, FACE_CHILD,
    FACE_SMITH, FACE_BARD, FACE_SHOPKEEPER, FACE_INNKEEPER, FACE_GUARD, FACE_RANGER,
    FACE_SLIME, FACE_GHOST, FACE_SKELETON, FACE_DEMON, FACE_WOLF,
    FACE_COUNT
};

// ============================================================== world
enum GroundKind : uint8_t {
    G_GRASS = 0, G_GRASS_DARK, G_DIRT, G_SAND, G_WATER, G_DEEP, G_ROCK, G_SNOW,
    G_ICE, G_SWAMP, G_COBBLE, G_FLOOR, G_COUNT
};
struct GroundTile {
    GroundKind kind = G_GRASS;
    uint8_t variant = 0;
    uint8_t deco = 0;      // 0 none, 1 tuft, 2 flowers, 3 pebble
    uint8_t decoVariant = 0;
};
GroundTile sampleGround(int tx, int ty);

// props that live on the ground layer, y-sorted with the player
enum PropKind : uint8_t {
    P_NONE = 0, P_TREE0, P_TREE1, P_TREE2, P_PINE0, P_PINE1, P_PINE2, P_DEADTREE,
    P_TREE_SNOW, P_ROCK0, P_ROCK1, P_ROCK2, P_BOULDER, P_CRYSTAL, P_BUSH, P_BUSH_PLAIN,
    P_MUSHROOM, P_PLANT, P_URN, P_STUMP, P_COUNT
};
struct WorldProp { PropKind kind = P_NONE; uint8_t variant = 0; };

struct Village;
struct Building {
    int kind = 0;      // 0..4 house variants, 5 shop, 6 inn, 7 temple, 8 manor
    int tx = 0, ty = 0;   // anchor tile: footprint bottom-centre, door below
    int w = 4, h = 3;     // footprint in tiles
    int sprite = 0;       // texture selector
};
struct VillageProp { Texture2D* tex = nullptr; float x = 0, y = 0; bool solid = false; float w = 0, h = 0; };

struct Village {
    bool valid = false;
    uint32_t sectorX = 0, sectorY = 0, seed = 0;
    int cx = 0, cy = 0;                 // plaza centre in tiles
    std::string name;
    std::vector<Building> buildings;
    struct Deco { int kind; float x, y; int variant; };   // see drawVillageDeco
    std::vector<Deco> decos;
    std::vector<std::pair<int, int>> paths;               // road tiles (dirt)
    std::vector<std::pair<int, int>> plaza;               // cobbled tiles
    bool startVillage = false;
};

void villageLoadAt(float x, float y);
bool inVillage(float x, float y);
bool nearStartVillage(float x, float y);

// collision + navigation
bool cellWalkable(int tx, int ty);
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
               NPC_PRIEST, NPC_ANIMAL, NPC_QUEST };
struct Npc {
    uint64_t id = 0;
    int sprite = 0;         // index into assets.npc
    int face = FACE_VILLAGER0;
    float x = 0, y = 0;     // tile units
    float homeX = 0, homeY = 0;
    float dx = 0, dy = 0;
    float thinkT = 0;
    int facing = 0;
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
enum LandmarkKind { LK_NONE = 0, LK_RUINS, LK_TOWER, LK_CASTLE, LK_STONES, LK_CAMP };
struct Landmark { LandmarkKind kind = LK_NONE; int tx = 0, ty = 0; uint32_t seed = 0; };
bool landmarkNear(int tx, int ty, Landmark& out);
bool inBossArena(float x, float y);

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
enum QuestId { QUEST_SLIMES = 0, QUEST_LOCKET, QUEST_CROWN, QUEST_COUNT };
enum QuestState { QS_UNKNOWN = 0, QS_OFFERED, QS_ACTIVE, QS_DONE };
struct Quest {
    int state = QS_UNKNOWN;
    int progress = 0;
    int goal = 0;
};
const char* questTitle(int id);
const char* questGoalText(int id);

// ============================================================== interior
enum RoomKind { ROOM_SHOP = 0, ROOM_INN, ROOM_TEMPLE, ROOM_HOUSE };
struct RoomProp { int tex; float x, y; bool solid; float w, h; };
struct Room {
    RoomKind kind = ROOM_SHOP;
    int w = 13, h = 9;
    int buildingIndex = -1;
    std::string title;
    std::vector<RoomProp> props;
    float exitX = 0, exitY = 0;      // door back to the overworld
};
void roomBuild(Room& room, const Building& b, const Village& v);
int  roomPropNear(const Room& room, float x, float y, float r);   // returns index or -1
bool roomBlocked(const Room& room, float x, float y, float r);

// ============================================================== modes
enum Mode {
    MODE_TITLE = 0, MODE_OVERWORLD, MODE_BATTLE, MODE_MENU, MODE_DIALOG,
    MODE_ROOM, MODE_GAMEOVER, MODE_INTRO
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
    DLG_GIVE_KEY, DLG_HEAL, DLG_CROWN_REWARD, DLG_SAVE_TIP
};
void startDialog(const std::vector<DialogLine>& lines, DialogAction action = DLG_NONE);

// ============================================================== context
struct Game {
    Mode mode = MODE_TITLE;
    Assets assets;
    RenderTexture2D rt{};

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
    std::vector<std::pair<int, int>> walkQueue;
    int walkDoorBuilding = -1;
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

    // chests
    std::unordered_set<uint64_t> openedChests;
    std::unordered_set<uint64_t> defeatedMonsters;
    bool bossDefeated = false;

    // interior
    Room room;
    float roomPx = 0, roomPy = 0;
    int roomFacing = 1;
    bool roomMoving = false;
    float roomAnimT = 0;
    int roomNpcSprite = 0;
    int roomNpcFace = FACE_SHOPKEEPER;
    std::string roomNpcName;

    // dialog
    std::vector<DialogLine> dialog;
    int dialogIndex = 0;
    float dialogReveal = 0;
    DialogAction dialogAction = DLG_NONE;
    Mode dialogReturn = MODE_OVERWORLD;

    // shop
    int shopTab = 0;               // 0 buy, 1 sell
    int shopIndex = 0;
    int shopEquipHero = -1;
    int shopEquipItem = -1;

    // inn
    int innCost = 20;

    // menu
    int menuPage = 0;              // 0 party, 1 items, 2 equip, 3 quests, 4 options
    int menuIndex = 0;
    int menuItem = -1;
    int menuUseHero = -1;
    int equipHero = 0, equipSlot = 0, equipIndex = 0;
    bool waitBattle = false;
    bool musicOn = true;

    // title
    int titleIndex = 0;
    float titleT = 0;
    bool saveExists = false;

    // camera & effects
    float camX = 0, camY = 0;
    float shakeT = 0, shakeMag = 0;
    float fade = 0;
    int fadeDir = 0;
    bool fadeToTitle = false;

    // clouds
    float cloudX[6]{}, cloudY[6]{}, cloudScale[6]{};

    // toast
    std::string toast;
    float toastT = 0;

    // intro
    float introT = 0;

    bool showHelp = true;
    bool debugMode = false;
    int hotButtons = 0;           // reset each frame; >0 suppresses click-to-move
};

extern Game g;
extern float gAnimTime;
Vector2 getVirtualMouse();
void pushToast(const std::string& msg);
void shakeScreen(float mag, float t);

// ============================================================== ui helpers
void drawWindow(Rectangle r, bool dimInside = true);
void drawWindowBlue(Rectangle r);
void drawText(const char* text, float x, float y, Color c, int size = 8);
void drawTextCentered(const char* text, float cx, float y, Color c, int size = 8);
Vector2 measureText(const char* text, int size = 8);
void drawBar(float x, float y, float w, float h, float frac, Color fill, Color back);
void drawItemIcon(int itemId, float x, float y, float size = 16);
std::string formatGil(int gil);
void drawButton(const char* text, Rectangle r, int size = 8);
bool uiButtonAt(const char* text, Rectangle r, int size = 8);

// ============================================================== save / load
bool saveGame();
bool loadGame();
bool saveExists();
void newGameState(uint32_t seed);
bool applyConsumable(int itemId, int heroIdx);
void addItem(int itemId, int amount);
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
void drawWorld();
void drawFieldMonsters();
void drawHUD();
void drawDialogBox();
void drawTitleScreen();
void drawGameOverScreen();
void drawIntroScreen();
void drawMenuScreen();
void drawShopScreen();
void drawToast();
void drawTransitionFx();

// dialog.cpp
void dialogUpdate(float dt);
void resolveDialogAction(DialogAction a);

// menu.cpp
void menuUpdate(float dt);

// room.cpp
void enterBuilding(int buildingIndex);
void exitRoom();
void roomUpdate(float dt);
void roomDraw();

// sound helper
void playSoundIf(Sound& s);
