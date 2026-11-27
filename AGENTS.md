# AGENTS.md — working on A Place for Agents

A place for agents: this file is the short, honest map of the repo for any
agent (human or AI) picking it up. Read it before changing code.

## What this is

An endless SNES-style JRPG in C++17 with [raylib](https://github.com/raysan5/raylib) 5.5.
You start in the hand-authored village **Elmswick** and the world generates
procedurally around you: seven biomes, generated villages, landmarks, chests,
field monsters, ATB battles, three quests, shops/inns/temples, interiors,
save/load, and a 13-cue streamed score. Current version: **0.2.0** (see
`CMakeLists.txt`).

## Build & run

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
./build/bin/APlaceForAgents              # Linux
build\bin\Release\APlaceForAgents.exe    # Windows (GUI subsystem, no console)
```

Linux needs X11/GL dev packages (`libgl1-mesa-dev libx11-dev libxrandr-dev
libxinerama-dev libxcursor-dev libxi-dev libasound2-dev`). raylib is fetched
by CMake `FetchContent` — no vendored copy. Runtime assets live in `assets/`
and are copied next to the binary by a post-build step; the game also finds
`assets/` relative to the working directory or the exe.

**Project policy: no tests.** Do not add or run tests. CI (`.github/workflows/build.yml`)
only builds and packages the Windows exe and Linux binary.

## How the game is put together

- `src/game.hpp` — shared types and the global `Game g` context. Everything
  hangs off this header; new shared state goes here.
- `src/main.cpp` — window, 512×288 virtual framebuffer stretched to fill the
  window (mouse mapping in `getVirtualMouse()` uses the same X/Y scales),
  mode loop, CRT scanline/vignette overlay, fade transitions.
- `src/world.cpp` / `src/world.hpp` — deterministic noise, biomes, the
  Millbrook river and story trails, villages, props, NPCs, chests, landmarks,
  field monsters, A* pathfinding. Rendering, collision and pathfinding all
  read `propAt()` / `sampleGround()` so they cannot disagree.
- `src/overworld.cpp` — field movement (cardinal-only keyboard, click-to-move
  with A*), camera, encounters, interactions, prompts.
- `src/render.cpp` — Y-sorted field renderer, HUD, title/intro/game-over,
  dialogue box. SNES look: navy windows with white double borders, gold
  accents, Press Start 2P, blinking cursor triangles.
- `src/dialog.cpp` — dialogue, pause menu (Party/Items/Equip/Quests/Options),
  shop, inn, Sound Room.
- `src/room.cpp` — interiors (shop/inn/temple/house).
- `src/battle.cpp` — side-view ATB battle (WAIT pauses gauges in menus,
  ACTIVE never pauses).
- `src/music.cpp` — one music director: scene → cue, crossfades, resume.
- `src/core.cpp` — item/hero/spell/enemy tables, asset loading, UI
  primitives, save/load (checksummed `SaveBlob`, version 4).

## Conventions to keep

- **Determinism:** world gen is a pure function of `(tx, ty, worldSeed)`.
  Never depend on visit order or camera position for what exists.
- **SNES feel:** 16px tiles, corner-mask autotiling, warm pastel palette with
  cool/purple shadows (see `tools/snes.py`), navy+gold UI, Press Start 2P at
  multiples of 8px. No modern rounded-glass UI.
- **Assets are committed.** `tools/build_art.py` / `tools/gen_assets.py`
  regenerate art + SFX; `tools/render_score.py` renders the score from
  `assets/source/openrtp/` MIDIs. Playing/building needs none of these.
- **Saves:** `aplaceforagents_save.sav` next to the binary. Changing
  `SaveBlob` layout breaks old saves (size check) — prefer reusing flag bits.
- **Windows:** the exe must stay a GUI-subsystem build (`WIN32` in CMake) —
  no console window. Don't add `printf`-only debugging that needs a console.

## Where to look for things

- Controls / first walks / what's in the game: [README.md](README.md)
- Art, music and font licenses + visual references: [CREDITS.md](CREDITS.md)
- Cue sheet (titles, durations, licenses): `assets/audio/SCORE.txt`
- Screenshots: `docs/screenshots/`
