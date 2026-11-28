// overworld.cpp — overworld movement, collision, transporter entry, and camera
#include "world.hpp"
#include "regions.hpp"
#include "battle.hpp"
#include "commons.hpp"
#include <algorithm>
#include <cmath>

std::vector<FieldMonster> gFieldMonsters;

namespace {
float gBlockedPulse = 0.0f;
float gStepAccum = 0.0f;
} // namespace

void pulseBlocked() {
    gBlockedPulse = 0.35f;
}

void overworldSnapCamera() {
    g.camX = g.px * TILE - g.vw * 0.5f;
    g.camY = g.py * TILE - g.vh * 0.5f;
}

const char* interactionPrompt() {
    int tx = (int)std::floor(g.px);
    int ty = (int)std::floor(g.py);
    OwSiteKind site = worldSiteAt(tx, ty);
    switch (site) {
        case OW_SITE_TOWN:    return "Enter Town";
        case OW_SITE_CASTLE:  return "Enter Castle";
        case OW_SITE_DUNGEON: return "Enter Cavern";
        case OW_SITE_TEMPLE:  return "Enter Temple";
        case OW_SITE_TOWER:   return "Enter Tower";
        case OW_SITE_RUINS:   return "Explore Ruins";
        case OW_SITE_CAMP:    return "Visit Camp";
        default: break;
    }
    return nullptr;
}

void overworldInteract() {
    if (commonsTryInteract()) return;
    int tx = (int)std::floor(g.px);
    int ty = (int)std::floor(g.py);
    if (worldEnterSiteAt(tx, ty)) {
        return;
    }
}

void overworldUpdate(float dt) {
    if (gBlockedPulse > 0.0f) gBlockedPulse = std::max(0.0f, gBlockedPulse - dt);

    float dx = 0.0f, dy = 0.0f;
    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))    dy -= 1.0f;
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))  dy += 1.0f;
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  dx -= 1.0f;
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) dx += 1.0f;

    if (dx != 0.0f || dy != 0.0f) {
        float len = std::sqrt(dx * dx + dy * dy);
        dx /= len;
        dy /= len;
        const float speed = 5.5f; // tiles per second
        float nx = g.px + dx * speed * dt;
        float ny = g.py + dy * speed * dt;

        if (worldTileWalkable((int)std::floor(nx), (int)std::floor(g.py))) {
            g.px = nx;
        } else {
            pulseBlocked();
        }
        if (worldTileWalkable((int)std::floor(g.px), (int)std::floor(ny))) {
            g.py = ny;
        } else {
            pulseBlocked();
        }

        gStepAccum += speed * dt;
        updateWorldStreaming(dt);
    }

    if (IsKeyPressed(KEY_E) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        overworldInteract();
    }

    commonsTick(dt);

    // Smooth camera follow
    float targetX = g.px * TILE - g.vw * 0.5f;
    float targetY = g.py * TILE - g.vh * 0.5f;
    float k = std::min(1.0f, dt * 10.0f);
    g.camX += (targetX - g.camX) * k;
    g.camY += (targetY - g.camY) * k;
}
