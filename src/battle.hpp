// battle.hpp — ATB battle for "A Place for Agents"
#pragma once

#include "game.hpp"
#include <vector>

void battleStart(const std::vector<EnemyKind>& kinds, int zoneLevel,
                 uint64_t fieldMonsterId = 0, bool isBoss = false);
void battleUpdate(float dt);
void battleDraw();       // draw inside the virtual RT

bool battleIsActive();

MusicCue battleMusicCue();
