# A Place for Agents

**An endless SNES-JRPG through a world that grows as you explore it.**

You wake in **Elmswick**, a hand-authored little town with a cobbled plaza, a
well, shops, a temple and neighbours who know your name. Walk out of the south
gate and the world keeps generating — meadows, forests, rivers, lakes, frozen
reaches, deserts, mires, lonely hills — and here and there another village with
an inn to warm your bones and a shop that sells honest steel.

Built with [raylib](https://github.com/raysan5/raylib). The art is adapted from
**OpenRTP** RPG Maker-style tiles, characters and portraits (see
[CREDITS.md](CREDITS.md)).

## Play

Download the latest build from the **Releases** page (or from the artifacts
of any CI run):

- `APlaceForAgents-Windows.zip` — run `APlaceForAgents.exe`
- `APlaceForAgents-Linux.zip` — run `./APlaceForAgents`

### Controls

| Input | Action |
|---|---|
| `WASD` / arrow keys | Walk (hold `Shift` to run) |
| Left mouse button | Walk anywhere — pathfinds around trees, water and buildings |
| Click a building | Walk to its door and go inside |
| `Space` / `E` / `Enter` | Talk, open a chest, enter a door, advance dialog |
| `Esc` / `Tab` | Pause menu (party, items, equip, quests, options/save) |
| Mouse | Menus, shops, battles — everything is clickable too |
| `F11` | Toggle fullscreen |

## What's in it

- **A village to start in.** Elmswick is hand-placed at the origin: a cobbled
  plaza with a working well, a shop and an inn either side of the road, a
  temple of the Light, six cottages with fences, gardens, torches and a campfire,
  plus ten-odd villagers, a farmer with a quest, a town dog and a signpost.
- **An endless procedural world** — deterministic noise terrain (elevation,
  moisture, temperature), seeded per run. Grassland, dark forest, sand, beach,
  swamp, rock and snow biomes with matching props (broadleaf trees, conifers,
  dead trees, snow pines, boulders, crystals, shrubs, toadstools, pebbles).
  Named zones: *The Greenmere, The Frozenreach, The Sunscorch Flats, The Mirefen,
  The Stonelands, The North Road, The Howling West, The Far Wilds.*
- **More villages out in the wilds** — deterministic per 96-tile sector, each
  named from a syllable table, laid out with plaza, roads, shop, inn, temple and
  wandering townsfolk. Interior scenes for shops, inns, temples and houses.
- **SNES-style ATB battles** — Active-Time gauges per combatant, three party
  members (Aldric the knight, Iris the mage, Pip the cleric), attack / spells /
  items / defend / flee, crits, elemental weaknesses, a shared ATB, spell
  schools (fire, storm, holy, physical) and per-class spell lists that unlock
  with levels.
- **Thirteen monsters** taken from the OpenRTP monster sheets: slime, imp, bat,
  hornet, wisp, wolf, skeleton, ghost, orc, serpent, golem, night rider — and the
  Slime King, who holds the Crown of Elm. They appear both as wandering field
  monsters (walk into one to fight it) and in random encounters.
- **22 items** — potions, ethers, elixirs, phoenix downs, three tiers of sword,
  armour and shield, a power ring, a mana amulet, quest keys. Shops buy and sell;
  equipment is slot-based per hero.
- **Three quests** with a quest log: slay six slimes for Farmer Hedrick, track
  down a traveller's lost locket in the wilds, and take the Crown of Elm back
  from the Slime King in the northern castle.
- **Chests, landmarks, day/night and weather-feel** — procedural treasure
  chests across the world, ruin / stone-circle / camp / tower / castle landmarks,
  drifting clouds, a slow day/night wash, screen shake, floating damage numbers.
- **Save & load** — checksummed binary save (`aplaceforagents_save.sav` next to
  the binary), title-screen *Continue*, autosave slot in the options page.
- **Dialog and portraits** — every named NPC has an OpenRTP face, a typewriter
  dialog box, and the shop / inn / temple flows live inside it.

## Build from source

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
./build/bin/APlaceForAgents              # Linux
build\bin\Release\APlaceForAgents.exe    # Windows
```

Linux needs the usual X11/GL dev packages
(`libgl1-mesa-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libasound2-dev`).
raylib 5.5 is fetched automatically via CMake `FetchContent`.

The game is drawn into a 512×288 virtual framebuffer and letterboxed to whatever
window size you give it, with integer scaling when there's room, so the pixels
stay crisp.

No tests in CI, by design — this repository is a place for agents to go wild and
the build is the check.

## Regenerating the assets

```bash
python3 tools/build_art.py    # needs: pillow, numpy
python3 tools/gen_assets.py   # needs: pillow, numpy, soundfile
```

The original OpenRTP source sheets are vendored under `assets/source/openrtp/`.
`tools/build_art.py` generates the remaining art and then runs
`tools/import_openrtp.py`, which adapts the source sheets with their magenta and
cyan colour keys (terrain tiles, objects, chests, buildings, walking sprites,
battle sprites and portraits). The outputs are committed to git.

## Layout

```
src/game.hpp      shared types & the global context
src/world.cpp     noise, biomes, villages, props, npcs, chests, landmarks,
                  field monsters, A* pathfinding
src/core.cpp      item/hero/spell/enemy tables, asset loading, UI primitives,
                  save & load
src/overworld.cpp field movement, camera, interactions, encounters
src/render.cpp    the field renderer, HUD, title, intro, game-over
src/dialog.cpp    dialog boxes, shop, inn, pause menu
src/room.cpp      interior scenes
src/battle.cpp    ATB battle (update + draw)
src/main.cpp      window, virtual framebuffer, mode loop
tools/            asset generation + the OpenRTP importer
assets/           runtime art & audio (committed)
.github/workflows/build.yml   builds the Windows exe and Linux binary on every push
```
