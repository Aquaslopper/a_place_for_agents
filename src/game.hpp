// game.hpp — shared state for "A Place for Agents"
// Everything the modes need lives in the global context `g`.
#pragma once

#include "raylib.h"
#include <string>
#include <vector>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>

constexpr int VRES_W = 512;
constexpr int VRES_H = 288;
constexpr int TILE = 16;

// ---------------------------------------------------------------- items
enum ItemId {
    IT_POTION = 0, IT_ETHER, IT_HIPOTION, IT_ELIXIR,
    IT_SWORD_BRONZE, IT_SWORD_IRON, IT_SWORD_MYTHRIL,
    IT_ARMOR_LEATHER, IT_ARMOR_CHAIN, IT_ARMOR_MYTHRIL,
    IT_COUNT
};
enum ItemKind { IK_CONSUMABLE, IK_WEAPON, IK_ARMOR };
struct ItemDef {
    const char* name;
    int price;
    ItemKind kind;
    int power;   // consumable: heal amount; equipment: bonus
    const char* desc;
};
const ItemDef& itemDef(int id);

// ---------------------------------------------------------------- heroes
enum Klass { KLASS_KNIGHT = 0, KLASS_MAGE = 1, KLASS_CLERIC = 2 };
struct Hero {
    int klass = 0;
    int level = 1, xp = 0;
    int hp = 1, mp = 0, maxHp = 1, maxMp = 0;
    int atk = 1, def = 1, spd = 1;
    int weapon = -1, armor = -1;   // ItemId or -1
    bool ko = false;
    // runtime ATB state lives in battle code
};
int heroAtk(const Hero& h);
int heroDef(const Hero& h);
int heroLevel(const Hero& h);          // == h.level
int xpToNext(const Hero& h);
const char* klassName(int klass);
const char* heroName(int klass);
void newGameState(uint32_t seed);
bool saveGame();
bool loadGame();
bool saveExists();
std::string saveFilePath();
bool loadAllAssets();
void unloadAllAssets();
int levelUpCheck(Hero& h);

// ---------------------------------------------------------------- enemies
enum EnemyKind { EN_SLIME = 0, EN_WOLF, EN_BAT, EN_WISP, EN_GOLEM, EN_COUNT };
struct EnemyDef {
    const char* name;
    int hp, atk, def, spd, xp, gil;
};
const EnemyDef& enemyDef(int kind);

// ---------------------------------------------------------------- assets
struct Assets {
    Texture2D grass[3]{}, tuft[3]{}, flower[3]{};
    Texture2D sand{}, dirt{}, snow{}, water[2]{}, rockHill{}, ice{}, cobble{};
    Texture2D tree[3]{}, pine[3]{}, deadTree{}, rock[3]{}, bush{}, well{}, sign{}, fence{}, mushroom{};
    Texture2D house[3]{}, shop{}, inn{}, castle{};
    Texture2D heroWalk[3]{};     // sheet 48x96 (cols idle/walkA/walkB rows down/up/side)
    Texture2D face[3]{};
    Texture2D enemy[EN_COUNT]{};
    Texture2D icon[IT_COUNT]{};
    Texture2D coinIcon{};
    Texture2D fxSlash{}, fxImpact{}, fxFireball{}, fxSpark{}, fxSparkle{}, fxPoof{};
    Texture2D titleBg{}, cloud{};
    Font font8{}, font16{}, font32{};
    Sound cursor{}, confirm{}, cancel{}, sword{}, hit{}, spell{}, heal{}, coin{},
          potion{}, door{}, encounter{}, levelup{}, flee{};
    Music title{}, field{}, battle{}, victory{}, gameover{};
    bool ok = false;
};

// ---------------------------------------------------------------- towns
struct HouseInst {
    int tx = 0, ty = 0;        // tile coords of house anchor (bottom-center)
    int variant = 0;           // 0..2 house sprite, -1 shop, -2 inn, -3 castle
    int role = 0;              // 0 house, 1 shop, 2 inn
};
struct Village {
    uint32_t sectorX = 0, sectorY = 0;
    uint32_t vseed = 0;
    float cx = 0, cy = 0;      // tile coords of plaza center
    std::string name;
    std::vector<HouseInst> houses;
    std::vector<std::pair<int,int>> wells, signs, fences, trees, bushes, mushrooms;
    bool valid = false;
};

// ---------------------------------------------------------------- modes
enum Mode {
    MODE_TITLE = 0,
    MODE_OVERWORLD,
    MODE_BATTLE,
    MODE_SHOP,
    MODE_INN,
    MODE_MENU,
    MODE_GAMEOVER,
    MODE_DIALOG
};

struct MenuAction { int id = -1; };

// ---------------------------------------------------------------- context
struct Game {
    Mode mode = MODE_TITLE;
    Mode returnAfterDialog = MODE_OVERWORLD;

    Assets assets;
    RenderTexture2D rt{};

    uint32_t worldSeed = 0;
    uint32_t runTime = 0;          // seconds of play (for day cycle & stats)

    // party
    Hero party[3];
    int gil = 0;
    int inventory[IT_COUNT]{};
    int defeatedCount = 0;

    // world position (tile units, float)
    float px = 0, py = 0;
    float moveTargetX = 0, moveTargetY = 0;   // mouse click-to-move target
    bool hasMoveTarget = false;
    int facing = 0;                // 0 down, 1 up, 2 side
    bool sideFlip = false;         // facing left
    bool moving = false;
    float walkAnimT = 0;
    float encounterGrace = 0;      // seconds after battle before next encounter
    int stepCounter = 0;

    // clouds
    float cloudX[6]{}; float cloudY[6]{}; float cloudScale[6]{};

    // toasts
    std::string toast; float toastT = 0;

    // menu state
    int menuIndex = 0;             // selected row in pause menu
    int menuPage = 0;              // 0 root, 1 items page uses itemsSelected
    int itemsIndex = 0;
    int useItemTarget = -1;        // choosing hero for item use
    int useItemId = -1;
    bool waitBattle = false;       // ATB Wait mode
    bool musicOn = true;
    float menuCooldown = 0;

    // dialog (village house / signs)
    std::string dialogTitle, dialogLine;
    bool dialogGift = false;

    // current village (recomputed on demand)
    Village village;
    bool villageLoaded = false;
    uint64_t villageSectorKey = ~0ull;

    // defeated field monsters (session only)
    std::unordered_set<uint64_t> defeatedMonsters;

    // title
    int titleIndex = 0;
    float titleT = 0;
    bool saveExists = false;

    // camera shake
    float shakeT = 0, shakeMag = 0;

    // fade transitions
    float fade = 0;                // 0 clear, 1 black
    int fadeDir = 0;               // -1 fade in, +1 fade out
    Mode fadeTarget = MODE_TITLE;

    // misc
    bool showHelp = true;
};

extern Game g;
Vector2 getVirtualMouse();
inline float gAnimTime = 0.0f;

// ---------------------------------------------------------------- helpers
bool applyConsumable(int itemId, int heroIdx);
void pushToast(const std::string& msg);
bool uiButton(const char* text, Rectangle r, int fontSize = 8);   // returns true on click
void drawPanel(Rectangle r);                                       // classic blue window
void drawDuskGradient(int w, int h, Color top, Color bottom);
std::string formatGil(int gil);
uint32_t hash2i(int x, int y, uint32_t seed);
float hashf(int x, int y, uint32_t seed);
float smoothNoise(float x, float y, uint32_t seed);
float fbm(float x, float y, uint32_t seed, int octaves);
