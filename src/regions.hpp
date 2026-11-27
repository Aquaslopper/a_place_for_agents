// regions.hpp — physical country boundaries and their shared mountain passes.
#pragma once

#include "game.hpp"

// The range geometry is a pure function of the seed and coordinates.  Ground,
// collision, roads and maps all use this same answer; opening a pass changes
// only the small central gate, never the geometry of the country.
enum RegionTerrain { REGION_NONE = 0, REGION_RANGE, REGION_PASS };
struct RegionCell {
    RegionTerrain terrain = REGION_NONE;
    int ring = 0;             // 1: Elmswick / Crownlands, 2: Crownlands / frontier
    bool gate = false;        // the narrow seal at the middle of a pass
};

RegionCell regionCellAt(int tx, int ty);
bool regionNearRange(int tx, int ty, float margin);
int regionAt(float x, float y);                 // 0 home, 1 Crownlands, 2 frontier
const char* regionNameAt(float x, float y);
bool regionPassOpen(int ring);
// Directions are N, E, S, W.  North is tied to the historic North Road; the
// other openings move with the seed.
void regionPassCenter(int ring, int direction, int& tx, int& ty);
bool regionLockedPassNear(float x, float y, int& ring);

// A guaranteed, seed-shaped frontier destination, beyond the Crown pass.
void frontierCavernSite(int& tx, int& ty);
bool frontierRoadAt(int tx, int ty);

// The miniature travelling map is another view of the same party and terrain.
constexpr float TRAVEL_PIXELS_PER_TILE = 2.0f;
void drawTravelMap();
