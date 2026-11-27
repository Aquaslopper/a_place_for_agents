// main.cpp — window, virtual 512x288 canvas, main loop, audio streams & transitions
#include "game.hpp"
#include "world.hpp"
#include "battle.hpp"
#include <cstdio>

static void titleUpdate(float dt) {
    g.titleT += dt;
    updateClouds(dt);
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
        g.titleIndex = (g.titleIndex + 2) % 3;
        playSoundIf(g.assets.cursor);
    }
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
        g.titleIndex = (g.titleIndex + 1) % 3;
        playSoundIf(g.assets.cursor);
    }

    auto trigger = [&](int idx) {
        if (idx == 0) {
            playSoundIf(g.assets.confirm);
            startNewAdventure();
        } else if (idx == 1) {
            if (g.saveExists && loadGame()) {
                g.mode = MODE_OVERWORLD;
                g.fade = 1.0f; g.fadeDir = -1;
                overworldSnapCamera();
                playSoundIf(g.assets.confirm);
                syncModeMusic();
            } else {
                playSoundIf(g.assets.cancel);
                pushToast("No save file found.");
            }
        } else if (idx == 2) {
            g.quitRequested = true;
        }
    };

    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
        trigger(g.titleIndex);
        return;
    }
    Vector2 m = getVirtualMouse();
    for (int i = 0; i < 3; i++) {
        Rectangle box{ VRES_W * 0.5f - 78.0f, (float)(146 + i * 24), 156.0f, 19.0f };
        if (CheckCollisionPointRec(m, box)) {
            g.titleIndex = i;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                trigger(i);
                return;
            }
        }
    }
}

static void introUpdate(float dt) {
    g.introT += dt;
    if ((g.introT > 0.6f && (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) ||
                             IsKeyPressed(KEY_ESCAPE) || IsMouseButtonPressed(MOUSE_LEFT_BUTTON)))
        || g.introT > 16.0f) {
        g.mode = MODE_OVERWORLD;
        g.fade = 1.0f; g.fadeDir = -1;
        overworldSnapCamera();
        playSoundIf(g.assets.confirm);
        syncModeMusic();
    }
}

static void gameOverUpdate() {
    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE)
        || IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        returnToTitle();
    }
}

static void updateMusicStreams() {
    if (!g.musicOn) return;
    auto tick = [](Music& m) {
        if (m.frameCount > 0 && IsMusicStreamPlaying(m)) UpdateMusicStream(m);
    };
    tick(g.assets.title);
    tick(g.assets.field);
    tick(g.assets.battle);
    tick(g.assets.victory);
    tick(g.assets.gameover);
}

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1280, 720, "A Place for Agents");
    SetExitKey(0); // ESC opens pause menu / backs out instead of abruptly quitting
    SetTargetFPS(60);

    InitAudioDevice();
    loadAllAssets();
    newGameState(1337u);
    g.mode = MODE_TITLE;
    refreshSaveExists();
    syncModeMusic();

    g.rt = LoadRenderTexture(VRES_W, VRES_H);
    SetTextureFilter(g.rt.texture, TEXTURE_FILTER_POINT);

    Mode prevMode = g.mode;

    while (!WindowShouldClose() && !g.quitRequested) {
        float dt = std::min(0.05f, GetFrameTime());
        g.hotButtons = 0;

        if (IsKeyPressed(KEY_F11)) ToggleFullscreen();

        // Fade transitions run smoothly in every mode
        if (g.fadeDir != 0) {
            g.fade += g.fadeDir * dt * 2.6f;
            if (g.fade <= 0.0f) { g.fade = 0.0f; g.fadeDir = 0; }
            if (g.fade >= 1.0f) { g.fade = 1.0f; g.fadeDir = 0; }
        }

        // Keep monotonic animation clock and toasts alive in modes that don't tick them themselves
        bool selfTicks = (g.mode == MODE_OVERWORLD || g.mode == MODE_ROOM || g.mode == MODE_BATTLE);
        if (!selfTicks) {
            gAnimTime += dt;
            g.totalTime += dt;
            if (g.toastT > 0) g.toastT = std::max(0.0f, g.toastT - dt);
        }

        switch (g.mode) {
            case MODE_TITLE:     titleUpdate(dt); break;
            case MODE_INTRO:     introUpdate(dt); break;
            case MODE_OVERWORLD: overworldUpdate(dt); break;
            case MODE_ROOM:      roomUpdate(dt); break;
            case MODE_DIALOG:    dialogUpdate(dt); break;
            case MODE_MENU:      menuUpdate(dt); break;
            case MODE_BATTLE:    battleUpdate(dt); break;
            case MODE_GAMEOVER:  gameOverUpdate(); break;
        }

        if (g.mode != prevMode) {
            prevMode = g.mode;
            syncModeMusic();
        }
        updateMusicStreams();

        // ---- render to crisp 512x288 virtual framebuffer
        BeginTextureMode(g.rt);
        ClearBackground(Color{ 10, 14, 26, 255 });
        switch (g.mode) {
            case MODE_TITLE:
                drawTitleScreen();
                break;
            case MODE_INTRO:
                drawIntroScreen();
                break;
            case MODE_OVERWORLD:
                drawWorld();
                drawHUD();
                break;
            case MODE_ROOM:
                roomDraw();
                break;
            case MODE_DIALOG:
                if (g.dialogReturn == MODE_ROOM) roomDraw();
                else { drawWorld(); drawHUD(); }
                drawDialogBox();
                break;
            case MODE_MENU:
                if (g.menuReturn == MODE_ROOM) roomDraw();
                else drawWorld();
                drawMenuScreen();
                break;
            case MODE_BATTLE:
                battleDraw();
                break;
            case MODE_GAMEOVER:
                drawGameOverScreen();
                break;
        }
        drawTransitionFx();
        EndTextureMode();

        // ---- present letterboxed integer-scaled canvas
        BeginDrawing();
        ClearBackground(Color{ 6, 8, 16, 255 });
        int sw = GetScreenWidth(), sh = GetScreenHeight();
        float scale = std::min((float)sw / VRES_W, (float)sh / VRES_H);
        if (scale >= 2.0f) scale = floorf(scale);
        float dw = VRES_W * scale, dh = VRES_H * scale;
        float ox = (sw - dw) * 0.5f, oy = (sh - dh) * 0.5f;
        DrawTexturePro(g.rt.texture,
                       { 0, 0, (float)VRES_W, -(float)VRES_H },
                       { ox, oy, dw, dh }, { 0, 0 }, 0.0f, WHITE);
        EndDrawing();
    }

    UnloadRenderTexture(g.rt);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
