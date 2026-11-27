// cinema.cpp — staged arrivals, camera glides and area plates
//
// The complaint this answers is a real one: the game used to *cut*. You left a
// shop, a dungeon, a battle or the title card and the next frame was a field
// with the camera already parked — a hard jump into a map with no shot, no
// name and no beat. Nothing here changes what the world is; it changes how the
// world is *presented*, which is most of what makes a 16-bit game read as one.
#include "cinema.hpp"
#include "regions.hpp"
#include "world.hpp"
#include "pieces.hpp"
#include <algorithm>
#include <cmath>

namespace {

// ---- the shot -------------------------------------------------------------
// One shot at a time, in four beats:
//   ESTABLISH  the camera is pulled back along the axis the party came from
//              and glides in; input is held, the palette comes up in steps.
//   PLATE_IN   the area's name slides up into a plate.
//   PLATE_HOLD the plate sits; the player may walk again.
//   PLATE_OUT  the plate lifts away.
enum ShotStage : int { SHOT_NONE = 0, SHOT_ESTABLISH, SHOT_PLATE_IN, SHOT_PLATE_HOLD, SHOT_PLATE_OUT };

constexpr float ESTABLISH_TIME = 1.30f;   // seconds of glide
constexpr float PLATE_IN_TIME   = 0.22f;
constexpr float PLATE_HOLD_TIME = 1.85f;
constexpr float PLATE_OUT_TIME  = 0.30f;
constexpr float SKIP_AFTER     = 0.45f;   // the pan can be skipped once it has read

struct Shot {
    int stage = SHOT_NONE;
    float t = 0.0f;
    float panX = 0.0f, panY = 0.0f;       // pixels the camera starts away from the party
    bool hasPlate = false;
    float plateA = 0.0f;                  // 0..1 plate visibility
    float plateRise = 0.0f;               // pixels, 6 -> 0 as the plate settles
    std::string title, sub;
};

Shot gShot;
uint64_t gNamedVillage = ~0ull;

// A pure ease-out. The camera decelerates into the party instead of stopping
// dead, which is the difference between a shot and a scroll.
float easeOut(float u) {
    u = std::max(0.0f, std::min(1.0f, u));
    float inv = 1.0f - u;
    return 1.0f - inv * inv * inv;
}

void beginShot(const std::string& title, const std::string& sub,
               float panTilesX, float panTilesY, bool withPlate) {
    gShot.t = 0.0f;
    gShot.panX = panTilesX * (float)TILE;
    gShot.panY = panTilesY * (float)TILE;
    gShot.hasPlate = withPlate;
    gShot.plateA = 0.0f;
    gShot.plateRise = 6.0f;
    gShot.title = title;
    gShot.sub = sub;
    bool pans = (std::fabs(gShot.panX) > 0.5f || std::fabs(gShot.panY) > 0.5f);
    gShot.stage = pans ? SHOT_ESTABLISH : (withPlate ? SHOT_PLATE_IN : SHOT_NONE);
}

const char* regionSubtitle(float x, float y) {
    switch (regionAt(x, y)) {
        case 0: return "the last field office";
        case 1: return "beyond the waystone";
        default: return "past the edge of the map";
    }
}

} // namespace

void cinemaClear() {
    gShot = Shot{};
    gNamedVillage = ~0ull;
}

void cinemaArrive(const std::string& title, const std::string& sub,
                  float panTilesX, float panTilesY) {
    if (title.empty()) return;
    beginShot(title, sub, panTilesX, panTilesY, true);
}

void cinemaAnnounce(const std::string& title, const std::string& sub) {
    if (title.empty()) return;
    // Never talk over a shot that is already naming something newer.
    if (gShot.stage == SHOT_ESTABLISH) return;
    beginShot(title, sub, 0.0f, 0.0f, true);
}

void cinemaVillageTick() {
    if (!g.villageLoaded) return;
    if (g.villageKey == gNamedVillage) return;
    gNamedVillage = g.villageKey;
    if (g.village.startVillage) return;      // the opening shot already said it
    cinemaAnnounce(g.village.name, "a village on the road");
}

void cinemaAnnounceRegion() {
    cinemaAnnounce(regionNameAt(g.px, g.py), regionSubtitle(g.px, g.py));
}

void cinemaUpdate(float dt) {
    if (gShot.stage == SHOT_NONE) return;
    gShot.t += dt;

    // The shot owns the camera while it establishes, so setting camX/camY here
    // is authoritative; overworldUpdate skips its own smoothing in that window.
    if (gShot.stage == SHOT_ESTABLISH) {
        float u = easeOut(gShot.t / ESTABLISH_TIME);
        float targetX = 0.0f, targetY = 0.0f;
        pieceOriginPixels(g.px, g.py, (float)g.vw, (float)g.vh, targetX, targetY);
        g.camX = targetX + gShot.panX * (1.0f - u);
        g.camY = targetY + gShot.panY * (1.0f - u);
        bool skip = gShot.t > SKIP_AFTER &&
                    (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE));
        if (skip) gShot.t = ESTABLISH_TIME;
        if (gShot.t >= ESTABLISH_TIME) {
            g.camX = targetX;
            g.camY = targetY;
            gShot.t = 0.0f;
            gShot.stage = gShot.hasPlate ? SHOT_PLATE_IN : SHOT_NONE;
        }
        return;
    }

    // Plate beats. Input is released as soon as the plate has begun to settle,
    // so a name card never costs the player a second of walking.
    if (gShot.stage == SHOT_PLATE_IN) {
        float u = std::min(1.0f, gShot.t / PLATE_IN_TIME);
        gShot.plateA = u;
        gShot.plateRise = 6.0f * (1.0f - easeOut(u));
        if (gShot.t >= PLATE_IN_TIME) { gShot.t = 0.0f; gShot.stage = SHOT_PLATE_HOLD; }
        return;
    }
    if (gShot.stage == SHOT_PLATE_HOLD) {
        gShot.plateA = 1.0f;
        gShot.plateRise = 0.0f;
        if (gShot.t >= PLATE_HOLD_TIME) { gShot.t = 0.0f; gShot.stage = SHOT_PLATE_OUT; }
        return;
    }
    if (gShot.stage == SHOT_PLATE_OUT) {
        float u = std::min(1.0f, gShot.t / PLATE_OUT_TIME);
        gShot.plateA = 1.0f - u;
        gShot.plateRise = -4.0f * u;
        if (gShot.t >= PLATE_OUT_TIME) {
            gShot = Shot{};
        }
    }
}

bool cinemaOwnsCamera() {
    return gShot.stage == SHOT_ESTABLISH;
}

bool cinemaBlocksInput() {
    if (gShot.stage == SHOT_ESTABLISH) return true;
    if (gShot.stage == SHOT_PLATE_IN) return true;
    return false;
}

bool cinemaPlateVisible() {
    return gShot.hasPlate && gShot.plateA > 0.01f &&
           (gShot.stage == SHOT_PLATE_IN || gShot.stage == SHOT_PLATE_HOLD ||
            gShot.stage == SHOT_PLATE_OUT);
}

void drawAreaPlate() {
    if (!cinemaPlateVisible()) return;

    const float a = std::max(0.0f, std::min(1.0f, gShot.plateA));
    unsigned char alpha = (unsigned char)(a * 255.0f);

    const char* title = gShot.title.c_str();
    const char* sub = gShot.sub.c_str();
    Vector2 tsz = measureText(title, 16);
    Vector2 ssz = (sub && sub[0]) ? measureText(sub, 8) : Vector2{ 0.0f, 0.0f };

    int contentW = (int)std::max(tsz.x, ssz.x);
    int plateW = std::max(184, contentW + 56);
    plateW = std::min(plateW, g.vw - 32);
    int plateH = (ssz.x > 0.0f) ? 44 : 32;
    int plateX = (g.vw - plateW) / 2;
    int plateY = 74 + (int)std::lround(gShot.plateRise);

    // A deep band, a hairline of gold above and below, and the name in the
    // gold the rest of the UI uses. Nothing rounded: this is a 16-bit plate.
    DrawRectangle(plateX - 1, plateY - 1, plateW + 2, plateH + 2,
                  Color{ 4, 6, 14, (unsigned char)(alpha * 0.72f) });
    DrawRectangle(plateX, plateY, plateW, plateH, Color{ 12, 18, 42, (unsigned char)(alpha * 0.90f) });
    DrawRectangle(plateX, plateY + 2, plateW, 1, Color{ 255, 214, 130, (unsigned char)(alpha * 0.85f) });
    DrawRectangle(plateX, plateY + plateH - 3, plateW, 1, Color{ 255, 214, 130, (unsigned char)(alpha * 0.85f) });
    DrawRectangle(plateX, plateY + 3, plateW, 1, Color{ 60, 78, 130, alpha });
    DrawRectangle(plateX, plateY + plateH - 4, plateW, 1, Color{ 60, 78, 130, alpha });

    float ty = (float)(plateY + (plateH - (int)tsz.y) / 2 - (ssz.x > 0.0f ? 5 : 0));
    drawTextCentered(title, (float)(plateX + plateW / 2), std::floor(ty),
                     Color{ 255, 236, 176, alpha }, 16);
    if (ssz.x > 0.0f) {
        drawTextCentered(sub, (float)(plateX + plateW / 2),
                         std::floor((float)(plateY + plateH) - 14.0f),
                         Color{ 186, 204, 240, alpha }, 8);
    }
}

void drawSteppedFade() {
    if (g.fade <= 0.001f) return;
    // SNES fades dropped the palette in fixed steps rather than ramping alpha.
    // Eight steps is what the hardware's brightness control actually did, and
    // the stops are what make a cut feel like a machine did it on purpose.
    int steps = (int)std::ceil(g.fade * 8.0f - 0.001f);
    steps = std::max(1, std::min(8, steps));
    float a = (float)steps / 8.0f;
    DrawRectangle(0, 0, g.vw, g.vh, Fade(BLACK, a));
}

void drawCinemaOverlay() {
    drawAreaPlate();
}
