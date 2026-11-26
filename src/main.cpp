// main.cpp — window, virtual framebuffer, mode loop
#include "game.hpp"
#include "battle.hpp"
#include "world.hpp"
#include <cstdio>
#include <cstring>

static const char* TITLE_QUIT = "QUIT";

static void updateMusic() {
    if (!g.musicOn) return;
    switch (g.mode) {
        case MODE_TITLE:     UpdateMusicStream(g.assets.title); break;
        case MODE_BATTLE:    UpdateMusicStream(g.assets.battle);
                             UpdateMusicStream(g.assets.victory); break;
        case MODE_GAMEOVER:  UpdateMusicStream(g.assets.gameover); break;
        case MODE_INTRO:     UpdateMusicStream(g.assets.title); break;
        default:             UpdateMusicStream(g.assets.field); break;
    }
}

// ================================================================ title input
static void titleUpdate(float dt) {
    g.titleT += dt;
    g.totalTime += dt;
    // drift the clouds behind the logo
    for (int i = 0; i < 6; i++) {
        if (g.cloudScale[i] == 0) {
            g.cloudScale[i] = 0.5f + hashf(i, 3, 11u) * 0.7f;
            g.cloudX[i] = hashf(i, 5, 13u) * (VRES_W + 200) - 100;
            g.cloudY[i] = hashf(i, 7, 17u) * 120 - 10;
        }
        g.cloudX[i] += dt * (4.0f + i * 1.4f);
        if (g.cloudX[i] > VRES_W + 140) g.cloudX[i] = -140;
    }

    Vector2 m = getVirtualMouse();
    for (int i = 0; i < 3; i++) {
        Rectangle r{ VRES_W * 0.5f - 75, 147.0f + i * 24, 150, 18 };
        if (CheckCollisionPointRec(m, r)) {
            if (g.titleIndex != i && !(i == 1 && !saveExists())) PlaySound(g.assets.cursor);
            if (!(i == 1 && !saveExists())) g.titleIndex = i;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !(i == 1 && !saveExists())) {
                PlaySound(g.assets.confirm);
                if (i == 0) { startNewAdventure(); return; }
                if (i == 1) {
                    if (loadGame()) {
                        g.mode = MODE_OVERWORLD;
                        g.fade = 1.0f;
                        g.fadeDir = -1;
                        overworldSnapCamera();
                        PlaySound(g.assets.confirm);
                        if (g.musicOn) { StopMusicStream(g.assets.title); PlayMusicStream(g.assets.field); }
                    } else {
                        pushToast("That save looks damaged.");
                    }
                    return;
                }
                if (i == 2) g.fadeToTitle = true;
            }
        }
    }
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
        int tries = 0;
        do { g.titleIndex = (g.titleIndex + 1) % 3; tries++; }
        while (tries < 3 && g.titleIndex == 1 && !saveExists());
        PlaySound(g.assets.cursor);
    }
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
        int tries = 0;
        do { g.titleIndex = (g.titleIndex + 2) % 3; tries++; }
        while (tries < 3 && g.titleIndex == 1 && !saveExists());
        PlaySound(g.assets.cursor);
    }
    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
        if (g.titleIndex == 0) { startNewAdventure(); return; }
        if (g.titleIndex == 2) { g.fadeToTitle = true; return; }
        if (loadGame()) {
            g.mode = MODE_OVERWORLD;
            g.fade = 1.0f; g.fadeDir = -1;
            overworldSnapCamera();
            if (g.musicOn) { StopMusicStream(g.assets.title); PlayMusicStream(g.assets.field); }
        } else {
            pushToast("No save to continue.");
        }
    }
    if (IsKeyPressed(KEY_ESCAPE)) g.fadeToTitle = true;
    (void)TITLE_QUIT;
}

// ================================================================ intro
static void introUpdate(float dt) {
    g.introT += dt;
    g.totalTime += dt;
    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) || IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        g.mode = MODE_OVERWORLD;
        overworldSnapCamera();
        g.fade = 0.0f;
        villageLoadAt(g.px, g.py);
        npcsLoadFor(g.px, g.py, true);
        if (!g.startVillageSeen) {
            g.startVillageSeen = true;
            std::vector<DialogLine> lines;
            lines.push_back({ FACE_KNIGHT, "Aldric", "Elmswick. I haven't seen the old well since I was a boy." });
            lines.push_back({ FACE_MAGE, "Iris", "The road south is clear. For now. Keep your sword loose." });
            lines.push_back({ FACE_CLERIC, "Pip", "Ease up, both of you. Somebody here might need a hand first." });
            lines.push_back({ -1, "", "Talk to the townsfolk with SPACE, open the menu with ESC,\nclick to walk, and step south out of the gate when you're ready." });
            startDialog(lines);
        }
        if (g.musicOn) {
            StopMusicStream(g.assets.title);
            PlayMusicStream(g.assets.field);
        }
    }
}

// ================================================================ gameover
static void gameoverUpdate(float dt) {
    g.totalTime += dt;
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && false) return;
    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) ||
        IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        StopMusicStream(g.assets.gameover);
        g.mode = MODE_TITLE;
        g.fade = 0.0f;
        if (g.musicOn) PlayMusicStream(g.assets.title);
    }
}

// ================================================================ main
int main(void) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(VRES_W * 2, VRES_H * 2, "A Place for Agents");
    InitAudioDevice();
    SetExitKey(0);
    SetTargetFPS(60);

    g.rt = LoadRenderTexture(VRES_W, VRES_H);
    SetTextureFilter(g.rt.texture, TEXTURE_FILTER_POINT);

    if (!loadAllAssets()) {
        TraceLog(LOG_ERROR, "assets failed to load");
    }
    newGameState(20251126u);
    g.mode = MODE_TITLE;
    if (g.musicOn && g.assets.title.frameCount > 0) PlayMusicStream(g.assets.title);

    bool running = true;
    while (running && !WindowShouldClose()) {
        float dt = GetFrameTime();
        if (dt > 0.1f) dt = 0.1f;      // never let a hitch teleport the hero
        g.hotButtons = 0;
        if (IsKeyPressed(KEY_F11)) ToggleFullscreen();
        if (g.debugMode && IsKeyPressed(KEY_F3)) g.showHelp = !g.showHelp;

        switch (g.mode) {
            case MODE_TITLE:     titleUpdate(dt); break;
            case MODE_INTRO:     introUpdate(dt); break;
            case MODE_OVERWORLD: overworldUpdate(dt); break;
            case MODE_BATTLE:    battleUpdate(dt); break;
            case MODE_MENU:      menuUpdate(dt); break;
            case MODE_DIALOG:    dialogUpdate(dt); break;
            case MODE_ROOM:      roomUpdate(dt); break;
            case MODE_GAMEOVER:  gameoverUpdate(dt); break;
        }

        // fade handling
        if (g.fadeDir < 0) {
            g.fade = std::max(0.0f, g.fade - dt * 1.6f);
            if (g.fade <= 0.0f) g.fadeDir = 0;
        } else if (g.fadeDir > 0) {
            g.fade = std::min(1.0f, g.fade + dt * 1.6f);
        }

        if (g.fadeToTitle) {
            g.fadeDir = 1;
            if (g.fade >= 1.0f) {
                running = false;
                break;
            }
        }

        updateMusic();

        // ---------------- draw into the virtual framebuffer
        BeginTextureMode(g.rt);
        ClearBackground(BLACK);
        switch (g.mode) {
            case MODE_TITLE:     drawTitleScreen(); break;
            case MODE_INTRO:     drawIntroScreen(); break;
            case MODE_OVERWORLD: drawWorld(); drawHUD(); break;
            case MODE_BATTLE:    battleDraw(); break;
            case MODE_MENU:      drawMenuScreen(); break;
            case MODE_DIALOG:
                // keep the world visible behind the box
                if (g.dialogReturn == MODE_ROOM) { roomDraw(); }
                else { drawWorld(); drawHUD(); }
                drawDialogBox();
                break;
            case MODE_ROOM:      roomDraw(); drawToast(); break;
            case MODE_GAMEOVER:  drawGameOverScreen(); break;
        }
        drawTransitionFx();
        EndTextureMode();

        // ---------------- present, letterboxed and integer-scaled
        BeginDrawing();
        ClearBackground(BLACK);
        float scale = std::min((float)GetScreenWidth() / VRES_W, (float)GetScreenHeight() / VRES_H);
        if (scale >= 2.0f) scale = floorf(scale);     // crisp pixels when there is room
        float dw = VRES_W * scale, dh = VRES_H * scale;
        float ox = (GetScreenWidth() - dw) * 0.5f, oy = (GetScreenHeight() - dh) * 0.5f;
        DrawTexturePro(g.rt.texture, { 0, 0, (float)g.rt.texture.width, -(float)g.rt.texture.height },
                       { ox, oy, dw, dh }, { 0, 0 }, 0.0f, WHITE);
        EndDrawing();
    }

    UnloadRenderTexture(g.rt);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
