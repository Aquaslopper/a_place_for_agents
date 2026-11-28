# Handoff: seeded plate-layout generator (the first procedural overworld step)

**Date:** 2026-11-27 · **Branch:** `arena/27049c45-a-place-for-agents`
**Status: NOT STARTED — no code written. This document is the entire spec. Do not re-survey; build.**

---

## Why this handoff exists

The previous agent spent the whole session surveying (reading docs, listing
plates, viewing PNGs, installing Pillow) and never wrote a line of code —
its turns were interrupted repeatedly before any file was created. There is
**no technical blocker**: the task is a self-contained Python tool, fully
specified below. If you are reading this, skip all exploration and go straight
to "What to build".

---

## The task (human's words)

> "we have these 3x3 plates now.... let us build a random seed generator that
> places these plates together"

Step by step. One tool. Nothing else.

## Hard constraints (from the human, repeated)

1. **Do NOT touch the deleted work:** `src/world.cpp`, `src/world.hpp`,
   `src/overworld.cpp` are deliberately deleted. Do not restore, recreate, or
   reference them. Do not edit anything in `src/` or `CMakeLists.txt`. The
   build is expected to be broken; that is a later step.
2. **One deliverable:** a Python tool that places the existing 3×3 plates into
   a seeded, edge-matched grid and renders a preview. No game integration, no
   docs rewrite, no water-plate fix, no C++ — unless the human asks next.
3. **One step at a time.** The human dictates each step and dislikes scope
   creep. Finish this tool, show the preview, stop.

## What already exists (verified this session — do not re-check)

- **100 plates**, 48×48 px each (3×3 of 16×16 tiles), in
  `docs/overworld_assets/plates/<name>.png`, canonical list in
  `index.json` (count 100). Built by `tools/build_overworld_plates.py`.
- **Plate naming convention** (drives edge labels):
  - `grass_plain` — all-grass base.
  - `<terrain>_<mask>` for terrain in
    `water, forest, sand, snow, dirt, mountain` and mask in the `MASKS`
    dict of `tools/build_overworld_plates.py` (full, pond, top, bottom,
    left, right, top2, bottom2, left2, right2, nw, ne, sw, se, vchannel,
    hchannel, cross — not every terrain has every mask; trust `index.json`).
  - **Site/dressing plates** (all stamped on an all-grass base, so all four
    sides are grass): `tree_round, tree_dead, tree_large, bush_flower,
    cactus, palm, town_red, town_snow, town_wood, town_snow2, castle_grey,
    castle_blue, cave, tower, temple, monument, camp, ruins, volcano, gate,
    bridge, colosseum`.
  - **Transporter plates** (terrain base + icon, sides come from the base
    mask): `bridge_over_water` = water vchannel ·
    `cave_in_mountain` = mountain full · `gate_pass` = mountain hchannel.
- **Masks are the 3×3 binary grids** in `MASKS` in
  `tools/build_overworld_plates.py` (e.g. `left = [[1,0,0],[1,0,0],[1,0,0]]`).
  A cell is terrain where mask=1, grass where 0.
- **Pillow 13.0.0 + numpy 2.4.6 are installed** (`pip install
  --break-system-packages pillow numpy` already done this session).
- **Known defect:** `water_*` plates render wrong (dark bands + white specks)
  — source art issue in `build_overworld_plates.py`, documented in its
  docstring. Ignore it; placement logic is independent of it.
- Previews/contact sheets live in `docs/overworld_assets/` — put the layout
  preview there too (e.g. `layout_seed<seed>.png`). Nothing under `assets/`,
  so **no manifest/validator run is needed** (validator only scans `assets/`).

## What to build

**File:** `tools/build_overworld_layout.py` (run from repo root, same style as
the other tools; plain `python3 tools/build_overworld_layout.py --seed N`).

### 1. Edge labels (derive from the mask, not from pixels)

For each plate, compute a 4-tuple of side labels `(N, E, S, W)`. Recommended
scheme — label each side by the **sequence of its 3 edge cells**
(terrain id or `grass`), e.g. for `forest_left` the west side is
`(forest, forest, forest)` and the east side `(grass, grass, grass)`.
Two adjacent plates are compatible iff the touching sides match
cell-for-cell (west side of the right plate == east side of the left plate).
This is the Carcassonne rule from the handoff doc and it makes roads/coasts
continue automatically. Site plates: all four sides = `(grass, grass, grass)`.

Alternative acceptable scheme (if cell-sequences prove awkward): one dominant
label per side, with a small compatibility table (grass↔anything,
water↔water, …). State which you used in the tool's docstring.

### 2. Placement

- CLI: `--seed` (required or default 1337), `--cols/--rows` (default
  something preview-friendly, e.g. 16×12 plates = 48×36 tiles),
  `--out` (default `docs/overworld_assets/layout_seed<seed>.png`).
- Seeded RNG only (`random.Random(seed)`) — same seed ⇒ byte-identical grid.
- Fill cell-by-cell (row-major is fine); at each cell filter the plate list
  to those compatible with already-placed N and W neighbours; pick one at
  random; **on empty candidate set, backtrack** (or restart with a
  deterministic attempt counter). No artificial border plates — the grid edge
  has no constraint (or, if you prefer, mark edge sides "wildcard"; say so).
- Terrain plates should appear as coherent patches, not salt-and-pepper:
  simplest way is to bias the candidate pick toward continuing the
  neighbour's terrain when compatible (e.g. weight same-terrain plates
  higher). Keep it simple; document the weighting.
- **Special-plate rules (light version, from the handoff):** site plates are
  rare — after the terrain grid is filled, punch a few site plates into
  grass-only cells (town spacing ≥ 4 plates apart; `cave`/`tower` only
  adjacent to a mountain-labelled side if that falls out naturally, else
  skip the rule and note it). Transporters (`bridge_over_water`,
  `cave_in_mountain`, `gate_pass`) may replace their exact base plate
  (same side labels ⇒ always legal).

### 3. Output

- Compose the grid by blitting the plate PNGs → one preview PNG (add a thin
  seam grid line every 48 px so plate boundaries are visible).
- Also write a small JSON next to it: `{"seed": …, "cols": …, "rows": …,
  "plates": [[row of names], …]}` — the canonical layout for later steps.
- Print a one-line summary: seed, grid size, counts per terrain, backtracks.

### 4. Verify (before reporting back)

1. Run twice with the same seed → `cmp` the two JSONs (must be identical).
2. Run a different seed → grid differs.
3. Write a tiny self-check inside the tool (or a `--check` flag): every
   horizontal/vertical neighbour pair satisfies the side-match rule; assert
   0 violations.
4. View the PNG yourself (`read_file`) — tiles must line up at seams.

## Out of scope (do not do)

- Fixing water plates · touching `src/` or the deleted files · C++ generator ·
  updating README/ART_BIBLE/GROWTH · committing anything the human did not
  ask for · opening a PR.

## Report back with

The command you ran, the preview image, the seed/size summary, the
self-check result — then stop and await the human's next dictated step.
