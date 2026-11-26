// dialog.cpp — dialog boxes, the shop, the inn, the temple and the pause menu
#include "game.hpp"
#include "world.hpp"
#include "battle.hpp"
#include <cstdio>

// ================================================================ dialog
void startDialog(const std::vector<DialogLine>& lines, DialogAction action) {
    g.dialog = lines;
    g.dialogIndex = 0;
    g.dialogReveal = 0;
    g.dialogAction = action;
    g.dialogReturn = (g.mode == MODE_MENU) ? MODE_MENU : (g.mode == MODE_ROOM ? MODE_ROOM : MODE_OVERWORLD);
    g.mode = MODE_DIALOG;
    if (!lines.empty() && lines[0].face >= 0) PlaySound(g.assets.confirm);
}

void resolveDialogAction(DialogAction a) {
    switch (a) {
        case DLG_SHOP:
            g.shopTab = 0; g.shopIndex = 0;
            g.mode = MODE_MENU;
            g.menuPage = 5;                      // shop page
            break;
        case DLG_INN:
            g.mode = MODE_MENU;
            g.menuPage = 6;                      // inn page
            break;
        case DLG_TEMPLE:
            for (auto& h : g.party) {
                h.hp = h.maxHp; h.mp = h.maxMp;
                h.ko = false;
            }
            PlaySound(g.assets.heal);
            pushToast("The Light restores the party.");
            g.mode = g.dialogReturn;
            break;
        case DLG_START_QUEST_SLIMES:
            questSetState(QUEST_SLIMES, QS_ACTIVE);
            g.slimesSlain = 0;
            pushToast("Quest accepted: Turnip Trouble");
            g.mode = g.dialogReturn;
            break;
        case DLG_FINISH_QUEST_SLIMES:
            questSetState(QUEST_SLIMES, QS_DONE);
            g.gil += 40;
            addItem(IT_POTION, 2);
            pushToast("Quest complete: +40 gil");
            PlaySound(g.assets.coin);
            g.mode = g.dialogReturn;
            break;
        case DLG_START_QUEST_LOCKET:
            questSetState(QUEST_LOCKET, QS_ACTIVE);
            pushToast("Quest accepted: The Lost Locket");
            g.mode = g.dialogReturn;
            break;
        case DLG_GIVE_KEY:
            addItem(IT_RUSTY_KEY, 1);
            PlaySound(g.assets.chime);
            g.mode = g.dialogReturn;
            break;
        default:
            g.mode = g.dialogReturn;
            break;
    }
}

void dialogUpdate(float dt) {
    if (g.dialog.empty()) { g.mode = g.dialogReturn; return; }
    const DialogLine& line = g.dialog[std::min<int>(g.dialogIndex, (int)g.dialog.size() - 1)];
    float &rev = g.dialogReveal;
    if (rev < (float)line.text.size()) {
        rev += dt * 46.0f;
        if ((int)rev % 3 == 0) PlaySound(g.assets.cursor);
        if (rev > (float)line.text.size()) rev = (float)line.text.size();
    }
    bool advance = IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) ||
                   IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    if (!advance) return;
    if (rev < (float)line.text.size()) {
        rev = (float)line.text.size();           // first press reveals the rest
        return;
    }
    if (g.dialogIndex + 1 < (int)g.dialog.size()) {
        g.dialogIndex++;
        rev = 0;
        PlaySound(g.assets.cursor);
    } else {
        DialogAction a = g.dialogAction;
        g.dialogAction = DLG_NONE;
        resolveDialogAction(a);
    }
}

// ================================================================ shop
static int shopStock(int* out, int max) {
    static const int stock[] = {
        IT_POTION, IT_HIPOTION, IT_ETHER, IT_HIETHER, IT_ANTIDOTE, IT_PHOENIX,
        IT_SWORD_BRONZE, IT_SWORD_IRON, IT_ARMOR_LEATHER, IT_ARMOR_CHAIN,
        IT_SHIELD_WOOD, IT_SHIELD_IRON, IT_RING_POWER, IT_AMULET_MANA,
    };
    int n = (int)(sizeof(stock) / sizeof(stock[0]));
    for (int i = 0; i < n && i < max; i++) out[i] = stock[i];
    return std::min(n, max);
}

static void equipItemToParty(int item) {
    const ItemDef& d = itemDef(item);
    for (int i = 0; i < 3; i++) {
        Hero& h = g.party[i];
        if (d.kind == IK_WEAPON && h.weapon < 0) { h.weapon = item; return; }
        if (d.kind == IK_ARMOR && h.armor < 0) { h.armor = item; return; }
        if (d.kind == IK_ACCESSORY && h.accessory < 0) { h.accessory = item; return; }
    }
}

static void shopUpdate() {
    int stock[16];
    int n = shopStock(stock, 16);
    Vector2 m = getVirtualMouse();

    // tabs
    if (uiButtonAt("BUY", { 8, 6, 60, 18 }, 8)) g.shopTab = 0;
    if (uiButtonAt("SELL", { 72, 6, 60, 18 }, 8)) g.shopTab = 1;
    if (uiButtonAt("LEAVE", { (float)VRES_W - 68, 6, 60, 18 }, 8)) {
        g.mode = MODE_OVERWORLD;
        return;
    }
    char gb[32];
    snprintf(gb, sizeof(gb), "%d gil", g.gil);
    drawText(gb, VRES_W - 150, 10, Color{ 255, 232, 150, 255 }, 8);

    if (g.shopTab == 0) {
        // buy
        for (int i = 0; i < n; i++) {
            Rectangle r{ 8.0f, 30.0f + i * 17, 236, 16 };
            if (i >= 13) break;
            bool hover = CheckCollisionPointRec(m, r);
            if (hover) g.shopIndex = i;
            if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                const ItemDef& d = itemDef(stock[i]);
                if (g.gil >= d.price) {
                    g.gil -= d.price;
                    addItem(stock[i], 1);
                    equipItemToParty(stock[i]);
                    PlaySound(g.assets.coin);
                } else {
                    pushToast("Not enough gil.");
                    PlaySound(g.assets.cancel);
                }
            }
        }
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) g.shopIndex = (g.shopIndex + 1) % std::min(n, 13);
        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) g.shopIndex = (g.shopIndex + std::min(n, 13) - 1) % std::min(n, 13);
        if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
            const ItemDef& d = itemDef(stock[g.shopIndex]);
            if (g.gil >= d.price) {
                g.gil -= d.price;
                addItem(stock[g.shopIndex], 1);
                equipItemToParty(stock[g.shopIndex]);
                PlaySound(g.assets.coin);
            } else {
                pushToast("Not enough gil.");
                PlaySound(g.assets.cancel);
            }
        }
    } else {
        // sell
        int shown = 0;
        for (int id = 0; id < IT_COUNT && shown < 13; id++) {
            if (itemCount(id) <= 0 || itemDef(id).price <= 0 || itemDef(id).kind == IK_KEY) continue;
            Rectangle r{ 8.0f, 30.0f + shown * 17, 236, 16 };
            if (CheckCollisionPointRec(m, r) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (takeItem(id, 1)) {
                    g.gil += itemDef(id).price / 2;
                    PlaySound(g.assets.coin);
                    pushToast(std::string("Sold ") + itemDef(id).name);
                }
            }
            shown++;
        }
    }
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_TAB)) {
        g.mode = MODE_OVERWORLD;
        PlaySound(g.assets.cancel);
        return;
    }
}

static void drawShop() {
    DrawRectangle(0, 0, VRES_W, VRES_H, Color{ 8, 10, 22, 255 });
    drawWindow({ 4, 4, VRES_W - 8, VRES_H - 8 });
    drawTextCentered("MIRA'S SUNDRIES", VRES_W * 0.5f, 32, Color{ 255, 236, 176, 255 }, 16);
    drawButton("BUY", { 8, 6, 60, 18 }, 8);
    drawButton("SELL", { 72, 6, 60, 18 }, 8);
    drawButton("LEAVE", { (float)VRES_W - 68, 6, 60, 18 }, 8);
    char gb[32];
    snprintf(gb, sizeof(gb), "%d gil", g.gil);
    drawText(gb, VRES_W - 150, 10, Color{ 255, 232, 150, 255 }, 8);

    if (g.shopTab == 0) {
        int stock[16];
        int n = shopStock(stock, 16);
        for (int i = 0; i < n && i < 13; i++) {
            const ItemDef& d = itemDef(stock[i]);
            bool sel = (g.shopIndex == i);
            float y = 56.0f + i * 17;
            if (sel) DrawRectangleRounded({ 8, y - 2, 236, 16 }, 0.3f, 4, Fade(Color{ 70, 90, 180, 255 }, 0.8f));
            drawItemIcon(stock[i], 12, y, 12);
            drawText(d.name, 28, y, sel ? Color{ 255, 236, 176, 255 } : WHITE, 8);
            char pb[32];
            snprintf(pb, sizeof(pb), "%d", d.price);
            drawText(pb, 200, y, g.gil >= d.price ? Color{ 200, 240, 200, 255 } : Color{ 220, 150, 150, 255 }, 8);
        }
        // description panel
        const ItemDef& d = itemDef(stock[std::max(0, std::min(n - 1, g.shopIndex))]);
        drawWindow({ 252, 40, 252, 220 });
        drawText(d.name, 262, 50, Color{ 255, 236, 176, 255 }, 8);
        // wrap the description
        std::string s = d.desc;
        float ty = 68;
        std::string row;
        int used = 0;
        for (char ch : s) {
            if (used >= 40) { drawText(row.c_str(), 262, ty, WHITE, 8); ty += 12; row.clear(); used = 0; }
            row += ch; used++;
        }
        if (!row.empty()) drawText(row.c_str(), 262, ty, WHITE, 8);
        float sy = 150;
        drawText("own front row", 262, sy, Color{ 200, 220, 255, 255 }, 8);
        char buf[64];
        for (int i = 0; i < 3; i++) {
            const Hero& h = g.party[i];
            snprintf(buf, sizeof(buf), "%s L%d  HP %d/%d", heroName(h.klass), h.level, h.hp, h.maxHp);
            drawText(buf, 262, sy + 18 + i * 12, Color{ 220, 228, 250, 255 }, 8);
        }
    } else {
        int shown = 0;
        for (int id = 0; id < IT_COUNT && shown < 13; id++) {
            if (itemCount(id) <= 0 || itemDef(id).price <= 0 || itemDef(id).kind == IK_KEY) continue;
            float y = 56.0f + shown * 17;
            drawItemIcon(id, 12, y, 12);
            char buf[64];
            snprintf(buf, sizeof(buf), "%s x%d", itemDef(id).name, itemCount(id));
            drawText(buf, 28, y, WHITE, 8);
            snprintf(buf, sizeof(buf), "%d", itemDef(id).price / 2);
            drawText(buf, 200, y, Color{ 200, 240, 200, 255 }, 8);
            shown++;
        }
        if (shown == 0) drawText("Nothing to sell.", 14, 60, Color{ 200, 200, 220, 255 }, 8);
    }
    drawText("SPACE: buy / sell   ESC: leave", 12, VRES_H - 16, Color{ 190, 200, 230, 200 }, 8);
}

// ================================================================ inn
static void innScreen() {
    DrawRectangle(0, 0, VRES_W, VRES_H, Color{ 10, 12, 26, 255 });
    drawWindow({ 66, 66, 380, 156 });
    drawTextCentered("THE SLEEPING STAG", VRES_W * 0.5f, 78, Color{ 255, 236, 176, 255 }, 16);
    char buf[64];
    snprintf(buf, sizeof(buf), "A bed for the whole party: %d gil", g.innCost);
    drawTextCentered(buf, VRES_W * 0.5f, 106, WHITE, 8);
    if (uiButtonAt("REST", { 116, 132, 120, 26 }, 8)) {
        if (g.gil >= g.innCost) {
            g.gil -= g.innCost;
            for (auto& h : g.party) { h.hp = h.maxHp; h.mp = h.maxMp; h.ko = false; }
            PlaySound(g.assets.heal);
            pushToast("The party rests well.");
            g.gil = std::max(0, g.gil);
            g.mode = MODE_OVERWORLD;
            g.fade = 0.0f;
            g.runTime += 0.0f;
        } else {
            pushToast("You cannot afford a room.");
            PlaySound(g.assets.cancel);
        }
    }
    if (uiButtonAt("LEAVE", { 276, 132, 120, 26 }, 8)) g.mode = MODE_OVERWORLD;
    drawTextCentered("Bren's blankets smell of woodsmoke.", VRES_W * 0.5f, 186,
                     Color{ 190, 200, 230, 220 }, 8);
}

// ================================================================ menu
static const char* MENU_TABS[5] = { "PARTY", "ITEMS", "EQUIP", "QUESTS", "OPTIONS" };

static void useItemInMenu(int item, int hero) {
    if (!applyConsumable(item, hero)) {
        pushToast("That would have no effect.");
        PlaySound(g.assets.cancel);
        return;
    }
    PlaySound(g.assets.potion);
    pushToast(std::string("Used ") + itemDef(item).name);
}

void menuUpdate(float dt) {
    (void)dt;
    g.totalTime += 0.0f;   // time already advances elsewhere

    // page 5 = shop, 6 = inn
    if (g.menuPage == 5) { shopUpdate(); return; }
    if (g.menuPage == 6) { innScreen(); return; }

    Vector2 m = getVirtualMouse();

    drawWindow({ 4, 4, VRES_W - 8, VRES_H - 8 });
    drawTextCentered("THE ROAD SO FAR", VRES_W * 0.5f, 10, Color{ 255, 236, 176, 255 }, 16);

    // left tab column
    for (int i = 0; i < 5; i++) {
        bool sel = (g.menuPage == i);
        Rectangle r{ 10, 32.0f + i * 18, 96, 16 };
        bool hover = CheckCollisionPointRec(m, r);
        if (sel || hover)
            DrawRectangleRounded(r, 0.3f, 4, Fade(Color{ 70, 90, 180, 255 }, sel ? 0.9f : 0.5f));
        drawText(MENU_TABS[i], r.x + 6, r.y + 4, sel ? Color{ 255, 236, 176, 255 } : WHITE, 8);
        if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            g.menuPage = i; g.menuIndex = 0; PlaySound(g.assets.cursor);
        }
    }

    if (g.menuPage == 0) {
        // party page
        for (int i = 0; i < 3; i++) {
            const Hero& h = g.party[i];
            float y = 36.0f + i * 62;
            drawWindow({ 118, y, VRES_W - 130, 56 });
            Texture2D& f = g.assets.face[i == 0 ? FACE_KNIGHT : (i == 1 ? FACE_MAGE : FACE_CLERIC)];
            if (f.id) DrawTexturePro(f, { 0, 0, (float)f.width, (float)f.height },
                                     { 124, y + 4, 46, 46 }, { 0, 0 }, 0.0f, WHITE);
            char buf[128];
            snprintf(buf, sizeof(buf), "%s  -  %s  Lv %d", heroName(h.klass), klassName(h.klass), h.level);
            drawText(buf, 178, y + 6, Color{ 255, 236, 176, 255 }, 8);
            snprintf(buf, sizeof(buf), "HP %3d/%3d", h.hp, h.maxHp);
            drawText(buf, 178, y + 20, Color{ 180, 240, 190, 255 }, 8);
            drawBar(250, y + 22, 80, 4, (float)h.hp / std::max(1, h.maxHp),
                    Color{ 96, 220, 120, 255 }, Color{ 24, 30, 50, 255 });
            snprintf(buf, sizeof(buf), "MP %3d/%3d", h.mp, h.maxMp);
            drawText(buf, 344, y + 20, Color{ 170, 200, 255, 255 }, 8);
            drawBar(416, y + 22, 60, 4, (float)h.mp / std::max(1, h.maxMp),
                    Color{ 96, 160, 255, 255 }, Color{ 24, 30, 50, 255 });
            snprintf(buf, sizeof(buf), "ATK %d   DEF %d   SPD %d   MAG %d", heroAtk(h), heroDef(h), h.spd, heroMag(h));
            drawText(buf, 178, y + 34, Color{ 210, 220, 245, 255 }, 8);
            snprintf(buf, sizeof(buf), "EXP %d / %d", h.xp, xpToNext(h));
            drawText(buf, 350, y + 34, Color{ 200, 210, 240, 220 }, 8);
            if (h.ko) drawText("K.O.", 560, y + 6, Color{ 255, 120, 120, 255 }, 8);
        }
        char buf[96];
        snprintf(buf, sizeof(buf), "gil %d      time %d:%02d      monsters %d",
                 g.gil, (int)(g.runTime / 60), (int)g.runTime % 60, g.defeatedCount);
        drawText(buf, 120, VRES_H - 28, Color{ 210, 220, 245, 255 }, 8);
        const char* zone = zoneNameAt(g.px, g.py);
        snprintf(buf, sizeof(buf), "at: %s", zone);
        drawText(buf, 120, VRES_H - 16, Color{ 180, 200, 240, 220 }, 8);
    } else if (g.menuPage == 1) {
        // items page
        int shown = 0;
        for (int id = 0; id < IT_COUNT && shown < 12; id++) {
            if (itemCount(id) <= 0) continue;
            float y = 36.0f + shown * 18;
            Rectangle r{ 118, y, 236, 17 };
            bool hover = CheckCollisionPointRec(m, r);
            if (hover) g.menuItem = id;
            if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && itemDef(id).kind == IK_CONSUMABLE) {
                useItemInMenu(id, g.menuUseHero < 0 ? 0 : g.menuUseHero);
                return;
            }
            drawItemIcon(id, 122, y + 1, 13);
            char buf[64];
            snprintf(buf, sizeof(buf), "%s", itemDef(id).name);
            drawText(buf, 142, y + 4, hover ? Color{ 255, 236, 176, 255 } : WHITE, 8);
            snprintf(buf, sizeof(buf), "x%d", itemCount(id));
            drawText(buf, 330, y + 4, Color{ 200, 220, 255, 255 }, 8);
            shown++;
        }
        if (shown == 0) drawText("The packs are empty.", 124, 40, Color{ 200, 200, 220, 255 }, 8);
        // which hero to use it on
        drawWindow({ 362, 32, 142, 118 });
        drawText("use on", 370, 38, Color{ 255, 236, 176, 255 }, 8);
        for (int i = 0; i < 3; i++) {
            Rectangle r{ 368, 54.0f + i * 20, 128, 18 };
            bool hover = CheckCollisionPointRec(m, r);
            if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { g.menuUseHero = i; PlaySound(g.assets.cursor); }
            if (g.menuUseHero == i || (g.menuUseHero < 0 && i == 0))
                DrawRectangleRounded(r, 0.3f, 4, Fade(Color{ 70, 90, 180, 255 }, 0.8f));
            char buf[64];
            const Hero& h = g.party[i];
            snprintf(buf, sizeof(buf), "%s  %d/%d", heroName(h.klass), h.hp, h.maxHp);
            drawText(buf, 374, r.y + 5, WHITE, 8);
        }
        if (g.menuItem >= 0) {
            drawWindow({ 362, 158, 142, 92 });
            drawText(itemDef(g.menuItem).name, 370, 164, Color{ 255, 236, 176, 255 }, 8);
            std::string s = itemDef(g.menuItem).desc;
            float ty = 180;
            std::string row;
            int used = 0;
            for (char ch : s) {
                if (used >= 20) { drawText(row.c_str(), 370, ty, WHITE, 8); ty += 12; row.clear(); used = 0; }
                row += ch; used++;
            }
            if (!row.empty()) drawText(row.c_str(), 370, ty, WHITE, 8);
        }
    } else if (g.menuPage == 2) {
        // equip page
        Hero& h = g.party[g.equipHero];
        for (int i = 0; i < 3; i++) {
            Rectangle r{ 118, 36.0f + i * 20, 120, 18 };
            bool hover = CheckCollisionPointRec(m, r);
            if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { g.equipHero = i; PlaySound(g.assets.cursor); }
            if (g.equipHero == i) DrawRectangleRounded(r, 0.3f, 4, Fade(Color{ 70, 90, 180, 255 }, 0.85f));
            drawText(heroName(g.party[i].klass), 124, r.y + 5, WHITE, 8);
        }
        const char* slots[3] = { "Weapon", "Armour", "Accessory" };
        int* eq[3] = { &h.weapon, &h.armor, &h.accessory };
        for (int s = 0; s < 3; s++) {
            Rectangle r{ 118, 108.0f + s * 20, 236, 18 };
            bool hover = CheckCollisionPointRec(m, r);
            if (hover) g.equipSlot = s;
            if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { g.equipSlot = s; g.equipIndex = 0; }
            if (g.equipSlot == s) DrawRectangleRounded(r, 0.3f, 4, Fade(Color{ 70, 90, 180, 255 }, 0.7f));
            char buf[96];
            snprintf(buf, sizeof(buf), "%s: %s", slots[s], *eq[s] >= 0 ? itemDef(*eq[s]).name : "-");
            drawText(buf, 124, r.y + 5, WHITE, 8);
        }
        // inventory of the matching kind
        int kind = (g.equipSlot == 0) ? IK_WEAPON : (g.equipSlot == 1 ? IK_ARMOR : IK_ACCESSORY);
        int shown = 0;
        for (int id = 0; id < IT_COUNT && shown < 9; id++) {
            if (itemCount(id) <= 0 || itemDef(id).kind != kind) continue;
            Rectangle r{ 362, 36.0f + shown * 18, 138, 17 };
            bool hover = CheckCollisionPointRec(m, r);
            if (hover) g.equipIndex = id;
            if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                *eq[g.equipSlot] = id;
                PlaySound(g.assets.confirm);
            }
            drawItemIcon(id, 366, r.y + 1, 13);
            drawText(itemDef(id).name, 384, r.y + 4, hover ? Color{ 255, 236, 176, 255 } : WHITE, 8);
            shown++;
        }
        // stats summary
        char buf[128];
        snprintf(buf, sizeof(buf), "ATK %d    DEF %d    MAG %d    SPD %d",
                 heroAtk(h), heroDef(h), heroMag(h), h.spd);
        drawText(buf, 120, 178, Color{ 210, 220, 245, 255 }, 8);
        drawText("click a slot, then an item to equip it", 120, 196,
                 Color{ 180, 200, 240, 200 }, 8);
    } else if (g.menuPage == 3) {
        for (int q = 0; q < QUEST_COUNT; q++) {
            float y = 36.0f + q * 54;
            drawWindow({ 118, y, VRES_W - 130, 48 });
            Color title = g.quests[q].state == QS_DONE ? Color{ 150, 200, 150, 255 }
                        : g.quests[q].state == QS_ACTIVE ? Color{ 255, 236, 176, 255 }
                        : Color{ 190, 200, 230, 255 };
            drawText(questTitle(q), 126, y + 6, title, 8);
            std::string goal = questGoalText(q);
            drawText(goal.c_str(), 126, y + 20, Color{ 220, 228, 250, 255 }, 8);
            const char* st = g.quests[q].state == QS_DONE ? "complete"
                           : g.quests[q].state == QS_ACTIVE ? "in progress"
                           : g.quests[q].state == QS_OFFERED ? "not started" : "unknown";
            char buf[96];
            if (q == QUEST_SLIMES && g.quests[q].state == QS_ACTIVE)
                snprintf(buf, sizeof(buf), "%s  (%d of 6 slimes)", st, g.slimesSlain);
            else
                snprintf(buf, sizeof(buf), "%s", st);
            drawText(buf, 126, y + 32, Color{ 190, 210, 250, 220 }, 8);
        }
    } else {
        // options
        float y = 40;
        if (uiButtonAt("SAVE GAME", { 130, y, 140, 22 }, 8)) {
            if (saveGame()) pushToast("Progress saved.");
            else pushToast("Could not write the save file.");
        }
        if (uiButtonAt(g.musicOn ? "MUSIC: ON" : "MUSIC: OFF", { 130, y + 30, 140, 22 }, 8)) {
            g.musicOn = !g.musicOn;
            if (g.musicOn) PlayMusicStream(g.assets.field);
            else StopMusicStream(g.assets.field);
        }
        if (uiButtonAt(g.showHelp ? "HELP: ON" : "HELP: OFF", { 130, y + 60, 140, 22 }, 8))
            g.showHelp = !g.showHelp;
        if (g.showHelp) drawText("movement: arrows / WASD / click", 290, y + 4, Color{ 220, 228, 250, 255 }, 8);
        drawText("interact: SPACE or E", 290, y + 18, Color{ 220, 228, 250, 255 }, 8);
        drawText("pause: ESC or TAB", 290, y + 32, Color{ 220, 228, 250, 255 }, 8);
        drawText("run: hold SHIFT", 290, y + 46, Color{ 220, 228, 250, 255 }, 8);
        drawText("save file: aplaceforagents_save.sav", 130, 200, Color{ 190, 200, 230, 200 }, 8);
    }

    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_TAB)) {
        g.mode = MODE_OVERWORLD;
        g.menuPage = 0;
        PlaySound(g.assets.cancel);
    }
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) {
        g.menuPage = (g.menuPage + 4) % 5; g.menuIndex = 0; PlaySound(g.assets.cursor);
    }
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) {
        g.menuPage = (g.menuPage + 1) % 5; g.menuIndex = 0; PlaySound(g.assets.cursor);
    }
}

void drawMenuScreen() {
    if (g.menuPage == 5) drawShop();
}
