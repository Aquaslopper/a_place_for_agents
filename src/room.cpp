// room.cpp — building interiors: shop, inn, temple, elder's hall & cottages
#include "game.hpp"
#include "world.hpp"
#include <cstdio>

enum RoomPropTex {
    RP_COUNTER = 0, RP_SHELF, RP_TABLE, RP_LONGTABLE, RP_BED, RP_BARREL, RP_KEG,
    RP_LANTERN, RP_CANDLE, RP_PLATE, RP_STATUE, RP_PILLAR, RP_PLANT,
    RP_URN, RP_STOOL, RP_FORGE_HEARTH, RP_ANVIL
};

static Texture2D* roomPropTexture(int tex) {
    switch (tex) {
        case RP_COUNTER:   return &g.assets.counter;
        case RP_SHELF:     return g.assets.shelf.id ? &g.assets.shelf : &g.assets.counter;
        case RP_TABLE:     return &g.assets.table;
        case RP_LONGTABLE: return g.assets.longtable.id ? &g.assets.longtable : &g.assets.table;
        case RP_BED:       return &g.assets.bed;
        case RP_BARREL:    return &g.assets.barrel;
        case RP_KEG:       return &g.assets.keg;
        case RP_LANTERN:   return &g.assets.lantern;
        case RP_CANDLE:    return &g.assets.candle;
        case RP_PLATE:     return &g.assets.plate;
        case RP_STATUE:    return &g.assets.statue;
        case RP_PILLAR:    return &g.assets.pillar;
        case RP_PLANT:     return &g.assets.plant;
        case RP_URN:       return &g.assets.urn;
        case RP_STOOL:     return &g.assets.stoolTex;
        case RP_FORGE_HEARTH: return &g.assets.forgeHearth;
        case RP_ANVIL: return &g.assets.forgeAnvil;
        default:           return &g.assets.barrel;
    }
}

void roomBuild(Room& room, const Building& b, const Village& v) {
    room = Room{};
    room.w = 15; room.h = 11;
    if (b.kind == 7) { room.w = 17; room.h = 12; }
    else if (b.kind == 8) { room.w = 17; room.h = 11; }
    else if (b.kind == 6) { room.w = 16; room.h = 11; }
    room.exitX = room.w * 0.5f;
    room.exitY = (float)(room.h - 1) - 0.1f;
    room.npcX = room.w * 0.5f;
    room.npcY = 2.6f;

    std::string vname = v.valid ? v.name : "Village";
    auto addP = [&](int tex, float x, float y, bool solid = true, float w = 0.85f, float h = 0.75f) {
        room.props.push_back({ tex, x, y, solid, w, h });
    };

    if (b.kind == 5) {
        room.kind = ROOM_SHOP;
        room.title = v.startVillage ? "Mira's Sundries" : (vname + " Sundries");
        g.roomNpcSprite = NS_MERCHANT;
        g.roomNpcFace = FACE_SHOPKEEPER;
        g.roomNpcName = v.startVillage ? "Mira the Trader" : "Village Trader";
        for (int x = 4; x <= room.w - 5; x++) addP(RP_COUNTER, x + 0.5f, 4.0f, true, 1.0f, 0.75f);
        // shelf.png is 44px (~2.75 tiles) wide
        addP(RP_SHELF, 3.3f, 1.8f, true, 2.6f, 0.85f);
        addP(RP_SHELF, room.w * 0.5f, 1.8f, true, 2.6f, 0.85f);
        addP(RP_SHELF, room.w - 3.3f, 1.8f, true, 2.6f, 0.85f);
        addP(RP_TABLE, 3.2f, 6.8f, true, 1.3f, 0.9f);
        addP(RP_PLATE, 3.2f, 6.45f, false, 0.4f, 0.4f);
        addP(RP_BARREL, 2.0f, room.h - 2.6f);
        addP(RP_BARREL, 3.0f, room.h - 2.6f);
        addP(RP_KEG,    room.w - 2.2f, room.h - 2.6f);
        addP(RP_KEG,    room.w - 3.3f, room.h - 2.6f);
        addP(RP_PLANT,  2.0f, 4.1f);
        addP(RP_PLANT,  room.w - 2.0f, 4.1f);
    } else if (b.kind == 6) {
        room.kind = ROOM_INN;
        room.title = v.startVillage ? "The Sleeping Stag Inn" : (vname + " Inn");
        g.roomNpcSprite = NS_MAYOR;
        g.roomNpcFace = FACE_INNKEEPER;
        g.roomNpcName = v.startVillage ? "Bren the Innkeep" : "Innkeeper";
        room.npcX = 4.5f; room.npcY = 2.6f;
        for (int x = 2; x <= 6; x++) addP(RP_COUNTER, x + 0.5f, 4.0f, true, 1.0f, 0.75f);
        addP(RP_SHELF, 4.5f, 1.8f, true, 2.6f, 0.85f);
        // Tavern dining table on left
        addP(RP_LONGTABLE, 4.4f, 7.1f, true, 2.7f, 0.9f);
        addP(RP_PLATE, 3.7f, 6.8f, false, 0.4f, 0.4f);
        addP(RP_PLATE, 5.1f, 6.8f, false, 0.4f, 0.4f);
        addP(RP_STOOL, 3.6f, 7.9f, false, 0.6f, 0.6f);
        addP(RP_STOOL, 5.2f, 7.9f, false, 0.6f, 0.6f);
        // Partition pillars separating common room from guest bunks
        addP(RP_PILLAR, 9.2f, 3.2f, true, 0.8f, 0.8f);
        addP(RP_PILLAR, 9.2f, 6.8f, true, 0.8f, 0.8f);
        addP(RP_BED,   room.w - 4.4f, 2.9f, true, 1.1f, 1.4f);
        addP(RP_BED,   room.w - 2.3f, 2.9f, true, 1.1f, 1.4f);
        addP(RP_BED,   room.w - 4.4f, 6.5f, true, 1.1f, 1.4f);
        addP(RP_BED,   room.w - 2.3f, 6.5f, true, 1.1f, 1.4f);
        addP(RP_LANTERN, 2.0f, 2.0f, false, 0.5f, 0.5f);
        addP(RP_KEG, 2.0f, room.h - 2.5f);
    } else if (b.kind == 7) {
        room.kind = ROOM_TEMPLE;
        room.title = v.startVillage ? "Chapel of the Meadow" : (vname + " Chapel");
        g.roomNpcSprite = NS_PRIEST;
        g.roomNpcFace = FACE_ELDER;
        g.roomNpcName = v.startVillage ? "Father Osric" : "Shrine Keeper";
        room.npcY = 3.2f;
        addP(RP_STATUE, room.w * 0.5f, 1.9f, true, 1.1f, 1.0f);
        addP(RP_CANDLE, room.w * 0.5f - 2.3f, 2.3f, false, 0.5f, 0.5f);
        addP(RP_CANDLE, room.w * 0.5f + 2.3f, 2.3f, false, 0.5f, 0.5f);
        for (int y = 4; y <= room.h - 4; y += 3) {
            addP(RP_PILLAR, 3.0f, (float)y, true, 0.8f, 0.8f);
            addP(RP_PILLAR, room.w - 3.0f, (float)y, true, 0.8f, 0.8f);
        }
        addP(RP_URN, 2.0f, 2.2f);
        addP(RP_URN, room.w - 2.0f, 2.2f);
        addP(RP_PLANT, 2.0f, room.h - 2.5f);
        addP(RP_PLANT, room.w - 2.0f, room.h - 2.5f);
    } else if (b.kind == 8) {
        // Elder's Hall / Council Manor
        room.kind = ROOM_HOUSE;
        room.title = v.startVillage ? "Elder's Hall" : (vname + " Council Hall");
        g.roomNpcSprite = NS_ELDER;
        g.roomNpcFace = FACE_ELDER;
        g.roomNpcName = v.startVillage ? "Old Maren" : "Village Elder";
        room.npcX = room.w * 0.5f;
        room.npcY = 2.9f;
        addP(RP_SHELF, 3.6f, 1.8f, true, 2.6f, 0.85f);
        addP(RP_SHELF, room.w - 3.6f, 1.8f, true, 2.6f, 0.85f);
        addP(RP_PILLAR, 3.0f, 4.4f, true, 0.8f, 0.8f);
        addP(RP_PILLAR, room.w - 3.0f, 4.4f, true, 0.8f, 0.8f);
        // Council map/archive tables flanking the central carpet
        addP(RP_LONGTABLE, 4.5f, 6.8f, true, 2.7f, 0.9f);
        addP(RP_LONGTABLE, room.w - 4.5f, 6.8f, true, 2.7f, 0.9f);
        addP(RP_CANDLE, 4.5f, 6.45f, false, 0.4f, 0.4f);
        addP(RP_CANDLE, room.w - 4.5f, 6.45f, false, 0.4f, 0.4f);
        addP(RP_STOOL, 4.5f, 7.65f, false, 0.6f, 0.6f);
        addP(RP_STOOL, room.w - 4.5f, 7.65f, false, 0.6f, 0.6f);
        addP(RP_URN, 2.0f, 2.2f);
        addP(RP_URN, room.w - 2.0f, 2.2f);
        addP(RP_PLANT, 2.0f, room.h - 2.5f);
        addP(RP_PLANT, room.w - 2.0f, room.h - 2.5f);
    } else if (b.kind == 10) {
        // Aldwin's Forge — the smithy interior
        room.kind = ROOM_FORGE;
        room.title = v.startVillage ? "Aldwin's Forge" : (vname + " Forge");
        g.roomNpcSprite = NS_SMITH;
        g.roomNpcFace = FACE_SMITH;
        g.roomNpcName = v.startVillage ? "Aldwin the Smith" : "Village Smith";
        room.npcX = room.w * 0.5f;
        room.npcY = 2.7f;
        // A working smithy, not a tavern table with plates and a candle.
        // Hearth left, stock right, clear counter and central visitor aisle.
        addP(RP_FORGE_HEARTH, 3.2f, 3.2f, true, 2.7f, 1.4f);
        addP(RP_ANVIL, 3.5f, 6.0f, true, 1.5f, 0.9f);
        addP(RP_SHELF, 11.6f, 1.8f, true, 2.6f, 0.85f);
        for (int x = 6; x <= 9; ++x) addP(RP_COUNTER, x + 0.5f, 4.2f, true, 1.0f, 0.75f);
        addP(RP_BARREL, 11.8f, 5.8f);
        addP(RP_BARREL, 12.8f, 5.8f);
        addP(RP_KEG, 2.1f, 7.7f);
        addP(RP_TABLE, 11.5f, 8.0f, true, 1.3f, 0.9f);
        addP(RP_LANTERN, 11.5f, 7.65f, false, 0.4f, 0.4f);

    } else {
        room.kind = ROOM_HOUSE;
        static const char* kCottageNames[5] = {
            "Weaver's Cottage", "Cooper's Cottage", "Herbalist's Home", "Cartographer's Nook", "Lantern Cottage"
        };
        room.title = v.startVillage ? kCottageNames[b.kind % 5] : "Village Cottage";
        g.roomNpcSprite = b.kind % 4;
        g.roomNpcFace = FACE_VILLAGER0 + (b.kind % 4);
        g.roomNpcName = v.startVillage ? (b.kind % 2 == 0 ? "Cottager" : "Townsperson") : "Homeowner";
        room.npcX = (b.kind & 1) ? 5.2f : 9.6f;
        room.npcY = 3.2f;
        addP(RP_BED,   2.5f, 2.9f, true, 1.1f, 1.4f);
        addP(RP_SHELF, room.w - 3.4f, 1.8f, true, 2.6f, 0.85f);
        addP(RP_TABLE, room.w - 4.2f, 5.6f, true, 1.3f, 0.9f);
        addP(RP_PLATE, room.w - 4.2f, 5.25f, false, 0.4f, 0.4f);
        addP(RP_STOOL, room.w - 5.4f, 5.8f, false, 0.6f, 0.6f);
        addP(RP_PLANT, 2.0f, room.h - 2.6f);
        addP(RP_BARREL, room.w - 2.2f, room.h - 2.6f);
    }
}

int roomPropNear(const Room& room, float x, float y, float r) {
    float bestD = r * r;
    int best = -1;
    for (int i = 0; i < (int)room.props.size(); i++) {
        float dx = room.props[i].x - x, dy = room.props[i].y - y;
        float d = dx * dx + dy * dy;
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

bool roomBlocked(const Room& room, float x, float y, float r) {
    bool inDoorGap = (fabsf(x - room.exitX) < 1.1f && y > room.h - 1.6f);
    if (x - r < 1.0f || x + r > room.w - 1.0f || y - r < 1.7f) return true;
    if (!inDoorGap && y + r > room.h - 1.0f) return true;
    if (y > room.h - 0.1f) return false;

    if (fabsf(x - room.npcX) < 0.48f + r && fabsf(y - room.npcY) < 0.42f + r) return true;

    for (const auto& p : room.props) {
        if (!p.solid) continue;
        if (fabsf(x - p.x) < p.w * 0.45f + r && fabsf(y - p.y) < p.h * 0.42f + r)
            return true;
    }
    return false;
}

static void roomOrigin(const Room& r, float& ox, float& oy) {
    ox = floorf((g.vw - r.w * TILE) * 0.5f);
    oy = floorf((g.vh - r.h * TILE) * 0.5f) + 4.0f;
}

void enterBuilding(int buildingIndex) {
    if (!g.villageLoaded || buildingIndex < 0 || buildingIndex >= (int)g.village.buildings.size()) return;
    const Building& b = g.village.buildings[buildingIndex];

    g.walkQueue.clear();
    g.hasMoveTarget = false;
    g.walkDoorBuilding = -1;
    g.walkTargetNpc = -1;
    g.walkTargetChest = 0;
    g.walkTargetInteract = false;

    roomBuild(g.room, b, g.village);
    g.room.buildingIndex = buildingIndex;
    g.roomPx = g.room.exitX;
    g.roomPy = g.room.exitY - 0.85f;
    g.roomFacing = 1;
    g.roomSideFlip = false;
    g.roomMoving = false;
    g.roomAnimT = 0;
    g.roomHasTarget = false;
    g.roomClickTalk = false;
    g.roomClickExit = false;

    if (g.village.startVillage) {
        g.shopTitle = "Mira's Sundries";
        g.innTitle  = "The Sleeping Stag";
    } else {
        g.shopTitle = g.village.name + " Sundries";
        g.innTitle  = g.village.name + " Inn";
    }
    g.shopForge = false;    // the forge re-arms this when its keeper talks shop

    g.mode = MODE_ROOM;
    g.fade = 1.0f; g.fadeDir = -1;
    playSoundIf(g.assets.door);
}

void exitRoom() {
    g.mode = MODE_OVERWORLD;
    g.roomHasTarget = false;
    g.roomClickTalk = false;
    g.roomClickExit = false;
    if (g.villageLoaded && g.room.buildingIndex >= 0 &&
        g.room.buildingIndex < (int)g.village.buildings.size()) {
        const Building& b = g.village.buildings[g.room.buildingIndex];
        int dtx = buildingDoorTx(b);
        int dty = buildingDoorTy(b);
        float ex = (float)dtx + 0.5f;
        float ey = (float)dty + 0.55f;
        if (positionBlocked(ex, ey, 0.28f)) {
            int ox = dtx, oy = dty + 1;
            nearestWalkable(dtx, dty, ox, oy);
            ex = ox + 0.5f; ey = oy + 0.5f;
        }
        g.px = ex;
        g.py = ey;
    }
    g.facing = 0;
    g.fade = 1.0f; g.fadeDir = -1;
    overworldSnapCamera();
    playSoundIf(g.assets.door);
}

static void talkToKeeper() {
    std::vector<DialogLine> lines;
    DialogAction act = DLG_NONE;
    if (g.room.kind == ROOM_SHOP) {
        g.shopForge = false;
        lines.push_back({ g.roomNpcFace, g.roomNpcName, "\"Steel, salves, and sundries. Buying or selling?\"" });
        act = DLG_SHOP;
    } else if (g.room.kind == ROOM_FORGE) {
        g.shopForge = true;
        g.shopTitle = g.room.title;
        lines.push_back({ g.roomNpcFace, g.roomNpcName, "\"Ore, ash and honest work. Weapons and armour, if you've the coin.\"" });
        act = DLG_SHOP;
    } else if (g.room.kind == ROOM_INN) {
        char buf[128];
        snprintf(buf, sizeof(buf), "\"A warm bed's %d gil, blankets included. Care to stay the night?\"", g.innCost);
        lines.push_back({ g.roomNpcFace, g.roomNpcName, buf });
        act = DLG_INN;
    } else if (g.room.kind == ROOM_TEMPLE) {
        lines.push_back({ g.roomNpcFace, g.roomNpcName, "\"The Light asks nothing but a moment of stillness.\"" });
        act = DLG_TEMPLE;
    } else if (g.roomNpcFace == FACE_ELDER && g.villageLoaded && g.village.startVillage) {
        if (g.quests[QUEST_CROWN].state == QS_DONE) {
            lines.push_back({ g.roomNpcFace, g.roomNpcName, "\"The Crown gleams once more in Elmswick. The road beyond is yours.\"" });
        } else if (itemCount(IT_CROWN) > 0 || g.bossDefeated) {
            lines.push_back({ g.roomNpcFace, g.roomNpcName, "\"You brought back the Crown of Elm! Take this Dragon Scale armour and 500 gil, heroes!\"" });
            act = DLG_CROWN_REWARD;
        } else if (g.quests[QUEST_CROWN].state == QS_ACTIVE) {
            std::string hint = compassHint(g.px, g.py, (float)BOSS_CASTLE_TX, (float)BOSS_CASTLE_TY);
            lines.push_back({ g.roomNpcFace, g.roomNpcName, "\"Follow the North Road (" + hint + ") to the old Castle of Elm.\"" });
        } else {
            lines.push_back({ g.roomNpcFace, g.roomNpcName, "\"The Slime King seized the old Castle of Elm up the North Road — along with our village's ancient Crown.\"" });
            lines.push_back({ g.roomNpcFace, g.roomNpcName, "\"When you are ready, reclaim the Crown of Elm!\"" });
            act = DLG_START_QUEST_CROWN;
        }
    } else {
        static const char* kHomeLines[4] = {
            "\"Make yourselves at home! Mind the third floorboard — it squeaks.\"",
            "\"If you're heading past Millbrook, pack an Antidote or two. Hornets nest in the reeds.\"",
            "\"Pip's herbs kept half the village standing through last winter's frost.\"",
            "\"Old Maren keeps every survey chart in the Elder's Hall up by the north bluff.\""
        };
        int idx = (g.room.buildingIndex >= 0) ? (g.room.buildingIndex & 3) : 0;
        lines.push_back({ g.roomNpcFace, g.roomNpcName, kHomeLines[idx] });
    }
    startDialog(lines, act);
}

void roomUpdate(float dt) {
    gAnimTime += dt;
    g.totalTime += dt;
    g.runTime += dt;
    if (g.toastT > 0) g.toastT = std::max(0.0f, g.toastT - dt);

    Room& r = g.room;
    int kx = 0, ky = 0;
    if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) kx -= 1;
    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) kx += 1;
    if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)) ky -= 1;
    if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)) ky += 1;

    float speed = (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) ? 6.5f : 4.2f;
    g.roomMoving = false;

    if (kx != 0 || ky != 0) {
        g.roomHasTarget = false;
        g.roomClickTalk = false;
        g.roomClickExit = false;
        if (kx != 0 && ky != 0) kx = 0;
        if (kx != 0) { g.roomFacing = 2; g.roomSideFlip = (kx < 0); }
        else g.roomFacing = (ky < 0) ? 1 : 0;

        float nx = g.roomPx + kx * speed * dt;
        float ny = g.roomPy + ky * speed * dt;
        if (kx != 0 && !roomBlocked(r, nx, g.roomPy, 0.28f)) { g.roomPx = nx; g.roomMoving = true; }
        else if (ky != 0 && !roomBlocked(r, g.roomPx, ny, 0.28f)) { g.roomPy = ny; g.roomMoving = true; }
    } else if (g.roomHasTarget) {
        float dx = g.roomPx - g.roomTargetX;
        float dy = g.roomPy - g.roomTargetY;
        dx = -dx; dy = -dy;
        float dist = sqrtf(dx * dx + dy * dy);
        if (dist <= speed * dt + 0.14f) {
            if (!roomBlocked(r, g.roomTargetX, g.roomTargetY, 0.28f)) {
                g.roomPx = g.roomTargetX; g.roomPy = g.roomTargetY;
            }
            bool doTalk = g.roomClickTalk;
            bool doExit = g.roomClickExit;
            g.roomHasTarget = false; g.roomClickTalk = false; g.roomClickExit = false;
            if (doExit) { exitRoom(); return; }
            if (doTalk) { talkToKeeper(); return; }
        } else {
            float vx = dx / dist, vy = dy / dist;
            if (fabsf(vx) > fabsf(vy)) { g.roomFacing = 2; g.roomSideFlip = vx < 0; }
            else g.roomFacing = (vy < 0) ? 1 : 0;
            float nx = g.roomPx + vx * speed * dt;
            float ny = g.roomPy + vy * speed * dt;
            bool moved = false;
            if (!roomBlocked(r, nx, g.roomPy, 0.28f)) { g.roomPx = nx; moved = true; }
            if (!roomBlocked(r, g.roomPx, ny, 0.28f)) { g.roomPy = ny; moved = true; }
            g.roomMoving = moved;
            if (!moved) {
                bool doTalk = g.roomClickTalk;
                bool doExit = g.roomClickExit;
                g.roomHasTarget = false; g.roomClickTalk = false; g.roomClickExit = false;
                float ndx = g.roomPx - r.npcX, ndy = g.roomPy - r.npcY;
                if (doTalk && ndx * ndx + ndy * ndy < 3.0f * 3.0f) talkToKeeper();
                else if (doExit && g.roomPy > r.exitY - 1.5f) exitRoom();
            }
        }
    }

    if (g.roomMoving) g.roomAnimT += dt;

    if (g.roomPy >= r.exitY - 0.15f && fabsf(g.roomPx - r.exitX) < 1.25f) {
        exitRoom();
        return;
    }

    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_TAB)) {
        g.menuReturn = MODE_ROOM;
        g.mode = MODE_MENU;
        g.menuPage = 0; g.menuIndex = 0;
        playSoundIf(g.assets.confirm);
        return;
    }

    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_E) || IsKeyPressed(KEY_ENTER)) {
        float dx = g.roomPx - r.npcX, dy = g.roomPy - r.npcY;
        if (dx * dx + dy * dy < 3.0f * 3.0f) {
            talkToKeeper();
        } else if (g.roomPy > r.exitY - 1.6f && fabsf(g.roomPx - r.exitX) < 1.8f) {
            exitRoom();
        } else {
            pushToast("Nothing to examine here.");
        }
        return;
    }

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        Vector2 m = getVirtualMouse();
        float ox, oy; roomOrigin(r, ox, oy);
        float rx = (m.x - ox) / TILE;
        float ry = (m.y - oy) / TILE;

        if (fabsf(rx - r.npcX) < 1.5f && ry >= r.npcY - 1.2f && ry <= r.npcY + 2.4f) {
            float dx = g.roomPx - r.npcX, dy = g.roomPy - r.npcY;
            if (dx * dx + dy * dy < 3.0f * 3.0f) {
                talkToKeeper();
            } else {
                g.roomHasTarget = true;
                g.roomTargetX = r.npcX;
                g.roomTargetY = std::min((float)(r.h - 2), r.npcY + 2.3f);
                g.roomClickTalk = true;
                g.roomClickExit = false;
            }
            return;
        }
        if (ry >= r.exitY - 0.9f && ry <= r.exitY + 1.2f && fabsf(rx - r.exitX) < 1.6f) {
            g.roomHasTarget = true;
            g.roomTargetX = r.exitX;
            g.roomTargetY = r.exitY;
            g.roomClickExit = true;
            g.roomClickTalk = false;
            return;
        }
        if (rx > 1.0f && rx < r.w - 1.0f && ry > 1.7f && ry < r.h - 1.0f) {
            g.roomHasTarget = true;
            g.roomTargetX = rx;
            g.roomTargetY = ry;
            g.roomClickTalk = false;
            g.roomClickExit = false;
        }
    }
}

void roomDraw() {
    ClearBackground(Color{ 10, 12, 22, 255 });
    const Room& r = g.room;
    float ox, oy;
    roomOrigin(r, ox, oy);

    // Outer frame + soft drop shadow
    DrawRectangle((int)ox - 5, (int)oy - 3, r.w * TILE + 12, r.h * TILE + 10, Color{ 4, 6, 12, 255 });
    DrawRectangle((int)ox - 3, (int)oy - 3, r.w * TILE + 6, r.h * TILE + 6, Color{ 6, 8, 16, 255 });
    DrawRectangleLines((int)ox - 2, (int)oy - 2, r.w * TILE + 4, r.h * TILE + 4, Color{ 86, 102, 148, 255 });

    bool isHall = (r.title.find("Hall") != std::string::npos);
    Texture2D& floorTex = (r.kind == ROOM_TEMPLE || r.kind == ROOM_FORGE) ? g.assets.stoneFloor
                        : ((r.kind == ROOM_SHOP || isHall) ? g.assets.tileFloor : g.assets.plankFloor);
    Texture2D& wallTex = g.assets.brickFloor[(r.kind == ROOM_TEMPLE || isHall) ? 1 : 0];

    for (int y = 0; y < r.h; y++) {
        for (int x = 0; x < r.w; x++) {
            float sx = ox + x * TILE, sy = oy + y * TILE;
            bool isDoor = (y == r.h - 1 && fabsf((x + 0.5f) - r.exitX) < 1.1f);
            bool isWall = (x == 0 || x == r.w - 1 || y == 0 || (y == r.h - 1 && !isDoor));
            if (isWall) {
                DrawRectangle((int)sx, (int)sy, TILE, TILE, Color{ 44, 38, 46, 255 });
                if (wallTex.id)
                    DrawTexturePro(wallTex, { 0, 0, (float)wallTex.width, (float)wallTex.height },
                                   { sx, sy, TILE, TILE }, { 0, 0 }, 0.0f, Color{ 152, 140, 132, 255 });
            } else {
                if (floorTex.id)
                    DrawTexturePro(floorTex, { 0, 0, (float)floorTex.width, (float)floorTex.height },
                                   { sx, sy, TILE, TILE }, { 0, 0 }, 0.0f, WHITE);
                else DrawRectangle((int)sx, (int)sy, TILE, TILE, Color{ 98, 72, 48, 255 });
            }
        }
    }

    // Back wall trim strip + warm interior ambient vignette
    DrawRectangle((int)(ox + TILE), (int)(oy + TILE), (r.w - 2) * TILE, 8, Color{ 62, 48, 42, 255 });
    DrawLine((int)(ox + TILE), (int)(oy + TILE + 8), (int)(ox + (r.w - 1) * TILE), (int)(oy + TILE + 8),
             Color{ 30, 22, 24, 255 });

    // Central nave / council runner carpet for Chapel & Elder's Hall
    if ((r.kind == ROOM_TEMPLE || isHall) && g.assets.carpet.id) {
        DrawTexturePro(g.assets.carpet, { 0, 0, (float)g.assets.carpet.width, (float)g.assets.carpet.height },
                       { ox + (r.exitX - 1.1f) * TILE, oy + 3.1f * TILE, TILE * 2.2f, (r.h - 4.8f) * TILE },
                       { 0, 0 }, 0.0f, Fade(WHITE, 0.92f));
    }

    // Exit rug
    if (g.assets.carpet.id) {
        DrawTexturePro(g.assets.carpet, { 0, 0, (float)g.assets.carpet.width, (float)g.assets.carpet.height },
                       { ox + (r.exitX - 1.0f) * TILE, oy + (r.h - 1.6f) * TILE, TILE * 2.0f, TILE * 1.5f },
                       { 0, 0 }, 0.0f, WHITE);
    } else {
        DrawRectangle((int)(ox + (r.exitX - 1.0f) * TILE), (int)(oy + (r.h - 1.6f) * TILE),
                      TILE * 2, (int)(TILE * 1.5f), Color{ 126, 44, 48, 255 });
    }

    // Y-sort props, keeper, and player cleanly
    struct Item { float fy; int type; int idx; };
    std::vector<Item> items;
    items.reserve(r.props.size() + 2);
    for (int i = 0; i < (int)r.props.size(); i++) {
        float sortY = r.props[i].y * TILE;
        if (r.props[i].tex == RP_PLATE || r.props[i].tex == RP_CANDLE)
            sortY += 8.0f; // tabletop items sort cleanly in front of the table surface
        items.push_back({ sortY, 0, i });
    }
    items.push_back({ (r.npcY + 0.35f) * TILE, 1, 0 });
    items.push_back({ (g.roomPy + 0.35f) * TILE, 2, 0 });
    std::stable_sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.fy < b.fy; });

    for (const auto& it : items) {
        if (it.type == 0) {
            const RoomProp& p = r.props[it.idx];
            Texture2D* t = roomPropTexture(p.tex);
            if (!t || t->id == 0) continue;
            float cx = ox + p.x * TILE;
            float fy = oy + (p.y + 0.45f) * TILE;
            if (p.tex == RP_FORGE_HEARTH) {
                float flicker = 0.8f + sinf(g.totalTime * 7.0f) * 0.12f + sinf(g.totalTime * 11.0f) * 0.08f;
                DrawCircleGradient((int)cx, (int)fy - 9, 34,
                                   Fade(Color{245, 154, 73, 255}, flicker * 0.22f), BLANK);
            }
            DrawTexturePro(*t, { 0, 0, (float)t->width, (float)t->height },
                           { floorf(cx - t->width * 0.5f), floorf(fy - t->height), (float)t->width, (float)t->height },
                           { 0, 0 }, 0.0f, WHITE);
        } else if (it.type == 1) {
            Texture2D& s = g.assets.npc[std::max(0, std::min(NS_KIND_COUNT - 1, g.roomNpcSprite))];
            if (s.id == 0) continue;
            float cx = ox + r.npcX * TILE;
            float fy = oy + (r.npcY + 0.35f) * TILE;
            DrawEllipse((int)cx, (int)fy - 1, 5.5f, 2.5f, Fade(BLACK, 0.25f));
            drawCharFrame(s, 0, false, false, 0.0f, cx, fy);
        } else {
            Texture2D& sheet = g.assets.heroWalk[std::max(0, std::min(2, g.party[0].klass))];
            float cx = ox + g.roomPx * TILE;
            float fy = oy + (g.roomPy + 0.35f) * TILE;
            DrawEllipse((int)cx, (int)fy - 1, 5.5f, 2.5f, Fade(BLACK, 0.28f));
            drawCharFrame(sheet, g.roomFacing, g.roomSideFlip, g.roomMoving, g.roomAnimT, cx, fy);
        }
    }

    // Room title header
    if (g.mode != MODE_MENU) {
        Vector2 tsz = measureText(r.title.c_str(), 8);
        float tw = std::max(148.0f, tsz.x + 24.0f);
        drawWindow({ floorf(g.vw * 0.5f - tw * 0.5f), 5.0f, tw, 21.0f });
        drawTextCentered(r.title.c_str(), g.vw * 0.5f, 12.0f, Color{ 255, 236, 176, 255 }, 8);
    }

    // Bottom prompt & toast ONLY when active in MODE_ROOM (never beneath MODE_DIALOG or MODE_MENU!)
    if (g.mode == MODE_ROOM) {
        float dx = g.roomPx - r.npcX, dy = g.roomPy - r.npcY;
        const char* hint = (dx * dx + dy * dy < 3.0f * 3.0f)
            ? "SPACE or CLICK: talk  |  ESC: menu"
            : "Walk down or click rug to leave  |  ESC: menu";
        float hw = measureText(hint, 8).x + 20.0f;
        drawWindowBlue({ floorf(g.vw * 0.5f - hw * 0.5f), (float)g.vh - 22.0f, hw, 18.0f });
        drawTextCentered(hint, g.vw * 0.5f, (float)g.vh - 17.0f, Color{ 235, 242, 255, 245 }, 8);
        drawToast();
    }
}
