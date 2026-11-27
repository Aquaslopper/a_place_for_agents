// render.cpp — the field renderer, HUD and mode screens
#include "game.hpp"
#include "world.hpp"
#include "regions.hpp"
#include "battle.hpp"
#include "commons.hpp"
#include "cinema.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

// ================================================================ helpers
static void texFoot(Texture2D t, float cx, float footY, Color tint = WHITE, float scale = 1.0f) {
    if (t.id == 0) return;
    DrawTexturePro(t, { 0, 0, (float)t.width, (float)t.height },
                   { floorf(cx - t.width * scale * 0.5f), floorf(footY - t.height * scale),
                     (float)t.width * scale, (float)t.height * scale },
                   { 0, 0 }, 0.0f, tint);
}

static void drawBackdropCover(Texture2D& tex) {
    if (tex.id == 0) return;
    float scale = std::max((float)g.vw / (float)tex.width, (float)g.vh / (float)tex.height);
    float dw = roundf((float)tex.width * scale);
    float dh = roundf((float)tex.height * scale);
    float dx = floorf(((float)g.vw - dw) * 0.5f);
    float dy = floorf(((float)g.vh - dh) * 0.5f);
    DrawTexturePro(tex, { 0, 0, (float)tex.width, (float)tex.height },
                   { dx, dy, dw, dh }, { 0, 0 }, 0.0f, WHITE);
}

static void drawCharSheet(Texture2D& sheet, int row, int frame, float cx, float footY, bool flip,
                          Color tint = WHITE) {
    if (sheet.id == 0) return;
    int cols = sheet.width / 24;
    int rows = sheet.height / 32;
    if (cols < 3 || rows < 1) return;
    row = std::max(0, std::min(rows - 1, row));
    frame = std::max(0, std::min(2, frame));
    Rectangle src{ (float)(frame * 24), (float)(row * 32), 24, 32 };
    if (flip) src.width = -24;
    Rectangle dst{ floorf(cx - 12.0f), floorf(footY - 32.0f), 24, 32 };
    DrawTexturePro(sheet, src, dst, { 0, 0 }, 0.0f, tint);
}

static Texture2D* buildingTexture(const Building& b) {
    switch (b.kind) {
        case 5: return &g.assets.shop;
        case 6: return &g.assets.inn;
        case 7: return &g.assets.temple;
        case 8: return &g.assets.manor;
        case 9: return &g.assets.castle;
        case 10: return &g.assets.smithy;
        default: return &g.assets.house[std::max(0, std::min(4, b.kind))];
    }
}

static Texture2D* charsetFor(int sprite) {
    if (sprite < 0 || sprite >= NS_KIND_COUNT) return &g.assets.npc[0];
    return &g.assets.npc[sprite];
}

// ================================================================ ground
enum Surface {
    S_DEEP, S_WATER, S_BEACH, S_MEADOW, S_WOOD, S_AMBER, S_HEATH, S_FEN,
    S_DUNES, S_SNOW, S_ICE, S_CRAG, S_PATH, S_SOIL, S_COBBLE, S_COURT,
    S_LEAVES, S_RIPPLES, S_MESA, S_FROST_TRACK, S_SNOW_CRAG, S_COUNT
};
static Surface surface(GroundTile t) {
    switch (t.kind) {
        case G_DEEP: return S_DEEP;
        case G_WATER: case G_BRIDGE_NS: case G_BRIDGE_EW: return S_WATER;
        case G_BEACH: return S_BEACH;
        case G_GRASS_DARK: return S_WOOD;
        case G_AMBER: return S_AMBER;
        case G_HEATH: return S_HEATH;
        case G_SWAMP: return S_FEN;
        case G_SAND: return S_DUNES;
        case G_SNOW: return S_SNOW;
        case G_ICE: return S_ICE;
        case G_ROCK: return t.biome == BI_DUNES ? S_MESA : t.biome == BI_FROST ? S_SNOW_CRAG : S_CRAG;
        case G_DIRT: return t.biome == BI_FROST ? S_FROST_TRACK : S_PATH;
        case G_LEAVES: return S_LEAVES;
        case G_RIPPLES: return S_RIPPLES;
        case G_SOIL: return S_SOIL;
        case G_COBBLE: return S_COBBLE;
        case G_FLOOR: return S_COURT;
        default: return S_MEADOW;
    }
}
static void stamp(Texture2D atlas, int mask, int row, int x, int y) {
    DrawTextureRec(atlas, { (float)(mask * TILE), (float)(row * TILE), TILE, TILE }, { (float)x, (float)y }, WHITE);
}

static void drawGround(int x0, int y0, int x1, int y1) {
    int w = x1 - x0 + 3, h = y1 - y0 + 3;
    static std::vector<GroundTile> view;
    static int lastX, lastY, lastW, lastH;
    static uint32_t seed;
    static uint64_t village;
    static int lastAccess = -1;
    uint64_t key = g.villageLoaded ? g.villageKey : ~0ull;
    int access = (regionPassOpen(1) ? 1 : 0) | (regionPassOpen(2) ? 2 : 0);
    auto at = [&](int x, int y) -> GroundTile& { return view[(y - y0 + 1) * w + x - x0 + 1]; };
    if (lastW != w || lastH != h || lastX != x0 || lastY != y0 || seed != g.worldSeed ||
        key != village || access != lastAccess) {
        view.resize(w * h);
        lastW = w; lastH = h; lastX = x0; lastY = y0; seed = g.worldSeed; village = key; lastAccess = access;
        for (int y = y0 - 1; y <= y1 + 1; ++y)
            for (int x = x0 - 1; x <= x1 + 1; ++x)
                at(x, y) = sampleGround(x, y);

        auto pave = [&](const std::vector<std::pair<int, int>>& cells, GroundKind mat, uint8_t deco = 0) {
            for (auto [x, y] : cells) {
                if (x >= x0 - 1 && y >= y0 - 1 && x <= x1 + 1 && y <= y1 + 1) {
                    auto& t = at(x, y);
                    if (surface(t) <= S_WATER) continue;
                    t.kind = mat;
                    t.deco = deco;
                }
            }
        };
        if (g.villageLoaded) {
            pave(g.village.grassDark, G_GRASS_DARK);
            pave(g.village.rocks, G_ROCK);
            pave(g.village.paths, G_DIRT);
            pave(g.village.plaza, G_COBBLE);
            pave(g.village.court, G_FLOOR);
            pave(g.village.soil, G_SOIL, 5);
            for (auto [x, y] : g.village.flowers) {
                if (x >= x0 - 1 && y >= y0 - 1 && x <= x1 + 1 && y <= y1 + 1) {
                    auto& t = at(x, y);
                    if (t.kind == G_GRASS || t.kind == G_GRASS_DARK) {
                        t.deco = 2;
                        t.decoVariant = (uint8_t)((abs(x) + abs(y)) & 1);
                    }
                }
            }
        }
    }
    int anim = (int)(g.totalTime * 2.5f) % 3;
    int camX = (int)roundf(g.camX), camY = (int)roundf(g.camY);
    for (int y = y0; y <= y1; ++y) for (int x = x0; x <= x1; ++x) {
        Surface corners[] = { surface(at(x, y)), surface(at(x + 1, y)),
                              surface(at(x, y + 1)), surface(at(x + 1, y + 1)) };
        int sx = x * TILE + TILE / 2 - camX, sy = y * TILE + TILE / 2 - camY;
        int phase = at(x, y).variant % 3;
        auto paint = [&](int s, int mask) { stamp(g.assets.landscape, mask, s * 3 + (s <= S_WATER ? anim : phase), sx, sy); };
        if (corners[0] == corners[1] && corners[0] == corners[2] && corners[0] == corners[3]) {
            paint(corners[0], 15);
            continue;
        }
        int masks[S_COUNT] = {};
        for (int i = 0; i < 4; ++i) masks[corners[i]] |= 1 << i;
        int left = 15;
        for (int s = 0; s < S_COUNT; ++s) if (masks[s]) { paint(s, left); left &= ~masks[s]; }
        int dry = 15 ^ (masks[S_WATER] | masks[S_DEEP]);
        if (dry && dry != 15) stamp(g.assets.relief, dry, (masks[S_SNOW] | masks[S_ICE]) ? 1 : 0, sx, sy);
        int ledge = masks[S_CRAG] | masks[S_MESA] | masks[S_SNOW_CRAG];
        if (ledge) stamp(g.assets.relief, ledge, masks[S_MESA] ? 3 : masks[S_SNOW_CRAG] ? 4 : 2, sx, sy);
    }
    for (int y = y0; y <= y1; ++y) for (int x = x0; x <= x1; ++x) {
        auto t = at(x, y);
        int sx = x * TILE - camX, sy = y * TILE - camY;
        bool ns = t.kind == G_BRIDGE_NS;
        if (ns || t.kind == G_BRIDGE_EW) {
            Texture2D& br = g.assets.bridge;
            if (br.id) {
                float rot = ns ? 0.f : 90.f;
                DrawTexturePro(br, { 0, 0, (float)br.width, (float)br.height },
                               { (float)(sx + TILE / 2), (float)(sy + TILE / 2), (float)br.width, (float)br.height },
                               { (float)br.width * 0.5f, (float)br.height * 0.5f }, rot, WHITE);
            }
            for (int s : { -1, 1 }) {
                if (at(x + (ns ? s : 0), y + (ns ? 0 : s)).kind == t.kind) continue;
                int rx = sx + (ns && s == 1 ? TILE - 3 : 0), ry = sy + (!ns && s == 1 ? TILE - 3 : 0);
                DrawRectangle(rx, ry, ns ? 3 : 16, ns ? 16 : 3, Color{ 89, 65, 51, 255 });
                DrawRectangle(rx + 1, ry + 1, ns ? 1 : 14, ns ? 14 : 1, Color{ 197, 164, 111, 255 });
                DrawRectangle(rx, ry - 2, 4, 5, Color{ 111, 78, 52, 255 });
                DrawRectangle(rx, ry - 2, 3, 2, Color{ 224, 192, 137, 255 });
            }
        } else if (t.deco) {
            Texture2D tex = t.deco == 1 ? g.assets.tuft[t.decoVariant % 3]
                          : t.deco == 2 ? g.assets.flowers[t.decoVariant % 2]
                          : t.deco == 3 ? g.assets.pebble
                          : t.deco == 4 ? g.assets.lilies
                                        : g.assets.turnips;
            DrawTexture(tex, sx + (TILE - tex.width) / 2, sy + TILE - tex.height, WHITE);
        }
        if (t.kind == G_WATER || t.kind == G_DEEP) {
            float tw = hashf(x, y, g.worldSeed ^ 0x51abU);
            float ph = fmodf(g.totalTime * 0.9f + tw * 7.f, 1.f);
            if (ph < 0.16f) {
                int gx = sx + 3 + (int)(tw * 10.f), gy = sy + 4 + (int)(hashf(y, x, g.worldSeed ^ 0x77cdU) * 8.f);
                DrawRectangle(gx, gy, 2, 1, Fade(Color{ 235, 250, 255, 255 }, 0.55f * (1.f - ph / 0.16f)));
            }
        }
    }
}

// ================================================================ clouds
void updateClouds(float dt) {
    for (int i = 0; i < 6; i++) {
        if (g.cloudScale[i] == 0) {
            g.cloudScale[i] = 0.6f + hashf(i, 3, 11u) * 0.7f;
            g.cloudX[i] = hashf(i, 5, 13u) * (g.vw + 200) - 100;
            g.cloudY[i] = hashf(i, 7, 17u) * 140 - 20;
        }
        g.cloudX[i] += dt * (5.0f + i * 1.7f) * g.cloudScale[i];
        if (g.cloudX[i] > g.vw + 140) g.cloudX[i] = -140;
    }
}
static void drawClouds(bool shadows = false) {
    Texture2D& c = g.assets.cloud;
    if (c.id == 0) return;
    for (int i = 0; i < 6; i++) {
        if (g.cloudScale[i] == 0) continue;
        float s = g.cloudScale[i] * 0.55f;
        DrawTexturePro(c, { 0, 0, (float)c.width, (float)c.height },
                       { g.cloudX[i], g.cloudY[i], c.width * s, c.height * s }, { 0, 0 }, 0.0f,
                       shadows ? Color{ 39, 54, 75, 20 } : Fade(WHITE, 0.45f));
    }
}

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
        if (dx * dx + dy * dy < 9.0f && fmodf(g.totalTime * 2.0f, 1.0f) < 0.6f)
            drawWorldLabel("!", sx, sy - t.height * 0.62f - 13, Color{ 255, 110, 110, 255 }, 8);
    }
}

// ================================================================ Y-sorted world layer
enum RenderKind : uint8_t {
    RK_PROP = 0, RK_DECO, RK_BUILDING, RK_LANDMARK_PART, RK_CHEST, RK_TOWER_CHEST,
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
    } else if (n.quest == QUEST_DEEP) {
        if (g.quests[QUEST_DEEP].state == QS_OFFERED) return '!';
        if (g.quests[QUEST_DEEP].state == QS_ACTIVE && itemCount(IT_STAR_METAL) > 0) return '?';
    } else if (n.quest == QUEST_COMMONS) {
        if (g.quests[QUEST_COMMONS].state == QS_OFFERED) return '!';
        if (g.quests[QUEST_COMMONS].state == QS_ACTIVE &&
            g.quests[QUEST_COMMONS].progress >= 2) return '?';
    }
    return 0;
}

static void drawLandmarkPart(const LandmarkPart& p, float nightAmt) {
    float cx = ((p.spanW % 2 == 0) ? (float)p.tx : ((float)p.tx + 0.5f)) * TILE - g.camX;
    float fy = (p.ty + 1.0f) * TILE - g.camY;
    Texture2D* t = nullptr;
    float shadowW = 10.0f, shadowH = 3.0f;
    switch (p.part) {
        case LP_CASTLE:    t = &g.assets.castle;     shadowW = 44.0f; shadowH = 7.0f; break;
        case LP_TOWER:     t = &g.assets.tower;      shadowW = 18.0f; shadowH = 4.5f; break;
        case LP_STATUE:    t = &g.assets.statue;     shadowW = 11.0f; shadowH = 3.0f; break;
        case LP_PILLAR:    t = &g.assets.pillar;     shadowW =  7.0f; shadowH = 2.5f; break;
        case LP_TORCH:     t = &g.assets.torch;      shadowW =  6.0f; shadowH = 2.0f; break;
        case LP_RUIN_ARCH: t = &g.assets.ruinArch;   shadowW = 15.0f; shadowH = 3.5f; break;
        case LP_MONUMENT:  t = &g.assets.ruinShrine; shadowW = 14.0f; shadowH = 3.5f; break;
        case LP_TENT:      t = &g.assets.tent;       shadowW = 20.0f; shadowH = 4.5f; break;
        case LP_CAMPFIRE:  t = &g.assets.campfire;   shadowW =  8.0f; shadowH = 2.5f; break;
        case LP_CAULDRON:  t = &g.assets.cauldron;   shadowW =  8.0f; shadowH = 2.5f; break;
        case LP_BARREL:    t = &g.assets.barrel;     shadowW =  6.0f; shadowH = 2.0f; break;
        case LP_CAVE:      t = &g.assets.caveMouth;  shadowW = 22.0f; shadowH = 5.0f; break;
        case LP_TIDEWELL: {
            // A found Tidewell runs; an unfound one is a dry basin, so the
            // player can read the quest's progress from the stone itself.
            bool found = false;
            for (int i = 0; i < TIDE_ISLANDS; i++) {
                if ((g.tideMask & (1 << i)) && tideIslandWellX(i) == (float)p.tx + 0.5f &&
                    tideIslandWellY(i) == (float)(p.ty - 1) + 0.5f) found = true;
            }
            t = found ? &g.assets.tidewell : &g.assets.tidewellDry;
            shadowW = 13.0f; shadowH = 3.5f;
            break;
        }
    }
    if (!t || t->id == 0) return;
    DrawEllipse((int)cx, (int)fy - 2, shadowW, shadowH, Fade(BLACK, 0.25f));
    texFoot(*t, cx, fy);
    if (p.part == LP_TORCH || p.part == LP_CAMPFIRE) {
        float pulse = 0.5f + 0.5f * sinf(g.totalTime * 4.5f + (float)p.tx);
        DrawCircleGradient((int)cx, (int)(fy - t->height * 0.55f), 22 + pulse * 6,
                           Fade(Color{ 255, 170, 60, 255 }, 0.22f + nightAmt * 0.18f), BLANK);
    }
    if (p.part == LP_CAVE) {
        // cold breath drifting out of the dark
        float pulse = 0.5f + 0.5f * sinf(g.totalTime * 2.2f + (float)p.tx * 0.7f);
        DrawCircleGradient((int)cx, (int)(fy - t->height * 0.35f), 26 + pulse * 8,
                           Fade(Color{ 90, 120, 220, 255 }, 0.14f + nightAmt * 0.14f), BLANK);
    }
}

void drawWorld() {
    float shakeX = 0, shakeY = 0;
    if (g.shakeT > 0) {
        shakeX = sinf(g.totalTime * 60.0f) * g.shakeMag;
        shakeY = cosf(g.totalTime * 47.0f) * g.shakeMag;
    }
    float saveCamX = g.camX, saveCamY = g.camY;
    g.camX = roundf(g.camX - shakeX);
    g.camY = roundf(g.camY - shakeY);

    int x0 = (int)floorf(g.camX / TILE) - 1;
    int y0 = (int)floorf(g.camY / TILE) - 1;
    int x1 = x0 + g.vw / TILE + 3;
    int y1 = y0 + g.vh / TILE + 3;

    drawGround(x0, y0, x1, y1);

    // A sealed defile should read as an objective, not an inexplicable wall.
    // The rock tiles carry the collision; the existing stone supplies its sign.
    for (int ring = 1; ring <= 2; ++ring) {
        if (regionPassOpen(ring)) continue;
        for (int dir = 0; dir < 4; ++dir) {
            int tx, ty; regionPassCenter(ring, dir, tx, ty);
            if (tx < x0 - 2 || tx > x1 + 2 || ty < y0 - 2 || ty > y1 + 2) continue;
            float sx = (tx + 0.5f) * TILE - g.camX;
            float sy = (ty + 1.0f) * TILE - g.camY;
            DrawCircleGradient((int)sx, (int)(sy - 9), 27,
                               Fade(Color{130, 176, 255, 255}, 0.27f), BLANK);
            texFoot(g.assets.statusStone[1], sx, sy, Color{195, 212, 255, 255});
            float dx = tx + 0.5f - g.px, dy = ty + 0.5f - g.py;
            if (dx * dx + dy * dy < 10.0f * 10.0f)
                drawWorldLabel("SEALED PASS", sx, sy - 50, Color{255, 225, 165, 255}, 8);
        }
    }

    float dayT = g.totalTime * 0.004f;
    float night = (sinf(dayT * 6.2831853f) + 1.0f) * 0.5f;
    float nightAmt = (night > 0.55f) ? (night - 0.55f) / 0.45f : 0.0f;

    static std::vector<RenderItem> list;
    static std::vector<Landmark> visibleLandmarks;
    static std::vector<LandmarkPart> landmarkParts;
    static std::vector<LandmarkPart> tmpParts;
    list.clear();
    landmarkParts.clear();

    // 1. World props
    for (int ty = y0 - 3; ty <= y1 + 4; ty++) {
        for (int tx = x0 - 3; tx <= x1 + 3; tx++) {
            PropKind p = propAt(tx, ty);
            if (p == P_NONE) continue;
            list.push_back({ (ty + 1.0f) * TILE, RK_PROP, (int)p, tx, ty, 0, 0 });
        }
    }

    // 2. Landmarks (individually Y-sorted sub-pieces!)
    landmarksInView(x0, y0, x1, y1, visibleLandmarks);
    for (const auto& lm : visibleLandmarks) {
        expandLandmarkParts(lm, tmpParts);
        for (const auto& pt : tmpParts) {
            int idx = (int)landmarkParts.size();
            landmarkParts.push_back(pt);
            list.push_back({ (pt.ty + 1.0f) * TILE, RK_LANDMARK_PART, idx, 0, 0, 0, 0 });
        }
    }

    // 3. Village decos & buildings
    if (g.villageLoaded) {
        for (int i = 0; i < (int)g.village.decos.size(); i++) {
            const auto& d = g.village.decos[i];
            if (d.x < x0 - 4 || d.x > x1 + 4 || d.y < y0 - 4 || d.y > y1 + 4) continue;
            list.push_back({ (d.y + 1.0f) * TILE, RK_DECO, i, 0, 0, d.x, d.y });
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
                if (it.a != P_BUSH && it.a != P_BUSH_PLAIN && it.a != P_MUSHROOM && it.a != P_REEDS)
                    DrawEllipse((int)fx, (int)fy - 2, w * 0.28f, 3.0f, Fade(BLACK, 0.20f));
                bool canopy = (it.a == P_TREE0 || it.a == P_TREE1 || it.a == P_TREE2 ||
                               it.a == P_PINE0 || it.a == P_PINE1 || it.a == P_PINE2 ||
                               it.a == P_AMBER_OAK || it.a == P_COPPER_OAK ||
                               it.a == P_WILLOW || it.a == P_TREE_SNOW);
                float sway = canopy ? sinf(g.totalTime * 1.4f + hashf(it.b, it.c, 0x5eedU) * 6.28f) * 1.0f : 0.0f;
                texFoot(*t, fx + sway, fy);
                break;
            }
            case RK_LANDMARK_PART:
                drawLandmarkPart(landmarkParts[it.a], nightAmt);
                break;
            case RK_DECO: {
                const auto& d = g.village.decos[it.a];
                int w = 0, h = 0;
                Texture2D* t = nullptr;
                villageDecoTexture(d.kind, d.variant, t, w, h);
                if (!t || t->id == 0) break;
                float cx = villageDecoCenterX(d) - g.camX;
                float fy = (floorf(d.y + 0.5f) + 1.0f) * TILE - g.camY;
                if (d.kind != VD_BUSH && d.kind != VD_FLOWER_BUSH && d.kind != VD_FLOWERBED)
                    DrawEllipse((int)cx, (int)fy - 1, w * 0.30f, 2.5f, Fade(BLACK, 0.20f));
                float sway = (d.kind == VD_SHADE_TREE || d.kind == VD_PINE)
                           ? sinf(g.totalTime * 1.4f + hashf((int)d.x, (int)d.y, 0x5eedU) * 6.28f) * 1.0f
                           : 0.0f;
                texFoot(*t, cx + sway, fy);
                if (d.kind == VD_STATUS_STONE || d.kind == VD_EMPTY_PLOT) {
                    // The ring's own light: a low glow that never quite settles,
                    // brighter once the keeper's flame is lit.
                    float pulse = 0.5f + 0.5f * sinf(g.totalTime * 1.4f + (float)d.variant * 1.7f);
                    float lit = (g.quests[QUEST_COMMONS].state == QS_DONE) ? 0.10f : 0.04f;
                    DrawCircleGradient((int)cx, (int)(fy - 3), 20 + pulse * 5,
                                       Fade(Color{ 255, 226, 160, 255 }, lit + 0.05f * pulse + nightAmt * 0.06f), BLANK);
                }
                if (d.kind == VD_TORCH || d.kind == VD_CAMPFIRE) {
                    float pulse = 0.5f + 0.5f * sinf(g.totalTime * 4.5f + d.x);
                    DrawCircleGradient((int)cx, (int)(fy - h * 0.5f), 22 + pulse * 6,
                                       Fade(Color{ 255, 170, 60, 255 }, 0.22f + nightAmt * 0.18f), BLANK);
                }
                if (d.kind == VD_CAULDRON && d.variant >= 4) {
                    // The keeper's brazier, lit: a small kept flame for the ring.
                    float pulse = 0.5f + 0.5f * sinf(g.totalTime * 3.6f + 1.3f);
                    float flick = sinf(g.totalTime * 11.0f) * 0.6f;
                    DrawCircleGradient((int)cx, (int)(fy - h - 2), 26 + pulse * 8 + flick,
                                       Fade(Color{ 255, 158, 60, 255 }, 0.26f + nightAmt * 0.20f), BLANK);
                    DrawRectangle((int)(cx - 3), (int)(fy - h - 7 - pulse * 2), 6, 5,
                                  Color{ 255, 196, 92, 255 });
                    DrawRectangle((int)(cx - 2), (int)(fy - h - 9 - pulse * 2), 4, 4,
                                  Color{ 255, 238, 168, 255 });
                    DrawRectangle((int)(cx - 1), (int)(fy - h - 10 - pulse * 2), 2, 3,
                                  Color{ 255, 252, 226, 255 });
                }
                break;
            }
            case RK_BUILDING: {
                const Building& b = g.village.buildings[it.a];
                Texture2D* t = buildingTexture(b);
                if (!t || t->id == 0) break;
                float cx = buildingCenterX(b) - g.camX;
                float fy = (b.ty + 1.0f) * TILE - g.camY;
                DrawEllipse((int)cx, (int)fy - 3, b.w * TILE * 0.44f, 5.0f, Fade(BLACK, 0.24f));
                texFoot(*t, cx, fy);
                if (nightAmt > 0.05f) {
                    DrawCircleGradient((int)cx, (int)(fy - 12), 26.0f,
                                       Fade(Color{ 255, 200, 100, 255 }, 0.28f * nightAmt), BLANK);
                    float fl = 0.85f + 0.15f * sinf(g.totalTime * 3.1f + (float)b.tx * 1.7f);
                    Color win = Fade(Color{ 255, 214, 120, 255 }, nightAmt * fl);
                    DrawRectangle((int)(cx - 14), (int)(fy - 20), 5, 6, win);
                    DrawRectangle((int)(cx + 9), (int)(fy - 20), 5, 6, win);
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
                if (g.moving) {
                    float ph = fmodf(g.walkAnimT * 3.5f, 1.0f);
                    DrawEllipse((int)cx, (int)fy - 1, 3.0f + ph * 3.0f, 1.5f + ph * 1.0f,
                                Fade(Color{ 230, 220, 190, 255 }, 0.30f * (1.0f - ph)));
                }
                drawCharSheet(sheet, row, frame, cx, fy, g.facing == 2 && g.sideFlip);
                break;
            }
        }
    }

    if (g.villageLoaded && inCommons(g.px, g.py)) commonsDrawWorld();

    if (g.clickFlashT > 0) {
        float a = g.clickFlashT / 0.4f;
        float cx = g.clickFlashX * TILE - g.camX;
        float cy = g.clickFlashY * TILE - g.camY;
        DrawRing({ cx, cy }, 4 + (1 - a) * 6, 6 + (1 - a) * 6, 0, 360, 20, Fade(Color{ 255, 240, 180, 255 }, a));
    }
    if (g.blockedFx > 0) {
        float cx = g.px * TILE - g.camX;
        float cy = g.py * TILE - g.camY;
        DrawRing({ cx, cy }, 10, 12, 0, 360, 16, Fade(Color{ 255, 120, 120, 255 }, g.blockedFx * 0.5f));
    }

    drawClouds(true);

    Biome biome = sampleGround((int)floorf(g.px), (int)floorf(g.py)).biome;
    if (biome == BI_FROST || biome == BI_AMBER || biome == BI_FEN) {
        int n = biome == BI_FROST ? 32 : 12;
        for (int i = 0; i < n; ++i) {
            float phase = hashf(i, 3, g.worldSeed) * 6.28f;
            float px = hashf(i, 7, g.worldSeed) * g.vw - g.camX + 8 * sinf(g.totalTime * 0.7f + phase);
            float py = hashf(i, 9, g.worldSeed) * g.vh - g.camY + g.totalTime * (biome == BI_FROST ? 16.f : biome == BI_AMBER ? 9.f : -3.f);
            int dx = (int)(px - floorf(px / g.vw) * g.vw), dy = (int)(py - floorf(py / g.vh) * g.vh);
            Color c = biome == BI_FROST ? Color{ 232, 247, 255, 185 } : biome == BI_AMBER ? Color{ 238, 185, 94, 200 } : Color{ 220, 242, 162, 175 };
            DrawRectangle(dx, dy, 1 + (i % 2), 1, c);
        }
    }

    {
        float px = g.px * TILE - g.camX, py = g.py * TILE - g.camY;
        bool day = nightAmt < 0.35f;
        if (day && (biome == BI_MEADOW || biome == BI_FOREST || biome == BI_AMBER)) {
            for (int i = 0; i < 3; i++) {
                float ph = g.totalTime * (0.35f + 0.12f * i) + (float)i * 2.1f;
                float bx = px + sinf(ph * 1.7f) * 46.f + sinf(ph * 0.6f) * 18.f;
                float by = py - 18.f + sinf(ph * 2.3f) * 10.f + cosf(ph * 1.1f) * 6.f;
                float flap = fmodf(g.totalTime * 9.f + (float)i * 2.f, 1.f) < 0.5f ? 1.f : 0.f;
                Color wc = (i == 0) ? Color{ 255, 240, 170, 230 } : (i == 1) ? Color{ 200, 225, 255, 230 } : Color{ 255, 205, 230, 230 };
                DrawRectangle((int)(bx - 2 - flap), (int)by, 2, 1, wc);
                DrawRectangle((int)bx, (int)by, 2, 1, wc);
            }
        }
        if (nightAmt > 0.25f && (biome == BI_FEN || biome == BI_FOREST || biome == BI_MEADOW)) {
            for (int i = 0; i < 10; i++) {
                float ph = g.totalTime * (0.5f + 0.2f * (i % 3)) + (float)i * 1.37f;
                float fx = px + sinf(ph) * 70.f + sinf(ph * 0.37f) * 24.f;
                float fy = py - 10.f + cosf(ph * 0.83f) * 34.f;
                float tw = 0.5f + 0.5f * sinf(g.totalTime * 2.4f + (float)i * 2.9f);
                DrawCircle((int)fx, (int)fy, 1, Fade(Color{ 190, 255, 170, 255 }, tw * nightAmt));
            }
        }
    }

    {
        float sun = sinf(dayT * 6.2831853f);
        float dusk = (sun > -0.25f && sun < 0.35f) ? (1.f - fabsf(sun - 0.05f) / 0.4f) : 0.f;
        if (dusk > 0.01f) DrawRectangle(0, 0, g.vw, g.vh, Fade(Color{ 255, 150, 70, 255 }, dusk * 0.10f));
    }

    if (nightAmt > 0.001f) {
        DrawRectangle(0, 0, g.vw, g.vh, Fade(Color{ 20, 26, 60, 255 }, nightAmt * 0.40f));
    }

    // ---- Post-sprite, post-lighting floating world markers (never occluded by canopies or roofs!)
    for (const auto& n : g.npcs) {
        char qm = npcQuestMarker(n);
        if (!qm) continue;
        float cx = n.x * TILE - g.camX;
        float fy = (n.y + 0.35f) * TILE - g.camY;
        if (cx < -16 || cx > g.vw + 16 || fy < -16 || fy > g.vh + 32) continue;
        float bob = sinf(g.totalTime * 4.5f) * 2.0f;
        Color qc = (qm == '?') ? Color{ 120, 255, 160, 255 } : Color{ 255, 228, 96, 255 };
        char qbuf[2] = { qm, 0 };
        drawWorldLabel(qbuf, cx, fy - 42.0f + bob, qc, 8);
    }
    // Every surveyed place names itself once the party is close enough to see
    // it, so a ruin or a camp on the horizon is something to walk toward rather
    // than another piece of scenery.
    for (const auto& lm : visibleLandmarks) {
        if (std::max(abs(lm.tx - (int)floorf(g.px)), abs(lm.ty - (int)floorf(g.py))) > 34) continue;
        int sx, sy;
        bool storyCave = lm.kind == LK_CAVERN && startCavernSite(sx, sy) && lm.tx == sx && lm.ty == sy;
        frontierCavernSite(sx, sy);
        bool frontierCave = lm.kind == LK_CAVERN && lm.tx == sx && lm.ty == sy;
        const char* label = "LANDMARK";
        Color col{ 244, 216, 159, 255 };
        switch (lm.kind) {
            case LK_CAVERN: label = storyCave ? "HOLLOW HILL" : (frontierCave ? "MARCHES CAVERN" : "CAVERN");
                            col = Color{ 244, 216, 159, 255 }; break;
            case LK_RUINS:  label = "RUINS";          col = Color{ 214, 202, 178, 255 }; break;
            case LK_TOWER:  label = "WATCHTOWER";     col = Color{ 214, 222, 240, 255 }; break;
            case LK_CASTLE: label = "CASTLE";         col = Color{ 236, 214, 176, 255 }; break;
            case LK_STONES: label = "STANDING STONES"; col = Color{ 206, 226, 214, 255 }; break;
            case LK_CAMP:   label = "CAMP";           col = Color{ 240, 196, 148, 255 }; break;
            default: break;
        }
        float cx = (lm.tx + 0.5f) * TILE - g.camX;
        float fy = (lm.ty + 1.0f) * TILE - g.camY;
        drawWorldLabel(label, cx, fy - 47, col, 8);
    }
    for (const auto& m : gFieldMonsters) {
        float dx = g.px - m.x, dy = g.py - m.y;
        if (dx * dx + dy * dy < 9.0f && fmodf(g.totalTime * 2.0f, 1.0f) < 0.65f) {
            Texture2D& t = g.assets.enemy[m.kind];
            float sx = m.x * TILE - g.camX;
            float sy = (m.y + 0.35f) * TILE - g.camY;
            drawWorldLabel("!", sx, sy - t.height * 0.62f - 14.0f, Color{ 255, 110, 110, 255 }, 8);
        }
    }
    if (!g.bossDefeated &&
        BOSS_KING_X >= x0 - 4 && BOSS_KING_X <= x1 + 4 &&
        BOSS_KING_Y >= y0 - 4 && BOSS_KING_Y <= y1 + 4) {
        float bob = sinf(g.totalTime * 2.8f) * 2.0f;
        float sx = BOSS_KING_X * TILE - g.camX;
        float sy = (BOSS_KING_Y + 0.45f) * TILE - g.camY;
        drawWorldLabel("SLIME KING", sx, sy - 48.0f + bob, Color{ 255, 228, 128, 255 }, 8);
    }

    // The Survey names the country underfoot; crossing into a new one raises its
    // name briefly, the way an old map writes a region in the middle of its hills.
    if (!g.regionName.empty() && g.regionBannerT > 0.0f) {
        float a = std::min(1.0f, g.regionBannerT * 1.5f);
        if (4.0f - g.regionBannerT < 0.35f) a *= (4.0f - g.regionBannerT) / 0.35f;
        Vector2 rs = measureText(g.regionName.c_str(), 8);
        float cx = (float)g.vw * 0.5f;
        drawWorldLabel(g.regionName.c_str(), cx, 26.0f,
                       Fade(Color{ 255, 238, 190, 255 }, a * 0.95f), 8);
        // two ink rules either side, so the name reads as a map's region title
        Color rule = Fade(Color{ 236, 214, 168, 255 }, a * 0.55f);
        DrawRectangle((int)(cx - rs.x * 0.5f) - 26, 30, 18, 1, rule);
        DrawRectangle((int)(cx + rs.x * 0.5f) + 8, 30, 18, 1, rule);
    }

    g.camX = saveCamX;
    g.camY = saveCamY;
}

// ================================================================ HUD
void drawHUD() {
    float wx = g.sailing ? gSailX : g.px, wy = g.sailing ? gSailY : g.py;
    const char* zone = g.sailing ? sailZoneName() : zoneNameAt(wx, wy);
    char zoneBuf[128];
    snprintf(zoneBuf, sizeof(zoneBuf), "%s / %s  Lv.%d", regionNameAt(wx, wy), zone,
             zoneLevelAt(wx, wy));
    Vector2 zsz = measureText(zoneBuf, 8);
    drawWindow({ 5, 5, zsz.x + 32, 20 });
    DrawTriangle({ 12, 16 }, { 16, 14 }, { 16, 7 }, Color{ 250, 218, 149, 255 });
    DrawTriangle({ 20, 12 }, { 16, 14 }, { 16, 21 }, Color{ 134, 173, 169, 255 });
    drawText(zoneBuf, 27, 11, Color{ 246, 238, 212, 255 }, 8);

    char buf[64];
    snprintf(buf, sizeof(buf), "%d", g.gil);
    int gw = (int)measureText(buf, 8).x + 28;
    int gx = g.vw - 5 - gw;
    drawWindow({ (float)gx, 5, (float)gw, 20 });
    DrawTexture(g.assets.icon[ICON_COIN], gx + 4, 6, WHITE);
    drawText(buf, (float)gx + 22, 11, Color{ 247, 211, 135, 255 }, 8);

    // During dialog, keep the bottom of the screen completely clear for the dialogue window
    if (g.mode == MODE_DIALOG) {
        drawToast();
        return;
    }

    // Party strip: 3 generous 112px columns (344px total) so 6-char names ("Aldric") + PSN/KO & gauges never collide
    constexpr int slotW = 112;
    constexpr int partyW = slotW * 3 + 8;
    int by = g.vh - 36;
    drawWindow({ 5, (float)by, (float)partyW, 31 });
    for (int i = 0; i < 3; ++i) {
        const Hero& h = g.party[i];
        int x = 9 + slotW * i;
        Texture2D f = g.assets.face[h.klass == KLASS_KNIGHT ? FACE_KNIGHT : h.klass == KLASS_MAGE ? FACE_MAGE : FACE_CLERIC];
        if (f.id) {
            DrawRectangle(x, by + 6, 19, 19, Color{ 10, 14, 28, 255 });
            DrawTexturePro(f, { 0, 0, (float)f.width, (float)f.height },
                           { (float)x + 0.5f, (float)by + 6.5f, 18, 18 }, { 0, 0 }, 0,
                           h.ko ? Color{ 135, 135, 154, 255 } : WHITE);
        }
        // Top row (by + 5): Hero name on left, KO / PSN badge right-aligned
        drawText(heroName(h.klass), (float)x + 22.f, (float)by + 5.f,
                 h.ko ? Color{ 226, 141, 139, 255 } : WHITE, 8);
        if (h.ko) {
            float kw = measureText("KO", 8).x;
            drawText("KO", (float)(x + slotW - 5) - kw, (float)by + 5.f, Color{ 255, 110, 110, 255 }, 8);
        } else if (h.poisoned) {
            float pw = measureText("PSN", 8).x;
            drawText("PSN", (float)(x + slotW - 5) - pw, (float)by + 5.f, Color{ 205, 140, 255, 255 }, 8);
        }

        // Bottom row (by + 17): Compact L# on left, crisp HP & MP gauges on right
        char lv[16];
        snprintf(lv, sizeof(lv), "L%d", h.level);
        drawText(lv, (float)x + 22.f, (float)by + 17.f, Color{ 175, 192, 232, 255 }, 8);
        drawBar((float)x + 53.f, (float)by + 16.f, 53.f, 5.f, (float)h.hp / std::max(1, h.maxHp),
                h.poisoned ? Color{ 187, 138, 192, 255 } : Color{ 96, 220, 120, 255 }, Color{ 20, 28, 58, 255 });
        drawBar((float)x + 53.f, (float)by + 23.f, 53.f, 4.f, (float)h.mp / std::max(1, h.maxMp),
                Color{ 96, 160, 255, 255 }, Color{ 20, 28, 58, 255 });
        if (i < 2) DrawRectangle(x + slotW - 2, by + 5, 1, 21, Color{ 76, 92, 144, 200 });
    }

    // Active quest tracker (top-right, below Gil box)
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
            else if (!g.hasWaystone) {
                int tx, ty; startCavernSite(tx, ty);
                line = "Waystone: Hollow Hill [" + compassHint(wx, wy, (float)tx, (float)ty) + "]";
            } else line += " [" + compassHint(wx, wy, (float)BOSS_CASTLE_TX, (float)BOSS_CASTLE_TY) + "]";
        }
        Vector2 sz = measureText(line.c_str(), 8);
        float qw = sz.x + 14.0f;
        drawWindow({ g.vw - qw - 5.0f, 28.0f, qw, 20.0f });
        drawText(line.c_str(), g.vw - qw + 2.0f, 34.0f, Color{ 255, 232, 150, 255 }, 8);
        break;
    }

    // Interaction prompt (right of Party HUD when wide enough, otherwise cleanly above Party HUD)
    const char* prompt = g.sailing ? nullptr : interactionPrompt();
    if (prompt) {
        Vector2 sz = measureText(prompt, 8);
        float pw = sz.x + 16.0f;
        float px = (float)g.vw - pw - 5.0f;
        float py = (px > (float)(partyW + 10)) ? (float)(g.vh - 31) : (float)(by - 25);
        drawWindowBlue({ px, py, pw, 21.0f });
        drawTextCentered(prompt, px + pw * 0.5f, py + 7.0f, Color{ 255, 240, 200, 255 }, 8);
    }
    drawToast();
}

void drawToast() {
    if (g.toastT <= 0 || g.toast.empty()) return;
    Vector2 sz = measureText(g.toast.c_str(), 8);
    float a = std::min(1.0f, g.toastT / 0.4f);
    float w = std::min((float)(g.vw - 20), sz.x + 22.0f);
    float h = 21.0f;
    float x = floorf(g.vw * 0.5f - w * 0.5f);
    bool hasTopRightQuest = (g.mode == MODE_OVERWORLD) &&
                            (g.quests[0].state == QS_ACTIVE || g.quests[1].state == QS_ACTIVE || g.quests[2].state == QS_ACTIVE);
    float y = g.mode == MODE_DUNGEON ? 128.0f : (hasTopRightQuest ? 52.0f : 30.0f);
    DrawRectangle((int)x + 2, (int)y + 2, (int)w, (int)h, Fade(BLACK, 0.4f * a));
    DrawRectangle((int)x, (int)y, (int)w, (int)h, Fade(Color{ 16, 22, 54, 255 }, 0.96f * a));
    DrawRectangleLinesEx({ x, y, w, h }, 1.0f, Fade(Color{ 255, 220, 140, 255 }, a));
    drawTextCentered(g.toast.c_str(), g.vw * 0.5f, y + 7.0f, Fade(WHITE, a), 8);
}

// ================================================================ dialog box
void drawDialogBox() {
    if (g.dialog.empty()) return;
    const DialogLine& line = g.dialog[std::min<int>(g.dialogIndex, (int)g.dialog.size() - 1)];
    float w = std::min((float)(g.vw - 12), 548.0f);
    float h = 88.0f;
    float bx = floorf((g.vw - w) * 0.5f);
    float y = (float)g.vh - h - 6.0f;
    drawWindow({ bx, y, w, h });
    if (line.face >= 0 && line.face < FACE_COUNT && g.assets.face[line.face].id) {
        Texture2D& f = g.assets.face[line.face];
        DrawRectangle((int)bx + 9, (int)y + 13, 50, 50, Color{ 10, 14, 28, 255 });
        DrawTexturePro(f, { 0, 0, (float)f.width, (float)f.height }, { bx + 10, y + 14, 48, 48 },
                       { 0, 0 }, 0.0f, WHITE);
        DrawRectangleLines((int)bx + 10, (int)y + 14, 48, 48, Color{ 255, 232, 150, 255 });
    }
    float tx = bx + ((line.face >= 0) ? 68.0f : 14.0f);
    float maxW = (bx + w) - tx - 14.0f;
    if (!line.speaker.empty()) {
        Vector2 nsz = measureText(line.speaker.c_str(), 8);
        drawWindowBlue({ tx - 4, y - 10, nsz.x + 16, 19 });
        drawText(line.speaker.c_str(), tx + 4, y - 4, Color{ 255, 232, 150, 255 }, 8);
    }

    size_t nchars = (size_t)std::max(0.0f, std::min((float)line.text.size(), g.dialogReveal));
    std::string shown = line.text.substr(0, nchars);
    drawTextWrapped(shown.c_str(), tx, y + 14.0f, maxW, 13.0f, WHITE, 8);

    if (g.dialogReveal >= (float)line.text.size()) {
        bool last = (g.dialogIndex >= (int)g.dialog.size() - 1);
        if (fmodf(g.totalTime * 2.0f, 1.0f) < 0.7f) {
            float ax = bx + w - 22.0f, ay = y + h - 14.0f;
            DrawTriangle({ ax, ay }, { ax + 10, ay }, { ax + 5, ay + 7 }, Color{ 255, 232, 150, 255 });
        }
        const char* hint = last ? "SPACE" : "SPACE more";
        Vector2 sz = measureText(hint, 8);
        drawText(hint, bx + w - sz.x - 28.0f, y + h - 14.0f, Color{ 186, 206, 255, 255 }, 8);
    }
}

// ================================================================ title
void drawTitleScreen() {
    if (g.assets.titleBg.id) drawBackdropCover(g.assets.titleBg);
    else ClearBackground(Color{ 12, 18, 40, 255 });

    drawClouds();

    DrawRectangleGradientV(0, 0, g.vw, g.vh / 2, Fade(BLACK, 0.38f), BLANK);
    DrawRectangleGradientV(0, g.vh / 2, g.vw, g.vh / 2, BLANK, Fade(BLACK, 0.62f));

    for (int i = 0; i < 14; i++) {
        float sx = hashf(i, 3, 0x77aaU) * g.vw;
        float sy = hashf(i, 7, 0x99bbU) * (g.vh * 0.52f);
        float tw = 0.5f + 0.5f * sinf(g.titleT * 2.2f + (float)i * 1.9f);
        if (tw > 0.55f) DrawRectangle((int)sx, (int)sy, 2, 2, Fade(Color{ 255, 250, 220, 255 }, tw * 0.8f));
    }

    const char* title = "A PLACE FOR AGENTS";
    float bob = sinf(g.titleT * 1.6f) * 1.5f;
    float logoY = floorf(g.vh * 0.5f - 96.0f + bob);
    drawTextCentered(title, g.vw * 0.5f + 3, logoY + 3, Color{ 10, 14, 28, 255 }, 24);
    drawTextCentered(title, g.vw * 0.5f + 1, logoY + 1, Color{ 70, 90, 160, 255 }, 24);
    drawTextCentered(title, g.vw * 0.5f,     logoY,     Color{ 255, 236, 176, 255 }, 24);
    drawTextCentered("a field office at the edge of the map", g.vw * 0.5f, logoY + 32, Color{ 230, 238, 255, 240 }, 8);

    float menuTop = floorf(g.vh * 0.5f - 28.0f);
    const char* opts[3] = { "NEW ADVENTURE", "CONTINUE", "QUIT" };
    for (int i = 0; i < 3; i++) {
        bool sel = (g.titleIndex == i);
        bool disabled = (i == 1 && !g.saveExists);
        float w = 176.0f;
        Rectangle box{ floorf(g.vw * 0.5f - w * 0.5f), menuTop + i * 24.0f, w, 20.0f };
        if (sel && !disabled) drawWindowBlue(box);
        else drawWindow(box);
        Color c = disabled ? Color{ 140, 150, 178, 190 }
                           : (sel ? Color{ 255, 236, 176, 255 } : Color{ 235, 240, 255, 245 });
        drawTextCentered(opts[i], g.vw * 0.5f, box.y + 6.0f, c, 8);
        if (sel && !disabled) drawCursor(box.x + 10.0f, box.y + 6.0f);
    }
    if (fmodf(g.titleT * 1.4f, 1.0f) < 0.65f)
        drawTextCentered("PRESS START", g.vw * 0.5f, menuTop + 76.0f, Color{ 255, 236, 176, 255 }, 8);

    // Attract row: walking party & slime above clean dark footer strip
    {
        float walk = fmodf(g.titleT * 26.0f, (float)(g.vw + 120)) - 60.0f;
        float gy = (float)g.vh - 34.0f;
        for (int i = 0; i < 3; i++) {
            Texture2D& sheet = g.assets.heroWalk[i];
            if (sheet.id == 0) continue;
            float cx = walk + (float)i * 30.0f;
            float fy = gy + sinf(g.titleT * 9.0f + (float)i * 1.3f) * 1.5f;
            DrawEllipse((int)cx, (int)fy, 6.0f, 2.5f, Fade(BLACK, 0.28f));
            int frame = 1 + ((int)(g.titleT * 7.0f + (float)i) & 1);
            Rectangle src{ (float)(frame * 24), 64.0f, 24, 32 };
            DrawTexturePro(sheet, src, { floorf(cx - 12.0f), floorf(fy - 32.0f), 24, 32 }, { 0, 0 }, 0.0f, WHITE);
        }
        Texture2D& sl = g.assets.enemy[EN_SLIME];
        if (sl.id) {
            float cx = walk - 34.0f;
            float fy = gy + sinf(g.titleT * 6.0f) * 1.5f;
            DrawEllipse((int)cx, (int)fy, 8.0f, 3.0f, Fade(BLACK, 0.28f));
            texFoot(sl, cx, fy, WHITE, 0.62f);
        }
    }

    DrawRectangle(0, g.vh - 28, g.vw, 28, Color{ 6, 8, 18, 185 });
    drawTextCentered("arrows / mouse to choose - SPACE to begin",
                     g.vw * 0.5f, (float)g.vh - 23.0f, Color{ 222, 230, 252, 240 }, 8);
    drawTextCentered("(C) 2026 A Place for Agents - art: OpenRTP - v0.2.0",
                     g.vw * 0.5f, (float)g.vh - 11.0f, Color{ 182, 194, 224, 210 }, 8);
    drawToast();
}

// ================================================================ game over
void drawGameOverScreen() {
    if (g.assets.gameoverPlate.id) drawBackdropCover(g.assets.gameoverPlate);
    else ClearBackground(Color{ 18, 8, 12, 255 });
    DrawRectangle(0, 0, g.vw, g.vh, Fade(Color{ 20, 4, 8, 255 }, 0.44f));
    float cy = floorf(g.vh * 0.5f - 46.0f);
    drawTextCentered("THE PARTY HAS FALLEN", g.vw * 0.5f + 2, cy + 2, Color{ 16, 4, 8, 255 }, 24);
    drawTextCentered("THE PARTY HAS FALLEN", g.vw * 0.5f,     cy,     Color{ 255, 210, 210, 255 }, 24);
    drawTextCentered("the meadow keeps its own", g.vw * 0.5f, cy + 36, Color{ 230, 220, 230, 230 }, 8);
    if (fmodf(g.totalTime * 2.0f, 2.0f) < 1.2f)
        drawTextCentered("SPACE or CLICK: return to title", g.vw * 0.5f, (float)g.vh - 48.0f,
                         Color{ 255, 236, 176, 255 }, 8);
}

// ================================================================ intro
void drawIntroScreen() {
    if (g.assets.dawn.id) drawBackdropCover(g.assets.dawn);
    else ClearBackground(Color{ 30, 40, 70, 255 });
    DrawRectangle(0, 0, g.vw, g.vh, Fade(Color{ 6, 10, 24, 255 }, 0.52f));

    const char* lines[] = {
        "Every map ends at the edge of Elmswick.",
        "That's where the field agents begin.",
        "Aldric keeps the way open.",
        "Iris looks for what maps leave out.",
        "Pip gets everyone safely home.",
        "Now the northern road has gone quiet.",
        "Three agents. One unfinished map.",
    };
    float drift = std::min(18.0f, g.introT * 4.5f);
    float startY = floorf(g.vh * 0.5f - 68.0f - drift);
    for (int i = 0; i < 7; i++) {
        const char* l = lines[i];
        if (l[0] == 0) continue;
        float alpha = std::max(0.0f, std::min(1.0f, (g.introT - i * 0.35f) * 1.4f));
        if (alpha <= 0.01f) continue;
        bool isTag = (i == 6);
        Color col = isTag ? Color{ 255, 236, 176, 255 } : Color{ 228, 234, 252, 255 };
        float y = startY + i * 20.0f + (isTag ? 6.0f : 0.0f);
        drawTextCentered(l, g.vw * 0.5f, y, Fade(col, alpha), 8);
    }
    if (g.introT > 1.2f && fmodf(g.totalTime * 2.0f, 2.0f) < 1.2f)
        drawTextCentered("SPACE or CLICK to begin", g.vw * 0.5f, (float)g.vh - 26.0f,
                         Color{ 255, 236, 176, 255 }, 8);
}

// ================================================================ transition fx
void drawTransitionFx() {
    // Every cut in the game uses the same eight-step palette fade (cinema.cpp).
    drawSteppedFade();
}
