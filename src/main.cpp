// main.cpp — window, virtual 512x288 canvas stretched to fill the window,
// main loop, audio streams & transitions
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
        Rectangle box{ VRES_W * 0.5f - 88.0f, (float)(112 + i * 24), 176.0f, 20.0f };
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
    }
}

static void gameOverUpdate() {
    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE)
        || IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        returnToTitle();
    }
}

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1280, 720, "A Place for Agents — an endless SNES-style JRPG");
    SetExitKey(0); // ESC opens pause menu / backs out instead of abruptly quitting
    SetTargetFPS(60);

    InitAudioDevice();
    loadAllAssets();
    newGameState(1337u);
    g.mode = MODE_TITLE;
    refreshSaveExists();

    g.rt = LoadRenderTexture(VRES_W, VRES_H);
    SetTextureFilter(g.rt.texture, TEXTURE_FILTER_POINT);

    while (!WindowShouldClose() && !g.quitRequested) {
        float dt = std::min(0.05f, GetFrameTime());
        // hotButtons is filled by last frame's draw (hover detection) and
        // read by this frame's update, so it is cleared just before drawing.

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

        updateScore(dt);

        g.hotButtons = 0; // fresh hover set for the draw below
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

        // ---- present: stretch the 512x288 canvas to fill the whole window.
        // No letterbox bars — the picture always reaches the window edges,
        // whatever size or aspect the window is. Mouse mapping in
        // getVirtualMouse() uses the same independent X/Y scales.
        BeginDrawing();
        ClearBackground(Color{ 6, 8, 16, 255 });
        int sw = GetScreenWidth(), sh = GetScreenHeight();
        if (sw < 1) sw = VRES_W;
        if (sh < 1) sh = VRES_H;
        DrawTexturePro(g.rt.texture,
                       { 0, 0, (float)VRES_W, -(float)VRES_H },
                       { 0, 0, (float)sw, (float)sh }, { 0, 0 }, 0.0f, WHITE);
        // Optional CRT dressing (Options -> CRT Filter): faint scanlines and
        // a soft vignette, the way the SNES picture sat on a tube TV.
        if (g.crtOn) {
            float lineH = (float)sh / VRES_H; // one virtual scanline in window px
            for (int y = 0; y < VRES_H; y += 2) {
                DrawRectangle(0, (int)(y * lineH), sw, std::max(1, (int)(lineH * 0.45f)),
                              Color{ 0, 0, 0, 26 });
            }
            int vw = std::max(24, sw / 10), vh = std::max(18, sh / 10);
            DrawRectangleGradientV(0, 0, sw, vh, Color{ 0, 0, 0, 52 }, BLANK);
            DrawRectangleGradientV(0, sh - vh, sw, vh, BLANK, Color{ 0, 0, 0, 52 });
            DrawRectangleGradientH(0, 0, vw, sh, Color{ 0, 0, 0, 40 }, BLANK);
            DrawRectangleGradientH(sw - vw, 0, vw, sh, BLANK, Color{ 0, 0, 0, 40 });
        }
        EndDrawing();
    }

    UnloadRenderTexture(g.rt);
    unloadScore();
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
