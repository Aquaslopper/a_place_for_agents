// battle.cpp — Active-Time-Battle, SNES style, mouse & keyboard
#include "battle.hpp"
#include "world.hpp"
#include <cmath>
#include <cstring>
#include <cstdlib>

namespace {

enum BPhase {
    BP_INTRO = 0,
    BP_MAIN,
    BP_MENU,       // command menu open
    BP_MAGIC,      // spell submenu
    BP_ITEM,       // item submenu
    BP_TARGET_FOE, // pick an enemy
    BP_TARGET_ALLY,// pick a hero (heal/item)
    BP_VICTORY,
    BP_DEFEAT,
    BP_FLED
};

struct EnemySlot {
    EnemyKind kind = EN_SLIME;
    int hp = 1, maxHp = 1, atk = 1, def = 1, spd = 1;
    int xp = 1, gil = 1;
    Vector2 pos = { 0, 0 };
    float gauge = 0.0f;
    float bob = 0.0f;
    float hitT = 0.0f;      // flash / shake timer
    bool alive = true;
    float poofT = 0.0f;
};

struct Floater {
    Vector2 pos;
    float t = 0.0f;
    std::string text;
    Color color = WHITE;
};

struct PendingEnemy { int idx = -1; float t = 0.0f; };
struct Projectile { bool active = false; Vector2 from{}, to{}; float t = 0.0f, dur = 0.45f; int target = -1; };

// forward decls for menu/target flow
int firstAliveFoe();
int firstAliveAlly();
void cycleFoe(int dir);
void confirmFoeTarget();
void confirmAllyTarget();
void castSelectedSpell();
void useSelectedItem();
void useItemOnHero(int hero, int item, int unused);
int gPendingSpell = -1;
int gPendingItem = -1;
BPhase gTargetReturn = BP_MENU;

EnemySlot gEnemies[3];
int gEnemyCount = 0;
BPhase gPhase = BP_INTRO;
float gPhaseT = 0.0f;
float gHeroGauge[3] = { 0, 0, 0 };
bool gHeroDefending[3] = { false, false, false };
int gActing = -1;             // hero index whose menu is open, -1 none
int gTargetFoe = 0;
int gTargetAlly = 0;
int gMenuSel = 0;             // selection in current menu
int gMagicSel = 0;
int gItemSel = 0;
float gBannerT = 0.0f;
std::vector<Floater> gFloaters;
PendingEnemy gPending[4];
int gPendingCount = 0;
Projectile gFireball;
float gLunge[3] = { 0, 0, 0 };      // hero attack lunge timer
float gEnemyLunge = 0.0f; int gEnemyLungeIdx = -1;
float gHeroHitFlash = 0.0f;
float gHitFlash = 0.0f;             // full screen flash
float gTime = 0.0f;
int gExpGain = 0, gGilGain = 0;
int gLevelUps[3] = { 0, 0, 0 };
uint64_t gFieldMonsterId = 0;
bool gActive = false;

// ---------------------------------------------------------------- utils
Rectangle enemyRect(int i) {
    Texture2D& t = g.assets.enemy[(int)gEnemies[i].kind];
    float sc = 2.2f;
    float w = t.width * sc, h = t.height * sc;
    return { gEnemies[i].pos.x - w / 2, gEnemies[i].pos.y - h, w, h };
}
Rectangle heroPanelRect(int i) {
    float pw = 150.0f, ph = 54.0f, gap = 8.0f;
    float x = (VRES_W - (3 * pw + 2 * gap)) / 2.0f + i * (pw + gap);
    return { x, (float)VRES_H - ph - 3.0f, pw, ph };
}
Vector2 heroBattlePos(int i) {
    // heroes on the left, facing right
    const Vector2 spots[3] = { { 84, 150 }, { 46, 192 }, { 108, 200 } };
    return spots[i];
}
void addFloat(Vector2 pos, const std::string& text, Color c) {
    gFloaters.push_back({ pos, 0.0f, text, c });
}
void screenShake(float mag, float t) { g.shakeMag = mag; g.shakeT = t; }

float spdGaugeRate(int spd) { return 0.055f + spd * 0.0115f; }
int zoneDamageBonus() { return 0; }

struct SpellDef { const char* name; int cost; int power; bool ally; bool aoe; };
const SpellDef SPELLS_MAGE[] = {
    { "Fire",  8, 34, false, false },
    { "Spark", 5, 20, false, false },
};
const SpellDef SPELLS_CLERIC[] = {
    { "Heal",     6, 62, true,  false },
    { "Heal All", 14, 44, true,  true  },
};
const SpellDef SPELLS_KNIGHT[] = {
    { "Power Slash", 6, -1, false, false },   // -1 = physical 1.8x
};
const SpellDef* spellsFor(int klass, int& outCount) {
    if (klass == KLASS_MAGE)   { outCount = 2; return SPELLS_MAGE; }
    if (klass == KLASS_CLERIC) { outCount = 2; return SPELLS_CLERIC; }
    outCount = 1; return SPELLS_KNIGHT;
}

bool atbPausedForMenu() {
    return gPhase == BP_MENU || gPhase == BP_MAGIC || gPhase == BP_ITEM ||
           gPhase == BP_TARGET_FOE || gPhase == BP_TARGET_ALLY;
}

// ---------------------------------------------------------------- start
void battleStartImpl(const std::vector<EnemyKind>& kinds, int zoneLevel, uint64_t fieldMonsterId) {
    gFieldMonsterId = fieldMonsterId;
    gEnemyCount = 0;
    const float zl = (float)(zoneLevel - 1);
    std::vector<int> counts(EN_COUNT, 0);
    for (EnemyKind k : kinds) {
        if (gEnemyCount >= 3) break;
        int idx = gEnemyCount++;
        const EnemyDef& d = enemyDef((int)k);
        EnemySlot& e = gEnemies[idx];
        e.kind = k;
        e.maxHp = (int)(d.hp * (1.0f + 0.28f * zl)) + 4 * zoneLevel;
        e.hp = e.maxHp;
        e.atk = d.atk + (int)(2 * zl) + zoneLevel / 2;
        e.def = d.def + (int)(1 * zl);
        e.spd = std::max(3, d.spd + (int)(zl * 0.8f));
        e.xp = (int)(d.xp * (1.0f + 0.35f * zl)) + 2 * zoneLevel;
        e.gil = (int)(d.gil * (1.0f + 0.4f * zl)) + 3 * zoneLevel;
        e.gauge = hashf(idx, (int)fieldMonsterId, g.worldSeed ^ 0xc1a5) * 0.4f;
        e.bob = hashf(idx, 913, g.worldSeed) * 6.2831853f;
        e.hitT = 0; e.poofT = 0; e.alive = true;
        counts[(int)k]++;
    }
    // formation positions (feet anchor)
    const Vector2 formation[3][3] = {
        { { 330, 168 }, { 0, 0 }, { 0, 0 } },
        { { 268, 168 }, { 388, 158 }, { 0, 0 } },
        { { 218, 172 }, { 330, 160 }, { 430, 176 } },
    };
    for (int i = 0; i < gEnemyCount; i++) gEnemies[i].pos = formation[gEnemyCount - 1][i];

    gPhase = BP_INTRO; gPhaseT = 0.0f;
    gHeroGauge[0] = gHeroGauge[1] = gHeroGauge[2] = 0.15f;
    gHeroDefending[0] = gHeroDefending[1] = gHeroDefending[2] = false;
    gActing = -1; gMenuSel = 0; gMagicSel = 0; gItemSel = 0;
    gTargetFoe = 0; gTargetAlly = 0;
    gPendingCount = 0; gFireball.active = false;
    for (int i = 0; i < 3; i++) gLunge[i] = 0;
    gEnemyLunge = 0; gHeroHitFlash = 0; gHitFlash = 0;
    gFloaters.clear();
    gBannerT = 2.0f;
    gLevelUps[0] = gLevelUps[1] = gLevelUps[2] = 0;
    gExpGain = gGilGain = 0;
    gTime = 0.0f;
    gActive = true;
    g.mode = MODE_BATTLE;
    if (g.musicOn) PlayMusicStream(g.assets.battle);
    PlaySound(g.assets.encounter);
}

// ---------------------------------------------------------------- damage
int calcPhysDamage(int atk, int def, float varianceSeed) {
    float base = (float)atk * 2.0f - (float)def * 1.1f;
    float variance = 0.85f + varianceSeed * 0.3f;
    int dmg = (int)(base * variance);
    return std::max(1, dmg);
}

void damageEnemy(int idx, int dmg, bool crit) {
    EnemySlot& e = gEnemies[idx];
    e.hp -= dmg;
    e.hitT = 0.35f;
    addFloat({ e.pos.x, e.pos.y - 60 }, std::to_string(dmg), crit ? Color{ 255, 224, 120, 255 } : WHITE);
    if (e.hp <= 0) {
        e.hp = 0; e.alive = false; e.poofT = 0.8f;
        addFloat({ e.pos.x, e.pos.y - 84 }, "DOWN", Color{ 200, 210, 255, 255 });
    }
}

void damageHero(int idx, int dmg) {
    Hero& h = g.party[idx];
    if (gHeroDefending[idx]) dmg = std::max(1, dmg / 2);
    h.hp -= dmg;
    gHeroHitFlash = 0.3f;
    Vector2 pos = heroBattlePos(idx);
    addFloat({ pos.x, pos.y - 64 }, std::to_string(dmg), Color{ 255, 150, 120, 255 });
    if (h.hp <= 0) {
        h.hp = 0; h.ko = true;
        addFloat({ pos.x, pos.y - 88 }, "KO", Color{ 255, 90, 90, 255 });
    }
}

// ---------------------------------------------------------------- turns
void endHeroTurn(int idx) {
    gHeroGauge[idx] = 0.0f;
    gHeroDefending[idx] = false;   // defend lasts until your next action
    gActing = -1;
    gPhase = BP_MAIN;
}

void checkBattleEnd() {
    bool anyFoe = false;
    for (int i = 0; i < gEnemyCount; i++) if (gEnemies[i].alive) anyFoe = true;
    if (!anyFoe) {
        // compute rewards
        int xp = 0, gil = 0;
        for (int i = 0; i < gEnemyCount; i++) { xp += gEnemies[i].xp; gil += gEnemies[i].gil; }
        xp /= 1;  // full share per hero
        gExpGain = xp; gGilGain = gil;
        g.gil += gil;
        for (int i = 0; i < 3; i++) {
            if (g.party[i].ko) continue;
            g.party[i].xp += xp;
            gLevelUps[i] = levelUpCheck(g.party[i]);
        }
        g.defeatedCount += gEnemyCount;
        if (gFieldMonsterId != 0) g.defeatedMonsters.insert(gFieldMonsterId);
        gPhase = BP_VICTORY; gPhaseT = 0.0f;
        StopMusicStream(g.assets.battle);
        if (g.musicOn) PlayMusicStream(g.assets.victory);
        return;
    }
    bool anyHero = false;
    for (auto& h : g.party) if (!h.ko) anyHero = true;
    if (!anyHero) {
        gPhase = BP_DEFEAT; gPhaseT = 0.0f;
        StopMusicStream(g.assets.battle);
        if (g.musicOn) PlayMusicStream(g.assets.gameover);
    }
}

void heroAttack(int heroIdx, int foeIdx) {
    Hero& h = g.party[heroIdx];
    float roll = hashf((int)gTime * 1000, heroIdx, g.worldSeed);
    bool crit = roll < 0.12f;
    int dmg = calcPhysDamage(heroAtk(h), gEnemies[foeIdx].def, hashf(gEnemies[foeIdx].hp, heroIdx, (uint32_t)gTime));
    if (crit) dmg = (int)(dmg * 1.6f);
    gLunge[heroIdx] = 0.32f;
    PlaySound(g.assets.sword);
    damageEnemy(foeIdx, dmg, crit);
    gHitFlash = std::max(gHitFlash, crit ? 0.35f : 0.18f);
    screenShake(crit ? 4.0f : 2.0f, 0.25f);
    checkBattleEnd();
    if (gPhase == BP_MAIN) endHeroTurn(heroIdx);
}

void heroCast(int heroIdx, int spellIdx, int targetIdx /* foe or ally */) {
    Hero& h = g.party[heroIdx];
    int spellCount = 0;
    const SpellDef* spells = spellsFor(h.klass, spellCount);
    if (spellIdx < 0 || spellIdx >= spellCount) { gPhase = BP_MAIN; return; }
    const SpellDef& sp = spells[spellIdx];
    if (h.mp < sp.cost) {
        addFloat(heroBattlePos(heroIdx), "Not enough MP!", Color{ 150, 190, 255, 255 });
        gPhase = BP_MENU;
        return;
    }
    h.mp -= sp.cost;

    if (h.klass == KLASS_KNIGHT) {
        // Power Slash — physical
        gLunge[heroIdx] = 0.36f;
        PlaySound(g.assets.sword);
        int dmg = (int)(heroAtk(h) * 1.8f * (0.85f + hashf(gEnemies[targetIdx].hp, 7, (uint32_t)gTime) * 0.3f));
        dmg = std::max(2, dmg);
        damageEnemy(targetIdx, dmg, false);
        screenShake(3.5f, 0.3f);
        checkBattleEnd();
        if (gPhase == BP_MAIN) endHeroTurn(heroIdx);
        return;
    }

    if (sp.ally) {
        PlaySound(g.assets.heal);
        auto healOne = [&](int i) {
            Hero& t = g.party[i];
            if (t.ko) return;
            int amount = sp.power + (int)(hashf(i, (int)gTime * 13, g.worldSeed) * 22.0f);
            t.hp = std::min(t.maxHp, t.hp + amount);
            Vector2 pos = heroBattlePos(i);
            addFloat({ pos.x, pos.y - 64 }, "+" + std::to_string(amount), Color{ 140, 255, 160, 255 });
        };
        if (sp.aoe) { for (int i = 0; i < 3; i++) healOne(i); }
        else healOne(targetIdx);
        gHitFlash = std::max(gHitFlash, 0.12f);
        if (gPhase == BP_MAIN) endHeroTurn(heroIdx);
        return;
    }

    // offensive magic
    PlaySound(g.assets.spell);
    if (spellIdx == 0 && h.klass == KLASS_MAGE) {
        // fireball projectile
        gFireball.active = true;
        gFireball.from = heroBattlePos(heroIdx);
        gFireball.to = { gEnemies[targetIdx].pos.x, gEnemies[targetIdx].pos.y - 30 };
        gFireball.t = 0.0f; gFireball.dur = 0.42f; gFireball.target = targetIdx;
        // damage applied on arrival in update()
    } else {
        // spark — instant
        int dmg = sp.power + (int)(hashf(targetIdx, (int)gTime * 29, g.worldSeed) * 14.0f) + zoneDamageBonus();
        dmg = std::max(1, dmg - gEnemies[targetIdx].def / 2);
        damageEnemy(targetIdx, dmg, false);
        screenShake(2.5f, 0.2f);
        gHitFlash = std::max(gHitFlash, 0.15f);
        checkBattleEnd();
    }
    if (gPhase == BP_MAIN) endHeroTurn(heroIdx);
}

void tryFlee(int heroIdx) {
    float roll = hashf((int)gTime * 77, heroIdx, g.worldSeed);
    if (roll < 0.7f) {
        PlaySound(g.assets.flee);
        gPhase = BP_FLED; gPhaseT = 0.0f;
        StopMusicStream(g.assets.battle);
    } else {
        addFloat({ VRES_W / 2.0f, 96 }, "Can't escape!", Color{ 255, 220, 150, 255 });
        endHeroTurn(heroIdx);
    }
}

void enemyAttack(int idx) {
    EnemySlot& e = gEnemies[idx];
    if (!e.alive) return;
    // pick target: prefer lowest HP living hero, slight randomness
    int best = -1; float bestScore = 1e9f;
    for (int i = 0; i < 3; i++) {
        if (g.party[i].ko) continue;
        float score = (float)g.party[i].hp * (0.7f + hashf(i, (int)gTime * 3, g.worldSeed));
        if (score < bestScore) { bestScore = score; best = i; }
    }
    if (best < 0) return;
    gEnemyLunge = 0.32f; gEnemyLungeIdx = idx;
    float roll = hashf((int)gTime * 500, idx, g.worldSeed);
    bool crit = roll < 0.10f;
    int dmg = calcPhysDamage(e.atk, heroDef(g.party[best]), hashf(best, (int)gTime, g.worldSeed + 5));
    if (crit) dmg = (int)(dmg * 1.5f);
    PlaySound(g.assets.hit);
    damageHero(best, dmg);
    screenShake(crit ? 4.0f : 2.0f, 0.25f);
    checkBattleEnd();
}

} // namespace

// ------------------------------------------------------------ namespace exports
void battleStart(const std::vector<EnemyKind>& kinds, int zoneLevel, uint64_t fieldMonsterId) {
    battleStartImpl(kinds, zoneLevel, fieldMonsterId);
}
bool battleIsActive() { return gActive; }

namespace {

// ================================================================ update
void updateIntro(float dt) {
    gPhaseT += dt; gTime += dt;
    if (gPhaseT >= 1.55f) { gPhase = BP_MAIN; gPhaseT = 0.0f; }
}

void applyPending(float dt, bool fill) {
    if (fill) {
        for (int i = 0; i < gEnemyCount; i++) {
            EnemySlot& e = gEnemies[i];
            if (!e.alive) continue;
            e.gauge += spdGaugeRate(e.spd) * dt;
            if (e.gauge >= 1.0f && gPendingCount < 4) {
                e.gauge = 0.0f;
                gPending[gPendingCount++] = { i, 0.45f };
            }
        }
        for (int i = 0; i < 3; i++) {
            if (g.party[i].ko) { gHeroGauge[i] = 0.0f; continue; }
            gHeroGauge[i] = std::min(1.0f, gHeroGauge[i] + spdGaugeRate(g.party[i].spd) * dt);
        }
    }
    for (int i = 0; i < gPendingCount;) {
        gPending[i].t -= dt;
        if (gPending[i].t <= 0.0f) {
            int idx = gPending[i].idx;
            // shift remaining entries down
            for (int j = i + 1; j < gPendingCount; j++) gPending[j - 1] = gPending[j];
            gPendingCount--;
            enemyAttack(idx);
            // if battle ended inside enemyAttack, stop
            if (gPhase == BP_DEFEAT || gPhase == BP_VICTORY) { gPendingCount = 0; return; }
        } else i++;
    }
}

void updateMain(float dt) {
    gTime += dt;
    gPhaseT += dt;
    if (gBannerT > 0) gBannerT -= dt;

    bool menuOpen = atbPausedForMenu();
    bool fill = (gPhase == BP_MAIN) || (!g.waitBattle && menuOpen);
    applyPending(dt, fill);

    // heroes whose gauge filled open their menu (one at a time)
    if (gPhase == BP_MAIN) {
        for (int i = 0; i < 3; i++) {
            if (g.party[i].ko) continue;
            if (gHeroGauge[i] >= 1.0f) {
                gActing = i; gPhase = BP_MENU; gMenuSel = 0;
                PlaySound(g.assets.cursor);
                break;
            }
        }
    }

    // timers
    if (gFireball.active) {
        gFireball.t += dt;
        if (gFireball.t >= gFireball.dur) {
            gFireball.active = false;
            int dmg = 30 + (int)(hashf(gFireball.target, (int)gTime * 31, g.worldSeed) * 16.0f) + zoneDamageBonus();
            dmg = std::max(1, dmg - gEnemies[gFireball.target].def / 2);
            damageEnemy(gFireball.target, dmg, false);
            gHitFlash = std::max(gHitFlash, 0.28f);
            screenShake(4.0f, 0.3f);
            checkBattleEnd();
            if (gPhase == BP_MAIN && gActing >= 0) endHeroTurn(gActing);
        }
    }
    for (int i = 0; i < gEnemyCount; i++) {
        if (gEnemies[i].hitT > 0) gEnemies[i].hitT -= dt;
        if (!gEnemies[i].alive && gEnemies[i].poofT > 0) gEnemies[i].poofT -= dt;
    }
    for (int i = 0; i < 3; i++) if (gLunge[i] > 0) gLunge[i] -= dt;
    if (gEnemyLunge > 0) gEnemyLunge -= dt;
    if (gHeroHitFlash > 0) gHeroHitFlash -= dt;
    if (gHitFlash > 0) gHitFlash -= dt;

    for (auto& f : gFloaters) f.t += dt;
    gFloaters.erase(std::remove_if(gFloaters.begin(), gFloaters.end(), [](const Floater& f) { return f.t > 1.15f; }), gFloaters.end());
}

void exitToOverworld() {
    StopMusicStream(g.assets.battle);
    StopMusicStream(g.assets.victory);
    if (g.musicOn) PlayMusicStream(g.assets.field);
    g.mode = MODE_OVERWORLD;
    g.encounterGrace = 3.0f;
    g.hasMoveTarget = false;
    gActive = false;
}

void updateVictory(float dt) {
    gTime += dt; gPhaseT += dt;
    for (auto& f : gFloaters) f.t += dt;
    gFloaters.erase(std::remove_if(gFloaters.begin(), gFloaters.end(), [](const Floater& f) { return f.t > 1.15f; }), gFloaters.end());
    if (gPhaseT > 1.0f && (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))) {
        PlaySound(g.assets.coin);
        exitToOverworld();
    }
}

void updateDefeat(float dt) {
    gTime += dt; gPhaseT += dt;
    if (gPhaseT > 2.6f) {
        g.mode = MODE_GAMEOVER;
        gActive = false;
    }
}

void updateFled(float dt) {
    gTime += dt; gPhaseT += dt;
    if (gPhaseT > 0.8f) exitToOverworld();
}

// ---------------------------------------------------------------- input
std::vector<std::pair<const char*, int>> consumableList() {
    std::vector<std::pair<const char*, int>> out;
    for (int id = 0; id < IT_COUNT; id++) {
        if (itemDef(id).kind != IK_CONSUMABLE) continue;
        if (g.inventory[id] <= 0) continue;
        out.push_back({ itemDef(id).name, id });
    }
    return out;
}

void confirmMenuCommand(int cmd) {
    PlaySound(g.assets.confirm);
    switch (cmd) {
        case 0: // Attack
            gPhase = BP_TARGET_FOE;
            gPendingSpell = -1; gPendingItem = -1; gTargetFoe = firstAliveFoe();
            gTargetReturn = BP_MENU;
            break;
        case 1: // Magic
            gPhase = BP_MAGIC; gMagicSel = 0;
            break;
        case 2: // Item
            gPhase = BP_ITEM; gItemSel = 0;
            break;
        case 3: // Defend
            gHeroDefending[gActing] = true;
            addFloat({ heroBattlePos(gActing).x, heroBattlePos(gActing).y - 70 }, "Defend", Color{ 170, 200, 255, 255 });
            endHeroTurn(gActing);
            break;
        case 4: // Run
            tryFlee(gActing);
            break;
    }
}

void updateMenuInput() {
    if (gActing < 0) { gPhase = BP_MAIN; return; }
    Vector2 m = getVirtualMouse();
    // command box above acting panel
    Rectangle pr = heroPanelRect(gActing);
    Rectangle box = { pr.x + 6, pr.y - 78, 116, 72 };
    const char* labels[5] = { "Attack", "Magic", "Item", "Defend", "Run" };
    for (int i = 0; i < 5; i++) {
        Rectangle row = { box.x + 3, box.y + 3 + i * 13, box.width - 6, 12 };
        if (CheckCollisionPointRec(m, row)) gMenuSel = i;
        if (CheckCollisionPointRec(m, row) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) confirmMenuCommand(i);
    }
    if (IsKeyPressed(KEY_DOWN)) { gMenuSel = (gMenuSel + 1) % 5; PlaySound(g.assets.cursor); }
    if (IsKeyPressed(KEY_UP))   { gMenuSel = (gMenuSel + 4) % 5; PlaySound(g.assets.cursor); }
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) confirmMenuCommand(gMenuSel);
}

void updateMagicInput() {
    if (gActing < 0) { gPhase = BP_MAIN; return; }
    Hero& h = g.party[gActing];
    int count = 0; const SpellDef* sp = spellsFor(h.klass, count);
    Vector2 m = getVirtualMouse();
    Rectangle pr = heroPanelRect(gActing);
    Rectangle box = { pr.x + 6, pr.y - 92, 168, (float)(18 + count * 13) };
    for (int i = 0; i < count; i++) {
        Rectangle row = { box.x + 3, box.y + 12 + i * 13, box.width - 6, 12 };
        if (CheckCollisionPointRec(m, row)) gMagicSel = i;
        if (CheckCollisionPointRec(m, row) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) gMagicSel = i, castSelectedSpell();
    }
    if (IsKeyPressed(KEY_DOWN)) { gMagicSel = (gMagicSel + 1) % count; PlaySound(g.assets.cursor); }
    if (IsKeyPressed(KEY_UP))   { gMagicSel = (gMagicSel + count - 1) % count; PlaySound(g.assets.cursor); }
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) castSelectedSpell();
    if (IsKeyPressed(KEY_ESCAPE)) { PlaySound(g.assets.cancel); gPhase = BP_MENU; }
}

void castSelectedSpell() {
    Hero& h = g.party[gActing];
    int count = 0; const SpellDef* sp = spellsFor(h.klass, count);
    const SpellDef& s = sp[gMagicSel];
    PlaySound(g.assets.confirm);
    if (s.aoe) { heroCast(gActing, gMagicSel, -1); return; }
    if (s.ally) { gPhase = BP_TARGET_ALLY; gPendingSpell = gMagicSel; gPendingItem = -1; gTargetAlly = 0; gTargetReturn = BP_MAGIC; return; }
    gPhase = BP_TARGET_FOE; gPendingSpell = gMagicSel; gPendingItem = -1; gTargetFoe = firstAliveFoe(); gTargetReturn = BP_MAGIC;
}

void updateItemInput() {
    if (gActing < 0) { gPhase = BP_MAIN; return; }
    auto items = consumableList();
    Vector2 m = getVirtualMouse();
    Rectangle pr = heroPanelRect(gActing);
    Rectangle box = { pr.x + 6, pr.y - 96, 176, (float)(20 + std::max(1, (int)items.size()) * 13) };
    for (int i = 0; i < (int)items.size(); i++) {
        Rectangle row = { box.x + 3, box.y + 14 + i * 13, box.width - 6, 12 };
        if (CheckCollisionPointRec(m, row)) gItemSel = i;
        if (CheckCollisionPointRec(m, row) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { gItemSel = i; useSelectedItem(); }
    }
    int n = std::max(1, (int)items.size());
    if (IsKeyPressed(KEY_DOWN)) { gItemSel = (gItemSel + 1) % n; PlaySound(g.assets.cursor); }
    if (IsKeyPressed(KEY_UP))   { gItemSel = (gItemSel + n - 1) % n; PlaySound(g.assets.cursor); }
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) useSelectedItem();
    if (IsKeyPressed(KEY_ESCAPE)) { PlaySound(g.assets.cancel); gPhase = BP_MENU; }
}

void useSelectedItem() {
    auto items = consumableList();
    if (items.empty()) { PlaySound(g.assets.cancel); return; }
    PlaySound(g.assets.confirm);
    gPhase = BP_TARGET_ALLY;
    gPendingItem = items[gItemSel].second;
    gPendingSpell = -1;
    gTargetAlly = firstAliveAlly();
    gTargetReturn = BP_ITEM;
}

int firstAliveFoe() {
    for (int i = 0; i < gEnemyCount; i++) if (gEnemies[i].alive) return i;
    return 0;
}
int firstAliveAlly() {
    for (int i = 0; i < 3; i++) if (!g.party[i].ko) return i;
    return 0;
}

void updateTargetFoe() {
    Vector2 m = getVirtualMouse();
    for (int i = 0; i < gEnemyCount; i++) {
        if (!gEnemies[i].alive) continue;
        Rectangle r = enemyRect(i);
        r.x -= 8; r.width += 16;
        if (CheckCollisionPointRec(m, r)) gTargetFoe = i;
        if (CheckCollisionPointRec(m, r) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { confirmFoeTarget(); return; }
    }
    if (IsKeyPressed(KEY_RIGHT)) cycleFoe(1);
    if (IsKeyPressed(KEY_LEFT))  cycleFoe(-1);
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) confirmFoeTarget();
    if (IsKeyPressed(KEY_ESCAPE)) { PlaySound(g.assets.cancel); gPhase = gTargetReturn; }
}

void cycleFoe(int dir) {
    for (int step = 1; step <= gEnemyCount; step++) {
        int i = (gTargetFoe + dir * step + gEnemyCount * 8) % gEnemyCount;
        if (gEnemies[i].alive) { gTargetFoe = i; PlaySound(g.assets.cursor); return; }
    }
}

void confirmFoeTarget() {
    int hero = gActing;
    if (gPendingSpell >= 0) { heroCast(hero, gPendingSpell, gTargetFoe); }
    else if (gPendingItem >= 0) { useItemOnHero(hero, gPendingItem, 2 /* unused */); }
    else { heroAttack(hero, gTargetFoe); }
}

void updateTargetAlly() {
    Vector2 m = getVirtualMouse();
    for (int i = 0; i < 3; i++) {
        Vector2 p = heroBattlePos(i);
        Rectangle r = { p.x - 18, p.y - 44, 40, 46 };
        if (CheckCollisionPointRec(m, r)) gTargetAlly = i;
        if (CheckCollisionPointRec(m, r) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { confirmAllyTarget(); return; }
    }
    if (IsKeyPressed(KEY_RIGHT)) { gTargetAlly = (gTargetAlly + 1) % 3; PlaySound(g.assets.cursor); }
    if (IsKeyPressed(KEY_LEFT))  { gTargetAlly = (gTargetAlly + 2) % 3; PlaySound(g.assets.cursor); }
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) confirmAllyTarget();
    if (IsKeyPressed(KEY_ESCAPE)) { PlaySound(g.assets.cancel); gPhase = gTargetReturn; }
}

void confirmAllyTarget() {
    int hero = gActing;
    if (gPendingSpell >= 0) { heroCast(hero, gPendingSpell, gTargetAlly); }
    else if (gPendingItem >= 0) {
        if (applyConsumable(gPendingItem, gTargetAlly)) {
            PlaySound(g.assets.potion);
            addFloat({ heroBattlePos(gTargetAlly).x, heroBattlePos(gTargetAlly).y - 70 },
                     itemDef(gPendingItem).name, Color{ 150, 220, 255, 255 });
        } else {
            addFloat({ heroBattlePos(gTargetAlly).x, heroBattlePos(gTargetAlly).y - 70 },
                     "No effect", Color{ 200, 200, 200, 255 });
        }
        endHeroTurn(hero);
    } else {
        endHeroTurn(hero);
    }
}

void useItemOnHero(int hero, int item, int /*unused*/) {
    // items are targeted at allies in this design; defend against misuse
    (void)hero; (void)item;
}

} // namespace

void battleUpdate(float dt) {
    switch (gPhase) {
        case BP_INTRO:   updateIntro(dt); break;
        case BP_MAIN:
        case BP_MENU:
        case BP_MAGIC:
        case BP_ITEM:
        case BP_TARGET_FOE:
        case BP_TARGET_ALLY: updateMain(dt); break;
        case BP_VICTORY: updateVictory(dt); break;
        case BP_DEFEAT:  updateDefeat(dt); break;
        case BP_FLED:    updateFled(dt); break;
    }
    if (gPhase == BP_MENU) updateMenuInput();
    else if (gPhase == BP_MAGIC) updateMagicInput();
    else if (gPhase == BP_ITEM) updateItemInput();
    else if (gPhase == BP_TARGET_FOE) updateTargetFoe();
    else if (gPhase == BP_TARGET_ALLY) updateTargetAlly();
}

namespace {

// ================================================================ draw
Color lerpCol(Color a, Color b, float t) {
    return { (unsigned char)(a.r + (b.r - a.r) * t), (unsigned char)(a.g + (b.g - a.g) * t),
             (unsigned char)(a.b + (b.b - a.b) * t), (unsigned char)(a.a + (b.a - a.a) * t) };
}

void drawBattleBackground() {
    TileInfo t = sampleTile((int)floorf(g.px), (int)floorf(g.py));
    bool snow = (t.kind == TK_SNOW || t.kind == TK_ICE);
    bool forest = moisture((float)g.px, (float)g.py, g.worldSeed) > 0.55f;
    Color skyTop = snow ? Color{40, 44, 70, 255} : Color{24, 28, 56, 255};
    Color skyMid = snow ? Color{110, 100, 120, 255} : Color{84, 52, 88, 255};
    Color skyHor = snow ? Color{212, 178, 140, 255} : Color{216, 124, 70, 255};
    int horizon = 148;
    for (int y = 0; y < horizon; y += 2) {
        float f = y / (float)horizon;
        Color c = (f < 0.55f) ? lerpCol(skyTop, skyMid, f / 0.55f) : lerpCol(skyMid, skyHor, (f - 0.55f) / 0.45f);
        DrawRectangle(0, y, VRES_W, 2, c);
    }
    // sun
    Vector2 sun = { 356, 118 };
    DrawCircleV(sun, 30, Fade(Color{255, 214, 130, 255}, 0.35f));
    DrawCircleV(sun, 17, Color{255, 224, 150, 255});
    // ridge lines (sawtooth silhouettes)
    for (int x = 0; x < VRES_W; x += 2) {
        float f = x * 0.028f;
        float r1 = 118.0f + sinf(f + 1.7f) * 16.0f + sinf(f * 0.37f + 0.4f) * 22.0f;
        DrawRectangle(x, (int)r1, 2, horizon - (int)r1, Color{23, 29, 51, 255});
    }
    for (int x = 0; x < VRES_W; x += 2) {
        float f = x * 0.017f;
        float r2 = 136.0f + sinf(f + 4.1f) * 12.0f + sinf(f * 0.5f + 2.2f) * 14.0f;
        DrawRectangle(x, (int)r2, 2, horizon - (int)r2, Color{17, 22, 40, 255});
    }
    // ground
    Color gTop = snow ? Color{84, 84, 104, 255} : (forest ? Color{30, 52, 34, 255} : Color{46, 72, 44, 255});
    Color gBot = snow ? Color{52, 52, 70, 255} : (forest ? Color{20, 36, 24, 255} : Color{30, 50, 30, 255});
    for (int y = horizon; y < VRES_H; y += 2) {
        float f = (y - horizon) / (float)(VRES_H - horizon);
        DrawRectangle(0, y, VRES_W, 2, lerpCol(gTop, gBot, f));
    }
    // battle clearing
    DrawRectangleGradientH(0, 0, 0, 0, BLANK, BLANK); // no-op keep symmetry
    DrawEllipse(256, 232, 236, 46, Fade(Color{0, 0, 0, 255}, 0.28f));
    DrawEllipse(256, 232, 200, 36, Fade(Color{255, 255, 255, 255}, 0.05f));
}

void drawHpMiniBar(float x, float y, float w, float frac, Color col) {
    DrawRectangle((int)x, (int)y, (int)w, 4, Color{10, 10, 18, 220});
    DrawRectangle((int)x, (int)y, (int)(w * std::max(0.0f, std::min(1.0f, frac))), 4, col);
}

void drawEnemies() {
    for (int i = 0; i < gEnemyCount; i++) {
        EnemySlot& e = gEnemies[i];
        Texture2D& tex = g.assets.enemy[(int)e.kind];
        float sc = 2.2f;
        float w = tex.width * sc, h = tex.height * sc;
        float bob = e.alive ? sinf(gTime * 2.2f + e.bob) * 3.0f : 0.0f;
        float drawX = e.pos.x - w / 2 + ((e.hitT > 0) ? sinf(gTime * 80.0f) * 3.5f * (e.hitT / 0.35f) : 0.0f);
        float drawY = e.pos.y - h + bob;
        // shadow
        DrawEllipse((int)e.pos.x, (int)(e.pos.y) + 2, (int)(w * 0.42f), 6, Fade(BLACK, 0.35f));
        // lunge toward heroes (leftwards)
        if (gEnemyLunge > 0 && gEnemyLungeIdx == i) {
            float p = 1.0f - gEnemyLunge / 0.32f;
            drawX -= sinf(p * 3.14159f) * 22.0f;
        }
        Color tint = (e.hitT > 0) ? Color{255, 190, 170, 255} : WHITE;
        if (!e.alive) {
            if (e.poofT > 0) {
                float a = e.poofT / 0.8f;
                DrawTexturePro(g.assets.fxPoof, { 0, 0, (float)g.assets.fxPoof.width, (float)g.assets.fxPoof.height },
                               { e.pos.x - 28 * a, e.pos.y - 30, 56 * a + 20, 38 * a + 14 }, { 0, 0 }, 0.0f, Fade(WHITE, a));
            }
            continue;
        }
        DrawTexturePro(tex, { 0, 0, (float)tex.width, (float)tex.height }, { drawX, drawY, w, h }, { 0, 0 }, 0.0f, tint);
        // target cursor
        if (gPhase == BP_TARGET_FOE && i == gTargetFoe) {
            float ty = drawY - 12 - sinf(gTime * 6.0f) * 3.0f;
            DrawTriangle({ e.pos.x - 6, ty }, { e.pos.x + 6, ty }, { e.pos.x, ty + 8 }, Color{255, 214, 90, 255});
            drawHpMiniBar(e.pos.x - 24, e.pos.y + 6, 48, (float)e.hp / (float)e.maxHp, Color{235, 120, 90, 255});
            const char* nm = enemyDef((int)e.kind).name;
            Vector2 ts = MeasureTextEx(g.assets.font8, nm, 8, 1);
            DrawTextEx(g.assets.font8, nm, { e.pos.x - ts.x / 2, e.pos.y + 13 }, 8, 1, Color{255, 228, 160, 255});
        }
    }
}

void drawHeroes() {
    for (int i = 0; i < 3; i++) {
        Hero& h = g.party[i];
        Vector2 pos = heroBattlePos(i);
        Texture2D& sheet = g.assets.heroWalk[h.klass];
        float sc = 2.0f;
        float fw = 16 * sc, fh = 24 * sc;
        float lunge = 0.0f;
        if (gLunge[i] > 0) { float p = 1.0f - gLunge[i] / 0.32f; lunge = sinf(p * 3.14159f) * 16.0f; }
        float bob = (!h.ko) ? sinf(gTime * 2.6f + i * 2.1f) * 1.5f : 0.0f;
        Rectangle src = { 0, 48, 16, 24 };   // side idle
        Rectangle dst = { pos.x - fw / 2 + lunge, pos.y - fh + bob, fw, fh };
        DrawEllipse((int)pos.x + (int)lunge, (int)pos.y + 2, 17, 4, Fade(BLACK, 0.35f));
        Color tint = h.ko ? Color{110, 110, 135, 255} : ((gHeroHitFlash > 0) ? Color{255, 200, 190, 255} : WHITE);
        DrawTexturePro(sheet, src, dst, { 0, 0 }, 0.0f, tint);
        if (!h.ko && gHeroDefending[i]) {
            DrawTextEx(g.assets.font8, "DEF", { pos.x - 11, pos.y - fh - 9 + bob }, 8, 1, Color{140, 190, 255, 255});
        }
        if (h.ko) {
            DrawTextEx(g.assets.font8, "KO", { pos.x - 8, pos.y - 14 }, 8, 1, Color{255, 100, 100, 255});
        }
        if (gPhase == BP_TARGET_ALLY && i == gTargetAlly) {
            float ty = pos.y - fh - 12 - sinf(gTime * 6.0f) * 3.0f;
            DrawTriangle({ pos.x - 6, ty }, { pos.x + 6, ty }, { pos.x, ty + 8 }, Color{120, 230, 140, 255});
        }
    }
}

void drawBar(float x, float y, float w, float h, float frac, Color col, Color bg = Color{16, 16, 26, 255}) {
    DrawRectangle((int)x, (int)y, (int)w, (int)h, bg);
    DrawRectangle((int)x, (int)y, (int)(w * std::max(0.0f, std::min(1.0f, frac))), (int)h, col);
}

void drawHeroPanel(int i) {
    Hero& h = g.party[i];
    Rectangle r = heroPanelRect(i);
    bool acting = (gActing == i) && (gPhase == BP_MENU || gPhase == BP_MAGIC || gPhase == BP_ITEM || gPhase == BP_TARGET_FOE || gPhase == BP_TARGET_ALLY);
    drawPanel(r);
    if (acting) {
        float pulse = 0.6f + 0.4f * sinf(gTime * 7.0f);
        DrawRectangleRoundedLinesEx({ r.x - 2, r.y - 2, r.width + 4, r.height + 4 }, 0.14f, 6, 1.6f,
                                    Fade(Color{255, 214, 90, 255}, pulse));
    }
    // portrait
    Texture2D& face = g.assets.face[h.klass];
    DrawTexturePro(face, { 0, 0, 32, 32 }, { r.x + 5, r.y + 7, 34, 34 }, { 0, 0 }, 0.0f, h.ko ? Color{120,120,140,255} : WHITE);
    float tx = r.x + 45;
    DrawTextEx(g.assets.font8, heroName(h.klass), { tx, r.y + 4 }, 8, 1, Color{235, 238, 255, 255});
    char hp[48], mp[48];
    snprintf(hp, sizeof(hp), "HP %d", h.hp);
    snprintf(mp, sizeof(mp), "MP %d", h.mp);
    DrawTextEx(g.assets.font8, hp, { tx, r.y + 14 }, 8, 1, Color{255, 200, 190, 255});
    drawBar(tx + 30, r.y + 16, 64, 5, (float)h.hp / h.maxHp, Color{224, 80, 70, 255});
    DrawTextEx(g.assets.font8, mp, { tx, r.y + 23 }, 8, 1, Color{180, 210, 255, 255});
    drawBar(tx + 30, r.y + 25, 64, 4, (float)h.mp / h.maxMp, Color{70, 140, 230, 255});
    // ATB bar
    float atb = gHeroGauge[i];
    Color atbCol = (atb >= 1.0f) ? (sinf(gTime * 10.0f) > 0 ? Color{255, 240, 150, 255} : Color{255, 200, 80, 255}) : Color{228, 184, 60, 255};
    drawBar(tx, r.y + 33, 94, 5, atb, atbCol);
    if (h.ko) {
        DrawRectangleRounded(r, 0.12f, 6, Fade(Color{40, 8, 8, 255}, 0.45f));
        DrawTextEx(g.assets.font8, "KO", { r.x + r.width / 2 - 8, r.y + r.height - 11 }, 8, 1, Color{255, 90, 90, 255});
    }
}

void drawFloaters() {
    for (auto& f : gFloaters) {
        float a = 1.0f - f.t / 1.15f;
        Vector2 p = { f.pos.x, f.pos.y - f.t * 24.0f };
        Vector2 ts = MeasureTextEx(g.assets.font8, f.text.c_str(), 8, 1);
        DrawTextEx(g.assets.font8, f.text.c_str(), { p.x - ts.x / 2 + 1, p.y + 1 }, 8, 1, Fade(BLACK, a * 0.7f));
        DrawTextEx(g.assets.font8, f.text.c_str(), { p.x - ts.x / 2, p.y }, 8, 1, Fade(f.color, a));
    }
}

void drawProjectiles() {
    if (!gFireball.active) return;
    float t = gFireball.t / gFireball.dur;
    Vector2 p;
    p.x = gFireball.from.x + (gFireball.to.x - gFireball.from.x) * t;
    p.y = gFireball.from.y + (gFireball.to.y - gFireball.from.y) * t - sinf(t * 3.14159f) * 26.0f;
    Texture2D& fb = g.assets.fxFireball;
    DrawTexturePro(fb, { 0, 0, (float)fb.width, (float)fb.height }, { p.x - 18, p.y - 18, 36, 36 }, { 0, 0 }, 0.0f, WHITE);
}

const char* enemyListText() {
    static char buf[96];
    buf[0] = 0;
    int counts[EN_COUNT] = { 0, 0, 0, 0, 0 };
    for (int i = 0; i < gEnemyCount; i++) counts[(int)gEnemies[i].kind]++;
    int pos = 0;
    bool first = true;
    for (int k = 0; k < EN_COUNT; k++) {
        if (counts[k] == 0) continue;
        if (!first) pos += snprintf(buf + pos, sizeof(buf) - pos, " and ");
        first = false;
        if (counts[k] > 1) pos += snprintf(buf + pos, sizeof(buf) - pos, "%s x%d", enemyDef(k).name, counts[k]);
        else pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", enemyDef(k).name);
    }
    return buf;
}

void drawIntro() {
    float t = gPhaseT;
    float slide;
    if (t < 0.5f) slide = t / 0.5f;         // in
    else if (t < 0.95f) slide = 1.0f;       // hold
    else slide = std::max(0.0f, 1.0f - (t - 0.95f) / 0.55f);  // out
    int barH = (int)(152 * slide);
    DrawRectangle(0, 0, VRES_W, barH, Color{8, 8, 14, 255});
    DrawRectangle(0, VRES_H - barH, VRES_W, barH, Color{8, 8, 14, 255});
    if (t > 0.35f && t < 1.35f) {
        char buf[128];
        snprintf(buf, sizeof(buf), "A wild %s appeared!", enemyListText());
        Vector2 ts = MeasureTextEx(g.assets.font8, buf, 8, 1);
        Rectangle r = { VRES_W / 2.0f - ts.x / 2 - 12, 124, ts.x + 24, 22 };
        drawPanel(r);
        DrawTextEx(g.assets.font8, buf, { r.x + 12, r.y + 7 }, 8, 1, Color{255, 240, 200, 255});
    }
}

void drawMenuBox() {
    if (gActing < 0) return;
    Rectangle pr = heroPanelRect(gActing);
    Rectangle box = { pr.x + 6, pr.y - 80, 116, 74 };
    drawPanel(box);
    const char* labels[5] = { "Attack", "Magic", "Item", "Defend", "Run" };
    for (int i = 0; i < 5; i++) {
        Color col = (i == gMenuSel) ? Color{255, 224, 120, 255} : Color{220, 226, 255, 255};
        if (i == gMenuSel) DrawTextEx(g.assets.font8, "-", { box.x + 2, box.y + 4 + i * 13 }, 8, 1, col);
        DrawTextEx(g.assets.font8, labels[i], { box.x + 12, box.y + 4 + i * 13 }, 8, 1, col);
    }
}

void drawMagicBox() {
    if (gActing < 0) return;
    Hero& h = g.party[gActing];
    int count = 0; const SpellDef* sp = spellsFor(h.klass, count);
    Rectangle pr = heroPanelRect(gActing);
    Rectangle box = { pr.x + 6, pr.y - 96, 170, (float)(20 + count * 13) };
    drawPanel(box);
    char title[32]; snprintf(title, sizeof(title), "Magic  MP %d", h.mp);
    DrawTextEx(g.assets.font8, title, { box.x + 6, box.y + 4 }, 8, 1, Color{170, 210, 255, 255});
    for (int i = 0; i < count; i++) {
        bool sel = (i == gMagicSel);
        bool can = h.mp >= sp[i].cost;
        Color col = !can ? Color{110, 110, 140, 255} : (sel ? Color{255, 224, 120, 255} : Color{220, 226, 255, 255});
        char row[48]; snprintf(row, sizeof(row), "%s %d", sp[i].name, sp[i].cost);
        if (sel) DrawTextEx(g.assets.font8, "-", { box.x + 2, box.y + 14 + i * 13 }, 8, 1, col);
        DrawTextEx(g.assets.font8, row, { box.x + 12, box.y + 14 + i * 13 }, 8, 1, col);
    }
}

void drawItemBox() {
    if (gActing < 0) return;
    auto items = consumableList();
    Rectangle pr = heroPanelRect(gActing);
    Rectangle box = { pr.x + 6, pr.y - 100, 178, (float)(22 + std::max(1, (int)items.size()) * 13) };
    drawPanel(box);
    DrawTextEx(g.assets.font8, "Items", { box.x + 6, box.y + 4 }, 8, 1, Color{180, 255, 190, 255});
    if (items.empty()) {
        DrawTextEx(g.assets.font8, "(empty)", { box.x + 12, box.y + 15 }, 8, 1, Color{150, 150, 170, 255});
        return;
    }
    for (int i = 0; i < (int)items.size(); i++) {
        bool sel = (i == gItemSel);
        Color col = sel ? Color{255, 224, 120, 255} : Color{220, 226, 255, 255};
        char row[48];
        snprintf(row, sizeof(row), "%s x%d", items[i].first, g.inventory[items[i].second]);
        if (sel) DrawTextEx(g.assets.font8, "-", { box.x + 2, box.y + 14 + i * 13 }, 8, 1, col);
        DrawTextEx(g.assets.font8, row, { box.x + 12, box.y + 14 + i * 13 }, 8, 1, col);
    }
}

void drawTargetHint() {
    const char* hint = (gPhase == BP_TARGET_FOE) ? "Choose a target  (Esc: back)" : "Choose an ally  (Esc: back)";
    Vector2 ts = MeasureTextEx(g.assets.font8, hint, 8, 1);
    DrawTextEx(g.assets.font8, hint, { VRES_W / 2.0f - ts.x / 2, 8 }, 8, 1, Color{220, 226, 255, 255});
}

void drawVictory() {
    DrawRectangle(0, 0, VRES_W, VRES_H, Fade(Color{10, 10, 24, 255}, 0.45f * std::min(1.0f, gPhaseT * 2.0f)));
    // banner
    const char* txt = "VICTORY!";
    Vector2 ts = MeasureTextEx(g.assets.font32, txt, 32, 2);
    float bob = sinf(gTime * 3.0f) * 2.0f;
    DrawTextEx(g.assets.font32, txt, { VRES_W / 2.0f - ts.x / 2 + 2, 44 + bob + 2 }, 32, 2, Color{90, 40, 10, 255});
    DrawTextEx(g.assets.font32, txt, { VRES_W / 2.0f - ts.x / 2, 44 + bob }, 32, 2, Color{255, 214, 90, 255});
    // results panel
    Rectangle r = { 146, 104, 220, 118 };
    drawPanel(r);
    char buf[64];
    snprintf(buf, sizeof(buf), "EXP  +%d", gExpGain);
    DrawTextEx(g.assets.font8, buf, { r.x + 14, r.y + 12 }, 8, 1, Color{235, 238, 255, 255});
    snprintf(buf, sizeof(buf), "Gil  +%d G", gGilGain);
    DrawTextEx(g.assets.font8, buf, { r.x + 14, r.y + 26 }, 8, 1, Color{255, 224, 120, 255});
    int ly = r.y + 44;
    for (int i = 0; i < 3; i++) {
        if (gLevelUps[i] > 0) {
            snprintf(buf, sizeof(buf), "%s -> Lv %d!", heroName(g.party[i].klass), g.party[i].level);
            DrawTextEx(g.assets.font8, buf, { r.x + 14, (float)ly }, 8, 1, Color{140, 255, 160, 255});
            ly += 13;
        }
    }
    if (gPhaseT > 1.0f && sinf(gTime * 4.0f) > -0.2f) {
        const char* hint = "click / Enter to continue";
        Vector2 hts = MeasureTextEx(g.assets.font8, hint, 8, 1);
        DrawTextEx(g.assets.font8, hint, { VRES_W / 2.0f - hts.x / 2, (float)(r.y + r.height - 14) }, 8, 1, Color{200, 208, 255, 200});
    }
}

void drawDefeat() {
    float a = std::min(1.0f, gPhaseT / 1.6f);
    DrawRectangle(0, 0, VRES_W, VRES_H, Fade(Color{0, 0, 0, 255}, a * 0.85f));
    if (gPhaseT > 0.7f) {
        const char* txt = "THE PARTY HAS FALLEN...";
        Vector2 ts = MeasureTextEx(g.assets.font16, txt, 16, 1);
        DrawTextEx(g.assets.font16, txt, { VRES_W / 2.0f - ts.x / 2, 116 }, 16, 1, Color{200, 60, 60, 255});
        const char* sub = "the wilds keep their secrets";
        Vector2 sts = MeasureTextEx(g.assets.font8, sub, 8, 1);
        DrawTextEx(g.assets.font8, sub, { VRES_W / 2.0f - sts.x / 2, 148 }, 8, 1, Color{160, 150, 170, 255});
    }
}

void drawFled() {
    DrawRectangle(0, 0, VRES_W, VRES_H, Fade(BLACK, 0.4f));
    const char* txt = "Got away safely.";
    Vector2 ts = MeasureTextEx(g.assets.font16, txt, 16, 1);
    DrawTextEx(g.assets.font16, txt, { VRES_W / 2.0f - ts.x / 2, 120 }, 16, 1, Color{220, 226, 255, 255});
}

} // namespace

void battleDraw() {
    drawBattleBackground();
    drawEnemies();
    drawHeroes();
    drawProjectiles();
    drawFloaters();
    for (int i = 0; i < 3; i++) drawHeroPanel(i);
    if (gPhase == BP_MENU) drawMenuBox();
    else if (gPhase == BP_MAGIC) drawMagicBox();
    else if (gPhase == BP_ITEM) drawItemBox();
    else if (gPhase == BP_TARGET_FOE || gPhase == BP_TARGET_ALLY) drawTargetHint();
    if (gPhase == BP_INTRO) drawIntro();
    else if (gPhase == BP_VICTORY) drawVictory();
    else if (gPhase == BP_DEFEAT) drawDefeat();
    else if (gPhase == BP_FLED) drawFled();
    // banner: whose turn
    if (gPhase == BP_MENU && gActing >= 0 && gBannerT > 0) { /* keep quiet */ }
    // flashes
    if (gHitFlash > 0) DrawRectangle(0, 0, VRES_W, VRES_H, Fade(WHITE, gHitFlash * 0.35f));
    if (gHeroHitFlash > 0) DrawRectangle(0, 0, VRES_W, VRES_H, Fade(Color{255, 60, 40, 255}, gHeroHitFlash * 0.25f));
}
