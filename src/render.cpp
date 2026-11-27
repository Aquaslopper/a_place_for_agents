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
    if (sprite < 0 || sprite >= NS_KIND_COUNT) return &g.assets.npc[0];
    return &g.assets.npc[sprite];
}

// ================================================================ ground
// The terrain atlas is on a dual grid: its corners are the centres of four
// collision cells. A full fill costs one quad. A mixed cell paints the lower
// material first, then just the masks above it. Three-way junctions have no
// transparent holes, and the marching corners continue across the viewport.
enum Surface { S_DEEP, S_WATER, S_BEACH, S_MEADOW, S_WOOD, S_AMBER, S_HEATH, S_FEN,
               S_DUNES, S_SNOW, S_ICE, S_CRAG, S_PATH, S_SOIL, S_COBBLE, S_COURT, S_LEAVES, S_RIPPLES, S_MESA, S_FROST_TRACK, S_SNOW_CRAG, S_COUNT };
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
        case G_ROCK: return t.biome==BI_DUNES ? S_MESA : t.biome==BI_FROST ? S_SNOW_CRAG : S_CRAG;
        case G_DIRT: return t.biome==BI_FROST ? S_FROST_TRACK : S_PATH;
        case G_LEAVES: return S_LEAVES;
        case G_RIPPLES: return S_RIPPLES;
        case G_SOIL: return S_SOIL;
        case G_COBBLE: return S_COBBLE;
        case G_FLOOR: return S_COURT;
        default: return S_MEADOW;
    }
}
static void stamp(Texture2D atlas, int mask, int row, int x, int y) {
    DrawTextureRec(atlas, {(float)(mask*TILE),(float)(row*TILE),TILE,TILE}, {(float)x,(float)y}, WHITE);
}

static void drawGround(int x0, int y0, int x1, int y1) {
    int w=x1-x0+3, h=y1-y0+3;
    static std::vector<GroundTile> view;
    static int lastX, lastY, lastW, lastH;
    static uint32_t seed;
    static uint64_t village;
    uint64_t key=g.villageLoaded ? g.villageKey : ~0ull;
    auto at = [&](int x,int y) -> GroundTile& { return view[(y-y0+1)*w+x-x0+1]; };
    if (lastW!=w || lastH!=h || lastX!=x0 || lastY!=y0 || seed!=g.worldSeed || key!=village) {
        view.resize(w*h); lastW=w; lastH=h; lastX=x0; lastY=y0; seed=g.worldSeed; village=key;
        for (int y=y0-1;y<=y1+1;++y) for (int x=x0-1;x<=x1+1;++x) at(x,y)=sampleGround(x,y);
        auto pave = [&](const std::vector<std::pair<int,int>>& cells, GroundKind mat) {
            for (auto [x,y]:cells) if (x>=x0-1 && y>=y0-1 && x<=x1+1 && y<=y1+1) {
                auto& t=at(x,y);
                if (surface(t)<=S_WATER) continue; // never bury a stream or its decking
                t.kind=mat; t.deco=0;
            }
        };
        if (g.villageLoaded) { pave(g.village.paths,G_DIRT); pave(g.village.plaza,G_COBBLE); }
    }
    int anim=(int)(g.totalTime*2.5f)%3;
    int camX=(int)roundf(g.camX), camY=(int)roundf(g.camY);
    for (int y=y0;y<=y1;++y) for (int x=x0;x<=x1;++x) {
        Surface corners[]={surface(at(x,y)),surface(at(x+1,y)),
                           surface(at(x,y+1)),surface(at(x+1,y+1))};
        int sx=x*TILE+TILE/2-camX, sy=y*TILE+TILE/2-camY;
        int phase=at(x,y).variant%3;
        auto paint=[&](int s,int mask) { stamp(g.assets.landscape,mask,s*3+(s<=S_WATER ? anim:phase),sx,sy); };
        if (corners[0]==corners[1] && corners[0]==corners[2] && corners[0]==corners[3]) {
            paint(corners[0],15); continue;
        }
        int masks[S_COUNT]={};
        for (int i=0;i<4;++i) masks[corners[i]] |= 1<<i;
        int left=15;
        for (int s=0;s<S_COUNT;++s) if (masks[s]) { paint(s,left); left &= ~masks[s]; }
        int dry=15^(masks[S_WATER]|masks[S_DEEP]);
        if (dry && dry!=15) stamp(g.assets.relief,dry,(masks[S_SNOW]|masks[S_ICE]) ? 1:0,sx,sy);
        int ledge=masks[S_CRAG]|masks[S_MESA]|masks[S_SNOW_CRAG];
        if (ledge) stamp(g.assets.relief,ledge,masks[S_MESA]?3:masks[S_SNOW_CRAG]?4:2,sx,sy);
    }
    for (int y=y0;y<=y1;++y) for (int x=x0;x<=x1;++x) {
        auto t=at(x,y);
        int sx=x*TILE-camX, sy=y*TILE-camY;
        bool ns=t.kind==G_BRIDGE_NS;
        if (ns || t.kind==G_BRIDGE_EW) {
            // Planks run along the crossing direction. Rotate about the tile
            // centre so E/W decks land exactly on their tile (DrawTextureEx
            // rotates about its origin, which misplaced E/W bridges).
            Texture2D& br=g.assets.bridge;
            if (br.id) {
                float rot=ns?0.f:90.f;
                DrawTexturePro(br,{0,0,(float)br.width,(float)br.height},
                               {(float)(sx+TILE/2),(float)(sy+TILE/2),(float)br.width,(float)br.height},
                               {(float)br.width*0.5f,(float)br.height*0.5f},rot,WHITE);
            }
            for (int s:{-1,1}) {
                if (at(x+(ns?s:0),y+(ns?0:s)).kind==t.kind) continue;
                int rx=sx+(ns && s==1 ? TILE-3:0), ry=sy+(!ns && s==1 ? TILE-3:0);
                DrawRectangle(rx,ry,ns?3:16,ns?16:3,Color{89,65,51,255});
                DrawRectangle(rx+1,ry+1,ns?1:14,ns?14:1,Color{197,164,111,255});
                DrawRectangle(rx,ry-2,4,5,Color{111,78,52,255});
                DrawRectangle(rx,ry-2,3,2,Color{224,192,137,255});
            }
        } else if (t.deco) {
            Texture2D tex=t.deco==1 ? g.assets.tuft[t.decoVariant%3] : t.deco==2 ? g.assets.flowers[t.decoVariant%2]
                        : t.deco==3 ? g.assets.pebble : t.deco==4 ? g.assets.lilies : g.assets.turnips;
            DrawTexture(tex,sx+(TILE-tex.width)/2,sy+TILE-tex.height,WHITE);
        }
        // Water glints: a few tiles twinkle like SNES water sparkle.
        if (t.kind==G_WATER || t.kind==G_DEEP) {
            float tw=hashf(x,y,g.worldSeed^0x51abU);
            float ph=fmodf(g.totalTime*0.9f+tw*7.f,1.f);
            if (ph<0.16f) {
                int gx=sx+3+(int)(tw*10.f), gy=sy+4+(int)(hashf(y,x,g.worldSeed^0x77cdU)*8.f);
                DrawRectangle(gx,gy,2,1,Fade(Color{235,250,255,255},0.55f*(1.f-ph/0.16f)));
            }
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
static void drawClouds(bool shadows=false) {
    Texture2D& c = g.assets.cloud;
    if (c.id == 0) return;
    for (int i = 0; i < 6; i++) {
        if (g.cloudScale[i] == 0) continue;
        float s = g.cloudScale[i] * 0.55f;
        DrawTexturePro(c, { 0, 0, (float)c.width, (float)c.height },
                       { g.cloudX[i], g.cloudY[i], c.width * s, c.height * s }, { 0, 0 }, 0.0f,
                       shadows ? Color{39,54,75,20} : Fade(WHITE, 0.45f));
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
            texFoot(g.assets.tent, cx - 28, fy);
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
    g.camX = roundf(g.camX-shakeX); g.camY = roundf(g.camY-shakeY);

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
                if (it.a != P_BUSH && it.a != P_BUSH_PLAIN && it.a != P_MUSHROOM)
                    DrawEllipse((int)fx, (int)fy - 2, w * 0.28f, 3.0f, Fade(BLACK, 0.18f));
                // Canopies breathe: a 1px sway, phased per tree so a grove
                // never moves in lockstep.
                bool canopy = (it.a == P_TREE0 || it.a == P_TREE1 || it.a == P_TREE2 ||
                               it.a == P_PINE0 || it.a == P_PINE1 || it.a == P_PINE2 ||
                               it.a == P_AMBER_OAK || it.a == P_COPPER_OAK ||
                               it.a == P_WILLOW || it.a == P_TREE_SNOW);
                float sway = canopy ? sinf(g.totalTime * 1.4f + hashf(it.b, it.c, 0x5eedU) * 6.28f) * 1.0f : 0.0f;
                texFoot(*t, fx + sway, fy);
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
                    // Lit windows: two warm panes on the facade, flickering
                    // very slightly so the village feels inhabited at night.
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
                // Footstep dust: a soft puff under the boots while walking.
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

    drawClouds(true);
    // Local weather is world-anchored; tiny flakes, falling leaves and fen
    // lights move behind the HUD rather than obscuring actors with a full wash.
    Biome biome=sampleGround((int)floorf(g.px),(int)floorf(g.py)).biome;
    if (biome==BI_FROST || biome==BI_AMBER || biome==BI_FEN) {
        int n=biome==BI_FROST?32:12;
        for (int i=0;i<n;++i) {
            float phase=hashf(i,3,g.worldSeed)*6.28f;
            float px=hashf(i,7,g.worldSeed)*VRES_W-g.camX+8*sinf(g.totalTime*0.7f+phase);
            float py=hashf(i,9,g.worldSeed)*VRES_H-g.camY+g.totalTime*(biome==BI_FROST?16.f:biome==BI_AMBER?9.f:-3.f);
            int dx=(int)(px-floorf(px/VRES_W)*VRES_W), dy=(int)(py-floorf(py/VRES_H)*VRES_H);
            Color c=biome==BI_FROST?Color{232,247,255,185}:biome==BI_AMBER?Color{238,185,94,200}:Color{220,242,162,175};
            DrawRectangle(dx,dy,1+(i%2),1,c);
        }
    }

    // Ambient life around the player (screen-anchored, cheap and cheerful):
    // butterflies over meadows by day, fireflies in the fen at night.
    {
        float px=g.px*TILE-g.camX, py=g.py*TILE-g.camY;
        bool day = nightAmt < 0.35f;
        if (day && (biome==BI_MEADOW || biome==BI_FOREST || biome==BI_AMBER)) {
            for (int i=0;i<3;i++) {
                float ph=g.totalTime*(0.35f+0.12f*i)+(float)i*2.1f;
                float bx=px+sinf(ph*1.7f)*46.f+sinf(ph*0.6f)*18.f;
                float by=py-18.f+sinf(ph*2.3f)*10.f+cosf(ph*1.1f)*6.f;
                float flap=fmodf(g.totalTime*9.f+(float)i*2.f,1.f)<0.5f?1.f:0.f;
                Color wc=(i==0)?Color{255,240,170,230}:(i==1)?Color{200,225,255,230}:Color{255,205,230,230};
                DrawRectangle((int)(bx-2-flap),(int)by,2,1,wc);
                DrawRectangle((int)bx,(int)by,2,1,wc);
            }
        }
        if (nightAmt > 0.25f && (biome==BI_FEN || biome==BI_FOREST || biome==BI_MEADOW)) {
            for (int i=0;i<10;i++) {
                float ph=g.totalTime*(0.5f+0.2f*(i%3))+(float)i*1.37f;
                float fx=px+sinf(ph)*70.f+sinf(ph*0.37f)*24.f;
                float fy=py-10.f+cosf(ph*0.83f)*34.f;
                float tw=0.5f+0.5f*sinf(g.totalTime*2.4f+(float)i*2.9f);
                DrawCircle((int)fx,(int)fy,1,Fade(Color{190,255,170,255},tw*nightAmt));
            }
        }
    }

    // Warm dawn/dusk tint when the sun is low but not yet night.
    {
        float sun=sinf(dayT*6.2831853f); // -1..1, same clock as nightAmt
        float dusk=(sun>-0.25f && sun<0.35f)?(1.f-fabsf(sun-0.05f)/0.4f):0.f;
        if (dusk>0.01f) DrawRectangle(0,0,VRES_W,VRES_H,Fade(Color{255,150,70,255},dusk*0.10f));
    }

    if (nightAmt > 0.001f) {
        DrawRectangle(0, 0, VRES_W, VRES_H, Fade(Color{ 20, 26, 60, 255 }, nightAmt * 0.40f));
    }

    g.camX = saveCamX; g.camY = saveCamY;
}

// ================================================================ HUD
// SNES travel HUD: navy windows with white double borders, like the
// originals. Zone + danger level top-left, gil top-right, party bottom-left.
// The full stat sheet is still in the pause menu.
void drawHUD() {
    const char* zone=zoneNameAt(g.px,g.py);
    char zoneBuf[96];
    snprintf(zoneBuf,sizeof(zoneBuf),"%s  Lv.%d",zone,zoneLevelAt(g.px,g.py));
    Vector2 zsz=measureText(zoneBuf,8);
    drawWindow({ 5, 5, zsz.x + 30, 20 });
    // Tiny two-colour compass needle.
    DrawTriangle({12,16},{16,14},{16,7},Color{250,218,149,255});
    DrawTriangle({20,12},{16,14},{16,21},Color{134,173,169,255});
    drawText(zoneBuf,27,11,Color{246,238,212,255},8);
    char buf[64]; snprintf(buf,sizeof(buf),"%d",g.gil);
    int gw=(int)measureText(buf,8).x+27, gx=VRES_W-5-gw;
    drawWindow({ (float)gx, 5, (float)gw, 20 });
    DrawTexture(g.assets.icon[ICON_COIN],gx+3,6,WHITE);
    drawText(buf,(float)gx+21,11,Color{247,211,135,255},8);

    constexpr int partyW=243, by=VRES_H-34;
    drawWindow({ 5, (float)by, (float)partyW, 29 });
    for (int i=0;i<3;++i) {
        const Hero& h=g.party[i]; int x=9+80*i;
        Texture2D f=g.assets.face[h.klass==KLASS_KNIGHT ? FACE_KNIGHT : h.klass==KLASS_MAGE ? FACE_MAGE : FACE_CLERIC];
        if (f.id) DrawTexturePro(f,{0,0,(float)f.width,(float)f.height},{(float)x,(float)by+5,17,18},{0,0},0,
                       h.ko ? Color{135,135,154,255} : WHITE);
        drawText(heroName(h.klass),x+20.f,by+5.f,h.ko ? Color{226,141,139,255} : WHITE,8);
        char lv[24]; snprintf(lv,sizeof(lv),"Lv.%d",h.level);
        drawText(lv,x+20.f,by+14.f,Color{165,180,215,255},8);
        // HP/MP gauges with white SNES borders.
        drawBar(x+44.f,by+15.f,32.f,4.f,(float)h.hp/std::max(1,h.maxHp),
                h.poisoned ? Color{187,138,192,255} : Color{96,220,120,255}, Color{24,34,68,255});
        drawBar(x+44.f,by+21.f,32.f,3.f,(float)h.mp/std::max(1,h.maxMp),
                Color{96,160,255,255}, Color{24,34,68,255});
        if (h.ko) drawText("KO",x+20.f,by+21.f,Color{255,110,110,255},8);
        else if (h.poisoned) drawText("PSN",x+20.f,by+21.f,Color{195,130,255,255},8);
        if(i<2) DrawRectangle(x+76,by+6,1,18,Color{71,85,87,255});
    }

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
        float px=VRES_W-pw-5;
        int py=px>partyW+8 ? VRES_H-30 : VRES_H-56;
        drawWindow({ px, (float)py, pw, 21 });
        drawTextCentered(prompt,px+pw*.5f,(float)py+7,Color{246,238,212,255},8);
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
// SNES dialogue: navy window, gold-framed portrait, name plate overlapping
// the top edge, and a blinking down-arrow when the line is fully revealed.
void drawDialogBox() {
    if (g.dialog.empty()) return;
    const DialogLine& line = g.dialog[std::min<int>(g.dialogIndex, (int)g.dialog.size() - 1)];
    float h = 80;
    float y = VRES_H - h - 6;
    drawWindow({ 6, y, VRES_W - 12, h });
    if (line.face >= 0 && line.face < FACE_COUNT && g.assets.face[line.face].id) {
        Texture2D& f = g.assets.face[line.face];
        DrawRectangle(13, (int)y + 9, 50, 50, Color{ 12, 16, 30, 255 });
        DrawTexturePro(f, { 0, 0, (float)f.width, (float)f.height }, { 14, y + 10, 48, 48 },
                       { 0, 0 }, 0.0f, WHITE);
        DrawRectangleLines(14, (int)y + 10, 48, 48, Color{ 255, 232, 150, 255 });
    }
    float tx = (line.face >= 0) ? 72.0f : 18.0f;
    float maxW = VRES_W - tx - 18.0f;
    if (!line.speaker.empty()) {
        Vector2 nsz = measureText(line.speaker.c_str(), 8);
        drawWindow({ tx - 4, y - 9, nsz.x + 16, 18 });
        drawText(line.speaker.c_str(), tx + 4, y - 4, Color{ 255, 232, 150, 255 }, 8);
    }

    // typewriter reveal with pixel-accurate word wrapping
    size_t nchars = (size_t)std::max(0.0f, std::min((float)line.text.size(), g.dialogReveal));
    std::string shown = line.text.substr(0, nchars);
    drawTextWrapped(shown.c_str(), tx, y + (line.speaker.empty() ? 12.0f : 16.0f), maxW, 12.0f, WHITE, 8);

    if (g.dialogReveal >= (float)line.text.size()) {
        bool last = (g.dialogIndex >= (int)g.dialog.size() - 1);
        // Blinking down-arrow (SNES "more" marker) + key hint.
        if (fmodf(g.totalTime * 2.0f, 1.0f) < 0.7f) {
            float ax = VRES_W - 30.0f, ay = y + h - 16.0f;
            DrawTriangle({ ax, ay }, { ax + 10, ay }, { ax + 5, ay + 7 }, Color{ 255, 232, 150, 255 });
        }
        const char* hint = last ? "SPACE" : "SPACE more";
        Vector2 sz = measureText(hint, 8);
        drawText(hint, VRES_W - sz.x - 44, y + h - 14, Color{ 180, 200, 255, 255 }, 8);
    }
}

// ================================================================ title
// SNES title presentation: scenic sky, layered gold logo with a soft drop
// shadow, navy menu windows with a blinking cursor, a walking party
// attract row, PRESS START blink, copyright and version lines.
void drawTitleScreen() {
    Texture2D& bg = g.assets.titleBg;
    if (bg.id) DrawTexturePro(bg, { 0, 0, (float)bg.width, (float)bg.height },
                              { 0, 0, VRES_W, VRES_H }, { 0, 0 }, 0.0f, WHITE);
    else ClearBackground(Color{ 12, 18, 40, 255 });

    drawClouds();

    DrawRectangleGradientV(0, 0, VRES_W, VRES_H / 2, Fade(BLACK, 0.35f), BLANK);
    DrawRectangleGradientV(0, VRES_H / 2, VRES_W, VRES_H / 2, BLANK, Fade(BLACK, 0.55f));

    // Twinkling sparkles over the sky.
    for (int i = 0; i < 14; i++) {
        float sx = hashf(i, 3, 0x77aaU) * VRES_W;
        float sy = hashf(i, 7, 0x99bbU) * 150.0f;
        float tw = 0.5f + 0.5f * sinf(g.titleT * 2.2f + (float)i * 1.9f);
        if (tw > 0.55f) DrawRectangle((int)sx, (int)sy, 2, 2, Fade(Color{ 255, 250, 220, 255 }, tw * 0.8f));
    }

    const char* title = "A PLACE FOR AGENTS";
    float bob = sinf(g.titleT * 1.6f) * 1.5f;
    // Layered logo: dark outline, navy mid, gold face.
    drawTextCentered(title, VRES_W * 0.5f + 3, 55 + bob, Color{ 12, 16, 30, 255 }, 24);
    drawTextCentered(title, VRES_W * 0.5f + 1, 53 + bob, Color{ 70, 90, 160, 255 }, 24);
    drawTextCentered(title, VRES_W * 0.5f,     52 + bob, Color{ 255, 236, 176, 255 }, 24);
    drawTextCentered("a field office at the edge of the map", VRES_W * 0.5f + 1, 85 + bob, Color{ 12, 16, 30, 200 }, 8);
    drawTextCentered("a field office at the edge of the map", VRES_W * 0.5f,     84 + bob, Color{ 230, 238, 255, 230 }, 8);

    const char* opts[3] = { "NEW ADVENTURE", "CONTINUE", "QUIT" };
    for (int i = 0; i < 3; i++) {
        bool sel = (g.titleIndex == i);
        bool disabled = (i == 1 && !g.saveExists);
        float w = 176;
        Rectangle box{ VRES_W * 0.5f - w * 0.5f, (float)(112 + i * 24), w, 20 };
        drawWindow(box);
        Color c = disabled ? Color{ 150, 160, 185, 190 }
                           : (sel ? Color{ 255, 236, 176, 255 } : Color{ 235, 240, 255, 245 });
        drawTextCentered(opts[i], VRES_W * 0.5f, box.y + 6, c, 8);
        if (sel && !disabled) drawCursor(box.x + 10, box.y + 6);
    }
    if (fmodf(g.titleT * 1.4f, 1.0f) < 0.65f)
        drawTextCentered("PRESS START", VRES_W * 0.5f, 190, Color{ 255, 236, 176, 255 }, 8);

    // Attract row: the party (and a slime) walk across the bottom forever.
    {
        float walk = fmodf(g.titleT * 26.0f, (float)(VRES_W + 120)) - 60.0f;
        float gy = VRES_H - 34.0f;
        DrawRectangle(0, (int)gy + 8, VRES_W, 2, Fade(Color{ 20, 30, 60, 255 }, 0.35f));
        for (int i = 0; i < 3; i++) {
            Texture2D& sheet = g.assets.heroWalk[i];
            if (sheet.id == 0) continue;
            float cx = walk + (float)i * 30.0f;
            float fy = gy + sinf(g.titleT * 9.0f + (float)i * 1.3f) * 1.5f;
            DrawEllipse((int)cx, (int)fy, 6.0f, 2.5f, Fade(BLACK, 0.25f));
            int frame = 1 + ((int)(g.titleT * 7.0f + (float)i) & 1);
            Rectangle src{ (float)(frame * 24), 64.0f, 24, 32 };
            DrawTexturePro(sheet, src, { cx - 12, fy - 32, 24, 32 }, { 0, 0 }, 0.0f, WHITE);
        }
        Texture2D& sl = g.assets.enemy[EN_SLIME];
        if (sl.id) {
            float cx = walk - 34.0f;
            float fy = gy + sinf(g.titleT * 6.0f) * 1.5f;
            DrawEllipse((int)cx, (int)fy, 8.0f, 3.0f, Fade(BLACK, 0.25f));
            texFoot(sl, cx, fy, WHITE, 0.62f);
        }
    }

    drawTextCentered("arrows / mouse to choose - SPACE to begin",
                     VRES_W * 0.5f, VRES_H - 22, Color{ 210, 220, 245, 220 }, 8);
    drawTextCentered("(C) 2026 A Place for Agents - art: OpenRTP - v0.2.0",
                     VRES_W * 0.5f, VRES_H - 11, Color{ 180, 190, 220, 180 }, 8);
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
        "Every map ends at the edge of Elmswick.",
        "That's where the field agents begin.",
        "Aldric keeps the way open.",
        "Iris looks for what maps leave out.",
        "Pip gets everyone safely home.",
        "Now the northern road has gone quiet.",
        "Three agents. One unfinished map.",
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
