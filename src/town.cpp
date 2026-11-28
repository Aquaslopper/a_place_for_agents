// town.cpp — real bounded, walled Lufia II-style procedural towns (MODE_TOWN).
//
// Stepping onto any town/hamlet/city icon on the overworld enters MODE_TOWN,
// generating the town deterministically from (g.worldSeed, owTx, owTy).
// Walking through any building doorway enters MODE_ROOM (and returns to the
// exact town doorstep). Walking out any open town gate returns to MODE_OVERWORLD.
#include "town.hpp"
#include "game.hpp"
#include "overworld_gen.hpp"
#include "town_gen.hpp"
#include "commons.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace {

struct LiveTownNpc {
    apfa::TownNpcSpec spec;
    float x = 0.0f, y = 0.0f;       // tile units (tile center)
    float homeX = 0.0f, homeY = 0.0f;
    float vx = 0.0f, vy = 0.0f;
    float thinkT = 1.0f;
    float animT = 0.0f;
    int facing = 0;                 // 0=down, 1=up, 2=side
    bool sideFlip = false;
    bool moving = false;
    int npcSprite = NS_VILLAGER0;
    int faceId = FACE_VILLAGER0;
};

struct TownState {
    bool active = false;
    apfa::TownDef def;
    std::vector<LiveTownNpc> npcs;
    float gateDebounce = 0.0f;
    float doorDebounce = 0.0f;
    std::string promptBuf;
};

static TownState sTown;

static void texFootT(Texture2D t, float cx, float footY, Color tint = WHITE, float scale = 1.0f) {
    if (t.id == 0) return;
    DrawTexturePro(t, { 0, 0, (float)t.width, (float)t.height },
                   { floorf(cx - t.width * scale * 0.5f), floorf(footY - t.height * scale),
                     (float)t.width * scale, (float)t.height * scale },
                   { 0, 0 }, 0.0f, tint);
}

static void drawCharT(Texture2D& sheet, int row, int frame, float cx, float footY, bool flip) {
    if (sheet.id == 0) return;
    int cols = sheet.width / 24;
    int rows = sheet.height / 32;
    if (cols < 3 || rows < 1) return;
    row = std::max(0, std::min(rows - 1, row));
    frame = std::max(0, std::min(2, frame));
    Rectangle src{ (float)(frame * 24), (float)(row * 32), 24, 32 };
    if (flip) src.width = -24;
    Rectangle dst{ floorf(cx - 12.0f), floorf(footY - 32.0f), 24, 32 };
    DrawTexturePro(sheet, src, dst, { 0, 0 }, 0.0f, WHITE);
}

static void mapNpcVisuals(const apfa::TownNpcSpec& sp, int& outSprite, int& outFace) {
    switch (sp.archetype) {
        case apfa::TNPC_GUARD:    outSprite = NS_GUARD;    outFace = FACE_GUARD; break;
        case apfa::TNPC_ELDER:    outSprite = NS_ELDER;    outFace = FACE_ELDER; break;
        case apfa::TNPC_FARMER:   outSprite = NS_FARMER;   outFace = FACE_VILLAGER1; break;
        case apfa::TNPC_CHILD:    outSprite = NS_CHILD;    outFace = FACE_CHILD; break;
        case apfa::TNPC_MERCHANT: outSprite = NS_MERCHANT; outFace = FACE_SHOPKEEPER; break;
        case apfa::TNPC_KEEPER:   outSprite = NS_KEEPER;   outFace = FACE_KEEPER; break;
        case apfa::TNPC_FERRYMAN: outSprite = NS_FERRYMAN; outFace = FACE_RANGER; break;
        case apfa::TNPC_ANIMAL:
            outSprite = (sp.spriteVariant % 3 == 0) ? NS_DOG
                      : (sp.spriteVariant % 3 == 1 ? NS_CAT : NS_CHICKEN);
            outFace = FACE_DOG;
            break;
        default: {
            int v = sp.spriteVariant & 3;
            outSprite = NS_VILLAGER0 + v;
            outFace = FACE_VILLAGER0 + v;
            break;
        }
    }
}

// Mirror TownDef's buildings into g.village so room.cpp's enterBuilding() works
// transparently for EVERY building in EVERY generated town across the world.
static void syncLegacyVillageFromTown(const apfa::TownDef& t) {
    g.village = Village{};
    g.village.valid = true;
    g.village.seed = t.townSeed;
    g.village.name = t.name;
    g.village.cx = t.w / 2;
    g.village.cy = t.h / 2;
    g.village.startVillage = t.isStartTown;
    g.village.buildings.clear();
    for (const auto& tb : t.buildings) {
        Building b;
        b.kind = static_cast<int>(tb.kind);
        b.tx = tb.tx;
        b.ty = tb.ty;
        b.w = tb.w;
        b.h = tb.h;
        b.sprite = static_cast<int>(tb.kind);
        g.village.buildings.push_back(b);
    }
    g.villageLoaded = true;
}

static void populateTownNpcs(const apfa::TownDef& t) {
    sTown.npcs.clear();
    for (const auto& sp : t.npcs) {
        LiveTownNpc ln;
        ln.spec = sp;
        ln.x = sp.tx + 0.5f;
        ln.y = sp.ty + 0.5f;
        ln.homeX = ln.x;
        ln.homeY = ln.y;
        ln.thinkT = 1.2f + (float)(apfa::owHash2(sp.tx, sp.ty, t.townSeed) % 30u) * 0.12f;
        mapNpcVisuals(sp, ln.npcSprite, ln.faceId);
        sTown.npcs.push_back(ln);
    }
}

static void snapTownCamera() {
    if (!sTown.active) return;
    float mapW = (float)(sTown.def.w * TILE);
    float mapH = (float)(sTown.def.h * TILE);
    float targetX = g.px * TILE - (float)g.vw * 0.5f;
    float targetY = g.py * TILE - (float)g.vh * 0.5f;
    if (mapW <= (float)g.vw) g.camX = floorf((mapW - (float)g.vw) * 0.5f);
    else g.camX = std::max(0.0f, std::min(mapW - (float)g.vw, targetX));
    if (mapH <= (float)g.vh) g.camY = floorf((mapH - (float)g.vh) * 0.5f);
    else g.camY = std::max(0.0f, std::min(mapH - (float)g.vh, targetY));
}

static bool townPosWalkable(float x, float y, float r = 0.28f) {
    if (!sTown.active) return false;
    float pts[4][2] = {
        { x - r, y - r * 0.5f },
        { x + r, y - r * 0.5f },
        { x - r, y + r * 0.6f },
        { x + r, y + r * 0.6f }
    };
    for (int i = 0; i < 4; ++i) {
        int tx = (int)floorf(pts[i][0]);
        int ty = (int)floorf(pts[i][1]);
        if (!apfa::townCellWalkable(sTown.def, tx, ty)) return false;
    }
    for (const auto& n : sTown.npcs) {
        float dx = x - n.x, dy = y - n.y;
        if (dx * dx + dy * dy < 0.42f * 0.42f) return false;
    }
    return true;
}

static std::string compassDir(int dx, int dy) {
    if (dx == 0 && dy == 0) return "here";
    std::string s;
    if (dy < -2) s += "north";
    else if (dy > 2) s += "south";
    if (dx < -2) s += (s.empty() ? "west" : "-west");
    else if (dx > 2) s += (s.empty() ? "east" : "-east");
    if (s.empty()) s = (std::abs(dx) >= std::abs(dy)) ? (dx > 0 ? "east" : "west") : (dy > 0 ? "south" : "north");
    return s;
}

static void talkToTownNpc(LiveTownNpc& n) {
    // Turn NPC to face the hero
    float dx = g.px - n.x, dy = g.py - n.y;
    if (fabsf(dx) > fabsf(dy)) {
        n.facing = 2;
        n.sideFlip = (dx < 0);
    } else {
        n.facing = (dy < 0) ? 1 : 0;
    }
    n.moving = false;
    n.vx = n.vy = 0;

    std::vector<DialogLine> lines;
    DialogAction act = DLG_NONE;
    const auto& t = sTown.def;

    if (n.spec.archetype == apfa::TNPC_ANIMAL) {
        const char* sound = (n.npcSprite == NS_DOG) ? "Woof! *wags tail happily*"
                          : (n.npcSprite == NS_CAT ? "Mrrrow... *blinks slowly in the sun*"
                                                   : "Cluck-cluck!");
        lines.push_back({ n.faceId, n.spec.name, sound });
        g.dialogReturn = MODE_TOWN;
        startDialog(lines, DLG_NONE);
        return;
    }

    // ---- Elmswick story NPCs -------------------------------------------------
    if (t.isStartTown) {
        if (n.spec.archetype == apfa::TNPC_ELDER) {
            if (g.quests[QUEST_CROWN].state == QS_DONE) {
                lines.push_back({ FACE_ELDER, "Elder Rowan",
                    "The whole vale breathes easier with the Slime King routed. Safe travels across the wider world!" });
            } else if (g.bossDefeated && itemCount(IT_CROWN) > 0) {
                lines.push_back({ FACE_ELDER, "Elder Rowan",
                    "You brought back the Slime Crown from Castle of Elm! Elmswick owes your party a deep debt." });
                act = DLG_CROWN_REWARD;
            } else if (g.quests[QUEST_CROWN].state == QS_UNKNOWN) {
                lines.push_back({ FACE_ELDER, "Elder Rowan",
                    "Welcome, travellers. Step out any town gate to reach the World Map. To the north-east lies Castle of Elm, where a Slime King has claimed the throne." });
                lines.push_back({ FACE_ELDER, "Elder Rowan",
                    "Gear up at Mira's Sundries and Bram's Forge first, and test your steel inside Hollow Hill cavern to the north-west!" });
                act = DLG_START_QUEST_CROWN;
            } else {
                auto castle = apfa::owBossCastle(g.worldSeed);
                auto cave = apfa::owStartCavern(g.worldSeed);
                char buf[220];
                std::snprintf(buf, sizeof(buf),
                    "Castle of Elm waits to the %s on the World Map. Hollow Hill cavern sits %s if you need training first.",
                    compassDir(castle.tx, castle.ty).c_str(), compassDir(cave.tx, cave.ty).c_str());
                lines.push_back({ FACE_ELDER, "Elder Rowan", buf });
            }
            g.dialogReturn = MODE_TOWN;
            startDialog(lines, act);
            return;
        }
        if (n.spec.archetype == apfa::TNPC_FARMER) {
            auto& q = g.quests[QUEST_SLIMES];
            if (q.state == QS_DONE) {
                lines.push_back({ FACE_VILLAGER1, "Farmer Hale",
                    "My turnips are finally growing in peace! Plenty of towns east and south on the World Map if you're travelling." });
            } else if (q.state == QS_ACTIVE && (q.progress >= 6 || g.slimesSlain >= 6)) {
                lines.push_back({ FACE_VILLAGER1, "Farmer Hale",
                    "You thinned out those slimes! Here's your reward — fresh from the harvest." });
                act = DLG_FINISH_QUEST_SLIMES;
            } else if (q.state == QS_UNKNOWN) {
                lines.push_back({ FACE_VILLAGER1, "Farmer Hale",
                    "Slimes out in the meadow past our gates have been chewing up my turnip crates! Defeat 6 slimes on the World Map and I'll pay you well." });
                act = DLG_START_QUEST_SLIMES;
            } else {
                char buf[160];
                std::snprintf(buf, sizeof(buf),
                    "Slimes cleared: %d / 6. Step outside the town gates and roam the grass to flush them out.",
                    std::min(6, std::max(q.progress, g.slimesSlain)));
                lines.push_back({ FACE_VILLAGER1, "Farmer Hale", buf });
            }
            g.dialogReturn = MODE_TOWN;
            startDialog(lines, act);
            return;
        }
        if (n.spec.archetype == apfa::TNPC_VILLAGER && n.spec.questId == QUEST_LOCKET) {
            auto& q = g.quests[QUEST_LOCKET];
            if (q.state == QS_DONE) {
                lines.push_back({ FACE_WOMAN, "Maren",
                    "Thank you again for my locket! I'll treasure it always." });
            } else if (g.locketFound || itemCount(IT_LOCKET) > 0) {
                lines.push_back({ FACE_WOMAN, "Maren",
                    "Oh! My silver locket! You braved the Standing Stones for me — please take this!" });
                act = DLG_FINISH_QUEST_LOCKET;
            } else if (q.state == QS_UNKNOWN) {
                lines.push_back({ FACE_WOMAN, "Maren",
                    "I dropped my mother's silver locket at the Standing Stones south-west of Elmswick on the World Map. Could you look for it?" });
                act = DLG_START_QUEST_LOCKET;
            } else {
                lines.push_back({ FACE_WOMAN, "Maren",
                    "The Standing Stones are south-west of Elmswick on the World Map — follow the golden road out our west or south gate." });
            }
            g.dialogReturn = MODE_TOWN;
            startDialog(lines, act);
            return;
        }
        if (n.spec.archetype == apfa::TNPC_KEEPER) {
            auto& q = g.quests[QUEST_COMMONS];
            if (q.state == QS_DONE) {
                lines.push_back({ FACE_KEEPER, "Keeper Oren",
                    "The Agents' Commons brazier burns bright in Elmswick. Every town out on the World Map keeps its own hearth." });
            } else if (q.state == QS_ACTIVE && q.progress >= q.goal) {
                lines.push_back({ FACE_KEEPER, "Keeper Oren",
                    "You've walked the road and returned with embers for the Commons! Let us kindle the brazier." });
                act = DLG_FINISH_QUEST_COMMONS;
            } else if (q.state == QS_UNKNOWN) {
                lines.push_back({ FACE_KEEPER, "Keeper Oren",
                    "This grove is the Agents' Commons. Explore the World Map or Hollow Hill, gather a spark of adventure, and return to kindle our ring." });
                act = DLG_START_QUEST_COMMONS;
            } else {
                lines.push_back({ FACE_KEEPER, "Keeper Oren",
                    "Inspect the three standing stones behind me, or venture into Hollow Hill north-west of town." });
            }
            g.dialogReturn = MODE_TOWN;
            startDialog(lines, act);
            return;
        }
        if (n.spec.archetype == apfa::TNPC_FERRYMAN) {
            if (g.hasBoat) {
                char buf[220];
                std::snprintf(buf, sizeof(buf),
                    "She's all yours! On the World Map, simply walk straight onto any sea coastline to board your ship. Tidewells lit: %d / 3.",
                    g.tideFound);
                lines.push_back({ FACE_RANGER, "Ferryman Bel", buf });
            } else if (g.gil >= 40) {
                lines.push_back({ FACE_RANGER, "Ferryman Bel",
                    "Looking to sail the blue sea south of Elmswick? For 40 Gil I'll deed you my sloop — step right off any coast on the World Map to set sail!" });
                act = DLG_BUY_BOAT;
            } else {
                lines.push_back({ FACE_RANGER, "Ferryman Bel",
                    "Bring me 40 Gil and I'll deed you a seaworthy boat so you can sail the southern ocean and reach the Tidewell Isles!" });
            }
            g.dialogReturn = MODE_TOWN;
            startDialog(lines, act);
            return;
        }
    }

    // ---- Procedural World Town NPCs (dynamic geography-aware dialogue!) ------
    if (n.spec.archetype == apfa::TNPC_ELDER) {
        apfa::OwSiteInfo nearDun, nearTown;
        bool hasD = apfa::owNearestSite(g.worldSeed, t.owTx, t.owTy, 26, nearDun, false, true);
        bool hasT = false;
        std::vector<apfa::OwSiteInfo> nearby;
        apfa::owSitesInRect(g.worldSeed, t.owTx - 30, t.owTy - 30, t.owTx + 30, t.owTy + 30, nearby);
        int bestD2 = 999999;
        for (const auto& s : nearby) {
            if (!apfa::owIsSettlement(s.type)) continue;
            if (s.tx == t.owTx && s.ty == t.owTy) continue;
            int d2 = (s.tx - t.owTx) * (s.tx - t.owTx) + (s.ty - t.owTy) * (s.ty - t.owTy);
            if (d2 < bestD2) { bestD2 = d2; nearTown = s; hasT = true; }
        }
        char buf[260];
        if (hasD && hasT) {
            std::snprintf(buf, sizeof(buf),
                "Welcome to %s! Watch yourself near %s to the %s. The road %s leads straight to %s.",
                t.name.c_str(), nearDun.name.c_str(),
                compassDir(nearDun.tx - t.owTx, nearDun.ty - t.owTy).c_str(),
                compassDir(nearTown.tx - t.owTx, nearTown.ty - t.owTy).c_str(),
                nearTown.name.c_str());
        } else {
            std::snprintf(buf, sizeof(buf),
                "Welcome to %s! Rest at our Inn and check the General Store before venturing further out.",
                t.name.c_str());
        }
        lines.push_back({ n.faceId, n.spec.name, buf });
    } else if (n.spec.archetype == apfa::TNPC_GUARD) {
        char buf[200];
        std::snprintf(buf, sizeof(buf),
            "All gates of %s stand open to honest travellers. Danger tier outside our walls: %d.",
            t.name.c_str(), t.dangerTier);
        lines.push_back({ n.faceId, n.spec.name, buf });
    } else if (n.spec.archetype == apfa::TNPC_MERCHANT) {
        const char* climNote = (t.climate == apfa::OWC_SAND)
            ? "Sun-dried spices and desert steel fetch a fine price out here!"
            : (t.climate == apfa::OWC_SNOW
                ? "Warm wool and winter tonics sell fast in this northern frost!"
                : "Fresh goods off the royal road — check inside the General Store and Smithy!");
        lines.push_back({ n.faceId, n.spec.name, climNote });
    } else if (n.spec.archetype == apfa::TNPC_CHILD) {
        lines.push_back({ n.faceId, n.spec.name,
            "I heard somebody hid treasure chests near the corners of the town walls! Don't tell the guards!" });
    } else {
        static const char* kChatter[4] = {
            "Nothing beats a quiet night inside solid stone walls after crossing the wilds.",
            "Press [M] out on the World Map to inspect your parchment chart of the surrounding lands!",
            "Every town's smithy stocks stronger steel the further you travel from Elmswick.",
            "Take a sip from the town square well if your feet are tired — the water's crisp!"
        };
        int idx = (int)(apfa::owHash2(n.spec.tx, n.spec.ty, t.townSeed) & 3u);
        lines.push_back({ n.faceId, n.spec.name, kChatter[idx] });
    }
    g.dialogReturn = MODE_TOWN;
    startDialog(lines, act);
}

static bool tryTownInteract() {
    if (!sTown.active) return false;
    const auto& t = sTown.def;

    // Look slightly ahead in the facing direction AND at player feet
    float fx = g.px + (g.facing == 2 ? (g.sideFlip ? -0.75f : 0.75f) : 0.0f);
    float fy = g.py + (g.facing == 0 ? 0.75f : (g.facing == 1 ? -0.75f : 0.0f));
    int ptx = (int)floorf(g.px), pty = (int)floorf(g.py);
    int ftx = (int)floorf(fx),   fty = (int)floorf(fy);

    // 1. Building doorway
    int bIdx = apfa::townDoorAt(t, ptx, pty);
    if (bIdx < 0) bIdx = apfa::townDoorAt(t, ftx, fty);
    if (bIdx >= 0 && bIdx < (int)t.buildings.size()) {
        g.interactT = 0.22f;
        playSoundIf(g.assets.door);
        enterBuilding(bIdx);
        return true;
    }

    // 2. Town Chest
    for (const auto& c : t.chests) {
        if (g.openedChests.count(c.id)) continue;
        float dx = g.px - (c.tx + 0.5f), dy = g.py - (c.ty + 0.5f);
        if (dx * dx + dy * dy < 1.55f * 1.55f) {
            g.openedChests.insert(c.id);
            playSoundIf(g.assets.treasure);
            std::string msg;
            if (c.item >= 0 && c.item < IT_COUNT) {
                addItem(c.item, c.amount, true);
                msg = std::string("Found ") + itemDef(c.item).name + "!";
            } else {
                g.gil += c.gil;
                msg = "Found " + formatGil(c.gil) + "!";
            }
            pushToast(msg);
            g.dialogReturn = MODE_TOWN;
            startDialog({ { -1, "Treasure Chest", msg } }, DLG_NONE);
            return true;
        }
    }

    // 3. Nearest NPC
    int bestNpc = -1;
    float bestD2 = 1.65f * 1.65f;
    for (int i = 0; i < (int)sTown.npcs.size(); ++i) {
        float dx = g.px - sTown.npcs[i].x, dy = g.py - sTown.npcs[i].y;
        float d2 = dx * dx + dy * dy;
        if (d2 < bestD2) { bestD2 = d2; bestNpc = i; }
    }
    if (bestNpc >= 0) {
        g.interactT = 0.20f;
        talkToTownNpc(sTown.npcs[bestNpc]);
        return true;
    }

    // 4. Interactive Props (Well, Noticeboard, Commons Stones/Brazier)
    for (const auto& p : t.props) {
        float dx = g.px - (p.tx + 0.5f), dy = g.py - (p.ty + 0.5f);
        if (dx * dx + dy * dy > 1.75f * 1.75f) continue;
        if (p.kind == apfa::TP_WELL) {
            playSoundIf(g.assets.heal);
            for (int i = 0; i < 3; ++i) {
                if (!g.party[i].ko) {
                    g.party[i].hp = std::min(g.party[i].maxHp, g.party[i].hp + g.party[i].maxHp / 4 + 5);
                    g.party[i].mp = std::min(g.party[i].maxMp, g.party[i].mp + g.party[i].maxMp / 4 + 2);
                }
            }
            g.dialogReturn = MODE_TOWN;
            startDialog({ { -1, "Town Well", "You drink cool, clear spring water from the stone well. Party refreshed!" } }, DLG_NONE);
            return true;
        }
        if (p.kind == apfa::TP_NOTICEBOARD) {
            apfa::OwSiteInfo nearDun;
            std::string body = "Noticeboard of " + t.name + ": ";
            if (apfa::owNearestSite(g.worldSeed, t.owTx, t.owTy, 26, nearDun, false, true)) {
                body += "Travellers beware " + nearDun.name + " to the " +
                        compassDir(nearDun.tx - t.owTx, nearDun.ty - t.owTy) + ". ";
            }
            body += "Walk through any open gate to return to the World Map. Press [ESC] for Party/Save, [F5] to Quicksave.";
            g.dialogReturn = MODE_TOWN;
            startDialog({ { -1, "Town Noticeboard", body } }, DLG_NONE);
            return true;
        }
        if (p.kind == apfa::TP_COMMONS_STONE || p.kind == apfa::TP_COMMONS_BRAZIER) {
            g.dialogReturn = MODE_TOWN;
            startDialog({ { FACE_KEEPER, "Agents' Commons",
                "Carved stones mark the quiet gathering circle of Elmswick. Their runes hum softly in the breeze." } }, DLG_NONE);
            return true;
        }
    }

    return false;
}

} // namespace

// ================================================================ Public API
const apfa::TownDef* townCurrent() {
    return sTown.active ? &sTown.def : nullptr;
}

bool townActive() { return sTown.active; }

const char* townCurrentName() {
    return sTown.active ? sTown.def.name.c_str() : "Elmswick";
}

int townDangerTier() {
    return sTown.active ? sTown.def.dangerTier : 1;
}

void townRebuildRuntime(int owTx, int owTy) {
    apfa::TownDef def = apfa::townGenerateFromCoords(g.worldSeed, owTx, owTy);
    sTown.def = std::move(def);
    sTown.active = true;
    sTown.gateDebounce = 0.6f;
    sTown.doorDebounce = 0.5f;
    g.currentTownOwTx = owTx;
    g.currentTownOwTy = owTy;
    syncLegacyVillageFromTown(sTown.def);
    populateTownNpcs(sTown.def);
    snapTownCamera();
}

void townEnter(const apfa::OwSiteInfo& site, int enterDir) {
    // Remember exact overworld icon coordinate so exiting returns cleanly!
    g.owReturnX = (float)site.tx + 0.5f;
    g.owReturnY = (float)site.ty + 0.5f;
    g.currentTownOwTx = site.tx;
    g.currentTownOwTy = site.ty;
    g.onShip = false;
    g.travelMap = false;

    sTown.def = apfa::townGenerate(g.worldSeed, site);
    sTown.active = true;
    sTown.gateDebounce = 0.65f;
    sTown.doorDebounce = 0.55f;

    syncLegacyVillageFromTown(sTown.def);
    populateTownNpcs(sTown.def);

    // Spawn just inside the matching gate (default South gate = dir 2)
    int sx = sTown.def.spawnTx, sy = sTown.def.spawnTy;
    int face = 1; // face north into town when entering via south gate
    for (const auto& gt : sTown.def.gates) {
        if (gt.dir == enterDir) {
            sx = gt.spawnTx;
            sy = gt.spawnTy;
            if (enterDir == 0) face = 0;      // entered N gate -> face south
            else if (enterDir == 2) face = 1; // entered S gate -> face north
            else { face = 2; g.sideFlip = (enterDir == 1); }
            break;
        }
    }

    g.px = (float)sx + 0.5f;
    g.py = (float)sy + 0.5f;
    g.facing = face;
    g.moving = false;
    g.walkQueue.clear();
    g.hasMoveTarget = false;
    g.mode = MODE_TOWN;
    g.regionName = sTown.def.name;
    g.regionBannerT = 2.6f;
    g.fade = 0.9f;
    g.fadeDir = -1;
    snapTownCamera();
    playSoundIf(g.assets.door);
}

void townEnterAtCoords(int owTx, int owTy, int enterDir) {
    apfa::OwSiteInfo site;
    if (!apfa::owSiteAtTile(g.worldSeed, owTx, owTy, site) || !apfa::owIsSettlement(site.type)) {
        site.type = (owTx == 0 && owTy == 0) ? apfa::OWS_TOWN : apfa::OWS_HAMLET;
        site.tx = owTx;
        site.ty = owTy;
        site.gateMask = 0x0F;
        site.isStartTown = (owTx == 0 && owTy == 0);
        site.name = site.isStartTown ? "Elmswick"
            : apfa::owSettlementName(g.worldSeed, owTx, owTy, site.type, apfa::OWC_TEMPERATE);
    }
    townEnter(site, enterDir);
}

void townExitToOverworld(int exitDir) {
    int owTx = sTown.active ? sTown.def.owTx : g.currentTownOwTx;
    int owTy = sTown.active ? sTown.def.owTy : g.currentTownOwTy;

    // Step 1 tile outside the town icon in the gate's exit direction if walkable,
    // otherwise stand on the town icon with generous re-entry debounce.
    static const int kDx[4] = { 0, 1, 0, -1 };
    static const int kDy[4] = { -1, 0, 1, 0 };
    int d = (exitDir >= 0 && exitDir < 4) ? exitDir : 2;
    int destTx = owTx + kDx[d];
    int destTy = owTy + kDy[d];
    auto destTile = apfa::owSampleTile(g.worldSeed, destTx, destTy);
    if (!apfa::owIsWalkableOnFoot(destTile.kind)) {
        destTx = owTx;
        destTy = owTy;
    }

    g.px = (float)destTx + 0.5f;
    g.py = (float)destTy + 0.5f;
    g.owReturnX = (float)owTx + 0.5f;
    g.owReturnY = (float)owTy + 0.5f;
    g.owSiteDebounce = 0.85f;
    g.owLastSiteTx = owTx;
    g.owLastSiteTy = owTy;
    g.facing = (d == 0) ? 1 : (d == 2 ? 0 : 2);
    if (d == 1) g.sideFlip = false;
    if (d == 3) g.sideFlip = true;
    g.moving = false;
    g.mode = MODE_OVERWORLD;
    g.regionName = apfa::owRegionTitle(g.worldSeed, destTx, destTy);
    g.regionBannerT = 2.4f;
    g.fade = 0.85f;
    g.fadeDir = -1;
    overworldSnapCamera();
    playSoundIf(g.assets.door);
}

const char* townPrompt() {
    if (!sTown.active) return nullptr;
    const auto& t = sTown.def;
    int ptx = (int)floorf(g.px), pty = (int)floorf(g.py);
    float fx = g.px + (g.facing == 2 ? (g.sideFlip ? -0.75f : 0.75f) : 0.0f);
    float fy = g.py + (g.facing == 0 ? 0.75f : (g.facing == 1 ? -0.75f : 0.0f));
    int ftx = (int)floorf(fx), fty = (int)floorf(fy);

    int bIdx = apfa::townDoorAt(t, ptx, pty);
    if (bIdx < 0) bIdx = apfa::townDoorAt(t, ftx, fty);
    if (bIdx >= 0 && bIdx < (int)t.buildings.size()) {
        sTown.promptBuf = "[Z / Enter] Enter " + t.buildings[bIdx].label;
        return sTown.promptBuf.c_str();
    }
    for (const auto& c : t.chests) {
        if (g.openedChests.count(c.id)) continue;
        float dx = g.px - (c.tx + 0.5f), dy = g.py - (c.ty + 0.5f);
        if (dx * dx + dy * dy < 1.55f * 1.55f) return "[Z / Enter] Open Chest";
    }
    for (const auto& n : sTown.npcs) {
        float dx = g.px - n.x, dy = g.py - n.y;
        if (dx * dx + dy * dy < 1.65f * 1.65f) {
            sTown.promptBuf = "[Z / Enter] Talk to " + n.spec.name;
            return sTown.promptBuf.c_str();
        }
    }
    for (const auto& p : t.props) {
        float dx = g.px - (p.tx + 0.5f), dy = g.py - (p.ty + 0.5f);
        if (dx * dx + dy * dy > 1.75f * 1.75f) continue;
        if (p.kind == apfa::TP_WELL) return "[Z / Enter] Drink from Well";
        if (p.kind == apfa::TP_NOTICEBOARD) return "[Z / Enter] Read Noticeboard";
        if (p.kind == apfa::TP_COMMONS_STONE || p.kind == apfa::TP_COMMONS_BRAZIER)
            return "[Z / Enter] Inspect Commons";
    }
    return nullptr;
}

void townUpdate(float dt) {
    if (!sTown.active) {
        townRebuildRuntime(g.currentTownOwTx, g.currentTownOwTy);
    }

    g.totalTime += dt;
    g.runTime += dt;
    if (sTown.gateDebounce > 0.0f) sTown.gateDebounce -= dt;
    if (sTown.doorDebounce > 0.0f) sTown.doorDebounce -= dt;
    if (g.interactT > 0.0f) g.interactT -= dt;

    // Pause menu & Quicksave
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_X)) {
        playSoundIf(g.assets.confirm);
        g.menuReturn = MODE_TOWN;
        g.menuPage = 0;
        g.menuIndex = 0;
        g.menuItem = -1;
        g.mode = MODE_MENU;
        return;
    }
    if (IsKeyPressed(KEY_F5)) {
        if (saveGame()) { playSoundIf(g.assets.chime); pushToast("Saved in " + sTown.def.name + "."); }
        else pushToast("Save failed.");
    }

    // Interact button
    if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        if (tryTownInteract()) return;
    }

    // Player movement
    float dx = 0.0f, dy = 0.0f;
    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) dx += 1.0f;
    if (IsKeyDown(KEY_LEFT)  || IsKeyDown(KEY_A)) dx -= 1.0f;
    if (IsKeyDown(KEY_DOWN)  || IsKeyDown(KEY_S)) dy += 1.0f;
    if (IsKeyDown(KEY_UP)    || IsKeyDown(KEY_W)) dy -= 1.0f;

    bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    float speed = shift ? 6.4f : 4.6f;

    if (dx != 0.0f || dy != 0.0f) {
        if (dx != 0.0f && dy != 0.0f) { dx *= 0.7071f; dy *= 0.7071f; }
        if (fabsf(dx) >= fabsf(dy)) {
            g.facing = 2;
            g.sideFlip = (dx < 0.0f);
        } else {
            g.facing = (dy > 0.0f) ? 0 : 1;
        }

        float nx = g.px + dx * speed * dt;
        float ny = g.py + dy * speed * dt;
        bool moved = false;
        if (townPosWalkable(nx, ny)) {
            g.px = nx; g.py = ny; moved = true;
        } else {
            if (dx != 0.0f && townPosWalkable(nx, g.py)) { g.px = nx; moved = true; }
            if (dy != 0.0f && townPosWalkable(g.px, ny)) { g.py = ny; moved = true; }
        }
        g.moving = moved;
        if (moved) g.walkAnimT += dt;

        // Stepping UP into a building door tile enters that interior smoothly!
        if (sTown.doorDebounce <= 0.0f && dy < -0.25f) {
            int ptx = (int)floorf(g.px);
            int pty = (int)floorf(g.py);
            int bIdx = apfa::townDoorAt(sTown.def, ptx, pty);
            if (bIdx >= 0) {
                const auto& b = sTown.def.buildings[bIdx];
                // Trigger when pushing against the top edge of the doorstep tile
                if (g.py < (float)b.doorTy + 0.38f && fabsf(g.px - ((float)b.doorTx + 0.5f)) < 0.55f) {
                    sTown.doorDebounce = 0.55f;
                    playSoundIf(g.assets.door);
                    enterBuilding(bIdx);
                    return;
                }
            }
        }
    } else {
        g.moving = false;
    }

    // Check stepping onto any open Town Gate -> exit cleanly to World Map!
    if (sTown.gateDebounce <= 0.0f) {
        int ptx = (int)floorf(g.px);
        int pty = (int)floorf(g.py);
        int gIdx = apfa::townGateAt(sTown.def, ptx, pty);
        if (gIdx >= 0) {
            townExitToOverworld(sTown.def.gates[gIdx].dir);
            return;
        }
    }

    // Gentle living stroll AI for townsfolk (skip Guards & Keeper who hold their posts)
    for (auto& n : sTown.npcs) {
        if (n.spec.archetype == apfa::TNPC_GUARD || n.spec.archetype == apfa::TNPC_KEEPER ||
            n.spec.archetype == apfa::TNPC_FERRYMAN) continue;
        n.thinkT -= dt;
        if (n.thinkT <= 0.0f) {
            uint32_t h = apfa::owHash2((int)(g.totalTime * 10.0f) + n.spec.tx, n.spec.ty, sTown.def.townSeed);
            if (n.moving) {
                n.moving = false;
                n.vx = n.vy = 0.0f;
                n.thinkT = 1.6f + (float)(h % 25u) * 0.1f;
            } else {
                int dir = (int)(h & 3u);
                static const float kVx[4] = { 0, 0, -1, 1 };
                static const float kVy[4] = { 1, -1, 0, 0 };
                n.vx = kVx[dir] * 1.15f;
                n.vy = kVy[dir] * 1.15f;
                n.facing = (dir == 0) ? 0 : (dir == 1 ? 1 : 2);
                n.sideFlip = (dir == 2);
                n.moving = true;
                n.thinkT = 0.7f + (float)((h >> 4) % 10u) * 0.1f;
            }
        }
        if (n.moving) {
            float nx = n.x + n.vx * dt;
            float ny = n.y + n.vy * dt;
            float distHome = (nx - n.homeX) * (nx - n.homeX) + (ny - n.homeY) * (ny - n.homeY);
            float distHero = (nx - g.px) * (nx - g.px) + (ny - g.py) * (ny - g.py);
            int tx = (int)floorf(nx), ty = (int)floorf(ny);
            if (distHome < 4.5f && distHero > 0.75f &&
                apfa::townCellWalkable(sTown.def, tx, ty) &&
                apfa::townDoorAt(sTown.def, tx, ty) < 0 &&
                apfa::townGateAt(sTown.def, tx, ty) < 0) {
                n.x = nx; n.y = ny;
                n.animT += dt;
            } else {
                n.moving = false;
                n.vx = n.vy = 0.0f;
                n.thinkT = 1.4f;
            }
        }
    }

    // Smooth clamped camera follow
    float mapW = (float)(sTown.def.w * TILE);
    float mapH = (float)(sTown.def.h * TILE);
    float desiredX = (mapW <= (float)g.vw) ? floorf((mapW - (float)g.vw) * 0.5f)
                   : std::max(0.0f, std::min(mapW - (float)g.vw, g.px * TILE - (float)g.vw * 0.5f));
    float desiredY = (mapH <= (float)g.vh) ? floorf((mapH - (float)g.vh) * 0.5f)
                   : std::max(0.0f, std::min(mapH - (float)g.vh, g.py * TILE - (float)g.vh * 0.5f));
    float k = std::min(1.0f, dt * 11.0f);
    g.camX += (desiredX - g.camX) * k;
    g.camY += (desiredY - g.camY) * k;
}

void townDraw() {
    if (!sTown.active) return;
    const auto& t = sTown.def;

    ClearBackground(Color{ 18, 22, 28, 255 });

    int x0 = std::max(0, (int)floorf(g.camX / TILE) - 1);
    int y0 = std::max(0, (int)floorf(g.camY / TILE) - 1);
    int x1 = std::min(t.w - 1, (int)ceilf((g.camX + (float)g.vw) / TILE) + 1);
    int y1 = std::min(t.h - 1, (int)ceilf((g.camY + (float)g.vh) / TILE) + 1);

    // 1. Ground & Perimeter Wall Tiles
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            apfa::TownTileKind tk = apfa::townTileAt(t, x, y);
            uint8_t var = t.tileVar[static_cast<std::size_t>(y) * static_cast<std::size_t>(t.w) + static_cast<std::size_t>(x)];
            float sx = floorf((float)(x * TILE) - g.camX);
            float sy = floorf((float)(y * TILE) - g.camY);

            Texture2D* tex = &g.assets.grassTile[0];
            Rectangle src{ 0, 0, 16, 16 };

            switch (tk) {
                case apfa::TT_GRASS:
                    if (t.climate == apfa::OWC_SAND) tex = &g.assets.sandFloor;
                    else if (t.climate == apfa::OWC_SNOW) tex = &g.assets.snowFloor;
                    else tex = (var == 0) ? &g.assets.grassTile[3] : &g.assets.grassTile[0];
                    break;
                case apfa::TT_GRASS_DARK:
                    if (t.climate == apfa::OWC_SAND) tex = &g.assets.sandFloor;
                    else if (t.climate == apfa::OWC_SNOW) tex = &g.assets.tundraFloor;
                    else tex = &g.assets.grassDarkTile;
                    break;
                case apfa::TT_ROAD:   tex = &g.assets.dirtFloor; break;
                case apfa::TT_COBBLE: tex = &g.assets.cobbleFloor; break;
                case apfa::TT_COURT:  tex = &g.assets.flagstoneFloor; break;
                case apfa::TT_SOIL:   tex = &g.assets.soilTile; break;
                case apfa::TT_WATER:  tex = &g.assets.waterTile; break;
                case apfa::TT_BRIDGE: tex = &g.assets.bridge; break;
                case apfa::TT_WALL_TOP:
                    tex = &g.assets.townBattlementAtlas;
                    src = { (float)((x & 1) * 16), 0, 16, 16 };
                    break;
                case apfa::TT_WALL_FACE:
                    tex = &g.assets.townMasonryAtlas;
                    src = { (float)((x & 1) * 16), 0, 16, 16 };
                    break;
                case apfa::TT_GATE_FLOOR:
                    tex = &g.assets.cobbleFloor;
                    break;
            }

            if (tex && tex->id != 0) {
                DrawTexturePro(*tex, src, { sx, sy, 16, 16 }, { 0, 0 }, 0.0f, WHITE);
            } else {
                DrawRectangle((int)sx, (int)sy, 16, 16, Color{ 48, 86, 30, 255 });
            }

            // Subtle animated shimmer on canal water
            if (tk == apfa::TT_WATER) {
                float wave = sinf(g.totalTime * 2.4f + (float)x * 0.9f + (float)y * 1.3f);
                if (wave > 0.45f)
                    DrawRectangle((int)sx + 3, (int)sy + 6 + (x & 3), 6, 1, Fade(WHITE, 0.35f));
            }
            // Kitchen garden crops on soil tiles
            if (tk == apfa::TT_SOIL && g.assets.turnips.id != 0) {
                DrawTexturePro(g.assets.turnips,
                    { 0, 0, (float)g.assets.turnips.width, (float)g.assets.turnips.height },
                    { sx, sy, 16, 16 }, { 0, 0 }, 0.0f, WHITE);
            }
            // Subtle directional chevrons on open Town Gates so exits are unmistakable
            if (tk == apfa::TT_GATE_FLOOR) {
                float pulse = 0.45f + 0.35f * sinf(g.totalTime * 3.5f);
                DrawRectangle((int)sx + 4, (int)sy + 6, 8, 4, Fade(Color{ 255, 232, 150, 255 }, pulse * 0.45f));
            }
        }
    }

    // 2. Y-Sorted Buildings, Props, Chests, NPCs & Hero
    struct DrawItem {
        float sortY;
        int kind; // 0=building, 1=prop, 2=chest, 3=npc, 4=hero
        int idx;
    };
    std::vector<DrawItem> items;
    items.reserve(t.buildings.size() + t.props.size() + t.chests.size() + sTown.npcs.size() + 1);

    for (int i = 0; i < (int)t.buildings.size(); ++i)
        items.push_back({ (float)(t.buildings[i].ty + 1) * TILE, 0, i });
    for (int i = 0; i < (int)t.props.size(); ++i)
        items.push_back({ (float)(t.props[i].ty + 1) * TILE - (t.props[i].solid ? 0.0f : 4.0f), 1, i });
    for (int i = 0; i < (int)t.chests.size(); ++i)
        items.push_back({ (float)(t.chests[i].ty + 1) * TILE, 2, i });
    for (int i = 0; i < (int)sTown.npcs.size(); ++i)
        items.push_back({ (sTown.npcs[i].y + 0.35f) * TILE, 3, i });
    items.push_back({ (g.py + 0.35f) * TILE, 4, 0 });

    std::stable_sort(items.begin(), items.end(),
                     [](const DrawItem& a, const DrawItem& b) { return a.sortY < b.sortY; });

    for (const auto& it : items) {
        if (it.kind == 0) {
            const auto& b = t.buildings[it.idx];
            Texture2D* tex = nullptr;
            switch (b.kind) {
                case apfa::TB_SHOP:   tex = &g.assets.shop; break;
                case apfa::TB_INN:    tex = &g.assets.inn; break;
                case apfa::TB_TEMPLE: tex = &g.assets.temple; break;
                case apfa::TB_MANOR:  tex = &g.assets.manor; break;
                case apfa::TB_CASTLE: tex = &g.assets.castle; break;
                case apfa::TB_SMITHY: tex = &g.assets.smithy; break;
                default: tex = &g.assets.house[std::max(0, std::min(4, (int)b.kind))]; break;
            }
            if (!tex || tex->id == 0) continue;
            float cx = ((float)b.tx + 0.5f) * TILE - g.camX;
            float fy = (float)(b.ty + 1) * TILE - g.camY;
            DrawEllipse((int)cx, (int)fy - 2, (float)(b.w * TILE) * 0.44f, 4.5f, Fade(BLACK, 0.25f));
            texFootT(*tex, cx, fy);
        } else if (it.kind == 1) {
            const auto& p = t.props[it.idx];
            Texture2D* tex = nullptr;
            switch (p.kind) {
                case apfa::TP_WELL:                  tex = &g.assets.well; break;
                case apfa::TP_NOTICEBOARD:           tex = &g.assets.noticeboard; break;
                case apfa::TP_SIGN_GOODS:            tex = &g.assets.townSignGoods; break;
                case apfa::TP_SIGN_INN:              tex = &g.assets.townSignInn; break;
                case apfa::TP_SIGN_WEAPON:           tex = &g.assets.townSignWeapon; break;
                case apfa::TP_BANNER_VERT:           tex = &g.assets.townBannerVert; break;
                case apfa::TP_BANNER_SHORT:          tex = &g.assets.townBannerShort; break;
                case apfa::TP_MARKET_FLOWERS_RED:    tex = &g.assets.townMarketRed; break;
                case apfa::TP_MARKET_FLOWERS_ORANGE: tex = &g.assets.townMarketOrange; break;
                case apfa::TP_MARKET_SACKS:          tex = &g.assets.townMarketSacks; break;
                case apfa::TP_BENCH:                 tex = &g.assets.bench; break;
                case apfa::TP_TORCH:                 tex = &g.assets.torch; break;
                case apfa::TP_FLOWERBED:             tex = &g.assets.flowerbed; break;
                case apfa::TP_BARREL:                tex = &g.assets.barrel; break;
                case apfa::TP_CRATE:                 tex = &g.assets.crate; break;
                case apfa::TP_TREE:
                    tex = (t.climate == apfa::OWC_SNOW) ? &g.assets.treeSnow
                                                        : &g.assets.tree[p.variant % 3];
                    break;
                case apfa::TP_PINE:                  tex = &g.assets.pine[p.variant % 3]; break;
                case apfa::TP_BUSH:                  tex = &g.assets.bush[p.variant & 1]; break;
                case apfa::TP_ROCK:                  tex = &g.assets.rock[p.variant % 3]; break;
                case apfa::TP_FENCE_H:               tex = &g.assets.fence; break;
                case apfa::TP_FENCE_POST:            tex = &g.assets.fencePost; break;
                case apfa::TP_TURRET:                tex = &g.assets.townRoundTurret; break;
                case apfa::TP_GATE_DOUBLE:           tex = &g.assets.townGateDouble; break;
                case apfa::TP_WALL_IVY:              tex = &g.assets.townWallIvy; break;
                case apfa::TP_STATUE:                tex = &g.assets.statue; break;
                case apfa::TP_COMMONS_STONE:         tex = &g.assets.statusStone[p.variant % 3]; break;
                case apfa::TP_COMMONS_BRAZIER:       tex = &g.assets.campfire; break;
                default: break;
            }
            if (!tex || tex->id == 0) continue;
            float cx = ((float)p.tx + 0.5f) * TILE - g.camX;
            float fy = (float)(p.ty + 1) * TILE - g.camY;
            if (p.kind == apfa::TP_GATE_DOUBLE) cx += 8.0f; // spans 2-tile north gate
            if (p.solid && p.kind != apfa::TP_FENCE_H)
                DrawEllipse((int)cx, (int)fy - 1, (float)tex->width * 0.30f, 2.5f, Fade(BLACK, 0.22f));
            texFootT(*tex, cx, fy);
            if (p.kind == apfa::TP_TORCH || p.kind == apfa::TP_COMMONS_BRAZIER) {
                float pulse = 0.5f + 0.5f * sinf(g.totalTime * 4.6f + (float)p.tx);
                DrawCircleGradient((int)cx, (int)(fy - 10), 18.0f + pulse * 5.0f,
                                   Fade(Color{ 255, 185, 80, 255 }, 0.22f), BLANK);
            }
        } else if (it.kind == 2) {
            const auto& c = t.chests[it.idx];
            bool opened = g.openedChests.count(c.id) > 0;
            Texture2D& tex = opened ? g.assets.chestOpen : g.assets.chestClosed;
            float cx = ((float)c.tx + 0.5f) * TILE - g.camX;
            float fy = (float)(c.ty + 1) * TILE - g.camY;
            DrawEllipse((int)cx, (int)fy - 1, 6.0f, 2.5f, Fade(BLACK, 0.24f));
            texFootT(tex, cx, fy);
        } else if (it.kind == 3) {
            const auto& n = sTown.npcs[it.idx];
            Texture2D& sheet = g.assets.npc[std::max(0, std::min((int)NS_KIND_COUNT - 1, n.npcSprite))];
            int frame = n.moving ? (1 + ((int)(n.animT * 6.0f) & 1)) : 0;
            float cx = n.x * TILE - g.camX;
            float fy = (n.y + 0.35f) * TILE - g.camY;
            DrawEllipse((int)cx, (int)fy - 2, 5.5f, 2.5f, Fade(BLACK, 0.24f));
            drawCharT(sheet, n.facing, frame, cx, fy, n.facing == 2 && n.sideFlip);
        } else {
            int klass = std::max(0, std::min(2, g.party[0].klass));
            Texture2D& sheet = g.assets.heroWalk[klass];
            int frame = g.moving ? (1 + ((int)(g.walkAnimT * 7.0f) & 1)) : 0;
            float cx = g.px * TILE - g.camX;
            float fy = (g.py + 0.35f) * TILE - g.camY;
            if (g.interactT > 0.0f) fy -= sinf((0.22f - g.interactT) * 14.0f) * 2.0f;
            DrawEllipse((int)cx, (int)fy - 2, 6.0f, 3.0f, Fade(BLACK, 0.28f));
            drawCharT(sheet, g.facing, frame, cx, fy, g.facing == 2 && g.sideFlip);
        }
    }
}
