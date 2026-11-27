// atlas.hpp — The Survey: one authority for where the world's places are.
//
// Before this file, four unrelated systems each rolled their own dice to decide
// whether a piece of the world existed: villages flipped a 32%-per-96-tile coin,
// landmarks a 50%-per-128-tile coin, caverns a 58%-per-64-tile coin, and each of
// them rejected candidates for its own reasons. The world that came out of that
// was made of terrain and almost nothing else: a walk from Elmswick reached the
// first other village after 243 tiles — sixteen screens of nothing — and one of
// the five landmark kinds never appeared at all, because its roll reused bits of
// the same hash that gated the sector.
//
// The Survey replaces the dice with a lattice. Every lattice cell is *owed* a
// place, so density no longer depends on luck:
//
//   villages    one per 112-tile cell   ~2.5 screens apart
//   landmarks   one per  96-tile cell   ~2   screens apart
//   caverns     one per  88-tile cell   ~1.7 screens apart
//
// Placement is a pure function of (worldSeed, cell): the same seed always puts
// the same village on the same tile, so terrain, collision, rendering, the map
// and a save made months ago all agree. A cell whose ideal spot is water walks
// a fixed ladder of fallback spots; only a cell that is genuinely all sea goes
// unclaimed, and the sea is supposed to be empty.
//
// The Survey also joins the settlements with roads. The roads are cut into the
// ground by world.cpp (G_DIRT, or plank bridges where they meet water), which
// gives a lost player the one thing this world never had: a way to walk and
// arrive somewhere.
//
// Nothing here calls sampleGround(), propAt() or cellWalkable(): those call
// *into* the Survey, and a cycle would make placement depend on visit order.
// All terrain reads go through worldSurveyTerrain(), the same noise the ground
// generator uses, one layer below the caches.
#pragma once

#include "game.hpp"
#include <string>
#include <vector>

// --- the three lattices ------------------------------------------------------
constexpr int ATLAS_VILLAGE_CELL  = 112;   // royal road spacing
constexpr int ATLAS_LANDMARK_CELL = 96;
constexpr int ATLAS_CAVERN_CELL   = 88;

// Site kinds. Kinds below AS_CAVERN are the five ruin-types a landmark can be;
// every kind is rolled from its own hash so all five are equally likely.
enum AtlasKind {
    AS_VILLAGE = 0,
    AS_CAVERN,
    AS_RUINS,
    AS_TOWER,
    AS_CASTLE,
    AS_STONES,
    AS_CAMP,
    AS_KIND_COUNT
};

// Lattice selector for the survey API. Villages and caverns are one kind each,
// so their kind doubles as their lattice; landmarks have five kinds and need
// a selector of their own.
constexpr int AS_LANDMARK = 100;

struct AtlasSite {
    int kind = AS_VILLAGE;      // AtlasKind
    int tx = 0, ty = 0;         // anchor tile (a village's square, a cave's mouth)
    int cellX = 0, cellY = 0;   // lattice cell that owns it
    uint32_t seed = 0;          // stable per-site seed (names, layouts, dungeon ids)
};

// --- surveys (pure functions of worldSeed + cell) ----------------------------
bool atlasVillageInCell(int cx, int cy, AtlasSite& out);
bool atlasLandmarkInCell(int cx, int cy, AtlasSite& out);
bool atlasCavernInCell(int cx, int cy, AtlasSite& out);

// The nearest site of each lattice within `radius` tiles of (tx,ty).
bool atlasVillageNear(int tx, int ty, int radius, AtlasSite& out);
bool atlasCavernNear(int tx, int ty, int radius, AtlasSite& out);
bool atlasLandmarkNear(int tx, int ty, int radius, AtlasSite& out);

// Everything within a rectangle, nearest-first only by kind grouping. Readers
// that draw the world (landmarksInView) get every kind through this.
void atlasSitesInRect(int x0, int y0, int x1, int y1, std::vector<AtlasSite>& out);
// Nearest site of any kind (used by the survey page and by hints).
bool atlasNearestSite(int tx, int ty, int maxRadius, AtlasSite& out);

// --- roads -------------------------------------------------------------------
// True when the royal roads run over this tile. Cheap: one cached chunk lookup.
bool atlasRoadAt(int tx, int ty);
// Which way the road runs here: bit0 = north/south, bit1 = east/west, 0 = no road.
// The ground generator uses it to lay planks the right way over water.
int atlasRoadAxis(int tx, int ty);
// Road polylines crossing the given box, for drawing the survey page.
struct AtlasRoad { std::vector<std::pair<int, int>> pts; };
void atlasRoadsInRect(int x0, int y0, int x1, int y1, std::vector<AtlasRoad>& out);
// The spur roads that leave Elmswick's four gates, so the start village is part
// of the network rather than an island in it.
void atlasOriginRoads(std::vector<AtlasRoad>& out);

// --- names -------------------------------------------------------------------
const char* atlasRegionName(int tx, int ty);          // e.g. "Millbrook Vale"
const char* atlasSiteKindName(int kind);              // e.g. "Ruins"
std::string atlasSiteName(const AtlasSite& s);        // settlement names come from world.cpp

// --- field hook --------------------------------------------------------------
// Called once per overworld frame: keeps the region banner in step with the
// ground the party is standing on. Sets g.regionName / g.regionBannerT.
void atlasUpdate(float px, float py, float dt);
// Reset the banner when a new adventure or a load moves the party somewhere else.
void atlasResetRegion();

// --- the survey page (drawn inside the menu frame) ---------------------------
void atlasJournalDraw(float ox, float oy);
