// world.cpp — unified unbounded Lufia II-style overworld runtime, site transitions,
// SNES autotiled overworld renderer, and [M] parchment world map overview.
#include "world.hpp"
#include "overworld_gen.hpp"
#include "town.hpp"
#include "battle.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <unordered_map>
#include <vector>

namespace {

static uint32_t sWorldSeed = 0;
static std::string sPromptBuf;
static OwRegion sCompatRegion;
static OwPlate  sCompatPlate;

static void drawAtlasCell(int col, int row, float dx, float dy) {
    if (g.assets.owAtlas.id == 0) return;
    Rectangle src{ (float)(col * 16), (float)(row * 16), 16.0f, 16.0f };
    Rectangle dst{ dx, dy, 16.0f, 16.0f };
    DrawTexturePro(g.assets.owAtlas, src, dst, { 0, 0 }, 0.0f, WHITE);
}

static Texture2D* siteIconTexture(const apfa::OwSiteInfo& s) {
    switch (s.type) {
        case apfa::OWS_HAMLET:
            if (s.climate == apfa::OWC_SNOW && g.assets.owTownSnow.id) return &g.assets.owTownSnow;
            return g.assets.owHamlet.id ? &g.assets.owHamlet : &g.assets.owTownRed;
        case apfa::OWS_TOWN:
            if (s.climate == apfa::OWC_SNOW && g.assets.owTownSnow.id) return &g.assets.owTownSnow;
            return g.assets.owTown.id ? &g.assets.owTown : &g.assets.owTownRed;
        case apfa::OWS_CITY:
            return g.assets.owCastleBlue.id ? &g.assets.owCastleBlue : &g.assets.owTown;
        case apfa::OWS_CASTLE:
            return g.assets.owCastleGrey.id ? &g.assets.owCastleGrey : &g.assets.owCastleBlue;
        case apfa::OWS_CAVE:
            if (s.climate == apfa::OWC_SAND && g.assets.owCaveSand.id) return &g.assets.owCaveSand;
            if (s.climate == apfa::OWC_SNOW && g.assets.owCaveSnow.id) return &g.assets.owCaveSnow;
            return &g.assets.owCaveGrass;
        case apfa::OWS_TOWER:  return &g.assets.owTower;
        case apfa::OWS_TEMPLE: return &g.assets.owTemple;
        case apfa::OWS_RUINS:  return &g.assets.owRuins;
        case apfa::OWS_CAMP:   return &g.assets.owCamp;
        default: return nullptr;
    }
}

static int themeForDungeonSite(const apfa::OwSiteInfo& s) {
    if (s.isStartCavern) return TH_MOSSY;
    if (s.isBossCastle)  return TH_CRYPT;
    if (s.climate == apfa::OWC_SNOW) return TH_FROST;
    if (s.climate == apfa::OWC_SAND) return TH_SUNSCORCH;
    switch (s.type) {
        case apfa::OWS_CASTLE: return TH_CRYPT;
        case apfa::OWS_RUINS:  return TH_AMBER;
        case apfa::OWS_TEMPLE: return TH_HEATH;
        case apfa::OWS_TOWER:  return TH_CRYPT;
        default: return (int)(s.seed % 3u); // TH_MOSSY, TH_ROOTED, TH_AMBER
    }
}

static OwSiteKind toLegacySiteKind(apfa::OwSiteType t) {
    switch (t) {
        case apfa::OWS_HAMLET:
        case apfa::OWS_TOWN:
        case apfa::OWS_CITY:   return OW_SITE_TOWN;
        case apfa::OWS_CASTLE: return OW_SITE_CASTLE;
        case apfa::OWS_CAVE:   return OW_SITE_DUNGEON;
        case apfa::OWS_TOWER:  return OW_SITE_TOWER;
        case apfa::OWS_TEMPLE: return OW_SITE_TEMPLE;
        case apfa::OWS_RUINS:  return OW_SITE_RUINS;
        case apfa::OWS_CAMP:   return OW_SITE_CAMP;
        default: return OW_SITE_NONE;
    }
}

static OwTerrain toLegacyTerrain(apfa::OwTileKind k) {
    switch (k) {
        case apfa::OWT_DEEP_SEA:
        case apfa::OWT_SEA:           return OW_WATER;
        case apfa::OWT_FOREST_LEAF:
        case apfa::OWT_FOREST_PINE:
        case apfa::OWT_FOREST_SNOW:   return OW_FOREST;
        case apfa::OWT_SAND:          return OW_SAND;
        case apfa::OWT_SNOW:          return OW_SNOW;
        case apfa::OWT_MOUNTAIN:
        case apfa::OWT_MOUNTAIN_SNOW: return OW_MOUNTAIN;
        case apfa::OWT_BRIDGE_H:
        case apfa::OWT_BRIDGE_V:
        case apfa::OWT_PASS_H:
        case apfa::OWT_PASS_V:        return OW_DIRT;
        default:                      return OW_GRASS;
    }
}

static uint32_t activeSeed() {
    return g.worldSeed ? g.worldSeed : (sWorldSeed ? sWorldSeed : 1337u);
}

static bool hasRoadConn(uint32_t seed, int tx, int ty) {
    auto t = apfa::owSampleTile(seed, tx, ty);
    return t.hasRoad || t.kind == apfa::OWT_BRIDGE_H || t.kind == apfa::OWT_BRIDGE_V ||
           t.kind == apfa::OWT_PASS_H || t.kind == apfa::OWT_PASS_V;
}

} // namespace

// ================================================================ Lifecycle
void worldInit(uint32_t seed) {
    sWorldSeed = seed ? seed : 1337u;
}

void worldReset(uint32_t seed) {
    worldInit(seed);
}

const OwRegion& worldGetRegion(int rx, int ry) {
    sCompatRegion.rx = rx;
    sCompatRegion.ry = ry;
    sCompatRegion.seed = activeSeed();
    if (sCompatRegion.plates.empty()) sCompatRegion.plates.resize(OW_REGION_COLS * OW_REGION_ROWS);
    return sCompatRegion;
}

const OwPlate& worldGetPlateAtTile(int tx, int ty) {
    auto t = apfa::owSampleTile(activeSeed(), tx, ty);
    sCompatPlate.terrain = toLegacyTerrain(t.kind);
    apfa::OwSiteInfo s;
    sCompatPlate.site = apfa::owSiteAtTile(activeSeed(), tx, ty, s)
                      ? toLegacySiteKind(s.type) : OW_SITE_NONE;
    sCompatPlate.isTransporter = (sCompatPlate.site != OW_SITE_NONE);
    return sCompatPlate;
}

OwTerrain worldTerrainAt(int tx, int ty) {
    return toLegacyTerrain(apfa::owSampleTile(activeSeed(), tx, ty).kind);
}

bool worldTileWalkable(int tx, int ty) {
    auto t = apfa::owSampleTile(activeSeed(), tx, ty);
    if (g.onShip) return apfa::owIsSailable(t.kind) || apfa::owIsWalkableOnFoot(t.kind);
    return apfa::owIsWalkableOnFoot(t.kind);
}

bool worldTileIsWater(int tx, int ty) {
    return apfa::owIsWater(apfa::owSampleTile(activeSeed(), tx, ty).kind);
}

OwSiteKind worldSiteAt(int tx, int ty) {
    apfa::OwSiteInfo s;
    if (apfa::owSiteAtTile(activeSeed(), tx, ty, s)) return toLegacySiteKind(s.type);
    return OW_SITE_NONE;
}

const char* worldSitePrompt(int tx, int ty) {
    apfa::OwSiteInfo s;
    if (!apfa::owSiteAtTile(activeSeed(), tx, ty, s)) return nullptr;
    const char* verb = "Enter";
    if (s.tideIslandIdx >= 0) verb = "Inspect";
    else if (s.isLocketSite && !g.locketFound) verb = "Search";
    sPromptBuf = std::string("[Z / Enter] ") + verb + " " + s.name;
    return sPromptBuf.c_str();
}

// ================================================================ Transitions
bool worldEnterSiteAt(int tx, int ty) {
    uint32_t seed = activeSeed();
    apfa::OwSiteInfo site;
    if (!apfa::owSiteAtTile(seed, tx, ty, site)) return false;

    // Save exact overworld coordinate before leaving MODE_OVERWORLD
    g.owReturnX = (float)site.tx + 0.5f;
    g.owReturnY = (float)site.ty + 0.5f;
    g.owLastSiteTx = site.tx;
    g.owLastSiteTy = site.ty;

    // 1. Settlements -> real bounded walled Lufia II Town!
    if (apfa::owIsSettlement(site.type)) {
        townEnter(site, 2);
        return true;
    }

    // 2. Tidewell Ocean Shrines (QUEST_TIDE)
    if (site.tideIslandIdx >= 0) {
        int bit = 1 << site.tideIslandIdx;
        if (!(g.tideMask & bit)) {
            g.tideMask |= bit;
            g.tideFound = std::min(3, g.tideFound + 1);
            if (g.quests[QUEST_TIDE].state == QS_UNKNOWN) {
                questSetState(QUEST_TIDE, QS_ACTIVE);
                g.quests[QUEST_TIDE].goal = 3;
            }
            g.quests[QUEST_TIDE].progress = g.tideFound;
            if (g.tideFound >= 3) questSetState(QUEST_TIDE, QS_DONE);
            playSoundIf(g.assets.chime);
            for (int i = 0; i < 3; ++i) {
                g.party[i].ko = false;
                g.party[i].hp = g.party[i].maxHp;
                g.party[i].mp = g.party[i].maxMp;
            }
            char buf[220];
            std::snprintf(buf, sizeof(buf),
                "You kindle the Tidewell on %s! Party fully restored. (%d / 3 Tidewells lit)",
                site.name.c_str(), g.tideFound);
            g.dialogReturn = MODE_OVERWORLD;
            startDialog({ { -1, site.name, buf } }, DLG_NONE);
        } else {
            g.dialogReturn = MODE_OVERWORLD;
            startDialog({ { -1, site.name, "The Tidewell's lantern glows steadily above the waves." } }, DLG_NONE);
        }
        g.owSiteDebounce = 0.85f;
        return true;
    }

    // 3. Standing Stones — Maren's Silver Locket discovery!
    if (site.isLocketSite && !g.locketFound) {
        g.locketFound = true;
        addItem(IT_LOCKET, 1, true);
        if (g.quests[QUEST_LOCKET].state == QS_ACTIVE)
            g.quests[QUEST_LOCKET].progress = 1;
        playSoundIf(g.assets.treasure);
        g.owSiteDebounce = 0.85f;
        g.dialogReturn = MODE_OVERWORLD;
        startDialog({
            { -1, "Standing Stones",
              "Glinting beneath the moss at the foot of the stones lies Maren's Silver Locket! Return it to her in Elmswick." }
        }, DLG_NONE);
        return true;
    }

    // 4. Castle of Elm — Slime King showdown!
    if (site.isBossCastle && !g.bossDefeated) {
        g.owSiteDebounce = 1.0f;
        g.dialogReturn = MODE_OVERWORLD;
        startDialog({
            { FACE_SLIME, "Slime King",
              "Gloop! You dare march into Castle of Elm?! Face the royal jelly!" }
        }, DLG_BOSS_SLIME_KING);
        return true;
    }

    // 5. Old Watchtower first-visit treasure!
    if (site.isTowerSite && !g.towerChestOpened) {
        g.towerChestOpened = true;
        addItem(IT_SWORD_IRON, 1, true);
        addItem(IT_HIPOTION, 2, true);
        playSoundIf(g.assets.treasure);
        g.owSiteDebounce = 0.85f;
        g.dialogReturn = MODE_OVERWORLD;
        startDialog({
            { -1, "Old Watchtower",
              "At the top of the crumbling watchtower you recover a sealed garrison chest: Iron Sword and 2 Hi-Potions!" }
        }, DLG_NONE);
        return true;
    }

    // 6. All Caves, Towers, Temples, Ruins, Castles -> Multi-floor Procedural Dungeon!
    if (apfa::owIsDungeonSite(site.type)) {
        int theme = themeForDungeonSite(site);
        enterDungeonNamed(site.tx, site.ty, site.isStartCavern, site.name.c_str(), theme);
        return true;
    }

    return false;
}

void worldReturnToOverworld() {
    g.mode = MODE_OVERWORLD;
    g.px = g.owReturnX;
    g.py = g.owReturnY;
    g.owSiteDebounce = 0.85f;
    g.fade = 0.85f;
    g.fadeDir = -1;
    overworldSnapCamera();
}

// ================================================================ Lufia II Overworld Renderer
void worldDrawOverworld() {
    uint32_t seed = activeSeed();
    ClearBackground(Color{ 30, 74, 114, 255 });

    int x0 = (int)floorf(g.camX / TILE) - 2;
    int y0 = (int)floorf(g.camY / TILE) - 2;
    int x1 = (int)ceilf((g.camX + (float)g.vw) / TILE) + 2;
    int y1 = (int)ceilf((g.camY + (float)g.vh) / TILE) + 2;

    int waterFrame = ((int)(g.totalTime * 2.4f)) & 3;

    // Pass 1: Autotiled SNES Ocean, Rounded Coastlines, Biome Overlays, Forests, Mountains, Roads
    for (int ty = y0; ty <= y1; ++ty) {
        for (int tx = x0; tx <= x1; ++tx) {
            auto cell = apfa::owSampleTile(seed, tx, ty);
            float dx = floorf((float)(tx * TILE) - g.camX);
            float dy = floorf((float)(ty * TILE) - g.camY);

            // Animated sparkling ocean always laid underneath so rounded shorelines sit over live waves
            if (cell.kind == apfa::OWT_DEEP_SEA) {
                drawAtlasCell(4 + ((waterFrame + tx + ty) & 3), 0, dx, dy);
                continue;
            }
            drawAtlasCell((waterFrame + ((tx + ty) & 1)) & 3, 0, dx, dy);
            if (cell.kind == apfa::OWT_SEA) continue;

            // Land coastline cardinal mask (N=1, E=2, S=4, W=8)
            auto kN = apfa::owSampleTile(seed, tx, ty - 1).kind;
            auto kE = apfa::owSampleTile(seed, tx + 1, ty).kind;
            auto kS = apfa::owSampleTile(seed, tx, ty + 1).kind;
            auto kW = apfa::owSampleTile(seed, tx - 1, ty).kind;

            int lmask = 0;
            if (apfa::owIsLand(kN)) lmask |= 1;
            if (apfa::owIsLand(kE)) lmask |= 2;
            if (apfa::owIsLand(kS)) lmask |= 4;
            if (apfa::owIsLand(kW)) lmask |= 8;

            bool isSand = (cell.kind == apfa::OWT_SAND);
            bool isSnow = (cell.kind == apfa::OWT_SNOW || cell.kind == apfa::OWT_FOREST_SNOW ||
                           cell.kind == apfa::OWT_MOUNTAIN_SNOW);

            if (isSand && lmask != 15) {
                drawAtlasCell(lmask, 2, dx, dy);
            } else if (isSnow && lmask != 15) {
                drawAtlasCell(lmask, 3, dx, dy);
            } else {
                drawAtlasCell(lmask, 1, dx, dy);
                if (isSand) {
                    int sm = 0;
                    if (kN == apfa::OWT_SAND || apfa::owIsWater(kN)) sm |= 1;
                    if (kE == apfa::OWT_SAND || apfa::owIsWater(kE)) sm |= 2;
                    if (kS == apfa::OWT_SAND || apfa::owIsWater(kS)) sm |= 4;
                    if (kW == apfa::OWT_SAND || apfa::owIsWater(kW)) sm |= 8;
                    drawAtlasCell(sm, 4, dx, dy);
                } else if (isSnow) {
                    auto snowish = [](apfa::OwTileKind k) {
                        return k == apfa::OWT_SNOW || k == apfa::OWT_FOREST_SNOW ||
                               k == apfa::OWT_MOUNTAIN_SNOW || apfa::owIsWater(k);
                    };
                    int sm = 0;
                    if (snowish(kN)) sm |= 1;
                    if (snowish(kE)) sm |= 2;
                    if (snowish(kS)) sm |= 4;
                    if (snowish(kW)) sm |= 8;
                    drawAtlasCell(sm, 5, dx, dy);
                }
            }

            // Forests, Swamp, Mountain Ranges
            if (cell.kind == apfa::OWT_FOREST_LEAF || cell.kind == apfa::OWT_FOREST_PINE ||
                cell.kind == apfa::OWT_FOREST_SNOW) {
                auto isF = [](apfa::OwTileKind k) {
                    return k == apfa::OWT_FOREST_LEAF || k == apfa::OWT_FOREST_PINE || k == apfa::OWT_FOREST_SNOW;
                };
                int fmask = 0;
                if (isF(kN)) fmask |= 1;
                if (isF(kE)) fmask |= 2;
                if (isF(kS)) fmask |= 4;
                if (isF(kW)) fmask |= 8;
                int row = (cell.kind == apfa::OWT_FOREST_LEAF) ? 6
                        : (cell.kind == apfa::OWT_FOREST_PINE ? 7 : 8);
                drawAtlasCell(fmask, row, dx, dy);
            } else if (cell.kind == apfa::OWT_SWAMP) {
                int sm = 0;
                if (kN == apfa::OWT_SWAMP) sm |= 1;
                if (kE == apfa::OWT_SWAMP) sm |= 2;
                if (kS == apfa::OWT_SWAMP) sm |= 4;
                if (kW == apfa::OWT_SWAMP) sm |= 8;
                drawAtlasCell(sm, 13, dx, dy);
            } else if (cell.kind == apfa::OWT_MOUNTAIN || cell.kind == apfa::OWT_MOUNTAIN_SNOW) {
                int mm = 0;
                if (apfa::owIsMountain(kN)) mm |= 1;
                if (apfa::owIsMountain(kE)) mm |= 2;
                if (apfa::owIsMountain(kS)) mm |= 4;
                if (apfa::owIsMountain(kW)) mm |= 8;
                drawAtlasCell(mm, (cell.kind == apfa::OWT_MOUNTAIN_SNOW) ? 10 : 9, dx, dy);
            } else if (cell.kind == apfa::OWT_SAND && !cell.hasRoad && lmask == 15) {
                // Rare scenic desert cactus / dune accent
                uint32_t h = apfa::owHash2(tx, ty, seed ^ 0xCA07u);
                if ((h % 19u) == 0u) drawAtlasCell(13, 0, dx, dy);
                else if ((h % 29u) == 0u) drawAtlasCell(14, 0, dx, dy);
            }

            // Stone Bridges, Mountain Passes, Golden Overworld Roads
            if (cell.kind == apfa::OWT_BRIDGE_H) {
                drawAtlasCell(8, 0, dx, dy);
            } else if (cell.kind == apfa::OWT_BRIDGE_V) {
                drawAtlasCell(9, 0, dx, dy);
            } else if (cell.kind == apfa::OWT_PASS_V) {
                drawAtlasCell(10, 0, dx, dy);
            } else if (cell.kind == apfa::OWT_PASS_H) {
                drawAtlasCell(11, 0, dx, dy);
            } else if (cell.hasRoad) {
                int rmask = 0;
                if (hasRoadConn(seed, tx, ty - 1)) rmask |= 1;
                if (hasRoadConn(seed, tx + 1, ty)) rmask |= 2;
                if (hasRoadConn(seed, tx, ty + 1)) rmask |= 4;
                if (hasRoadConn(seed, tx - 1, ty)) rmask |= 8;
                drawAtlasCell(rmask, 11, dx, dy);
            }
        }
    }

    // Pass 2: Y-Sorted Miniature Lufia II Site Icons + Hero / Ship!
    std::vector<apfa::OwSiteInfo> sites;
    apfa::owSitesInRect(seed, x0 - 2, y0 - 2, x1 + 2, y1 + 2, sites);

    struct OwDraw { float sortY; int kind; int idx; };
    std::vector<OwDraw> list;
    list.reserve(sites.size() + 1);
    for (int i = 0; i < (int)sites.size(); ++i)
        list.push_back({ (float)(sites[i].ty + 1) * TILE - 1.0f, 0, i });
    list.push_back({ (g.py + 0.35f) * TILE, 1, 0 });

    std::stable_sort(list.begin(), list.end(),
                     [](const OwDraw& a, const OwDraw& b) { return a.sortY < b.sortY; });

    for (const auto& it : list) {
        if (it.kind == 0) {
            const auto& s = sites[it.idx];
            Texture2D* tex = siteIconTexture(s);
            if (!tex || tex->id == 0) continue;
            float cx = ((float)s.tx + 0.5f) * TILE - g.camX;
            float fy = ((float)s.ty + 0.92f) * TILE - g.camY;
            DrawEllipse((int)cx, (int)fy - 1, (float)tex->width * 0.36f, 3.0f, Fade(BLACK, 0.28f));
            DrawTexturePro(*tex, { 0, 0, (float)tex->width, (float)tex->height },
                           { floorf(cx - (float)tex->width * 0.5f), floorf(fy - (float)tex->height),
                             (float)tex->width, (float)tex->height },
                           { 0, 0 }, 0.0f, WHITE);

            // Subtle quest shimmer on unvisited key sites (Locket, Watchtower, Boss Castle, Tidewells)
            bool questGlint = (s.isLocketSite && !g.locketFound) ||
                              (s.isTowerSite && !g.towerChestOpened) ||
                              (s.isBossCastle && !g.bossDefeated) ||
                              (s.tideIslandIdx >= 0 && !(g.tideMask & (1 << s.tideIslandIdx)));
            if (questGlint) {
                float pulse = 0.5f + 0.5f * sinf(g.totalTime * 4.0f + (float)s.tx);
                DrawCircleGradient((int)cx, (int)(fy - (float)tex->height * 0.5f), 11.0f + pulse * 4.0f,
                                   Fade(Color{ 255, 232, 130, 255 }, 0.25f + pulse * 0.12f), BLANK);
            }

            // Crisp floating label when player is within 3 tiles of this site
            float pdx = g.px - ((float)s.tx + 0.5f);
            float pdy = g.py - ((float)s.ty + 0.5f);
            if (pdx * pdx + pdy * pdy < 3.6f * 3.6f) {
                drawWorldLabel(s.name.c_str(), cx, fy - (float)tex->height - 10.0f,
                               apfa::owIsSettlement(s.type) ? Color{ 255, 240, 175, 255 }
                                                            : Color{ 255, 195, 165, 255 }, 8);
            }
        } else {
            float cx = g.px * TILE - g.camX;
            float fy = (g.py + 0.35f) * TILE - g.camY;
            if (g.onShip && g.assets.boat.id != 0) {
                float bob = sinf(g.totalTime * 3.4f) * 1.4f;
                // Gentle foam wake under the hull
                DrawEllipse((int)cx, (int)(fy + 1), 9.0f, 3.5f, Fade(Color{ 220, 245, 255, 255 }, 0.45f));
                int row = (g.facing == 0) ? 0 : (g.facing == 1 ? 3 : (g.sideFlip ? 1 : 2));
                int frame = g.moving ? (((int)(g.walkAnimT * 5.0f)) % 3) : 1;
                Rectangle src{ (float)(frame * 24), (float)(row * 32), 24, 32 };
                Rectangle dst{ floorf(cx - 11.0f), floorf(fy - 25.0f + bob), 22.0f, 28.0f };
                DrawTexturePro(g.assets.boat, src, dst, { 0, 0 }, 0.0f, WHITE);
            } else {
                int klass = std::max(0, std::min(2, g.party[0].klass));
                Texture2D& sheet = g.assets.heroWalk[klass];
                int frame = g.moving ? (1 + ((int)(g.walkAnimT * 7.0f) & 1)) : 0;
                DrawEllipse((int)cx, (int)fy - 1, 5.2f, 2.4f, Fade(BLACK, 0.30f));
                if (sheet.id != 0) {
                    Rectangle src{ (float)(frame * 24), (float)(g.facing * 32), 24.0f, 32.0f };
                    if (g.facing == 2 && g.sideFlip) src.width = -24.0f;
                    // Classic SNES miniature overworld hero scale (~0.78x)
                    float dw = 19.0f, dh = 25.0f;
                    DrawTexturePro(sheet, src, { floorf(cx - dw * 0.5f), floorf(fy - dh), dw, dh },
                                   { 0, 0 }, 0.0f, WHITE);
                }
            }
        }
    }

    // Pass 3: [M] Parchment Overworld Map Overlay when toggled
    if (g.travelMap) {
        worldDrawOverviewMap();
    }
}

void worldDrawOverviewMap() {
    uint32_t seed = activeSeed();
    const int mapCols = 112;
    const int mapRows = 64;
    const int cellPx = 3;
    const int mapW = mapCols * cellPx; // 336
    const int mapH = mapRows * cellPx; // 192
    const float panelW = (float)mapW + 24.0f;
    const float panelH = (float)mapH + 46.0f;
    const float px0 = floorf(((float)g.vw - panelW) * 0.5f);
    const float py0 = floorf(((float)g.vh - panelH) * 0.5f);

    DrawRectangle(0, 0, g.vw, g.vh, Fade(BLACK, 0.55f));
    drawWindow({ px0, py0, panelW, panelH }, true);
    drawTextCentered("WORLD MAP   [M / ESC to close]", px0 + panelW * 0.5f, py0 + 7.0f,
                     Color{ 255, 232, 150, 255 }, 8);

    const float ox = px0 + 12.0f;
    const float oy = py0 + 21.0f;
    const int centerTx = (int)floorf(g.px);
    const int centerTy = (int)floorf(g.py);
    const int startTx = centerTx - mapCols / 2;
    const int startTy = centerTy - mapRows / 2;

    for (int r = 0; r < mapRows; ++r) {
        for (int c = 0; c < mapCols; ++c) {
            auto cell = apfa::owSampleTile(seed, startTx + c, startTy + r);
            Color col{ 70, 134, 52, 255 };
            switch (cell.kind) {
                case apfa::OWT_DEEP_SEA:      col = Color{ 26, 62, 108, 255 }; break;
                case apfa::OWT_SEA:           col = Color{ 44, 104, 154, 255 }; break;
                case apfa::OWT_GRASS:         col = Color{ 76, 138, 54, 255 }; break;
                case apfa::OWT_FOREST_LEAF:
                case apfa::OWT_FOREST_PINE:   col = Color{ 42, 92, 38, 255 }; break;
                case apfa::OWT_SAND:          col = Color{ 226, 198, 134, 255 }; break;
                case apfa::OWT_SNOW:          col = Color{ 230, 238, 244, 255 }; break;
                case apfa::OWT_FOREST_SNOW:   col = Color{ 168, 192, 196, 255 }; break;
                case apfa::OWT_SWAMP:         col = Color{ 62, 98, 72, 255 }; break;
                case apfa::OWT_MOUNTAIN:      col = Color{ 118, 96, 66, 255 }; break;
                case apfa::OWT_MOUNTAIN_SNOW: col = Color{ 168, 174, 184, 255 }; break;
                case apfa::OWT_BRIDGE_H:
                case apfa::OWT_BRIDGE_V:      col = Color{ 196, 150, 96, 255 }; break;
                case apfa::OWT_PASS_H:
                case apfa::OWT_PASS_V:        col = Color{ 182, 158, 110, 255 }; break;
                default: break;
            }
            if (cell.hasRoad && !apfa::owIsWater(cell.kind))
                col = Color{ 208, 176, 114, 255 };
            DrawRectangle((int)(ox + c * cellPx), (int)(oy + r * cellPx), cellPx, cellPx, col);
        }
    }

    // Draw sites on the overview map
    std::vector<apfa::OwSiteInfo> sites;
    apfa::owSitesInRect(seed, startTx, startTy, startTx + mapCols - 1, startTy + mapRows - 1, sites);
    for (const auto& s : sites) {
        if (s.tx < startTx || s.tx >= startTx + mapCols || s.ty < startTy || s.ty >= startTy + mapRows) continue;
        int sx = (int)(ox + (s.tx - startTx) * cellPx + 1);
        int sy = (int)(oy + (s.ty - startTy) * cellPx + 1);
        if (apfa::owIsSettlement(s.type)) {
            Color c = (s.type == apfa::OWS_CITY || s.isStartTown)
                    ? Color{ 255, 224, 96, 255 } : Color{ 255, 194, 112, 255 };
            DrawRectangle(sx - 2, sy - 2, 5, 5, Color{ 20, 16, 12, 255 });
            DrawRectangle(sx - 1, sy - 1, 3, 3, c);
        } else {
            DrawRectangle(sx - 1, sy - 1, 4, 4, Color{ 22, 12, 14, 255 });
            DrawRectangle(sx, sy, 2, 2, Color{ 235, 92, 92, 255 });
        }
    }

    // Blinking player marker at center
    int hx = (int)(ox + (centerTx - startTx) * cellPx + 1);
    int hy = (int)(oy + (centerTy - startTy) * cellPx + 1);
    bool blink = ((int)(g.totalTime * 4.5f) & 1) == 0;
    DrawRectangle(hx - 2, hy - 2, 5, 5, BLACK);
    DrawRectangle(hx - 1, hy - 1, 3, 3, blink ? WHITE : Color{ 120, 230, 255, 255 });

    // Legend bar
    float ly = oy + (float)mapH + 6.0f;
    DrawRectangle((int)ox + 4, (int)ly + 2, 5, 5, Color{ 255, 224, 96, 255 });
    drawText("Town / City", ox + 13, ly, Color{ 235, 232, 215, 255 }, 8);
    DrawRectangle((int)ox + 95, (int)ly + 2, 5, 5, Color{ 235, 92, 92, 255 });
    drawText("Dungeon / Ruin", ox + 104, ly, Color{ 235, 232, 215, 255 }, 8);
    DrawRectangle((int)ox + 206, (int)ly + 2, 5, 5, WHITE);
    drawText("You", ox + 215, ly, Color{ 235, 232, 215, 255 }, 8);

    std::string reg = apfa::owRegionTitle(seed, centerTx, centerTy);
    drawText(reg.c_str(), ox + (float)mapW - measureText(reg.c_str(), 8).x - 2, ly,
             Color{ 180, 220, 255, 255 }, 8);
}

// ================================================================ Legacy compat helpers
SurveyTerrain worldSurveyTerrain(int tx, int ty) {
    SurveyTerrain st;
    auto t = apfa::owSampleTile(activeSeed(), tx, ty);
    st.terrain = toLegacyTerrain(t.kind);
    st.water = apfa::owIsWater(t.kind);
    st.deep = (t.kind == apfa::OWT_DEEP_SEA);
    st.crag = apfa::owIsMountain(t.kind);
    st.forest = (t.kind == apfa::OWT_FOREST_LEAF || t.kind == apfa::OWT_FOREST_PINE || t.kind == apfa::OWT_FOREST_SNOW);
    st.sand = (t.kind == apfa::OWT_SAND);
    st.swamp = (t.kind == apfa::OWT_SWAMP);
    st.road = t.hasRoad;
    if (t.kind == apfa::OWT_SAND) st.biome = BI_DUNES;
    else if (t.kind == apfa::OWT_SNOW || t.kind == apfa::OWT_FOREST_SNOW || t.kind == apfa::OWT_MOUNTAIN_SNOW) st.biome = BI_FROST;
    else if (t.kind == apfa::OWT_SWAMP) st.biome = BI_FEN;
    else if (st.forest) st.biome = BI_FOREST;
    else st.biome = BI_MEADOW;
    st.elevation = st.water ? -0.3f : (st.crag ? 0.9f : 0.4f);
    return st;
}

bool worldNearSpecialSite(int tx, int ty) {
    apfa::OwSiteInfo s;
    return apfa::owNearestSite(activeSeed(), tx, ty, 3, s);
}

int worldTrailAt(int tx, int ty) {
    return apfa::owSampleTile(activeSeed(), tx, ty).hasRoad ? 1 : 0;
}

Landmark northRoadCavern() {
    auto c = apfa::owStartCavern(activeSeed());
    return Landmark{ LK_CAVERN, c.tx, c.ty, c.seed };
}

std::string compassHint(float fromX, float fromY, float toX, float toY) {
    float dx = toX - fromX, dy = toY - fromY;
    float d = std::sqrt(dx * dx + dy * dy);
    if (d < 2.0f) return "right here";
    std::string dir;
    if (dy < -2.0f) dir += "north";
    else if (dy > 2.0f) dir += "south";
    if (dx < -2.0f) dir += (dir.empty() ? "west" : "-west");
    else if (dx > 2.0f) dir += (dir.empty() ? "east" : "-east");
    if (dir.empty()) dir = "nearby";
    return dir;
}

int buildingDoorTx(const Building& b) { return b.tx; }
int buildingDoorTy(const Building& b) { return b.ty + 1; }

int tileAt(int tx, int ty) {
    return static_cast<int>(apfa::owSampleTile(activeSeed(), tx, ty).kind);
}

bool isTileSolid(int tx, int ty) {
    return !worldTileWalkable(tx, ty);
}

void ensureChunksAround(int tx, int ty) {
    (void)apfa::owSampleTile(activeSeed(), tx, ty);
}

void updateWorldStreaming(float dt) {
    (void)dt;
}
