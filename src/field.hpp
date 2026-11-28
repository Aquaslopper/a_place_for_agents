// field.hpp — the field world: terrain sampling, villages, props, NPCs,
// landmarks and pathfinding. This is the layer the in-game renderer, the
// collider, the dungeon siting code and the dialogue all read from.
//
// The plate world in world.hpp is a separate, discrete overworld model; the
// types it owns (VillageDeco, LandmarkPart, propAt, ...) are declared there and
// shared, so nothing is duplicated here.
#pragma once
#include "game.hpp"
#include "world.hpp"

// ---------------------------------------------------------------- noise
uint32_t hash2i(int x, int y, uint32_t seed);
float hashf(int x, int y, uint32_t seed);
float smoothNoise(float x, float y, uint32_t seed);

// Cheap, cache-free terrain read: the noise layer below every cache, so the
// Survey can decide where a village or a cave mouth belongs without calling
// sampleGround() (which calls back into the Survey and would depend on visit
// order).
struct FieldSurvey {
    float elevation = 0.0f;
    float brookDistance = 99.0f;
    Biome biome = BI_MEADOW;
    bool water = false, deep = false, crag = false, ice = false, swamp = false;
};
FieldSurvey fieldSurveyTerrain(int tx, int ty);

// ---------------------------------------------------------------- the field
bool  cellWalkable(int tx, int ty);
bool  cellSailable(int tx, int ty);
float shoreRadius();
bool  positionBlocked(float x, float y, float radius);
bool  dockFind(bool forBoat, Dock& out);

// ---------------------------------------------------------------- the tide
int         tideIslandCount();
const char* tideIslandName(int i);

// ---------------------------------------------------------------- places
bool        inVillage(float x, float y);
bool        inBossArena(float x, float y);
bool        inOpenSea(float x, float y);
const char* zoneNameAt(float x, float y);
int         zoneLevelAt(float x, float y);
void        buildStartVillage(Village& v, uint32_t seed);
const char* npcRoleName(int role);

// ---------------------------------------------------------------- saveables
void chestsLoadFor(const std::string& key);
void npcsLoadFor(const std::string& key);
void fieldMonstersInView(std::vector<int>& out);

// ---------------------------------------------------------------- movement
bool findWalkPath(float sx, float sy, float tx, float ty,
                  std::vector<std::pair<int, int>>& out);
