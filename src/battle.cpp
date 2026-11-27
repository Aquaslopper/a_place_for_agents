// battle.cpp — classic side-view ATB combat
#include "game.hpp"
#include "battle.hpp"
#include "world.hpp"
#include <cstdio>

enum BattlePhase : uint8_t {
    BP_INTRO = 0, BP_ACTIVE, BP_COMMAND, BP_MAGIC, BP_ITEM, BP_TARGET_FOE, BP_TARGET_ALLY,
    BP_VICTORY, BP_DEFEAT, BP_FLED
};

enum FxKind : uint8_t { FX_SLASH = 0, FX_FIREBALL, FX_SPARK, FX_SPARKLE, FX_IMPACT, FX_POOF };

struct BattleEnemy {
    EnemyKind kind = EN_SLIME;
    int hp = 1, maxHp = 1;
    int atk = 1, def = 1, spd = 1;
    int xp = 1, gil = 1;
    int weak = 0;
    int drop = -1, dropChance = 0;
    bool alive = true;
    bool boss = false;
    float atb = 0;
    float x = 0, y = 0;
    float hitT = 0;
    float lungeT = 0;
};

struct Floater {
    std::string text;
    float x, y, vy, life;
    Color col;
};

struct BattleFx {
    FxKind kind;
    float x, y, life, maxLife;
};

struct BattleState {
    BattlePhase phase = BP_INTRO;
    float phaseT = 0;
    std::vector<BattleEnemy> foes;
    int activeHero = -1;
    int cmdCursor = 0;
    int subCursor = 0;
    int targetCursor = 0;
    bool defending[3] = {};
    float atb[3] = {};
    float heroHitT[3] = {};
    float heroLungeT[3] = {};
    float screenShake = 0;
    std::string banner;
    float bannerT = 0;
    std::vector<Floater> floaters;
    std::vector<BattleFx> fx;
    int xpGain = 0, gilGain = 0;
    std::vector<std::string> rewards;
    uint64_t fieldMonsterId = 0;
    bool isBoss = false;
    bool crownDropped = false;
    int zoneLevel = 1;
};

static BattleState B;
static int gPendingSpellIdx = -1; // index into spellsFor(activeHero)
static int gPendingItem = -1;     // ItemId or -1

bool battleIsActive() { return g.mode == MODE_BATTLE; }

static void spawnFloater(const std::string& t, float x, float y, Color c) {
    B.floaters.push_back({ t, x, y, -26.0f, 1.15f, c });
}

static void spawnFx(FxKind kind, float x, float y, float dur = 0.34f) {
    B.fx.push_back({ kind, x, y, dur, dur });
}

static void setBanner(const std::string& s, float dur = 1.6f) {
    B.banner = s; B.bannerT = dur;
}

static float heroX(int i) { return 372.0f + (g.party[i].row == 1 ? 18.0f : 0.0f) + i * 16.0f; }
static float heroY(int i) { return 102.0f + i * 32.0f; }

static int firstAliveFoe() {
    for (int i = 0; i < (int)B.foes.size(); i++) if (B.foes[i].alive) return i;
    return -1;
}
static int aliveFoeCount() {
    int n = 0; for (auto& f : B.foes) if (f.alive) n++;
    return n;
}
static int aliveHeroCount() {
    int n = 0; for (int i = 0; i < 3; i++) if (!g.party[i].ko) n++;
    return n;
}

void battleStart(const std::vector<EnemyKind>& kinds, int zoneLevel, uint64_t fieldMonsterId, bool isBoss) {
    B = BattleState{};
    B.fieldMonsterId = fieldMonsterId;
    B.zoneLevel = std::max(1, zoneLevel);
    B.isBoss = isBoss;
    B.phase = BP_INTRO;
    B.phaseT = 0.55f;
    gPendingSpellIdx = -1;
    gPendingItem = -1;

    int count = std::max(1, std::min(4, (int)kinds.size()));
    for (int i = 0; i < count; i++) {
        EnemyKind k = kinds[i];
        const EnemyDef& d = enemyDef(k);
        if (k == EN_SLIME_KING) B.isBoss = true;
        float scale = (k == EN_SLIME_KING)
            ? (1.0f + 0.08f * std::max(0, B.zoneLevel - 4))
            : (1.0f + 0.14f * (B.zoneLevel - 1));
        BattleEnemy e;
        e.kind = k;
        e.boss = (k == EN_SLIME_KING);
        e.maxHp = std::max(10, (int)roundf(d.hp * scale));
        e.hp = e.maxHp;
        e.atk = std::max(4, (int)roundf(d.atk * scale));
        e.def = std::max(1, (int)roundf(d.def * scale));
        e.spd = d.spd;
        e.xp  = (int)roundf(d.xp * scale);
        e.gil = (int)roundf(d.gil * scale);
        e.weak = d.weak;
        e.drop = d.drop;
        e.dropChance = d.dropChance;
        e.alive = true;
        e.atb = 0.15f + 0.2f * hashf(i, B.zoneLevel, 13u);
        e.hitT = 0;
        e.lungeT = 0;
        e.x = (e.boss ? 94.0f : 76.0f) + (i % 2) * 58.0f;
        e.y = (count == 1) ? 132.0f : (92.0f + i * (96.0f / std::max(1, count - 1)));
        B.foes.push_back(e);
    }

    for (int i = 0; i < 3; i++) {
        B.atb[i] = g.party[i].ko ? 0.0f : (0.28f + i * 0.14f);
        B.defending[i] = false;
        B.heroHitT[i] = 0;
        B.heroLungeT[i] = 0;
    }

    if (B.isBoss) setBanner("The Slime King blocks the gate!", 2.2f);
    else if (count == 1) setBanner(std::string("A wild ") + enemyDef(kinds[0]).name + " draws near!", 1.8f);
    else setBanner("A band of monsters attacks!", 1.8f);

    g.mode = MODE_BATTLE;
    g.fade = 1.0f; g.fadeDir = -1;
    playSoundIf(g.assets.encounter);
    syncModeMusic();
}

// ================================================================ damage math
static int physicalDamage(int atk, int def, bool defenderBackRow, bool defending, bool& crit) {
    float r = 0.88f + hashf(GetRandomValue(0, 9999), (int)(g.totalTime * 100), 17u) * 0.24f;
    float base = std::max(1.0f, atk * 1.45f - def * 0.65f);
    crit = (GetRandomValue(0, 99) < 10);
    if (crit) base *= 1.55f;
    if (defenderBackRow) base *= 0.70f;
    if (defending) base *= 0.50f;
    return std::max(1, (int)roundf(base * r));
}

static int magicDamage(int mag, int power, int def, bool weak) {
    float r = 0.90f + hashf(GetRandomValue(0, 9999), (int)(g.totalTime * 100), 23u) * 0.20f;
    float base = std::max(2.0f, (power + mag * 1.6f) - def * 0.35f);
    if (weak) base *= 1.5f;
    return std::max(2, (int)roundf(base * r));
}

static FxKind spellFxKind(int fx) {
    switch (fx) {
        case 1: return FX_FIREBALL;
        case 2: return FX_SPARK;
        case 3: return FX_SPARKLE;
        case 4: return FX_SPARKLE;
        default: return FX_SLASH;
    }
}

static void checkBattleEnd();

static void damageFoe(int idx, int dmg, bool crit, bool weak, FxKind fx = FX_SLASH) {
    BattleEnemy& f = B.foes[idx];
    if (!f.alive) return;
    f.hp = std::max(0, f.hp - dmg);
    f.hitT = 0.26f;
    spawnFx(crit ? FX_IMPACT : fx, f.x, f.y - 16);
    char buf[32];
    snprintf(buf, sizeof(buf), crit ? "%d!" : "%d", dmg);
    Color col = weak ? Color{ 255, 224, 96, 255 } : (crit ? Color{ 255, 232, 130, 255 } : WHITE);
    spawnFloater(buf, f.x, f.y - 24, col);
    if (weak) spawnFloater("WEAK!", f.x, f.y - 36, Color{ 255, 185, 80, 255 });
    playSoundIf(fx == FX_SLASH ? g.assets.sword : g.assets.spell);
    if (f.hp <= 0) {
        f.alive = false;
        B.xpGain += f.xp;
        B.gilGain += f.gil;
        g.defeatedCount++;
        spawnFx(FX_POOF, f.x, f.y - 14, 0.45f);
        if (f.kind == EN_SLIME) {
            g.slimesSlain++;
            if (g.quests[QUEST_SLIMES].state == QS_ACTIVE)
                g.quests[QUEST_SLIMES].progress = std::min(6, g.slimesSlain);
        }
        if (f.kind == EN_SLIME_KING) {
            g.bossDefeated = true;
            if (itemCount(IT_CROWN) == 0) {
                addItem(IT_CROWN, 1, true);
                B.crownDropped = true;
            }
            if (g.quests[QUEST_CROWN].state == QS_ACTIVE)
                g.quests[QUEST_CROWN].progress = 1;
        } else if (f.drop >= 0 && f.drop < IT_COUNT && f.drop != IT_CROWN &&
                   GetRandomValue(0, 99) < f.dropChance) {
            addItem(f.drop, 1, true);
            B.rewards.push_back(std::string("Found ") + itemDef(f.drop).name + "!");
        }
    }
    checkBattleEnd();
}

static void damageHero(int idx, int dmg, bool crit, bool canPoison = false) {
    Hero& h = g.party[idx];
    if (h.ko) return;
    h.hp = std::max(0, h.hp - dmg);
    B.heroHitT[idx] = 0.28f;
    B.screenShake = std::max(B.screenShake, crit ? 0.22f : 0.12f);
    spawnFx(crit ? FX_IMPACT : FX_SLASH, heroX(idx), heroY(idx) - 10);
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", dmg);
    spawnFloater(buf, heroX(idx), heroY(idx) - 18, Color{ 255, 110, 110, 255 });
    playSoundIf(g.assets.hit);
    if (h.hp <= 0) {
        h.ko = true;
        h.poisoned = false;
        B.atb[idx] = 0;
        spawnFloater("K.O.", heroX(idx), heroY(idx) - 30, Color{ 255, 80, 80, 255 });
    } else if (canPoison && !h.poisoned && GetRandomValue(0, 99) < 22) {
        h.poisoned = true;
        spawnFloater("POISON!", heroX(idx), heroY(idx) - 30, Color{ 195, 115, 255, 255 });
    }
    checkBattleEnd();
}

static void checkBattleEnd() {
    if (B.phase == BP_VICTORY || B.phase == BP_DEFEAT || B.phase == BP_FLED) return;
    if (aliveFoeCount() == 0) {
        B.phase = BP_VICTORY;
        B.phaseT = 0.45f;
        g.gil += B.gilGain;
        if (B.fieldMonsterId != 0) g.defeatedMonsters.insert(B.fieldMonsterId);
        for (int i = 0; i < 3; i++) {
            Hero& h = g.party[i];
            int share = h.ko ? std::max(1, B.xpGain / 4) : B.xpGain;
            h.xp += share;
            if (levelUpCheck(h) > 0) {
                char buf[64];
                snprintf(buf, sizeof(buf), "%s reached Lv.%d!", heroName(h.klass), h.level);
                B.rewards.push_back(buf);
            }
        }
        if (B.crownDropped) {
            B.rewards.push_back("Reclaimed the Crown of Elm!");
        }
        playSoundIf(g.assets.levelup);
        stopAllMusic();
        if (g.musicOn && g.assets.victory.frameCount > 0) PlayMusicStream(g.assets.victory);
        setBanner(B.isBoss ? "The Slime King dissolves into harmless jelly!" : "Victory!", 4.0f);
    } else if (aliveHeroCount() == 0) {
        B.phase = BP_DEFEAT;
        B.phaseT = 1.4f;
        playSoundIf(g.assets.cancel);
        setBanner("The party has fallen...", 3.0f);
    }
}

// ================================================================ enemy turn
static void runEnemyTurn(int fi) {
    BattleEnemy& f = B.foes[fi];
    if (!f.alive) return;
    f.atb = 0.0f;
    f.lungeT = 0.24f;

    std::vector<int> targets;
    for (int i = 0; i < 3; i++) if (!g.party[i].ko) targets.push_back(i);
    if (targets.empty()) return;

    // Slime King special: Royal Quake AoE
    if (f.boss && GetRandomValue(0, 99) < 35) {
        setBanner("Slime King uses Royal Quake!", 1.5f);
        for (int ti : targets) {
            int dmg = magicDamage(f.atk / 2, 14, heroDef(g.party[ti]), false);
            if (B.defending[ti]) dmg = std::max(1, dmg / 2);
            damageHero(ti, dmg, false, true);
            if (B.phase != BP_ACTIVE) break;
        }
        return;
    }

    // Arcane enemies cast Shadow Bolt
    if ((f.kind == EN_WISP || f.kind == EN_GHOST || f.kind == EN_NIGHT_RIDER) && GetRandomValue(0, 99) < 40) {
        int ti = targets[GetRandomValue(0, (int)targets.size() - 1)];
        int dmg = magicDamage(f.atk / 2 + 4, 10, heroDef(g.party[ti]), false);
        if (B.defending[ti]) dmg = std::max(1, dmg / 2);
        char b[96];
        snprintf(b, sizeof(b), "%s casts Shadow Bolt!", enemyDef(f.kind).name);
        setBanner(b, 1.3f);
        spawnFx(FX_SPARK, heroX(ti), heroY(ti) - 10);
        damageHero(ti, dmg, false, false);
        return;
    }

    int ti = targets[GetRandomValue(0, (int)targets.size() - 1)];
    bool crit = false;
    int dmg = physicalDamage(f.atk, heroDef(g.party[ti]), g.party[ti].row == 1, B.defending[ti], crit);
    bool poisonous = (f.kind == EN_SLIME || f.kind == EN_HORNET || f.kind == EN_SERPENT || f.kind == EN_SLIME_KING);
    char b[96];
    snprintf(b, sizeof(b), "%s strikes %s!", enemyDef(f.kind).name, heroName(g.party[ti].klass));
    setBanner(b, 1.2f);
    damageHero(ti, dmg, crit, poisonous);
}

// ================================================================ hero actions
static std::vector<int> usableBattleItems() {
    std::vector<int> out;
    for (int i = 0; i < IT_COUNT; i++)
        if (g.inventory[i] > 0 && itemDef(i).kind == IK_CONSUMABLE) out.push_back(i);
    return out;
}

static void endHeroTurn() {
    if (B.activeHero >= 0 && B.activeHero < 3) B.atb[B.activeHero] = 0.0f;
    B.activeHero = -1;
    gPendingSpellIdx = -1;
    gPendingItem = -1;
    if (B.phase != BP_VICTORY && B.phase != BP_DEFEAT && B.phase != BP_FLED)
        B.phase = BP_ACTIVE;
}

static void executeHeroAttack(int hi, int fi) {
    Hero& h = g.party[hi];
    B.heroLungeT[hi] = 0.22f;
    bool crit = false;
    int dmg = physicalDamage(heroAtk(h), B.foes[fi].def, h.row == 1, false, crit);
    char b[96];
    snprintf(b, sizeof(b), "%s attacks %s!", heroName(h.klass), enemyDef(B.foes[fi].kind).name);
    setBanner(b, 1.2f);
    damageFoe(fi, dmg, crit, false, FX_SLASH);
    endHeroTurn();
}

static void executeHeroSpellOnFoe(int hi, const SpellDef& sp, int fi) {
    Hero& h = g.party[hi];
    if (h.mp < sp.cost) { playSoundIf(g.assets.cancel); setBanner("Not enough MP!", 1.2f); return; }
    h.mp -= sp.cost;
    B.heroLungeT[hi] = 0.24f;
    char b[96];
    snprintf(b, sizeof(b), "%s uses %s!", heroName(h.klass), sp.name);
    setBanner(b, 1.3f);
    FxKind fx = spellFxKind(sp.fx);

    auto hitOne = [&](int idx) {
        bool weak = (sp.school != SCH_NONE) && ((B.foes[idx].weak & (1 << sp.school)) != 0);
        if (sp.school == SCH_PHYS) {
            bool crit = false;
            int dmg = physicalDamage(heroAtk(h) + sp.power / 2, B.foes[idx].def, false, false, crit);
            damageFoe(idx, dmg, crit, weak, fx);
        } else {
            int dmg = magicDamage(heroMag(h), sp.power, B.foes[idx].def, weak);
            damageFoe(idx, dmg, false, weak, fx);
        }
    };

    if (sp.aoe) {
        for (int i = 0; i < (int)B.foes.size(); i++) {
            if (!B.foes[i].alive) continue;
            hitOne(i);
            if (B.phase == BP_VICTORY) break;
        }
    } else if (fi >= 0 && fi < (int)B.foes.size() && B.foes[fi].alive) {
        hitOne(fi);
    }
    endHeroTurn();
}

static void executeHeroHealSpell(int hi, const SpellDef& sp, int targetHero) {
    Hero& h = g.party[hi];
    if (h.mp < sp.cost) { playSoundIf(g.assets.cancel); setBanner("Not enough MP!", 1.2f); return; }

    bool isRevive = (strcmp(sp.name, "Revive") == 0);
    if (isRevive) {
        Hero& t = g.party[targetHero];
        if (!t.ko) {
            playSoundIf(g.assets.cancel);
            setBanner("That ally is not fallen!", 1.2f);
            return;
        }
        h.mp -= sp.cost;
        t.ko = false;
        t.poisoned = false;
        t.hp = std::max(1, t.maxHp / 2);
        B.atb[targetHero] = 0.25f;
        spawnFx(FX_SPARKLE, heroX(targetHero), heroY(targetHero) - 10);
        spawnFloater("REVIVE!", heroX(targetHero), heroY(targetHero) - 22, Color{ 255, 240, 140, 255 });
        playSoundIf(g.assets.heal);
        setBanner(std::string(heroName(h.klass)) + " revives " + heroName(t.klass) + "!", 1.4f);
        endHeroTurn();
        return;
    }

    h.mp -= sp.cost;
    int amt = sp.power + heroMag(h) * 2;
    if (sp.aoe) {
        for (int i = 0; i < 3; i++) {
            if (g.party[i].ko) continue;
            g.party[i].hp = std::min(g.party[i].maxHp, g.party[i].hp + amt);
            spawnFx(FX_SPARKLE, heroX(i), heroY(i) - 10);
            spawnFloater("+" + std::to_string(amt), heroX(i), heroY(i) - 20, Color{ 110, 255, 140, 255 });
        }
        setBanner(std::string(heroName(h.klass)) + " casts " + sp.name + "!", 1.4f);
    } else {
        Hero& t = g.party[targetHero];
        if (!t.ko) {
            t.hp = std::min(t.maxHp, t.hp + amt);
            if (strcmp(sp.name, "Last Stand") == 0) B.defending[targetHero] = true;
            spawnFx(FX_SPARKLE, heroX(targetHero), heroY(targetHero) - 10);
            spawnFloater("+" + std::to_string(amt), heroX(targetHero), heroY(targetHero) - 20, Color{ 110, 255, 140, 255 });
        }
        setBanner(std::string(heroName(h.klass)) + " casts " + sp.name + " on " + heroName(t.klass) + "!", 1.3f);
    }
    playSoundIf(g.assets.heal);
    endHeroTurn();
}

static void executeHeroItem(int hi, int id, int targetHero) {
    Hero& t = g.party[targetHero];
    bool wasPoisoned = t.poisoned;
    if (applyConsumable(id, targetHero)) {
        spawnFx(FX_SPARKLE, heroX(targetHero), heroY(targetHero) - 10);
        if (id == IT_PHOENIX) {
            B.atb[targetHero] = 0.25f;
            spawnFloater("REVIVE!", heroX(targetHero), heroY(targetHero) - 20, Color{ 255, 240, 140, 255 });
        } else if (id == IT_ANTIDOTE) {
            spawnFloater(wasPoisoned ? "CURED!" : "+20 HP", heroX(targetHero), heroY(targetHero) - 20,
                         Color{ 170, 255, 190, 255 });
        } else if (id == IT_ETHER || id == IT_HIETHER) {
            spawnFloater("+" + std::to_string(itemDef(id).power) + " MP", heroX(targetHero), heroY(targetHero) - 20,
                         Color{ 120, 190, 255, 255 });
        } else if (id == IT_ELIXIR) {
            spawnFloater("FULL!", heroX(targetHero), heroY(targetHero) - 20, Color{ 255, 240, 140, 255 });
        } else {
            spawnFloater("+" + std::to_string(itemDef(id).power), heroX(targetHero), heroY(targetHero) - 20,
                         Color{ 110, 255, 140, 255 });
        }
        playSoundIf(g.assets.potion);
        setBanner(std::string(heroName(g.party[hi].klass)) + " uses " + itemDef(id).name + " on " + heroName(t.klass) + "!", 1.3f);
        endHeroTurn();
    } else {
        playSoundIf(g.assets.cancel);
        setBanner("It had no effect!", 1.0f);
    }
}

// ================================================================ shared layout geometry
static Rectangle cmdRect(int i)     { return { 14.0f,  (float)(194 + i * 17), 90.0f,  15.0f }; }
static Rectangle subListRect(int i) { return { 114.0f, (float)(194 + i * 16), 144.0f, 15.0f }; }

// ================================================================ update
void battleUpdate(float dt) {
    gAnimTime += dt;
    g.totalTime += dt;
    g.runTime += dt;
    if (g.toastT > 0) g.toastT = std::max(0.0f, g.toastT - dt);
    if (B.bannerT > 0) B.bannerT = std::max(0.0f, B.bannerT - dt);
    if (B.screenShake > 0) B.screenShake = std::max(0.0f, B.screenShake - dt);
    for (int i = 0; i < 3; i++) {
        if (B.heroHitT[i] > 0) B.heroHitT[i] = std::max(0.0f, B.heroHitT[i] - dt);
        if (B.heroLungeT[i] > 0) B.heroLungeT[i] = std::max(0.0f, B.heroLungeT[i] - dt);
    }
    for (auto& f : B.foes) {
        if (f.hitT > 0) f.hitT = std::max(0.0f, f.hitT - dt);
        if (f.lungeT > 0) f.lungeT = std::max(0.0f, f.lungeT - dt);
    }
    for (auto& fl : B.floaters) { fl.y += fl.vy * dt; fl.life -= dt; }
    B.floaters.erase(std::remove_if(B.floaters.begin(), B.floaters.end(),
                                    [](const Floater& f) { return f.life <= 0; }), B.floaters.end());
    for (auto& e : B.fx) e.life -= dt;
    B.fx.erase(std::remove_if(B.fx.begin(), B.fx.end(),
                              [](const BattleFx& e) { return e.life <= 0; }), B.fx.end());

    if (B.phase == BP_INTRO) {
        B.phaseT -= dt;
        if (B.phaseT <= 0) B.phase = BP_ACTIVE;
        return;
    }

    if (B.phase == BP_VICTORY || B.phase == BP_FLED) {
        B.phaseT = std::max(0.0f, B.phaseT - dt);
        if (B.phaseT <= 0 && (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) || IsMouseButtonPressed(MOUSE_LEFT_BUTTON))) {
            g.mode = MODE_OVERWORLD;
            g.fade = 1.0f; g.fadeDir = -1;
            g.encounterGrace = 3.5f;
            syncModeMusic();
        }
        return;
    }

    if (B.phase == BP_DEFEAT) {
        B.phaseT -= dt;
        if (B.phaseT <= 0) {
            g.mode = MODE_GAMEOVER;
            g.fade = 1.0f; g.fadeDir = -1;
            syncModeMusic();
        }
        return;
    }

    // Advance ATB gauges (respects WAIT mode when browsing sub-menus / targets)
    bool pauseAtb = g.waitBattle && (B.phase == BP_MAGIC || B.phase == BP_ITEM ||
                                     B.phase == BP_TARGET_FOE || B.phase == BP_TARGET_ALLY);
    if (!pauseAtb) {
        for (int i = 0; i < 3; i++) {
            Hero& h = g.party[i];
            if (h.ko || i == B.activeHero) continue;
            B.atb[i] = std::min(1.0f, B.atb[i] + dt * (0.14f + h.spd * 0.014f));
        }
        for (int i = 0; i < (int)B.foes.size(); i++) {
            BattleEnemy& f = B.foes[i];
            if (!f.alive) continue;
            f.atb += dt * (0.12f + f.spd * 0.012f);
            if (f.atb >= 1.0f && B.phase == BP_ACTIVE) {
                runEnemyTurn(i);
                if (B.phase != BP_ACTIVE) return;
            }
        }
    }

    // Ready hero opens command tray
    if (B.phase == BP_ACTIVE) {
        for (int i = 0; i < 3; i++) {
            if (!g.party[i].ko && B.atb[i] >= 1.0f) {
                B.activeHero = i;
                B.defending[i] = false;
                if (g.party[i].poisoned) {
                    int pdmg = std::max(1, g.party[i].maxHp / 14);
                    g.party[i].hp = std::max(1, g.party[i].hp - pdmg);
                    spawnFloater("-" + std::to_string(pdmg) + " PSN", heroX(i), heroY(i) - 24,
                                 Color{ 195, 115, 255, 255 });
                }
                B.phase = BP_COMMAND;
                B.cmdCursor = 0;
                gPendingSpellIdx = -1;
                gPendingItem = -1;
                playSoundIf(g.assets.cursor);
                return;
            }
        }
        return;
    }

    if (B.activeHero >= 0 && g.party[B.activeHero].ko) {
        endHeroTurn();
        return;
    }

    Vector2 m = getVirtualMouse();
    bool clicked = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

    // ---- Command menu (0=Attack, 1=Skill, 2=Item, 3=Defend, 4=Flee)
    if (B.phase == BP_COMMAND) {
        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) { B.cmdCursor = (B.cmdCursor + 4) % 5; playSoundIf(g.assets.cursor); }
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) { B.cmdCursor = (B.cmdCursor + 1) % 5; playSoundIf(g.assets.cursor); }

        auto triggerCmd = [&](int c) {
            gPendingSpellIdx = -1;
            gPendingItem = -1;
            if (c == 0) {
                B.targetCursor = firstAliveFoe();
                B.phase = BP_TARGET_FOE;
                playSoundIf(g.assets.confirm);
            } else if (c == 1) {
                SpellList sl = spellsFor(g.party[B.activeHero].klass, g.party[B.activeHero].level);
                if (sl.count == 0) {
                    playSoundIf(g.assets.cancel);
                    setBanner("No skills learned yet.", 1.1f);
                } else {
                    B.subCursor = 0; B.phase = BP_MAGIC; playSoundIf(g.assets.confirm);
                }
            } else if (c == 2) {
                if (usableBattleItems().empty()) {
                    playSoundIf(g.assets.cancel);
                    setBanner("No usable items in bag.", 1.1f);
                } else {
                    B.subCursor = 0; B.phase = BP_ITEM; playSoundIf(g.assets.confirm);
                }
            } else if (c == 3) {
                B.defending[B.activeHero] = true;
                setBanner(std::string(heroName(g.party[B.activeHero].klass)) + " braces for impact.", 1.1f);
                playSoundIf(g.assets.confirm);
                endHeroTurn();
            } else if (c == 4) {
                if (B.isBoss) {
                    playSoundIf(g.assets.cancel);
                    setBanner("You can't flee from the Slime King!", 1.5f);
                } else if (GetRandomValue(0, 99) < 68) {
                    B.phase = BP_FLED;
                    B.phaseT = 0.25f;
                    playSoundIf(g.assets.flee);
                    setBanner("Escaped safely!", 1.6f);
                } else {
                    playSoundIf(g.assets.cancel);
                    setBanner("Couldn't get away!", 1.2f);
                    endHeroTurn();
                }
            }
        };

        if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) triggerCmd(B.cmdCursor);
        if (clicked) {
            for (int i = 0; i < 5; i++) {
                if (CheckCollisionPointRec(m, cmdRect(i))) { B.cmdCursor = i; triggerCmd(i); return; }
            }
            for (int i = 0; i < (int)B.foes.size(); i++) {
                if (!B.foes[i].alive) continue;
                if (CheckCollisionPointRec(m, { B.foes[i].x - 24, B.foes[i].y - 44, 48, 48 })) {
                    executeHeroAttack(B.activeHero, i);
                    return;
                }
            }
        }
        return;
    }

    // ---- Magic / Skills
    if (B.phase == BP_MAGIC) {
        SpellList sl = spellsFor(g.party[B.activeHero].klass, g.party[B.activeHero].level);
        if (sl.count == 0 || IsKeyPressed(KEY_ESCAPE)) { B.phase = BP_COMMAND; playSoundIf(g.assets.cancel); return; }
        if (B.subCursor >= sl.count) B.subCursor = 0;
        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) { B.subCursor = (B.subCursor + sl.count - 1) % sl.count; playSoundIf(g.assets.cursor); }
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) { B.subCursor = (B.subCursor + 1) % sl.count; playSoundIf(g.assets.cursor); }

        auto chooseSpell = [&](int idx) {
            const SpellDef& sp = sl.spells[idx];
            if (g.party[B.activeHero].mp < sp.cost) {
                playSoundIf(g.assets.cancel);
                setBanner("Not enough MP!", 1.1f);
                return;
            }
            gPendingSpellIdx = idx;
            gPendingItem = -1;
            if (sp.ally) {
                if (sp.aoe) {
                    executeHeroHealSpell(B.activeHero, sp, 0);
                } else {
                    B.targetCursor = B.activeHero;
                    if (strcmp(sp.name, "Revive") == 0) {
                        for (int k = 0; k < 3; k++) if (g.party[k].ko) { B.targetCursor = k; break; }
                    }
                    B.phase = BP_TARGET_ALLY;
                    playSoundIf(g.assets.confirm);
                }
            } else if (sp.aoe) {
                executeHeroSpellOnFoe(B.activeHero, sp, firstAliveFoe());
            } else {
                B.targetCursor = firstAliveFoe();
                B.phase = BP_TARGET_FOE;
                playSoundIf(g.assets.confirm);
            }
        };

        if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) chooseSpell(B.subCursor);
        if (clicked) {
            for (int i = 0; i < sl.count && i < 5; i++) {
                if (CheckCollisionPointRec(m, subListRect(i))) { B.subCursor = i; chooseSpell(i); return; }
            }
        }
        return;
    }

    // ---- Items
    if (B.phase == BP_ITEM) {
        auto items = usableBattleItems();
        int n = (int)items.size();
        if (n == 0 || IsKeyPressed(KEY_ESCAPE)) { B.phase = BP_COMMAND; playSoundIf(g.assets.cancel); return; }
        if (B.subCursor >= n) B.subCursor = 0;
        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) { B.subCursor = (B.subCursor + n - 1) % n; playSoundIf(g.assets.cursor); }
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) { B.subCursor = (B.subCursor + 1) % n; playSoundIf(g.assets.cursor); }

        auto chooseItem = [&](int idx) {
            gPendingItem = items[idx];
            gPendingSpellIdx = -1;
            B.targetCursor = B.activeHero;
            if (gPendingItem == IT_PHOENIX) {
                for (int k = 0; k < 3; k++) if (g.party[k].ko) { B.targetCursor = k; break; }
            }
            B.phase = BP_TARGET_ALLY;
            playSoundIf(g.assets.confirm);
        };

        if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) chooseItem(B.subCursor);
        if (clicked) {
            for (int i = 0; i < n && i < 5; i++) {
                if (CheckCollisionPointRec(m, subListRect(i))) { B.subCursor = i; chooseItem(i); return; }
            }
        }
        return;
    }

    // ---- Target enemy
    if (B.phase == BP_TARGET_FOE) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            gPendingSpellIdx = -1;
            B.phase = BP_COMMAND;
            playSoundIf(g.assets.cancel);
            return;
        }
        if (B.targetCursor < 0 || B.targetCursor >= (int)B.foes.size() || !B.foes[B.targetCursor].alive)
            B.targetCursor = firstAliveFoe();
        if (B.targetCursor < 0) { checkBattleEnd(); return; }

        int dir = 0;
        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_W) || IsKeyPressed(KEY_A)) dir = -1;
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_S) || IsKeyPressed(KEY_D)) dir = 1;
        if (dir != 0) {
            int n = (int)B.foes.size();
            for (int step = 1; step <= n; step++) {
                int cand = (B.targetCursor + dir * step + n) % n;
                if (B.foes[cand].alive) { B.targetCursor = cand; playSoundIf(g.assets.cursor); break; }
            }
        }
        auto commitFoe = [&](int fi) {
            if (gPendingSpellIdx >= 0) {
                SpellList sl = spellsFor(g.party[B.activeHero].klass, g.party[B.activeHero].level);
                if (gPendingSpellIdx < sl.count) {
                    executeHeroSpellOnFoe(B.activeHero, sl.spells[gPendingSpellIdx], fi);
                    return;
                }
            }
            executeHeroAttack(B.activeHero, fi);
        };
        if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) { commitFoe(B.targetCursor); return; }
        if (clicked) {
            for (int i = 0; i < (int)B.foes.size(); i++) {
                if (!B.foes[i].alive) continue;
                if (CheckCollisionPointRec(m, { B.foes[i].x - 28, B.foes[i].y - 48, 56, 54 })) {
                    commitFoe(i);
                    return;
                }
            }
        }
        return;
    }

    // ---- Target ally
    if (B.phase == BP_TARGET_ALLY) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            gPendingSpellIdx = -1;
            gPendingItem = -1;
            B.phase = BP_COMMAND;
            playSoundIf(g.assets.cancel);
            return;
        }
        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W) || IsKeyPressed(KEY_LEFT)) {
            B.targetCursor = (B.targetCursor + 2) % 3; playSoundIf(g.assets.cursor);
        }
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S) || IsKeyPressed(KEY_RIGHT)) {
            B.targetCursor = (B.targetCursor + 1) % 3; playSoundIf(g.assets.cursor);
        }
        auto commitAlly = [&](int ti) {
            if (gPendingSpellIdx >= 0) {
                SpellList sl = spellsFor(g.party[B.activeHero].klass, g.party[B.activeHero].level);
                if (gPendingSpellIdx < sl.count) { executeHeroHealSpell(B.activeHero, sl.spells[gPendingSpellIdx], ti); return; }
            }
            if (gPendingItem >= 0) executeHeroItem(B.activeHero, gPendingItem, ti);
        };
        if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) { commitAlly(B.targetCursor); return; }
        if (clicked) {
            for (int i = 0; i < 3; i++) {
                Rectangle spriteBox{ heroX(i) - 18, heroY(i) - 26, 36, 34 };
                Rectangle cardBox{ 266.0f, (float)(194 + i * 27), 232.0f, 25.0f };
                if (CheckCollisionPointRec(m, spriteBox) || CheckCollisionPointRec(m, cardBox)) {
                    commitAlly(i);
                    return;
                }
            }
        }
    }
}

// ================================================================ draw
static Texture2D* fxTex(FxKind k) {
    switch (k) {
        case FX_SLASH:    return &g.assets.fxSlash;
        case FX_FIREBALL: return &g.assets.fxFireball;
        case FX_SPARK:    return &g.assets.fxSpark;
        case FX_SPARKLE:  return &g.assets.fxSparkle;
        case FX_IMPACT:   return &g.assets.fxImpact;
        case FX_POOF:     return &g.assets.fxPoof;
    }
    return &g.assets.fxSlash;
}

static void drawBattleBackdrop(float sx, float sy) {
    GroundTile gt = sampleGround((int)floorf(g.px), (int)floorf(g.py));
    Color skyTop{ 34, 62, 120, 255 }, skyBot{ 126, 168, 224, 255 };
    Texture2D* ground = &g.assets.grass[0];
    if (B.isBoss) {
        skyTop = { 24, 16, 44, 255 }; skyBot = { 76, 44, 92, 255 };
        ground = &g.assets.flagstone;
    } else if (gt.kind == G_SNOW || gt.kind == G_ICE) {
        skyTop = { 62, 92, 140, 255 }; skyBot = { 196, 222, 248, 255 };
        ground = &g.assets.snow[0];
    } else if (gt.kind == G_SAND) {
        skyTop = { 64, 112, 178, 255 }; skyBot = { 235, 206, 152, 255 };
        ground = &g.assets.sand;
    } else if (gt.kind == G_SWAMP) {
        skyTop = { 28, 44, 54, 255 }; skyBot = { 76, 108, 92, 255 };
        ground = &g.assets.swamp;
    } else if (gt.kind == G_ROCK) {
        skyTop = { 46, 54, 84, 255 }; skyBot = { 132, 144, 174, 255 };
        ground = &g.assets.rockHill[0];
    }

    DrawRectangleGradientV((int)sx, (int)sy, VRES_W, 88, skyTop, skyBot);
    // Distant hills silhouette
    for (int x = 0; x < VRES_W; x += 8) {
        float h = 16.0f + sinf(x * 0.018f + 1.2f) * 9.0f + cosf(x * 0.043f) * 5.0f;
        DrawRectangle((int)(x + sx), (int)(86 - h + sy), 9, (int)h + 6,
                      B.isBoss ? Color{ 44, 30, 62, 255 } : Color{ 56, 88, 72, 255 });
    }
    // Tiled battleground floor
    for (int y = 84; y < 190; y += TILE) {
        for (int x = 0; x < VRES_W; x += TILE) {
            if (ground && ground->id) {
                DrawTexturePro(*ground, { 0, 0, (float)ground->width, (float)ground->height },
                               { x + sx, y + sy, TILE, TILE }, { 0, 0 }, 0.0f, WHITE);
            } else {
                DrawRectangle((int)(x + sx), (int)(y + sy), TILE, TILE, Color{ 68, 128, 64, 255 });
            }
        }
    }
    DrawRectangleGradientV((int)sx, (int)(84 + sy), VRES_W, 18, Fade(BLACK, 0.25f), BLANK);
}

void battleDraw() {
    float sx = (B.screenShake > 0) ? sinf(g.totalTime * 70.0f) * 3.0f : 0.0f;
    float sy = (B.screenShake > 0) ? cosf(g.totalTime * 57.0f) * 2.0f : 0.0f;

    drawBattleBackdrop(sx, sy);

    // Enemies (left side)
    for (int i = 0; i < (int)B.foes.size(); i++) {
        const BattleEnemy& f = B.foes[i];
        if (!f.alive) continue;
        Texture2D& t = (g.assets.enemyBig[f.kind].id ? g.assets.enemyBig[f.kind] : g.assets.enemy[f.kind]);
        float lunge = (f.lungeT > 0) ? sinf(f.lungeT / 0.24f * 3.14159f) * 14.0f : 0.0f;
        float bob = sinf(g.totalTime * 3.2f + i * 1.3f) * 1.8f;
        float scale = f.boss ? 0.68f : 0.56f;
        float ex = f.x + lunge + sx;
        float ey = f.y + sy;
        DrawEllipse((int)ex, (int)ey - 2, t.width * scale * 0.34f, 4.0f, Fade(BLACK, 0.30f));
        Color tint = (f.hitT > 0 && ((int)(f.hitT * 30) & 1)) ? Color{ 255, 130, 130, 255 } : WHITE;
        // Battle sprites are drawn facing left; flip horizontally so foes on the left face right toward the party.
        float srcW = (f.kind == EN_SLIME || f.kind == EN_GHOST || f.kind == EN_SLIME_KING)
            ? (float)t.width : -(float)t.width;
        DrawTexturePro(t, { 0, 0, srcW, (float)t.height },
                       { ex - t.width * scale * 0.5f, ey - t.height * scale + bob,
                         t.width * scale, t.height * scale },
                       { 0, 0 }, 0.0f, tint);
        float bw = f.boss ? 56.0f : 34.0f;
        drawBar(ex - bw * 0.5f, ey + 3, bw, 4, (float)f.hp / std::max(1, f.maxHp),
                f.boss ? Color{ 255, 190, 70, 255 } : Color{ 235, 95, 95, 255 }, Color{ 26, 20, 32, 220 });
        if (B.phase == BP_TARGET_FOE && B.targetCursor == i) {
            float ptrBob = sinf(g.totalTime * 8.0f) * 2.5f;
            drawTextCentered("v", ex, ey - t.height * scale - 12 + ptrBob, Color{ 255, 232, 120, 255 }, 8);
            drawTextCentered(enemyDef(f.kind).name, ex, ey - t.height * scale - 22 + ptrBob, WHITE, 8);
        }
    }

    // Heroes (right side, facing left)
    for (int i = 0; i < 3; i++) {
        const Hero& h = g.party[i];
        float lunge = (B.heroLungeT[i] > 0) ? -sinf(B.heroLungeT[i] / 0.22f * 3.14159f) * 16.0f : 0.0f;
        float hx = heroX(i) + lunge + sx;
        float hy = heroY(i) + sy;
        DrawEllipse((int)hx, (int)hy - 1, 7.0f, 3.0f, Fade(BLACK, 0.30f));

        Texture2D& sheet = g.assets.heroWalk[std::max(0, std::min(2, h.klass))];
        int frame = (B.activeHero == i) ? (1 + ((int)(g.totalTime * 5.0f) & 1)) : 0;
        Color tint = WHITE;
        if (h.ko) tint = Color{ 130, 120, 150, 210 };
        else if (B.heroHitT[i] > 0 && ((int)(B.heroHitT[i] * 30) & 1)) tint = Color{ 255, 130, 130, 255 };
        else if (h.poisoned) tint = Color{ 215, 175, 255, 255 };

        Rectangle src{ (float)(frame * 24), 64.0f, -24.0f, 32.0f };
        float rot = h.ko ? 90.0f : 0.0f;
        DrawTexturePro(sheet, src, { hx - 12, hy - 30, 24, 32 },
                       h.ko ? Vector2{ 12, 16 } : Vector2{ 0, 0 }, rot, tint);

        if (B.defending[i] && !h.ko)
            DrawRing({ hx, hy - 14 }, 13, 15, 0, 360, 18, Fade(Color{ 130, 200, 255, 255 }, 0.55f));
        if (B.activeHero == i && !h.ko)
            drawTextCentered(">", hx - 18, hy - 18, Color{ 255, 232, 120, 255 }, 8);
        if (B.phase == BP_TARGET_ALLY && B.targetCursor == i) {
            float ptrBob = sinf(g.totalTime * 8.0f) * 2.5f;
            drawTextCentered("v", hx, hy - 40 + ptrBob, Color{ 120, 255, 160, 255 }, 8);
        }
    }

    // FX sprites
    for (const auto& e : B.fx) {
        Texture2D* t = fxTex(e.kind);
        if (!t || t->id == 0) continue;
        float a = std::max(0.0f, e.life / std::max(0.01f, e.maxLife));
        float sc = 1.0f + (1.0f - a) * 0.45f;
        DrawTexturePro(*t, { 0, 0, (float)t->width, (float)t->height },
                       { e.x - t->width * sc * 0.5f, e.y - t->height * sc * 0.5f,
                         t->width * sc, t->height * sc },
                       { 0, 0 }, 0.0f, Fade(WHITE, a));
    }

    // Floating numbers
    for (const auto& fl : B.floaters) {
        float a = std::min(1.0f, fl.life * 2.0f);
        drawTextCentered(fl.text.c_str(), fl.x + 1, fl.y + 1, Fade(BLACK, a), 8);
        drawTextCentered(fl.text.c_str(), fl.x,     fl.y,     Fade(fl.col, a), 8);
    }

    // Top action banner & targeting hint (never overlaps bottom HUD)
    if (B.bannerT > 0 && !B.banner.empty()) {
        drawWindow({ 28, 6, VRES_W - 56.0f, 22 });
        drawTextCentered(B.banner.c_str(), VRES_W * 0.5f, 13, Color{ 255, 236, 176, 255 }, 8);
    }
    if (B.phase == BP_TARGET_FOE || B.phase == BP_TARGET_ALLY) {
        const char* hint = (B.phase == BP_TARGET_FOE)
            ? "Pick enemy (Arrows/Click, SPACE confirm, ESC back)"
            : "Pick ally (Arrows/Click, SPACE confirm, ESC back)";
        drawWindow({ 60, 30, VRES_W - 120.0f, 18 });
        drawTextCentered(hint, VRES_W * 0.5f, 35, Color{ 170, 240, 195, 255 }, 8);
    }

    // Bottom combat tray
    DrawRectangle(0, 186, VRES_W, VRES_H - 186, Color{ 10, 14, 30, 255 });
    drawWindow({ 6, 188, 104, 94 });
    const char* cmds[5] = { "Attack", "Skill", "Item", "Defend", "Flee" };
    for (int i = 0; i < 5; i++) {
        Rectangle r = cmdRect(i);
        bool sel = (B.phase == BP_COMMAND && B.cmdCursor == i);
        bool dis = (i == 4 && B.isBoss);
        if (sel) {
            DrawRectangleRounded(r, 0.3f, 4, Color{ 56, 86, 164, 255 });
            DrawRectangleRoundedLinesEx(r, 0.3f, 4, 1.0f, Color{ 255, 232, 150, 255 });
        }
        Color c = dis ? Color{ 130, 130, 155, 220 }
                : (B.phase == BP_COMMAND ? (sel ? Color{ 255, 236, 176, 255 } : WHITE)
                                         : Color{ 165, 175, 210, 220 });
        drawText(cmds[i], r.x + 8, r.y + 4, c, 8);
    }

    // Middle panel (sub-list OR living enemies)
    drawWindow({ 110, 188, 152, 94 });
    if (B.phase == BP_MAGIC && B.activeHero >= 0) {
        SpellList sl = spellsFor(g.party[B.activeHero].klass, g.party[B.activeHero].level);
        for (int i = 0; i < sl.count && i < 5; i++) {
            Rectangle r = subListRect(i);
            bool sel = (B.subCursor == i);
            if (sel) DrawRectangle((int)r.x, (int)r.y, (int)r.width, (int)r.height, Color{ 56, 86, 164, 255 });
            const SpellDef& sp = sl.spells[i];
            bool ok = g.party[B.activeHero].mp >= sp.cost;
            drawText(sp.name, r.x + 4, r.y + 3, ok ? WHITE : Color{ 140, 145, 175, 255 }, 8);
            char mp[16]; snprintf(mp, sizeof(mp), "%dM", sp.cost);
            Vector2 sz = measureText(mp, 8);
            drawText(mp, r.x + r.width - sz.x - 4, r.y + 3,
                     ok ? Color{ 130, 195, 255, 255 } : Color{ 210, 110, 110, 255 }, 8);
        }
    } else if (B.phase == BP_ITEM) {
        auto items = usableBattleItems();
        for (int i = 0; i < (int)items.size() && i < 5; i++) {
            Rectangle r = subListRect(i);
            bool sel = (B.subCursor == i);
            if (sel) DrawRectangle((int)r.x, (int)r.y, (int)r.width, (int)r.height, Color{ 56, 86, 164, 255 });
            drawText(itemDef(items[i]).name, r.x + 4, r.y + 3, WHITE, 8);
            char q[16]; snprintf(q, sizeof(q), "x%d", itemCount(items[i]));
            Vector2 sz = measureText(q, 8);
            drawText(q, r.x + r.width - sz.x - 4, r.y + 3, Color{ 255, 232, 150, 255 }, 8);
        }
    } else {
        int row = 0;
        for (int i = 0; i < (int)B.foes.size(); i++) {
            if (!B.foes[i].alive) continue;
            float y = 196.0f + row * 17.0f;
            bool tgt = (B.phase == BP_TARGET_FOE && B.targetCursor == i);
            if (tgt) DrawRectangle(114, (int)y - 1, 144, 15, Color{ 56, 86, 164, 230 });
            drawText(enemyDef(B.foes[i].kind).name, 118, y + 2,
                     tgt ? Color{ 255, 236, 176, 255 } : Color{ 225, 232, 250, 255 }, 8);
            char hp[24]; snprintf(hp, sizeof(hp), "%d", B.foes[i].hp);
            Vector2 sz = measureText(hp, 8);
            drawText(hp, 254 - sz.x, y + 2, Color{ 245, 165, 165, 255 }, 8);
            row++;
        }
    }

    // Right panel: party status & ATB bars
    drawWindow({ 264, 188, 242, 94 });
    for (int i = 0; i < 3; i++) {
        const Hero& h = g.party[i];
        float y = 195.0f + i * 28.0f;
        bool act = (B.activeHero == i);
        if (act) DrawRectangleRounded({ 268, y - 2, 234, 25 }, 0.25f, 4, Color{ 44, 68, 138, 220 });
        Color nc = h.ko ? Color{ 225, 110, 110, 255 }
                 : (h.poisoned ? Color{ 200, 145, 255, 255 }
                               : (act ? Color{ 255, 236, 176, 255 } : WHITE));
        drawText(heroName(h.klass), 273, y + 1, nc, 8);
        if (h.ko) drawText("KO", 322, y + 1, Color{ 255, 110, 110, 255 }, 8);
        else if (h.poisoned) drawText("PSN", 318, y + 1, Color{ 195, 125, 255, 255 }, 8);

        char hp[32]; snprintf(hp, sizeof(hp), "%d/%d", h.hp, h.maxHp);
        drawText(hp, 344, y + 1, WHITE, 8);
        char mp[24]; snprintf(mp, sizeof(mp), "%dM", h.mp);
        Vector2 msz = measureText(mp, 8);
        drawText(mp, 444 - msz.x, y + 1, Color{ 130, 195, 255, 255 }, 8);

        drawBar(273, y + 13, 114, 5, (float)h.hp / std::max(1, h.maxHp),
                Color{ 96, 220, 120, 255 }, Color{ 24, 34, 68, 255 });
        drawBar(450, y + 4, 48, 8, B.atb[i],
                B.atb[i] >= 1.0f ? Color{ 255, 230, 110, 255 } : Color{ 110, 180, 255, 255 },
                Color{ 24, 34, 68, 255 });
    }

    // Victory / Fled summary modal
    if (B.phase == BP_VICTORY || B.phase == BP_FLED) {
        DrawRectangle(0, 0, VRES_W, VRES_H, Fade(BLACK, 0.45f));
        int showRows = std::min(5, (int)B.rewards.size());
        float mh = (B.phase == BP_VICTORY) ? (96.0f + (float)showRows * 14.0f) : 70.0f;
        float my = VRES_H * 0.5f - mh * 0.5f - 10.0f;
        drawWindow({ 116, my, 280, mh });
        if (B.phase == BP_VICTORY) {
            drawTextCentered(B.isBoss ? "BOSS DEFEATED!" : "VICTORY!", VRES_W * 0.5f, my + 12,
                             Color{ 255, 236, 176, 255 }, 8);
            char rew[96];
            snprintf(rew, sizeof(rew), "Gained +%d XP   +%d Gil", B.xpGain, B.gilGain);
            drawTextCentered(rew, VRES_W * 0.5f, my + 32, Color{ 150, 240, 175, 255 }, 8);
            for (int i = 0; i < showRows; i++) {
                drawTextCentered(B.rewards[i].c_str(), VRES_W * 0.5f, my + 50 + i * 14,
                                 Color{ 255, 224, 120, 255 }, 8);
            }
        } else {
            drawTextCentered("ESCAPED!", VRES_W * 0.5f, my + 18, Color{ 200, 225, 255, 255 }, 8);
        }
        drawTextCentered("SPACE or CLICK to continue", VRES_W * 0.5f, my + mh - 18,
                         Color{ 210, 225, 250, 235 }, 8);
    }
    drawToast();
}
