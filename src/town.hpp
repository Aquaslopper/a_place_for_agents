// town.hpp — runtime bounded Lufia II-style town mode (MODE_TOWN).
#pragma once

#include "overworld_gen.hpp"
#include "town_gen.hpp"

void townEnter(const apfa::OwSiteInfo& site, int enterDir = 2);
const apfa::TownDef* townCurrent();
