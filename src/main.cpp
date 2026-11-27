// main.cpp — window, dynamic aspect-matched virtual canvas, sharp scaling,
// main loop, audio streams & transitions
#include "game.hpp"
#include "world.hpp"
#include "regions.hpp"
#include "battle.hpp"
#include "cinema.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

// The Windows build is a GUI-subsystem exe (WIN32 in CMake, so no console
// window), but the game still uses the plain main() entry point. MSVC's
// windows subsystem expects WinMain by default; point the linker at the
// console-style startup so main() is used. (MinGW's -mwindows already
// provides a WinMain that calls main(), so no pragma is needed there.)
#if defined(_MSC_VER)
#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
#endif

static void syncVirtualCanvas() {
    int sw = std::max(1, GetScreenWidth());
    int sh = std::max(1, GetScreenHeight());
    float aspect = std::clamp((float)sw / (float)sh, 1.20f, 2.60f);
    float baseAspect = (float)BASE_VW / (float)BASE_VH;

    int targetVw = BASE_VW;
    int targetVh = BASE_VH;
    if (aspect >= baseAspect) {
        targetVh = BASE_VH;
        targetVw = std::max(BASE_VW, ((int)std::lround(BASE_VH * aspect) + 1) & ~1);
    } else {
        targetVw = BASE_VW;
        targetVh = std::max(BASE_VH, ((int)std::lround(BASE_VW / aspect) + 1) & ~1);
    }

    if (g.rt.id == 0 || targetVw != g.vw || targetVh != g.vh) {
        if (g.rt.id != 0) UnloadRenderTexture(g.rt);
        g.vw = targetVw;
        g.vh = targetVh;
        g.rt = LoadRenderTexture(g.vw, g.vh);
        SetTextureFilter(g.rt.texture, TEXTURE_FILTER_POINT);
    }
}

void toggleFullscreenClean() {
    ToggleBorderlessWindowed();
    g.borderlessFs = IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE) || IsWindowFullscreen();
    syncVirtualCanvas();
}

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
    float menuTop = std::floor(g.vh * 0.5f - 28.0f);
    for (int i = 0; i < 3; i++) {
        Rectangle box{ std::floor(g.vw * 0.5f - 88.0f), menuTop + i * 24.0f, 176.0f, 20.0f };
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
    if ((g.introT > 0.9f && (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) ||
                             IsKeyPressed(KEY_ESCAPE) || IsMouseButtonPressed(MOUSE_LEFT_BUTTON)))
        || g.introT > 16.0f) {
        // Not a cut: the palette comes up in steps while the camera pulls back
        // from the north gate and glides in to the party, and the village names
        // itself once the shot settles. This is the game's first impression, so
        // it is staged like an opening shot rather than a fade to gameplay.
        g.mode = MODE_OVERWORLD;
        g.fade = 1.0f; g.fadeDir = -1;
        g.facing = 1;
        g.moving = false;
        overworldSnapCamera();
        cinemaArrive("ELMSWICK", "the last field office", 0.0f, -7.0f);
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
    int sw = std::max(1, GetScreenWidth());
    int sh = std::max(1, GetScreenHeight());
    int vw = std::max(1, g.vw);
    int vh = std::max(1, g.vh);
    float rawAspect = (float)sw / (float)sh;
    if (rawAspect >= 1.20f && rawAspect <= 2.60f) {
        return { 0.0f, 0.0f, (float)sw, (float)sh };
    }
    float scale = std::min((float)sw / (float)vw, (float)sh / (float)vh);
    float w = std::round((float)vw * scale);
    float h = std::round((float)vh * scale);
    return { std::floor((sw - w) * 0.5f), std::floor((sh - h) * 0.5f), w, h };
}

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1280, 720, "A Place for Agents — an endless field assignment");
    SetWindowMinSize(640, 360);
    int mon = GetCurrentMonitor();
    int mw = GetMonitorWidth(mon), mh = GetMonitorHeight(mon);
    if (mw > 0 && mh > 0) {
        int initW = (mw >= 1440 && mh >= 840) ? 1280 : 1024;
        int initH = (mw >= 1440 && mh >= 840) ? 720 : 576;
        SetWindowSize(initW, initH);
        SetWindowPosition((mw - initW) / 2, (mh - initH) / 2);
    }
    SetExitKey(0); // ESC opens pause menu / backs out instead of abruptly quitting
    SetTargetFPS(60);

    InitAudioDevice();
    loadAllAssets();
    syncVirtualCanvas();
    newGameState(1337u);
    g.mode = MODE_TITLE;
    refreshSaveExists();

    while (!WindowShouldClose() && !g.quitRequested) {
        syncVirtualCanvas();
        float dt = std::min(0.05f, GetFrameTime());

        if (IsKeyPressed(KEY_F11) || (IsKeyDown(KEY_LEFT_ALT) && IsKeyPressed(KEY_ENTER))) {
            toggleFullscreenClean();
        }

        // Fade transitions run smoothly in every mode
        if (g.fadeDir != 0) {
            g.fade += g.fadeDir * dt * 2.6f;
            if (g.fade <= 0.0f) { g.fade = 0.0f; g.fadeDir = 0; }
            if (g.fade >= 1.0f) { g.fade = 1.0f; g.fadeDir = 0; }
        }

        // Keep monotonic animation clock and toasts alive in modes that don't tick them themselves
        bool selfTicks = (g.mode == MODE_OVERWORLD || g.mode == MODE_ROOM ||
                          g.mode == MODE_DUNGEON || g.mode == MODE_BATTLE ||
                          g.mode == MODE_SAIL);
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
            case MODE_DUNGEON:   dungeonUpdate(dt); break;
            case MODE_SAIL:      sailUpdate(dt); break;
            case MODE_DIALOG:    dialogUpdate(dt); break;
            case MODE_MENU:      menuUpdate(dt); break;
            case MODE_BATTLE:    battleUpdate(dt); break;
            case MODE_GAMEOVER:  gameOverUpdate(); break;
        }

        updateScore(dt);

        g.hotButtons = 0; // fresh hover set for the draw below
        // ---- render to crisp virtual framebuffer
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
                if (g.travelMap) drawTravelMap(); else drawWorld();
                drawHUD();
                drawCinemaOverlay();
                break;
            case MODE_ROOM:
                roomDraw();
                drawCinemaOverlay();
                break;
            case MODE_DUNGEON:
                dungeonDraw();
                drawCinemaOverlay();
                break;
            case MODE_SAIL:
                sailDraw();
                drawHUD();
                drawCinemaOverlay();
                break;
            case MODE_DIALOG:
                if (g.dialogReturn == MODE_ROOM) roomDraw();
                else if (g.dialogReturn == MODE_DUNGEON) dungeonDraw();
                else if (g.dialogReturn == MODE_SAIL) { sailDraw(); drawHUD(); }
                else { if (g.travelMap) drawTravelMap(); else drawWorld(); drawHUD(); }
                drawDialogBox();
                break;
            case MODE_MENU:
                if (g.menuReturn == MODE_ROOM) roomDraw();
                else if (g.menuReturn == MODE_DUNGEON) dungeonDraw();
                else if (g.menuReturn == MODE_SAIL) { sailDraw(); drawHUD(); }
                else { if (g.travelMap) drawTravelMap(); else drawWorld(); }
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

        // ---- sharp integer-oversampled presentation filling the window cleanly
        Rectangle dst = getPresentationRect();
        int preScale = std::clamp((int)std::ceil(std::max(dst.width / (float)g.vw, dst.height / (float)g.vh)), 1, 4);
        int upW = g.vw * preScale;
        int upH = g.vh * preScale;
        bool exactInteger = (std::abs(dst.width - (float)upW) < 0.5f && std::abs(dst.height - (float)upH) < 0.5f);

        if (!exactInteger && preScale > 1) {
            if (g.upscaleRt.id == 0 || g.upscaleRt.texture.width != upW || g.upscaleRt.texture.height != upH) {
                if (g.upscaleRt.id != 0) UnloadRenderTexture(g.upscaleRt);
                g.upscaleRt = LoadRenderTexture(upW, upH);
                SetTextureFilter(g.upscaleRt.texture, TEXTURE_FILTER_BILINEAR);
            }
            BeginTextureMode(g.upscaleRt);
            ClearBackground(BLACK);
            DrawTexturePro(g.rt.texture,
                           { 0, 0, (float)g.vw, -(float)g.vh },
                           { 0, 0, (float)upW, (float)upH },
                           { 0, 0 }, 0.0f, WHITE);
            EndTextureMode();
        }

        BeginDrawing();
        ClearBackground(Color{ 6, 8, 16, 255 });
        if (!exactInteger && preScale > 1 && g.upscaleRt.id != 0) {
            DrawTexturePro(g.upscaleRt.texture,
                           { 0, 0, (float)upW, -(float)upH },
                           dst, { 0, 0 }, 0.0f, WHITE);
        } else {
            DrawTexturePro(g.rt.texture,
                           { 0, 0, (float)g.vw, -(float)g.vh },
                           dst, { 0, 0 }, 0.0f, WHITE);
        }

        float pixelScale = dst.height / (float)g.vh;
        if (g.crtOn && pixelScale >= 1.25f) {
            float bandH = std::max(1.0f, pixelScale * 0.26f);
            for (int y = 0; y < g.vh; ++y) {
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

    if (g.upscaleRt.id != 0) UnloadRenderTexture(g.upscaleRt);
    UnloadRenderTexture(g.rt);
    unloadScore();
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
