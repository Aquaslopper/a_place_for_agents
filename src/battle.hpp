// battle.hpp — ATB battle for "A Place for Agents"
#pragma once

#include "game.hpp"
#include <vector>

// The victory, not the enemy's individual death, awards a persistent key.
// Other procedural cavern guardians use GUARDIAN_NONE and their stable ID.
enum GuardianReward { GUARDIAN_NONE = 0, GUARDIAN_WAYSTONE, GUARDIAN_FRONTIER };
void battleStart(const std::vector<EnemyKind>& kinds, int zoneLevel,
                 uint64_t fieldMonsterId = 0, bool isBoss = false,
                 GuardianReward guardianReward = GUARDIAN_NONE);
void battleUpdate(float dt);
void battleDraw();       // draw inside the virtual RT

bool battleIsActive();

MusicCue battleMusicCue();
