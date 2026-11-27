// render.cpp — the field renderer, HUD and mode screens
#include "game.hpp"
#include "world.hpp"
#include "battle.hpp"
#include <cstdio>

// ================================================================ helpers
// bottom-centred world sprite
static void texFoot(Texture2D t, float cx, float footY, Color tint = WHITE, float scale = 1.0f) {
    if (t.id == 0) return;
    DrawTexturePro(t, { 0, 0, (float)t.width, (float)t.height },
                   { cx - t.width * scale * 0.5f, footY - t.height * scale,
                     (float)t.width * scale, (float)t.height * scale },
                   { 0, 0 }, 0.0f, tint);
}

static void drawCharSheet(Texture2D& sheet, int row, int frame, float cx, float footY, bool flip,
                          Color tint = WHITE) {
    if (sheet.id == 0) return;
    int cols = sheet.width / 24;         // 3
    int rows = sheet.height / 32;        // 3
    if (cols < 3 || rows < 1) return;
    row = std::max(0, std::min(rows - 1, row));
    frame = std::max(0, std::min(2, frame));
    Rectangle src{ (float)(frame * 24), (float)(row * 32), 24, 32 };
    if (flip) { src.width = -24; }
    Rectangle dst{ cx - 12, footY - 32, 24, 32 };
    DrawTexturePro(sheet, src, dst, { 0, 0 }, 0.0f, tint);
}

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

static Texture2D* charsetFor(int sprite) {
    if (sprite < 0 || sprite >= 16) return &g.assets.npc[0];
    return &g.assets.npc[sprite];
}

// ================================================================ ground
static Texture2D* groundTexture(const GroundTile& gt, int waterAnim) {
    switch (gt.kind) {
        case G_GRASS:      return &g.assets.grass[gt.variant & 3];
        case G_GRASS_DARK: return (g.assets.grassTall.id ? &g.assets.grassTall : &g.assets.grass[2]);
        case G_DIRT:       return &g.assets.dirt;
        case G_SAND:       return (gt.variant & 1) ? &g.assets.sand : &g.assets.beach;
        case G_WATER:      return &g.assets.water[waterAnim & 1];
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
            Texture2D* t = groundTexture(gt, anim);
            if (!t || t->id == 0) continue;
            DrawTexturePro(*t, { 0, 0, (float)t->width, (float)t->height },
                           { tx * TILE - g.camX, ty * TILE - g.camY, (float)TILE, (float)TILE },
                           { 0, 0 }, 0.0f, WHITE);
        }
    }
    if (g.villageLoaded) {
        for (const auto& p : g.village.paths) {
            if (p.first < x0 - 2 || p.first > x1 + 2 || p.second < y0 - 2 || p.second > y1 + 2) continue;
            Texture2D& d = g.assets.dirt;
            DrawTexturePro(d, { 0, 0, (float)d.width, (float)d.height },
                           { p.first * TILE - g.camX, p.second * TILE - g.camY, (float)TILE, (float)TILE },
                           { 0, 0 }, 0.0f, WHITE);
        }
        for (const auto& p : g.village.plaza) {
            if (p.first < x0 - 2 || p.first > x1 + 2 || p.second < y0 - 2 || p.second > y1 + 2) continue;
            Texture2D& c = g.assets.cobble;
            DrawTexturePro(c, { 0, 0, (float)c.width, (float)c.height },
                           { p.first * TILE - g.camX, p.second * TILE - g.camY, (float)TILE, (float)TILE },
                           { 0, 0 }, 0.0f, WHITE);
        }
    }
    for (int ty = y0; ty <= y1; ty++) {
        for (int tx = x0; tx <= x1; tx++) {
            GroundTile gt = sampleGround(tx, ty);
            if (gt.deco == 0) continue;
            Texture2D* t = nullptr;
            if (gt.deco == 1) t = &g.assets.tuft[gt.decoVariant % 3];
            else if (gt.deco == 2) t = &g.assets.flowers[gt.decoVariant % 2];
            else t = &g.assets.pebble;
            if (!t || t->id == 0) continue;
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
        if (g.cloudScale[i] == 0) continue;
        float s = g.cloudScale[i] * 0.55f;
        DrawTexturePro(c, { 0, 0, (float)c.width, (float)c.height },
                       { g.cloudX[i], g.cloudY[i], c.width * s, c.height * s }, { 0, 0 }, 0.0f,
                       Fade(WHITE, 0.45f));
    }
}

// ================================================================ field monsters (standalone hook)
void drawFieldMonsters() {
    for (const auto& m : gFieldMonsters) {
        Texture2D& t = g.assets.enemy[m.kind];
        float ph = m.phase + g.totalTime;
        float bob = sinf(ph * 3.0f) * 1.5f;
        float sx = m.x * TILE - g.camX;
        float sy = (m.y + 0.35f) * TILE - g.camY;
        DrawEllipse((int)sx, (int)sy - 1, (float)t.width * 0.28f, 3.0f, Fade(BLACK, 0.25f));
        texFoot(t, sx, sy + bob, WHITE, 0.62f);
        float dx = g.px - m.x, dy = g.py - m.y;
        if (dx * dx + dy * dy < 9.0f) {
            if (fmodf(g.totalTime * 2.0f, 1.0f) < 0.6f)
                drawText("!", sx - 3, sy - t.height * 0.62f - 12, Color{ 255, 90, 90, 255 }, 8);
        }
    }
}

// ================================================================ Y-sorted world layer
enum RenderKind : uint8_t {
    RK_PROP = 0, RK_DECO, RK_BUILDING, RK_LANDMARK, RK_CHEST, RK_TOWER_CHEST,
    RK_LOCKET, RK_NPC, RK_MONSTER, RK_BOSS, RK_PLAYER
};
struct RenderItem {
    float footY;
    RenderKind kind;
    int a, b, c;
    float fx, fy;
};

static char npcQuestMarker(const Npc& n) {
    if (n.quest == QUEST_SLIMES) {
        if (g.quests[QUEST_SLIMES].state == QS_OFFERED) return '!';
        if (g.quests[QUEST_SLIMES].state == QS_ACTIVE && g.slimesSlain >= 6) return '?';
    } else if (n.quest == QUEST_LOCKET) {
        if (g.quests[QUEST_LOCKET].state == QS_OFFERED) return '!';
        if (g.quests[QUEST_LOCKET].state == QS_ACTIVE && (g.locketFound || itemCount(IT_LOCKET) > 0)) return '?';
    } else if (n.quest == QUEST_CROWN) {
        if (g.quests[QUEST_CROWN].state == QS_OFFERED) return '!';
        if (g.quests[QUEST_CROWN].state != QS_DONE && (itemCount(IT_CROWN) > 0 || g.bossDefeated)) return '?';
    }
    return 0;
}

static void drawLandmarkAt(const Landmark& lm) {
    float cx = (lm.tx + 0.5f) * TILE - g.camX;
    float fy = (lm.ty + 1.0f) * TILE - g.camY;
    switch (lm.kind) {
        case LK_RUINS:
            texFoot(g.assets.ruinArch, cx - 18, fy);
            texFoot(g.assets.ruinShrine, cx + 22, fy);
            texFoot(g.assets.pillar, cx - 40, fy);
            break;
        case LK_TOWER:
            DrawEllipse((int)cx, (int)fy - 2, 16, 4, Fade(BLACK, 0.25f));
            texFoot(g.assets.tower, cx, fy);
            break;
        case LK_CASTLE:
            DrawEllipse((int)cx, (int)fy - 4, 44, 7, Fade(BLACK, 0.28f));
            texFoot(g.assets.castle, cx, fy);
            // Flanking stone pillars & torches at the Castle of Elm
            texFoot(g.assets.pillar, cx - 52, fy + 16);
            texFoot(g.assets.pillar, cx + 52, fy + 16);
            texFoot(g.assets.torch, cx - 36, fy + 24);
            texFoot(g.assets.torch, cx + 36, fy + 24);
            break;
        case LK_STONES:
            DrawEllipse((int)cx, (int)fy - 2, 12, 3, Fade(BLACK, 0.22f));
            texFoot(g.assets.statue, cx, fy);
            texFoot(g.assets.pillar, cx - 28, fy + 6);
            texFoot(g.assets.pillar, cx + 28, fy + 6);
            break;
        case LK_CAMP:
            texFoot(g.assets.campfire, cx, fy);
            texFoot(g.assets.bench, cx - 24, fy);
            texFoot(g.assets.cauldron, cx + 24, fy);
            break;
        default: break;
    }
}

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

    // Day/night factor used both for warm door glow and ambient wash
    float dayT = g.totalTime * 0.004f;
    float night = (sinf(dayT * 6.2831853f) + 1.0f) * 0.5f;
    float nightAmt = (night > 0.55f) ? (night - 0.55f) / 0.45f : 0.0f;

    // Collect EVERY world object into one unified Y-sorted list
    static std::vector<RenderItem> list;
    static std::vector<Landmark> visibleLandmarks;
    list.clear();

    // 1. World props
    for (int ty = y0 - 3; ty <= y1 + 4; ty++) {
        for (int tx = x0 - 3; tx <= x1 + 3; tx++) {
            PropKind p = propAt(tx, ty);
            if (p == P_NONE) continue;
            list.push_back({ (ty + 1.0f) * TILE, RK_PROP, (int)p, tx, ty, 0, 0 });
        }
    }

    // 2. Landmarks
    landmarksInView(x0, y0, x1, y1, visibleLandmarks);
    for (int i = 0; i < (int)visibleLandmarks.size(); i++) {
        list.push_back({ (visibleLandmarks[i].ty + 1.0f) * TILE, RK_LANDMARK, i, 0, 0, 0, 0 });
    }

    // 3. Village decos & buildings
    if (g.villageLoaded) {
        for (int i = 0; i < (int)g.village.decos.size(); i++) {
            const auto& d = g.village.decos[i];
            if (d.x < x0 - 4 || d.x > x1 + 4 || d.y < y0 - 4 || d.y > y1 + 4) continue;
            list.push_back({ (d.y + 1.0f) * TILE, RK_DECO, d.kind, d.variant, 0, d.x, d.y });
        }
        for (int i = 0; i < (int)g.village.buildings.size(); i++) {
            const auto& b = g.village.buildings[i];
            if (b.tx < x0 - 8 || b.tx > x1 + 8 || b.ty < y0 - 8 || b.ty > y1 + 8) continue;
            list.push_back({ (b.ty + 1.0f) * TILE, RK_BUILDING, i, 0, 0, 0, 0 });
        }
    }

    // 4. Procedural chests
    for (int i = 0; i < (int)gChestsInView.size(); i++) {
        const auto& c = gChestsInView[i];
        list.push_back({ (c.ty + 1.0f) * TILE, RK_CHEST, i, 0, 0, 0, 0 });
    }

    // 5. Watchtower locked chest
    if (TOWER_CHEST_TX >= x0 - 2 && TOWER_CHEST_TX <= x1 + 2 &&
        TOWER_CHEST_TY >= y0 - 2 && TOWER_CHEST_TY <= y1 + 2) {
        list.push_back({ (TOWER_CHEST_TY + 1.0f) * TILE, RK_TOWER_CHEST, 0, 0, 0, 0, 0 });
    }

    // 6. Lost Locket glint
    if (!g.locketFound &&
        LOCKET_PICKUP_X >= x0 - 2 && LOCKET_PICKUP_X <= x1 + 2 &&
        LOCKET_PICKUP_Y >= y0 - 2 && LOCKET_PICKUP_Y <= y1 + 2) {
        list.push_back({ (LOCKET_PICKUP_Y + 0.35f) * TILE, RK_LOCKET, 0, 0, 0, LOCKET_PICKUP_X, LOCKET_PICKUP_Y });
    }

    // 7. NPCs
    for (int i = 0; i < (int)g.npcs.size(); i++) {
        list.push_back({ (g.npcs[i].y + 0.35f) * TILE, RK_NPC, i, 0, 0, 0, 0 });
    }

    // 8. Field monsters
    for (int i = 0; i < (int)gFieldMonsters.size(); i++) {
        list.push_back({ (gFieldMonsters[i].y + 0.35f) * TILE, RK_MONSTER, i, 0, 0, 0, 0 });
    }

    // 9. Slime King field boss
    if (!g.bossDefeated &&
        BOSS_KING_X >= x0 - 4 && BOSS_KING_X <= x1 + 4 &&
        BOSS_KING_Y >= y0 - 4 && BOSS_KING_Y <= y1 + 4) {
        list.push_back({ (BOSS_KING_Y + 0.45f) * TILE, RK_BOSS, 0, 0, 0, BOSS_KING_X, BOSS_KING_Y });
    }

    // 10. Player
    list.push_back({ (g.py + 0.35f) * TILE, RK_PLAYER, 0, 0, 0, 0, 0 });

    std::stable_sort(list.begin(), list.end(),
                     [](const RenderItem& a, const RenderItem& b) { return a.footY < b.footY; });

    for (const auto& it : list) {
        switch (it.kind) {
            case RK_PROP: {
                int w = 0, h = 0;
                Texture2D* t = propTexture((PropKind)it.a, &w, &h);
                if (!t || t->id == 0) break;
                float fx = (it.b + 0.5f) * TILE - g.camX;
                float fy = (it.c + 1.0f) * TILE - g.camY;
                DrawEllipse((int)fx, (int)fy - 2, w * 0.28f, 3.0f, Fade(BLACK, 0.18f));
                texFoot(*t, fx, fy);
                break;
            }
            case RK_LANDMARK:
                drawLandmarkAt(visibleLandmarks[it.a]);
                break;
            case RK_DECO: {
                int w = 0, h = 0;
                Texture2D* t = nullptr;
                villageDecoTexture(it.a, it.b, t, w, h);
                if (!t || t->id == 0) break;
                float cx = (it.fx + 0.5f) * TILE - g.camX;
                float fy = (it.fy + 1.0f) * TILE - g.camY;
                DrawEllipse((int)cx, (int)fy - 1, w * 0.30f, 2.5f, Fade(BLACK, 0.18f));
                texFoot(*t, cx, fy);
                if (it.a == VD_TORCH || it.a == VD_CAMPFIRE) {
                    float pulse = 0.5f + 0.5f * sinf(g.totalTime * 4.5f + it.fx);
                    DrawCircleGradient((int)cx, (int)(fy - h * 0.5f), 22 + pulse * 6,
                                       Fade(Color{ 255, 170, 60, 255 }, 0.22f + nightAmt * 0.18f), BLANK);
                }
                break;
            }
            case RK_BUILDING: {
                const Building& b = g.village.buildings[it.a];
                Texture2D* t = buildingTexture(b);
                if (!t || t->id == 0) break;
                float cx = (b.tx + 0.5f) * TILE - g.camX;
                float fy = (b.ty + 1.0f) * TILE - g.camY;
                DrawEllipse((int)cx, (int)fy - 3, b.w * TILE * 0.44f, 5.0f, Fade(BLACK, 0.22f));
                texFoot(*t, cx, fy);
                if (nightAmt > 0.05f) {
                    DrawCircleGradient((int)cx, (int)(fy - 12), 26.0f,
                                       Fade(Color{ 255, 200, 100, 255 }, 0.28f * nightAmt), BLANK);
                }
                break;
            }
            case RK_CHEST: {
                const Chest& c = gChestsInView[it.a];
                bool opened = g.openedChests.count(c.id) > 0;
                Texture2D& t = opened ? g.assets.chestOpen : g.assets.chestClosed;
                float cx = (c.tx + 0.5f) * TILE - g.camX;
                float fy = (c.ty + 1.0f) * TILE - g.camY;
                DrawEllipse((int)cx, (int)fy - 1, 6.0f, 2.5f, Fade(BLACK, 0.25f));
                texFoot(t, cx, fy);
                break;
            }
            case RK_TOWER_CHEST: {
                Texture2D& t = g.towerChestOpened ? g.assets.chestOpen : g.assets.chestClosed;
                float cx = (TOWER_CHEST_TX + 0.5f) * TILE - g.camX;
                float fy = (TOWER_CHEST_TY + 1.0f) * TILE - g.camY;
                DrawEllipse((int)cx, (int)fy - 1, 7.0f, 3.0f, Fade(BLACK, 0.28f));
                texFoot(t, cx, fy, g.towerChestOpened ? WHITE : Color{ 255, 238, 190, 255 });
                if (!g.towerChestOpened) {
                    float pulse = 0.5f + 0.5f * sinf(g.totalTime * 4.0f);
                    DrawCircleGradient((int)cx, (int)(fy - 8), 14 + pulse * 4,
                                       Fade(Color{ 255, 220, 120, 255 }, 0.30f), BLANK);
                }
                break;
            }
            case RK_LOCKET: {
                float cx = it.fx * TILE - g.camX;
                float fy = (it.fy + 0.35f) * TILE - g.camY;
                float pulse = 0.5f + 0.5f * sinf(g.totalTime * 5.0f);
                DrawCircleGradient((int)cx, (int)(fy - 4), 12 + pulse * 5,
                                   Fade(Color{ 180, 230, 255, 255 }, 0.45f), BLANK);
                drawItemIcon(IT_LOCKET, cx - 6, fy - 11 - pulse * 2.0f, 12);
                break;
            }
            case RK_NPC: {
                const Npc& n = g.npcs[it.a];
                Texture2D* sheet = charsetFor(n.sprite);
                if (!sheet || sheet->id == 0) break;
                int row = n.facing;
                int frame = n.moving ? (1 + ((int)(n.animT * 6.0f) & 1)) : 0;
                float cx = n.x * TILE - g.camX;
                float fy = (n.y + 0.35f) * TILE - g.camY;
                DrawEllipse((int)cx, (int)fy - 2, 5.5f, 2.5f, Fade(BLACK, 0.24f));
                drawCharSheet(*sheet, row, frame, cx, fy, n.facing == 2 && n.sideFlip);
                char qm = npcQuestMarker(n);
                if (qm != 0) {
                    float bob = sinf(g.totalTime * 4.5f) * 2.0f;
                    Color qc = (qm == '?') ? Color{ 120, 255, 160, 255 } : Color{ 255, 225, 90, 255 };
                    char qbuf[2] = { qm, 0 };
                    drawTextCentered(qbuf, cx + 1, fy - 41 + bob, Color{ 12, 16, 30, 255 }, 8);
                    drawTextCentered(qbuf, cx, fy - 42 + bob, qc, 8);
                }
                break;
            }
            case RK_MONSTER: {
                const FieldMonster& m = gFieldMonsters[it.a];
                Texture2D& t = g.assets.enemy[m.kind];
                float ph = m.phase + g.totalTime;
                float bob = sinf(ph * 3.0f) * 1.5f;
                float sx = m.x * TILE - g.camX;
                float sy = (m.y + 0.35f) * TILE - g.camY;
                DrawEllipse((int)sx, (int)sy - 1, (float)t.width * 0.28f, 3.0f, Fade(BLACK, 0.25f));
                texFoot(t, sx, sy + bob, WHITE, 0.62f);
                float dx = g.px - m.x, dy = g.py - m.y;
                if (dx * dx + dy * dy < 9.0f && fmodf(g.totalTime * 2.0f, 1.0f) < 0.6f)
                    drawText("!", sx - 3, sy - t.height * 0.62f - 12, Color{ 255, 90, 90, 255 }, 8);
                break;
            }
            case RK_BOSS: {
                Texture2D& t = g.assets.enemy[EN_SLIME_KING];
                float bob = sinf(g.totalTime * 2.8f) * 2.0f;
                float sx = it.fx * TILE - g.camX;
                float sy = (it.fy + 0.45f) * TILE - g.camY;
                DrawCircleGradient((int)sx, (int)(sy - 16), 28, Fade(Color{ 120, 255, 160, 255 }, 0.22f), BLANK);
                DrawEllipse((int)sx, (int)sy - 1, 18.0f, 5.0f, Fade(BLACK, 0.32f));
                texFoot(t, sx, sy + bob, WHITE, 0.92f);
                drawTextCentered("SLIME KING", sx, sy - 46 + bob, Color{ 255, 224, 120, 255 }, 8);
                break;
            }
            case RK_PLAYER: {
                int klass = g.party[0].klass;
                Texture2D& sheet = g.assets.heroWalk[std::max(0, std::min(2, klass))];
                int row = (g.facing == 0) ? 0 : (g.facing == 1 ? 1 : 2);
                int frame = g.moving ? (1 + ((int)(g.walkAnimT * 7.0f) & 1)) : 0;
                float cx = g.px * TILE - g.camX;
                float fy = (g.py + 0.35f) * TILE - g.camY;
                if (g.interactT > 0) fy -= sinf((0.25f - g.interactT) * 12.0f) * 2.0f;
                DrawEllipse((int)cx, (int)fy - 2, 6.0f, 3.0f, Fade(BLACK, 0.28f));
                drawCharSheet(sheet, row, frame, cx, fy, g.facing == 2 && g.sideFlip);
                break;
            }
        }
    }

    // click marker
    if (g.clickFlashT > 0) {
        float a = g.clickFlashT / 0.4f;
        float cx = g.clickFlashX * TILE - g.camX;
        float cy = g.clickFlashY * TILE - g.camY;
        DrawRing({ cx, cy }, 4 + (1 - a) * 6, 6 + (1 - a) * 6, 0, 360, 20, Fade(Color{ 255, 240, 180, 255 }, a));
    }
    // blocked bump
    if (g.blockedFx > 0) {
        float cx = g.px * TILE - g.camX;
        float cy = g.py * TILE - g.camY;
        DrawRing({ cx, cy }, 10, 12, 0, 360, 16, Fade(Color{ 255, 120, 120, 255 }, g.blockedFx * 0.5f));
    }

    drawClouds();

    if (nightAmt > 0.001f) {
        DrawRectangle(0, 0, VRES_W, VRES_H, Fade(Color{ 20, 26, 60, 255 }, nightAmt * 0.40f));
    }

    g.camX = saveCamX; g.camY = saveCamY;
}

// ================================================================ HUD
void drawHUD() {
    // place label (dynamically sized to fit any zone name)
    const char* zone = zoneNameAt(g.px, g.py);
    Vector2 zsz = measureText(zone, 8);
    float zw = std::max(120.0f, zsz.x + 18.0f);
    drawWindow({ 4, 4, zw, 22 });
    drawText(zone, 12, 11, Color{ 236, 240, 255, 255 }, 8);

    // party vitals, bottom-left (wider so names never overlap bars)
    float y = VRES_H - 60;
    drawWindow({ 4, y - 4, 198, 60 });
    for (int i = 0; i < 3; i++) {
        const Hero& h = g.party[i];
        float hy = y + 2 + i * 18;
        Texture2D& f = g.assets.face[i == 0 ? FACE_KNIGHT : (i == 1 ? FACE_MAGE : FACE_CLERIC)];
        if (f.id) DrawTexturePro(f, { 0, 0, (float)f.width, (float)f.height }, { 9, hy - 1, 15, 15 },
                                 { 0, 0 }, 0.0f, h.ko ? Color{ 130, 130, 160, 255 } : WHITE);
        char buf[64];
        snprintf(buf, sizeof(buf), "%-6s L%d", heroName(h.klass), h.level);
        Color nc = h.ko ? Color{ 220, 120, 120, 255 }
                 : (h.poisoned ? Color{ 195, 145, 255, 255 } : Color{ 236, 240, 255, 255 });
        drawText(buf, 27, hy + 2, nc, 8);
        drawBar(116, hy + 3, 48, 5, (float)h.hp / std::max(1, h.maxHp),
                h.ko ? Color{ 120, 60, 60, 255 } : Color{ 96, 220, 120, 255 }, Color{ 30, 40, 80, 255 });
        drawBar(168, hy + 3, 28, 5, (float)h.mp / std::max(1, h.maxMp),
                Color{ 96, 160, 255, 255 }, Color{ 30, 40, 80, 255 });
    }

    // gil, top-right
    drawWindow({ VRES_W - 104, 4, 100, 22 });
    Texture2D& coin = g.assets.icon[ICON_COIN];
    if (coin.id) DrawTexturePro(coin, { 0, 0, (float)coin.width, (float)coin.height },
                               { VRES_W - 97.0f, 8, 13, 13 }, { 0, 0 }, 0.0f, WHITE);
    char buf[64];
    snprintf(buf, sizeof(buf), "%d", g.gil);
    drawText(buf, VRES_W - 80.0f, 11, Color{ 255, 232, 150, 255 }, 8);

    // active quest tracker with progress / compass hint
    for (int q = 0; q < QUEST_COUNT; q++) {
        if (g.quests[q].state != QS_ACTIVE) continue;
        std::string line = questTitle(q);
        if (q == QUEST_SLIMES) {
            snprintf(buf, sizeof(buf), " (%d/6)", std::min(6, g.slimesSlain));
            line += buf;
        } else if (q == QUEST_LOCKET) {
            if (g.locketFound) line += " (Return)";
            else line += " [" + compassHint(g.px, g.py, LOCKET_PICKUP_X, LOCKET_PICKUP_Y) + "]";
        } else if (q == QUEST_CROWN) {
            if (itemCount(IT_CROWN) > 0 || g.bossDefeated) line += " (Return)";
            else line += " [" + compassHint(g.px, g.py, (float)BOSS_CASTLE_TX, (float)BOSS_CASTLE_TY) + "]";
        }
        Vector2 sz = measureText(line.c_str(), 8);
        drawWindow({ VRES_W - sz.x - 18, 30, sz.x + 14, 20 });
        drawText(line.c_str(), VRES_W - sz.x - 11, 36, Color{ 255, 232, 150, 255 }, 8);
        break;
    }

    // interaction prompt (positioned clearly to the right of the party HUD or centered above it)
    const char* prompt = interactionPrompt();
    if (prompt) {
        Vector2 sz = measureText(prompt, 8);
        float pw = sz.x + 16;
        float px = std::max(208.0f, VRES_W * 0.5f - pw * 0.5f);
        if (px + pw > VRES_W - 4) px = VRES_W - 4 - pw;
        drawWindow({ px, VRES_H - 26, pw, 20 });
        drawTextCentered(prompt, px + pw * 0.5f, VRES_H - 20, Color{ 236, 240, 255, 255 }, 8);
    }
    drawToast();
}

void drawToast() {
    if (g.toastT <= 0 || g.toast.empty()) return;
    Vector2 sz = measureText(g.toast.c_str(), 8);
    float a = std::min(1.0f, g.toastT / 0.4f);
    float w = std::min((float)(VRES_W - 16), sz.x + 20), h = 22;
    float x = VRES_W * 0.5f - w * 0.5f, y = 32;
    DrawRectangleRounded({ x, y, w, h }, 0.4f, 4, Fade(Color{ 12, 16, 40, 255 }, 0.90f * a));
    DrawRectangleRoundedLinesEx({ x, y, w, h }, 0.4f, 4, 1.0f, Fade(Color{ 255, 220, 140, 255 }, a));
    drawTextCentered(g.toast.c_str(), VRES_W * 0.5f, y + 7, Fade(WHITE, a), 8);
}

// ================================================================ dialog box
void drawDialogBox() {
    if (g.dialog.empty()) return;
    const DialogLine& line = g.dialog[std::min<int>(g.dialogIndex, (int)g.dialog.size() - 1)];
    float h = 80;
    float y = VRES_H - h - 6;
    drawWindow({ 6, y, VRES_W - 12, h });
    if (line.face >= 0 && line.face < FACE_COUNT && g.assets.face[line.face].id) {
        Texture2D& f = g.assets.face[line.face];
        DrawTexturePro(f, { 0, 0, (float)f.width, (float)f.height }, { 14, y + 10, 48, 48 },
                       { 0, 0 }, 0.0f, WHITE);
        DrawRectangleLines(14, (int)y + 10, 48, 48, Color{ 255, 232, 150, 255 });
    }
    float tx = (line.face >= 0) ? 72.0f : 18.0f;
    float maxW = VRES_W - tx - 18.0f;
    if (!line.speaker.empty())
        drawText(line.speaker.c_str(), tx, y + 8, Color{ 255, 232, 150, 255 }, 8);

    // typewriter reveal with pixel-accurate word wrapping
    size_t nchars = (size_t)std::max(0.0f, std::min((float)line.text.size(), g.dialogReveal));
    std::string shown = line.text.substr(0, nchars);
    drawTextWrapped(shown.c_str(), tx, y + (line.speaker.empty() ? 12.0f : 22.0f), maxW, 12.0f, WHITE, 8);

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
    else ClearBackground(Color{ 12, 18, 40, 255 });

    drawClouds();

    DrawRectangleGradientV(0, 0, VRES_W, VRES_H / 2, Fade(BLACK, 0.35f), BLANK);
    DrawRectangleGradientV(0, VRES_H / 2, VRES_W, VRES_H / 2, BLANK, Fade(BLACK, 0.55f));

    const char* title = "A PLACE FOR AGENTS";
    float bob = sinf(g.titleT * 1.6f) * 1.5f;
    drawTextCentered(title, VRES_W * 0.5f + 2, 54 + bob, Color{ 12, 16, 30, 255 }, 24);
    drawTextCentered(title, VRES_W * 0.5f,     52 + bob, Color{ 255, 236, 176, 255 }, 24);
    drawTextCentered("an endless meadow tale", VRES_W * 0.5f + 1, 85 + bob, Color{ 12, 16, 30, 200 }, 8);
    drawTextCentered("an endless meadow tale", VRES_W * 0.5f,     84 + bob, Color{ 230, 238, 255, 230 }, 8);

    const char* opts[3] = { "NEW ADVENTURE", "CONTINUE", "QUIT" };
    for (int i = 0; i < 3; i++) {
        bool sel = (g.titleIndex == i);
        bool disabled = (i == 1 && !g.saveExists);
        float w = 156;
        Rectangle box{ VRES_W * 0.5f - w * 0.5f, (float)(146 + i * 24), w, 19 };
        if (sel) {
            DrawRectangleRounded(box, 0.5f, 4, Fade(Color{ 20, 30, 80, 255 }, 0.90f));
            DrawRectangleRoundedLinesEx(box, 0.5f, 4, 1.2f, Color{ 255, 236, 176, 255 });
        } else {
            DrawRectangleRounded(box, 0.5f, 4, Fade(Color{ 10, 16, 40, 255 }, 0.45f));
        }
        Color c = disabled ? Color{ 150, 160, 185, 190 }
                           : (sel ? Color{ 255, 236, 176, 255 } : Color{ 235, 240, 255, 245 });
        drawTextCentered(opts[i], VRES_W * 0.5f + 1, box.y + 6, Fade(BLACK, 0.65f), 8);
        drawTextCentered(opts[i], VRES_W * 0.5f,     box.y + 5, c, 8);
    }
    drawTextCentered("arrows / mouse to choose - SPACE to begin",
                     VRES_W * 0.5f, VRES_H - 26, Color{ 210, 220, 245, 220 }, 8);
    drawTextCentered("art: OpenRTP - press F11 for fullscreen",
                     VRES_W * 0.5f, VRES_H - 14, Color{ 180, 190, 220, 180 }, 8);
    drawToast();
}

// ================================================================ game over
void drawGameOverScreen() {
    if (g.assets.gameoverPlate.id)
        DrawTexturePro(g.assets.gameoverPlate, { 0, 0, (float)g.assets.gameoverPlate.width,
                                                (float)g.assets.gameoverPlate.height },
                       { 0, 0, VRES_W, VRES_H }, { 0, 0 }, 0.0f, WHITE);
    else ClearBackground(Color{ 18, 8, 12, 255 });
    DrawRectangle(0, 0, VRES_W, VRES_H, Fade(Color{ 20, 4, 8, 255 }, 0.40f));
    drawTextCentered("THE PARTY HAS FALLEN", VRES_W * 0.5f + 2, 98, Color{ 16, 4, 8, 255 }, 24);
    drawTextCentered("THE PARTY HAS FALLEN", VRES_W * 0.5f,     96, Color{ 255, 210, 210, 255 }, 24);
    drawTextCentered("the meadow keeps its own", VRES_W * 0.5f, 132, Color{ 230, 220, 230, 220 }, 8);
    if (fmodf(g.totalTime * 2.0f, 2.0f) < 1.2f)
        drawTextCentered("SPACE or CLICK: return to title", VRES_W * 0.5f, VRES_H - 52,
                         Color{ 255, 236, 176, 255 }, 8);
}

// ================================================================ intro
void drawIntroScreen() {
    if (g.assets.dawn.id)
        DrawTexturePro(g.assets.dawn, { 0, 0, (float)g.assets.dawn.width, (float)g.assets.dawn.height },
                       { 0, 0, VRES_W, VRES_H }, { 0, 0 }, 0.0f, WHITE);
    else ClearBackground(Color{ 30, 40, 70, 255 });
    DrawRectangle(0, 0, VRES_W, VRES_H, Fade(Color{ 6, 10, 24, 255 }, 0.48f));

    const char* lines[] = {
        "Every map you were given ends here.",
        "The road does not. Beyond the meadow",
        "the wilds roll on without a border,",
        "and the Slime King keeps the crown",
        "of the first village.",
        "",
        "You are three, and you have one sword.",
    };
    float drift = std::min(18.0f, g.introT * 4.5f);
    float startY = 74.0f - drift;
    for (int i = 0; i < 7; i++) {
        const char* l = lines[i];
        if (l[0] == 0) continue;
        float alpha = std::max(0.0f, std::min(1.0f, (g.introT - i * 0.35f) * 1.4f));
        if (alpha <= 0.01f) continue;
        bool isTag = (i == 6);
        Color col = isTag ? Color{ 255, 236, 176, 255 } : Color{ 228, 234, 252, 255 };
        float y = startY + i * 20.0f + (isTag ? 6.0f : 0.0f);
        drawTextCentered(l, VRES_W * 0.5f + 1, y + 1, Fade(BLACK, 0.7f * alpha), 8);
        drawTextCentered(l, VRES_W * 0.5f,     y,     Fade(col, alpha), 8);
    }
    if (g.introT > 1.2f && fmodf(g.totalTime * 2.0f, 2.0f) < 1.2f)
        drawTextCentered("SPACE or CLICK to begin", VRES_W * 0.5f, VRES_H - 26,
                         Color{ 255, 236, 176, 255 }, 8);
}

// ================================================================ transition fx
void drawTransitionFx() {
    if (g.fade <= 0.001f) return;
    DrawRectangle(0, 0, VRES_W, VRES_H, Fade(BLACK, g.fade));
}
