// sail.cpp — the Open Water: docking, sailing, landings, and the Tidemill Isles.
//
// This is the growth that turns the millpond from wallpaper into a road.
//
// Before it, water was a wall everywhere: cellWalkable() refused G_WATER, so
// the 5,000-tile lake west of Elmswick could be looked at and never entered,
// and every island in it was decoration. Past the coast the same was true of
// the whole sea — the world simply ran on forever in every direction, which
// is how a player ends up walking thirty minutes north without arriving
// anywhere.
//
// The Open Water fixes both halves:
//
//   * The world has a COAST (world.cpp, shoreT()). Land stops at a seed's
//     own shoreline and the sea past it is deep and unenterable. One
//     continent, about a minute and a half across at running speed — the size
//     of an SNES world map, not the size of an ocean.
//   * The water inside the coast is SAILABLE (cellSailable()). The pond, the
//     rivers and the highland tarns are all fair water; deep water is not a
//     different rule, it is simply water.
//   * The ferryman sells you the boat (DLG_BUY_BOAT) and every dock on the
//     map boards it. Docks are found from the shore, never authored, so they
//     move with the seed and always sit on ground you can stand on.
//   * Three Tidemill Isles are stamped into the pond (world.cpp). Each has a
//     Tidewell: dry until found, running afterwards. Finding all three
//     finishes "The Tidemill Islands" and the isles' stones light for good.
//
// The sail is its own Mode (MODE_SAIL), not a flag on the overworld. It has
// its own camera, its own encounter roll (water things only), its own music
// (the field cue, because the score has one and silence over a lake is worse),
// and it draws its own water. Battles return here; the menu returns here; the
// map is still the same world underneath, so the Journal works unchanged.
#include "game.hpp"
#include "world.hpp"
#include "regions.hpp"
#include "battle.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

float gSailX = 0, gSailY = 0, gSailFacing = 0;
static bool sailMoving = false;
static float sailAnimT = 0;
static float sailGrace = 0;          // seconds of no encounters after landing
static float sailStepDist = 0;
static int sailSteps = 0;
static float sailCamX = 0, sailCamY = 0;
static float sailBob = 0;
static float wake[10][2];            // recent stern positions, newest last
static int wakeN = 0;
static float wakeAge[10];

static void stampTile(Texture2D atlas, int mask, int row, int x, int y) {
    DrawTextureRec(atlas, { (float)(mask * TILE), (float)(row * TILE), TILE, TILE },
                   { (float)x, (float)y }, WHITE);
}
static const float SAIL_SPEED = 5.6f;     // tiles / second: a little faster than a walk
static const float SAIL_RUN = 8.6f;       // and a proper clip under full sail

// ================================================================ names

// Which isle's Tidewell stands at this tile, or -1.
static int isleAtWell(int tx, int ty) {
    for (int i = 0; i < tideIslandCount(); i++) {
        if ((int)floorf(tideIslandWellX(i)) == tx && (int)floorf(tideIslandWellY(i)) == ty) return i;
    }
    return -1;
}

const char* sailZoneName() {
    static char buf[64];
    // The pond has one name; the rest of the water borrows the country's.
    if (gSailX * gSailX + gSailY * gSailY < 130.0f * 130.0f && gSailX < 40.0f && gSailY > -60.0f) {
        return "Elmswick Millpond";
    }
    snprintf(buf, sizeof(buf), "The Open Water");
    return buf;
}

// ================================================================ docks
// Boarding and landing search a few tiles from the bank. Do not let that
// convenience hop move a party across a still-sealed regional boundary.
static bool sealedBorderBetween(int ax, int ay, int bx, int by) {
    int a = regionAt(ax + 0.5f, ay + 0.5f);
    int b = regionAt(bx + 0.5f, by + 0.5f);
    for (int ring = 1; ring <= 2; ++ring)
        if (!regionPassOpen(ring) && ((a < ring) != (b < ring))) return true;
    return false;
}

// Landing needs a beach: a walkable tile with water beside it. The party steps
// ashore onto that tile; the boat stays at the water's edge behind them.
static bool landingNear(int wx, int wy, int maxR, int& ox, int& oy) {
    for (int r = 0; r <= maxR; r++) {
        for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) {
            if (std::max(abs(dx), abs(dy)) != r) continue;
            int x = wx + dx, y = wy + dy;
            if (sealedBorderBetween(wx, wy, x, y) || !cellWalkable(x, y)) continue;
            for (int oy2 = -1; oy2 <= 1; oy2++) for (int ox2 = -1; ox2 <= 1; ox2++) {
                if (sampleGround(x + ox2, y + oy2).kind == G_WATER) { ox = x; oy = y; return true; }
            }
        }
    }
    return false;
}

// ================================================================ boarding
// The water tile the boat is moored at: nearest open water to where the party
// stands. Casting off is a step off the bank, not a walk back to a dock — the
// ferryman rents you the boat itself, and a rented boat waits where you left it.
static bool mooringNear(int tx, int ty, int maxR, int& ox, int& oy) {
    for (int r = 1; r <= maxR; r++) {
        for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) {
            if (std::max(abs(dx), abs(dy)) != r) continue;
            if (!sealedBorderBetween(tx, ty, tx + dx, ty + dy) && cellSailable(tx + dx, ty + dy)) {
                ox = tx + dx; oy = ty + dy; return true;
            }
        }
    }
    return false;
}

bool sailCanBoard() {
    if (!g.hasBoat) return false;
    int wx = 0, wy = 0;
    return mooringNear((int)floorf(g.px), (int)floorf(g.py), 3, wx, wy);
}

void sailEnter() {
    int wx = 0, wy = 0;
    if (!sailCanBoard() || !mooringNear((int)floorf(g.px), (int)floorf(g.py), 3, wx, wy)) {
        pushToast(g.hasBoat ? "No open water within reach." : "You have no boat.");
        return;
    }
    gSailX = wx + 0.5f;
    gSailY = wy + 0.5f;
    g.sailing = true;
    g.mode = MODE_SAIL;
    g.moving = false;
    g.walkQueue.clear();
    g.hasMoveTarget = false;
    sailCamX = gSailX * TILE - g.vw * 0.5f;
    sailCamY = gSailY * TILE - g.vh * 0.5f;
    sailGrace = 2.5f;
    wakeN = 0;
    sailMoving = false;
    g.surfT = 0;
    g.fade = 1.0f;
    g.fadeDir = -1;
    playSoundIf(g.assets.door);
    pushToast("Cast off. SPACE at a beach to land.");
}

void sailExit() {
    int lx = 0, ly = 0;
    int sx = (int)floorf(gSailX), sy = (int)floorf(gSailY);
    if (!landingNear(sx, sy, 3, lx, ly)) {
        pushToast("No landing here — sail closer to a shore.");
        return;
    }
    g.sailing = false;
    g.mode = MODE_OVERWORLD;
    g.px = lx + 0.5f;
    g.py = ly + 0.5f;
    g.facing = 0;
    g.moving = false;
    // A beach or island is a place to explore at field scale. The normal
    // boundary check can switch back to the travel map after leaving it.
    g.travelMap = false;
    g.travelContext = 0;
    g.encounterGrace = 3.0f;
    g.stepDist = 0;
    overworldSnapCamera();
    g.fade = 1.0f;
    g.fadeDir = -1;
    playSoundIf(g.assets.door);
    // landing next to a Tidewell is how you *find* it: the party must walk the
    // last few tiles themselves, so the discovery stays a thing you did.
    int isle = isleAtWell(lx, ly);
    if (isle >= 0 && !(g.tideMask & (1 << isle))) {
        char msg[128];
        snprintf(msg, sizeof(msg), "You step ashore on %s.", tideIslandName(isle));
        pushToast(msg);
    }
}

// ================================================================ the Tidewells
// Interacting with a Tidewell inside a battle-free moment: light it, count it,
// and finish the quest at three. Called from overworldInteract().
bool tideTryInteract() {
    for (int i = 0; i < tideIslandCount(); i++) {
        float wx = tideIslandWellX(i), wy = tideIslandWellY(i);
        float dx = wx - g.px, dy = wy - g.py;
        if (dx * dx + dy * dy > 2.6f * 2.6f) continue;
        if (g.tideMask & (1 << i)) {
            std::vector<DialogLine> lines;
            lines.push_back({ -1, "", std::string("The Tidewell on ") + tideIslandName(i) +
                                       " runs clear and cold. Its stones remember you." });
            startDialog(lines);
            return true;
        }
        g.tideMask |= (1 << i);
        g.tideFound = std::min(TIDE_ISLANDS, g.tideFound + 1);
        if (g.quests[QUEST_TIDE].state != QS_DONE) {
            g.quests[QUEST_TIDE].state = QS_ACTIVE;
            g.quests[QUEST_TIDE].progress = g.tideFound;
            g.quests[QUEST_TIDE].goal = TIDE_ISLANDS;
        }
        playSoundIf(g.assets.treasure);
        char buf[128];
        snprintf(buf, sizeof(buf), "Tidewell %d of %d found: the basin fills as you watch.",
                 g.tideFound, TIDE_ISLANDS);
        std::vector<DialogLine> lines;
        lines.push_back({ -1, "", std::string("You lay a hand on the stone of ") +
                                   tideIslandName(i) + "." });
        lines.push_back({ -1, "", buf });
        if (g.tideFound >= TIDE_ISLANDS) {
            lines.push_back({ -1, "", "All three wells run. The Tidemill Islands are tended again." });
            startDialog(lines, DLG_SAIL_TIDE);
        } else {
            lines.push_back({ -1, "", "The other isles are out there, low on the water. Sail and look." });
            startDialog(lines);
        }
        return true;
    }
    return false;
}

// A prompt when the party stands by a Tidewell (after the quest's other lines).
bool tidePrompt(const char** text) {
    for (int i = 0; i < tideIslandCount(); i++) {
        float dx = tideIslandWellX(i) - g.px, dy = tideIslandWellY(i) - g.py;
        if (dx * dx + dy * dy > 2.6f * 2.6f) continue;
        *text = (g.tideMask & (1 << i)) ? "SPACE: the running Tidewell" : "SPACE: wake the Tidewell";
        return true;
    }
    return false;
}

// ================================================================ movement
static void sailStep(float dt) {
    int kx = 0, ky = 0;
    if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) kx -= 1;
    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) kx += 1;
    if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)) ky -= 1;
    if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)) ky += 1;
    if (kx != 0 && ky != 0) kx = 0;                     // cardinal only, like the SNES
    if (kx == 0 && ky == 0) {
        sailMoving = false;
        return;
    }
    float speed = (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) ? SAIL_RUN : SAIL_SPEED;
    float nx = gSailX + kx * speed * dt;
    float ny = gSailY + ky * speed * dt;
    // The hull has a bow and a stern: test both corners so a boat cannot clip
    // its nose through a bank and stick there.
    float half = 0.42f;
    bool ok = true;
    for (int sy = -1; sy <= 1 && ok; sy += 2) {
        for (int sx = -1; sx <= 1 && ok; sx += 2) {
            if (!cellSailable((int)floorf(nx + sx * half), (int)floorf(ny + sy * half))) ok = false;
        }
    }
    if (ok) {
        gSailX = nx;
        gSailY = ny;
        sailMoving = true;
        sailAnimT += dt * (speed / SAIL_SPEED);
        g.facing = (kx < 0) ? 2 : (kx > 0 ? 2 : (ky < 0 ? 1 : 0));
        if (kx != 0) g.sideFlip = kx < 0;
        g.surfT += dt;
        // the wake: stern positions drop behind the hull and fade
        static float dropT = 0;
        dropT += dt;
        if (dropT > 0.09f) {
            dropT = 0;
            for (int i = 0; i < std::min(wakeN, 9); i++) {
                wake[i][0] = wake[i + 1][0]; wake[i][1] = wake[i + 1][1];
                wakeAge[i] = wakeAge[i + 1] + 0.09f;
            }
            wake[std::min(wakeN, 9)][0] = gSailX - kx * 0.55f;
            wake[std::min(wakeN, 9)][1] = gSailY - ky * 0.55f;
            wakeAge[std::min(wakeN, 9)] = 0;
            wakeN = std::min(wakeN + 1, 9);
        }
    } else {
        sailMoving = false;
        g.blockedFx = std::min(1.0f, g.blockedFx + 0.35f);
    }
    // encounters at sea: only on open water, and only things that live there
    if (sailMoving && sailGrace <= 0) {
        sailStepDist += speed * dt;
        if (sailStepDist >= 7.5f) {
            sailStepDist -= 7.5f;
            sailSteps++;
            float roll = hashf((int)floorf(gSailX) * 13 + sailSteps * 29,
                               (int)floorf(gSailY) * 17 + g.defeatedCount * 5,
                               g.worldSeed ^ 0x5a110u);
            int zl = std::max(2, zoneLevelAt(gSailX, gSailY));
            float chance = std::min(0.20f, 0.08f + 0.008f * (float)zl);
            if (roll < chance) {
                static const EnemyKind sea[4] = { EN_SERPENT, EN_NAGA, EN_DRAKE, EN_NAGA };
                EnemyKind k = sea[(hash2i((int)gSailX, (int)gSailY, g.worldSeed) >> 3) & 3];
                std::vector<EnemyKind> group{ k };
                if (zl >= 4 && roll < chance * 0.45f) group.push_back(EN_SERPENT);
                battleStart(group, std::max(3, zl));
                sailGrace = 4.0f;
            }
        }
    }
}

// ================================================================ update
void sailUpdate(float dt) {
    gAnimTime += dt;
    g.totalTime += dt;
    g.runTime += dt;
    g.surfT += dt;
    if (g.toastT > 0) g.toastT = std::max(0.0f, g.toastT - dt);
    if (g.blockedFx > 0) g.blockedFx = std::max(0.0f, g.blockedFx - dt * 2.0f);
    if (g.interactT > 0) g.interactT = std::max(0.0f, g.interactT - dt);
    if (g.shakeT > 0) g.shakeT = std::max(0.0f, g.shakeT - dt);
    if (sailGrace > 0) sailGrace = std::max(0.0f, sailGrace - dt);

    sailBob = sinf(g.surfT * 1.7f) * 1.2f;

    sailStep(dt);

    // camera: same smoothing as the walk, but a touch looser so the horizon
    // glides instead of sticking to the hull
    float tx = gSailX * TILE - g.vw * 0.5f;
    float ty = gSailY * TILE - g.vh * 0.5f;
    float k = 1.0f - powf(0.0025f, dt);
    sailCamX += (tx - sailCamX) * k;
    sailCamY += (ty - sailCamY) * k;
    if (!std::isfinite(sailCamX)) sailCamX = tx;
    if (!std::isfinite(sailCamY)) sailCamY = ty;
    g.camX = roundf(sailCamX);
    g.camY = roundf(sailCamY);

    // wake ages
    for (int i = 0; i < wakeN; i++) wakeAge[i] += dt;

    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_TAB)) {
        g.menuReturn = MODE_SAIL;
        g.mode = MODE_MENU;
        g.menuPage = 0; g.menuIndex = 0;
        g.journalTab = 0; g.commonsJournalSel = 0;
        playSoundIf(g.assets.confirm);
        return;
    }
    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_E) || IsKeyPressed(KEY_ENTER)) {
        sailInteract();
        return;
    }
    // Shift+Space or the F key: whistle for the dock you left, so a player can
    // never be stranded on the water with a spent boat.
    if (IsKeyPressed(KEY_F)) {
        Dock d;
        if (dockFind(true, d)) {
            gSailX = d.wx + 0.5f;
            gSailY = d.wy + 0.5f;
            g.fade = 1.0f;
            g.fadeDir = -1;
            pushToast("The ferryman poles you back to the dock.");
            playSoundIf(g.assets.door);
        }
    }
}

void sailInteract() {
    int tx = (int)floorf(gSailX), ty = (int)floorf(gSailY);
    // Against a beach: land.
    int lx = 0, ly = 0;
    if (landingNear(tx, ty, 2, lx, ly)) { sailExit(); return; }
    // Against a Tidewell isle's shore: the well is a step inland, so landing is
    // the whole interaction. Nothing else can happen out on the water.
    pushToast("Deep water. Find a beach or a dock.");
}

bool sailPrompt(const char** text) {
    int tx = (int)floorf(gSailX), ty = (int)floorf(gSailY);
    int lx = 0, ly = 0;
    if (landingNear(tx, ty, 2, lx, ly)) {
        static char buf[96];
        int isle = isleAtWell(lx, ly);
        if (isle >= 0) snprintf(buf, sizeof(buf), "SPACE: land on %s", tideIslandName(isle));
        else {
            Dock d;
            if (dockFind(true, d) && abs(d.tx - lx) < 3 && abs(d.ty - ly) < 3)
                snprintf(buf, sizeof(buf), "SPACE: tie up at the dock");
            else snprintf(buf, sizeof(buf), "SPACE: step ashore");
        }
        *text = buf;
        return true;
    }
    *text = "F: hail the ferry";
    return true;
}

// ================================================================ drawing
// The water is drawn as four animated bands of the same water tiles the
// overworld uses, plus a surf line on every shore — no new tile art, no new
// renderer pass, and it matches the pond the party just stepped off.
static void drawSailWater(int x0, int y0, int x1, int y1) {
    const int anim = (int)(g.surfT * 2.5f) % 3;
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            GroundTile t = sampleGround(x, y);
            int sx = x * TILE - (int)g.camX;
            int sy = y * TILE - (int)g.camY;
            bool wet = (t.kind == G_WATER || t.kind == G_DEEP);
            if (wet) {
                // Row order shared with Surface in render.cpp: deep, water,
                // beach, meadow... Three animation phases per surface, and the
                // shallow row's phases are the same autotile the pond uses on
                // foot, so stepping into the boat changes nothing about colour.
                stampTile(g.assets.landscape, 15, (t.kind == G_DEEP ? 0 : 1) * 3 + anim, sx, sy);
            } else {
                stampTile(g.assets.landscape, 15, (t.kind == G_BEACH ? 2 : 3) * 3 + (t.variant % 3), sx, sy);
            }
        }
    }
    // the far sea: a darker band past the coast so a sailor can read "wall"
    // from a boat without a line of text. It is drawn, not a tile.
    {
        float sr = shoreRadius();
        for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
            float d = sqrtf((float)x * x + (float)y * y);
            if (d < sr + 2.0f) continue;
            if (sampleGround(x, y).kind != G_DEEP) continue;
            int sx = x * TILE - (int)g.camX, sy = y * TILE - (int)g.camY;
            DrawRectangle(sx, sy, TILE, TILE, Fade(Color{ 10, 24, 52, 255 }, 0.30f));
        }
    }
    // the surf: foam on every wet/land seam, animated so a shore breathes
    static const int DX4[4] = { 1, -1, 0, 0 }, DY4[4] = { 0, 0, 1, -1 };
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            GroundTile t = sampleGround(x, y);
            if (t.kind != G_WATER && t.kind != G_DEEP) continue;
            int sx = x * TILE - (int)g.camX;
            int sy = y * TILE - (int)g.camY;
            for (int k = 0; k < 4; k++) {
                GroundKind nb = sampleGround(x + DX4[k], y + DY4[k]).kind;
                if (nb == G_WATER || nb == G_DEEP) continue;
                float ph = g.surfT * 1.9f + (float)(x * 3 + y * 5) * 0.7f;
                float pulse = 0.40f + 0.30f * sinf(ph);
                Color foam = Fade(Color{ 236, 250, 252, 255 }, pulse);
                int fx = sx + 8 + DX4[k] * 7;
                int fy = sy + 8 + DY4[k] * 7;
                DrawRectangle(fx - 7, fy - 2, 14, 4, foam);
                if (k < 2) DrawRectangle(fx - 4, fy + 2, 8, 2, Fade(foam, 0.5f));
            }
        }
    }
}

void sailDraw() {
    int x0 = (int)floorf(g.camX / TILE) - 1;
    int y0 = (int)floorf(g.camY / TILE) - 1;
    int x1 = x0 + g.vw / TILE + 3;
    int y1 = y0 + g.vh / TILE + 3;
    drawSailWater(x0, y0, x1, y1);

    // the hull's wake, oldest first so it sits under the boat
    for (int i = 0; i < wakeN; i++) {
        float a = std::max(0.0f, 0.5f - wakeAge[i] * 0.35f);
        if (a <= 0.0f) continue;
        int wx = (int)(wake[i][0] * TILE - g.camX);
        int wy = (int)(wake[i][1] * TILE - g.camY);
        DrawRectangle(wx - 4, wy - 1, 8, 3, Fade(Color{ 226, 244, 250, 255 }, a));
        DrawRectangle(wx - 2, wy - 3, 3, 6, Fade(Color{ 226, 244, 250, 255 }, a * 0.6f));
    }

    // the boat: row 0 bow-up, 1 bow-down, 2 bow-right; 3/4 view from the side
    if (g.assets.boat.id) {
        int row = (g.facing == 0) ? 1 : (g.facing == 1 ? 0 : 2);
        int frame = sailMoving ? (1 + ((int)(sailAnimT * 6.0f) & 1)) : 0;
        float cx = gSailX * TILE - g.camX;
        float fy = (gSailY + 0.35f) * TILE - g.camY + sailBob * 0.5f;
        Rectangle src{ (float)(frame * 24), (float)(row * 32), 24.0f, 32.0f };
        float fw = (g.facing == 2 && g.sideFlip) ? -24.0f : 24.0f;
        Rectangle dst{ cx, fy, 24.0f, 32.0f };
        DrawTexturePro(g.assets.boat, { src.x + (fw < 0 ? 24.0f : 0.0f), src.y, fw, 32.0f }, dst,
                       { 12.0f, 30.0f }, 0.0f, WHITE);
        // oar-wash under the hull: a small, always-moving foam patch
        DrawEllipse((int)cx, (int)(fy + 3), 10.0f, 3.0f,
                    Fade(Color{ 232, 248, 252, 255 }, 0.30f + 0.10f * sinf(g.surfT * 3.0f)));
    }

    // the party seen from above the gunwale: three heads, so the boat is theirs
    for (int i = 0; i < 3; i++) {
        Texture2D& sheet = g.assets.heroWalk[i];
        if (!sheet.id) continue;
        float ox = (float)(i - 1) * 5.0f;
        float cx = gSailX * TILE - g.camX + ox;
        float fy = (gSailY + 0.30f) * TILE - g.camY + sailBob * 0.5f;
        Rectangle src{ 24.0f, 0.0f, 24.0f, 32.0f };
        DrawTexturePro(sheet, src, { cx, fy, 24.0f, 32.0f }, { 12.0f, 30.0f }, 0.0f,
                       Fade(WHITE, 0.92f));
    }

    // a compass ribbon: sea does not give you landmarks, so say which way home is
    {
        float hx = -gSailX, hy = -gSailY;
        float len = sqrtf(hx * hx + hy * hy);
        if (len > 1.0f) { hx /= len; hy /= len; }
        float cx = (float)g.vw * 0.5f + hx * 54.0f;
        float cy = (float)g.vh * 0.5f + hy * 42.0f;
        DrawCircleGradient((int)cx, (int)cy, 20.0f, Fade(Color{ 255, 232, 170, 255 }, 0.10f), BLANK);
        DrawCircle((int)cx, (int)cy, 4, Fade(Color{ 255, 226, 150, 255 }, 0.55f));
        DrawCircleLines((int)cx, (int)cy, 7, Fade(Color{ 255, 226, 150, 255 }, 0.30f));
    }
}

void sailHud() {
    // A bow placard: the water's name and the number of wells still asleep.
    const char* name = sailZoneName();
    float w = measureText(name, 8).x;
    drawPixelPanel({ 8, 8, w + 20, 22 }, Color{ 16, 30, 44, 235 }, Color{ 98, 140, 164, 255 });
    drawText(name, 18, 14, Color{ 210, 232, 240, 255 }, 8);
    char wells[64];
    snprintf(wells, sizeof(wells), "Tidewells %d/%d", g.tideFound, TIDE_ISLANDS);
    float ww = measureText(wells, 8).x;
    drawPixelPanel({ g.vw - ww - 28.0f, 8, ww + 20, 22 },
                   Color{ 16, 30, 44, 235 }, Color{ 98, 140, 164, 255 });
    drawText(wells, g.vw - ww - 18.0f, 14,
             g.tideFound >= TIDE_ISLANDS ? Color{ 255, 226, 150, 255 } : Color{ 210, 232, 240, 255 }, 8);
    // the landing prompt along the bottom
    const char* prompt = nullptr;
    if (sailPrompt(&prompt) && prompt) {
        float pw = measureText(prompt, 8).x;
        drawPixelPanel({ (g.vw - pw) * 0.5f - 10, (float)g.vh - 30, pw + 20, 20 },
                       Color{ 16, 30, 44, 235 }, Color{ 98, 140, 164, 255 });
        drawText(prompt, (g.vw - pw) * 0.5f, (float)g.vh - 24, Color{ 214, 236, 244, 255 }, 8);
    }
}
