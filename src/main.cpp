// main.cpp — window, virtual 512x288 canvas, pixel-perfect presentation,
// main loop, audio streams & transitions
#include "game.hpp"
#include "world.hpp"
#include "battle.hpp"
#include <cstdio>

// The Windows build is a GUI-subsystem exe (WIN32 in CMake, so no console
// window), but the game still uses the plain main() entry point. MSVC's
// windows subsystem expects WinMain by default; point the linker at the
// console-style startup so main() is used. (MinGW's -mwindows already
// provides a WinMain that calls main(), so no pragma is needed there.)
#if defined(_MSC_VER)
#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
#endif

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

Rectangle getPresentationRect() {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    if (sw < 1) sw = VRES_W;
    if (sh < 1) sh = VRES_H;
    float scale = std::min((float)sw / VRES_W, (float)sh / VRES_H);
    // Keep every source pixel an even-sized block on normal/large displays.
    // Below 2x, fractional scaling is preferable to leaving a tiny 1x picture.
    if (scale >= 2.0f) scale = floorf(scale);
    float w = VRES_W * scale, h = VRES_H * scale;
    return { floorf((sw - w) * 0.5f), floorf((sh - h) * 0.5f), w, h };
}

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1024, 576, "A Place for Agents — an endless field assignment");
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

        // ---- present without stretching the art or changing its aspect.
        // Integer enlargement keeps chunky pixels even; narrow windows fall
        // back to a smooth fractional scale rather than shrinking to 1x.
        BeginDrawing();
        ClearBackground(Color{ 6, 8, 16, 255 });
        Rectangle dst = getPresentationRect();
        DrawTexturePro(g.rt.texture,
                       { 0, 0, (float)VRES_W, -(float)VRES_H },
                       dst, { 0, 0 }, 0.0f, WHITE);

        // The CRT option is deliberately restrained: one very faint beam
        // shadow per source row, plus a soft edge falloff. It is aligned to
        // the scaled image (not the desktop), so resize/fullscreen cannot
        // turn it into thick, uneven bars. Clean pixel art is the default.
        float pixelScale = dst.height / VRES_H;
        if (g.crtOn && pixelScale >= 1.25f) {
            float bandH = std::max(1.0f, pixelScale * 0.26f);
            for (int y = 0; y < VRES_H; ++y) {
                float bandY = dst.y + (y + 0.72f) * pixelScale;
                DrawRectangleRec({ dst.x, bandY, dst.width, bandH }, Color{ 0, 0, 0, 15 });
            }
            float vw = std::max(12.0f, dst.width * 0.055f);
            float vh = std::max(10.0f, dst.height * 0.055f);
            int x = (int)dst.x, y = (int)dst.y;
            int w = (int)dst.width, h = (int)dst.height;
            DrawRectangleGradientV(x, y, w, (int)vh, Color{ 0, 0, 0, 20 }, BLANK);
            DrawRectangleGradientV(x, y + h - (int)vh, w, (int)vh, BLANK, Color{ 0, 0, 0, 20 });
            DrawRectangleGradientH(x, y, (int)vw, h, Color{ 0, 0, 0, 16 }, BLANK);
            DrawRectangleGradientH(x + w - (int)vw, y, (int)vw, h, BLANK, Color{ 0, 0, 0, 16 });
        }
        EndDrawing();
    }

    UnloadRenderTexture(g.rt);
    unloadScore();
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
