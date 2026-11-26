// battle.cpp — active-time battle, SNES style, mouse & keyboard
#include "battle.hpp"
#include "world.hpp"
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <cctype>

namespace {

enum BPhase {
    BP_INTRO = 0, BP_MAIN, BP_MENU, BP_MAGIC, BP_ITEM,
    BP_TARGET_FOE, BP_TARGET_ALLY, BP_VICTORY, BP_DEFEAT, BP_FLED
};

struct EnemySlot {
    EnemyKind kind = EN_SLIME;
    int hp = 1, maxHp = 1, atk = 1, def = 1, spd = 1;
    int xp = 1, gil = 1;
    int drop = -1, dropChance = 0;
    int weak = 0;
    Vector2 pos = { 0, 0 };
    float gauge = 0.0f, bob = 0.0f, hitT = 0.0f, poofT = 0.0f;
    bool alive = true;
};

struct Floater { Vector2 pos; float t = 0.0f; std::string text; Color color = WHITE; };
struct PendingEnemy { int idx = -1; float t = 0.0f; };
struct Projectile { bool active = false; Vector2 from{}, to{}; float t = 0.0f, dur = 0.45f; int target = -1; };

int firstAliveFoe();
int firstAliveAlly();
void cycleFoe(int dir);
void confirmFoeTarget();
void confirmAllyTarget();
void castSelectedSpell();
void useSelectedItem();
void useItemOnHero(int hero, int item);
void exitToOverworld();

int gPendingSpell = -1;
int gPendingItem = -1;
BPhase gTargetReturn = BP_MENU;

EnemySlot gEnemies[3];
int gEnemyCount = 0;
BPhase gPhase = BP_INTRO;
float gPhaseT = 0.0f;
float gHeroGauge[3] = { 0, 0, 0 };
bool gHeroDefending[3] = { false, false, false };
int gActing = -1;
int gTargetFoe = 0, gTargetAlly = 0;
int gMenuSel = 0, gMagicSel = 0, gItemSel = 0;
float gBannerT = 0.0f;
std::vector<Floater> gFloaters;
PendingEnemy gPending[4];
int gPendingCount = 0;
Projectile gFireball;
float gLunge[3] = { 0, 0, 0 };
float gEnemyLunge = 0.0f; int gEnemyLungeIdx = -1;
float gHeroHitFlash = 0.0f;
float gHitFlash = 0.0f;
float gTime = 0.0f;
int gExpGain = 0, gGilGain = 0;
int gLevelUps[3] = { 0, 0, 0 };
std::vector<std::string> gLoot;
uint64_t gFieldMonsterId = 0;
bool gActive = false;
Vector2 gLastEnemyPos = { 0, 0 };

// ---------------------------------------------------------------- utils
Rectangle enemyRect(int i) {
    Texture2D& t = g.assets.enemyBig[(int)gEnemies[i].kind];
    float w = t.width, h = t.height;
    return { gEnemies[i].pos.x - w / 2, gEnemies[i].pos.y - h, w, h };
}
Vector2 heroBattlePos(int i) {
    const Vector2 spots[3] = { { 96, 160 }, { 46, 210 }, { 128, 218 } };
    return spots[i];
}
void addFloat(Vector2 pos, const std::string& text, Color c) {
    gFloaters.push_back({ pos, 0.0f, text, c });
}
void screenShake(float mag, float t) { g.shakeMag = mag; g.shakeT = t; }
float spdGaugeRate(int spd) { return 0.055f + spd * 0.0115f; }

bool atbPausedForMenu() {
    return gPhase == BP_MENU || gPhase == BP_MAGIC || gPhase == BP_ITEM ||
           gPhase == BP_TARGET_FOE || gPhase == BP_TARGET_ALLY;
}

// ---------------------------------------------------------------- start
void battleStartImpl(const std::vector<EnemyKind>& kinds, int zoneLevel, uint64_t fieldMonsterId) {
    gFieldMonsterId = fieldMonsterId;
    gEnemyCount = 0;
    const float zl = (float)(zoneLevel - 1);
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
        e.drop = d.drop; e.dropChance = d.dropChance; e.weak = d.weak;
        e.gauge = hashf(idx, (int)fieldMonsterId, g.worldSeed ^ 0xc1a5) * 0.4f;
        e.bob = hashf(idx, 913, g.worldSeed) * 6.2831853f;
        e.hitT = 0; e.poofT = 0; e.alive = true;
    }
    const Vector2 formation[3][3] = {
        { { 348, 176 }, { 0, 0 }, { 0, 0 } },
        { { 280, 176 }, { 404, 162 }, { 0, 0 } },
        { { 228, 180 }, { 344, 166 }, { 444, 186 } },
    };
    for (int i = 0; i < gEnemyCount; i++) gEnemies[i].pos = formation[gEnemyCount - 1][i];
    if (gEnemyCount > 0) gLastEnemyPos = gEnemies[0].pos;

    gPhase = BP_INTRO; gPhaseT = 0.0f;
    gHeroGauge[0] = gHeroGauge[1] = gHeroGauge[2] = 0.15f;
    gHeroDefending[0] = gHeroDefending[1] = gHeroDefending[2] = false;
    gActing = -1; gMenuSel = 0; gMagicSel = 0; gItemSel = 0;
    gTargetFoe = 0; gTargetAlly = 0;
    gPendingCount = 0; gFireball.active = false;
    for (int i = 0; i < 3; i++) gLunge[i] = 0;
    gEnemyLunge = 0; gHeroHitFlash = 0; gHitFlash = 0;
    gFloaters.clear();
    gLoot.clear();
    gBannerT = 2.0f;
    gLevelUps[0] = gLevelUps[1] = gLevelUps[2] = 0;
    gExpGain = gGilGain = 0;
    gTime = 0.0f;
    gActive = true;
    g.mode = MODE_BATTLE;
    StopMusicStream(g.assets.field);
    StopMusicStream(g.assets.title);
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

void damageEnemy(int idx, int dmg, bool crit, int school = SCH_NONE) {
    EnemySlot& e = gEnemies[idx];
    if (school != SCH_NONE && (e.weak & (1 << school))) {
        dmg = (int)(dmg * 1.6f);
        addFloat({ e.pos.x, e.pos.y - 76 }, "WEAK!", Color{ 255, 210, 120, 255 });
    }
    e.hp -= dmg;
    e.hitT = 0.35f;
    addFloat({ e.pos.x, e.pos.y - 60 }, std::to_string(dmg), crit ? Color{ 255, 224, 120, 255 } : WHITE);
    if (e.hp <= 0) {
        e.hp = 0; e.alive = false; e.poofT = 0.8f;
        addFloat({ e.pos.x, e.pos.y - 84 }, "DOWN", Color{ 200, 210, 255, 255 });
        PlaySound(g.assets.hit);
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
    gHeroDefending[idx] = false;
    gActing = -1;
    gPhase = BP_MAIN;
}
void endAFoeTurn(int idx) {
    gEnemies[idx].gauge = 0.0f;
}

int aliveHeroCount() {
    int n = 0;
    for (auto& h : g.party) if (!h.ko) n++;
    return n;
}

void checkBattleEnd() {
    bool anyFoe = false;
    for (int i = 0; i < gEnemyCount; i++) if (gEnemies[i].alive) anyFoe = true;
    if (!anyFoe) {
        int xp = 0, gil = 0;
        for (int i = 0; i < gEnemyCount; i++) { xp += gEnemies[i].xp; gil += gEnemies[i].gil; }
        gExpGain = xp; gGilGain = gil;
        g.gil += gil;
        // loot
        for (int i = 0; i < gEnemyCount; i++) {
            EnemySlot& e = gEnemies[i];
            if (e.drop < 0) continue;
            float roll = hashf(i, (int)(gTime * 1000), g.worldSeed ^ 0x10001u) * 100.0f;
            if (roll < (float)e.dropChance) {
                addItem(e.drop, 1);
                gLoot.push_back(itemDef(e.drop).name);
            }
        }
        for (int i = 0; i < 3; i++) {
            if (g.party[i].ko) continue;
            g.party[i].xp += xp;
            gLevelUps[i] = levelUpCheck(g.party[i]);
        }
        g.defeatedCount += gEnemyCount;
        if (gFieldMonsterId != 0) g.defeatedMonsters.insert(gFieldMonsterId);
        // quest hooks
        for (int i = 0; i < gEnemyCount; i++) {
            if (gEnemies[i].kind == EN_SLIME) g.slimesSlain++;
            if (gEnemies[i].kind == EN_SLIME_KING) {
                g.bossDefeated = true;
                if (questActive(QUEST_CROWN)) {
                    questSetState(QUEST_CROWN, QS_DONE);
                    addItem(IT_CROWN, 1);
                    addItem(IT_SWORD_DRAGON, 1);
                }
            }
        }
        gPhase = BP_VICTORY; gPhaseT = 0.0f;
        StopMusicStream(g.assets.battle);
        if (g.musicOn) PlayMusicStream(g.assets.victory);
        PlaySound(g.assets.levelup);
        return;
    }
    if (aliveHeroCount() == 0) {
        gPhase = BP_DEFEAT; gPhaseT = 0.0f;
        StopMusicStream(g.assets.battle);
        if (g.musicOn) PlayMusicStream(g.assets.gameover);
    }
}

void heroAttack(int heroIdx, int foeIdx) {
    Hero& h = g.party[heroIdx];
    float roll = hashf((int)(gTime * 1000), heroIdx, g.worldSeed);
    bool crit = roll < 0.12f;
    int dmg = calcPhysDamage(heroAtk(h), gEnemies[foeIdx].def,
                             hashf(gEnemies[foeIdx].hp, heroIdx, (uint32_t)(gTime * 1000)));
    if (crit) dmg = (int)(dmg * 1.6f);
    gLunge[heroIdx] = 0.32f;
    PlaySound(g.assets.sword);
    damageEnemy(foeIdx, dmg, crit);
    gHitFlash = std::max(gHitFlash, crit ? 0.35f : 0.18f);
    screenShake(crit ? 4.0f : 2.0f, 0.25f);
    checkBattleEnd();
    if (gPhase == BP_MAIN) endHeroTurn(heroIdx);
}

void heroCast(int heroIdx, int spellIdx, int targetIdx) {
    Hero& h = g.party[heroIdx];
    SpellList list = spellsFor(h.klass, h.level);
    if (!list.spells || spellIdx < 0 || spellIdx >= list.count) { gPhase = BP_MAIN; return; }
    const SpellDef& sp = list.spells[spellIdx];
    if (h.mp < sp.cost) {
        addFloat(heroBattlePos(heroIdx), "Not enough MP!", Color{ 150, 190, 255, 255 });
        PlaySound(g.assets.cancel);
        gPhase = BP_MENU;
        return;
    }
    h.mp -= sp.cost;

    if (sp.name[0] == 'R' && std::strcmp(sp.name, "Revive") == 0) {
        if (!g.party[targetIdx].ko) {
            addFloat(heroBattlePos(heroIdx), "No one to revive!", Color{ 150, 190, 255, 255 });
            gPhase = BP_MENU; return;
        }
        g.party[targetIdx].ko = false;
        g.party[targetIdx].hp = g.party[targetIdx].maxHp / 2;
        PlaySound(g.assets.heal);
        addFloat(heroBattlePos(targetIdx), "REVIVED", Color{ 180, 255, 200, 255 });
        endHeroTurn(heroIdx);
        return;
    }
    if (std::strcmp(sp.name, "Last Stand") == 0 || std::strcmp(sp.name, "Sanctuary") == 0) {
        PlaySound(g.assets.heal);
        if (std::strcmp(sp.name, "Last Stand") == 0) {
            gHeroDefending[heroIdx] = true;
            Hero& t = g.party[heroIdx];
            t.hp = std::min(t.maxHp, t.hp + 24);
            addFloat(heroBattlePos(heroIdx), "GUARD", Color{ 190, 220, 255, 255 });
        } else {
            for (int i = 0; i < 3; i++) {
                Hero& t = g.party[i];
                if (t.ko) { t.ko = false; }
                t.hp = std::min(t.maxHp, t.hp + 90);
                t.mp = std::min(t.maxMp, t.mp + 30);
                gHeroDefending[i] = true;
                addFloat(heroBattlePos(i), "+90", Color{ 150, 255, 190, 255 });
            }
        }
        endHeroTurn(heroIdx);
        return;
    }

    if (sp.ally) {
        PlaySound(g.assets.heal);
        auto healOne = [&](int i) {
            Hero& t = g.party[i];
            if (t.ko) return;
            int amount = sp.power + (int)(hashf(i, (int)(gTime * 13), g.worldSeed) * 22.0f);
            t.hp = std::min(t.maxHp, t.hp + amount);
            addFloat({ heroBattlePos(i).x, heroBattlePos(i).y - 64 }, "+" + std::to_string(amount),
                     Color{ 140, 255, 160, 255 });
        };
        if (sp.aoe) { for (int i = 0; i < 3; i++) healOne(i); }
        else healOne(targetIdx);
        gHitFlash = std::max(gHitFlash, 0.12f);
        endHeroTurn(heroIdx);
        return;
    }

    PlaySound(g.assets.spell);
    if (sp.school == SCH_HOLY && h.klass == KLASS_CLERIC) {
        // holy bolt / smite — instant, plus a sparkle
        int dmg = sp.power + heroMag(h) + (int)(hashf(targetIdx, (int)(gTime * 29), g.worldSeed) * 14.0f);
        dmg = std::max(1, dmg - gEnemies[targetIdx].def / 2);
        damageEnemy(targetIdx, dmg, false, SCH_HOLY);
        screenShake(2.5f, 0.2f);
        gHitFlash = std::max(gHitFlash, 0.15f);
        checkBattleEnd();
        if (gPhase == BP_MAIN) endHeroTurn(heroIdx);
        return;
    }
    if (sp.fx == 1) {
        gFireball.active = true;
        gFireball.from = heroBattlePos(heroIdx);
        gFireball.to = { gEnemies[targetIdx].pos.x, gEnemies[targetIdx].pos.y - 30 };
        gFireball.t = 0.0f; gFireball.dur = 0.42f; gFireball.target = targetIdx;
        gPendingSpell = spellIdx;
        return;
    }
    // instant elemental hit
    int dmg = sp.power + heroMag(h) + (int)(hashf(targetIdx, (int)(gTime * 29), g.worldSeed) * 14.0f);
    dmg = std::max(1, dmg - gEnemies[targetIdx].def / 2);
    damageEnemy(targetIdx, dmg, false, sp.school);
    screenShake(2.5f, 0.2f);
    gHitFlash = std::max(gHitFlash, 0.15f);
    checkBattleEnd();
    if (gPhase == BP_MAIN) endHeroTurn(heroIdx);
}

void tryFlee(int heroIdx) {
    float roll = hashf((int)(gTime * 77), heroIdx, g.worldSeed);
    float chance = 0.62f;
    if (roll < chance) {
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
    int best = -1; float bestScore = 1e9f;
    for (int i = 0; i < 3; i++) {
        if (g.party[i].ko) continue;
        float score = (float)g.party[i].hp * (0.7f + hashf(i, (int)(gTime * 3), g.worldSeed));
        if (score < bestScore) { bestScore = score; best = i; }
    }
    if (best < 0) return;
    gEnemyLunge = 0.32f; gEnemyLungeIdx = idx;
    float roll = hashf((int)(gTime * 500), idx, g.worldSeed);
    bool crit = roll < 0.10f;
    int dmg = calcPhysDamage(e.atk, heroDef(g.party[best]),
                             hashf(best, (int)(gTime * 1000), g.worldSeed + 5));
    if (crit) dmg = (int)(dmg * 1.5f);
    PlaySound(g.assets.hit);
    damageHero(best, dmg);
    screenShake(crit ? 4.0f : 2.0f, 0.25f);
    endAFoeTurn(idx);
    checkBattleEnd();
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

// ---------------------------------------------------------------- updates
void updateIntro(float dt) {
    gPhaseT += dt;
    gTime += dt;
    if (gPhaseT > 0.9f) { gPhase = BP_MAIN; gPhaseT = 0.0f; }
}

void applyPending(float dt) {
    for (int i = 0; i < gPendingCount; i++) {
        gPending[i].t -= dt;
        if (gPending[i].t <= 0.0f && gPending[i].idx >= 0) {
            if (gEnemies[gPending[i].idx].alive) enemyAttack(gPending[i].idx);
            gPending[i].idx = -1;
        }
    }
    // compact
    int w = 0;
    for (int i = 0; i < gPendingCount; i++)
        if (gPending[i].idx >= 0) gPending[w++] = gPending[i];
    gPendingCount = w;
}

void updateMain(float dt) {
    gTime += dt;
    if (gPhaseT > 0) gPhaseT -= dt;
    if (gBannerT > 0) gBannerT -= dt;

    // hero ATB gauges fill while nobody's menu is open
    for (int i = 0; i < 3; i++) {
        if (g.party[i].ko) continue;
        gHeroGauge[i] = std::min(1.0f, gHeroGauge[i] + dt * spdGaugeRate(g.party[i].spd) * 0.55f);
    }
    // enemy gauges
    for (int i = 0; i < gEnemyCount; i++) {
        if (!gEnemies[i].alive) continue;
        gEnemies[i].gauge = std::min(1.0f, gEnemies[i].gauge + dt * spdGaugeRate(gEnemies[i].spd) * 0.42f);
        if (gEnemies[i].gauge >= 1.0f) {
            gEnemies[i].gauge = 1.0f;
            bool queued = false;
            for (int p = 0; p < gPendingCount; p++) if (gPending[p].idx == i) queued = true;
            if (!queued && gPendingCount < 4) {
                gPending[gPendingCount++] = { i, 0.25f + 0.1f * gPendingCount };
            }
        }
    }
    applyPending(dt);

    // fireball in flight
    if (gFireball.active) {
        gFireball.t += dt;
        if (gFireball.t >= gFireball.dur) {
            gFireball.active = false;
            int dmg = 30 + (int)(hashf(gFireball.target, (int)(gTime * 31), g.worldSeed) * 16.0f);
            dmg += heroMag(g.party[gActing >= 0 ? gActing : 0]);
            dmg = std::max(1, dmg - gEnemies[gFireball.target].def / 2);
            damageEnemy(gFireball.target, dmg, false, SCH_FIRE);
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

    // whoever fills their gauge first gets the command menu
    if (gPhase == BP_MAIN && gActing < 0) {
        int first = -1; float best = -1.0f;
        for (int i = 0; i < 3; i++) {
            if (g.party[i].ko) continue;
            if (gHeroGauge[i] >= 1.0f && gHeroGauge[i] > best) { best = gHeroGauge[i]; first = i; }
        }
        if (first >= 0) {
            gActing = first;
            gPhase = BP_MENU;
            gMenuSel = 0;
            PlaySound(g.assets.cursor);
        }
    }
}

void updateVictory(float dt) {
    gTime += dt; gPhaseT += dt;
    for (auto& f : gFloaters) f.t += dt;
    gFloaters.erase(std::remove_if(gFloaters.begin(), gFloaters.end(),
                                   [](const Floater& f) { return f.t > 1.15f; }),
                    gFloaters.end());
    if (gPhaseT > 1.0f && (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || IsKeyPressed(KEY_ENTER) ||
                           IsKeyPressed(KEY_SPACE))) {
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
        case 0:   // attack
            gPhase = BP_TARGET_FOE;
            gPendingSpell = -1; gPendingItem = -1;
            gTargetFoe = firstAliveFoe();
            gTargetReturn = BP_MENU;
            break;
        case 1:   // magic
            gPhase = BP_MAGIC; gMagicSel = 0;
            break;
        case 2:   // item
            gPhase = BP_ITEM; gItemSel = 0;
            break;
        case 3:   // defend
            gHeroDefending[gActing] = true;
            addFloat(heroBattlePos(gActing), "GUARD", Color{ 190, 220, 255, 255 });
            endHeroTurn(gActing);
            break;
        case 4:   // flee
            tryFlee(gActing);
            break;
    }
}

void updateMenuInput() {
    const char* cmds[5] = { "Attack", "Magic", "Item", "Defend", "Flee" };
    for (int i = 0; i < 5; i++) {
        Rectangle r{ 8.0f, 60.0f + i * 20, 96, 19 };
        Vector2 m = getVirtualMouse();
        if (CheckCollisionPointRec(m, r)) {
            if (gMenuSel != i) PlaySound(g.assets.cursor);
            gMenuSel = i;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { confirmMenuCommand(i); return; }
        }
    }
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) { gMenuSel = (gMenuSel + 1) % 5; PlaySound(g.assets.cursor); }
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) { gMenuSel = (gMenuSel + 4) % 5; PlaySound(g.assets.cursor); }
    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) { confirmMenuCommand(gMenuSel); return; }
    if (IsKeyPressed(KEY_ESCAPE)) { /* nothing — the hero must act */ }
    (void)cmds;
}

void castSelectedSpell() {
    int count = 0;
    SpellList list = spellsFor(g.party[gActing].klass, g.party[gActing].level);
    count = list.count;
    if (gMagicSel < 0 || gMagicSel >= count) { gPhase = BP_MENU; return; }
    const SpellDef& sp = list.spells[gMagicSel];
    gPendingSpell = gMagicSel;
    if (sp.ally) {
        gPhase = BP_TARGET_ALLY;
        gTargetAlly = (g.party[gActing].ko || g.party[gActing].hp < g.party[gActing].maxHp) ? gActing : firstAliveAlly();
        gTargetReturn = BP_MAGIC;
    } else {
        gPhase = BP_TARGET_FOE;
        gTargetFoe = firstAliveFoe();
        gTargetReturn = BP_MAGIC;
    }
}

void updateMagicInput() {
    SpellList list = spellsFor(g.party[gActing].klass, g.party[gActing].level);
    Vector2 m = getVirtualMouse();
    for (int i = 0; i < list.count; i++) {
        Rectangle r{ 8.0f, 260.0f - (float)(list.count * 18) + i * 18, 220, 17 };
        if (CheckCollisionPointRec(m, r)) {
            if (gMagicSel != i) PlaySound(g.assets.cursor);
            gMagicSel = i;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { castSelectedSpell(); return; }
        }
    }
    if (list.count == 0) { gPhase = BP_MENU; return; }
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) { gMagicSel = (gMagicSel + 1) % list.count; PlaySound(g.assets.cursor); }
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) { gMagicSel = (gMagicSel + list.count - 1) % list.count; PlaySound(g.assets.cursor); }
    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) { castSelectedSpell(); return; }
    if (IsKeyPressed(KEY_ESCAPE)) { PlaySound(g.assets.cancel); gPhase = BP_MENU; }
}

void useItemOnHero(int hero, int item) {
    if (item < 0) { gPhase = BP_MENU; return; }
    if (!applyConsumable(item, hero)) {
        addFloat(heroBattlePos(hero), "No effect", Color{ 190, 200, 230, 255 });
        PlaySound(g.assets.cancel);
        gPhase = BP_ITEM;
        return;
    }
    PlaySound(g.assets.potion);
    const ItemDef& d = itemDef(item);
    Vector2 pos = heroBattlePos(hero);
    if (item == IT_PHOENIX || item == IT_ELIXIR) {
        g.party[hero].ko = false;
        addFloat({ pos.x, pos.y - 70 }, "REVIVED", Color{ 180, 255, 200, 255 });
    } else {
        addFloat({ pos.x, pos.y - 64 }, std::string("+") + d.name, Color{ 140, 255, 160, 255 });
    }
    endHeroTurn(gActing);
}

void useSelectedItem() {
    auto list = consumableList();
    if (list.empty()) { gPhase = BP_MENU; return; }
    if (gItemSel >= (int)list.size()) gItemSel = 0;
    gPendingItem = list[gItemSel].second;
    gPhase = BP_TARGET_ALLY;
    gTargetAlly = gActing;
    gTargetReturn = BP_ITEM;
}

void updateItemInput() {
    auto list = consumableList();
    Vector2 m = getVirtualMouse();
    for (int i = 0; i < (int)list.size(); i++) {
        Rectangle r{ 8.0f, 260.0f - (float)(list.size() * 18) + i * 18, 220, 17 };
        if (CheckCollisionPointRec(m, r)) {
            if (gItemSel != i) PlaySound(g.assets.cursor);
            gItemSel = i;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { useSelectedItem(); return; }
        }
    }
    if (list.empty()) { gPhase = BP_MENU; return; }
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) { gItemSel = (gItemSel + 1) % (int)list.size(); PlaySound(g.assets.cursor); }
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) { gItemSel = (gItemSel + (int)list.size() - 1) % (int)list.size(); PlaySound(g.assets.cursor); }
    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) { useSelectedItem(); return; }
    if (IsKeyPressed(KEY_ESCAPE)) { PlaySound(g.assets.cancel); gPhase = BP_MENU; }
}

int firstAliveFoe() {
    for (int i = 0; i < gEnemyCount; i++) if (gEnemies[i].alive) return i;
    return 0;
}
int firstAliveAlly() {
    for (int i = 0; i < 3; i++) if (!g.party[i].ko) return i;
    return 0;
}
void cycleFoe(int dir) {
    for (int n = 0; n < gEnemyCount; n++) {
        gTargetFoe = (gTargetFoe + dir + gEnemyCount) % gEnemyCount;
        if (gEnemies[gTargetFoe].alive) return;
    }
}
void confirmFoeTarget() {
    if (!gEnemies[gTargetFoe].alive) { gTargetFoe = firstAliveFoe(); return; }
    if (gPendingItem >= 0) { useItemOnHero(gActing, gPendingItem); return; }
    if (gPendingSpell >= 0) heroCast(gActing, gPendingSpell, gTargetFoe);
    else heroAttack(gActing, gTargetFoe);
}
void confirmAllyTarget() {
    if (gPendingItem >= 0) { useItemOnHero(gTargetAlly, gPendingItem); return; }
    heroCast(gActing, gPendingSpell, gTargetAlly);
}

void updateTargetFoe() {
    Vector2 m = getVirtualMouse();
    for (int i = 0; i < gEnemyCount; i++) {
        if (!gEnemies[i].alive) continue;
        if (CheckCollisionPointRec(m, enemyRect(i))) {
            if (gTargetFoe != i) PlaySound(g.assets.cursor);
            gTargetFoe = i;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { confirmFoeTarget(); return; }
        }
    }
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) { cycleFoe(1); PlaySound(g.assets.cursor); }
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) { cycleFoe(-1); PlaySound(g.assets.cursor); }
    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) { confirmFoeTarget(); return; }
    if (IsKeyPressed(KEY_ESCAPE)) { PlaySound(g.assets.cancel); gPhase = gTargetReturn; gPendingSpell = -1; }
}

void updateTargetAlly() {
    Vector2 m = getVirtualMouse();
    for (int i = 0; i < 3; i++) {
        bool reviveItem = (gPendingItem == IT_PHOENIX || gPendingItem == IT_ELIXIR);
        bool reviveSpell = false;
        if (gPendingSpell >= 0) {
            SpellList sl = spellsFor(g.party[gActing].klass, g.party[gActing].level);
            if (sl.spells && gPendingSpell < sl.count &&
                std::strcmp(sl.spells[gPendingSpell].name, "Revive") == 0)
                reviveSpell = true;
        }
        if (g.party[i].ko && !reviveItem && !reviveSpell) continue;
        Rectangle r{ heroBattlePos(i).x - 24, heroBattlePos(i).y - 64, 48, 68 };
        if (CheckCollisionPointRec(m, r)) {
            if (gTargetAlly != i) PlaySound(g.assets.cursor);
            gTargetAlly = i;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { confirmAllyTarget(); return; }
        }
    }
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) { gTargetAlly = (gTargetAlly + 1) % 3; PlaySound(g.assets.cursor); }
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) { gTargetAlly = (gTargetAlly + 2) % 3; PlaySound(g.assets.cursor); }
    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) { confirmAllyTarget(); return; }
    if (IsKeyPressed(KEY_ESCAPE)) { PlaySound(g.assets.cancel); gPhase = gTargetReturn; gPendingSpell = -1; gPendingItem = -1; }
}

// ---------------------------------------------------------------- drawing
Color lerpCol(Color a, Color b, float t) {
    auto c = [&](unsigned char x, unsigned char y) { return (unsigned char)(x + (y - x) * t); };
    return { c(a.r, b.r), c(a.g, b.g), c(a.b, b.b), c(a.a, b.a) };
}

void drawBattleBackground() {
    // a stylised sky + hills backdrop built from the palette (no extra art needed)
    float t = gTime;
    DrawRectangle(-8, -12, VRES_W + 16, VRES_H + 24, Color{ 40, 52, 92, 255 });
    DrawRectangleGradientV(0, 0, VRES_W, VRES_H,
                           Color{ 56, 74, 132, 255 }, Color{ 148, 176, 208, 255 });
    // sun / moon
    DrawCircleGradient(VRES_W - 74, 46, 26, Fade(Color{ 255, 236, 170, 255 }, 0.9f), BLANK);
    // distant hills
    for (int layer = 0; layer < 3; layer++) {
        Color c = lerpCol({ 86, 116, 152, 255 }, { 44, 62, 84, 255 }, layer / 2.0f);
        for (int x = -20; x < VRES_W + 20; x += 8) {
            float h = 70.0f + sinf((x + layer * 40) * 0.012f + layer * 1.7f) * 26.0f +
                      sinf((x + layer * 13) * 0.031f) * 10.0f;
            DrawRectangle(x, (int)(150 + layer * 26 - h), 9, (int)(h + 60), c);
        }
    }
    // grass stage
    DrawRectangleGradientV(0, 196, VRES_W, VRES_H - 196,
                           Color{ 96, 140, 78, 255 }, Color{ 44, 72, 44, 255 });
    for (int i = 0; i < 60; i++) {
        float x = hashf(i, 3, 5u) * VRES_W;
        float y = 196 + hashf(i, 7, 9u) * (VRES_H - 196);
        DrawRectangle((int)x, (int)y, 1, 2, Fade(Color{ 150, 200, 120, 255 }, 0.35f));
    }
    (void)t;
}

void drawHpMiniBar(float x, float y, float w, float frac, Color col) {
    drawBar(x, y, w, 4, frac, col, Color{ 16, 18, 32, 255 });
}

void drawEnemies() {
    for (int i = 0; i < gEnemyCount; i++) {
        EnemySlot& e = gEnemies[i];
        Texture2D& t = g.assets.enemyBig[(int)e.kind];
        if (t.id == 0) continue;
        float bob = sinf(gTime * 2.2f + e.bob) * 2.0f;
        float lunge = 0.0f;
        if (gEnemyLunge > 0 && gEnemyLungeIdx == i) lunge = sinf((0.32f - gEnemyLunge) * 14.0f) * 14.0f;
        Color tint = WHITE;
        if (e.hitT > 0) tint = (fmodf(e.hitT * 30.0f, 2.0f) < 1.0f) ? Color{ 255, 170, 170, 255 } : WHITE;
        if (!e.alive) {
            float a = std::max(0.0f, e.poofT / 0.8f);
            tint = Fade(WHITE, a);
        }
        Rectangle dst{ e.pos.x - t.width * 0.5f - lunge * 0.4f,
                       e.pos.y - t.height + bob, (float)t.width, (float)t.height };
        DrawEllipse((int)e.pos.x, (int)e.pos.y, t.width * 0.32f, 6, Fade(BLACK, 0.25f));
        DrawTexturePro(t, { 0, 0, (float)t.width, (float)t.height }, dst, { 0, 0 }, 0.0f, tint);
        if (e.alive) {
            // target reticle
            if (gPhase == BP_TARGET_FOE && gTargetFoe == i) {
                float y = e.pos.y - t.height - 14;
                float w = t.width + 8;
                float x = e.pos.x - w * 0.5f;
                Color rc = Color{ 255, 224, 120, 255 };
                DrawRectangleLines((int)x, (int)y, (int)w, (int)(t.height + 16), rc);
                drawTextCentered(enemyDef((int)e.kind).name, e.pos.x, y - 12, rc, 8);
            }
            // health pip bar
            float w = std::min(90.0f, (float)t.width * 0.8f);
            drawHpMiniBar(e.pos.x - w * 0.5f, e.pos.y + 4, w, (float)e.hp / std::max(1, e.maxHp),
                          Color{ 220, 90, 90, 255 });
            // ATB gauge under it
            drawBar(e.pos.x - w * 0.5f, e.pos.y + 10, w, 2, e.gauge,
                    Color{ 240, 180, 90, 200 }, Color{ 20, 24, 40, 160 });
        }
    }
}

void drawHeroes() {
    for (int i = 0; i < 3; i++) {
        Hero& h = g.party[i];
        Vector2 pos = heroBattlePos(i);
        Texture2D& sheet = g.assets.heroWalk[std::max(0, std::min(2, h.klass))];
        int row = h.ko ? 0 : 2;
        int frame = h.ko ? 1 : (1 + ((int)(gTime * 5.0f + i) & 1));
        Color tint = h.ko ? Color{ 120, 120, 150, 255 } : WHITE;
        if (gHeroHitFlash > 0 && fmodf(gHeroHitFlash * 30.0f, 2.0f) < 1.0f) tint = Color{ 255, 160, 150, 255 };
        float lunge = 0.0f;
        if (gLunge[i] > 0) lunge = sinf((0.32f - gLunge[i]) * 14.0f) * 16.0f;
        float baseX = pos.x + lunge;
        float baseY = pos.y;
        if (gActing == i) baseY -= 2;
        Rectangle src{ (float)(frame * 24), (float)(row * 32), 24, 32 };
        Rectangle dst{ baseX - 24, baseY - 64, 48, 64 };
        DrawEllipse((int)baseX, (int)baseY, 13, 4, Fade(BLACK, 0.28f));
        DrawTexturePro(sheet, src, dst, { 0, 0 }, 0.0f, tint);
        if (gPhase == BP_TARGET_ALLY && gTargetAlly == i) {
            DrawRectangleLines((int)(baseX - 26), (int)(baseY - 68), 52, 74, Color{ 255, 224, 120, 255 });
        }
    }
}

void drawFloaters() {
    for (auto& f : gFloaters) {
        float a = 1.0f - std::max(0.0f, (f.t - 0.7f) / 0.45f);
        float y = f.pos.y - f.t * 22.0f;
        drawTextCentered(f.text.c_str(), f.pos.x, y, Fade(f.color, a), 8);
    }
}

void drawProjectiles() {
    if (!gFireball.active) return;
    float t = gFireball.t / gFireball.dur;
    float x = gFireball.from.x + (gFireball.to.x - gFireball.from.x) * t;
    float y = gFireball.from.y + (gFireball.to.y - gFireball.from.y) * t - sinf(t * 3.14159f) * 22.0f;
    DrawCircleGradient((int)x, (int)y, 14, Fade(Color{ 255, 210, 120, 255 }, 0.95f),
                       Fade(Color{ 200, 60, 30, 255 }, 0.15f));
    DrawCircleV({ x, y }, 5, Color{ 255, 250, 220, 255 });
}

void drawHeroPanel(int i) {
    const Hero& h = g.party[i];
    float pw = 158.0f, ph = 54.0f, gap = 6.0f;
    float x = 6.0f + i * (pw + gap);
    float y = VRES_H - ph - 4.0f;
    drawWindow({ x, y, pw, ph });
    Texture2D& f = g.assets.face[i == 0 ? FACE_KNIGHT : (i == 1 ? FACE_MAGE : FACE_CLERIC)];
    if (f.id) {
        DrawTexturePro(f, { 0, 0, (float)f.width, (float)f.height }, { x + 6, y + 6, 30, 30 },
                       { 0, 0 }, 0.0f, h.ko ? Color{ 130, 130, 160, 255 } : WHITE);
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "%s", heroName(h.klass));
    drawText(buf, x + 42, y + 6, h.ko ? Color{ 200, 140, 140, 255 } : Color{ 236, 240, 255, 255 }, 8);
    snprintf(buf, sizeof(buf), "Lv %d", h.level);
    drawText(buf, x + pw - 40, y + 6, Color{ 200, 210, 240, 255 }, 8);
    snprintf(buf, sizeof(buf), "%d/%d", h.hp, h.maxHp);
    drawText(buf, x + 42, y + 19, Color{ 170, 240, 180, 255 }, 8);
    drawBar(x + 42, y + 29, pw - 50, 3, (float)h.hp / std::max(1, h.maxHp),
            Color{ 96, 220, 120, 255 }, Color{ 24, 30, 50, 255 });
    snprintf(buf, sizeof(buf), "%d/%d", h.mp, h.maxMp);
    drawText(buf, x + 42, y + 34, Color{ 160, 200, 255, 255 }, 8);
    drawBar(x + 42, y + 44, pw - 50, 2, (float)h.mp / std::max(1, h.maxMp),
            Color{ 96, 160, 255, 255 }, Color{ 24, 30, 50, 255 });
    // ATB gauge
    drawBar(x + 6, y + ph - 6, pw - 12, 3, gHeroGauge[i],
            h.ko ? Color{ 90, 90, 120, 255 } : Color{ 255, 210, 120, 255 }, Color{ 20, 24, 40, 255 });
    if (gActing == i) {
        DrawRectangleLines((int)x, (int)y, (int)pw, (int)ph, Color{ 255, 224, 120, 255 });
    }
}

void drawIntro() {
    float a = std::min(1.0f, gPhaseT / 0.5f);
    for (int y = 0; y < VRES_H; y += 2) DrawRectangle(0, y, VRES_W, 2, Fade(BLACK, 0.45f * a));
    const char* names[3] = { "", "", "" };
    (void)names;
    std::string title = "";
    for (int i = 0; i < gEnemyCount; i++) {
        if (i) title += " & ";
        title += enemyDef((int)gEnemies[i].kind).name;
    }
    if (!title.empty()) title[0] = (char)toupper(title[0]);
    DrawRectangle(0, VRES_H / 2 - 20, VRES_W, 40, Fade(Color{ 6, 8, 20, 255 }, 0.8f * a));
    drawTextCentered(title.c_str(), VRES_W * 0.5f, VRES_H / 2 - 10, Fade(Color{ 255, 236, 176, 255 }, a), 16);
    drawTextCentered("attacks!", VRES_W * 0.5f, VRES_H / 2 + 6, Fade(WHITE, a * 0.8f), 8);
}

void drawMenuBox() {
    drawWindow({ 4, 54, 104, 106 });
    const char* cmds[5] = { "Attack", "Magic", "Item", "Defend", "Flee" };
    for (int i = 0; i < 5; i++) {
        bool sel = (gMenuSel == i);
        if (sel)
            DrawRectangleRounded({ 8, 60.0f + i * 20, 96, 17 }, 0.3f, 4, Fade(Color{ 70, 90, 180, 255 }, 0.85f));
        drawText(cmds[i], 14, 64.0f + i * 20, sel ? Color{ 255, 236, 176, 255 } : WHITE, 8);
    }
    const Hero& h = g.party[gActing];
    char buf[64];
    snprintf(buf, sizeof(buf), "%s  Lv %d", heroName(h.klass), h.level);
    drawWindow({ 4, 32, 150, 20 });
    drawText(buf, 10, 38, Color{ 236, 240, 255, 255 }, 8);
}

void drawMagicBox() {
    Hero& h = g.party[gActing];
    SpellList list = spellsFor(h.klass, h.level);
    float hh = list.count * 18 + 12;
    drawWindow({ 4, 260 - hh, 236, hh });
    for (int i = 0; i < list.count; i++) {
        const SpellDef& sp = list.spells[i];
        bool sel = (gMagicSel == i);
        bool can = h.mp >= sp.cost;
        if (sel) DrawRectangleRounded({ 8, 260 - hh + 5.0f + i * 18, 228, 17 }, 0.3f, 4,
                                      Fade(Color{ 70, 90, 180, 255 }, 0.85f));
        Color c = can ? (sel ? Color{ 255, 236, 176, 255 } : WHITE) : Color{ 140, 140, 170, 255 };
        drawText(sp.name, 12, 260 - hh + 9.0f + i * 18, c, 8);
        char buf[32];
        snprintf(buf, sizeof(buf), "%2d MP", sp.cost);
        drawText(buf, 206, 260 - hh + 9.0f + i * 18, c, 8);
    }
    drawText("ESC: back", 12, 262, Color{ 170, 180, 210, 220 }, 8);
}

void drawItemBox() {
    auto list = consumableList();
    float hh = list.size() * 18 + 12;
    if (hh < 30) hh = 30;
    drawWindow({ 4, 260 - hh, 236, hh });
    for (int i = 0; i < (int)list.size(); i++) {
        bool sel = (gItemSel == i);
        if (sel) DrawRectangleRounded({ 8, 260 - hh + 5.0f + i * 18, 228, 17 }, 0.3f, 4,
                                      Fade(Color{ 70, 90, 180, 255 }, 0.85f));
        drawItemIcon(list[i].second, 12, 260 - hh + 7.0f + i * 18, 12);
        Color c = sel ? Color{ 255, 236, 176, 255 } : WHITE;
        drawText(list[i].first, 28, 260 - hh + 9.0f + i * 18, c, 8);
        char buf[16];
        snprintf(buf, sizeof(buf), "x%d", itemCount(list[i].second));
        drawText(buf, 214, 260 - hh + 9.0f + i * 18, c, 8);
    }
    if (list.empty()) drawText("No items.", 14, 260 - hh + 10, Color{ 190, 200, 230, 255 }, 8);
}

void drawVictory() {
    for (int y = 0; y < VRES_H; y += 2) DrawRectangle(0, y, VRES_W, 2, Fade(BLACK, 0.35f));
    float w = 260, h = 148;
    float x = VRES_W * 0.5f - w * 0.5f, y = VRES_H * 0.5f - h * 0.5f - 10;
    drawWindow({ x, y, w, h });
    drawTextCentered("VICTORY", VRES_W * 0.5f, y + 10, Color{ 255, 236, 176, 255 }, 16);
    char buf[64];
    snprintf(buf, sizeof(buf), "%d EXP", gExpGain);
    drawText(buf, x + 20, y + 36, Color{ 200, 230, 255, 255 }, 8);
    snprintf(buf, sizeof(buf), "%d gil", gGilGain);
    drawText(buf, x + 150, y + 36, Color{ 255, 230, 160, 255 }, 8);
    float ly = y + 54;
    for (int i = 0; i < 3; i++) {
        if (gLevelUps[i] > 0) {
            snprintf(buf, sizeof(buf), "%s reached level %d!", heroName(g.party[i].klass),
                     g.party[i].level);
            drawText(buf, x + 20, ly, Color{ 160, 255, 180, 255 }, 8);
            ly += 14;
        }
    }
    for (auto& l : gLoot) {
        snprintf(buf, sizeof(buf), "Found %s", l.c_str());
        drawText(buf, x + 20, ly, Color{ 255, 220, 170, 255 }, 8);
        ly += 14;
    }
    if (fmodf(gTime * 2.0f, 2.0f) < 1.2f)
        drawTextCentered("SPACE to continue", VRES_W * 0.5f, y + h - 18,
                         Color{ 200, 210, 240, 255 }, 8);
}

} // namespace

// ------------------------------------------------------------ exports
void battleStart(const std::vector<EnemyKind>& kinds, int zoneLevel, uint64_t fieldMonsterId) {
    battleStartImpl(kinds, zoneLevel, fieldMonsterId);
}
bool battleIsActive() { return gActive; }

void battleUpdate(float dt) {
    if (!gActive) return;
    switch (gPhase) {
        case BP_INTRO:       updateIntro(dt); break;
        case BP_MAIN:        updateMain(dt); break;
        case BP_MENU:        gTime += dt; updateMain(0.0f); updateMenuInput(); break;
        case BP_MAGIC:       gTime += dt; updateMain(0.0f); updateMagicInput(); break;
        case BP_ITEM:        gTime += dt; updateMain(0.0f); updateItemInput(); break;
        case BP_TARGET_FOE:  gTime += dt; updateMain(0.0f); updateTargetFoe(); break;
        case BP_TARGET_ALLY: gTime += dt; updateMain(0.0f); updateTargetAlly(); break;
        case BP_VICTORY:     updateVictory(dt); break;
        case BP_DEFEAT:      updateDefeat(dt); break;
        case BP_FLED:        updateFled(dt); break;
    }
    // animate floaters in every phase
    for (auto& f : gFloaters) f.t += dt;
    gFloaters.erase(std::remove_if(gFloaters.begin(), gFloaters.end(),
                                   [](const Floater& f) { return f.t > 1.15f; }),
                    gFloaters.end());
    if (g.shakeT > 0) g.shakeT -= dt;
}

void battleDraw() {
    if (!gActive) return;
    float sx = 0, sy = 0;
    if (g.shakeT > 0) {
        sx = sinf(gTime * 63.0f) * g.shakeMag;
        sy = cosf(gTime * 51.0f) * g.shakeMag;
    }
    DrawRectangle(0, 0, VRES_W, VRES_H, Color{ 8, 10, 18, 255 });
    // render everything into an offset camera
    Camera2D cam{ { sx, sy }, { 0, 0 }, 0.0f, 1.0f };
    BeginMode2D(cam);
    drawBattleBackground();
    drawEnemies();
    drawHeroes();
    drawProjectiles();
    EndMode2D();

    drawFloaters();
    if (gHitFlash > 0) DrawRectangle(0, 0, VRES_W, VRES_H, Fade(WHITE, gHitFlash * 0.35f));

    if (gPhase == BP_INTRO) {
        drawIntro();
    } else if (gPhase == BP_VICTORY) {
        drawVictory();
    } else if (gPhase == BP_DEFEAT) {
        DrawRectangle(0, 0, VRES_W, VRES_H, Fade(Color{ 24, 4, 8, 255 }, 0.55f));
        drawTextCentered("DEFEAT", VRES_W * 0.5f, VRES_H * 0.5f - 8, Color{ 255, 190, 190, 255 }, 24);
    } else if (gPhase == BP_FLED) {
        DrawRectangle(0, 0, VRES_W, VRES_H, Fade(BLACK, 0.4f));
        drawTextCentered("Escaped!", VRES_W * 0.5f, VRES_H * 0.5f - 8, Color{ 220, 230, 255, 255 }, 16);
    } else {
        for (int i = 0; i < 3; i++) drawHeroPanel(i);
        if (gPhase == BP_MENU) drawMenuBox();
        else if (gPhase == BP_MAGIC) drawMagicBox();
        else if (gPhase == BP_ITEM) drawItemBox();
        else if (gPhase == BP_TARGET_FOE)
            drawTextCentered("choose a target", VRES_W * 0.5f, VRES_H - 22,
                             Color{ 255, 236, 176, 255 }, 8);
        else if (gPhase == BP_TARGET_ALLY)
            drawTextCentered("choose a companion", VRES_W * 0.5f, VRES_H - 22,
                             Color{ 255, 236, 176, 255 }, 8);
    }
    if (gBannerT > 0) {
        float a = std::min(1.0f, gBannerT);
        DrawRectangle(0, 6, VRES_W, 22, Fade(Color{ 8, 10, 24, 255 }, 0.7f * a));
        char buf[96];
        const char* zn = zoneNameAt(g.px, g.py);
        snprintf(buf, sizeof(buf), "%s  -  zone level %d", zn, zoneLevelAt(g.px, g.py));
        drawTextCentered(buf, VRES_W * 0.5f, 12, Fade(WHITE, a), 8);
    }
}
