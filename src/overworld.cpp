// overworld.cpp — field movement, pathing, encounters, interaction, camera
#include "game.hpp"
#include "world.hpp"
#include "battle.hpp"
#include <cstdio>

std::vector<Chest> gChestsInView;
std::vector<FieldMonster> gFieldMonsters;

static const float WALK_SPEED = 4.2f;     // tiles / second
static const float RUN_SPEED = 7.0f;

Vector2 getVirtualMouse() {
    Vector2 m = GetMousePosition();
    float scale = std::min((float)GetScreenWidth() / VRES_W, (float)GetScreenHeight() / VRES_H);
    float offX = (GetScreenWidth() - VRES_W * scale) * 0.5f;
    float offY = (GetScreenHeight() - VRES_H * scale) * 0.5f;
    return { (m.x - offX) / scale, (m.y - offY) / scale };
}

static Rectangle virtualRect() {
    return { 0, 0, (float)VRES_W, (float)VRES_H };
}

static bool mouseInVirtual() {
    Vector2 m = getVirtualMouse();
    return CheckCollisionPointRec(m, virtualRect());
}

// ================================================================ camera
static void updateCamera(float dt) {
    float targetX = g.px * TILE - VRES_W * 0.5f;
    float targetY = g.py * TILE - VRES_H * 0.5f;
    float k = 1.0f - powf(0.0006f, dt);     // frame-rate independent smoothing
    g.camX += (targetX - g.camX) * k;
    g.camY += (targetY - g.camY) * k;
    if (!std::isfinite(g.camX)) g.camX = targetX;
}

void overworldSnapCamera() {
    g.camX = g.px * TILE - VRES_W * 0.5f;
    g.camY = g.py * TILE - VRES_H * 0.5f;
}

// ================================================================ movement
static void beginMove(int dirX, int dirY, float dt) {
    float speed = (IsKeyDown(KEY_LEFT_SHIFT) ? RUN_SPEED : WALK_SPEED);
    float nx = g.px + dirX * speed * dt;
    float ny = g.py + dirY * speed * dt;
    const float r = 0.30f;
    bool moved = false;
    if (dirX != 0) {
        if (!positionBlocked(nx, g.py, r)) { g.px = nx; moved = true; }
    } else if (dirY != 0) {
        if (!positionBlocked(g.px, ny, r)) { g.py = ny; moved = true; }
    }
    if (moved) {
        g.walkAnimT += dt;
        g.moving = true;
        g.stepCounter++;
        if (g.encounterGrace > 0) g.encounterGrace -= dt;
        else {
            // step-based encounter roll on open ground
            if (g.stepCounter % 14 == 0 && !inVillage(g.px, g.py) &&
                !nearStartVillage(g.px, g.py) && !inBossArena(g.px, g.py)) {
                float chance = 0.10f + 0.02f * (float)zoneLevelAt(g.px, g.py);
                if (hashf((int)(g.px * 100), (int)(g.py * 100), g.worldSeed) < chance) {
                    EnemyKind k = rollBiomeEnemy((int)floorf(g.px), (int)floorf(g.py),
                                                 zoneLevelAt(g.px, g.py));
                    battleStart({ k }, zoneLevelAt(g.px, g.py));
                    g.encounterGrace = 3.0f;
                }
            }
        }
    } else {
        pulseBlocked();
    }
}

void pulseBlocked() {
    g.blockedFx = std::min(1.0f, g.blockedFx + 0.35f);
}

static void stepPath(float dt) {
    if (g.walkQueue.empty()) { g.hasMoveTarget = false; g.moving = false; return; }
    auto [tx, ty] = g.walkQueue.front();
    float cx = tx + 0.5f, cy = ty + 0.5f;
    float dx = cx - g.px, dy = cy - g.py;
    float dist = sqrtf(dx * dx + dy * dy);
    float speed = (IsKeyDown(KEY_LEFT_SHIFT) ? RUN_SPEED : WALK_SPEED);
    float stepLen = speed * dt;
    if (dist <= stepLen || dist < 0.001f) {
        g.px = cx; g.py = cy;
        g.walkQueue.erase(g.walkQueue.begin());
        g.walkAnimT += dt;
        g.moving = true;
        g.stepCounter++;
        if (g.walkQueue.empty()) {
            g.hasMoveTarget = false;
            g.moving = false;
            if (g.walkDoorBuilding >= 0) {
                int wi = g.walkDoorBuilding;
                g.walkDoorBuilding = -1;
                enterBuilding(wi);
            }
        }
        return;
    }
    float nx = g.px + dx / dist * stepLen;
    float ny = g.py + dy / dist * stepLen;
    if (fabsf(dx) > fabsf(dy)) {
        g.facing = 2; g.sideFlip = dx < 0;
    } else {
        g.facing = dy > 0 ? 0 : 1;
    }
    if (!positionBlocked(nx, ny, 0.28f)) {
        g.px = nx; g.py = ny;
        g.moving = true;
        g.walkAnimT += dt;
    } else {
        g.walkQueue.clear();
        g.hasMoveTarget = false;
        g.walkDoorBuilding = -1;
        pulseBlocked();
    }
}

// ================================================================ monsters on the field
static void updateFieldMonsters(float dt) {
    fieldMonstersInView(g.px, g.py, gFieldMonsters);
    for (auto& m : gFieldMonsters) {
        m.phase += dt;
        m.x = m.anchorX + sinf(m.phase * 0.7f) * 0.7f;
        m.y = m.anchorY + cosf(m.phase * 0.9f) * 0.5f;
        float dx = g.px - m.x, dy = g.py - m.y;
        if (dx * dx + dy * dy < 0.85f * 0.85f) {
            battleStart({ m.kind }, zoneLevelAt(g.px, g.py), m.id);
            g.encounterGrace = 3.0f;
            return;
        }
    }
    // a short grace after battle also freezes random encounters
    if (g.encounterGrace > 0) g.encounterGrace -= dt * 0.25f;
}

// ================================================================ interaction
static void interactWithNearest() {
    // NPC first
    int npc = npcNear(g.px, g.py, 1.35f);
    if (npc >= 0) {
        Npc& n = g.npcs[npc];
        std::vector<DialogLine> lines;
        DialogAction action = DLG_NONE;
        if (n.role == NPC_SHOPKEEP) {
            lines.push_back({ n.face, n.name, n.name + " spreads her hands over the counter. \"Buying or selling?\"" });
            action = DLG_SHOP;
        } else if (n.role == NPC_INNKEEP) {
            lines.push_back({ n.face, n.name, "\"A bed's 20 gil, blankets included. Fights cost extra.\"" });
            action = DLG_INN;
        } else if (n.role == NPC_PRIEST) {
            lines.push_back({ n.face, n.name, "\"The Light asks nothing but a moment of stillness.\"" });
            action = DLG_TEMPLE;
        } else if (n.role == NPC_QUEST && n.name.find("Hedrick") != std::string::npos) {
            if (g.quests[QUEST_SLIMES].state == QS_OFFERED) {
                lines.push_back({ n.face, n.name, "\"You there! Slimes in my turnip rows again. Six of them, at least!\"" });
                lines.push_back({ n.face, n.name, "\"Thin them out and I'll pay you 40 gil. Mind the teeth. They have teeth, somehow.\"" });
                action = DLG_START_QUEST_SLIMES;
            } else if (g.quests[QUEST_SLIMES].state == QS_ACTIVE) {
                if (g.slimesSlain >= 6) {
                    lines.push_back({ n.face, n.name, "\"Six! Six slimes! My turnips thank you.\"" });
                    lines.push_back({ n.face, n.name, "\"Here — 40 gil. Don't spend it all on potions.\"" });
                    action = DLG_FINISH_QUEST_SLIMES;
                } else {
                    char buf[128];
                    snprintf(buf, sizeof(buf), "\"Still %d in my rows. You'll know them by the squelching.\"",
                             6 - g.slimesSlain);
                    lines.push_back({ n.face, n.name, buf });
                }
            } else {
                lines.push_back({ n.face, n.name, "\"The turnips are safe, thanks to you. Sit a while, hero.\"" });
            }
        } else {
            lines.push_back({ n.face, n.name, npcLine(n, n.lineSet + (int)(g.totalTime * 0.05f)) });
            int second = (int)(g.runTime * 0.5f);
            if (second % 3 == 0) lines.push_back({ n.face, n.name, npcLine(n, n.lineSet + 3) });
        }
        startDialog(lines, action);
        g.interactT = 0.25f;
        return;
    }

    // chest in front of the player
    for (size_t i = 0; i < gChestsInView.size(); i++) {
        Chest& c = gChestsInView[i];
        float dx = (c.tx + 0.5f) - g.px, dy = (c.ty + 0.5f) - g.py;
        if (dx * dx + dy * dy > 1.4f * 1.4f) continue;
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
        PlaySound(g.assets.treasure);
        g.interactT = 0.3f;
        return;
    }

    // building door
    if (inVillage(g.px, g.py)) {
        for (size_t i = 0; i < g.village.buildings.size(); i++) {
            const Building& b = g.village.buildings[i];
            float dx = (float)b.tx + 0.5f - g.px;
            float dy = (float)b.ty + 1.5f - g.py;
            if (dx * dx + dy * dy < 2.0f) {
                enterBuilding((int)i);
                return;
            }
        }
    }
    // the sign by the plaza
    if (inVillage(g.px, g.py) && fabsf(g.px) < 7 && g.py > 2 && g.py < 8) {
        std::vector<DialogLine> lines;
        lines.push_back({ -1, "", "ELMSWICK — 'Where the meadow begins.'" });
        lines.push_back({ -1, "", "South gate: the south meadow, then the wilds.\nNorth: the old temple of the Light." });
        startDialog(lines);
        return;
    }
    // nothing underfoot
    pushToast("Nothing here.");
}

void overworldInteract() { interactWithNearest(); }

// ================================================================ clicking
static void clickToMove() {
    Vector2 m = getVirtualMouse();
    if (!mouseInVirtual()) return;
    if (!IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) return;
    float wx = (m.x + g.camX) / TILE;
    float wy = (m.y + g.camY) / TILE;

    // clicking a building enters it
    if (inVillage(g.px, g.py)) {
        for (size_t i = 0; i < g.village.buildings.size(); i++) {
            const Building& b = g.village.buildings[i];
            float x0 = (float)(b.tx - b.w / 2) * TILE - g.camX;
            float y0 = (float)(b.ty - b.h + 1) * TILE - g.camY;
            float bw = (float)b.w * TILE, bh = (float)(b.h + 1) * TILE;
            if (CheckCollisionPointRec(m, { x0, y0, bw, bh })) {
                WalkPath path;
                int doorTx = b.tx, doorTy = b.ty + 1;
                if (findWalkPath(g.px, g.py, doorTx, doorTy, path) && !path.cells.empty()) {
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
    // clicking an npc starts a conversation from afar
    for (size_t i = 0; i < g.npcs.size(); i++) {
        const Npc& n = g.npcs[i];
        float sx = n.x * TILE - g.camX, sy = (n.y - 1.0f) * TILE - g.camY;
        if (CheckCollisionPointRec(m, { sx - 8, sy - 4, 32, 40 })) {
            WalkPath path;
            int tx = (int)floorf(n.x), ty = (int)floorf(n.y) + 1;
            nearestWalkable(tx, ty, tx, ty);
            if (findWalkPath(g.px, g.py, tx, ty, path) && !path.cells.empty()) {
                g.walkQueue = path.cells;
                g.hasMoveTarget = true;
                g.walkDoorBuilding = -1;
            }
            return;
        }
    }
    // otherwise walk
    int tx = (int)floorf(wx), ty = (int)floorf(wy);
    WalkPath path;
    if (findWalkPath(g.px, g.py, tx, ty, path) && !path.cells.empty()) {
        g.walkQueue = path.cells;
        g.hasMoveTarget = true;
        g.walkDoorBuilding = -1;
        g.clickFlashT = 0.4f;
        g.clickFlashX = (float)tx + 0.5f;
        g.clickFlashY = (float)ty + 0.5f;
    } else {
        pulseBlocked();
    }
}

// ================================================================ update
void overworldUpdate(float dt) {
    gAnimTime += dt;
    g.totalTime += dt;
    g.runTime += dt;
    if (g.toastT > 0) g.toastT -= dt;
    if (g.clickFlashT > 0) g.clickFlashT -= dt;
    if (g.interactT > 0) g.interactT -= dt;
    if (g.blockedFx > 0) g.blockedFx = std::max(0.0f, g.blockedFx - dt * 2.0f);
    if (g.shakeT > 0) g.shakeT -= dt;

    villageLoadAt(g.px, g.py);
    npcsLoadFor(g.px, g.py);
    chestsLoadFor(g.px, g.py);

    // NPC idle drift
    for (auto& n : g.npcs) {
        n.animT += dt;
        n.thinkT -= dt;
        if (n.thinkT <= 0) {
            n.thinkT = 2.0f + hashf((int)(n.x * 100), (int)(n.y * 100) + (int)(g.totalTime), 7u) * 3.0f;
            float a = hashf((int)(n.y * 100), (int)(n.x * 100) + (int)(g.totalTime * 3), 11u) * 6.2831853f;
            n.dx = (fabsf(cosf(a)) > 0.6f) ? (cosf(a) > 0 ? 1.0f : -1.0f) : 0.0f;
            n.dy = (fabsf(sinf(a)) > 0.6f) ? (sinf(a) > 0 ? 1.0f : -1.0f) : 0.0f;
            n.moving = (n.dx != 0 || n.dy != 0);
        }
        if (n.moving) {
            float nx = n.x + n.dx * 1.4f * dt;
            float ny = n.y + n.dy * 1.4f * dt;
            float dx = nx - n.homeX, dy = ny - n.homeY;
            if (dx * dx + dy * dy < 9.0f && !positionBlocked(nx, ny, 0.25f)) {
                n.x = nx; n.y = ny;
            } else {
                n.dx = -n.dx; n.dy = -n.dy;
            }
        }
    }

    // keyboard
    int kx = 0, ky = 0;
    if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) kx -= 1;
    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) kx += 1;
    if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)) ky -= 1;
    if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)) ky += 1;
    if (kx != 0 || ky != 0) {
        g.walkQueue.clear();
        g.hasMoveTarget = false;
        g.walkDoorBuilding = -1;
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
        if (g.encounterGrace > 0) g.encounterGrace -= dt * 0.35f;
    }

    updateFieldMonsters(dt);
    updateCamera(dt);

    // input: menu / interact
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_TAB)) {
        g.mode = MODE_MENU;
        g.menuPage = 0; g.menuIndex = 0;
        PlaySound(g.assets.confirm);
        return;
    }
    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_E) || IsKeyPressed(KEY_ENTER)) {
        interactWithNearest();
        return;
    }
    clickToMove();

    // hovering a sign / npc shows a prompt dot; handled in render
}

// ================================================================ interaction prompt
const char* interactionPrompt() {
    static char buf[128];
    if (!g.npcs.empty()) {
        int i = npcNear(g.px, g.py, 1.35f);
        if (i >= 0) {
            snprintf(buf, sizeof(buf), "SPACE: talk to %s", g.npcs[i].name.c_str());
            return buf;
        }
    }
    for (auto& c : gChestsInView) {
        float dx = (c.tx + 0.5f) - g.px, dy = (c.ty + 0.5f) - g.py;
        if (dx * dx + dy * dy < 1.4f * 1.4f) {
            return g.openedChests.count(c.id) ? "SPACE: empty chest" : "SPACE: open chest";
        }
    }
    if (inVillage(g.px, g.py)) {
        for (auto& b : g.village.buildings) {
            float dx = (float)b.tx + 0.5f - g.px, dy = (float)b.ty + 1.5f - g.py;
            if (dx * dx + dy * dy < 2.0f) return "SPACE: go inside";
        }
    }
    return nullptr;
}
