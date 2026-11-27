// dialog.cpp — conversation box, pause menu (Party/Items/Equip/Quests/Options), Shop & Inn
#include "game.hpp"
#include "world.hpp"
#include "battle.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

static inline float uiOx() { return floorf((g.vw - BASE_VW) * 0.5f); }
static inline float uiOy() { return floorf((g.vh - BASE_VH) * 0.5f); }

// ================================================================ dialog
void startDialog(const std::vector<DialogLine>& lines, DialogAction action) {
    if (lines.empty()) return;
    g.dialog = lines;
    g.dialogIndex = 0;
    g.dialogReveal = 0;
    g.dialogLastBlip = -1;
    g.dialogAction = action;
    g.dialogReturn = (g.mode == MODE_DIALOG) ? g.dialogReturn : g.mode;
    g.mode = MODE_DIALOG;
}

void resolveDialogAction(DialogAction a) {
    switch (a) {
        case DLG_SHOP:
            g.menuReturn = g.dialogReturn;
            g.mode = MODE_MENU;
            g.menuPage = 5;
            g.shopTab = 0;
            g.shopIndex = 0;
            g.shopScroll = 0;
            if (g.shopTitle.empty()) g.shopTitle = "Mira's Sundries";
            break;
        case DLG_INN:
            g.menuReturn = g.dialogReturn;
            g.mode = MODE_MENU;
            g.menuPage = 6;
            g.innChoice = 0;
            if (g.innTitle.empty()) g.innTitle = "The Sleeping Stag";
            break;
        case DLG_TEMPLE:
        case DLG_HEAL:
            for (auto& h : g.party) {
                h.hp = h.maxHp; h.mp = h.maxMp;
                h.ko = false; h.poisoned = false;
            }
            playSoundIf(g.assets.heal);
            pushToast("The Light restores the party completely.");
            break;
        case DLG_START_QUEST_SLIMES:
            questSetState(QUEST_SLIMES, QS_ACTIVE);
            g.quests[QUEST_SLIMES].progress = std::min(6, g.slimesSlain);
            playSoundIf(g.assets.confirm);
            pushToast("Quest started: Turnip Trouble");
            break;
        case DLG_FINISH_QUEST_SLIMES:
            questSetState(QUEST_SLIMES, QS_DONE);
            g.quests[QUEST_SLIMES].progress = 6;
            g.gil += 60;
            addItem(IT_POTION, 2, true);
            playSoundIf(g.assets.levelup);
            pushToast("Quest complete! +60 gil, 2x Potion.");
            break;
        case DLG_START_QUEST_LOCKET:
            questSetState(QUEST_LOCKET, QS_ACTIVE);
            g.quests[QUEST_LOCKET].progress = g.locketFound ? 1 : 0;
            playSoundIf(g.assets.confirm);
            pushToast("Quest started: The Lost Locket");
            break;
        case DLG_FINISH_QUEST_LOCKET:
            questSetState(QUEST_LOCKET, QS_DONE);
            g.quests[QUEST_LOCKET].progress = 1;
            takeItem(IT_LOCKET, 1);
            g.gil += 120;
            addItem(IT_AMULET_MANA, 1, true);
            playSoundIf(g.assets.levelup);
            pushToast("Quest complete! +120 gil, Mana Amulet.");
            break;
        case DLG_START_QUEST_CROWN:
            questSetState(QUEST_CROWN, QS_ACTIVE);
            g.quests[QUEST_CROWN].progress = g.bossDefeated ? 1 : 0;
            playSoundIf(g.assets.confirm);
            pushToast("Quest started: The Crown of Elm");
            break;
        case DLG_GIVE_KEY:
            addItem(IT_RUSTY_KEY, 1, true);
            playSoundIf(g.assets.treasure);
            pushToast("Received Rusty Key!");
            break;
        case DLG_BOSS_SLIME_KING: {
            int zl = std::max(4, zoneLevelAt(g.px, g.py));
            std::vector<EnemyKind> group = { EN_SLIME_KING, EN_SLIME, EN_SLIME };
            battleStart(group, zl, 0, true);
            break;
        }
        case DLG_CROWN_REWARD:
            questSetState(QUEST_CROWN, QS_DONE);
            g.quests[QUEST_CROWN].progress = 1;
            g.gil += 500;
            addItem(IT_ARMOR_DRAGON, 1, true);
            playSoundIf(g.assets.levelup);
            pushToast("Elmswick saved! +500 gil, Dragon Scale!");
            break;
        case DLG_SAVE_TIP:
            pushToast("Open Menu (ESC) -> Options to save anytime.");
            break;
        default: break;
    }
}

void dialogUpdate(float dt) {
    if (g.dialog.empty()) { g.mode = g.dialogReturn; return; }
    const DialogLine& line = g.dialog[std::min<int>(g.dialogIndex, (int)g.dialog.size() - 1)];
    g.dialogReveal += dt * 44.0f;
    int chars = (int)g.dialogReveal;
    if (chars < (int)line.text.size() && (chars / 3) != g.dialogLastBlip) {
        g.dialogLastBlip = chars / 3;
        playSoundIf(g.assets.cursor);
    }

    bool advance = IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_E)
                || IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    bool skip = IsKeyPressed(KEY_ESCAPE);

    if (skip) {
        g.mode = g.dialogReturn;
        DialogAction act = g.dialogAction;
        g.dialogAction = DLG_NONE;
        g.dialog.clear();
        resolveDialogAction(act);
        return;
    }
    if (advance) {
        if (g.dialogReveal < (float)line.text.size()) {
            g.dialogReveal = (float)line.text.size();
        } else {
            g.dialogIndex++;
            g.dialogReveal = 0;
            g.dialogLastBlip = -1;
            if (g.dialogIndex >= (int)g.dialog.size()) {
                g.mode = g.dialogReturn;
                DialogAction act = g.dialogAction;
                g.dialogAction = DLG_NONE;
                g.dialog.clear();
                resolveDialogAction(act);
            }
        }
    }
}

// ================================================================ inventory/shop helpers
static std::vector<int> ownedItems() {
    std::vector<int> out;
    for (int i = 0; i < IT_COUNT; i++)
        if (g.inventory[i] > 0) out.push_back(i);
    return out;
}

static std::vector<int> shopStock() {
    std::vector<int> stock = {
        IT_POTION, IT_HIPOTION, IT_ETHER, IT_ANTIDOTE, IT_PHOENIX,
        IT_SWORD_BRONZE, IT_SWORD_IRON,
        IT_ARMOR_LEATHER, IT_ARMOR_CHAIN,
        IT_SHIELD_WOOD, IT_SHIELD_IRON,
        IT_RING_POWER, IT_AMULET_MANA
    };
    int zl = zoneLevelAt(g.px, g.py);
    if (zl >= 3 || g.bossDefeated) {
        stock.push_back(IT_HIETHER);
        stock.push_back(IT_ELIXIR);
        stock.push_back(IT_SWORD_MYTHRIL);
        stock.push_back(IT_ARMOR_MYTHRIL);
    }
    return stock;
}

static std::vector<int> equippableForSlot(int slot) {
    ItemKind want = (slot == 0) ? IK_WEAPON : (slot == 1 ? IK_ARMOR : IK_ACCESSORY);
    std::vector<int> out;
    for (int i = 0; i < IT_COUNT; i++) {
        if (g.inventory[i] > 0 && itemDef(i).kind == want) out.push_back(i);
    }
    return out;
}

static const int SHOP_VISIBLE = 9;
static const float SHOP_ROW_Y0 = 56.0f;
static const float SHOP_ROW_H  = 17.0f;
static inline Rectangle innBtnStay()  { return { uiOx() + 146.0f, uiOy() + 180.0f, 100.0f, 24.0f }; }
static inline Rectangle innBtnLeave() { return { uiOx() + 266.0f, uiOy() + 180.0f, 100.0f, 24.0f }; }

static void clampShopScroll(int count) {
    if (count <= 0) { g.shopIndex = 0; g.shopScroll = 0; return; }
    if (g.shopIndex < 0) g.shopIndex = count - 1;
    if (g.shopIndex >= count) g.shopIndex = 0;
    if (g.shopIndex < g.shopScroll) g.shopScroll = g.shopIndex;
    if (g.shopIndex >= g.shopScroll + SHOP_VISIBLE) g.shopScroll = g.shopIndex - SHOP_VISIBLE + 1;
    int maxScroll = std::max(0, count - SHOP_VISIBLE);
    g.shopScroll = std::max(0, std::min(maxScroll, g.shopScroll));
}

static void buyShopItem(int id) {
    const ItemDef& d = itemDef(id);
    if (g.gil >= d.price) {
        g.gil -= d.price;
        addItem(id, 1, true);
        playSoundIf(g.assets.coin);
        pushToast(std::string("Bought ") + d.name + "!");
    } else {
        playSoundIf(g.assets.cancel);
        pushToast("Not enough gil!");
    }
}

static void sellOwnedItem(int id) {
    const ItemDef& d = itemDef(id);
    if (!d.sells || d.kind == IK_KEY || d.price <= 0) {
        playSoundIf(g.assets.cancel);
        pushToast("Can't sell that item.");
        return;
    }
    int sell = std::max(1, d.price / 2);
    if (takeItem(id, 1)) {
        g.gil += sell;
        playSoundIf(g.assets.coin);
        char buf[80];
        snprintf(buf, sizeof(buf), "Sold %s for %d gil.", d.name, sell);
        pushToast(buf);
    }
}

static void innRest() {
    if (g.gil >= g.innCost) {
        g.gil -= g.innCost;
        for (auto& h : g.party) {
            h.hp = h.maxHp; h.mp = h.maxMp;
            h.ko = false; h.poisoned = false;
        }
        g.fade = 1.0f; g.fadeDir = -1;
        playSoundIf(g.assets.levelup);
        pushToast("Rested soundly. Full HP & MP restored!");
        g.mode = g.menuReturn;
        g.menuPage = 0;
    } else {
        playSoundIf(g.assets.cancel);
        pushToast("Not enough gil for a room.");
    }
}

// ================================================================ shop & inn updates
static void shopUpdateInternal() {
    float ox = uiOx(), oy = uiOy();
    auto stock = shopStock();
    auto owned = ownedItems();
    int count = (g.shopTab == 0) ? (int)stock.size() : (int)owned.size();
    clampShopScroll(count);

    if (IsKeyPressed(KEY_ESCAPE)) {
        g.mode = g.menuReturn; g.menuPage = 0;
        playSoundIf(g.assets.cancel);
        return;
    }
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_TAB)) {
        g.shopTab = 1 - g.shopTab;
        g.shopIndex = 0; g.shopScroll = 0;
        playSoundIf(g.assets.cursor);
        return;
    }
    if (count > 0) {
        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
            g.shopIndex = (g.shopIndex + count - 1) % count;
            clampShopScroll(count);
            playSoundIf(g.assets.cursor);
        }
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
            g.shopIndex = (g.shopIndex + 1) % count;
            clampShopScroll(count);
            playSoundIf(g.assets.cursor);
        }
        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f && count > SHOP_VISIBLE) {
            g.shopScroll = std::max(0, std::min(count - SHOP_VISIBLE, g.shopScroll - (int)wheel));
        }
        if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
            if (g.shopTab == 0) buyShopItem(stock[g.shopIndex]);
            else {
                sellOwnedItem(owned[g.shopIndex]);
                clampShopScroll((int)ownedItems().size());
            }
        }
    }

    Vector2 m = getVirtualMouse();
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (CheckCollisionPointRec(m, { ox + 26, oy + 30, 80, 18 })) {
            g.shopTab = 0; g.shopIndex = 0; g.shopScroll = 0; playSoundIf(g.assets.cursor);
        } else if (CheckCollisionPointRec(m, { ox + 112, oy + 30, 80, 18 })) {
            g.shopTab = 1; g.shopIndex = 0; g.shopScroll = 0; playSoundIf(g.assets.cursor);
        } else if (CheckCollisionPointRec(m, { ox + BASE_VW - 88.0f, oy + 30, 62, 18 })) {
            g.mode = g.menuReturn; g.menuPage = 0; playSoundIf(g.assets.cancel);
        } else {
            for (int row = 0; row < SHOP_VISIBLE; row++) {
                int idx = g.shopScroll + row;
                if (idx >= count) break;
                Rectangle rr{ ox + 24, oy + SHOP_ROW_Y0 + row * SHOP_ROW_H, BASE_VW - 48.0f, SHOP_ROW_H - 1.0f };
                if (CheckCollisionPointRec(m, rr)) {
                    g.shopIndex = idx;
                    if (g.shopTab == 0) buyShopItem(stock[idx]);
                    else {
                        sellOwnedItem(owned[idx]);
                        clampShopScroll((int)ownedItems().size());
                    }
                    break;
                }
            }
        }
    }
}

static void innUpdateInternal() {
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_A) || IsKeyPressed(KEY_D)) {
        g.innChoice = 1 - g.innChoice;
        playSoundIf(g.assets.cursor);
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        g.mode = g.menuReturn; g.menuPage = 0;
        playSoundIf(g.assets.cancel);
        return;
    }
    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
        if (g.innChoice == 0) innRest();
        else { g.mode = g.menuReturn; g.menuPage = 0; playSoundIf(g.assets.cancel); }
        return;
    }
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        Vector2 m = getVirtualMouse();
        if (CheckCollisionPointRec(m, innBtnStay())) { g.innChoice = 0; innRest(); }
        else if (CheckCollisionPointRec(m, innBtnLeave())) { g.mode = g.menuReturn; g.menuPage = 0; playSoundIf(g.assets.cancel); }
    }
}

// ================================================================ pause menu update
static bool gShowControlsModal = false;

static Rectangle trackRow(int i) {
    return { uiOx() + 35.f + (i / 7) * 224.f, uiOy() + 57.f + (i % 7) * 21.f, 214.f, 18.f };
}
static inline Rectangle soundBackRect() { return { uiOx() + 382.f, uiOy() + 238.f, 89.f, 21.f }; }

static void soundRoomUpdate() {
    Vector2 m = getVirtualMouse();
    bool click = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_TAB) || (click && CheckCollisionPointRec(m, soundBackRect()))) {
        g.menuPage = 4; playSoundIf(g.assets.cancel); return;
    }
    int sel = g.audition;
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) sel = (sel + MUS_COUNT - 1) % MUS_COUNT;
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) sel = (sel + 1) % MUS_COUNT;
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_A) || IsKeyPressed(KEY_D))
        sel = sel >= 7 ? sel - 7 : std::min(sel + 7, (int)MUS_COUNT - 1);
    if (click) for (int i = 0; i < MUS_COUNT; ++i) if (CheckCollisionPointRec(m, trackRow(i))) sel = i;
    if (sel != g.audition) { g.audition = (MusicCue)sel; playSoundIf(g.assets.cursor); }
}

static void drawSoundRoom() {
    float ox = uiOx(), oy = uiOy();
    DrawRectangle(0, 0, g.vw, g.vh, Fade(BLACK, 0.76f));
    drawWindow({ ox + 23, oy + 15, 466, 256 });
    drawText("SOUND ROOM", ox + 36, oy + 27, Color{ 255, 231, 166, 255 }, 16);
    drawText("OpenRTP / 13 cues", ox + 301, oy + 31, Color{ 190, 205, 229, 255 }, 8);
    for (int i = 0; i < MUS_COUNT; ++i) {
        Rectangle r = trackRow(i);
        bool sel = g.audition == i;
        char n[4]; snprintf(n, sizeof(n), "%02d", i + 1);
        DrawRectangleRec(r, sel ? Color{ 64, 80, 122, 255 } : Color{ 26, 37, 74, 230 });
        if (sel) DrawRectangle((int)r.x, (int)r.y, 2, (int)r.height, Color{ 245, 210, 136, 255 });
        drawText(n, r.x + 7, r.y + 5, Color{ 205, 186, 142, 255 }, 8);
        drawText(cueTitle((MusicCue)i), r.x + 32, r.y + 5, sel ? WHITE : Color{ 195, 208, 232, 255 }, 8);
    }
    Music m = g.assets.score[g.audition];
    bool valid = IsMusicValid(m) && m.stream.buffer;
    float time = valid ? GetMusicTimePlayed(m) : 0.f, len = valid ? GetMusicTimeLength(m) : 0.f;
    char info[96];
    snprintf(info, sizeof(info), "%s  %d:%02d / %d:%02d", cueTitle(g.audition),
             (int)time / 60, (int)time % 60, (int)len / 60, (int)len % 60);
    drawText(!valid ? "No audio device or stream" : g.musicOn ? info : "Music is off in Options",
             ox + 37, oy + 212, WHITE, 8);
    drawBar(ox + 37, oy + 226, 432, 4, len > 0 ? time / len : 0, Color{ 223, 189, 123, 255 }, Color{ 38, 51, 84, 255 });
    drawText("Arrows or click to listen", ox + 37, oy + 245, Color{ 185, 201, 224, 255 }, 8);
    drawButton("Back", soundBackRect(), 8, false);
}

void menuUpdate(float dt) {
    (void)dt;
    float ox = uiOx(), oy = uiOy();
    if (g.menuPage == 7) { soundRoomUpdate(); return; }
    if (g.menuPage == 5) { shopUpdateInternal(); return; }
    if (g.menuPage == 6) { innUpdateInternal(); return; }

    if (gShowControlsModal) {
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)
            || IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            gShowControlsModal = false;
            playSoundIf(g.assets.cancel);
        }
        return;
    }

    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_TAB)) {
        if (g.menuPage == 1 && g.menuItem >= 0) {
            g.menuItem = -1;
            playSoundIf(g.assets.cancel);
        } else if (g.menuPage == 2 && g.equipIndex >= 0) {
            g.equipIndex = -1;
            playSoundIf(g.assets.cancel);
        } else {
            g.mode = g.menuReturn;
            playSoundIf(g.assets.cancel);
        }
        return;
    }

    bool inSub = (g.menuPage == 1 && g.menuItem >= 0) || (g.menuPage == 2 && g.equipIndex >= 0);
    if (!inSub) {
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_Q)) {
            g.menuPage = (g.menuPage + 4) % 5;
            g.menuIndex = 0; g.menuItem = -1; g.equipIndex = -1;
            playSoundIf(g.assets.cursor);
            return;
        }
        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_E)) {
            g.menuPage = (g.menuPage + 1) % 5;
            g.menuIndex = 0; g.menuItem = -1; g.equipIndex = -1;
            playSoundIf(g.assets.cursor);
            return;
        }
    }

    Vector2 m = getVirtualMouse();
    bool clicked = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

    if (clicked) {
        for (int i = 0; i < 5; i++) {
            if (CheckCollisionPointRec(m, { ox + 16, oy + 42.0f + i * 26.0f, 84, 22 })) {
                g.menuPage = i;
                g.menuIndex = 0; g.menuItem = -1; g.equipIndex = -1;
                playSoundIf(g.assets.cursor);
                return;
            }
        }
    }

    // ---- Page 0: Party
    if (g.menuPage == 0) {
        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) { g.menuIndex = (g.menuIndex + 2) % 3; playSoundIf(g.assets.cursor); }
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) { g.menuIndex = (g.menuIndex + 1) % 3; playSoundIf(g.assets.cursor); }
        if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
            g.party[g.menuIndex].row = 1 - g.party[g.menuIndex].row;
            playSoundIf(g.assets.confirm);
        }
        if (clicked) {
            for (int i = 0; i < 3; i++) {
                if (CheckCollisionPointRec(m, { ox + 118, oy + 20.0f + i * 78.0f, 376, 72 })) {
                    g.menuIndex = i;
                    g.party[i].row = 1 - g.party[i].row;
                    playSoundIf(g.assets.confirm);
                    break;
                }
            }
        }
        return;
    }

    // ---- Page 1: Items
    if (g.menuPage == 1) {
        auto items = ownedItems();
        int n = (int)items.size();
        if (n == 0) { g.menuIndex = 0; g.menuItem = -1; return; }
        if (g.menuIndex >= n) g.menuIndex = n - 1;

        if (g.menuItem < 0) {
            if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) { g.menuIndex = (g.menuIndex + n - 1) % n; playSoundIf(g.assets.cursor); }
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) { g.menuIndex = (g.menuIndex + 1) % n; playSoundIf(g.assets.cursor); }
            if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
                int id = items[g.menuIndex];
                const ItemDef& d = itemDef(id);
                if (d.kind == IK_CONSUMABLE) { g.menuItem = id; g.menuUseHero = 0; playSoundIf(g.assets.confirm); }
                else if (d.kind == IK_KEY) pushToast(d.desc);
                else pushToast("Equip gear in the Equip tab.");
            }
            if (clicked) {
                int scroll = std::max(0, g.menuIndex - 8);
                for (int row = 0; row < 10 && scroll + row < n; row++) {
                    if (CheckCollisionPointRec(m, { ox + 122, oy + 42.0f + row * 17.0f, 366, 16 })) {
                        g.menuIndex = scroll + row;
                        int id = items[g.menuIndex];
                        const ItemDef& d = itemDef(id);
                        if (d.kind == IK_CONSUMABLE) { g.menuItem = id; g.menuUseHero = 0; playSoundIf(g.assets.confirm); }
                        else pushToast(d.kind == IK_KEY ? d.desc : "Equip gear in the Equip tab.");
                        break;
                    }
                }
            }
        } else {
            if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W) || IsKeyPressed(KEY_LEFT)) {
                g.menuUseHero = (g.menuUseHero + 2) % 3; playSoundIf(g.assets.cursor);
            }
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S) || IsKeyPressed(KEY_RIGHT)) {
                g.menuUseHero = (g.menuUseHero + 1) % 3; playSoundIf(g.assets.cursor);
            }
            if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
                if (applyConsumable(g.menuItem, g.menuUseHero)) {
                    playSoundIf(g.assets.heal);
                    if (itemCount(g.menuItem) <= 0) g.menuItem = -1;
                } else {
                    playSoundIf(g.assets.cancel);
                    pushToast("No effect on that hero.");
                }
            }
            if (clicked) {
                for (int i = 0; i < 3; i++) {
                    if (CheckCollisionPointRec(m, { ox + 110, oy + 90.0f + i * 32.0f, 292, 26 })) {
                        g.menuUseHero = i;
                        if (applyConsumable(g.menuItem, i)) {
                            playSoundIf(g.assets.heal);
                            if (itemCount(g.menuItem) <= 0) g.menuItem = -1;
                        } else {
                            playSoundIf(g.assets.cancel);
                            pushToast("No effect on that hero.");
                        }
                        return;
                    }
                }
                g.menuItem = -1;
            }
        }
        return;
    }

    // ---- Page 2: Equip
    if (g.menuPage == 2) {
        g.equipHero = std::max(0, std::min(2, g.equipHero));
        g.equipSlot = std::max(0, std::min(2, g.equipSlot));

        if (g.equipIndex < 0) {
            if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) { g.equipSlot = (g.equipSlot + 2) % 3; playSoundIf(g.assets.cursor); }
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) { g.equipSlot = (g.equipSlot + 1) % 3; playSoundIf(g.assets.cursor); }
            if (IsKeyPressed(KEY_A)) { g.equipHero = (g.equipHero + 2) % 3; playSoundIf(g.assets.cursor); }
            if (IsKeyPressed(KEY_D)) { g.equipHero = (g.equipHero + 1) % 3; playSoundIf(g.assets.cursor); }
            if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
                g.equipIndex = 0;
                playSoundIf(g.assets.confirm);
            }
            if (clicked) {
                for (int i = 0; i < 3; i++) {
                    if (CheckCollisionPointRec(m, { ox + 122.0f + i * 122.0f, oy + 24, 116, 22 })) {
                        g.equipHero = i; playSoundIf(g.assets.cursor); return;
                    }
                }
                for (int s = 0; s < 3; s++) {
                    if (CheckCollisionPointRec(m, { ox + 122, oy + 74.0f + s * 28.0f, 186, 24 })) {
                        g.equipSlot = s; g.equipIndex = 0; playSoundIf(g.assets.confirm); return;
                    }
                }
            }
        } else {
            auto cands = equippableForSlot(g.equipSlot);
            int total = (int)cands.size() + 1;
            if (g.equipIndex >= total) g.equipIndex = 0;
            if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) { g.equipIndex = (g.equipIndex + total - 1) % total; playSoundIf(g.assets.cursor); }
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) { g.equipIndex = (g.equipIndex + 1) % total; playSoundIf(g.assets.cursor); }
            if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
                if (g.equipIndex < (int)cands.size()) {
                    equipHeroItem(g.equipHero, g.equipSlot, cands[g.equipIndex]);
                    playSoundIf(g.assets.confirm);
                } else {
                    unequipHeroSlot(g.equipHero, g.equipSlot);
                    playSoundIf(g.assets.cancel);
                }
                g.equipIndex = -1;
            }
            if (clicked) {
                for (int i = 0; i < total && i < 8; i++) {
                    if (CheckCollisionPointRec(m, { ox + 316, oy + 74.0f + i * 20.0f, 174, 18 })) {
                        if (i < (int)cands.size()) {
                            equipHeroItem(g.equipHero, g.equipSlot, cands[i]);
                            playSoundIf(g.assets.confirm);
                        } else {
                            unequipHeroSlot(g.equipHero, g.equipSlot);
                            playSoundIf(g.assets.cancel);
                        }
                        g.equipIndex = -1;
                        return;
                    }
                }
                g.equipIndex = -1;
            }
        }
        return;
    }

    // ---- Page 3: Quests
    if (g.menuPage == 3) {
        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) { g.menuIndex = (g.menuIndex + QUEST_COUNT - 1) % QUEST_COUNT; playSoundIf(g.assets.cursor); }
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) { g.menuIndex = (g.menuIndex + 1) % QUEST_COUNT; playSoundIf(g.assets.cursor); }
        return;
    }

    // ---- Page 4: Options & Save/Load
    if (g.menuPage == 4) {
        const int NOPT = 9;
        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) { g.optionsIndex = (g.optionsIndex + NOPT - 1) % NOPT; playSoundIf(g.assets.cursor); }
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) { g.optionsIndex = (g.optionsIndex + 1) % NOPT; playSoundIf(g.assets.cursor); }
        auto act = [&](int idx) {
            if (idx == 0) {
                if (saveGame()) { playSoundIf(g.assets.levelup); pushToast("Progress saved!"); }
                else { playSoundIf(g.assets.cancel); pushToast("Could not write save file."); }
            } else if (idx == 1) {
                if (loadGame()) {
                    playSoundIf(g.assets.confirm);
                    pushToast("Loaded saved adventure!");
                    g.mode = MODE_OVERWORLD;
                    overworldSnapCamera();
                } else {
                    playSoundIf(g.assets.cancel);
                    pushToast("No valid save file found.");
                }
            } else if (idx == 2) {
                g.musicOn = !g.musicOn;
                playSoundIf(g.assets.confirm);
            } else if (idx == 3) {
                g.waitBattle = !g.waitBattle;
                playSoundIf(g.assets.confirm);
            } else if (idx == 4) {
                toggleFullscreenClean();
                playSoundIf(g.assets.confirm);
            } else if (idx == 5) {
                g.crtOn = !g.crtOn;
                playSoundIf(g.assets.confirm);
            } else if (idx == 6) {
                gShowControlsModal = true;
                playSoundIf(g.assets.confirm);
            } else if (idx == 7) {
                g.audition = playingCue() == MUS_NONE ? MUS_VILLAGE : playingCue();
                g.menuPage = 7; playSoundIf(g.assets.confirm);
            } else if (idx == 8) {
                returnToTitle();
            }
        };
        if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) act(g.optionsIndex);
        if (clicked) {
            for (int i = 0; i < NOPT; i++) {
                if (CheckCollisionPointRec(m, { ox + 126, oy + 40.0f + i * 24.0f, 360, 21 })) {
                    g.optionsIndex = i; act(i); break;
                }
            }
        }
    }
}

// ================================================================ shop / inn / pause menu render
static void drawShopPage() {
    float ox = uiOx(), oy = uiOy();
    auto stock = shopStock();
    auto owned = ownedItems();
    int count = (g.shopTab == 0) ? (int)stock.size() : (int)owned.size();
    clampShopScroll(count);

    DrawRectangle(0, 0, g.vw, g.vh, Fade(Color{ 8, 12, 26, 255 }, 0.85f));
    drawWindow({ ox + 14, oy + 10, BASE_VW - 28.0f, BASE_VH - 20.0f });

    const char* title = g.shopTitle.empty() ? "Mira's Sundries" : g.shopTitle.c_str();
    drawText(title, ox + 26, oy + 17, Color{ 255, 232, 150, 255 }, 8);
    char gbuf[48]; snprintf(gbuf, sizeof(gbuf), "Gil: %d", g.gil);
    Vector2 gsz = measureText(gbuf, 8);
    drawText(gbuf, ox + BASE_VW - 26.0f - gsz.x, oy + 17, Color{ 255, 232, 150, 255 }, 8);

    auto drawTab = [&](Rectangle r, const char* label, bool active) {
        DrawRectangleRounded(r, 0.35f, 4, active ? Color{ 62, 92, 170, 255 } : Color{ 26, 36, 78, 255 });
        DrawRectangleRoundedLinesEx(r, 0.35f, 4, 1.0f,
                                    active ? Color{ 255, 232, 150, 255 } : Color{ 90, 110, 160, 255 });
        drawTextCentered(label, r.x + r.width * 0.5f, r.y + 5,
                         active ? Color{ 255, 240, 180, 255 } : Color{ 185, 200, 235, 255 }, 8);
    };
    drawTab({ ox + 26, oy + 30, 80, 18 }, "BUY",  g.shopTab == 0);
    drawTab({ ox + 112, oy + 30, 80, 18 }, "SELL", g.shopTab == 1);
    drawTab({ ox + BASE_VW - 88.0f, oy + 30, 62, 18 }, "EXIT", false);

    if (count == 0) {
        drawText("Your bag is empty.", ox + 34, oy + 80, Color{ 180, 190, 220, 255 }, 8);
    } else {
        for (int row = 0; row < SHOP_VISIBLE; row++) {
            int idx = g.shopScroll + row;
            if (idx >= count) break;
            float y = oy + SHOP_ROW_Y0 + row * SHOP_ROW_H;
            bool sel = (idx == g.shopIndex);
            if (sel) {
                DrawRectangle((int)ox + 24, (int)y, BASE_VW - 48, (int)SHOP_ROW_H - 1, Color{ 52, 76, 152, 230 });
                DrawRectangleLines((int)ox + 24, (int)y, BASE_VW - 48, (int)SHOP_ROW_H - 1, Color{ 255, 232, 150, 200 });
            }
            int id = (g.shopTab == 0) ? stock[idx] : owned[idx];
            const ItemDef& d = itemDef(id);
            if (sel) drawCursor(ox + 27, y + 4);
            drawItemIcon(id, ox + 38, y + 1, 14);
            drawText(d.name, ox + 56, y + 4, WHITE, 8);

            if (g.shopTab == 0) {
                int own = itemCount(id);
                if (own > 0) {
                    char obuf[24]; snprintf(obuf, sizeof(obuf), "Own:%d", own);
                    drawText(obuf, ox + 272, y + 4, Color{ 165, 210, 255, 255 }, 8);
                }
                char pbuf[24]; snprintf(pbuf, sizeof(pbuf), "%d g", d.price);
                Color pc = (g.gil >= d.price) ? Color{ 255, 232, 150, 255 } : Color{ 220, 110, 110, 255 };
                Vector2 psz = measureText(pbuf, 8);
                drawText(pbuf, ox + BASE_VW - 32.0f - psz.x, y + 4, pc, 8);
            } else {
                char qbuf[24]; snprintf(qbuf, sizeof(qbuf), "x%d", itemCount(id));
                drawText(qbuf, ox + 276, y + 4, Color{ 180, 200, 240, 255 }, 8);
                if (!d.sells || d.kind == IK_KEY || d.price <= 0) {
                    Vector2 ksz = measureText("Key item", 8);
                    drawText("Key item", ox + BASE_VW - 32.0f - ksz.x, y + 4, Color{ 160, 170, 200, 255 }, 8);
                } else {
                    char sbuf[24]; snprintf(sbuf, sizeof(sbuf), "+%d g", std::max(1, d.price / 2));
                    Vector2 ssz = measureText(sbuf, 8);
                    drawText(sbuf, ox + BASE_VW - 32.0f - ssz.x, y + 4, Color{ 150, 240, 170, 255 }, 8);
                }
            }
        }
        if (count > SHOP_VISIBLE) {
            char sc[48];
            snprintf(sc, sizeof(sc), "%d-%d / %d", g.shopScroll + 1,
                     std::min(count, g.shopScroll + SHOP_VISIBLE), count);
            Vector2 ssz = measureText(sc, 8);
            drawText(sc, ox + BASE_VW - 32.0f - ssz.x, oy + 212, Color{ 170, 185, 225, 255 }, 8);
        }
    }

    DrawLine((int)ox + 24, (int)oy + 224, (int)ox + BASE_VW - 24, (int)oy + 224, Color{ 90, 110, 160, 180 });
    if (count > 0 && g.shopIndex >= 0 && g.shopIndex < count) {
        int id = (g.shopTab == 0) ? stock[g.shopIndex] : owned[g.shopIndex];
        drawTextWrapped(itemDef(id).desc, ox + 28, oy + 232, BASE_VW - 56.0f, 11, Color{ 225, 235, 255, 255 }, 8);
    }
    drawText("Arrows: select  L/R: tab  SPACE: trade  ESC: exit",
             ox + 28, oy + BASE_VH - 22.0f, Color{ 170, 185, 225, 230 }, 8);
    drawToast();
}

static void drawInnPage() {
    float ox = uiOx(), oy = uiOy();
    DrawRectangle(0, 0, g.vw, g.vh, Fade(Color{ 8, 12, 26, 255 }, 0.82f));
    drawWindow({ ox + 66, oy + 46, 380, 174 });
    const char* title = g.innTitle.empty() ? "The Sleeping Stag" : g.innTitle.c_str();
    drawTextCentered(title, g.vw * 0.5f, oy + 58, Color{ 255, 232, 150, 255 }, 8);

    char buf[96];
    snprintf(buf, sizeof(buf), "Rest until dawn for %d gil?  (Gil: %d)", g.innCost, g.gil);
    drawTextCentered(buf, g.vw * 0.5f, oy + 78, WHITE, 8);
    drawTextCentered("Restores all HP, MP, KO, and Poison.", g.vw * 0.5f, oy + 94, Color{ 185, 205, 240, 255 }, 8);

    // 3 spacious hero status cards (no collision at any HP digit count!)
    for (int i = 0; i < 3; i++) {
        const Hero& h = g.party[i];
        Rectangle card{ ox + 80.0f + i * 120.0f, oy + 114.0f, 112.0f, 52.0f };
        DrawRectangleRounded(card, 0.2f, 4, Color{ 20, 30, 68, 225 });
        DrawRectangleRoundedLinesEx(card, 0.2f, 4, 1.0f, Color{ 84, 104, 162, 200 });
        drawTextCentered(heroName(h.klass), card.x + card.width * 0.5f, card.y + 7,
                         h.ko ? Color{ 240, 130, 130, 255 } : Color{ 255, 236, 176, 255 }, 8);
        char hp[32];
        snprintf(hp, sizeof(hp), "HP %d/%d", h.hp, h.maxHp);
        drawTextCentered(hp, card.x + card.width * 0.5f, card.y + 22,
                         h.ko ? Color{ 240, 120, 120, 255 } : Color{ 155, 235, 175, 255 }, 8);
        char mp[32];
        snprintf(mp, sizeof(mp), "MP %d/%d", h.mp, h.maxMp);
        drawTextCentered(mp, card.x + card.width * 0.5f, card.y + 36, Color{ 165, 205, 255, 255 }, 8);
    }

    auto drawBtn = [&](Rectangle r, const char* label, bool sel, bool dis) {
        DrawRectangleRounded(r, 0.35f, 4, sel ? Color{ 56, 86, 164, 255 } : Color{ 26, 36, 78, 255 });
        DrawRectangleRoundedLinesEx(r, 0.35f, 4, 1.2f,
                                    sel ? Color{ 255, 232, 150, 255 } : Color{ 95, 115, 165, 255 });
        Color c = dis ? Color{ 160, 120, 120, 255 }
                      : (sel ? Color{ 255, 236, 176, 255 } : WHITE);
        drawTextCentered(label, r.x + r.width * 0.5f, r.y + 8, c, 8);
    };
    drawBtn(innBtnStay(),  "STAY",  g.innChoice == 0, g.gil < g.innCost);
    drawBtn(innBtnLeave(), "LEAVE", g.innChoice == 1, false);
    drawToast();
}

static void drawHeroPicker(const char* title, int sel) {
    float ox = uiOx(), oy = uiOy();
    DrawRectangle(0, 0, g.vw, g.vh, Fade(BLACK, 0.52f));
    drawWindow({ ox + 96, oy + 62, 320, 156 });
    drawTextCentered(title, g.vw * 0.5f, oy + 72, Color{ 255, 232, 150, 255 }, 8);
    for (int i = 0; i < 3; i++) {
        const Hero& h = g.party[i];
        Rectangle r{ ox + 110, oy + 90.0f + i * 32.0f, 292, 26 };
        bool s = (i == sel);
        DrawRectangleRounded(r, 0.3f, 4, s ? Color{ 56, 86, 164, 255 } : Color{ 24, 34, 72, 255 });
        DrawRectangleRoundedLinesEx(r, 0.3f, 4, 1.0f,
                                    s ? Color{ 255, 232, 150, 255 } : Color{ 90, 110, 160, 255 });
        if (s) drawCursor(r.x + 5, r.y + 9);
        char buf[96];
        snprintf(buf, sizeof(buf), "%-6s HP %d/%d  MP %d/%d%s",
                 heroName(h.klass), h.hp, h.maxHp, h.mp, h.maxMp,
                 h.ko ? " KO" : (h.poisoned ? " PSN" : ""));
        drawText(buf, r.x + 15, r.y + 9, h.ko ? Color{ 240, 120, 120, 255 } : WHITE, 8);
    }
    drawTextCentered("SPACE/Click: confirm  |  ESC: cancel", g.vw * 0.5f, oy + 196, Color{ 180, 195, 230, 255 }, 8);
}

void drawMenuScreen() {
    if (g.menuPage == 7) { drawSoundRoom(); return; }
    if (g.menuPage == 5) { drawShopPage(); return; }
    if (g.menuPage == 6) { drawInnPage(); return; }

    float ox = uiOx(), oy = uiOy();
    DrawRectangle(0, 0, g.vw, g.vh, Fade(Color{ 8, 12, 26, 255 }, 0.85f));
    drawWindow({ ox + 10, oy + 10, 96, BASE_VH - 20.0f });
    drawTextCentered("MENU", ox + 58, oy + 19, Color{ 255, 232, 150, 255 }, 8);

    const char* tabs[5] = { "Party", "Items", "Equip", "Quests", "Options" };
    for (int i = 0; i < 5; i++) {
        bool sel = (i == g.menuPage);
        Rectangle r{ ox + 16, oy + 42.0f + i * 26.0f, 84, 22 };
        if (sel) {
            DrawRectangleRounded(r, 0.35f, 4, Color{ 56, 86, 164, 255 });
            DrawRectangleRoundedLinesEx(r, 0.35f, 4, 1.0f, Color{ 255, 232, 150, 255 });
        }
        drawText(tabs[i], ox + 24, r.y + 7, sel ? Color{ 255, 236, 176, 255 } : Color{ 215, 225, 248, 255 }, 8);
    }
    char gbuf[32]; snprintf(gbuf, sizeof(gbuf), "%d g", g.gil);
    drawText("Gil", ox + 20, oy + BASE_VH - 46.0f, Color{ 170, 185, 225, 255 }, 8);
    drawText(gbuf, ox + 20, oy + BASE_VH - 34.0f, Color{ 255, 232, 150, 255 }, 8);

    drawWindow({ ox + 110, oy + 10, BASE_VW - 120.0f, BASE_VH - 20.0f });

    // ---- Page 0: Party
    if (g.menuPage == 0) {
        for (int i = 0; i < 3; i++) {
            const Hero& h = g.party[i];
            float y = oy + 20.0f + i * 78.0f;
            bool sel = (i == g.menuIndex);
            DrawRectangleRounded({ ox + 118, y, 376, 72 }, 0.16f, 4,
                                 sel ? Color{ 34, 52, 114, 245 } : Color{ 22, 32, 74, 215 });
            DrawRectangleRoundedLinesEx({ ox + 118, y, 376, 72 }, 0.16f, 4, 1.0f,
                                        sel ? Color{ 255, 232, 150, 255 } : Color{ 80, 100, 155, 220 });
            Texture2D& f = g.assets.face[i == 0 ? FACE_KNIGHT : (i == 1 ? FACE_MAGE : FACE_CLERIC)];
            if (f.id) DrawTexturePro(f, { 0, 0, (float)f.width, (float)f.height },
                                     { ox + 126, y + 12, 48, 48 }, { 0, 0 }, 0.0f,
                                     h.ko ? Color{ 130, 130, 160, 255 } : WHITE);
            char line[96];
            snprintf(line, sizeof(line), "%s (%s) Lv.%d", heroName(h.klass), klassName(h.klass), h.level);
            drawText(line, ox + 182, y + 8, Color{ 255, 236, 176, 255 }, 8);

            const char* rowLabel = (h.row == 0) ? "[FRONT]" : "[BACK]";
            float rw = measureText(rowLabel, 8).x;
            drawText(rowLabel, ox + 486.0f - rw, y + 8,
                     h.row == 0 ? Color{ 150, 235, 175, 255 } : Color{ 160, 200, 255, 255 }, 8);

            if (h.ko) {
                float kw = measureText("K.O.", 8).x;
                drawText("K.O.", ox + 486.0f - kw, y + 23, Color{ 255, 110, 110, 255 }, 8);
            } else if (h.poisoned) {
                float pw = measureText("PSN", 8).x;
                drawText("PSN", ox + 486.0f - pw, y + 23, Color{ 195, 130, 255, 255 }, 8);
            }

            snprintf(line, sizeof(line), "HP %d/%d", h.hp, h.maxHp);
            drawText(line, ox + 182, y + 23, WHITE, 8);
            drawBar(ox + 282, y + 25, 88, 6, (float)h.hp / std::max(1, h.maxHp),
                    Color{ 96, 220, 120, 255 }, Color{ 24, 34, 70, 255 });

            snprintf(line, sizeof(line), "MP %d/%d", h.mp, h.maxMp);
            drawText(line, ox + 182, y + 38, Color{ 180, 210, 255, 255 }, 8);
            drawBar(ox + 282, y + 40, 88, 6, (float)h.mp / std::max(1, h.maxMp),
                    Color{ 96, 160, 255, 255 }, Color{ 24, 34, 70, 255 });

            snprintf(line, sizeof(line), "XP %d/%d", h.xp, xpToNext(h));
            float xw = measureText(line, 8).x;
            drawText(line, ox + 486.0f - xw, y + 38, Color{ 215, 224, 248, 255 }, 8);

            snprintf(line, sizeof(line), "ATK %d   DEF %d   MAG %d   SPD %d",
                     heroAtk(h), heroDef(h), heroMag(h), h.spd);
            drawText(line, ox + 182, y + 54, Color{ 195, 210, 240, 255 }, 8);
        }
        drawText("SPACE/Click: switch row  |  ESC: close",
                 ox + 122, oy + BASE_VH - 22.0f, Color{ 170, 185, 225, 230 }, 8);
    }

    // ---- Page 1: Items
    else if (g.menuPage == 1) {
        drawText("BAG / ITEMS", ox + 124, oy + 20, Color{ 255, 232, 150, 255 }, 8);
        auto items = ownedItems();
        int n = (int)items.size();
        if (n == 0) {
            drawText("Your bag is empty.", ox + 130, oy + 60, Color{ 180, 190, 220, 255 }, 8);
        } else {
            int scroll = std::max(0, g.menuIndex - 8);
            for (int row = 0; row < 10 && scroll + row < n; row++) {
                int idx = scroll + row;
                int id = items[idx];
                float y = oy + 42.0f + row * 17.0f;
                bool sel = (idx == g.menuIndex);
                if (sel) {
                    DrawRectangle((int)ox + 122, (int)y, 366, 16, Color{ 52, 76, 152, 230 });
                    DrawRectangleLines((int)ox + 122, (int)y, 366, 16, Color{ 255, 232, 150, 200 });
                    drawCursor(ox + 125, y + 4);
                }
                const ItemDef& d = itemDef(id);
                drawItemIcon(id, ox + 135, y + 1, 14);
                drawText(d.name, ox + 153, y + 4, WHITE, 8);
                const char* tag = (d.kind == IK_CONSUMABLE) ? "Use"
                                : (d.kind == IK_KEY ? "Key" : "Gear");
                drawText(tag, ox + 388, y + 4, Color{ 165, 195, 235, 255 }, 8);
                char qbuf[16]; snprintf(qbuf, sizeof(qbuf), "x%d", itemCount(id));
                float qw = measureText(qbuf, 8).x;
                drawText(qbuf, ox + 482.0f - qw, y + 4, Color{ 255, 232, 150, 255 }, 8);
            }
            DrawLine((int)ox + 122, (int)oy + 220, (int)ox + 488, (int)oy + 220, Color{ 90, 110, 160, 180 });
            if (g.menuIndex >= 0 && g.menuIndex < n)
                drawTextWrapped(itemDef(items[g.menuIndex]).desc, ox + 124, oy + 228, 362, 11, Color{ 225, 235, 255, 255 }, 8);
        }
        drawText("SPACE/Click: use item  |  L/R: tab",
                 ox + 122, oy + BASE_VH - 22.0f, Color{ 170, 185, 225, 230 }, 8);
        if (g.menuItem >= 0) {
            std::string t = std::string("Use ") + itemDef(g.menuItem).name + " on:";
            drawHeroPicker(t.c_str(), g.menuUseHero);
        }
    }

    // ---- Page 2: Equip
    else if (g.menuPage == 2) {
        g.equipHero = std::max(0, std::min(2, g.equipHero));
        g.equipSlot = std::max(0, std::min(2, g.equipSlot));
        for (int i = 0; i < 3; i++) {
            Rectangle r{ ox + 122.0f + i * 122.0f, oy + 24, 116, 22 };
            bool sel = (i == g.equipHero);
            DrawRectangleRounded(r, 0.35f, 4, sel ? Color{ 56, 86, 164, 255 } : Color{ 26, 36, 78, 255 });
            DrawRectangleRoundedLinesEx(r, 0.35f, 4, 1.0f,
                                        sel ? Color{ 255, 232, 150, 255 } : Color{ 90, 110, 160, 255 });
            drawTextCentered(heroName(g.party[i].klass), r.x + r.width * 0.5f, r.y + 7,
                             sel ? Color{ 255, 236, 176, 255 } : WHITE, 8);
        }
        const Hero& h = g.party[g.equipHero];
        char stats[96];
        snprintf(stats, sizeof(stats), "ATK %d   DEF %d   MAG %d   SPD %d",
                 heroAtk(h), heroDef(h), heroMag(h), h.spd);
        drawText(stats, ox + 124, oy + 54, Color{ 165, 235, 180, 255 }, 8);

        const char* sname[3] = { "Weapon", "Armor", "Accessory" };
        int cur[3] = { h.weapon, h.armor, h.accessory };
        for (int s = 0; s < 3; s++) {
            Rectangle r{ ox + 122, oy + 74.0f + s * 28.0f, 186, 24 };
            bool sel = (g.equipSlot == s);
            DrawRectangleRounded(r, 0.25f, 4, sel ? Color{ 50, 74, 146, 240 } : Color{ 24, 34, 72, 220 });
            DrawRectangleRoundedLinesEx(r, 0.25f, 4, 1.0f,
                                        sel ? Color{ 255, 232, 150, 255 } : Color{ 85, 105, 155, 200 });
            drawText(sname[s], r.x + 6, r.y + 3, Color{ 180, 200, 235, 255 }, 8);
            const char* eqName = (cur[s] >= 0) ? itemDef(cur[s]).name : "- None -";
            drawText(eqName, r.x + 6, r.y + 13, WHITE, 8);
        }

        auto cands = equippableForSlot(g.equipSlot);
        drawText(g.equipIndex >= 0 ? "Choose gear:" : "Available in bag:", ox + 318, oy + 60, Color{ 255, 232, 150, 255 }, 8);
        int total = (int)cands.size() + 1;
        for (int i = 0; i < total && i < 8; i++) {
            Rectangle r{ ox + 316, oy + 74.0f + i * 20.0f, 174, 18 };
            bool sel = (g.equipIndex >= 0 && i == g.equipIndex);
            if (sel) {
                DrawRectangleRounded(r, 0.25f, 4, Color{ 56, 86, 164, 255 });
                DrawRectangleRoundedLinesEx(r, 0.25f, 4, 1.0f, Color{ 255, 232, 150, 255 });
            }
            if (i < (int)cands.size()) {
                drawItemIcon(cands[i], r.x + 4, r.y + 2, 14);
                drawText(itemDef(cands[i]).name, r.x + 22, r.y + 5, WHITE, 8);
            } else {
                drawText("[Unequip]", r.x + 8, r.y + 5, Color{ 225, 165, 165, 255 }, 8);
            }
        }
        drawText("A/D: hero  |  UP/DN: slot  |  SPACE: equip",
                 ox + 122, oy + BASE_VH - 22.0f, Color{ 170, 185, 225, 230 }, 8);
    }

    // ---- Page 3: Quests
    else if (g.menuPage == 3) {
        drawText("JOURNAL / QUESTS", ox + 124, oy + 20, Color{ 255, 232, 150, 255 }, 8);
        for (int q = 0; q < QUEST_COUNT; q++) {
            float y = oy + 40.0f + q * 61.0f;
            bool sel = (q == g.menuIndex);
            DrawRectangleRounded({ ox + 122, y, 366, 56 }, 0.16f, 4,
                                 sel ? Color{ 36, 54, 116, 240 } : Color{ 22, 32, 74, 210 });
            DrawRectangleRoundedLinesEx({ ox + 122, y, 366, 56 }, 0.16f, 4, 1.0f,
                                        sel ? Color{ 255, 232, 150, 255 } : Color{ 80, 100, 155, 200 });
            int st = g.quests[q].state;
            const char* badge = (st == QS_DONE) ? "COMPLETE"
                              : (st == QS_ACTIVE ? "ACTIVE" : "AVAILABLE");
            Color bc = (st == QS_DONE) ? Color{ 120, 240, 150, 255 }
                     : (st == QS_ACTIVE ? Color{ 255, 225, 110, 255 } : Color{ 175, 190, 220, 255 });
            drawText(questTitle(q), ox + 130, y + 6, WHITE, 8);
            Vector2 bsz = measureText(badge, 8);
            drawText(badge, ox + 480 - bsz.x, y + 6, bc, 8);

            drawTextWrapped(questGoalText(q), ox + 130, y + 19, 348, 10, Color{ 200, 212, 240, 255 }, 8);

            std::string prog;
            if (q == QUEST_SLIMES && st == QS_ACTIVE)
                prog = "Progress: " + std::to_string(std::min(6, g.slimesSlain)) + " / 6 slimes defeated";
            else if (q == QUEST_LOCKET && st == QS_ACTIVE)
                prog = g.locketFound ? "Locket found! Return to Tamsin."
                                     : "Standing Stones: " + compassHint(g.px, g.py, LOCKET_PICKUP_X, LOCKET_PICKUP_Y);
            else if (q == QUEST_CROWN && st == QS_ACTIVE)
                prog = (itemCount(IT_CROWN) > 0 || g.bossDefeated)
                     ? "Crown reclaimed! Return to Old Maren."
                     : "Castle of Elm: " + compassHint(g.px, g.py, (float)BOSS_CASTLE_TX, (float)BOSS_CASTLE_TY);
            if (!prog.empty()) drawText(prog.c_str(), ox + 130, y + 43, Color{ 155, 235, 185, 255 }, 8);
        }
        char stat[96];
        int mins = (int)(g.runTime / 60.0f), secs = (int)g.runTime % 60;
        snprintf(stat, sizeof(stat), "Time %02d:%02d  |  Foes: %d  |  Chests: %d",
                 mins, secs, g.defeatedCount, (int)g.openedChests.size() + (g.towerChestOpened ? 1 : 0));
        drawText(stat, ox + 124, oy + 240, Color{ 185, 200, 235, 255 }, 8);
    }

    // ---- Page 4: Options & Save/Load
    else if (g.menuPage == 4) {
        drawText("OPTIONS & SAVE", ox + 124, oy + 20, Color{ 255, 232, 150, 255 }, 8);
        const char* rows[9] = {
            "Save Adventure to Disk",
            g.saveExists ? "Load Saved Adventure" : "Load Save (none yet)",
            g.musicOn ? "Music: ON" : "Music: OFF",
            g.waitBattle ? "Battle ATB: WAIT (menu pause)" : "Battle ATB: ACTIVE (real-time)",
            "Toggle Fullscreen (F11 / Alt+Enter)",
            g.crtOn ? "Soft CRT: ON (scanlines)" : "Soft CRT: OFF (crisp pixels)",
            "Controls & Field Tips",
            "Sound Room (13 Tracks)",
            "Return to Title Screen",
        };
        for (int i = 0; i < 9; i++) {
            Rectangle r{ ox + 126, oy + 40.0f + i * 24.0f, 360, 21 };
            bool sel = (i == g.optionsIndex);
            DrawRectangleRounded(r, 0.3f, 4, sel ? Color{ 56, 86, 164, 255 } : Color{ 24, 34, 72, 220 });
            DrawRectangleRoundedLinesEx(r, 0.3f, 4, 1.0f,
                                        sel ? Color{ 255, 232, 150, 255 } : Color{ 85, 105, 155, 200 });
            Color c = (i == 8) ? Color{ 255, 175, 175, 255 }
                               : (sel ? Color{ 255, 236, 176, 255 } : WHITE);
            drawText(rows[i], r.x + 14, r.y + 6, c, 8);
            if (sel) drawCursor(r.x + 4, r.y + 6);
        }
        if (gShowControlsModal) {
            DrawRectangle(0, 0, g.vw, g.vh, Fade(BLACK, 0.65f));
            drawWindow({ ox + 22, oy + 34, 468, 218 });
            drawTextCentered("CONTROLS & FIELD TIPS", g.vw * 0.5f, oy + 46, Color{ 255, 232, 150, 255 }, 8);
            const char* help[] = {
                "WASD / Arrows / Click : Walk (hold Shift to run)",
                "SPACE / E / Enter     : Talk, chests, doors",
                "ESC / TAB             : Open / close Pause Menu",
                "In Battle             : Arrows or Click commands",
                "F11 / Alt+Enter       : Toggle Fullscreen",
                "",
                "Adventure Tips:",
                "- Meet Hedrick, Tamsin, Old Maren & Odo in town.",
                "- Back-row heroes take half melee damage.",
                "- Follow the North Road to the Castle of Elm.",
            };
            for (int i = 0; i < 10; i++)
                drawText(help[i], ox + 38.0f, oy + 68.0f + i * 14.5f, Color{ 225, 235, 255, 255 }, 8);
            drawTextCentered("SPACE or CLICK to close", g.vw * 0.5f, oy + 232, Color{ 255, 232, 150, 255 }, 8);
        }
    }

    drawToast();
}
