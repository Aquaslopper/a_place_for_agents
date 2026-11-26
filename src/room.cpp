// room.cpp — the interiors behind every village door
#include "game.hpp"
#include "world.hpp"
#include <cstdio>

static void addProp(Room& r, int tex, float x, float y, bool solid, float w, float h) {
    r.props.push_back({ tex, x, y, solid, w, h });
}
enum { RT_SHELF = 0, RT_COUNTER, RT_TABLE, RT_BARREL, RT_KEG, RT_LANTERN, RT_CANDLE, RT_PLATE,
       RT_PLANT, RT_STOOL, RT_BED, RT_CHEST_C, RT_CHEST_O, RT_URN, RT_CARPET, RT_ARMOUR,
       RT_WEAPONS, RT_POTION, RT_FIREPLACE, RT_CAULDRON, RT_BOOKS, RT_SIGN };

static Texture2D* roomTexture(int id) {
    Assets& a = g.assets;
    switch (id) {
        case RT_SHELF:     return &a.shelf;
        case RT_COUNTER:   return &a.counter;
        case RT_TABLE:     return &a.table;
        case RT_BARREL:    return &a.barrel;
        case RT_KEG:       return &a.keg;
        case RT_LANTERN:   return &a.lantern;
        case RT_CANDLE:    return &a.candle;
        case RT_PLATE:     return &a.plate;
        case RT_PLANT:     return &a.plant;
        case RT_STOOL:     return &a.stoolTex;
        case RT_BED:       return &a.bed;
        case RT_CHEST_C:   return &a.chestClosed;
        case RT_CHEST_O:   return &a.chestOpen;
        case RT_URN:       return &a.urn;
        case RT_CARPET:    return &a.carpet;
        case RT_ARMOUR:    return &a.statue;
        case RT_WEAPONS:   return &a.shelf;
        case RT_POTION:    return &a.plate;
        case RT_FIREPLACE: return &a.campfire;
        case RT_CAULDRON:  return &a.cauldron;
        case RT_BOOKS:     return &a.shelf;
        case RT_SIGN:      return &a.sign;
        default:           return nullptr;
    }
}

void roomBuild(Room& room, const Building& b, const Village& v) {
    room = Room{};
    room.buildingIndex = -1;
    for (size_t i = 0; i < v.buildings.size(); i++) {
        const Building& vb = v.buildings[i];
        if (vb.tx == b.tx && vb.ty == b.ty) { room.buildingIndex = (int)i; break; }
    }
    room.w = 15;
    room.h = 11;
    room.exitX = room.w * 0.5f;
    room.exitY = room.h - 2.5f;

    switch (b.kind) {
        case 5: room.kind = ROOM_SHOP;   room.title = "Mira's Sundries";    break;
        case 6: room.kind = ROOM_INN;    room.title = "The Sleeping Stag";  break;
        case 7: room.kind = ROOM_TEMPLE; room.title = "Temple of the Light"; break;
        default: room.kind = ROOM_HOUSE; room.title = "A Warm Hearth";      break;
    }

    // the counters and shelves that make each room read at a glance
    if (room.kind == ROOM_SHOP) {
        addProp(room, RT_COUNTER, 3, 5, true, 3, 1);
        addProp(room, RT_COUNTER, 8, 5, true, 3, 1);
        addProp(room, RT_SHELF, 2, 2, false, 3, 2);
        addProp(room, RT_SHELF, 7, 2, false, 3, 2);
        addProp(room, RT_SHELF, 12, 2, false, 3, 2);
        addProp(room, RT_BARREL, 13, 6, true, 1, 1);
        addProp(room, RT_KEG, 1, 6, true, 1, 1);
        addProp(room, RT_LANTERN, 6, 2, false, 1, 1);
        addProp(room, RT_PLATE, 4, 5, false, 1, 1);
        addProp(room, RT_POTION, 9, 5, false, 1, 1);
    } else if (room.kind == ROOM_INN) {
        addProp(room, RT_COUNTER, 3, 4, true, 3, 1);
        addProp(room, RT_KEG, 2, 6, true, 1, 1);
        addProp(room, RT_BARREL, 3, 6, true, 1, 1);
        addProp(room, RT_TABLE, 8, 6, true, 2, 1);
        addProp(room, RT_STOOL, 7, 6, false, 1, 1);
        addProp(room, RT_STOOL, 10, 6, false, 1, 1);
        addProp(room, RT_BED, 12, 2, true, 2, 1);
        addProp(room, RT_BED, 12, 4, true, 2, 1);
        addProp(room, RT_FIREPLACE, 6, 2, true, 1, 1);
        addProp(room, RT_CANDLE, 9, 3, false, 1, 1);
        addProp(room, RT_LANTERN, 1, 2, false, 1, 1);
        addProp(room, RT_CARPET, 8, 8, false, 3, 2);
    } else if (room.kind == ROOM_TEMPLE) {
        addProp(room, RT_SIGN, 7, 2, true, 2, 2);
        addProp(room, RT_CANDLE, 4, 3, false, 1, 1);
        addProp(room, RT_CANDLE, 10, 3, false, 1, 1);
        addProp(room, RT_CARPET, 7, 6, false, 3, 4);
        addProp(room, RT_LANTERN, 2, 4, false, 1, 1);
        addProp(room, RT_LANTERN, 12, 4, false, 1, 1);
        addProp(room, RT_PLANT, 1, 2, false, 1, 1);
        addProp(room, RT_PLANT, 13, 2, false, 1, 1);
    } else {
        addProp(room, RT_TABLE, 5, 4, true, 2, 1);
        addProp(room, RT_STOOL, 4, 4, false, 1, 1);
        addProp(room, RT_STOOL, 7, 4, false, 1, 1);
        addProp(room, RT_BED, 2, 2, true, 2, 1);
        addProp(room, RT_FIREPLACE, 9, 2, true, 1, 1);
        addProp(room, RT_SHELF, 12, 2, false, 2, 2);
        addProp(room, RT_PLANT, 13, 5, false, 1, 1);
        addProp(room, RT_CHEST_C, 1, 6, true, 1, 1);
        addProp(room, RT_CARPET, 6, 6, false, 3, 2);
        addProp(room, RT_URN, 11, 6, true, 1, 1);
    }
}

bool roomBlocked(const Room& room, float x, float y, float r) {
    // walls
    if (x < 1.0f + r || x > room.w - 1.0f - r) return true;
    if (y < 1.0f + r || y > room.h - 1.0f - r) return true;
    for (const auto& p : room.props) {
        if (!p.solid) continue;
        if (x > p.x - 0.5f - r && x < p.x + p.w - 0.5f + r &&
            y > p.y - 0.5f - r && y < p.y + p.h - 0.5f + r)
            return true;
    }
    return false;
}

int roomPropNear(const Room& room, float x, float y, float r) {
    for (int i = 0; i < (int)room.props.size(); i++) {
        const auto& p = room.props[i];
        float dx = (p.x + p.w * 0.5f - 0.5f) - x;
        float dy = (p.y + p.h * 0.5f - 0.5f) - y;
        if (dx * dx + dy * dy < r * r) return i;
    }
    return -1;
}

// ================================================================ enter / exit
void enterBuilding(int buildingIndex) {
    if (!g.villageLoaded) return;
    if (buildingIndex < 0 || buildingIndex >= (int)g.village.buildings.size()) return;
    const Building& b = g.village.buildings[buildingIndex];
    roomBuild(g.room, b, g.village);
    g.roomPx = g.room.exitX;
    g.roomPy = g.room.exitY;
    g.roomFacing = 1;
    g.roomMoving = false;
    g.roomAnimT = 0;
    g.mode = MODE_ROOM;
    g.fade = 0.6f;
    PlaySound(g.assets.door);

    // who is behind the counter
    g.roomNpcSprite = 11;        // merchant
    g.roomNpcFace = FACE_SHOPKEEPER;
    g.roomNpcName = "Mira";
    if (g.room.kind == ROOM_INN) { g.roomNpcSprite = 6; g.roomNpcFace = FACE_INNKEEPER; g.roomNpcName = "Bren"; }
    if (g.room.kind == ROOM_TEMPLE) { g.roomNpcSprite = 13; g.roomNpcFace = FACE_ELDER; g.roomNpcName = "Father Osric"; }
    if (g.room.kind == ROOM_HOUSE) { g.roomNpcSprite = 0; g.roomNpcFace = FACE_VILLAGER0; g.roomNpcName = "A villager"; }
}

void exitRoom() {
    g.mode = MODE_OVERWORLD;
    g.fade = 0.5f;
    PlaySound(g.assets.door);
    // step back out in front of the door
    if (g.villageLoaded && g.room.buildingIndex >= 0 &&
        g.room.buildingIndex < (int)g.village.buildings.size()) {
        const Building& b = g.village.buildings[g.room.buildingIndex];
        g.px = (float)b.tx + 0.5f;
        g.py = (float)b.ty + 2.8f;
    }
    overworldSnapCamera();
    g.menuPage = 0;
}

// ================================================================ update
void roomUpdate(float dt) {
    g.totalTime += dt;
    g.runTime += dt;
    if (g.toastT > 0) g.toastT -= dt;

    Room& r = g.room;
    int kx = 0, ky = 0;
    if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) kx -= 1;
    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) kx += 1;
    if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)) ky -= 1;
    if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)) ky += 1;
    if (kx != 0 && ky != 0) kx = 0;

    const float speed = 3.4f;
    bool moved = false;
    if (kx != 0) {
        g.roomFacing = 2;
        float nx = g.roomPx + kx * speed * dt;
        if (!roomBlocked(r, nx, g.roomPy, 0.28f)) { g.roomPx = nx; moved = true; }
    } else if (ky != 0) {
        g.roomFacing = ky > 0 ? 0 : 1;
        float ny = g.roomPy + ky * speed * dt;
        if (!roomBlocked(r, g.roomPx, ny, 0.28f)) { g.roomPy = ny; moved = true; }
    }
    if (moved) { g.roomAnimT += dt; g.roomMoving = true; } else g.roomMoving = false;

    // leave through the door mat at the bottom
    if (g.roomPy > r.h - 2.0f && g.roomPy < r.h - 0.6f &&
        fabsf(g.roomPx - r.exitX) < 1.2f && ky > 0) {
        exitRoom();
        return;
    }

    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_TAB)) {
        g.mode = MODE_MENU; g.menuPage = 0;
        PlaySound(g.assets.confirm);
        return;
    }
    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_E)) {
        // talk to the keeper behind the counter
        float dx = 7.5f - g.roomPx, dy = 4.5f - g.roomPy;
        if (dx * dx + dy * dy < 5.0f) {
            std::vector<DialogLine> lines;
            DialogAction action = DLG_NONE;
            if (g.room.kind == ROOM_SHOP) {
                lines.push_back({ FACE_SHOPKEEPER, "Mira", "Steel, salves and sundries. What's it to be?" });
                action = DLG_SHOP;
            } else if (g.room.kind == ROOM_INN) {
                lines.push_back({ FACE_INNKEEPER, "Bren", "\"A bed's 20 gil. Blankets included. Monsters extra.\"" });
                action = DLG_INN;
            } else if (g.room.kind == ROOM_TEMPLE) {
                lines.push_back({ FACE_ELDER, "Father Osric", "Kneel, traveller. The Light does the rest." });
                action = DLG_TEMPLE;
            } else {
                lines.push_back({ FACE_VILLAGER0, "Villager", "Mind the rug. It's older than my grandmother." });
            }
            g.dialogReturn = MODE_ROOM;
            startDialog(lines, action);
            return;
        }
        // examine furniture
        int i = roomPropNear(r, g.roomPx, g.roomPy, 1.2f);
        if (i >= 0) {
            const RoomProp& p = r.props[i];
            const char* msg = "Nothing special.";
            if (p.tex == RT_CHEST_C) msg = "A chest. It isn't yours, but nobody's watching.";
            else if (p.tex == RT_SHELF) msg = "Shelves of odd bottles, each one labelled in someone's hurry.";
            else if (p.tex == RT_BED) msg = "A bed. It looks like it has heard things.";
            else if (p.tex == RT_FIREPLACE) msg = "The fire mutters and settles.";
            std::vector<DialogLine> lines;
            lines.push_back({ -1, "", msg });
            g.dialogReturn = MODE_ROOM;
            startDialog(lines);
        }
    }
}

// ================================================================ draw
static void drawRoomFloor() {
    Room& r = g.room;
    Texture2D* floorTex = &g.assets.plankFloor;
    if (r.kind == ROOM_TEMPLE) floorTex = &g.assets.stoneFloor;
    else if (r.kind == ROOM_SHOP) floorTex = &g.assets.tileFloor;
    if (!floorTex->id) floorTex = &g.assets.cobble;

    for (int ty = 0; ty < r.h; ty++) {
        for (int tx = 0; tx < r.w; tx++) {
            DrawTexturePro(*floorTex, { 0, 0, (float)floorTex->width, (float)floorTex->height },
                           { 26.0f + tx * TILE, 20.0f + ty * TILE, (float)TILE, (float)TILE },
                           { 0, 0 }, 0.0f, (ty % 2) ? Color{ 245, 240, 235, 255 } : WHITE);
        }
    }
    // walls
    Texture2D* wallTex = &g.assets.brickFloor[0];
    if (!wallTex->id) wallTex = &g.assets.flagstone;
    for (int tx = 0; tx < r.w; tx++) {
        DrawTexturePro(*wallTex, { 0, 0, (float)wallTex->width, (float)wallTex->height },
                       { 26.0f + tx * TILE, 4.0f, (float)TILE, (float)TILE }, { 0, 0 }, 0.0f,
                       Color{ 190, 190, 200, 255 });
    }
    for (int ty = 0; ty < r.h; ty++) {
        DrawTexturePro(*wallTex, { 0, 0, (float)wallTex->width, (float)wallTex->height },
                       { 10.0f, 20.0f + ty * TILE, (float)TILE, (float)TILE }, { 0, 0 }, 0.0f,
                       Color{ 150, 150, 165, 255 });
        DrawTexturePro(*wallTex, { 0, 0, (float)wallTex->width, (float)wallTex->height },
                       { 26.0f + r.w * TILE, 20.0f + ty * TILE, (float)TILE, (float)TILE },
                       { 0, 0 }, 0.0f, Color{ 150, 150, 165, 255 });
    }
    // door mat
    DrawRectangle((int)(26 + (r.exitX - 1) * TILE), (int)(20 + (r.h - 1.6f) * TILE),
                  TILE * 2, TILE / 2, Color{ 80, 60, 40, 255 });
}

void roomDraw() {
    Room& r = g.room;
    DrawRectangle(0, 0, VRES_W, VRES_H, Color{ 8, 8, 12, 255 });
    drawWindow({ 4, 4, VRES_W - 8, VRES_H - 8 });
    drawRoomFloor();

    // props, y-sorted with the player
    struct P { float y; int idx; };
    static std::vector<P> order;
    order.clear();
    for (int i = 0; i < (int)r.props.size(); i++)
        order.push_back({ r.props[i].y + r.props[i].h, i });
    std::sort(order.begin(), order.end(), [](const P& a, const P& b) { return a.y < b.y; });

    auto drawProp = [&](const RoomProp& p) {
        Texture2D* t = roomTexture(p.tex);
        if (!t || t->id == 0) return;
        float footX = 26.0f + (p.x - 0.5f) * TILE + (p.w * TILE) * 0.5f;
        float footY = 20.0f + (p.y + p.h - 1.0f) * TILE + TILE;
        float scale = 1.0f;
        if (p.w > 1.0f) scale = std::min(2.0f, (p.w * TILE) / (float)t->width);
        float w = t->width * scale, h = t->height * scale;
        DrawEllipse((int)footX, (int)footY - 1, w * 0.32f, 3.0f, Fade(BLACK, 0.2f));
        DrawTexturePro(*t, { 0, 0, (float)t->width, (float)t->height },
                       { footX - w * 0.5f, footY - h, w, h }, { 0, 0 }, 0.0f, WHITE);
    };

    // the keeper stands behind the counter
    float npcX = 7.5f, npcY = 4.2f;
    Texture2D& keeper = g.assets.npc[std::max(0, std::min(15, g.roomNpcSprite))];
    float playerFoot = 20.0f + (g.roomPy + 0.5f) * TILE;

    bool drewPlayer = false;
    for (auto& o : order) {
        float propFoot = 20.0f + (r.props[o.idx].y + r.props[o.idx].h) * TILE;
        if (!drewPlayer && playerFoot < propFoot) {
            // draw keeper then player
            if (keeper.id) {
                DrawTexturePro(keeper, { 24, 32, 24, 32 },
                               { 26.0f + (npcX - 0.5f) * TILE, 20.0f + (npcY) * TILE, 24, 32 },
                               { 0, 0 }, 0.0f, WHITE);
            }
            int row = g.roomFacing == 0 ? 0 : (g.roomFacing == 1 ? 1 : 2);
            int frame = g.roomMoving ? (1 + ((int)(g.roomAnimT * 7.0f) & 1)) : 1;
            Texture2D& hero = g.assets.heroWalk[std::max(0, std::min(2, g.party[0].klass))];
            Rectangle src{ (float)(frame * 24), (float)(row * 32), 24, 32 };
            Rectangle dst{ 26.0f + (g.roomPx - 0.5f) * TILE, playerFoot - 32, 24, 32 };
            DrawEllipse((int)(dst.x + 12), (int)playerFoot - 2, 6.0f, 3.0f, Fade(BLACK, 0.25f));
            DrawTexturePro(hero, src, dst, { 0, 0 }, 0.0f, WHITE);
            drewPlayer = true;
        }
        drawProp(r.props[o.idx]);
    }
    if (!drewPlayer) {
        if (keeper.id) {
            DrawTexturePro(keeper, { 24, 32, 24, 32 },
                           { 26.0f + (npcX - 0.5f) * TILE, 20.0f + npcY * TILE, 24, 32 },
                           { 0, 0 }, 0.0f, WHITE);
        }
        int row = g.roomFacing == 0 ? 0 : (g.roomFacing == 1 ? 1 : 2);
        int frame = g.roomMoving ? (1 + ((int)(g.roomAnimT * 7.0f) & 1)) : 1;
        Texture2D& hero = g.assets.heroWalk[std::max(0, std::min(2, g.party[0].klass))];
        Rectangle src{ (float)(frame * 24), (float)(row * 32), 24, 32 };
        Rectangle dst{ 26.0f + (g.roomPx - 0.5f) * TILE, playerFoot - 32, 24, 32 };
        DrawEllipse((int)(dst.x + 12), (int)playerFoot - 2, 6.0f, 3.0f, Fade(BLACK, 0.25f));
        DrawTexturePro(hero, src, dst, { 0, 0 }, 0.0f, WHITE);
    }

    // room name plate
    Vector2 sz = measureText(r.title.c_str(), 8);
    drawWindow({ VRES_W * 0.5f - sz.x * 0.5f - 8, 6, sz.x + 16, 20 });
    drawTextCentered(r.title.c_str(), VRES_W * 0.5f, 12, Color{ 255, 236, 176, 255 }, 8);

    // prompt
    const char* prompt = nullptr;
    float dx = 7.5f - g.roomPx, dy = 4.5f - g.roomPy;
    if (dx * dx + dy * dy < 5.0f) prompt = "SPACE: talk";
    else prompt = "SPACE: look   -   walk down through the door to leave";
    Vector2 ps = measureText(prompt, 8);
    drawWindow({ VRES_W * 0.5f - ps.x * 0.5f - 8, VRES_H - 30, ps.x + 16, 20 });
    drawTextCentered(prompt, VRES_W * 0.5f, VRES_H - 24, Color{ 214, 224, 250, 255 }, 8);
    drawToast();
}
