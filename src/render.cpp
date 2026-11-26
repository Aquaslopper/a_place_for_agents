// render.cpp — the field renderer, HUD and mode screens
#include "game.hpp"
#include "world.hpp"
#include "battle.hpp"
#include <cstdio>

// ================================================================ helpers
static void texAt(Texture2D t, float x, float y, Color tint = WHITE, float scale = 1.0f) {
    if (t.id == 0) return;
    Rectangle src{ 0, 0, (float)t.width, (float)t.height };
    Rectangle dst{ x, y, t.width * scale, t.height * scale };
    DrawTexturePro(t, src, dst, { 0, 0 }, 0.0f, tint);
}

// bottom-centred world sprite
static void texFoot(Texture2D t, float cx, float footY, Color tint = WHITE, float scale = 1.0f) {
    if (t.id == 0) return;
    DrawTexturePro(t, { 0, 0, (float)t.width, (float)t.height },
                   { cx - t.width * scale * 0.5f, footY - t.height * scale,
                     (float)t.width * scale, (float)t.height * scale },
                   { 0, 0 }, 0.0f, tint);
}

// ================================================================ field monsters
void drawFieldMonsters() {
    for (const auto& m : gFieldMonsters) {
        Texture2D& t = g.assets.enemy[m.kind];
        float bob = sinf(m.phase * 3.0f) * 1.5f;
        float sx = (m.x + 0.5f) * TILE - g.camX;
        float sy = (m.y + 1.0f) * TILE - g.camY;
        // soft shadow
        DrawEllipse((int)sx, (int)sy - 1, (float)t.width * 0.30f, 3.0f, Fade(BLACK, 0.25f));
        texFoot(t, sx, sy + bob, WHITE, 0.62f);
        // the classic exclamation when it notices you
        float dx = g.px - m.x, dy = g.py - m.y;
        if (dx * dx + dy * dy < 9.0f) {
            float t2 = fmodf(g.totalTime * 2.0f, 1.0f);
            if (t2 < 0.6f) drawText("!", sx - 2, sy - t.height * 0.62f - 14, Color{ 255, 90, 90, 255 }, 8);
        }
    }
}

// ================================================================ ground
static Texture2D* groundTexture(const GroundTile& gt) {
    static int anim = 0;
    switch (gt.kind) {
        case G_GRASS:      return &g.assets.grass[gt.variant & 3];
        case G_GRASS_DARK: return &g.assets.grass[(gt.variant & 1) ? 2 : 1];
        case G_DIRT:       return &g.assets.dirt;
        case G_SAND:       return (gt.variant & 1) ? &g.assets.sand : &g.assets.beach;
        case G_WATER:      return &g.assets.water[anim];
        case G_DEEP:       return &g.assets.deepWater;
        case G_ROCK:       return &g.assets.rockHill[gt.variant & 1];
        case G_SNOW:       return &g.assets.snow[gt.variant & 1];
        case G_ICE:        return &g.assets.ice[gt.variant & 1];
        case G_SWAMP:      return &g.assets.swamp;
        case G_COBBLE:     return &g.assets.cobble;
        case G_FLOOR:      return &g.assets.flagstone;
        default:           return &g.assets.grass[0];
    }
}

static void drawGround(int x0, int y0, int x1, int y1) {
    int anim = (int)(g.totalTime * 3.0f) & 1;
    for (int ty = y0; ty <= y1; ty++) {
        for (int tx = x0; tx <= x1; tx++) {
            GroundTile gt = sampleGround(tx, ty);
            Texture2D* t = groundTexture(gt);
            if (gt.kind == G_WATER) t = &g.assets.water[anim];
            if (!t || t->id == 0) continue;
            DrawTexturePro(*t, { 0, 0, (float)t->width, (float)t->height },
                           { tx * TILE - g.camX, ty * TILE - g.camY, (float)TILE, (float)TILE },
                           { 0, 0 }, 0.0f, WHITE);
        }
    }
    // path tiles from villages
    if (g.villageLoaded) {
        for (auto& p : g.village.paths) {
            if (p.first < x0 - 2 || p.first > x1 + 2 || p.second < y0 - 2 || p.second > y1 + 2) continue;
            Texture2D& d = g.assets.dirt;
            DrawTexturePro(d, { 0, 0, (float)d.width, (float)d.height },
                           { p.first * TILE - g.camX, p.second * TILE - g.camY, (float)TILE, (float)TILE },
                           { 0, 0 }, 0.0f, WHITE);
        }
        for (auto& p : g.village.plaza) {
            if (p.first < x0 - 2 || p.first > x1 + 2 || p.second < y0 - 2 || p.second > y1 + 2) continue;
            Texture2D& c = g.assets.cobble;
            DrawTexturePro(c, { 0, 0, (float)c.width, (float)c.height },
                           { p.first * TILE - g.camX, p.second * TILE - g.camY, (float)TILE, (float)TILE },
                           { 0, 0 }, 0.0f, WHITE);
        }
    }
    // ground decoration
    for (int ty = y0; ty <= y1; ty++) {
        for (int tx = x0; tx <= x1; tx++) {
            GroundTile gt = sampleGround(tx, ty);
            if (gt.deco == 0) continue;
            Texture2D* t = nullptr;
            if (gt.deco == 1) t = &g.assets.tuft[gt.decoVariant % 3];
            else if (gt.deco == 2) t = &g.assets.flowers[gt.decoVariant % 2];
            else t = &g.assets.pebble;
            if (!t || t->id == 0) continue;
            // sit the decoration in the lower half of its tile, like the chipset expects
            float w = t->width > TILE ? (float)TILE : (float)t->width;
            float h = t->height > TILE ? (float)TILE : (float)t->height;
            DrawTexturePro(*t, { 0, 0, (float)t->width, (float)t->height },
                           { tx * TILE - g.camX + (TILE - w) * 0.5f,
                             ty * TILE - g.camY + (TILE - h),
                             w, h },
                           { 0, 0 }, 0.0f, WHITE);
        }
    }
}

// ================================================================ props
static void drawProps(int x0, int y0, int x1, int y1, float playerFootY) {
    // collect and y-sort so trees properly overlap the hero
    struct Item { float y; int kind; int tx, ty; };
    static std::vector<Item> items;
    items.clear();
    for (int ty = y0 - 3; ty <= y1 + 3; ty++) {
        for (int tx = x0 - 3; tx <= x1 + 3; tx++) {
            PropKind p = propAt(tx, ty);
            if (p == P_NONE) continue;
            items.push_back({ (float)(ty + 1) * TILE, (int)p, tx, ty });
        }
    }
    std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.y < b.y; });
    for (auto& it : items) {
        int w = 0, h = 0;
        Texture2D* t = propTexture((PropKind)it.kind, &w, &h);
        if (!t || t->id == 0) continue;
        float fx = it.tx * TILE - g.camX + TILE * 0.5f;
        float fy = it.ty * TILE - g.camY + TILE;
        DrawEllipse((int)fx, (int)fy - 2, w * 0.28f, 3.0f, Fade(BLACK, 0.18f));
        texFoot(*t, fx, fy);
    }
}

// ================================================================ buildings
static Texture2D* buildingTexture(const Building& b) {
    switch (b.kind) {
        case 5: return &g.assets.shop;
        case 6: return &g.assets.inn;
        case 7: return &g.assets.temple;
        case 8: return &g.assets.manor;
        case 9: return &g.assets.castle;
        default: return &g.assets.house[std::max(0, std::min(4, b.kind))];
    }
}

static void drawBuildings(int x0, int y0, int x1, int y1) {
    if (!g.villageLoaded) return;
    for (auto& b : g.village.buildings) {
        if (b.tx < x0 - 10 || b.tx > x1 + 10 || b.ty < y0 - 10 || b.ty > y1 + 10) continue;
        Texture2D* t = buildingTexture(b);
        if (!t || t->id == 0) continue;
        float cx = b.tx * TILE - g.camX + TILE * 0.5f;
        float fy = (b.ty + 1) * TILE - g.camY;
        DrawEllipse((int)cx, (int)fy - 3, b.w * TILE * 0.42f, 5.0f, Fade(BLACK, 0.22f));
        texFoot(*t, cx, fy);
        // a warm lamp glow at the door at night
    }
}

// ================================================================ characters
static Texture2D* charsetFor(int sprite) {
    if (sprite < 0 || sprite >= 16) return &g.assets.npc[0];
    return &g.assets.npc[sprite];
}

static void drawCharSheet(Texture2D& sheet, int row, int frame, float cx, float footY, bool flip,
                          Color tint = WHITE) {
    if (sheet.id == 0) return;
    int cols = sheet.width / 24;         // 3
    int rows = sheet.height / 32;        // 3 or 4
    if (cols < 3 || rows < 1) return;
    row = std::max(0, std::min(rows - 1, row));
    frame = std::max(0, std::min(2, frame));
    Rectangle src{ (float)(frame * 24), (float)(row * 32), 24, 32 };
    if (flip) { src.width = -24; src.x += 24; }
    Rectangle dst{ cx - 12, footY - 32, 24, 32 };
    DrawTexturePro(sheet, src, dst, { 0, 0 }, 0.0f, tint);
}

static void drawNPCs() {
    for (auto& n : g.npcs) {
        Texture2D* sheet = charsetFor(n.sprite);
        if (!sheet || sheet->id == 0) continue;
        int row = 0;
        if (n.moving) row = (n.dx != 0) ? 2 : (n.dy > 0 ? 0 : 1);
        else row = 0;
        int frame = 1;
        if (n.moving) frame = 1 + ((int)(n.animT * 6.0f) & 1);
        float cx = n.x * TILE - g.camX + TILE * 0.5f;
        float fy = (n.y + 1) * TILE - g.camY;
        DrawEllipse((int)cx, (int)fy - 2, 5.0f, 2.5f, Fade(BLACK, 0.22f));
        drawCharSheet(*sheet, row, frame, cx, fy, n.dx < 0);
    }
}

static void drawPlayer() {
    int klass = g.party[0].klass;
    Texture2D& sheet = g.assets.heroWalk[std::max(0, std::min(2, klass))];
    int row = (g.facing == 0) ? 0 : (g.facing == 1 ? 1 : 2);
    int frame = 1;
    if (g.moving) frame = 1 + ((int)(g.walkAnimT * 7.0f) & 1);
    float cx = g.px * TILE - g.camX + TILE * 0.5f;
    float fy = (g.py + 1) * TILE - g.camY;
    if (g.interactT > 0) fy -= sinf((0.25f - g.interactT) * 12.0f) * 2.0f;
    DrawEllipse((int)cx, (int)fy - 2, 6.0f, 3.0f, Fade(BLACK, 0.28f));
    drawCharSheet(sheet, row, frame, cx, fy, g.facing == 2 && g.sideFlip);
}

// ================================================================ chests & landmarks
static void drawChests() {
    for (auto& c : gChestsInView) {
        bool opened = g.openedChests.count(c.id) > 0;
        Texture2D& t = opened ? g.assets.chestOpen : g.assets.chestClosed;
        float cx = c.tx * TILE - g.camX + TILE * 0.5f;
        float fy = (c.ty + 1) * TILE - g.camY;
        DrawEllipse((int)cx, (int)fy - 1, 6.0f, 2.5f, Fade(BLACK, 0.25f));
        texFoot(t, cx, fy);
    }
}

static void drawLandmark(const Landmark& lm) {
    float cx = lm.tx * TILE - g.camX + TILE * 0.5f;
    float fy = (lm.ty + 1) * TILE - g.camY;
    switch (lm.kind) {
        case LK_RUINS:
            texFoot(g.assets.ruinArch, cx - 18, fy);
            texFoot(g.assets.ruinShrine, cx + 22, fy);
            texFoot(g.assets.pillar, cx - 40, fy);
            break;
        case LK_TOWER:  texFoot(g.assets.tower, cx, fy); break;
        case LK_CASTLE: texFoot(g.assets.castle, cx, fy); break;
        case LK_STONES:
            texFoot(g.assets.statue, cx, fy);
            texFoot(g.assets.pillar, cx - 26, fy);
            texFoot(g.assets.pillar, cx + 26, fy);
            break;
        case LK_CAMP:
            texFoot(g.assets.campfire, cx, fy);
            texFoot(g.assets.bench, cx - 24, fy);
            texFoot(g.assets.cauldron, cx + 24, fy);
            break;
        default: break;
    }
}

// ================================================================ village deco
static void drawVillageDeco() {
    if (!g.villageLoaded) return;
    struct D { float y; int kind, variant; float x; };
    static std::vector<D> list;
    list.clear();
    for (auto& d : g.village.decos) list.push_back({ d.y, d.kind, d.variant, d.x });
    std::sort(list.begin(), list.end(), [](const D& a, const D& b) { return a.y < b.y; });
    for (auto& d : list) {
        int w = 0, h = 0;
        Texture2D* t = nullptr;
        villageDecoTexture(d.kind, d.variant, t, w, h);
        if (!t || t->id == 0) continue;
        float cx = d.x * TILE - g.camX + TILE * 0.5f;
        float fy = (d.y + 1) * TILE - g.camY;
        DrawEllipse((int)cx, (int)fy - 1, w * 0.3f, 2.5f, Fade(BLACK, 0.18f));
        texFoot(*t, cx, fy);
        // torches and campfires glow
        if (d.kind == VD_TORCH || d.kind == VD_CAMPFIRE) {
            float pulse = 0.5f + 0.5f * sinf(g.totalTime * 4.0f + d.x);
            DrawCircleGradient((int)cx, (int)(fy - h * 0.5f), 22 + pulse * 6,
                               Fade(Color{ 255, 170, 60, 255 }, 0.20f), BLANK);
        }
    }
}

// ================================================================ clouds
void updateClouds(float dt) {
    for (int i = 0; i < 6; i++) {
        if (g.cloudScale[i] == 0) {
            g.cloudScale[i] = 0.6f + hashf(i, 3, 11u) * 0.7f;
            g.cloudX[i] = hashf(i, 5, 13u) * (VRES_W + 200) - 100;
            g.cloudY[i] = hashf(i, 7, 17u) * 140 - 20;
        }
        g.cloudX[i] += dt * (5.0f + i * 1.7f) * g.cloudScale[i];
        if (g.cloudX[i] > VRES_W + 140) g.cloudX[i] = -140;
    }
}
static void drawClouds() {
    Texture2D& c = g.assets.cloud;
    if (c.id == 0) return;
    for (int i = 0; i < 6; i++) {
        float s = g.cloudScale[i] * 0.55f;
        DrawTexturePro(c, { 0, 0, (float)c.width, (float)c.height },
                       { g.cloudX[i], g.cloudY[i], c.width * s, c.height * s }, { 0, 0 }, 0.0f,
                       Fade(WHITE, 0.55f));
    }
}

// ================================================================ the world
void drawWorld() {
    float shakeX = 0, shakeY = 0;
    if (g.shakeT > 0) {
        shakeX = sinf(g.totalTime * 60.0f) * g.shakeMag;
        shakeY = cosf(g.totalTime * 47.0f) * g.shakeMag;
    }
    float saveCamX = g.camX, saveCamY = g.camY;
    g.camX -= shakeX; g.camY -= shakeY;

    int x0 = (int)floorf(g.camX / TILE) - 1;
    int y0 = (int)floorf(g.camY / TILE) - 1;
    int x1 = x0 + VRES_W / TILE + 2;
    int y1 = y0 + VRES_H / TILE + 2;

    drawGround(x0, y0, x1, y1);

    // y-sorted actor layer: player + npcs + props share one sort
    drawProps(x0, y0, x1, y1, g.py * TILE);

    // landmarks
    for (int sy = 0; sy < 2; sy++) for (int sx = 0; sx < 2; sx++) {
        Landmark lm;
        int lx = (x0 + sx * 16 + 8), ly = (y0 + sy * 12 + 6);
        if (landmarkNear(lx, ly, lm)) drawLandmark(lm);
    }

    drawVillageDeco();
    drawBuildings(x0, y0, x1, y1);
    drawChests();
    drawNPCs();
    drawFieldMonsters();
    drawPlayer();

    // click marker
    if (g.clickFlashT > 0) {
        float a = g.clickFlashT / 0.5f;
        float cx = g.clickFlashX * TILE - g.camX;
        float cy = g.clickFlashY * TILE - g.camY;
        DrawRing({ cx, cy }, 4 + (1 - a) * 6, 6 + (1 - a) * 6, 0, 360, 20, Fade(Color{ 255, 240, 180, 255 }, a));
    }
    // blocked bump
    if (g.blockedFx > 0) {
        float cx = g.px * TILE - g.camX + TILE * 0.5f;
        float cy = g.py * TILE - g.camY;
        DrawRing({ cx, cy }, 10, 12, 0, 360, 16, Fade(Color{ 255, 120, 120, 255 }, g.blockedFx * 0.5f));
    }

    drawClouds();

    // day/night wash: a slow cycle so the world feels alive
    float t = g.totalTime * 0.004f;
    float night = (sinf(t * 6.2831853f) + 1.0f) * 0.5f;
    if (night > 0.55f) {
        float a = (night - 0.55f) / 0.45f * 0.45f;
        DrawRectangle(0, 0, VRES_W, VRES_H, Fade(Color{ 20, 26, 60, 255 }, a));
    }

    g.camX = saveCamX; g.camY = saveCamY;
}

// ================================================================ HUD
void drawHUD() {
    // place label
    const char* zone = zoneNameAt(g.px, g.py);
    drawWindow({ 4, 4, 190, 22 });
    drawText(zone, 10, 9, Color{ 236, 240, 255, 255 }, 8);

    // party vitals, bottom-left
    float y = VRES_H - 58;
    drawWindow({ 4, y - 4, 168, 58 });
    for (int i = 0; i < 3; i++) {
        const Hero& h = g.party[i];
        float hy = y + 2 + i * 17;
        Texture2D& f = g.assets.face[i == 0 ? FACE_KNIGHT : (i == 1 ? FACE_MAGE : FACE_CLERIC)];
        if (f.id) DrawTexturePro(f, { 0, 0, 48, 48 }, { 8, hy - 1, 15, 15 }, { 0, 0 }, 0.0f, WHITE);
        char buf[64];
        snprintf(buf, sizeof(buf), "%s L%d", heroName(h.klass), h.level);
        drawText(buf, 26, hy, h.ko ? Color{ 200, 120, 120, 255 } : Color{ 236, 240, 255, 255 }, 8);
        drawBar(88, hy + 1, 44, 4, (float)h.hp / std::max(1, h.maxHp),
                h.ko ? Color{ 120, 60, 60, 255 } : Color{ 96, 220, 120, 255 }, Color{ 30, 40, 80, 255 });
        drawBar(136, hy + 1, 26, 4, (float)h.mp / std::max(1, h.maxMp),
                Color{ 96, 160, 255, 255 }, Color{ 30, 40, 80, 255 });
    }

    // gil, top-right
    drawWindow({ VRES_W - 112, 4, 108, 22 });
    Texture2D& coin = g.assets.icon[ICON_COIN];
    if (coin.id) DrawTexturePro(coin, { 0, 0, (float)coin.width, (float)coin.height },
                               { VRES_W - 104.0f, 8, 13, 13 }, { 0, 0 }, 0.0f, WHITE);
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", g.gil);
    drawText(buf, VRES_W - 87.0f, 9, Color{ 255, 232, 150, 255 }, 8);

    // quest tracker
    for (int q = 0; q < QUEST_COUNT; q++) {
        if (g.quests[q].state != QS_ACTIVE) continue;
        const char* title = questTitle(q);
        Vector2 sz = measureText(title, 8);
        drawWindow({ VRES_W - sz.x - 20, 32, sz.x + 16, 20 });
        drawText(title, VRES_W - sz.x - 12, 38, Color{ 255, 232, 150, 255 }, 8);
        break;
    }

    // interaction prompt
    const char* prompt = interactionPrompt();
    if (prompt) {
        Vector2 sz = measureText(prompt, 8);
        drawWindow({ VRES_W * 0.5f - sz.x * 0.5f - 8, VRES_H - 46, sz.x + 16, 20 });
        drawTextCentered(prompt, VRES_W * 0.5f, VRES_H - 40, Color{ 236, 240, 255, 255 }, 8);
    }
    drawToast();
}

void drawToast() {
    if (g.toastT <= 0 || g.toast.empty()) return;
    Vector2 sz = measureText(g.toast.c_str(), 8);
    float a = std::min(1.0f, g.toastT / 0.4f);
    float w = sz.x + 20, h = 22;
    float x = VRES_W * 0.5f - w * 0.5f, y = 34;
    DrawRectangleRounded({ x, y, w, h }, 0.4f, 4, Fade(Color{ 12, 16, 40, 255 }, 0.85f * a));
    DrawRectangleRoundedLinesEx({ x, y, w, h }, 0.4f, 4, 1.0f, Fade(Color{ 255, 220, 140, 255 }, a));
    drawTextCentered(g.toast.c_str(), VRES_W * 0.5f, y + 7, Fade(WHITE, a), 8);
}

// ================================================================ dialog box
void drawDialogBox() {
    if (g.dialog.empty()) return;
    const DialogLine& line = g.dialog[std::min<int>(g.dialogIndex, (int)g.dialog.size() - 1)];
    float h = 76;
    float y = VRES_H - h - 6;
    drawWindow({ 6, y, VRES_W - 12, h });
    if (line.face >= 0 && line.face < FACE_COUNT && g.assets.face[line.face].id) {
        Texture2D& f = g.assets.face[line.face];
        DrawTexturePro(f, { 0, 0, (float)f.width, (float)f.height }, { 14, y + 8, 48, 48 },
                       { 0, 0 }, 0.0f, WHITE);
        DrawRectangleLines(14, (int)y + 8, 48, 48, Color{ 255, 232, 150, 255 });
    }
    float tx = (line.face >= 0) ? 72 : 18;
    if (!line.speaker.empty())
        drawText(line.speaker.c_str(), tx, y + 8, Color{ 255, 232, 150, 255 }, 8);

    // typewriter reveal
    std::string shown = line.text.substr(0, (size_t)g.dialogReveal);
    float ty = y + 24;
    int maxChars = (VRES_W - (int)tx - 24) / 6;      // ~6px per glyph at size 8
    std::string row;
    int used = 0;
    for (char ch : shown) {
        if (ch == '\n' || used >= maxChars) { drawText(row.c_str(), tx, ty, WHITE, 8); ty += 12; row.clear(); used = 0; if (ch == '\n') continue; }
        row += ch; used++;
    }
    if (!row.empty()) drawText(row.c_str(), tx, ty, WHITE, 8);

    if (g.dialogReveal >= (float)line.text.size()) {
        bool last = (g.dialogIndex >= (int)g.dialog.size() - 1);
        const char* hint = last ? "[SPACE]" : "[SPACE] more";
        Vector2 sz = measureText(hint, 8);
        if (fmodf(g.totalTime * 3.0f, 2.0f) < 1.2f)
            drawText(hint, VRES_W - sz.x - 16, y + h - 14, Color{ 180, 200, 255, 255 }, 8);
    }
}

// ================================================================ title
void drawTitleScreen() {
    Texture2D& bg = g.assets.titleBg;
    if (bg.id) DrawTexturePro(bg, { 0, 0, (float)bg.width, (float)bg.height },
                              { 0, 0, VRES_W, VRES_H }, { 0, 0 }, 0.0f, WHITE);
    else { ClearBackground(Color{ 12, 18, 40, 255 }); }

    // animated cloud drift over the painting
    drawClouds();

    // vignette
    DrawRectangleGradientV(0, 0, VRES_W, VRES_H / 2, Fade(BLACK, 0.35f), BLANK);
    DrawRectangleGradientV(0, VRES_H / 2, VRES_W, VRES_H / 2, BLANK, Fade(BLACK, 0.55f));

    const char* title = "A PLACE FOR AGENTS";
    float bob = sinf(g.titleT * 1.6f) * 1.5f;
    drawTextCentered(title, VRES_W * 0.5f, 54 + bob, Color{ 12, 16, 30, 255 }, 24);
    drawTextCentered(title, VRES_W * 0.5f, 52 + bob, Color{ 255, 236, 176, 255 }, 24);
    drawTextCentered("an endless meadow tale", VRES_W * 0.5f, 84 + bob, Color{ 220, 230, 255, 200 }, 8);

    const char* opts[3] = { "NEW ADVENTURE", "CONTINUE", "QUIT" };
    for (int i = 0; i < 3; i++) {
        bool sel = (g.titleIndex == i);
        bool disabled = (i == 1 && !saveExists());
        Color c = disabled ? Color{ 120, 130, 160, 200 }
                           : (sel ? Color{ 255, 236, 176, 255 } : Color{ 224, 232, 255, 220 });
        if (sel) {
            float w = 150;
            DrawRectangleRounded({ VRES_W * 0.5f - w * 0.5f, (float)(148 + i * 24 - 3), w, 18 }, 0.5f, 4,
                                 Fade(Color{ 20, 30, 80, 255 }, 0.85f));
            DrawRectangleRoundedLinesEx({ VRES_W * 0.5f - w * 0.5f, (float)(148 + i * 24 - 3), w, 18 },
                                        0.5f, 4, 1.0f, Color{ 255, 236, 176, 255 });
        }
        drawTextCentered(opts[i], VRES_W * 0.5f, 150.0f + i * 24, c, 8);
    }
    drawTextCentered("arrows / mouse  to choose  -  SPACE to begin",
                     VRES_W * 0.5f, VRES_H - 26, Color{ 200, 210, 240, 200 }, 8);
    drawTextCentered("art: OpenRTP by finalbossblues  -  (c) 2025",
                     VRES_W * 0.5f, VRES_H - 14, Color{ 170, 180, 210, 160 }, 8);
}

// ================================================================ game over
void drawGameOverScreen() {
    if (g.assets.gameoverPlate.id)
        DrawTexturePro(g.assets.gameoverPlate, { 0, 0, (float)g.assets.gameoverPlate.width,
                                                (float)g.assets.gameoverPlate.height },
                       { 0, 0, VRES_W, VRES_H }, { 0, 0 }, 0.0f, WHITE);
    else { ClearBackground(Color{ 18, 8, 12, 255 }); }
    DrawRectangle(0, 0, VRES_W, VRES_H, Fade(Color{ 20, 4, 8, 255 }, 0.35f));
    drawTextCentered("THE PARTY HAS FALLEN", VRES_W * 0.5f, 96, Color{ 255, 210, 210, 255 }, 24);
    drawTextCentered("the meadow keeps its own", VRES_W * 0.5f, 130, Color{ 230, 220, 230, 220 }, 8);
    if (fmodf(g.totalTime * 2.0f, 2.0f) < 1.2f)
        drawTextCentered("SPACE: return to the title", VRES_W * 0.5f, VRES_H - 58,
                         Color{ 255, 236, 176, 255 }, 8);
}

// ================================================================ intro
void drawIntroScreen() {
    // a slow pan across the dawn panorama while the story scrolls
    if (g.assets.dawn.id)
        DrawTexturePro(g.assets.dawn, { 0, 0, (float)g.assets.dawn.width, (float)g.assets.dawn.height },
                       { 0, 0, VRES_W, VRES_H }, { 0, 0 }, 0.0f, WHITE);
    else ClearBackground(Color{ 30, 40, 70, 255 });
    DrawRectangle(0, 0, VRES_W, VRES_H, Fade(Color{ 6, 10, 24, 255 }, 0.35f));

    const char* lines[] = {
        "Every map you were given ends here.",
        "The road does not. Beyond the meadow",
        "the wilds roll on without a border,",
        "and something out there keeps the crown",
        "of the first village.",
        "",
        "You are three, and you have one sword.",
    };
    float startY = 70 - g.introT * 6.0f;
    for (int i = 0; i < 7; i++) {
        const char* l = lines[i];
        if (l[0] == 0) continue;
        Vector2 sz = measureText(l, 8);
        float w = sz.x + 8;
        bool isName = i >= 5;
        drawTextCentered(l, VRES_W * 0.5f, startY + i * 22,
                         isName ? Color{ 255, 236, 176, 255 } : Color{ 226, 232, 250, 255 },
                         isName ? 16 : 8);
    }
    if (g.introT > 2.0f && fmodf(g.totalTime * 2.0f, 2.0f) < 1.2f)
        drawTextCentered("SPACE to begin", VRES_W * 0.5f, VRES_H - 26, Color{ 255, 236, 176, 255 }, 8);
}

// ================================================================ transition fx
void drawTransitionFx() {
    if (g.fade <= 0.001f) return;
    DrawRectangle(0, 0, VRES_W, VRES_H, Fade(BLACK, g.fade));
}
