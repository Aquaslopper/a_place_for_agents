// regions.cpp — three countries, two ranges, and the passes between them.
//
// Unlike the old sector ways, a range is not something a road can excavate.
// Its shape and its four narrow passes are derived from the seed, without any
// terrain, placement or save-state queries. This makes it safe for the Survey
// to use while it is choosing sites and routing roads.
#include "regions.hpp"
#include "world.hpp"
#include "atlas.hpp"
#include <cmath>
#include <cstdlib>

namespace {
constexpr float RING_BASE[2] = { 106.0f, 184.0f };
constexpr float RANGE_HALF_WIDTH = 5.0f;
constexpr float PASS_APPROACH = 18.0f;

float ringRadius(float x, float y, int ring) {
    float d = sqrtf(x * x + y * y);
    float nx = d > 0.001f ? x / d : 0.0f;
    float ny = d > 0.001f ? y / d : -1.0f;
    uint32_t salt = ring == 1 ? 0x1921acu : 0x775eb1u;
    float drift = (float)((int)(hash2i(ring, 71, g.worldSeed ^ salt) % 7u) - 3);
    float undulation = (smoothNoise(nx * 2.1f + 31.0f, ny * 2.1f - 18.0f,
                                    g.worldSeed ^ salt) - 0.5f) * 10.0f;
    return RING_BASE[ring - 1] + drift + undulation;
}

// Direction is north/east/south/west; a pass is a straight, seven-tile-wide
// defile through the entire thickness of the range. The north pass is centred
// on the North Road (and on the new frontier spur); all others are seed-placed.
int passOffset(int ring, int direction) {
    if (direction == 0) return 2;
    return (int)(hash2i(ring, direction, g.worldSeed ^ 0x490a7u) % 31u) - 15;
}

int directionOf(float x, float y) {
    if (fabsf(x) > fabsf(y)) return x > 0 ? 1 : 3;
    return y < 0 ? 0 : 2;
}
}

RegionCell regionCellAt(int tx, int ty) {
    float x = tx + 0.5f, y = ty + 0.5f;
    float d2 = x * x + y * y;
    // The noise can move either range by at most eight tiles. Most of the
    // world doesn't need a square root or any angular noise at all.
    int ring = 0;
    if (d2 >= 80.0f * 80.0f && d2 <= 133.0f * 133.0f) ring = 1;
    else if (d2 >= 158.0f * 158.0f && d2 <= 211.0f * 211.0f) ring = 2;
    if (!ring) return {};
    float delta = sqrtf(d2) - ringRadius(x, y, ring);
    if (fabsf(delta) > PASS_APPROACH) return {};
    int dir = directionOf(x, y);
    float across = (dir == 0 || dir == 2) ? x : y;
    if (fabsf(across - (float)passOffset(ring, dir) - 0.5f) <= 3.5f)
        return { REGION_PASS, ring, fabsf(delta) <= 3.0f };
    if (fabsf(delta) <= RANGE_HALF_WIDTH) return { REGION_RANGE, ring, false };
    return {};
}

bool regionNearRange(int tx, int ty, float margin) {
    float x = tx + 0.5f, y = ty + 0.5f;
    float d = sqrtf(x * x + y * y);
    for (int ring = 1; ring <= 2; ++ring) {
        if (fabsf(d - RING_BASE[ring - 1]) > RANGE_HALF_WIDTH + margin + 8.0f) continue;
        if (fabsf(d - ringRadius(x, y, ring)) <= RANGE_HALF_WIDTH + margin) return true;
    }
    return false;
}

int regionAt(float x, float y) {
    float d = sqrtf(x * x + y * y);
    if (d >= ringRadius(x, y, 2)) return 2;
    if (d >= ringRadius(x, y, 1)) return 1;
    return 0;
}

const char* regionNameAt(float x, float y) {
    switch (regionAt(x, y)) {
        case 0: return "Elmswick Vale";
        case 1: return "The Crownlands";
        default: return "The Far Marches";
    }
}

bool regionPassOpen(int ring) {
    if (ring == 1) return g.hasWaystone;
    if (ring == 2) return g.bossDefeated || g.legacyFarPass;
    return true;
}

void regionPassCenter(int ring, int direction, int& tx, int& ty) {
    ring = std::max(1, std::min(2, ring));
    direction = (direction % 4 + 4) % 4;
    float along = RING_BASE[ring - 1];
    float cross = (float)passOffset(ring, direction) + 0.5f;
    float x = 0.0f, y = 0.0f;
    // Iteration accounts for the angular undulation without assuming that a
    // north/south pass lies on the circle's exact axis.
    for (int i = 0; i < 4; ++i) {
        x = direction == 0 || direction == 2 ? cross : (direction == 1 ? along : -along);
        y = direction == 1 || direction == 3 ? cross : (direction == 2 ? along : -along);
        along = sqrtf(std::max(1.0f, ringRadius(x, y, ring) * ringRadius(x, y, ring) - cross * cross));
    }
    tx = (int)floorf(x); ty = (int)floorf(y);
}

bool regionLockedPassNear(float x, float y, int& ring) {
    for (int r = 1; r <= 2; ++r) {
        if (regionPassOpen(r)) continue;
        for (int dir = 0; dir < 4; ++dir) {
            int tx, ty; regionPassCenter(r, dir, tx, ty);
            float dx = x - (tx + 0.5f), dy = y - (ty + 0.5f);
            if (dx * dx + dy * dy <= 7.0f * 7.0f) { ring = r; return true; }
        }
    }
    return false;
}

void frontierCavernSite(int& tx, int& ty) {
    // Choose a different northern foothill in each world, but keep it inside
    // even the smallest possible coast and beyond the outer mountain range.
    uint32_t h = hash2i(11, 29, g.worldSeed ^ 0xf204u);
    tx = (int)(h % 65u) - 32;
    ty = -213 - (int)((h >> 8) % 11u);
}

bool frontierRoadAt(int tx, int ty) {
    int cx, cy; frontierCavernSite(cx, cy);
    // From the North Road at y=-126, skirt the castle, cross the north pass,
    // then branch towards this seed's cavern. Three-wide, with square joins:
    // the guaranteed route must not depend on a fortunate terrain roll.
    if (ty >= -127 && ty <= -125 && tx >= 0 && tx <= 15) return true;
    if (tx >= 13 && tx <= 15 && ty >= -164 && ty <= -125) return true;
    if (ty >= -166 && ty <= -164 && tx >= 1 && tx <= 15) return true;
    if (tx >= 1 && tx <= 3 && ty >= cy + 8 && ty <= -164) return true;
    if (ty >= cy + 7 && ty <= cy + 9 &&
        tx >= std::min(2, cx) - 1 && tx <= std::max(2, cx) + 1) return true;
    if (tx >= cx - 1 && tx <= cx + 1 && ty >= cy + 1 && ty <= cy + 9) return true;
    return false;
}

// ================================================================ world-map view
// This is not a second, disconnected map. It is a compressed view of the very
// same coordinates and collision grid: encounters, party position, passes and
// destinations are shared with the close-up field. Sampling a small tile atlas
// keeps the map cheap enough to move on while retaining its 16-bit palette.
void drawTravelMap() {
    constexpr int STEP = 2;
    constexpr int PIXELS = (int)(STEP * TRAVEL_PIXELS_PER_TILE);
    struct MapView {
        uint32_t seed = 0;
        int x = 0, y = 0, w = 0, h = 0, access = -1;
        bool valid = false;
        std::vector<uint8_t> surfaces;
        std::vector<AtlasSite> places;
        std::vector<AtlasRoad> roads;
    };
    static MapView map;
    int w = (g.vw + PIXELS - 1) / PIXELS + 16;
    int h = (g.vh + PIXELS - 1) / PIXELS + 16;
    int anchorX = (int)floorf(g.px / 16.0f) * 16;
    int anchorY = (int)floorf(g.py / 16.0f) * 16;
    int x0 = anchorX - (w / 2) * STEP, y0 = anchorY - (h / 2) * STEP;
    int access = (regionPassOpen(1) ? 1 : 0) | (regionPassOpen(2) ? 2 : 0);
    if (!map.valid || map.seed != g.worldSeed || map.x != x0 || map.y != y0 ||
        map.w != w || map.h != h || map.access != access) {
        map.valid = true; map.seed = g.worldSeed; map.x = x0; map.y = y0;
        map.w = w; map.h = h; map.access = access;
        map.surfaces.resize((size_t)w * h);
        for (int by = 0; by < h; ++by) for (int bx = 0; bx < w; ++bx) {
            int tx = x0 + bx * STEP, ty = y0 + by * STEP;
            SurveyTerrain t = worldSurveyTerrain(tx, ty);
            int s = 3; // landscape.png surface rows: meadow
            if (inOpenSea((float)tx + 0.5f, (float)ty + 0.5f) || t.deep) s = 0;
            else if (t.water) s = 1;
            else if (t.ice) s = 10;
            else if (t.crag) s = 11;
            else switch (t.biome) {
                case BI_FOREST: s = 4; break;
                case BI_AMBER:  s = 5; break;
                case BI_HEATH:  s = 6; break;
                case BI_FEN:    s = 7; break;
                case BI_DUNES:  s = 8; break;
                case BI_FROST:  s = 9; break;
                default: break;
            }
            RegionCell rc = regionCellAt(tx, ty);
            if (rc.terrain == REGION_RANGE || (rc.gate && !regionPassOpen(rc.ring))) s = 11;
            else if (rc.terrain == REGION_PASS || frontierRoadAt(tx, ty)) s = 12;
            map.surfaces[(size_t)by * w + bx] = (uint8_t)s;
        }
        atlasSitesInRect(x0, y0, x0 + w * STEP, y0 + h * STEP, map.places);
        atlasRoadsInRect(x0, y0, x0 + w * STEP, y0 + h * STEP, map.roads);
    }
    auto screen = [](float tx, float ty) {
        return Vector2{g.vw * 0.5f + (tx - g.px) * TRAVEL_PIXELS_PER_TILE,
                       g.vh * 0.5f + (ty - g.py) * TRAVEL_PIXELS_PER_TILE};
    };
    ClearBackground(Color{38, 68, 96, 255});
    for (int by = 0; by < h; ++by) for (int bx = 0; bx < w; ++bx) {
        Vector2 p = screen((float)(map.x + bx * STEP), (float)(map.y + by * STEP));
        if (p.x < -PIXELS || p.y < -PIXELS || p.x > g.vw || p.y > g.vh) continue;
        int row = map.surfaces[(size_t)by * w + bx] * 3;
        DrawTexturePro(g.assets.landscape, {15.0f * TILE, (float)(row * TILE), TILE, TILE},
                       {floorf(p.x), floorf(p.y), (float)PIXELS, (float)PIXELS}, {0, 0}, 0, WHITE);
    }
    // Survey roads are drawn under the symbols. The carved pass and its seal
    // still win in actual ground/collision, even when a road reaches the gate.
    for (const AtlasRoad& road : map.roads) for (size_t i = 1; i < road.pts.size(); ++i) {
        Vector2 a = screen((float)road.pts[i - 1].first, (float)road.pts[i - 1].second);
        Vector2 b = screen((float)road.pts[i].first, (float)road.pts[i].second);
        DrawLineEx(a, b, 1.8f, Color{218, 190, 135, 225});
    }
    auto line = [&](float ax, float ay, float bx, float by) {
        DrawLineEx(screen(ax, ay), screen(bx, by), 2.0f, Color{218, 190, 135, 235});
    };
    int fx, fy; frontierCavernSite(fx, fy);
    line(2, -126, 14, -126); line(14, -126, 14, -165);
    line(14, -165, 2, -165); line(2, -165, 2, (float)fy + 8);
    line(2, (float)fy + 8, (float)fx, (float)fy + 8);
    line((float)fx, (float)fy + 8, (float)fx, (float)fy + 1);

    for (int ring = 1; ring <= 2; ++ring) for (int dir = 0; dir < 4; ++dir) {
        int tx, ty; regionPassCenter(ring, dir, tx, ty);
        Vector2 p = screen(tx + 0.5f, ty + 0.5f);
        if (p.x < -10 || p.y < -10 || p.x > g.vw + 10 || p.y > g.vh + 10) continue;
        Color c = regionPassOpen(ring) ? Color{255, 225, 145, 255} : Color{220, 105, 95, 255};
        DrawRectangle((int)p.x - 3, (int)p.y - 3, 7, 7, Color{32, 33, 48, 255});
        DrawRectangle((int)p.x - 2, (int)p.y - 2, 5, 5, c);
    }
    auto icon = [&](Texture2D tex, float tx, float ty, Color color) {
        Vector2 p = screen(tx, ty);
        if (p.x < -16 || p.y < -16 || p.x > g.vw + 16 || p.y > g.vh + 16) return;
        if (tex.id) DrawTexturePro(tex, {0, 0, (float)tex.width, (float)tex.height},
                                   {floorf(p.x - 7), floorf(p.y - 10), 14, 14}, {0, 0}, 0, color);
        else DrawRectangle((int)p.x - 3, (int)p.y - 3, 6, 6, color);
    };
    for (const AtlasSite& place : map.places) {
        Texture2D tex = place.kind == AS_VILLAGE ? g.assets.house[0]
                      : (place.kind == AS_CAVERN ? g.assets.caveMouth : g.assets.statusStone[0]);
        icon(tex, (float)place.tx, (float)place.ty,
             place.kind == AS_CAVERN ? Color{230, 220, 205, 255} : WHITE);
    }
    int hx, hy; startCavernSite(hx, hy);
    icon(g.assets.house[0], 0, 0, WHITE);
    icon(g.assets.caveMouth, (float)hx, (float)hy, WHITE);
    Landmark north = northRoadCavern();
    icon(g.assets.caveMouth, (float)north.tx, (float)north.ty, WHITE);
    icon(g.assets.castle, (float)BOSS_CASTLE_TX, (float)BOSS_CASTLE_TY, WHITE);
    icon(g.assets.caveMouth, (float)fx, (float)fy, Color{255, 225, 175, 255});
    icon(g.assets.tower, (float)TOWER_SITE_TX, (float)TOWER_SITE_TY, WHITE);
    icon(g.assets.statusStone[0], (float)LOCKET_SITE_TX, (float)LOCKET_SITE_TY, WHITE);
    for (const FieldMonster& m : gFieldMonsters) {
        Vector2 p = screen(m.x, m.y);
        if (p.x >= 0 && p.y >= 0 && p.x < g.vw && p.y < g.vh)
            DrawCircle((int)p.x, (int)p.y, 2, Color{225, 75, 73, 255});
    }
    Texture2D hero = g.assets.heroWalk[std::max(0, std::min(2, g.party[0].klass))];
    int row = g.facing == 0 ? 0 : (g.facing == 1 ? 1 : 2);
    int frame = g.moving ? 1 + ((int)(g.walkAnimT * 7.0f) & 1) : 0;
    float sw = g.facing == 2 && g.sideFlip ? -24.0f : 24.0f;
    Vector2 p = screen(g.px, g.py);
    DrawEllipse((int)p.x, (int)p.y + 4, 5, 2, Fade(BLACK, 0.35f));
    if (hero.id) DrawTexturePro(hero, {(float)(frame * 24), (float)(row * 32), sw, 32},
                                {floorf(p.x - 6), floorf(p.y - 12), 12, 16}, {0, 0}, 0, WHITE);
    else DrawCircle((int)p.x, (int)p.y, 3, WHITE);

    drawWorldLabel(regionNameAt(g.px, g.py), g.vw * 0.5f, 29, Color{255, 235, 184, 255}, 8);
    drawWindowBlue({(float)g.vw - 122, 49, 117, 19});
    drawText("M: CLOSE VIEW", (float)g.vw - 116, 55, Color{226, 236, 255, 255}, 8);
}
