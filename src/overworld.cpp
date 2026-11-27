// overworld.cpp — field movement, pathing, encounters, interaction, camera
#include "game.hpp"
#include "commons.hpp"
#include "world.hpp"
#include "atlas.hpp"
#include "regions.hpp"
#include "battle.hpp"
#include <cstdio>

// The keeper's errand, "The Kept Light" (the first growth from the Commons).
// Two wisps hold the hedge east of the ring, at a fixed, surveyable spot.
constexpr int KEEPER_WISPS = 2;
constexpr float KEEPER_WISP_X = 26.5f;   // the hedge at the ring's east shoulder
constexpr float KEEPER_WISP_Y = 9.5f;
constexpr float KEEPER_STAND_X = 25.5f;  // the keeper's post: inside the gate corridor
constexpr float KEEPER_STAND_Y = 8.5f;

// She starts the day at the hedge she is worried about and steps back to the
// gate once the errand is accepted, so the wisp vantage is never behind her.
static void keeperTakePost(const Game& g, float& x, float& y);

static void keeperUpdatePost() {
    for (auto& n : g.npcs) {
        if (n.role != NPC_KEEPER) continue;
        float kx, ky;
        keeperTakePost(g, kx, ky);
        n.x = n.homeX = kx;
        n.y = n.homeY = ky;
        n.moving = false;
    }
}

static void keeperTakePost(const Game& g, float& x, float& y) {
    // Offered (or unseen) = she waits by the hedge, showing you where the
    // trouble is. Accepted or finished = she keeps the gate.
    int st = g.quests[QUEST_COMMONS].state;
    bool busy = (st == QS_ACTIVE || st == QS_DONE);
    x = KEEPER_STAND_X;
    y = busy ? KEEPER_STAND_Y : KEEPER_WISP_Y + 0.5f;
}

std::vector<Chest> gChestsInView;
std::vector<FieldMonster> gFieldMonsters;

static const float WALK_SPEED = 4.4f;     // tiles / second
static const float RUN_SPEED  = 7.2f;

Vector2 getVirtualMouse() {
    Vector2 m = GetMousePosition();
    Rectangle dst = getPresentationRect();
    if (dst.width <= 0.0f || dst.height <= 0.0f) return { -1.0f, -1.0f };
    return { (m.x - dst.x) * (float)g.vw / dst.width,
             (m.y - dst.y) * (float)g.vh / dst.height };
}

static bool mouseInVirtual() {
    Vector2 m = getVirtualMouse();
    return m.x >= 0 && m.x < g.vw && m.y >= 0 && m.y < g.vh;
}

// ================================================================ camera
static void updateCamera(float dt) {
    float targetX = g.px * TILE - g.vw * 0.5f;
    float targetY = g.py * TILE - g.vh * 0.5f;
    float k = 1.0f - powf(0.0004f, dt);     // frame-rate independent smoothing
    g.camX += (targetX - g.camX) * k;
    g.camY += (targetY - g.camY) * k;
    if (!std::isfinite(g.camX)) g.camX = targetX;
    if (!std::isfinite(g.camY)) g.camY = targetY;
}

void overworldSnapCamera() {
    g.camX = g.px * TILE - g.vw * 0.5f;
    g.camY = g.py * TILE - g.vh * 0.5f;
}

// ================================================================ encounters
static bool maybeTriggerRandomEncounter(float distMoved) {
    if (g.encounterGrace > 0.0f) return false;
    if (inVillage(g.px, g.py) || nearStartVillage(g.px, g.py) || inBossArena(g.px, g.py))
        return false;
    g.stepDist += distMoved;
    if (g.stepDist < 5.5f) return false;
    g.stepDist -= 5.5f;
    g.stepCounter++;

    int zl = zoneLevelAt(g.px, g.py);
    float chance = std::min(0.28f, 0.11f + 0.012f * (float)zl);
    float roll = hashf((int)floorf(g.px) * 17 + g.stepCounter * 31,
                       (int)floorf(g.py) * 19 + g.defeatedCount * 7,
                       g.worldSeed ^ 0x3ec0u);
    if (roll < chance) {
        int tx = (int)floorf(g.px), ty = (int)floorf(g.py);
        EnemyKind primary = rollBiomeEnemy(tx, ty, zl);
        auto group = buildEncounterGroup(primary, tx, ty, zl);
        battleStart(group, zl);
        g.encounterGrace = 3.5f;
        return true;
    }
    return false;
}

// ================================================================ movement
void pulseBlocked() {
    g.blockedFx = std::min(1.0f, g.blockedFx + 0.35f);
}

static void clearWalkTarget() {
    g.walkQueue.clear();
    g.hasMoveTarget = false;
    g.walkDoorBuilding = -1;
    g.walkTargetNpc = -1;
    g.walkTargetChest = 0;
    g.walkTargetInteract = false;
}

static void beginMove(int dirX, int dirY, float dt) {
    float speed = (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) ? RUN_SPEED : WALK_SPEED;
    if (g.travelMap) speed *= 1.35f;
    float nx = g.px + dirX * speed * dt;
    float ny = g.py + dirY * speed * dt;
    const float r = 0.28f;
    bool moved = false;
    float prevX = g.px, prevY = g.py;
    if (dirX != 0) {
        if (!positionBlocked(nx, g.py, r)) { g.px = nx; moved = true; }
    } else if (dirY != 0) {
        if (!positionBlocked(g.px, ny, r)) { g.py = ny; moved = true; }
    }
    if (moved) {
        g.walkAnimT += dt;
        g.moving = true;
        float d = fabsf(g.px - prevX) + fabsf(g.py - prevY);
        if (g.encounterGrace > 0) g.encounterGrace = std::max(0.0f, g.encounterGrace - dt);
        else maybeTriggerRandomEncounter(d);
    } else {
        g.moving = false;
        pulseBlocked();
        int ring = 0;
        if (g.toastT <= 0.0f && regionLockedPassNear(g.px, g.py, ring))
            pushToast("The pass is sealed. Press SPACE to inspect it.");
    }
}

static void talkToNpc(int idx);
static void interactWithNearest();

static void stepPath(float dt) {
    if (g.walkQueue.empty()) { g.hasMoveTarget = false; g.moving = false; return; }
    auto [tx, ty] = g.walkQueue.front();
    float cx = tx + 0.5f, cy = ty + 0.5f;
    float dx = cx - g.px, dy = cy - g.py;
    float dist = sqrtf(dx * dx + dy * dy);
    float speed = (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) ? RUN_SPEED : WALK_SPEED;
    if (g.travelMap) speed *= 1.35f;
    float stepLen = speed * dt;

    if (fabsf(dx) > fabsf(dy) && dist > 0.01f) {
        g.facing = 2; g.sideFlip = dx < 0;
    } else if (dist > 0.01f) {
        g.facing = dy > 0 ? 0 : 1;
    }

    if (dist <= stepLen || dist < 0.001f) {
        float moved = dist;
        g.px = cx; g.py = cy;
        g.walkQueue.erase(g.walkQueue.begin());
        g.walkAnimT += dt;
        g.moving = true;
        if (g.encounterGrace > 0) g.encounterGrace = std::max(0.0f, g.encounterGrace - dt);
        else if (maybeTriggerRandomEncounter(moved)) return;

        if (g.walkQueue.empty()) {
            int doorIdx = g.walkDoorBuilding;
            int npcIdx = g.walkTargetNpc;
            bool doInteract = g.walkTargetInteract;
            clearWalkTarget();
            g.moving = false;
            if (doorIdx >= 0) {
                enterBuilding(doorIdx);
            } else if (npcIdx >= 0 && npcIdx < (int)g.npcs.size()) {
                talkToNpc(npcIdx);
            } else if (doInteract) {
                interactWithNearest();
            }
        }
        return;
    }

    float nx = g.px + dx / dist * stepLen;
    float ny = g.py + dy / dist * stepLen;
    if (!positionBlocked(nx, ny, 0.26f)) {
        g.px = nx; g.py = ny;
        g.moving = true;
        g.walkAnimT += dt;
        if (g.encounterGrace > 0) g.encounterGrace = std::max(0.0f, g.encounterGrace - dt);
        else maybeTriggerRandomEncounter(stepLen);
    } else {
        clearWalkTarget();
        g.moving = false;
        pulseBlocked();
    }
}

// ================================================================ monsters on the field
static void updateFieldMonsters(float dt) {
    (void)dt;
    fieldMonstersInView(g.px, g.py, gFieldMonsters);
    for (auto& m : gFieldMonsters) {
        float ph = m.phase + g.totalTime;
        float cx = m.anchorX + sinf(ph * 0.7f) * 0.65f;
        float cy = m.anchorY + cosf(ph * 0.9f) * 0.45f;
        if (!positionBlocked(cx, cy, 0.22f)) { m.x = cx; m.y = cy; }
        else { m.x = m.anchorX; m.y = m.anchorY; }

        if (g.encounterGrace > 0.0f) continue;
        float dx = g.px - m.x, dy = g.py - m.y;
        if (dx * dx + dy * dy < 0.78f * 0.78f) {
            int zl = zoneLevelAt(g.px, g.py);
            auto group = buildEncounterGroup(m.kind, (int)floorf(m.x), (int)floorf(m.y), zl);
            battleStart(group, zl, m.id, false);
            g.encounterGrace = 3.5f;
            clearWalkTarget();
            return;
        }
    }

    // The keeper's errand (the Agents' Commons): two wisps haunt the hedge
    // at the ring's east shoulder. They are ordinary field battles with their own,
    // deterministic vantage point, so the errand is completable the moment a
    // new party accepts it — no luck-of-the-spawn-grid on the story's first step.
    if (g.quests[QUEST_COMMONS].state == QS_ACTIVE && g.encounterGrace <= 0.0f &&
        g.quests[QUEST_COMMONS].progress < KEEPER_WISPS) {
        // The hedge itself is the vantage: the keeper steps to the gate the
        // moment the errand is accepted (see keeperTakePost), so this point is
        // reached only by walking round her into the ring's east shoulder.
        float dx = KEEPER_WISP_X - g.px, dy = KEEPER_WISP_Y - g.py;
        if (dx * dx + dy * dy < 0.9f * 0.9f) {
            clearWalkTarget();
            int zl = std::max(1, zoneLevelAt(g.px, g.py));
            battleStart({ EN_WISP }, zl, 0, false);
            g.quests[QUEST_COMMONS].progress = std::min(KEEPER_WISPS, g.quests[QUEST_COMMONS].progress + 1);
            g.encounterGrace = 3.5f;
            return;
        }
    }

    // Slime King boss outside the Northern Castle gate
    if (!g.bossDefeated && g.encounterGrace <= 0.0f) {
        float dx = g.px - BOSS_KING_X, dy = g.py - BOSS_KING_Y;
        if (dx * dx + dy * dy < 1.15f * 1.15f) {
            clearWalkTarget();
            std::vector<DialogLine> lines;
            lines.push_back({ FACE_SLIME, "Slime King",
                              "Field agents? I'll file your reports under 'lost in a puddle!'" });
            lines.push_back({ FACE_KNIGHT, "Aldric",
                              "The crown belongs to Elmswick. We came to bring it home." });
            lines.push_back({ FACE_SLIME, "Slime King",
                              "Then come and take it! Guards — squish the paperwork!" });
            startDialog(lines, DLG_BOSS_SLIME_KING);
            g.encounterGrace = 3.5f;
        }
    }
}

// ================================================================ interaction
static void setVillageServiceNames() {
    g.shopForge = false;                 // smiths re-arm this when talked to
    if (g.villageLoaded && !g.village.startVillage) {
        g.shopTitle = g.village.name + " Sundries";
        g.innTitle  = g.village.name + " Inn";
    } else {
        g.shopTitle = "Mira's Sundries";
        g.innTitle  = "The Sleeping Stag";
    }
}

static void talkToNpc(int npcIdx) {
    if (npcIdx < 0 || npcIdx >= (int)g.npcs.size()) return;
    Npc& n = g.npcs[npcIdx];
    // Turn NPC to face the player
    float dx = g.px - n.x, dy = g.py - n.y;
    if (fabsf(dx) > fabsf(dy)) { n.facing = 2; n.sideFlip = dx < 0; }
    else { n.facing = dy > 0 ? 0 : 1; }
    n.moving = false;
    n.thinkT = 2.5f;

    std::vector<DialogLine> lines;
    DialogAction action = DLG_NONE;
    setVillageServiceNames();

    if (n.sprite == NS_SMITH) {
        g.shopForge = true;
        g.shopTitle = (g.villageLoaded && !g.village.startVillage)
                          ? g.village.name + " Forge" : std::string("Aldwin's Forge");
        lines.push_back({ n.face, n.name, "\"Ore, ash and honest work. Weapons and armour, if you've the coin.\"" });
        action = DLG_SHOP;
    } else if (n.role == NPC_SHOPKEEP) {
        lines.push_back({ n.face, n.name, "\"Steel, salves and sundries. Buying or selling?\"" });
        action = DLG_SHOP;
    } else if (n.role == NPC_INNKEEP) {
        char buf[128];
        snprintf(buf, sizeof(buf), "\"A warm bed's %d gil, blankets included. Fights cost extra.\"", g.innCost);
        lines.push_back({ n.face, n.name, buf });
        action = DLG_INN;
    } else if (n.role == NPC_PRIEST) {
        lines.push_back({ n.face, n.name, "\"The Light asks nothing but a moment of stillness.\"" });
        action = DLG_TEMPLE;
    } else if (n.quest == QUEST_SLIMES) {
        if (g.quests[QUEST_SLIMES].state == QS_OFFERED || g.quests[QUEST_SLIMES].state == QS_UNKNOWN) {
            lines.push_back({ n.face, n.name, "\"You there! Slimes in my turnip rows again. Six of them, at least!\"" });
            lines.push_back({ n.face, n.name, "\"Thin them out in the south meadow and I'll pay you 60 gil and two Potions.\"" });
            action = DLG_START_QUEST_SLIMES;
        } else if (g.quests[QUEST_SLIMES].state == QS_ACTIVE) {
            if (g.slimesSlain >= 6) {
                lines.push_back({ n.face, n.name, "\"Six slimes! My turnips can finally grow in peace.\"" });
                lines.push_back({ n.face, n.name, "\"Here — 60 gil and a couple of Potions. Fine work, friends.\"" });
                action = DLG_FINISH_QUEST_SLIMES;
            } else {
                char buf[128];
                snprintf(buf, sizeof(buf), "\"Still %d slime%s left in the south meadow. You'll know them by the squelching.\"",
                         6 - g.slimesSlain, (6 - g.slimesSlain == 1) ? "" : "s");
                lines.push_back({ n.face, n.name, buf });
            }
        } else {
            lines.push_back({ n.face, n.name, "\"The turnips are safe, thanks to you. Sit a while, hero.\"" });
        }
    } else if (n.quest == QUEST_LOCKET) {
        if (g.quests[QUEST_LOCKET].state == QS_OFFERED || g.quests[QUEST_LOCKET].state == QS_UNKNOWN) {
            lines.push_back({ n.face, n.name, "\"Excuse me — you're heading out into the wilds, aren't you?\"" });
            lines.push_back({ n.face, n.name, "\"I dropped my mother's silver locket by the Standing Stones southeast of town. Bring it back and I'll give you my Mana Amulet!\"" });
            action = DLG_START_QUEST_LOCKET;
        } else if (g.quests[QUEST_LOCKET].state == QS_ACTIVE) {
            if (g.locketFound || itemCount(IT_LOCKET) > 0) {
                lines.push_back({ n.face, n.name, "\"My locket! You really found it out there by the stones!\"" });
                lines.push_back({ n.face, n.name, "\"Thank you so much. Please take this Mana Amulet and 120 gil — it'll serve you better than me.\"" });
                action = DLG_FINISH_QUEST_LOCKET;
            } else {
                std::string hint = compassHint(g.px, g.py, LOCKET_PICKUP_X, LOCKET_PICKUP_Y);
                lines.push_back({ n.face, n.name, "\"Look for the ring of standing stones (" + hint + "). The silver should catch the light.\"" });
            }
        } else {
            lines.push_back({ n.face, n.name, "\"I've clasped the locket tight this time. Safe roads to the three of you!\"" });
        }
    } else if (n.quest == QUEST_CROWN) {
        if (g.quests[QUEST_CROWN].state == QS_DONE) {
            lines.push_back({ n.face, n.name, "\"The Crown of Elm rests where it belongs. Walk proud.\"" });
            if (!g.frontierGuardian)
                lines.push_back({ n.face, n.name, "\"Past the castle, the road winds through the far pass to a cavern. Its guardian waits for a brave party.\"" });
            else
                lines.push_back({ n.face, n.name, "\"Word reached us of the guardian beyond the far pass. You have walked farther than our oldest maps.\"" });
        } else if (itemCount(IT_CROWN) > 0 || g.bossDefeated) {
            lines.push_back({ n.face, n.name, "\"By the Light... is that the Crown of Elm?! You bested the Slime King!\"" });
            lines.push_back({ n.face, n.name, "\"Elmswick owes you a debt it could never repay. Take this Dragon Scale armour and 500 gil, heroes!\"" });
            lines.push_back({ n.face, n.name, "\"Past the castle, the road crosses the far pass and leads to another guardian's cavern.\"" });
            action = DLG_CROWN_REWARD;
        } else if (g.quests[QUEST_CROWN].state == QS_ACTIVE) {
            std::string hint = compassHint(g.px, g.py, (float)BOSS_CASTLE_TX, (float)BOSS_CASTLE_TY);
            if (!g.hasWaystone)
                lines.push_back({ n.face, n.name, "\"The mountain pass is sealed. First find the Waystone in Hollow Hill, west of Elmswick.\"" });
            lines.push_back({ n.face, n.name, "\"Follow the North Road (" + hint + ") to the old Castle of Elm. Gear up well before facing the Slime King.\"" });
        } else {
            lines.push_back({ n.face, n.name, "\"Aldric... it is good to see you home. Yet dark jelly creeps along the North Road.\"" });
            lines.push_back({ n.face, n.name, "\"The Slime King seized the old Castle of Elm — and with it, our village's ancient Crown.\"" });
            lines.push_back({ n.face, n.name, "\"Hollow Hill lies west of town. Its guardian holds the Waystone that opens the North Road's mountain pass.\"" });
            lines.push_back({ n.face, n.name, "\"Then follow the North Road and bring back the Crown of Elm. The Crown opens the far passes too.\"" });
            action = DLG_START_QUEST_CROWN;
        }
    } else if (n.quest == QUEST_DEEP) {
        auto& q = g.quests[QUEST_DEEP];
        if (q.state == QS_DONE) {
            lines.push_back({ n.face, n.name, "\"The forge still sings with that star metal. Elmswick won't forget what you brought back.\"" });
        } else if (q.state == QS_ACTIVE && itemCount(IT_STAR_METAL) > 0) {
            lines.push_back({ n.face, n.name, "\"That glow in your pack... is that star metal? Don't just stand there — let me see it!\"" });
            action = DLG_FINISH_QUEST_DEEP;
        } else if (q.state == QS_ACTIVE) {
            int stx = 0, sty = 0;
            std::string hint = startCavernSite(stx, sty)
                ? compassHint(g.px, g.py, (float)stx, (float)sty) : std::string("somewhere west");
            lines.push_back({ n.face, n.name, "\"The old vaults still hold star metal. The stone mouth west of town (" + hint + ") is the way down.\"" });
            lines.push_back({ n.face, n.name, "\"Bring me one shard and Aldwin will owe you a smith's favour. And mind the dark.\"" });
        } else {
            lines.push_back({ n.face, n.name, "\"Aldwin needs star metal for a blade that won't break. The hills west of town open into old vaults.\"" });
            lines.push_back({ n.face, n.name, "\"Find a shard in the deepest chamber and I'll make it worth your while.\"" });
            action = DLG_START_QUEST_DEEP;
        }
    } else if (n.quest == QUEST_TIDE) {
        Quest& q = g.quests[QUEST_TIDE];
        char buf[192];
        if (!g.hasBoat) {
            lines.push_back({ n.face, n.name,
                              "\"You'll be the three from Elmswick. I'm Bel. I keep the boat.\"" });
            lines.push_back({ n.face, n.name,
                              "\"Out past this bank the millpond runs miles west, and there's three\n"
                              "low islands out on it with old wells nobody's tended in a age.\"" });
            lines.push_back({ n.face, n.name,
                              "\"Forty gil and the boat's yours for as long as you've a mind.\n"
                              "Stand on any bank and press Space and she'll come to you.\"" });
            action = DLG_BUY_BOAT;
        } else if (q.state == QS_DONE) {
            lines.push_back({ n.face, n.name,
                              "\"Three wells running again. You can hear the water from the dock on a still night.\"" });
            lines.push_back({ n.face, n.name,
                              "\"Keep the boat as long as you want it. There's water I've never sounded out there.\"" });
        } else {
            snprintf(buf, sizeof(buf),
                     "\"Tidewells found: %d of %d. %s\"", g.tideFound, TIDE_ISLANDS,
                     g.tideFound == 0 ? "Low on the water, west and south of the town bank." :
                     (g.tideFound < TIDE_ISLANDS ? "One still asleep somewhere out there." :
                      "That's all of them."));
            lines.push_back({ n.face, n.name, buf });
            lines.push_back({ n.face, n.name,
                              "\"Follow the wake back the way you came and nothing out there bites you\n"
                              "that hasn't bitten me first.\"" });
        }
    } else if (n.role == NPC_KEEPER) {
        Quest& q = g.quests[QUEST_COMMONS];
        int built = commonsBuiltStones();
        char buf[192];
        if (q.state == QS_DONE) {
            snprintf(buf, sizeof(buf),
                     "\"Look at it burn. %d stone%s raised, and the ring still has room.\"", built,
                     built == 1 ? "" : "s");
            lines.push_back({ n.face, n.name, buf });
            lines.push_back({ n.face, n.name,
                              "\"Walk the ring and read it as often as you like. The next plot is already marked.\"", });
            lines.push_back({ n.face, n.name,
                              "\"Whoever comes after us will find it warm. That is the whole of my office.\"", });
        } else if (q.state == QS_ACTIVE) {
            int slain = std::min(KEEPER_WISPS, g.quests[QUEST_COMMONS].progress);
            if (slain >= KEEPER_WISPS) {
                lines.push_back({ n.face, n.name,
                                  "\"The hedge is quiet. Whatever was in it has gone back to the dark.\"", });
                lines.push_back({ n.face, n.name,
                                  "\"Now the brazier. Say the words with me: one flame, kept for the ring.\"", });
                action = DLG_FINISH_QUEST_COMMONS;
            } else {
                snprintf(buf, sizeof(buf),
                         "\"Still %d wisp%s at the hedge on the ring's east shoulder, just past me. Bring them down.\"",
                         KEEPER_WISPS - slain, (KEEPER_WISPS - slain == 1) ? "" : "s");
                lines.push_back({ n.face, n.name, buf });
                lines.push_back({ n.face, n.name,
                                  "\"Sword, staff, prayer — whatever answers. They lurk just south-east of the gate.\"", });
            }
        } else {
            lines.push_back({ n.face, n.name,
                              "\"Halt a moment. You are standing at the edge of the Agents' Commons.\"", });
            lines.push_back({ n.face, n.name,
                              "\"Our village keeps maps. This place keeps hands. Every stone in the ring is work somebody finished.\"", });
            snprintf(buf, sizeof(buf),
                     "\"%d stones stand. %d plots are empty, and I will not fill one myself.\"", built,
                     commonsPlotCount());
            lines.push_back({ n.face, n.name, buf });
            lines.push_back({ n.face, n.name,
                              "\"But a place also needs keeping. Wisps have taken the hedge past my gate and leave the ring dark.\"", });
            lines.push_back({ n.face, n.name,
                              "\"Still it, and I will teach you how to light the brazier. Then read the stones.\"", });
            action = DLG_START_QUEST_COMMONS;
        }
    } else if (n.role == NPC_GUARD && n.name.find("Odo") != std::string::npos) {
        if (itemCount(IT_RUSTY_KEY) == 0 && !g.towerChestOpened) {
            lines.push_back({ n.face, n.name, "\"Heading out past the gate? Take this old Rusty Key I pulled from the ditch.\"" });
            lines.push_back({ n.face, n.name, "\"Might fit the iron chest at the ruined Watchtower out west. Watch for wolves.\"" });
            action = DLG_GIVE_KEY;
        } else if (!g.towerChestOpened) {
            std::string hint = compassHint(g.px, g.py, (float)TOWER_SITE_TX, (float)TOWER_SITE_TY);
            lines.push_back({ n.face, n.name, "\"That Rusty Key should open the chest at the Watchtower (" + hint + ").\"" });
        } else {
            lines.push_back({ n.face, n.name, "\"Found good steel in the old tower, eh? Keep your blade sharp out there.\"" });
        }
    } else {
        lines.push_back({ n.face, n.name, npcLine(n, n.lineSet + (int)(g.totalTime * 0.05f)) });
        if (n.role != NPC_ANIMAL && ((int)(g.runTime * 0.5f) % 3 == 0))
            lines.push_back({ n.face, n.name, npcLine(n, n.lineSet + 3) });
    }
    startDialog(lines, action);
    g.interactT = 0.25f;
}

static void interactWithNearest() {
    // 1. Slime King boss
    if (!g.bossDefeated) {
        float dx = BOSS_KING_X - g.px, dy = BOSS_KING_Y - g.py;
        if (dx * dx + dy * dy < 2.2f * 2.2f) {
            std::vector<DialogLine> lines;
            lines.push_back({ FACE_SLIME, "Slime King",
                              "BWAHAHA! So Elmswick sent three tin cans to fetch its shiny hat?" });
            lines.push_back({ FACE_KNIGHT, "Aldric",
                              "That crown belongs to the village. Give it back!" });
            lines.push_back({ FACE_SLIME, "Slime King",
                              "Come and take it! Royal Guards, squish them!" });
            startDialog(lines, DLG_BOSS_SLIME_KING);
            g.interactT = 0.25f;
            return;
        }
    }

    // Each range has a visible seal and tells the player how to open it.
    {
        int ring = 0;
        if (regionLockedPassNear(g.px, g.py, ring)) {
            std::vector<DialogLine> lines;
            if (ring == 1) {
                lines.push_back({ -1, "", "A stone seal blocks the pass to the Crownlands." });
                lines.push_back({ FACE_MAGE, "Iris", "The Waystone can wake it. The guardian of Hollow Hill holds that key." });
                lines.push_back({ -1, "", "Take the west road from Elmswick to Hollow Hill. Wake its two vault runes and defeat the guardian." });
            } else {
                lines.push_back({ -1, "", "The mountains close the road into the Far Marches." });
                lines.push_back({ FACE_CLERIC, "Pip", "The Crown of Elm is the key. The Slime King has it at the castle on the North Road." });
            }
            startDialog(lines);
            g.interactT = 0.25f;
            return;
        }
    }

    // 2. Lost Locket pickup at the Standing Stones
    if (!g.locketFound) {
        float dx = LOCKET_PICKUP_X - g.px, dy = LOCKET_PICKUP_Y - g.py;
        if (dx * dx + dy * dy < 1.6f * 1.6f) {
            g.locketFound = true;
            addItem(IT_LOCKET, 1, true);
            if (g.quests[QUEST_LOCKET].state == QS_ACTIVE)
                g.quests[QUEST_LOCKET].progress = 1;
            playSoundIf(g.assets.treasure);
            std::vector<DialogLine> lines;
            lines.push_back({ FACE_CLERIC, "Pip", "Look! Something glinting in the grass beneath the statue!" });
            lines.push_back({ -1, "", "Found the Lost Locket! Return it to Tamsin in Elmswick." });
            startDialog(lines);
            g.interactT = 0.3f;
            return;
        }
    }

    // 3. Locked Watchtower chest
    {
        float dx = (TOWER_CHEST_TX + 0.5f) - g.px, dy = (TOWER_CHEST_TY + 0.5f) - g.py;
        if (dx * dx + dy * dy < 1.5f * 1.5f) {
            if (g.towerChestOpened) {
                pushToast("The tower chest is empty.");
                return;
            }
            if (itemCount(IT_RUSTY_KEY) > 0) {
                takeItem(IT_RUSTY_KEY, 1);
                g.towerChestOpened = true;
                addItem(IT_SWORD_MYTHRIL, 1, true);
                addItem(IT_ELIXIR, 1, true);
                playSoundIf(g.assets.treasure);
                std::vector<DialogLine> lines;
                lines.push_back({ -1, "", "The Rusty Key turns with a loud clack!" });
                lines.push_back({ -1, "", "Found Mythril Sword and an Elixir inside the watchtower chest!" });
                startDialog(lines);
                g.interactT = 0.3f;
            } else {
                std::vector<DialogLine> lines;
                lines.push_back({ -1, "", "A heavy iron chest sits beneath the watchtower. It's locked tight." });
                lines.push_back({ FACE_MAGE, "Iris", "Maybe someone back in Elmswick has a key for this." });
                startDialog(lines);
            }
            return;
        }
    }

    // 4. Cavern mouths — descend into a procedurally cut dungeon
    {
        Landmark lm;
        int mtx = (int)floorf(g.px), mty = (int)floorf(g.py);
        if (landmarkNear(mtx, mty, lm) && lm.kind == LK_CAVERN) {
            float dx = (lm.tx + 0.5f) - g.px, dy = (lm.ty + 1.4f) - g.py;
            if (dx * dx + dy * dy < 2.4f * 2.4f) {
                int stx = 0, sty = 0;
                bool start = startCavernSite(stx, sty) && stx == lm.tx && sty == lm.ty;
                enterDungeon(lm.tx, lm.ty, start);
                g.interactT = 0.3f;
                return;
            }
        }
    }

    // 4b. Push off: if the ferryman has sold you the boat and open water is a
    // step away, SPACE casts off. This is deliberately ahead of every bank
    // interaction, because standing on a jetty the only thing you want is water.
    if (g.hasBoat && sailCanBoard()) { sailEnter(); return; }

    // 4c. A Tidemill Tidewell: the isles' one act.
    if (tideTryInteract()) { g.interactT = 0.25f; return; }

    // 5. The Agents' Commons: its stones, its plots, its brazier, its board.
    if (g.villageLoaded && inCommons(g.px, g.py) && commonsTryInteract()) {
        g.interactT = 0.25f;
        return;
    }

    // 6. Nearby NPC
    int npc = npcNear(g.px, g.py, 1.55f);
    if (npc >= 0) {
        talkToNpc(npc);
        return;
    }

    // 5. Procedural chests
    for (size_t i = 0; i < gChestsInView.size(); i++) {
        Chest& c = gChestsInView[i];
        float dx = (c.tx + 0.5f) - g.px, dy = (c.ty + 0.5f) - g.py;
        if (dx * dx + dy * dy > 1.5f * 1.5f) continue;
        if (g.openedChests.count(c.id)) {
            pushToast("The chest is empty.");
            return;
        }
        g.openedChests.insert(c.id);
        if (c.gil > 0) {
            g.gil += c.gil;
            char buf[64];
            snprintf(buf, sizeof(buf), "Found %d gil!", c.gil);
            pushToast(buf);
        } else {
            addItem(c.item, c.amount);
        }
        playSoundIf(g.assets.treasure);
        g.interactT = 0.3f;
        return;
    }

    // 6. Building doors
    if (g.villageLoaded) {
        for (size_t i = 0; i < g.village.buildings.size(); i++) {
            const Building& b = g.village.buildings[i];
            float dx = (float)buildingDoorTx(b) + 0.5f - g.px;
            float dy = (float)buildingDoorTy(b) + 0.4f - g.py;
            if (dx * dx + dy * dy < 1.75f * 1.75f) {
                enterBuilding((int)i);
                return;
            }
        }
    }

    // 7. Village signs, noticeboards, mailboxes & wells
    if (g.villageLoaded && inVillage(g.px, g.py)) {
        for (const auto& d : g.village.decos) {
            float dx = (d.x + 0.5f) - g.px, dy = (d.y + 0.5f) - g.py;
            if ((d.kind == VD_SIGN || d.kind == VD_NOTICEBOARD) && dx * dx + dy * dy < 1.85f * 1.85f) {
                std::vector<DialogLine> lines;
                if (d.kind == VD_NOTICEBOARD) {
                    lines.push_back({ -1, "", "ELMSWICK FIELD BOARD — Posted notices:" });
                    lines.push_back({ -1, "", "'Wanted: six fewer meadow slimes (see Farmer Hedrick). Lost: one silver locket near the Standing Stones (see Tamsin).'" });
                    lines.push_back({ -1, "", "'HOLLOW HILL: west road, then north spur. Wake the two runes in the deepest vault and defeat its guardian to find the Waystone.'" });
                    lines.push_back({ -1, "", "'The Waystone opens the mountain passes. The Crown of Elm opens the far passes beyond the castle.'" });
                } else if (g.village.startVillage) {
                    lines.push_back({ -1, "", "ELMSWICK — 'Where the meadow begins.'" });
                    lines.push_back({ -1, "", "North past the chapel: Briarwood and the castle. West past the shop: the marsh and Old Watchtower." });
                    lines.push_back({ -1, "", "South across Millbrook: east for the Standing Stones, southwest for the Sunscorch Flats." });
                } else {
                    lines.push_back({ -1, "", g.village.name + " — A quiet hearth in the wilds." });
                    lines.push_back({ -1, "", "Rest at the inn, stock up at the shop, and keep the Light close." });
                }
                startDialog(lines);
                g.interactT = 0.2f;
                return;
            }
            if (d.kind == VD_MAILBOX && dx * dx + dy * dy < 1.6f * 1.6f) {
                pushToast("Tied with twine: field dispatches and seed catalogues.");
                g.interactT = 0.15f;
                return;
            }
            if (d.kind == VD_WELL && dx * dx + dy * dy < 2.2f * 2.2f) {
                bool healed = false;
                for (auto& h : g.party) {
                    if (!h.ko && h.hp < h.maxHp) {
                        h.hp = std::min(h.maxHp, h.hp + 5);
                        healed = true;
                    }
                }
                std::vector<DialogLine> lines;
                lines.push_back({ -1, "", "Cold spring water glimmers far below. Your whisper echoes back: '...Aldric...'" });
                if (healed) {
                    lines.push_back({ -1, "", "A cool sip refreshes the party (+5 HP)." });
                    playSoundIf(g.assets.heal);
                }
                startDialog(lines);
                g.interactT = 0.2f;
                return;
            }
        }
    }

    pushToast("Nothing here.");
}

void overworldInteract() { interactWithNearest(); }

// ================================================================ clicking
static void clickToMove() {
    if (g.hotButtons > 0) return;
    Vector2 m = getVirtualMouse();
    if (!mouseInVirtual()) return;
    if (!IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) return;
    float wx = g.travelMap ? g.px + (m.x - g.vw * 0.5f) / TRAVEL_PIXELS_PER_TILE
                           : (m.x + g.camX) / TILE;
    float wy = g.travelMap ? g.py + (m.y - g.vh * 0.5f) / TRAVEL_PIXELS_PER_TILE
                           : (m.y + g.camY) / TILE;

    if (g.travelMap) {
        // Icons and ground share world coordinates, not the close-up camera.
        // Cave clicks walk to the mouth; the view will change as we approach.
        Landmark cave;
        int tx = (int)floorf(wx), ty = (int)floorf(wy);
        bool enterCave = landmarkNear(tx, ty, cave) && cave.kind == LK_CAVERN &&
                         fabsf(wx - cave.tx) < 4.0f && fabsf(wy - cave.ty) < 4.0f;
        if (enterCave) { tx = cave.tx; ty = cave.ty + 1; }
        WalkPath path;
        if (findWalkPath(g.px, g.py, tx, ty, path) && !path.cells.empty()) {
            clearWalkTarget();
            g.walkQueue = path.cells;
            g.hasMoveTarget = true;
            // A blocked or too-distant destination may yield a partial path.
            // Only interact if its endpoint actually reaches the chosen mouth.
            const auto& end = path.cells.back();
            float ex = end.first + 0.5f - (cave.tx + 0.5f);
            float ey = end.second + 0.5f - (cave.ty + 1.4f);
            g.walkTargetInteract = enterCave && ex * ex + ey * ey < 2.4f * 2.4f;
            g.clickFlashT = 0.4f;
            g.clickFlashX = tx + 0.5f; g.clickFlashY = ty + 0.5f;
        } else pulseBlocked();
        return;
    }

    // 1. Clicking a building enters it
    if (g.villageLoaded) {
        for (size_t i = 0; i < g.village.buildings.size(); i++) {
            const Building& b = g.village.buildings[i];
            float x0 = (float)buildingX0(b) * TILE - g.camX;
            float y0 = (float)buildingY0(b) * TILE - g.camY;
            float bw = (float)b.w * TILE, bh = (float)(b.h + 1) * TILE;
            if (CheckCollisionPointRec(m, { x0, y0, bw, bh })) {
                int doorTx = buildingDoorTx(b), doorTy = buildingDoorTy(b);
                float ddx = (doorTx + 0.5f) - g.px, ddy = (doorTy + 0.5f) - g.py;
                if (ddx * ddx + ddy * ddy < 1.3f * 1.3f) {
                    enterBuilding((int)i);
                    return;
                }
                WalkPath path;
                if (findWalkPath(g.px, g.py, doorTx, doorTy, path) && !path.cells.empty()) {
                    clearWalkTarget();
                    g.walkQueue = path.cells;
                    g.hasMoveTarget = true;
                    g.walkDoorBuilding = (int)i;
                    g.clickFlashT = 0.35f;
                    g.clickFlashX = (float)doorTx + 0.5f;
                    g.clickFlashY = (float)doorTy + 0.5f;
                }
                return;
            }
        }
    }

    // 2. Clicking an NPC walks up and talks automatically
    for (size_t i = 0; i < g.npcs.size(); i++) {
        const Npc& n = g.npcs[i];
        float cx = n.x * TILE - g.camX;
        float fy = (n.y + 0.35f) * TILE - g.camY;
        if (CheckCollisionPointRec(m, { cx - 12, fy - 32, 24, 34 })) {
            float ddx = n.x - g.px, ddy = n.y - g.py;
            if (ddx * ddx + ddy * ddy < 1.55f * 1.55f) {
                talkToNpc((int)i);
                return;
            }
            int tx = (int)floorf(n.x), ty = (int)floorf(n.y) + 1;
            nearestWalkable(tx, ty, tx, ty);
            WalkPath path;
            if (findWalkPath(g.px, g.py, tx, ty, path) && !path.cells.empty()) {
                clearWalkTarget();
                g.walkQueue = path.cells;
                g.hasMoveTarget = true;
                g.walkTargetNpc = (int)i;
                g.clickFlashT = 0.35f;
                g.clickFlashX = n.x;
                g.clickFlashY = n.y;
            }
            return;
        }
    }

    // 3. Clicking a chest walks up and opens it
    for (const auto& c : gChestsInView) {
        float sx = c.tx * TILE - g.camX, sy = c.ty * TILE - g.camY;
        if (CheckCollisionPointRec(m, { sx - 4, sy - 6, 24, 24 })) {
            float ddx = (c.tx + 0.5f) - g.px, ddy = (c.ty + 0.5f) - g.py;
            if (ddx * ddx + ddy * ddy < 1.5f * 1.5f) {
                interactWithNearest();
                return;
            }
            int tx = c.tx, ty = c.ty + 1;
            nearestWalkable(tx, ty, tx, ty);
            WalkPath path;
            if (findWalkPath(g.px, g.py, tx, ty, path) && !path.cells.empty()) {
                clearWalkTarget();
                g.walkQueue = path.cells;
                g.hasMoveTarget = true;
                g.walkTargetInteract = true;
                g.clickFlashT = 0.35f;
                g.clickFlashX = c.tx + 0.5f;
                g.clickFlashY = c.ty + 0.5f;
            }
            return;
        }
    }

    // Cavern silhouettes are doors too, not blocked ground-click targets.
    Landmark cave;
    if (landmarkNear((int)floorf(wx), (int)floorf(wy), cave) && cave.kind == LK_CAVERN &&
        wx >= cave.tx - 1 && wx <= cave.tx + 2 && wy >= cave.ty - 2 && wy <= cave.ty + 1.5f) {
        float dx = cave.tx + 0.5f - g.px, dy = cave.ty + 1.4f - g.py;
        if (dx * dx + dy * dy < 2.4f * 2.4f) {
            clearWalkTarget();
            interactWithNearest();
        } else {
            WalkPath path;
            if (findWalkPath(g.px, g.py, cave.tx, cave.ty + 1, path) && !path.cells.empty()) {
                clearWalkTarget();
                g.walkQueue = path.cells;
                g.hasMoveTarget = true;
                g.walkTargetInteract = true;
                g.clickFlashT = 0.35f;
                g.clickFlashX = cave.tx + 0.5f;
                g.clickFlashY = cave.ty + 1.5f;
            } else pulseBlocked();
        }
        return;
    }

    // 4. Clicking the Lost Locket or Watchtower chest
    if (!g.locketFound && fabsf(wx - LOCKET_PICKUP_X) < 1.0f && fabsf(wy - LOCKET_PICKUP_Y) < 1.0f) {
        WalkPath path;
        if (findWalkPath(g.px, g.py, (int)floorf(LOCKET_PICKUP_X), (int)floorf(LOCKET_PICKUP_Y), path) && !path.cells.empty()) {
            clearWalkTarget();
            g.walkQueue = path.cells;
            g.hasMoveTarget = true;
            g.walkTargetInteract = true;
            g.clickFlashT = 0.35f;
            g.clickFlashX = LOCKET_PICKUP_X;
            g.clickFlashY = LOCKET_PICKUP_Y;
            return;
        }
    }
    if (!g.towerChestOpened && fabsf(wx - (TOWER_CHEST_TX + 0.5f)) < 1.0f && fabsf(wy - (TOWER_CHEST_TY + 0.5f)) < 1.0f) {
        WalkPath path;
        int tx = TOWER_CHEST_TX, ty = TOWER_CHEST_TY + 1;
        nearestWalkable(tx, ty, tx, ty);
        if (findWalkPath(g.px, g.py, tx, ty, path) && !path.cells.empty()) {
            clearWalkTarget();
            g.walkQueue = path.cells;
            g.hasMoveTarget = true;
            g.walkTargetInteract = true;
            g.clickFlashT = 0.35f;
            g.clickFlashX = TOWER_CHEST_TX + 0.5f;
            g.clickFlashY = TOWER_CHEST_TY + 0.5f;
            return;
        }
    }

    // 5. Otherwise walk to clicked tile
    int tx = (int)floorf(wx), ty = (int)floorf(wy);
    WalkPath path;
    if (findWalkPath(g.px, g.py, tx, ty, path) && !path.cells.empty()) {
        clearWalkTarget();
        g.walkQueue = path.cells;
        g.hasMoveTarget = true;
        g.walkDoorBuilding = path.doorBuilding;
        g.clickFlashT = 0.4f;
        g.clickFlashX = (float)tx + 0.5f;
        g.clickFlashY = (float)ty + 0.5f;
    } else {
        pulseBlocked();
    }
}

// ================================================================ update
// Switch views at the edge of a settlement or landmark, while keeping the
// party on the same physical map. Hysteresis at village edges avoids a flicker;
// M remains a manual override until the next place boundary is crossed.
static void updateTravelContext() {
    bool wasLocal = g.travelContext == 0;
    float homeRadius = wasLocal ? 32.0f : 28.0f;
    bool local = g.px * g.px + g.py * g.py < homeRadius * homeRadius;
    if (!local && g.villageLoaded && !g.village.startVillage) {
        float dx = g.px - g.village.cx, dy = g.py - g.village.cy;
        float radius = wasLocal ? 27.0f : 23.0f;
        local = dx * dx + dy * dy < radius * radius;
    }
    if (!local) {
        Landmark lm;
        local = landmarkNear((int)floorf(g.px), (int)floorf(g.py), lm);
    }
    for (int i = 0; !local && i < tideIslandCount(); ++i) {
        float dx = g.px - tideIslandWellX(i), dy = g.py - tideIslandWellY(i);
        local = dx * dx + dy * dy < 12.0f * 12.0f;
    }
    int next = local ? 0 : 1;
    if (g.travelContext < 0) {
        g.travelContext = next;
        g.travelMap = !local;
        if (local) overworldSnapCamera();
    } else if (g.travelContext != next) {
        g.travelContext = next;
        g.travelMap = !local;
        if (local) overworldSnapCamera();
        g.fade = 0.55f; g.fadeDir = -1;
        if (!local) pushToast("Travelling map. Press M for a closer view.");
    }
}

void overworldUpdate(float dt) {
    gAnimTime += dt;
    g.totalTime += dt;
    g.runTime += dt;
    if (g.toastT > 0) g.toastT = std::max(0.0f, g.toastT - dt);
    if (g.clickFlashT > 0) g.clickFlashT = std::max(0.0f, g.clickFlashT - dt);
    if (g.interactT > 0) g.interactT = std::max(0.0f, g.interactT - dt);
    if (g.blockedFx > 0) g.blockedFx = std::max(0.0f, g.blockedFx - dt * 2.0f);
    if (g.shakeT > 0) g.shakeT = std::max(0.0f, g.shakeT - dt);

    updateClouds(dt);
    atlasUpdate(g.px, g.py, dt);   // name the country; raise the banner on a border
    villageLoadAt(g.px, g.py);
    updateTravelContext();
    npcsLoadFor(g.px, g.py);
    chestsLoadFor(g.px, g.py);
    keeperUpdatePost();         // she stands at the gate, or by the hedge she worries about
    commonsTick(dt);            // the Commons brazier follows the quest table

    // NPC idle drift (shopkeepers/innkeepers/priests stay near their post)
    for (auto& n : g.npcs) {
        n.animT += dt;
        n.thinkT -= dt;
        // The keeper holds her post exactly: she is the Commons' one voice and
        // the wisp vantage is measured from where she stands.
        bool stationary = (n.role == NPC_SHOPKEEP || n.role == NPC_INNKEEP || n.role == NPC_PRIEST ||
                           n.role == NPC_KEEPER);
        if (stationary) {
            n.moving = false;
            n.facing = 0;
            continue;
        }
        if (n.thinkT <= 0) {
            n.thinkT = 2.0f + hashf((int)(n.x * 100), (int)(n.y * 100) + (int)(g.totalTime), 7u) * 3.0f;
            float r = hashf((int)(n.y * 100), (int)(n.x * 100) + (int)(g.totalTime * 3), 11u);
            if (r < 0.45f) {
                n.dx = 0; n.dy = 0; n.moving = false;
            } else {
                int dir = (int)((r - 0.45f) / 0.55f * 4.0f) % 4;
                n.dx = (dir == 0) ? 1.0f : (dir == 1 ? -1.0f : 0.0f);
                n.dy = (dir == 2) ? 1.0f : (dir == 3 ? -1.0f : 0.0f);
                n.moving = true;
                if (n.dx != 0) { n.facing = 2; n.sideFlip = n.dx < 0; }
                else { n.facing = n.dy > 0 ? 0 : 1; }
            }
        }
        if (n.moving) {
            float nx = n.x + n.dx * 1.25f * dt;
            float ny = n.y + n.dy * 1.25f * dt;
            float dx = nx - n.homeX, dy = ny - n.homeY;
            if (dx * dx + dy * dy < 6.25f && !positionBlocked(nx, ny, 0.25f)) {
                n.x = nx; n.y = ny;
            } else {
                n.dx = 0; n.dy = 0; n.moving = false;
            }
        }
    }

    // keyboard movement
    int kx = 0, ky = 0;
    if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) kx -= 1;
    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) kx += 1;
    if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)) ky -= 1;
    if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)) ky += 1;
    if (kx != 0 || ky != 0) {
        clearWalkTarget();
        if (kx != 0 && ky != 0) kx = 0;             // cardinal only, like the SNES
        if (kx < 0) { g.facing = 2; g.sideFlip = true; }
        else if (kx > 0) { g.facing = 2; g.sideFlip = false; }
        else if (ky < 0) g.facing = 1;
        else g.facing = 0;
        beginMove(kx, ky, dt);
    } else if (!g.walkQueue.empty()) {
        stepPath(dt);
    } else {
        g.moving = false;
        if (g.encounterGrace > 0) g.encounterGrace = std::max(0.0f, g.encounterGrace - dt * 0.4f);
    }

    if (g.mode != MODE_OVERWORLD) return;
    updateFieldMonsters(dt);
    if (g.mode != MODE_OVERWORLD) return;
    updateCamera(dt);

    // input: menu / interact
    if (IsKeyPressed(KEY_M)) {
        g.travelMap = !g.travelMap;
        if (!g.travelMap) overworldSnapCamera();
        g.fade = 0.45f; g.fadeDir = -1;
        pushToast(g.travelMap ? "Travelling map." : "Close view.");
        return;
    }
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_TAB)) {
        g.menuReturn = MODE_OVERWORLD;
        g.mode = MODE_MENU;
        g.menuPage = 0; g.menuIndex = 0;
        g.journalTab = 0; g.commonsJournalSel = 0;
        playSoundIf(g.assets.confirm);
        return;
    }
    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_E) || IsKeyPressed(KEY_ENTER)) {
        interactWithNearest();
        return;
    }
    clickToMove();
}

// ================================================================ interaction prompt
const char* interactionPrompt() {
    static char buf[128];
    if (!g.bossDefeated) {
        float dx = BOSS_KING_X - g.px, dy = BOSS_KING_Y - g.py;
        if (dx * dx + dy * dy < 2.2f * 2.2f) return "SPACE: challenge Slime King";
    }
    {
        int ring = 0;
        if (regionLockedPassNear(g.px, g.py, ring)) return "SPACE: inspect the sealed pass";
    }
    if (!g.locketFound) {
        float dx = LOCKET_PICKUP_X - g.px, dy = LOCKET_PICKUP_Y - g.py;
        if (dx * dx + dy * dy < 1.6f * 1.6f) return "SPACE: pick up Lost Locket";
    }
    {
        float dx = (TOWER_CHEST_TX + 0.5f) - g.px, dy = (TOWER_CHEST_TY + 0.5f) - g.py;
        if (dx * dx + dy * dy < 1.5f * 1.5f)
            return g.towerChestOpened ? "SPACE: empty chest"
                 : (itemCount(IT_RUSTY_KEY) > 0 ? "SPACE: unlock tower chest" : "SPACE: locked chest");
    }
    if (g.hasBoat && sailCanBoard()) return "SPACE: push off in the boat";
    {
        const char* tide = nullptr;
        if (tidePrompt(&tide)) return tide;
    }
    if (g.villageLoaded && inCommons(g.px, g.py)) {
        int ptx = (int)floorf(g.px), pty = (int)floorf(g.py);
        for (int s2 = 0; s2 < commonsBuiltStones(); s2++) {
            int tx, ty;
            commonsSlotCell(s2, tx, ty);
            float dx = (tx + 0.5f) - g.px, dy = (ty + 0.5f) - g.py;
            if (dx * dx + dy * dy < 2.4f * 2.4f) return "SPACE: read the stone";
        }
        for (int pl = 0; pl < commonsPlotCount(); pl++) {
            int tx, ty;
            commonsSlotCell(commonsBuiltStones() + pl, tx, ty);
            float dx = (tx + 0.5f) - g.px, dy = (ty + 0.5f) - g.py;
            if (dx * dx + dy * dy < 2.6f * 2.6f) return "SPACE: inspect the empty plot";
        }
        (void)ptx; (void)pty;
        float bdx = (float)(COMMONS_CX - 1) + 0.5f - g.px, bdy = (float)(COMMONS_CY + 4) + 0.5f - g.py;
        if (bdx * bdx + bdy * bdy < 2.0f * 2.0f) return "SPACE: read the keeper's board";
        float cdx = (float)COMMONS_CX + 0.5f - g.px, cdy = (float)COMMONS_CY + 0.5f - g.py;
        if (cdx * cdx + cdy * cdy < 2.5f * 2.5f) return "SPACE: the cold brazier";
    }
    if (!g.npcs.empty()) {
        int i = npcNear(g.px, g.py, 1.55f);
        if (i >= 0) {
            snprintf(buf, sizeof(buf), "SPACE: talk to %s", g.npcs[i].name.c_str());
            return buf;
        }
    }
    {
        Landmark lm;
        int mtx = (int)floorf(g.px), mty = (int)floorf(g.py);
        if (landmarkNear(mtx, mty, lm) && lm.kind == LK_CAVERN) {
            float dx = (lm.tx + 0.5f) - g.px, dy = (lm.ty + 1.4f) - g.py;
            if (dx * dx + dy * dy < 2.4f * 2.4f) {
                int stx = 0, sty = 0;
                bool start = startCavernSite(stx, sty) && stx == lm.tx && sty == lm.ty;
                return start ? "SPACE: enter Hollow Hill" : "SPACE: enter the cavern";
            }
        }
    }
    for (const auto& c : gChestsInView) {
        float dx = (c.tx + 0.5f) - g.px, dy = (c.ty + 0.5f) - g.py;
        if (dx * dx + dy * dy < 1.5f * 1.5f)
            return g.openedChests.count(c.id) ? "SPACE: empty chest" : "SPACE: open chest";
    }
    if (g.villageLoaded) {
        for (const auto& b : g.village.buildings) {
            float dx = (float)buildingDoorTx(b) + 0.5f - g.px, dy = (float)buildingDoorTy(b) + 0.4f - g.py;
            if (dx * dx + dy * dy < 1.75f * 1.75f) return "SPACE: go inside";
        }
        if (inVillage(g.px, g.py)) {
            for (const auto& d : g.village.decos) {
                float dx = (d.x + 0.5f) - g.px, dy = (d.y + 0.5f) - g.py;
                if (d.kind == VD_NOTICEBOARD && dx * dx + dy * dy < 1.85f * 1.85f) return "SPACE: read notices";
                if (d.kind == VD_SIGN && dx * dx + dy * dy < 1.85f * 1.85f) return "SPACE: read sign";
                if (d.kind == VD_MAILBOX && dx * dx + dy * dy < 1.6f * 1.6f) return "SPACE: check mailbox";
                if (d.kind == VD_WELL && dx * dx + dy * dy < 2.2f * 2.2f) return "SPACE: look in well";
            }
        }
    }
    return nullptr;
}
