// overworld.cpp — Lufia II-style miniature overworld movement, ship sailing,
// site entry, step-based encounters, world map toggle, and camera tracking.
#include "game.hpp"
#include "world.hpp"
#include "overworld_gen.hpp"
#include "battle.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

std::vector<FieldMonster> gFieldMonsters;
std::vector<Chest> gChestsInView;

void overworldSnapCamera() {
    g.camX = g.px * TILE - (float)g.vw * 0.5f;
    g.camY = g.py * TILE - (float)g.vh * 0.5f;
}

void pulseBlocked() {
    g.blockedFx = 0.25f;
}

const char* interactionPrompt() {
    int tx = (int)floorf(g.px);
    int ty = (int)floorf(g.py);
    if (const char* p = worldSitePrompt(tx, ty)) return p;
    // Also check one tile ahead in facing direction so approaching a town/cave highlights immediately
    int fx = tx + (g.facing == 2 ? (g.sideFlip ? -1 : 1) : 0);
    int fy = ty + (g.facing == 0 ? 1 : (g.facing == 1 ? -1 : 0));
    return worldSitePrompt(fx, fy);
}

void overworldInteract() {
    int tx = (int)floorf(g.px);
    int ty = (int)floorf(g.py);
    if (worldEnterSiteAt(tx, ty)) return;
    int fx = tx + (g.facing == 2 ? (g.sideFlip ? -1 : 1) : 0);
    int fy = ty + (g.facing == 0 ? 1 : (g.facing == 1 ? -1 : 0));
    if (worldEnterSiteAt(fx, fy)) return;
}

static bool canMoveToOw(float x, float y, bool& wouldBoardShip, bool& wouldDisembark) {
    wouldBoardShip = false;
    wouldDisembark = false;
    int tx = (int)floorf(x);
    int ty = (int)floorf(y);
    auto cell = apfa::owSampleTile(g.worldSeed ? g.worldSeed : 1337u, tx, ty);

    if (g.onShip) {
        if (apfa::owIsSailable(cell.kind)) return true;
        if (apfa::owIsWalkableOnFoot(cell.kind)) {
            wouldDisembark = true;
            return true;
        }
        return false;
    } else {
        if (apfa::owIsWalkableOnFoot(cell.kind)) return true;
        if (g.hasBoat && apfa::owIsSailable(cell.kind)) {
            wouldBoardShip = true;
            return true;
        }
        return false;
    }
}

void overworldUpdate(float dt) {
    g.totalTime += dt;
    g.runTime += dt;
    if (g.owSiteDebounce > 0.0f) g.owSiteDebounce -= dt;
    if (g.encounterGrace > 0.0f) g.encounterGrace -= dt;
    if (g.blockedFx > 0.0f) g.blockedFx -= dt;

    // Toggle [M] parchment World Map
    if (IsKeyPressed(KEY_M)) {
        g.travelMap = !g.travelMap;
        playSoundIf(g.assets.cursor);
        return;
    }
    if (g.travelMap) {
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_X) || IsKeyPressed(KEY_TAB) ||
            IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
            g.travelMap = false;
            playSoundIf(g.assets.cancel);
        }
        return;
    }

    // Pause menu
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_X)) {
        playSoundIf(g.assets.confirm);
        g.menuReturn = MODE_OVERWORLD;
        g.menuPage = 0;
        g.menuIndex = 0;
        g.menuItem = -1;
        g.mode = MODE_MENU;
        return;
    }

    // Quicksave
    if (IsKeyPressed(KEY_F5)) {
        if (saveGame()) { playSoundIf(g.assets.chime); pushToast("World Map progress saved."); }
        else pushToast("Save failed.");
    }

    // Explicit site entry button
    if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        int tx = (int)floorf(g.px);
        int ty = (int)floorf(g.py);
        if (worldEnterSiteAt(tx, ty)) return;
        int fx = tx + (g.facing == 2 ? (g.sideFlip ? -1 : 1) : 0);
        int fy = ty + (g.facing == 0 ? 1 : (g.facing == 1 ? -1 : 0));
        if (worldEnterSiteAt(fx, fy)) return;
    }

    // Directional input
    float dx = 0.0f, dy = 0.0f;
    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) dx += 1.0f;
    if (IsKeyDown(KEY_LEFT)  || IsKeyDown(KEY_A)) dx -= 1.0f;
    if (IsKeyDown(KEY_DOWN)  || IsKeyDown(KEY_S)) dy += 1.0f;
    if (IsKeyDown(KEY_UP)    || IsKeyDown(KEY_W)) dy -= 1.0f;

    bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    float speed = g.onShip ? (shift ? 7.8f : 6.0f) : (shift ? 6.2f : 4.4f);

    if (dx != 0.0f || dy != 0.0f) {
        if (dx != 0.0f && dy != 0.0f) { dx *= 0.7071f; dy *= 0.7071f; }
        if (fabsf(dx) >= fabsf(dy)) {
            g.facing = 2;
            g.sideFlip = (dx < 0.0f);
        } else {
            g.facing = (dy > 0.0f) ? 0 : 1;
        }

        float oldX = g.px, oldY = g.py;
        float nx = g.px + dx * speed * dt;
        float ny = g.py + dy * speed * dt;

        bool moved = false;
        bool board = false, disembark = false;

        // Try sliding on X and Y independently for crisp overworld navigation
        if (dx != 0.0f && canMoveToOw(nx, g.py, board, disembark)) {
            g.px = nx; moved = true;
            if (board) { g.onShip = true; pushToast("Set sail!"); }
            else if (disembark) { g.onShip = false; }
        }
        board = disembark = false;
        if (dy != 0.0f && canMoveToOw(g.px, ny, board, disembark)) {
            g.py = ny; moved = true;
            if (board) { g.onShip = true; pushToast("Set sail!"); }
            else if (disembark) { g.onShip = false; }
        }

        g.moving = moved;
        if (moved) {
            g.walkAnimT += dt;
            float dist = hypotf(g.px - oldX, g.py - oldY);
            int curTx = (int)floorf(g.px);
            int curTy = (int)floorf(g.py);

            // Clear site re-entry guard once player walks off the exited site tile
            if (curTx != g.owLastSiteTx || curTy != g.owLastSiteTy) {
                g.owLastSiteTx = INT_MIN;
                g.owLastSiteTy = INT_MIN;
            }

            // Update region banner when entering a new land
            std::string reg = apfa::owRegionTitle(g.worldSeed ? g.worldSeed : 1337u, curTx, curTy);
            if (reg != g.regionName) {
                g.regionName = reg;
                g.regionBannerT = 2.4f;
            }

            // Auto-enter site upon stepping cleanly onto a site icon tile (classic Lufia II!)
            if (g.owSiteDebounce <= 0.0f &&
                (curTx != g.owLastSiteTx || curTy != g.owLastSiteTy)) {
                apfa::OwSiteInfo s;
                if (apfa::owSiteAtTile(g.worldSeed ? g.worldSeed : 1337u, curTx, curTy, s)) {
                    float cx = (float)s.tx + 0.5f, cy = (float)s.ty + 0.5f;
                    if (hypotf(g.px - cx, g.py - cy) < 0.36f) {
                        if (worldEnterSiteAt(curTx, curTy)) return;
                    }
                }
            }

            // Step-based wild encounters (never within 2 tiles of any settlement!)
            apfa::OwSiteInfo nearTown;
            bool safeNearTown = apfa::owNearestSite(g.worldSeed ? g.worldSeed : 1337u,
                                                    curTx, curTy, 2, nearTown, true, false);
            if (!safeNearTown && g.encounterGrace <= 0.0f) {
                auto cell = apfa::owSampleTile(g.worldSeed ? g.worldSeed : 1337u, curTx, curTy);
                float rateMul = cell.hasRoad ? 0.45f
                              : (cell.kind == apfa::OWT_FOREST_LEAF || cell.kind == apfa::OWT_FOREST_PINE ||
                                 cell.kind == apfa::OWT_FOREST_SNOW || cell.kind == apfa::OWT_SWAMP ? 1.35f : 1.0f);
                g.stepDist += dist * rateMul;
                if (g.stepDist >= 11.5f) {
                    g.stepDist = 0.0f;
                    g.encounterGrace = 4.0f;
                    int tier = apfa::owDangerTier(curTx, curTy);
                    int zoneLv = std::max(1, tier * 2 - 1);
                    EnemyKind lead = rollBiomeEnemy(curTx * 4, curTy * 4, zoneLv);
                    auto group = buildEncounterGroup(lead, curTx * 4, curTy * 4, zoneLv);
                    g.battleReturn = MODE_OVERWORLD;
                    g.owReturnX = g.px;
                    g.owReturnY = g.py;
                    battleStart(group, zoneLv, 0, false);
                    return;
                }
            }
        }
    } else {
        g.moving = false;
    }

    // Smooth camera follow
    float targetX = g.px * TILE - (float)g.vw * 0.5f;
    float targetY = g.py * TILE - (float)g.vh * 0.5f;
    float k = std::min(1.0f, dt * 11.0f);
    g.camX += (targetX - g.camX) * k;
    g.camY += (targetY - g.camY) * k;
}
