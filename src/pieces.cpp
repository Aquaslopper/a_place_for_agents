// pieces.cpp — seam generation for the field's map pieces
#include "pieces.hpp"
#include "world.hpp"
#include "atlas.hpp"
#include "regions.hpp"
#include "commons.hpp"
#include <algorithm>
#include <cmath>

namespace {

inline uint32_t mix(uint32_t a, uint32_t b, uint32_t c) {
    a ^= b + 0x9e3779b9u + (a << 6) + (a >> 2);
    a ^= c + 0x85ebca6bu + (a << 6) + (a >> 2);
    a ^= a >> 15;
    a *= 0x2c1b3c6du;
    a ^= a >> 12;
    return a;
}

struct SeamRef {
    int seam = 0;      // the boundary line's index (shared by both pieces)
    int axis = 0;      // 0 = vertical seam (x = seam*PIECE_W), 1 = horizontal
    int depth = 0;     // tiles from the seam line, 0 = touching it
    int cross = 0;     // the piece index along the seam (gives each piece a gate)
    int along = 0;     // tile offset along the seam, 0..piece size-1
    bool valid = false;
};

SeamRef seamFor(int tx, int ty) {
    int px = pieceIndexOf(tx, PIECE_W);
    int py = pieceIndexOf(ty, PIECE_H);
    int lx = pieceLocalX(tx);
    int ly = pieceLocalY(ty);

    SeamRef best;
    int dL = lx, dR = PIECE_W - 1 - lx;
    best.seam = (dL <= dR) ? px : px + 1;
    best.axis = 0;
    best.depth = std::min(dL, dR);
    best.cross = py;
    best.along = ly;
    best.valid = true;

    int dT = ly, dB = PIECE_H - 1 - ly;
    int depthV = (dT <= dB) ? dT : dB;
    if (depthV < best.depth) {
        best.seam = (dT <= dB) ? py : py + 1;
        best.axis = 1;
        best.depth = depthV;
        best.cross = px;
        best.along = lx;
    }
    return best;
}

bool softGround(GroundKind k) {
    return k == G_GRASS || k == G_GRASS_DARK || k == G_AMBER || k == G_HEATH ||
           k == G_LEAVES || k == G_SWAMP;
}

// The seams step around everything the older systems authored into place: the
// village and its precinct, roads and wild ways, the mountain passes, dungeon
// aprons and story sites. A piece edge frames the *wilds*; it never fences the
// content a player has to reach.
bool seamProtected(int tx, int ty) {
    if (tx * tx + ty * ty < 20 * 20) return true;                 // Elmswick
    if (inVillage((float)tx + 0.5f, (float)ty + 0.5f)) return true;
    if (inCommons((float)tx + 0.5f, (float)ty + 0.5f)) return true;
    if (worldNearSpecialSite(tx, ty)) return true;
    if (worldTrailAt(tx, ty) != 0) return true;
    if (atlasRoadAt(tx, ty)) return true;
    if (frontierRoadAt(tx, ty)) return true;
    if (regionCellAt(tx, ty).terrain != REGION_NONE) return true;
    return false;
}

int seamKey(int seam, int axis, int cross) {
    return (int)(mix((uint32_t)g.worldSeed ^ 0x5eed1234u,
                     (uint32_t)(seam * 131 + axis * 7),
                     (uint32_t)(cross * 977 + 17)) & 0xffffffu);
}

} // namespace

// ================================================================ geometry
int pieceFloorDiv(int a, int b) {
    int q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) --q;
    return q;
}

int pieceIndexOf(int tileCoord, int size) {
    int origin = (size == PIECE_W) ? PIECE_OX : PIECE_OY;
    return pieceFloorDiv(tileCoord - origin, size);
}

int pieceLocalX(int tx) {
    int px = pieceIndexOf(tx, PIECE_W);
    return tx - (px * PIECE_W + PIECE_OX);
}
int pieceLocalY(int ty) {
    int py = pieceIndexOf(ty, PIECE_H);
    return ty - (py * PIECE_H + PIECE_OY);
}

void pieceOfTile(int tx, int ty, int& pieceX, int& pieceY) {
    pieceX = pieceIndexOf(tx, PIECE_W);
    pieceY = pieceIndexOf(ty, PIECE_H);
}

void pieceOriginPixels(float tileX, float tileY, float viewW, float viewH,
                       float& outX, float& outY) {
    int px = pieceIndexOf((int)floorf(tileX), PIECE_W);
    int py = pieceIndexOf((int)floorf(tileY), PIECE_H);
    float pieceW = PIECE_W * (float)TILE;
    float pieceH = PIECE_H * (float)TILE;
    // Camera clamped to the piece: it scrolls inside the map, stops at the
    // framed edge, and centres a piece that is smaller than a very wide window.
    float left = px * pieceW + PIECE_OX * (float)TILE;
    float top = py * pieceH + PIECE_OY * (float)TILE;
    float right = left + pieceW;
    float bottom = top + pieceH;
    if (viewW >= pieceW) outX = left - (viewW - pieceW) * 0.5f;
    else {
        float follow = tileX * TILE - viewW * 0.5f;
        outX = std::max(left, std::min(follow, right - viewW));
    }
    if (viewH >= pieceH) outY = top - (viewH - pieceH) * 0.5f;
    else {
        float follow = tileY * TILE - viewH * 0.5f;
        outY = std::max(top, std::min(follow, bottom - viewH));
    }
}

// ================================================================ seams
int pieceSeamStyle(int seamIndex, int axis, int cross) {
    int roll = seamKey(seamIndex, axis, cross) % 100;
    if (roll < 32) return SEAM_OPEN;      // an edge you can see through and pass
    if (roll < 72) return SEAM_WOOD;      // a treeline with a gate in it
    if (roll < 90) return SEAM_HEDGE;     // a low, dark hedge bank
    return SEAM_RIDGE;                    // a rock wall with a defile
}

int pieceSeamGate(int seamIndex, int axis, int cross) {
    int length = (axis == 0) ? PIECE_H : PIECE_W;
    int span = length - 10;                       // keep gates off the corners
    if (span < 1) span = 1;
    return 3 + (int)((seamKey(seamIndex, axis, cross) >> 9) % (unsigned)span);
}

bool pieceSeamGap(int seamIndex, int axis, int cross, int along) {
    int gate = pieceSeamGate(seamIndex, axis, cross);
    return along >= gate && along < gate + 4;
}

// ================================================================ terrain
void pieceDressGround(int tx, int ty, GroundTile& t) {
    if (!softGround(t.kind)) return;
    SeamRef s = seamFor(tx, ty);
    if (!s.valid || s.depth >= PIECE_BAND) return;
    if (seamProtected(tx, ty)) return;
    if (pieceSeamGap(s.seam, s.axis, s.cross, s.along)) return;

    switch (pieceSeamStyle(s.seam, s.axis, s.cross)) {
        case SEAM_OPEN:
            return;
        case SEAM_WOOD:
            // The shaded litter beneath a treeline, never a bare lawn stripe.
            if (t.kind != G_LEAVES) t.kind = G_GRASS_DARK;
            t.deco = 0;
            return;
        case SEAM_HEDGE:
            t.kind = G_GRASS_DARK;
            t.deco = 0;
            return;
        case SEAM_RIDGE:
            // A one-tile crest with a dark shelf under it reads as a fold in the
            // land, the way a 16-bit map walled an edge.
            if (s.depth == 0) { t.kind = G_ROCK; t.deco = 0; }
            else { t.kind = G_GRASS_DARK; t.deco = 0; }
            return;
        default:
            return;
    }
}

namespace {

struct Feature { int kind = 0; int cx = 0, cy = 0; };

Feature featureFor(int tx, int ty) {
    int px = pieceIndexOf(tx, PIECE_W);
    int py = pieceIndexOf(ty, PIECE_H);
    uint32_t h = mix((uint32_t)g.worldSeed ^ 0xf3a7u, (uint32_t)(px * 2654435761u),
                     (uint32_t)(py * 40503u + 11u));
    int ox = px * PIECE_W + PIECE_OX;
    int oy = py * PIECE_H + PIECE_OY;
    Feature f;
    f.kind = 1 + (int)((h >> 7) % 5u);          // 1..5; 5 is a clearing
    f.cx = ox + PIECE_W / 2 + (int)((h >> 3) % 11u) - 5;
    f.cy = oy + PIECE_H / 2 + (int)((h >> 15) % 7u) - 3;
    return f;
}

} // namespace

PropKind pieceFeatureProp(int tx, int ty) {
    // Keep the party out of the seams step: a feature never lands on authored
    // ground, and never inside a piece's border band.
    SeamRef s = seamFor(tx, ty);
    if (s.valid && s.depth < PIECE_BAND + 1) return P_NONE;
    if (seamProtected(tx, ty)) return P_NONE;

    GroundTile gt = sampleGround(tx, ty);
    if (!softGround(gt.kind)) return P_NONE;

    Feature f = featureFor(tx, ty);
    if (f.kind == 5) return P_NONE;            // a clearing: deliberately empty
    int dx = tx - f.cx, dy = ty - f.cy;
    if (dx < -3 || dx > 3 || dy < -2 || dy > 2) return P_NONE;
    uint32_t h = mix((uint32_t)(tx * 2654435761u), (uint32_t)(ty * 2246822519u), g.worldSeed ^ 0x9e11u);

    switch (f.kind) {
        case 1:  // copse
            if ((dx == 0 && dy == 0) || (dx == 1 && dy == 0) || (dx == 0 && dy == 1) ||
                (dx == 2 && dy == 1) || (dx == -1 && dy == 1))
                return (h % 3u == 0) ? P_TREE0 : P_TREE1;
            if ((dx == 2 && dy == -1) || (dx == -2 && dy == 0)) return P_BUSH;
            return P_NONE;
        case 2:  // boulder ring
            if (std::abs(dx) == 1 && std::abs(dy) <= 1) return (h % 4u == 0) ? P_BOULDER : P_ROCK0;
            if (dx == 0 && dy == 1 && (h % 3u) == 0) return P_ROCK1;
            return P_NONE;
        case 3:  // thicket
            if ((dx + dy) % 2 == 0 && std::abs(dx) <= 2 && std::abs(dy) <= 1)
                return (h % 5u == 0) ? P_MUSHROOM : P_BUSH;
            return P_NONE;
        case 4:  // fallen trunk & toadstools
            if (dy == 0 && dx >= -1 && dx <= 1) return P_STUMP;
            if (std::abs(dx) <= 2 && std::abs(dy) == 1 && (h % 100u) < 30u) return P_MUSHROOM;
            return P_NONE;
        default:
            return P_NONE;
    }
}

PropKind pieceBorderProp(int tx, int ty) {
    SeamRef s = seamFor(tx, ty);
    if (!s.valid) return P_NONE;

    // A gate and its approach are always clear: a seam may never wall the party
    // in, so the corridor is wider than the gate itself.
    int gate = pieceSeamGate(s.seam, s.axis, s.cross);
    bool corridor = (s.along >= gate - 2 && s.along <= gate + 5) && s.depth < 6;
    if (corridor) return P_NONE;

    if (s.depth >= PIECE_BAND) return P_NONE;
    if (seamProtected(tx, ty)) return P_NONE;
    if (pieceSeamGap(s.seam, s.axis, s.cross, s.along)) return P_NONE;

    GroundTile gt = sampleGround(tx, ty);
    if (!softGround(gt.kind)) return P_NONE;

    int style = pieceSeamStyle(s.seam, s.axis, s.cross);
    uint32_t h = mix((uint32_t)(tx * 73856093), (uint32_t)(ty * 19349663), g.worldSeed ^ 0xb0de11u);

    if (style == SEAM_WOOD) {
        // Trunks stand on alternate tiles of the crest so the line reads as a
        // treeline with depth, not a hedge of pixels.
        if (s.depth == 0 && ((s.along + s.seam + s.cross) & 1) == 0) {
            uint32_t r = h % 100u;
            if (r < 46) return P_TREE1;
            if (r < 74) return P_TREE0;
            if (r < 88) return P_PINE1;
            return P_PINE0;
        }
        if (s.depth == 1 && (h % 100u) < 14u) return P_BUSH_PLAIN;
        return P_NONE;
    }
    if (style == SEAM_HEDGE) {
        if (s.depth == 0 && ((s.along + s.cross) & 1) == 0) return P_BUSH;
        if (s.depth == 1 && (h % 100u) < 10u) return P_BUSH_PLAIN;
        return P_NONE;
    }
    if (style == SEAM_RIDGE) {
        if (s.depth == 1 && (h % 100u) < 22u) return P_ROCK0;
        return P_NONE;
    }
    return P_NONE;
}
