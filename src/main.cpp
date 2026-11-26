// main.cpp — "A Place for Agents" — an endless JRPG
#include "game.hpp"
#include "world.hpp"
#include "battle.hpp"
#include <ctime>
#include <cstdio>
#include <cstring>
#include <algorithm>

// ------------------------------------------------------------ virtual mouse
bool gForceQuit = false;
static Vector2 g_vmouse{ 0, 0 };
Vector2 getVirtualMouse() { return g_vmouse; }

// ------------------------------------------------------------ layout
static float gScale = 1.0f, gOffX = 0.0f, gOffY = 0.0f;
static void computeLayout() {
    int w = GetScreenWidth(), h = GetScreenHeight();
    gScale = std::min(w / (float)VRES_W, h / (float)VRES_H);
    gOffX = (w - VRES_W * gScale) / 2.0f;
    gOffY = (h - VRES_H * gScale) / 2.0f;
}

static Color lerpCol(Color a, Color b, float t) {
    return { (unsigned char)(a.r + (b.r - a.r) * t), (unsigned char)(a.g + (b.g - a.g) * t),
             (unsigned char)(a.b + (b.b - a.b) * t), (unsigned char)(a.a + (b.a - a.a) * t) };
}

// ------------------------------------------------------------ wild objects (shared render/collision)
enum WildKind { WK_NONE = 0, WK_TREE0, WK_TREE1, WK_TREE2, WK_PINE, WK_DEADTREE, WK_ROCK, WK_BUSH, WK_MUSH };
static WildKind wildObjectAt(int tx, int ty) {
    if (inSpawnSafeZone((float)tx, (float)ty)) return WK_NONE;
    if (villageContains((float)tx, (float)ty)) return WK_NONE;
    TileInfo t = sampleTile(tx, ty);
    uint32_t h = hash2i(tx, ty, g.worldSeed ^ 0x77ee5u);
    float hf = (float)h / 4294967295.0f;
    if (t.kind == TK_GRASS) {
        float m = moisture((float)tx, (float)ty, g.worldSeed);
        if (m > 0.55f && hf < 0.17f) {
            float v = hashf(tx, ty, g.worldSeed ^ 0xf00du);
            if (v < 0.12f) return WK_PINE;
            return (v < 0.40f) ? WK_TREE0 : (v < 0.72f ? WK_TREE1 : WK_TREE2);
        }
        if (hf > 0.965f) return WK_MUSH;
        if (hf > 0.935f) return WK_ROCK;
        if (hf > 0.905f) return WK_BUSH;
        return WK_NONE;
    }
    if (t.kind == TK_SNOW) {
        if (hf < 0.17f) return WK_PINE;
        if (hf > 0.965f) return WK_ROCK;
        return WK_NONE;
    }
    if (t.kind == TK_SAND) {
        if (hf < 0.02f) return WK_DEADTREE;
        return WK_NONE;
    }
    return WK_NONE;
}
static bool wildBlocks(int tx, int ty) {
    WildKind w = wildObjectAt(tx, ty);
    return w == WK_TREE0 || w == WK_TREE1 || w == WK_TREE2 || w == WK_PINE || w == WK_DEADTREE || w == WK_ROCK;
}

// ------------------------------------------------------------ camera
static float camX = 0, camY = 0;

// =============================================================== MUSIC
static void applyMusicVolume() {
    SetMusicVolume(g.assets.title,    g.musicOn ? 0.55f : 0.0f);
    SetMusicVolume(g.assets.field,    g.musicOn ? 0.50f : 0.0f);
    SetMusicVolume(g.assets.battle,   g.musicOn ? 0.55f : 0.0f);
    SetMusicVolume(g.assets.victory,  g.musicOn ? 0.70f : 0.0f);
    SetMusicVolume(g.assets.gameover, g.musicOn ? 0.70f : 0.0f);
}

static void ensureMusic() {
    static Mode last = (Mode)-1;
    Mode want = g.mode;
    if (g.mode == MODE_BATTLE) { last = want; return; }  // battle.cpp manages its own
    if (want == MODE_MENU) want = MODE_OVERWORLD;
    if (want == MODE_SHOP || want == MODE_INN || want == MODE_DIALOG) want = MODE_OVERWORLD;
    if (want == MODE_GAMEOVER) { /* keep gameover jingle */ last = want; return; }
    if (last == want) return;
    last = want;
    StopMusicStream(g.assets.title);
    StopMusicStream(g.assets.field);
    if (!g.musicOn) return;
    if (want == MODE_TITLE) PlayMusicStream(g.assets.title);
    else PlayMusicStream(g.assets.field);
}
static void updateMusicStreams() {
    UpdateMusicStream(g.assets.title);
    UpdateMusicStream(g.assets.field);
    UpdateMusicStream(g.assets.battle);
    UpdateMusicStream(g.assets.victory);
    UpdateMusicStream(g.assets.gameover);
}

// =============================================================== OVERWORLD UPDATE
static int s_lastTileX = 99999, s_lastTileY = 99999;

// NOTE: collision lives in world.cpp (positionBlocked / cellWalkable) so that
// the renderer, the pathfinder and the walk code all agree on what blocks.
static bool circleBlockedAt(float x, float y, float r) { return positionBlocked(x, y, r); }

static void tryEncounter(int tx, int ty) {
    if (g.encounterGrace > 0) return;
    TileInfo t = sampleTile(tx, ty);
    if (t.kind != TK_GRASS) return;
    if (villageContains((float)tx, (float)ty)) return;
    if (inSpawnSafeZone((float)tx, (float)ty)) return;
    float roll = hashf(tx * 31 + g.stepCounter, ty * 17 - g.stepCounter, g.worldSeed ^ 0xe4c0a5u);
    if (roll > 0.055f) return;   // ~1 encounter per 18 tiles
    // formation
    int avgLv = (g.party[0].level + g.party[1].level + g.party[2].level) / 3;
    int zl = zoneLevelAt((float)tx, (float)ty);
    float r = hashf(g.stepCounter, (int)(gAnimTime * 997.0f) & 0xffff, g.worldSeed ^ 0xbadd1eu);
    int n = 1;
    if (avgLv >= 2) n = (r < 0.4f) ? 1 : 2;
    if (avgLv >= 5) n = (r < 0.25f) ? 2 : 3;
    std::vector<EnemyKind> kinds;
    for (int i = 0; i < n; i++) {
        int ox = (int)(hashf(i, g.stepCounter, g.worldSeed) * 40.0f) - 20;
        int oy = (int)(hashf(i, g.stepCounter + 7, g.worldSeed) * 40.0f) - 20;
        EnemyKind k = rollBiomeEnemy(tx + ox, ty + oy);
        if (k == EN_GOLEM && zl < 4) k = EN_SLIME;
        kinds.push_back(k);
    }
    if (kinds.empty()) kinds.push_back(EN_SLIME);
    battleStart(kinds, zl);
}

static void interactWithVillage() {
    int tx = (int)floorf(g.px), ty = (int)floorf(g.py);
    int role = 0; Village v;
    if (!villageInteractAt(tx, ty, &role, &v)) {
        // also check one tile below door facing south
        if (!villageInteractAt(tx, ty, &role, &v)) return;
    }
    PlaySound(g.assets.door);
    if (role == 1) { g.mode = MODE_SHOP; }
    else if (role == 2) { g.mode = MODE_INN; }
    else {
        g.mode = MODE_DIALOG;
        g.dialogGift = (hashf(tx, ty, g.worldSeed ^ 0xace) > 0.72f);
        if (g.dialogGift) g.inventory[IT_POTION]++;
    }
}

// ---------------------------------------------------------------- mouse command
// A single left click means "go there". The click is turned into a walked path
// (A* in world.cpp), so trees, river banks and houses are routed around instead
// of silently swallowing the input. Clicking a house door walks there and opens
// it. Clicking again anywhere interrupts; a second click while walking just
// re-targets.
static void cancelWalk() {
    g.hasMoveTarget = false;
    g.walkQueue.clear();
    g.walkDoorHouse = -1;
}

static void commandWalkTo(int tileX, int tileY) {
    WalkPath nav;
    if (!findWalkPath(g.px, g.py, tileX, tileY, nav)) return;
    g.walkQueue = nav.cells;
    g.walkDoorHouse = nav.doorHouse;
    g.hasMoveTarget = true;
    if (!g.walkQueue.empty()) {
        g.moveTargetX = g.walkQueue.back().first + 0.5f;
        g.moveTargetY = g.walkQueue.back().second + 0.5f;
    } else {
        g.moveTargetX = tileX + 0.5f;
        g.moveTargetY = tileY + 0.5f;
    }
}

static void onWalkArrived() {
    int door = g.walkDoorHouse;
    cancelWalk();
    if (door >= 0 && door < (int)g.village.houses.size()) {
        const HouseInst& h = g.village.houses[door];
        // stand in front of the door and interact exactly as if the player
        // had pressed the action key there
        g.px = h.tx + 0.5f;
        g.py = h.ty + 1.5f;
        g.facing = 1;                 // face up at the door
        interactWithVillage();
    }
}

// returns the axis-aligned direction to step this frame toward the next
// queued tile; false when the queue is exhausted or unreachable.
static bool stepAlongPath(float dt, float& outDx, float& outDy) {
    while (!g.walkQueue.empty()) {
        int cx = (int)floorf(g.px), cy = (int)floorf(g.py);
        auto& next = g.walkQueue.front();
        if (cx == next.first && cy == next.second) { g.walkQueue.erase(g.walkQueue.begin()); continue; }
        float wx = next.first + 0.5f, wy = next.second + 0.5f;
        float dx = wx - g.px, dy = wy - g.py;
        float len = sqrtf(dx * dx + dy * dy);
        if (len < 1e-4f) { g.walkQueue.erase(g.walkQueue.begin()); continue; }
        outDx = dx / len; outDy = dy / len;
        return true;
    }
    return false;
}

// Interaction prompt for the tile the hero could act on right now, either the
// tile they stand on or the one directly in front of them.
static bool nearbyInteract(int* outRole, int* outHouse) {
    int tx = (int)floorf(g.px + 0.5f), ty = (int)floorf(g.py + 0.5f);
    if (g.villageLoaded) {
        for (int i = 0; i < (int)g.village.houses.size(); i++) {
            const HouseInst& h = g.village.houses[i];
            if (tx == h.tx && (ty == h.ty + 1 || ty == h.ty)) {
                if (outRole) *outRole = h.role;
                if (outHouse) *outHouse = i;
                return true;
            }
        }
    }
    return false;
}

static void updateOverworld(float dt) {
    if (g.encounterGrace > 0) g.encounterGrace -= dt;
    if (g.toastT > 0) g.toastT -= dt;
    if (g.clickFlashT > 0) g.clickFlashT -= dt;
    if (g.blockedFx > 0) g.blockedFx -= dt;
    g.runTime += dt;

    // village proximity bookkeeping
    villageLoadFor(g.px, g.py);

    float dx = 0, dy = 0;
    bool keyboardMove = false;
    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))    { dy -= 1; keyboardMove = true; }
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))  { dy += 1; keyboardMove = true; }
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  { dx -= 1; keyboardMove = true; }
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) { dx += 1; keyboardMove = true; }
    if (keyboardMove) cancelWalk();

    // ---- mouse: click to walk (left button), anywhere in the world view
    if (!keyboardMove && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        Vector2 m = getVirtualMouse();
        int tx = (int)floorf((camX + m.x) / TILE);
        int ty = (int)floorf((camY + m.y) / TILE);
        g.clickFlashX = tx + 0.5f; g.clickFlashY = ty + 0.5f; g.clickFlashT = 0.45f;
        commandWalkTo(tx, ty);
    }

    if (!keyboardMove && g.hasMoveTarget) {
        float px = 0, py = 0;
        if (!stepAlongPath(dt, px, py)) {
            onWalkArrived();
        } else {
            dx = px; dy = py;
        }
    }

    float len = sqrtf(dx * dx + dy * dy);
    g.moving = false;
    if (len > 0.001f) {
        dx /= len; dy /= len;
        float speed = 4.3f;
        float nx = g.px + dx * speed * dt;
        float ny = g.py + dy * speed * dt;
        bool slid = false;
        if (!circleBlockedAt(nx, g.py, 0.26f)) { g.px = nx; slid = true; }
        if (!circleBlockedAt(g.px, ny, 0.26f)) { g.py = ny; slid = true; }
        if (slid) {
            g.moving = true;
            g.walkAnimT += dt * 9.0f;
        } else {
            // genuinely stuck: stop pushing and drop the click-to-walk goal so
            // the hero does not grind against a rock forever
            if (g.hasMoveTarget) {
                cancelWalk();
                g.blockedFx = 0.35f;
                PlaySound(g.assets.cancel);
            }
        }
        if (fabsf(dx) > fabsf(dy)) { g.facing = 2; g.sideFlip = dx < 0; }
        else if (dy < 0) { g.facing = 1; }
        else { g.facing = 0; }
    }

    // tile-change bookkeeping → encounters
    int tx = (int)floorf(g.px), ty = (int)floorf(g.py);
    if (tx != s_lastTileX || ty != s_lastTileY) {
        if (s_lastTileX != 99999) { g.stepCounter++; tryEncounter(tx, ty); }
        s_lastTileX = tx; s_lastTileY = ty;
    }

    // field monsters — contact triggers battle
    std::vector<FieldMonster> mons;
    fieldMonstersInView(g.px, g.py, mons);
    for (const auto& fm : mons) {
        // monsters wander: wobble offset for display & contact
        float wob = sinf(gAnimTime * 0.9f + fm.phase);
        float ddx = fm.x + wob * 0.7f - g.px, ddy = fm.y + cosf(gAnimTime * 0.8f + fm.phase) * 0.7f - g.py;
        if (ddx * ddx + ddy * ddy < 0.72f * 0.72f) {
            int avgLv = (g.party[0].level + g.party[1].level + g.party[2].level) / 3;
            int zl = zoneLevelAt(fm.x, fm.y);
            std::vector<EnemyKind> kinds = { fm.kind };
            if (avgLv >= 3 || zl >= 3) kinds.push_back((fm.kind == EN_WISP) ? EN_BAT : EN_SLIME);
            battleStart(kinds, zl, fm.id);
            return;
        }
    }

    // village interaction: keyboard, or a click on the open door in front of you
    if (IsKeyPressed(KEY_E) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) interactWithVillage();
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !keyboardMove) {
        int role, house;
        if (nearbyInteract(&role, &house)) {
            // the click we already handled above may have been the walk command;
            // if the hero is already at the door this opens it right away
            int tx = (int)floorf(g.px + 0.5f), ty = (int)floorf(g.py + 0.5f);
            const HouseInst& h = g.village.houses[house];
            bool atDoor = (ty == h.ty + 1) && fabsf(g.px - (h.tx + 0.5f)) < 0.75f;
            if (atDoor) { cancelWalk(); interactWithVillage(); }
            else { (void)tx; }
        }
    }

    if (IsKeyPressed(KEY_ESCAPE)) { g.mode = MODE_MENU; g.menuIndex = 0; PlaySound(g.assets.confirm); }

    // camera + shake
    camX = g.px * TILE - VRES_W / 2.0f;
    camY = g.py * TILE - VRES_H / 2.0f;
    if (g.shakeT > 0) {
        g.shakeT -= dt;
        camX += ((hashf((int)(gAnimTime * 1000), 1, 99) - 0.5f) * 2.0f) * g.shakeMag;
        camY += ((hashf((int)(gAnimTime * 1000), 2, 99) - 0.5f) * 2.0f) * g.shakeMag;
    }
}

// =============================================================== OVERWORLD DRAW
static void drawShadow(float x, float y, float w, float h) {
    // two-tone contact shadow (soft outer + darker core), SNES style
    DrawEllipse((int)x, (int)y, (int)(w * 1.15f), (int)(h * 1.2f), Fade(Color{ 26, 22, 44, 255 }, 0.26f));
    DrawEllipse((int)x, (int)y, (int)w, (int)h, Fade(Color{ 22, 18, 38, 255 }, 0.34f));
}

static void drawWildObject(WildKind kind, float wx, float wy, int variantSeed) {
    switch (kind) {
        case WK_TREE0: DrawTexture(g.assets.tree[0], (int)wx, (int)wy, WHITE); break;
        case WK_TREE1: DrawTexture(g.assets.tree[1], (int)wx, (int)wy, WHITE); break;
        case WK_TREE2: DrawTexture(g.assets.tree[2], (int)wx, (int)wy, WHITE); break;
        case WK_PINE:  DrawTexture(g.assets.pine[variantSeed % 3], (int)wx, (int)wy, WHITE); break;
        case WK_DEADTREE: DrawTexture(g.assets.deadTree, (int)wx, (int)wy, WHITE); break;
        case WK_ROCK:  DrawTexture(g.assets.rock[variantSeed % 3], (int)wx, (int)wy, WHITE); break;
        case WK_BUSH:  DrawTexture(g.assets.bush, (int)wx, (int)wy, WHITE); break;
        case WK_MUSH:  DrawTexture(g.assets.mushroom, (int)wx, (int)wy, WHITE); break;
        default: break;
    }
}

static void drawVillageOverlay() {
    if (!g.villageLoaded) return;
    const Village& v = g.village;
    int pcx = (int)floorf(v.cx), pcy = (int)floorf(v.cy);
    // dirt paths: plaza -> each doorstep (Bresenham), drawn under the plaza
    for (const HouseInst& h : v.houses) {
        int x0 = pcx, y0 = pcy, x1 = h.tx, y1 = h.ty + 1;
        int dx = (x1 >= x0) ? (x1 - x0) : (x0 - x1), dy = (y1 >= y0) ? (y1 - y0) : (y0 - y1);
        int sx = (x0 < x1) ? 1 : -1, sy = (y0 < y1) ? 1 : -1;
        int err = dx - dy, x = x0, y = y0;
        for (int guard = 0; guard < 64; guard++) {
            float ddx = x - v.cx, ddy = y - v.cy;
            if (ddx * ddx + ddy * ddy > 3.1f * 3.1f) {
                TileInfo pt = sampleTile(x, y);
                if (pt.kind == TK_GRASS || pt.kind == TK_COBBLE)
                    DrawTexture(g.assets.dirt, (int)(x * TILE - camX), (int)(y * TILE - camY), WHITE);
            }
            if (x == x1 && y == y1) break;
            int e2 = 2 * err;
            if (e2 > -dy) { err -= dy; x += sx; }
            if (e2 < dx)  { err += dx; y += sy; }
        }
    }
    // cobble plaza
    for (int ty = pcy - 4; ty <= pcy + 4; ty++) {
        for (int tx = pcx - 4; tx <= pcx + 4; tx++) {
            float dx = tx - v.cx, dy = ty - v.cy;
            if (dx * dx + dy * dy > 3.1f * 3.1f) continue;
            DrawTexture(g.assets.cobble, (int)(tx * TILE - camX), (int)(ty * TILE - camY), WHITE);
        }
    }
}

struct SpriteItem { float y; int type; int idx; };
static void drawWorld() {
    int x0 = (int)floorf(camX / TILE) - 1, x1 = (int)floorf((camX + VRES_W) / TILE) + 1;
    int y0 = (int)floorf(camY / TILE) - 1, y1 = (int)floorf((camY + VRES_H) / TILE) + 1;
    int waterFrame = ((int)(gAnimTime * 0.8f)) & 1;

    // ground pass
    for (int ty = y0; ty <= y1; ty++) {
        for (int tx = x0; tx <= x1; tx++) {
            TileInfo t = sampleTile(tx, ty);
            Texture2D tex;
            switch (t.kind) {
                case TK_WATER: tex = g.assets.water[waterFrame]; break;
                case TK_ICE:   tex = g.assets.ice; break;
                case TK_SAND:  tex = g.assets.sand; break;
                case TK_SNOW:  tex = g.assets.snow; break;
                case TK_ROCK:  tex = g.assets.rockHill; break;
                default:       tex = g.assets.grass[t.variant]; break;
            }
            int sx = (int)(tx * TILE - camX), sy = (int)(ty * TILE - camY);
            DrawTexture(tex, sx, sy, WHITE);
            // decorative tuft/flower overlays
            if (t.deco == 1) DrawTexture(g.assets.tuft[t.decoVariant], sx, sy, WHITE);
            else if (t.deco == 2) DrawTexture(g.assets.flower[t.decoVariant], sx, sy, WHITE);
        }
    }
    drawVillageOverlay();

    // sprite pass, y-sorted
    std::vector<SpriteItem> items;
    items.reserve(64);
    // wild objects
    for (int ty = y0; ty <= y1; ty++) {
        for (int tx = x0; tx <= x1; tx++) {
            WildKind w = wildObjectAt(tx, ty);
            if (w == WK_NONE) continue;
            items.push_back({ (float)(ty + 1) * TILE, 0, ty * 100000 + tx });
        }
    }
    // village props & buildings
    if (g.villageLoaded) {
        const Village& v = g.village;
        for (int i = 0; i < (int)v.houses.size(); i++) items.push_back({ (float)(v.houses[i].ty + 1) * TILE, 1, i });
        for (int i = 0; i < (int)v.wells.size(); i++)  items.push_back({ (float)(v.wells[i].second + 1) * TILE, 2, i });
        for (int i = 0; i < (int)v.signs.size(); i++)  items.push_back({ (float)(v.signs[i].second + 1) * TILE, 3, i });
        for (int i = 0; i < (int)v.fences.size(); i++) items.push_back({ (float)(v.fences[i].second + 1) * TILE, 4, i });
        for (int i = 0; i < (int)v.trees.size(); i++)  items.push_back({ (float)(v.trees[i].second + 1) * TILE, 5, i });
        for (int i = 0; i < (int)v.bushes.size(); i++) items.push_back({ (float)(v.bushes[i].second + 1) * TILE, 6, i });
        for (int i = 0; i < (int)v.mushrooms.size(); i++) items.push_back({ (float)(v.mushrooms[i].second + 1) * TILE, 7, i });
    }
    // field monsters
    std::vector<FieldMonster> mons;
    fieldMonstersInView(g.px, g.py, mons);
    for (int i = 0; i < (int)mons.size(); i++) {
        FieldMonster& fm = mons[i];
        fm.x += sinf(gAnimTime * 0.9f + fm.phase) * 0.7f;
        fm.y += cosf(gAnimTime * 0.8f + fm.phase) * 0.7f;
        items.push_back({ fm.y * TILE, 8, i });
    }
    // hero
    items.push_back({ g.py * TILE, 9, 0 });

    std::sort(items.begin(), items.end(), [](const SpriteItem& a, const SpriteItem& b) { return a.y < b.y; });

    for (const auto& it : items) {
        switch (it.type) {
            case 0: {
                int tx = it.idx % 100000, ty = it.idx / 100000;
                WildKind w = wildObjectAt(tx, ty);
                int vs = (int)(hash2i(tx, ty, g.worldSeed ^ 0xf00du) % 3);
                float feetX = tx * TILE - camX + TILE / 2.0f, feetY = (ty + 1) * TILE - camY;
                if (w == WK_TREE0 || w == WK_TREE1 || w == WK_TREE2 || w == WK_PINE || w == WK_DEADTREE)
                    drawShadow(feetX, feetY - 1, 12, 3);
                Texture2D* tex = nullptr;
                int tw = 0, th = 0;
                switch (w) {
                    case WK_TREE0: tex = &g.assets.tree[0]; break;
                    case WK_TREE1: tex = &g.assets.tree[1]; break;
                    case WK_TREE2: tex = &g.assets.tree[2]; break;
                    case WK_PINE:  tex = &g.assets.pine[vs]; break;
                    case WK_DEADTREE: tex = &g.assets.deadTree; break;
                    case WK_ROCK:  tex = &g.assets.rock[vs]; break;
                    case WK_BUSH:  tex = &g.assets.bush; break;
                    case WK_MUSH:  tex = &g.assets.mushroom; break;
                    default: break;
                }
                if (tex) {
                    tw = tex->width; th = tex->height;
                    DrawTexture(*tex, (int)(feetX - tw / 2), (int)(feetY - th), WHITE);
                }
                break;
            }
            case 1: {
                const HouseInst& h = g.village.houses[it.idx];
                Texture2D* tex = &g.assets.house[h.variant < 0 ? 0 : h.variant];
                if (h.variant == -1) tex = &g.assets.shop;
                else if (h.variant == -2) tex = &g.assets.inn;
                float feetX = h.tx * TILE - camX + TILE / 2.0f, feetY = (h.ty + 1) * TILE - camY;
                DrawTexture(*tex, (int)(feetX - tex->width / 2), (int)(feetY - tex->height), WHITE);
                break;
            }
            case 2: {
                const auto& p = g.village.wells[it.idx];
                Texture2D& t = g.assets.well;
                drawShadow(p.first * TILE - camX + 8, (p.second + 1) * TILE - camY, 12, 3);
                DrawTexture(t, (int)(p.first * TILE - camX + 8 - t.width / 2), (int)((p.second + 1) * TILE - camY - t.height), WHITE);
                break;
            }
            case 3: {
                const auto& p = g.village.signs[it.idx];
                Texture2D& t = g.assets.sign;
                DrawTexture(t, (int)(p.first * TILE - camX + 8 - t.width / 2), (int)((p.second + 1) * TILE - camY - t.height), WHITE);
                break;
            }
            case 4: {
                const auto& p = g.village.fences[it.idx];
                DrawTexture(g.assets.fence, (int)(p.first * TILE - camX), (int)((p.second + 1) * TILE - camY - 10), WHITE);
                break;
            }
            case 5: {
                const auto& p = g.village.trees[it.idx];
                int v = (int)(hash2i(p.first, p.second, g.worldSeed ^ 0xbeU) % 3);
                float feetX = p.first * TILE - camX + 8, feetY = (p.second + 1) * TILE - camY;
                drawShadow(feetX, feetY, 12, 3);
                DrawTexture(g.assets.tree[v], (int)(feetX - g.assets.tree[v].width / 2), (int)(feetY - g.assets.tree[v].height), WHITE);
                break;
            }
            case 6: {
                const auto& p = g.village.bushes[it.idx];
                DrawTexture(g.assets.bush, (int)(p.first * TILE - camX), (int)((p.second + 1) * TILE - camY - 12), WHITE);
                break;
            }
            case 7: {
                const auto& p = g.village.mushrooms[it.idx];
                DrawTexture(g.assets.mushroom, (int)(p.first * TILE - camX + 2), (int)((p.second + 1) * TILE - camY - 10), WHITE);
                break;
            }
            case 8: {
                FieldMonster& fm = mons[it.idx];
                Texture2D& tex = g.assets.enemy[(int)fm.kind];
                float feetX = fm.x * TILE - camX, feetY = fm.y * TILE - camY;
                float sc = 15.0f / (float)std::max(tex.width, tex.height);
                float w = tex.width * sc, h = tex.height * sc;
                float bob = sinf(gAnimTime * 3.0f + fm.phase) * 1.5f;
                drawShadow(feetX, feetY + 1, w * 0.5f, 3);
                DrawTexturePro(tex, { 0, 0, (float)tex.width, (float)tex.height },
                               { feetX - w / 2, feetY - h + bob, w, h }, { 0, 0 }, 0.0f, WHITE);
                break;
            }
            case 9: {
                // hero
                Texture2D& sheet = g.assets.heroWalk[g.party[0].klass];
                int row = (g.facing == 0) ? 0 : (g.facing == 1 ? 1 : 2);
                int frame = 0;
                if (g.moving) {
                    float cyc = fmodf(g.walkAnimT, 3.14159f * 2.0f);
                    frame = (cyc < 1.05f) ? 1 : ((cyc < 2.1f) ? 2 : 0);
                }
                Rectangle src = { (float)frame * 24, (float)row * 32, 24, 32 };
                float feetX = g.px * TILE - camX, feetY = g.py * TILE - camY;
                drawShadow(feetX, feetY + 1, 7, 2.5f);
                Rectangle dst = { feetX - 12, feetY - 32, 24, 32 };
                // OpenRTP frames are 24x32 at native world scale.
                Vector2 origin = { 0, 0 };
                if (g.facing == 2 && g.sideFlip) {
                    src.x = src.x + 24.0f; src.width = -24.0f;
                }
                DrawTexturePro(sheet, src, dst, origin, 0.0f, WHITE);
                break;
            }
            default: break;
        }
    }

    // clouds
    for (int i = 0; i < 6; i++) {
        float cx = fmodf(g.cloudX[i] + gAnimTime * (4.0f + i * 1.3f), VRES_W + 200.0f) - 100.0f;
        float sc = g.cloudScale[i];
        DrawTexturePro(g.assets.cloud, { 0, 0, (float)g.assets.cloud.width, (float)g.assets.cloud.height },
                       { cx, g.cloudY[i], 96 * sc, 40 * sc }, { 0, 0 }, 0.0f, Fade(WHITE, 0.82f));
    }

    // day/night tint. SNES-era games fade dusk with a dither pattern rather
    // than a smooth wash, so the extra darkness is drawn as a checkerboard.
    float cycle = fmodf((float)g.runTime / 85.0f, 1.0f);
    float night = 0.42f * (0.55f - 0.55f * cosf(cycle * 6.2831853f));
    if (night > 0.02f) {
        Color dusk{ 14, 18, 58, 255 };
        DrawRectangle(0, 0, VRES_W, VRES_H, Fade(dusk, night * 0.55f));
        for (int y = 0; y < VRES_H; y += 2) {
            for (int x = ((y / 2) & 1) ? 1 : 0; x < VRES_W; x += 2) {
                DrawRectangle(x, y, 1, 1, Fade(dusk, night * 0.5f));
            }
        }
    }
}

static void drawMiniBar(float x, float y, float w, float h, float frac, Color col) {
    DrawRectangle((int)x, (int)y, (int)w, (int)h, Color{12, 12, 20, 220});
    DrawRectangle((int)x, (int)y, (int)(w * std::max(0.0f, std::min(1.0f, frac))), (int)h, col);
}

// =============================================================== HUD
static void drawHud() {
    // zone name (top-left)
    const char* zone = villageContains(g.px, g.py) && g.villageLoaded ? g.village.name.c_str() : zoneNameAt(g.px, g.py);
    DrawTextEx(g.assets.font8, zone, { 8, 6 }, 8, 1, Fade(Color{255, 240, 200, 255}, 0.92f));
    // gil (top-right)
    std::string gil = formatGil(g.gil);
    Vector2 ts = MeasureTextEx(g.assets.font8, gil.c_str(), 8, 1);
    DrawTexture(g.assets.coinIcon, VRES_W - (int)ts.x - 26, 4, WHITE);
    DrawTextEx(g.assets.font8, gil.c_str(), { VRES_W - ts.x - 6, 6 }, 8, 1, Color{255, 224, 120, 255});
    // party chips (bottom-left)
    for (int i = 0; i < 3; i++) {
        Hero& h = g.party[i];
        float y = VRES_H - 3 - (3 - i) * 21.0f;
        DrawRectangleRounded({ 4, y, 118, 19 }, 0.3f, 4, Fade(Color{14, 18, 44, 255}, 0.78f));
        Texture2D& face = g.assets.face[h.klass];
        DrawTexturePro(face, { 0, 0, 32, 32 }, { 6, y + 1, 17, 17 }, { 0, 0 }, 0.0f, h.ko ? Color{130,130,150,255} : WHITE);
        float bx = 26, bw = 52;
        drawMiniBar(bx, y + 3, bw, 4, (float)h.hp / h.maxHp, Color{224, 80, 70, 255});
        drawMiniBar(bx, y + 9, bw, 3, (float)h.mp / h.maxMp, Color{70, 140, 230, 255});
        if (h.hp * 4 < h.maxHp && !h.ko) {
            float pulse = 0.6f + 0.4f * sinf(gAnimTime * 8.0f);
            DrawTextEx(g.assets.font8, "!", { bx + bw + 3, y + 2 }, 8, 1, Fade(Color{255, 90, 70, 255}, pulse));
        }
    }
    // interact prompt
    int role = 0, house = -1;
    if (nearbyInteract(&role, &house)) {
        const char* what = role == 1 ? "enter the shop" : (role == 2 ? "enter the inn" : "enter the house");
        char hint[80]; snprintf(hint, sizeof(hint), "click the door or E - %s", what);
        Vector2 hts = MeasureTextEx(g.assets.font8, hint, 8, 1);
        Rectangle r = { VRES_W / 2.0f - hts.x / 2 - 8, (float)VRES_H - 32, hts.x + 16, 14 };
        drawPanel(r);
        DrawTextEx(g.assets.font8, hint, { r.x + 8, r.y + 3 }, 8, 1, WHITE);
    }
    // click marker: a small ring where the player clicked, and a nudge arrow
    // while walking, so the mouse command is visibly acknowledged
    if (g.clickFlashT > 0) {
        float t = 1.0f - g.clickFlashT / 0.45f;
        float sx = g.clickFlashX * TILE - camX, sy = g.clickFlashY * TILE - camY;
        float rr = 3.0f + t * 7.0f;
        DrawRing({ sx, sy }, rr, rr + 1.4f, 0, 360, 24, Fade(Color{255, 240, 180, 255}, 1.0f - t));
        DrawCircleV({ sx, sy }, 1.6f, Fade(Color{255, 250, 220, 255}, 1.0f - t));
    }
    if (g.hasMoveTarget) {
        int tx = (int)floorf(g.px), ty = (int)floorf(g.py);
        float sx = tx * TILE - camX + 8.0f, sy = ty * TILE - camY + 2.0f;
        DrawCircleLines((int)sx, (int)sy, 6.0f + 1.2f * sinf(gAnimTime * 6.0f), Fade(Color{255, 255, 255, 120}, 0.9f));
    }
    // toast
    if (g.toastT > 0) {
        float a = std::min(1.0f, g.toastT / 0.4f);
        Vector2 tts = MeasureTextEx(g.assets.font8, g.toast.c_str(), 8, 1);
        Rectangle r = { VRES_W / 2.0f - tts.x / 2 - 8, 30, tts.x + 16, 14 };
        drawPanel(r);
        DrawTextEx(g.assets.font8, g.toast.c_str(), { r.x + 8, r.y + 3 }, 8, 1, Fade(Color{220, 255, 220, 255}, a));
    }
    // help line
    if (g.runTime < 22.0f) {
        const char* help = "click anywhere to walk   doors, shops & inns: click them   ESC menu";
        Vector2 hts = MeasureTextEx(g.assets.font8, help, 8, 1);
        DrawTextEx(g.assets.font8, help, { VRES_W / 2.0f - hts.x / 2, (float)VRES_H - 12 }, 8, 1, Fade(WHITE, 0.55f));
    }
}
// =============================================================== TITLE
static void updateTitle(float dt) {
    g.titleT += dt;
    int count = g.saveExists ? 3 : 2;
    if (IsKeyPressed(KEY_DOWN)) { g.titleIndex = (g.titleIndex + 1) % count; PlaySound(g.assets.cursor); }
    if (IsKeyPressed(KEY_UP))   { g.titleIndex = (g.titleIndex + count - 1) % count; PlaySound(g.assets.cursor); }
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        PlaySound(g.assets.confirm);
        if (g.titleIndex == 0) {
            uint32_t seed = (uint32_t)time(nullptr) * 2654435761u + (uint32_t)GetRandomValue(1, 99999);
            newGameState(seed);
            g.mode = MODE_OVERWORLD;
            g.fade = 1.0f; g.fadeDir = -1;
            pushToast("The wilds await...");
        } else if (g.titleIndex == 1 && g.saveExists) {
            if (loadGame()) {
                g.mode = MODE_OVERWORLD;
                g.fade = 1.0f; g.fadeDir = -1;
                pushToast("Welcome back, traveler.");
            }
        } else {
            gForceQuit = true;
        }
    }
}
static void drawTitle() {
    DrawTexture(g.assets.titleBg, 0, 0, WHITE);
    // drifting clouds
    for (int i = 0; i < 3; i++) {
        float cx = fmodf(g.titleT * (5.0f + i * 2.0f) + i * 170.0f, VRES_W + 240.0f) - 120.0f;
        DrawTexturePro(g.assets.cloud, { 0, 0, (float)g.assets.cloud.width, (float)g.assets.cloud.height },
                       { cx, 18.0f + i * 26.0f, 110, 46 }, { 0, 0 }, 0.0f, Fade(WHITE, 0.75f));
    }
    // bottom gradient for legibility
    for (int y = 0; y < 90; y++) {
        float a = (90 - y) / 90.0f * 0.62f;
        DrawRectangle(0, VRES_H - y, VRES_W, 1, Fade(Color{8, 8, 18, 255}, a));
    }
    float bob = sinf(g.titleT * 1.6f) * 2.0f;
    // logo
    const char* line1 = "A PLACE FOR";
    Vector2 ts1 = MeasureTextEx(g.assets.font16, line1, 16, 3);
    DrawTextEx(g.assets.font16, line1, { VRES_W / 2.0f - ts1.x / 2 + 1, 30 + bob + 1 }, 16, 3, Color{60, 30, 10, 255});
    DrawTextEx(g.assets.font16, line1, { VRES_W / 2.0f - ts1.x / 2, 30 + bob }, 16, 3, Color{255, 214, 130, 255});
    const char* line2 = "AGENTS";
    Vector2 ts2 = MeasureTextEx(g.assets.font32, line2, 32, 6);
    DrawTextEx(g.assets.font32, line2, { VRES_W / 2.0f - ts2.x / 2 + 3, 52 + bob + 3 }, 32, 6, Color{30, 12, 26, 255});
    DrawTextEx(g.assets.font32, line2, { VRES_W / 2.0f - ts2.x / 2, 52 + bob }, 32, 6, Color{248, 240, 255, 255});
    const char* tag = "an endless jrpg through the wilds";
    Vector2 ts3 = MeasureTextEx(g.assets.font8, tag, 8, 1);
    DrawTextEx(g.assets.font8, tag, { VRES_W / 2.0f - ts3.x / 2, 100 + bob }, 8, 1, Color{230, 210, 230, 255});
    // buttons
    float bw = 176, bh = 19, bx = VRES_W / 2.0f - bw / 2;
    float by = 150;
    int idx = 0;
    auto drawBtn = [&](const char* label) {
        Rectangle r = { bx, by, bw, bh };
        bool hover = CheckCollisionPointRec(getVirtualMouse(), r);
        bool sel = (g.titleIndex == idx) || hover;
        if (hover) g.titleIndex = idx;
        DrawRectangleRounded(r, 0.22f, 5, sel ? Color{58, 50, 110, 235} : Color{22, 20, 48, 210});
        DrawRectangleRoundedLinesEx(r, 0.22f, 5, 1.5f, sel ? Color{255, 214, 130, 255} : Color{140, 130, 200, 255});
        Vector2 ls = MeasureTextEx(g.assets.font8, label, 8, 1);
        DrawTextEx(g.assets.font8, label, { r.x + r.width / 2 - ls.x / 2, r.y + 5 }, 8, 1, sel ? WHITE : Color{210, 205, 235, 255});
        by += 24;
        idx++;
        return r;
    };
    Rectangle rNew = drawBtn("New Adventure");
    Rectangle rCon = { 0, 0, 0, 0 };
    if (g.saveExists) rCon = drawBtn("Continue");
    Rectangle rQuit = drawBtn("Quit");
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        Vector2 m = getVirtualMouse();
        if (CheckCollisionPointRec(m, rNew)) { g.titleIndex = 0; PlaySound(g.assets.confirm); uint32_t seed = (uint32_t)time(nullptr) * 2654435761u + (uint32_t)GetRandomValue(1, 99999); newGameState(seed); g.mode = MODE_OVERWORLD; g.fade = 1.0f; g.fadeDir = -1; pushToast("The wilds await..."); }
        else if (g.saveExists && CheckCollisionPointRec(m, rCon)) { g.titleIndex = 1; if (loadGame()) { g.mode = MODE_OVERWORLD; g.fade = 1.0f; g.fadeDir = -1; pushToast("Welcome back, traveler."); } PlaySound(g.assets.confirm); }
        else if (CheckCollisionPointRec(m, rQuit)) { gForceQuit = true; }
    }
    // footer
    const char* foot = "mouse or keyboard  -  generated world, no two journeys alike";
    Vector2 fts = MeasureTextEx(g.assets.font8, foot, 8, 1);
    DrawTextEx(g.assets.font8, foot, { VRES_W / 2.0f - fts.x / 2, VRES_H - 12 }, 8, 1, Fade(WHITE, 0.5f));
}

// =============================================================== PAUSE MENU
static int s_itemsTargetMode = 0;   // 0 = list, 1 = pick hero
static int s_pendingItem = -1;

static const char* MENU_ITEMS[] = { "Resume", "Status", "Items", "Save", "Options", "Quit to Title" };
static const int MENU_COUNT = 6;

static void drawStatusPage(Rectangle r) {
    DrawTextEx(g.assets.font8, "STATUS", { r.x + 10, r.y + 8 }, 8, 1, Color{255, 214, 130, 255});
    for (int i = 0; i < 3; i++) {
        Hero& h = g.party[i];
        float y = r.y + 22 + i * 76;
        DrawRectangleRounded({ r.x + 8, y, r.width - 16, 70 }, 0.14f, 5, Color{28, 32, 74, 255});
        Texture2D& face = g.assets.face[h.klass];
        DrawTexturePro(face, { 0, 0, 32, 32 }, { r.x + 14, y + 6, 40, 40 }, { 0, 0 }, 0.0f, WHITE);
        float tx = r.x + 62;
        char buf[64];
        snprintf(buf, sizeof(buf), "%s  Lv %d", heroName(h.klass), h.level);
        DrawTextEx(g.assets.font8, buf, { tx, y + 4 }, 8, 1, WHITE);
        snprintf(buf, sizeof(buf), "HP %d/%d", h.hp, h.maxHp);
        DrawTextEx(g.assets.font8, buf, { tx, y + 16 }, 8, 1, Color{255, 200, 190, 255});
        snprintf(buf, sizeof(buf), "MP %d/%d", h.mp, h.maxMp);
        DrawTextEx(g.assets.font8, buf, { tx, y + 26 }, 8, 1, Color{180, 210, 255, 255});
        snprintf(buf, sizeof(buf), "ATK %d  DEF %d  SPD %d", heroAtk(h), heroDef(h), h.spd);
        DrawTextEx(g.assets.font8, buf, { tx, y + 36 }, 8, 1, Color{220, 226, 255, 255});
        const char* wpn = h.weapon >= 0 ? itemDef(h.weapon).name : "Bare hands";
        const char* arm = h.armor >= 0 ? itemDef(h.armor).name : "Clothes";
        snprintf(buf, sizeof(buf), "%s / %s", wpn, arm);
        DrawTextEx(g.assets.font8, buf, { tx, y + 48 }, 8, 1, Color{190, 200, 240, 255});
        snprintf(buf, sizeof(buf), "XP %d/%d", h.xp, xpToNext(h));
        DrawTextEx(g.assets.font8, buf, { tx, y + 58 }, 8, 1, Color{150, 160, 210, 255});
    }
}

static void updateItemsPage(Rectangle r, float dt) {
    if (s_itemsTargetMode == 1) {
        DrawTextEx(g.assets.font8, "Use on...", { r.x + 10, r.y + 8 }, 8, 1, Color{180, 255, 190, 255});
        for (int i = 0; i < 3; i++) {
            Hero& h = g.party[i];
            Rectangle row = { r.x + 8, r.y + 24 + i * 34, r.width - 16, 30 };
            bool hover = CheckCollisionPointRec(getVirtualMouse(), row);
            DrawRectangleRounded(row, 0.2f, 4, hover ? Color{48, 54, 110, 255} : Color{30, 34, 78, 255});
            Texture2D& face = g.assets.face[h.klass];
            DrawTexturePro(face, { 0, 0, 32, 32 }, { row.x + 4, row.y + 3, 24, 24 }, { 0, 0 }, 0.0f, WHITE);
            char buf[48]; snprintf(buf, sizeof(buf), "%s  %d/%d", heroName(h.klass), h.hp, h.maxHp);
            DrawTextEx(g.assets.font8, buf, { row.x + 34, row.y + 11 }, 8, 1, WHITE);
            if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (applyConsumable(s_pendingItem, i)) {
                    PlaySound(g.assets.potion);
                    pushToast("Used!");
                } else {
                    PlaySound(g.assets.cancel);
                    pushToast(h.ko ? "They're down... Elixir can revive." : "No effect.");
                }
                s_itemsTargetMode = 0;
            }
        }
        if (IsKeyPressed(KEY_ESCAPE)) s_itemsTargetMode = 0;
        return;
    }
    DrawTextEx(g.assets.font8, "ITEMS", { r.x + 10, r.y + 8 }, 8, 1, Color{180, 255, 190, 255});
    int row = 0;
    for (int id = 0; id < IT_COUNT; id++) {
        if (itemDef(id).kind != IK_CONSUMABLE) continue;
        if (g.inventory[id] <= 0) continue;
        Rectangle rr = { r.x + 8, r.y + 24 + row * 24, r.width - 16, 21 };
        bool hover = CheckCollisionPointRec(getVirtualMouse(), rr);
        DrawRectangleRounded(rr, 0.2f, 4, hover ? Color{48, 54, 110, 255} : Color{30, 34, 78, 255});
        DrawTexture(g.assets.icon[id], (int)rr.x + 2, (int)rr.y + 2, WHITE);
        char buf[48]; snprintf(buf, sizeof(buf), "%s x%d", itemDef(id).name, g.inventory[id]);
        DrawTextEx(g.assets.font8, buf, { rr.x + 22, rr.y + 7 }, 8, 1, WHITE);
        if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            s_pendingItem = id; s_itemsTargetMode = 1; PlaySound(g.assets.cursor);
        }
        row++;
    }
    if (row == 0) DrawTextEx(g.assets.font8, "(empty)", { r.x + 12, r.y + 26 }, 8, 1, Color{150, 150, 170, 255});
}

static void updateMenu(float dt) {
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_E)) { g.mode = MODE_OVERWORLD; PlaySound(g.assets.cancel); return; }
    // page 2 (items) draws and handles its own input/clicks in drawMenu

    // keyboard navigation of root list
    if (g.menuPage == 0) {
        if (IsKeyPressed(KEY_DOWN)) { g.menuIndex = (g.menuIndex + 1) % MENU_COUNT; PlaySound(g.assets.cursor); }
        if (IsKeyPressed(KEY_UP))   { g.menuIndex = (g.menuIndex + MENU_COUNT - 1) % MENU_COUNT; PlaySound(g.assets.cursor); }
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
            PlaySound(g.assets.confirm);
            if (g.menuIndex == 0) g.mode = MODE_OVERWORLD;
            else if (g.menuIndex == 1) g.menuPage = 1;
            else if (g.menuIndex == 2) g.menuPage = 2;
            else if (g.menuIndex == 3) { if (saveGame()) pushToast("Progress saved."); else pushToast("Save failed!"); }
            else if (g.menuIndex == 4) g.menuPage = 4;
            else if (g.menuIndex == 5) { g.mode = MODE_TITLE; g.saveExists = saveExists(); StopMusicStream(g.assets.field); }
        }
    } else {
        if (IsKeyPressed(KEY_ESCAPE)) g.menuPage = 0;
    }
    // mouse clicks on root list
    if (g.menuPage == 0) {
        Rectangle lp = { 8, 8, 200, VRES_H - 16 };
        for (int i = 0; i < MENU_COUNT; i++) {
            Rectangle row = { lp.x + 6, lp.y + 26 + i * 22, lp.width - 12, 20 };
            if (CheckCollisionPointRec(getVirtualMouse(), row)) {
                g.menuIndex = i;
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    PlaySound(g.assets.confirm);
                    if (i == 0) g.mode = MODE_OVERWORLD;
                    else if (i == 1) g.menuPage = 1;
                    else if (i == 2) { g.menuPage = 2; s_itemsTargetMode = 0; }
                    else if (i == 3) { if (saveGame()) pushToast("Progress saved."); else pushToast("Save failed!"); }
                    else if (i == 4) g.menuPage = 4;
                    else if (i == 5) { g.mode = MODE_TITLE; g.saveExists = saveExists(); StopMusicStream(g.assets.field); }
                }
            }
        }
    } else if (g.menuPage == 4) {
        // options: toggle rows handled in draw via click
    }
    // options clicks
    if (g.menuPage == 4) {
        Rectangle rp = { 216, 8, 288, VRES_H - 16 };
        Rectangle row0 = { rp.x + 8, rp.y + 26, rp.width - 16, 22 };
        Rectangle row1 = { rp.x + 8, rp.y + 52, rp.width - 16, 22 };
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            Vector2 m = getVirtualMouse();
            if (CheckCollisionPointRec(m, row0)) { g.waitBattle = !g.waitBattle; PlaySound(g.assets.confirm); }
            if (CheckCollisionPointRec(m, row1)) { g.musicOn = !g.musicOn; applyMusicVolume(); PlaySound(g.assets.confirm); }
        }
    }
}

static void drawMenu() {
    // dim behind
    DrawRectangle(0, 0, VRES_W, VRES_H, Fade(BLACK, 0.35f));
    Rectangle lp = { 8, 8, 200, VRES_H - 16 };
    drawPanel(lp);
    DrawTextEx(g.assets.font8, "MENU", { lp.x + 10, lp.y + 8 }, 8, 1, Color{255, 214, 130, 255});
    for (int i = 0; i < MENU_COUNT; i++) {
        Rectangle row = { lp.x + 6, lp.y + 26 + i * 22, lp.width - 12, 20 };
        bool sel = (i == g.menuIndex);
        Color col = sel ? Color{255, 224, 120, 255} : Color{220, 226, 255, 255};
        if (sel) DrawTextEx(g.assets.font8, "-", { row.x + 2, row.y + 6 }, 8, 1, col);
        DrawTextEx(g.assets.font8, MENU_ITEMS[i], { row.x + 12, row.y + 6 }, 8, 1, col);
    }
    // gil bottom of left panel
    std::string gil = formatGil(g.gil);
    DrawTexture(g.assets.coinIcon, (int)lp.x + 8, (int)lp.y + lp.height - 24, WHITE);
    DrawTextEx(g.assets.font8, gil.c_str(), { lp.x + 26, lp.y + lp.height - 20 }, 8, 1, Color{255, 224, 120, 255});

    Rectangle rp = { 216, 8, 288, VRES_H - 16 };
    drawPanel(rp);
    if (g.menuPage == 1) drawStatusPage(rp);
    else if (g.menuPage == 2) updateItemsPage(rp, 0.0f);   // draw-only call handles hover visuals
    else if (g.menuPage == 4) {
        DrawTextEx(g.assets.font8, "OPTIONS", { rp.x + 10, rp.y + 8 }, 8, 1, Color{200, 220, 255, 255});
        char buf[64];
        snprintf(buf, sizeof(buf), "ATB Speed:  %s", g.waitBattle ? "Wait" : "Active");
        DrawTextEx(g.assets.font8, buf, { rp.x + 16, rp.y + 30 }, 8, 1, WHITE);
        snprintf(buf, sizeof(buf), "Music:      %s", g.musicOn ? "On" : "Off");
        DrawTextEx(g.assets.font8, buf, { rp.x + 16, rp.y + 56 }, 8, 1, WHITE);
        DrawTextEx(g.assets.font8, "Active: gauges fill during menus", { rp.x + 12, rp.y + 96 }, 8, 1, Color{150, 155, 200, 255});
        DrawTextEx(g.assets.font8, "Wait: gauges freeze in menus", { rp.x + 12, rp.y + 110 }, 8, 1, Color{150, 155, 200, 255});
        DrawTextEx(g.assets.font8, "click a row to toggle", { rp.x + 12, rp.y + 132 }, 8, 1, Color{120, 125, 165, 255});
    } else {
        DrawTextEx(g.assets.font8, "A PLACE FOR AGENTS", { rp.x + 10, rp.y + 8 }, 8, 1, Color{190, 200, 240, 255});
        DrawTextEx(g.assets.font8, "The wilds grow as you explore.", { rp.x + 10, rp.y + 26 }, 8, 1, Color{160, 168, 215, 255});
        DrawTextEx(g.assets.font8, "Villages hold shops and inns,", { rp.x + 10, rp.y + 40 }, 8, 1, Color{160, 168, 215, 255});
        DrawTextEx(g.assets.font8, "and monsters grow bolder far", { rp.x + 10, rp.y + 54 }, 8, 1, Color{160, 168, 215, 255});
        DrawTextEx(g.assets.font8, "from home...", { rp.x + 10, rp.y + 68 }, 8, 1, Color{160, 168, 215, 255});
    }
    if (g.toastT > 0) {
        Vector2 tts = MeasureTextEx(g.assets.font8, g.toast.c_str(), 8, 1);
        Rectangle tr = { VRES_W / 2.0f - tts.x / 2 - 8, (float)VRES_H - 24, tts.x + 16, 14 };
        drawPanel(tr);
        DrawTextEx(g.assets.font8, g.toast.c_str(), { tr.x + 8, tr.y + 3 }, 8, 1, Color{220, 255, 220, 255});
    }
}

// =============================================================== SHOP
static int s_shopPendingEquip = -1;   // item id waiting for "equip to who?"

static void buyItem(int id) {
    int price = itemDef(id).price;
    if (g.gil < price) { PlaySound(g.assets.cancel); pushToast("Not enough gil!"); return; }
    if (itemDef(id).kind == IK_CONSUMABLE) {
        g.gil -= price; g.inventory[id]++;
        PlaySound(g.assets.coin); pushToast(std::string("Bought ") + itemDef(id).name + "!");
    } else {
        g.gil -= price; s_shopPendingEquip = id;
        PlaySound(g.assets.coin);
    }
}

static void updateShop(float dt) {
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_E)) { s_shopPendingEquip = -1; g.mode = MODE_OVERWORLD; PlaySound(g.assets.cancel); return; }
    if (s_shopPendingEquip >= 0) {
        Vector2 m = getVirtualMouse();
        Rectangle center = { VRES_W / 2.0f - 110, 96, 220, 128 };
        static const char* prompts[3] = { "Equip to Aldric?", "Equip to Iris?", "Equip to Pip?" };
        for (int i = 0; i < 3; i++) {
            Rectangle row = { center.x + 8, center.y + 22 + i * 30, center.width - 16, 27 };
            if (CheckCollisionPointRec(m, row) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (itemDef(s_shopPendingEquip).kind == IK_WEAPON) g.party[i].weapon = s_shopPendingEquip;
                else g.party[i].armor = s_shopPendingEquip;
                PlaySound(g.assets.confirm);
                pushToast(std::string("Equipped ") + itemDef(s_shopPendingEquip).name + " to " + heroName(g.party[i].klass) + "!");
                s_shopPendingEquip = -1;
                return;
            }
        }
        return;
    }
    // leave button
    Vector2 m = getVirtualMouse();
    Rectangle leave = { VRES_W - 92, VRES_H - 32, 78, 20 };
    if (CheckCollisionPointRec(m, leave) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { g.mode = MODE_OVERWORLD; PlaySound(g.assets.door); return; }
    // click rows
    Rectangle lp = { 24, 34, 250, 210 };
    for (int id = 0; id < IT_COUNT; id++) {
        Rectangle row = { lp.x + 4, lp.y + 4 + id * 19, lp.width - 8, 18 };
        if (CheckCollisionPointRec(m, row) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) buyItem(id);
    }
}

static void drawShop() {
    DrawRectangle(0, 0, VRES_W, VRES_H, Fade(BLACK, 0.35f));
    const char* town = (g.villageLoaded && g.village.name.size()) ? g.village.name.c_str() : "Traveling Merchant";
    Rectangle r = { 10, 8, VRES_W - 20, VRES_H - 16 };
    drawPanel(r);
    char title[64]; snprintf(title, sizeof(title), "SHOP  -  %s", town);
    DrawTextEx(g.assets.font8, title, { r.x + 10, r.y + 6 }, 8, 1, Color{255, 214, 130, 255});
    // gil
    std::string gil = formatGil(g.gil);
    Vector2 ts = MeasureTextEx(g.assets.font8, gil.c_str(), 8, 1);
    DrawTexture(g.assets.coinIcon, (int)r.x + r.width - (int)ts.x - 26, (int)r.y + 4, WHITE);
    DrawTextEx(g.assets.font8, gil.c_str(), { r.x + r.width - ts.x - 6, r.y + 6 }, 8, 1, Color{255, 224, 120, 255});

    // item list (left)
    Rectangle lp = { 24, 34, 250, 210 };
    Vector2 m = getVirtualMouse();
    int hoverId = -1;
    for (int id = 0; id < IT_COUNT; id++) {
        Rectangle row = { lp.x + 4, lp.y + 4 + id * 19, lp.width - 8, 18 };
        bool hover = CheckCollisionPointRec(m, row);
        if (hover) hoverId = id;
        if (hover) DrawRectangleRounded(row, 0.25f, 3, Color{48, 54, 112, 255});
        DrawTexture(g.assets.icon[id], (int)row.x + 1, (int)row.y + 1, WHITE);
        Color col = (g.gil >= itemDef(id).price) ? Color{225, 230, 255, 255} : Color{140, 140, 160, 255};
        DrawTextEx(g.assets.font8, itemDef(id).name, { row.x + 20, row.y + 5 }, 8, 1, col);
        char price[24]; snprintf(price, sizeof(price), "%d", itemDef(id).price);
        Vector2 ps = MeasureTextEx(g.assets.font8, price, 8, 1);
        DrawTextEx(g.assets.font8, price, { row.x + row.width - ps.x - 4, row.y + 5 }, 8, 1, Color{255, 224, 120, 255});
    }
    // description (right)
    Rectangle dp = { 284, 44, 204, 130 };
    DrawRectangleRounded(dp, 0.12f, 5, Color{26, 30, 70, 255});
    if (hoverId >= 0) {
        DrawTextEx(g.assets.font8, itemDef(hoverId).name, { dp.x + 8, dp.y + 8 }, 8, 1, WHITE);
        DrawTextEx(g.assets.font8, itemDef(hoverId).desc, { dp.x + 8, dp.y + 26 }, 8, 1, Color{190, 205, 245, 255});
        const char* owned = (itemDef(hoverId).kind == IK_CONSUMABLE) ? "" : "equips on purchase";
        DrawTextEx(g.assets.font8, owned, { dp.x + 8, dp.y + 44 }, 8, 1, Color{150, 160, 210, 255});
        char pb[48]; snprintf(pb, sizeof(pb), "price: %d G", itemDef(hoverId).price);
        DrawTextEx(g.assets.font8, pb, { dp.x + 8, dp.y + 62 }, 8, 1, Color{255, 224, 120, 255});
    } else {
        DrawTextEx(g.assets.font8, "Fine wares for", { dp.x + 8, dp.y + 12 }, 8, 1, Color{170, 180, 225, 255});
        DrawTextEx(g.assets.font8, "weary travelers.", { dp.x + 8, dp.y + 26 }, 8, 1, Color{170, 180, 225, 255});
        DrawTextEx(g.assets.font8, "Weapons and mail", { dp.x + 8, dp.y + 44 }, 8, 1, Color{150, 160, 210, 255});
        DrawTextEx(g.assets.font8, "can be tried on,", { dp.x + 8, dp.y + 58 }, 8, 1, Color{150, 160, 210, 255});
        DrawTextEx(g.assets.font8, "free of charge.", { dp.x + 8, dp.y + 72 }, 8, 1, Color{150, 160, 210, 255});
    }
    // equip picker overlay
    if (s_shopPendingEquip >= 0) {
        Rectangle center = { VRES_W / 2.0f - 110, 96, 220, 128 };
        drawPanel(center);
        char head[48]; snprintf(head, sizeof(head), "Equip to whom?");
        DrawTextEx(g.assets.font8, head, { center.x + 10, center.y + 6 }, 8, 1, Color{255, 214, 130, 255});
        static const char* names[3] = { "Aldric", "Iris", "Pip" };
        for (int i = 0; i < 3; i++) {
            Rectangle row = { center.x + 8, center.y + 22 + i * 30, center.width - 16, 27 };
            bool hover = CheckCollisionPointRec(m, row);
            DrawRectangleRounded(row, 0.2f, 4, hover ? Color{52, 58, 118, 255} : Color{32, 36, 80, 255});
            Texture2D& face = g.assets.face[i];
            DrawTexturePro(face, { 0, 0, 32, 32 }, { row.x + 3, row.y + 1, 24, 24 }, { 0, 0 }, 0.0f, WHITE);
            char buf[48]; snprintf(buf, sizeof(buf), "%s", names[i]);
            DrawTextEx(g.assets.font8, buf, { row.x + 34, row.y + 9 }, 8, 1, WHITE);
        }
    }
    // leave button
    Rectangle leave = { VRES_W - 92, VRES_H - 32, 78, 20 };
    bool hoverLeave = CheckCollisionPointRec(m, leave);
    DrawRectangleRounded(leave, 0.25f, 4, hoverLeave ? Color{64, 70, 140, 255} : Color{40, 44, 96, 255});
    DrawRectangleRoundedLinesEx(leave, 0.25f, 4, 1.2f, hoverLeave ? WHITE : Color{150,158,230,255});
    const char* lt = "Leave";
    Vector2 lts = MeasureTextEx(g.assets.font8, lt, 8, 1);
    DrawTextEx(g.assets.font8, lt, { leave.x + leave.width / 2 - lts.x / 2, leave.y + 6 }, 8, 1, WHITE);
}

// =============================================================== INN
static const int INN_COST = 12;
static void updateInn(float dt) {
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_E)) { g.mode = MODE_OVERWORLD; PlaySound(g.assets.cancel); return; }
    Vector2 m = getVirtualMouse();
    Rectangle center = { VRES_W / 2.0f - 130, 86, 260, 130 };
    Rectangle rest = { center.x + 16, center.y + 78, 104, 30 };
    Rectangle leave = { center.x + center.width - 120, center.y + 78, 104, 30 };
    if (CheckCollisionPointRec(m, rest) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (g.gil >= INN_COST) {
            g.gil -= INN_COST;
            for (auto& h : g.party) { h.hp = h.maxHp; h.mp = h.maxMp; h.ko = false; }
            PlaySound(g.assets.door);
            pushToast("The party is fully rested!");
            g.mode = MODE_OVERWORLD;
        } else {
            PlaySound(g.assets.cancel);
            pushToast("Not enough gil!");
        }
    } else if (CheckCollisionPointRec(m, leave) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        g.mode = MODE_OVERWORLD; PlaySound(g.assets.door);
    }
}
static void drawInn() {
    DrawRectangle(0, 0, VRES_W, VRES_H, Fade(BLACK, 0.4f));
    const char* town = (g.villageLoaded && g.village.name.size()) ? g.village.name.c_str() : "Wayside Inn";
    Rectangle center = { VRES_W / 2.0f - 130, 86, 260, 130 };
    drawPanel(center);
    char title[64]; snprintf(title, sizeof(title), "INN  -  %s", town);
    DrawTextEx(g.assets.font8, title, { center.x + 12, center.y + 10 }, 8, 1, Color{255, 214, 130, 255});
    DrawTextEx(g.assets.font8, "Rest by the fire for", { center.x + 12, center.y + 30 }, 8, 1, Color{220, 226, 255, 255});
    char cost[48]; snprintf(cost, sizeof(cost), "%d G  -  full HP & MP, cure KO", INN_COST);
    DrawTextEx(g.assets.font8, cost, { center.x + 12, center.y + 44 }, 8, 1, Color{190, 205, 245, 255});
    uiButton("Rest", { center.x + 16, center.y + 78, 104, 30 }, 8);
    uiButton("Leave", { center.x + center.width - 120, center.y + 78, 104, 30 }, 8);
    if (g.toastT > 0) {
        Vector2 tts = MeasureTextEx(g.assets.font8, g.toast.c_str(), 8, 1);
        Rectangle tr = { VRES_W / 2.0f - tts.x / 2 - 8, (float)VRES_H - 24, tts.x + 16, 14 };
        drawPanel(tr);
        DrawTextEx(g.assets.font8, g.toast.c_str(), { tr.x + 8, tr.y + 3 }, 8, 1, Color{220, 255, 220, 255});
    }
}

// =============================================================== DIALOG (houses)
static const char* VILLAGER_LINES[] = {
    "The wilds hum tonight... the trees keep\nsecrets that aren't yours.",
    "They say a city waits beyond the great\npeaks. Nobody walks that far and back.",
    "Rest well, traveler. The meadow feeds\nthe bold and the foolish alike.",
    "I heard bells under the hills last winter.\nBeautiful, terrible bells.",
    "Take the left fork at the old pine.\nThe right fork bites.",
    "Every map in this house is a lie.\nBeautiful, useless lies.",
};
static const int VILLAGER_LINE_COUNT = 6;
static void updateDialog(float dt) {
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_E) || IsKeyPressed(KEY_ENTER) || IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        g.mode = MODE_OVERWORLD; PlaySound(g.assets.door);
    }
}
static void drawDialog() {
    DrawRectangle(0, 0, VRES_W, VRES_H, Fade(BLACK, 0.25f));
    Rectangle r = { 30, 178, VRES_W - 60, 96 };
    drawPanel(r);
    DrawTextEx(g.assets.font8, "VILLAGER", { r.x + 10, r.y + 6 }, 8, 1, Color{255, 214, 130, 255});
    if (g.dialogGift) {
        DrawTextEx(g.assets.font8, "A potion for the road.", { r.x + 10, r.y + 24 }, 8, 1, Color{220, 226, 255, 255});
        DrawTextEx(g.assets.font8, "May your blade stay sharp!", { r.x + 10, r.y + 38 }, 8, 1, Color{180, 255, 190, 255});
        DrawTexture(g.assets.icon[IT_POTION], (int)r.x + 10, (int)r.y + 52, WHITE);
        DrawTextEx(g.assets.font8, "+1 Potion", { r.x + 30, r.y + 56 }, 8, 1, Color{255, 200, 190, 255});
    } else {
        int idx = (int)((gAnimTime * 0.5f)) % VILLAGER_LINE_COUNT;
        const char* line = VILLAGER_LINES[idx];
        // manual 2-line split on '\n'
        const char* nl = strchr(line, '\n');
        if (nl) {
            char l1[96]; size_t n = (size_t)(nl - line);
            memcpy(l1, line, n); l1[n] = 0;
            DrawTextEx(g.assets.font8, l1, { r.x + 10, r.y + 24 }, 8, 1, Color{220, 226, 255, 255});
            DrawTextEx(g.assets.font8, nl + 1, { r.x + 10, r.y + 38 }, 8, 1, Color{220, 226, 255, 255});
        } else {
            DrawTextEx(g.assets.font8, line, { r.x + 10, r.y + 24 }, 8, 1, Color{220, 226, 255, 255});
        }
    }
    DrawTextEx(g.assets.font8, "click / E to leave", { r.x + 10, r.y + 74 }, 8, 1, Color{150, 155, 205, 255});
}

// =============================================================== GAME OVER
static void updateGameover(float dt) {
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        StopMusicStream(g.assets.gameover);
        g.mode = MODE_TITLE;
        g.saveExists = saveExists();
        g.fade = 1.0f; g.fadeDir = -1;
        PlaySound(g.assets.confirm);
    }
}
static void drawGameover() {
    DrawRectangle(0, 0, VRES_W, VRES_H, Color{5, 4, 10, 255});
    DrawTexturePro(g.assets.titleBg, { 0, 0, (float)g.assets.titleBg.width, (float)g.assets.titleBg.height },
                   { 0, 0, VRES_W, VRES_H }, { 0, 0 }, 0.0f, Fade(Color{70, 60, 90, 255}, 0.25f));
    const char* t = "THE WILDS RECLAIM YOU";
    Vector2 ts = MeasureTextEx(g.assets.font16, t, 16, 2);
    DrawTextEx(g.assets.font16, t, { VRES_W / 2.0f - ts.x / 2, 108 }, 16, 2, Color{190, 60, 60, 255});
    if (sinf(gAnimTime * 3.2f) > -0.15f) {
        const char* hint = "click / Enter to return to title";
        Vector2 hts = MeasureTextEx(g.assets.font8, hint, 8, 1);
        DrawTextEx(g.assets.font8, hint, { VRES_W / 2.0f - hts.x / 2, 158 }, 8, 1, Color{180, 170, 200, 255});
    }
}

// =============================================================== MAIN
int main() {
    InitWindow(1280, 720, "A Place for Agents");
    SetTargetFPS(60);
    SetExitKey(0);  // ESC opens the menu in-game instead of quitting

    g.rt = LoadRenderTexture(VRES_W, VRES_H);
    SetTextureFilter(g.rt.texture, TEXTURE_FILTER_POINT);

    if (!loadAllAssets()) {
        // even with missing assets we keep running; raylib draws are safe no-ops for 0 ids mostly
    } else {
        applyMusicVolume();
    }

    newGameState((uint32_t)time(nullptr) * 2654435761u ^ 0xA9E4701u);
    g.mode = MODE_TITLE;
    g.saveExists = saveExists();
    gAnimTime = 0.0f;

    for (int i = 0; i < 6; i++) {
        g.cloudX[i] = (float)(i * 97);
        g.cloudY[i] = 6.0f + (float)((i * 41) % 64);
        g.cloudScale[i] = 0.7f + (float)(i % 3) * 0.4f;
    }
    if (g.musicOn) PlayMusicStream(g.assets.title);

    while (!WindowShouldClose() && !gForceQuit) {
        float dt = std::min(GetFrameTime(), 0.05f);
        gAnimTime += dt;

        computeLayout();
        Vector2 mp = GetMousePosition();
        g_vmouse = { (mp.x - gOffX) / gScale, (mp.y - gOffY) / gScale };

        updateMusicStreams();
        ensureMusic();

        // ---- update
        switch (g.mode) {
            case MODE_TITLE:     updateTitle(dt); break;
            case MODE_OVERWORLD: updateOverworld(dt); break;
            case MODE_BATTLE:    battleUpdate(dt); if (g.toastT > 0) g.toastT -= dt; break;
            case MODE_MENU:      updateMenu(dt); if (g.toastT > 0) g.toastT -= dt; break;
            case MODE_SHOP:      updateShop(dt); if (g.toastT > 0) g.toastT -= dt; break;
            case MODE_INN:       updateInn(dt); if (g.toastT > 0) g.toastT -= dt; break;
            case MODE_DIALOG:    updateDialog(dt); break;
            case MODE_GAMEOVER:  updateGameover(dt); break;
        }
        // fade (fade-in from black)
        if (g.fadeDir != 0) {
            g.fade += g.fadeDir * dt * 1.7f;
            if (g.fade <= 0.0f) { g.fade = 0.0f; g.fadeDir = 0; }
            if (g.fade >= 1.0f) { g.fade = 1.0f; }
        }

        // ---- render to virtual target
        BeginTextureMode(g.rt);
            ClearBackground(Color{10, 10, 18, 255});
            switch (g.mode) {
                case MODE_TITLE:
                    drawTitle();
                    break;
                case MODE_OVERWORLD:
                    drawWorld();
                    drawHud();
                    break;
                case MODE_BATTLE:
                    battleDraw();
                    break;
                case MODE_MENU:
                    drawWorld();
                    drawHud();
                    drawMenu();
                    break;
                case MODE_SHOP:
                    drawWorld();
                    drawShop();
                    break;
                case MODE_INN:
                    drawWorld();
                    drawInn();
                    break;
                case MODE_DIALOG:
                    drawWorld();
                    drawDialog();
                    break;
                case MODE_GAMEOVER:
                    drawGameover();
                    break;
            }
            if (g.fade > 0.0f) DrawRectangle(0, 0, VRES_W, VRES_H, Fade(BLACK, g.fade));
        EndTextureMode();

        // ---- blit with letterboxing
        BeginDrawing();
            ClearBackground(BLACK);
            DrawTexturePro(g.rt.texture,
                           { 0.0f, 0.0f, (float)VRES_W, -(float)VRES_H },
                           { gOffX, gOffY, VRES_W * gScale, VRES_H * gScale },
                           { 0.0f, 0.0f }, 0.0f, WHITE);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
