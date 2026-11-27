# ART BIBLE — the one art system for *A Place for Agents*

**Status: authoritative.** If code, tools, or assets disagree with this file,
this file (plus [CREDITS.md](../CREDITS.md) for licensing) is what to fix
towards — *within the guardrails of §0*. Workstream A (A1–A7) is indexed in
§8 so any agent can find which rule implements which requirement.

---

## 0. The guardrail that outranks everything (read first)

**The OpenRTP assets are the game's art. Keep them, credit them, build on
them. Never replace them.**

In November 2026 an agent "unified" the art by regenerating all 250 runtime
PNGs procedurally, deleting `tools/import_openrtp.py`, and rewriting
CREDITS.md to claim the art was "100% original repository-authored". That
destroyed the work of every agent before it and misattributed
EasyRPG/OpenRTP art. PR #15 reverted it in full; `tools/validate_assets.py`
now fails CI on every ingredient of that failure (deleted import pipeline,
de-credited CREDITS, stale/corrupt manifest, non-contract sizes).

Concretely, for every future agent:

- **Never** regenerate, redraw, or "clean up" a runtime PNG to replace an
  OpenRTP-derived one. Pixel content changes go through
  `tools/import_openrtp.py` (crop / re-order / palette-ramp of the vendored
  sheets) or they don't happen.
- **Never** delete `tools/import_openrtp.py`, `tools/rtp_sheet.py`,
  `assets/source/openrtp/`, or the OpenRTP tables in CREDITS.md.
- **Never** describe the runtime art as wholly original. Provenance follows
  the *source sheet* (see [ART_MANIFEST.md](../ART_MANIFEST.md)).
- Renames are allowed only when content is byte-identical, every code/tool
  reference is updated, and the manifest is regenerated (e.g. PR #16:
  `icons/candle.png → icons/icon_candle.png` to end a basename collision).
- New art is allowed only as an *in-dialect addition* (§2.5): same grid,
  palette family, outline and light as the OpenRTP sheet it sits next to,
  credited CC0 in CREDITS.md, listed in the manifest. Additions extend the
  roster; they do not substitute for it.

## 1. A1 — One dialect: EasyRPG/OpenRTP

The audit (what is *actually* in use, not what we wish were in use):

| Layer | Source in use | How it reaches runtime | License |
|---|---|---|---|
| Terrain, props, buildings | OpenRTP `ChipSet/{Exterior,Interior,Dungeon,World}.png` | `tools/import_openrtp.py` + `landscape_art.py` + `village_art.py` crops, palette ramps, corner-mask composition | CC0 (Jason Perry / finalbossblues) |
| Heroes, NPCs, animals | OpenRTP `CharSet/{Actor,People,Animal}*.png` | colour-keyed crops, RM2k frame re-ordering in `import_openrtp.py` | CC BY 4.0 / CC0 per sheet (VictorSena, Marina Navarro Travesset, Garakuta…) |
| Monsters | OpenRTP `CharSet/Monster*.png`, `Monster/Hornet.png` | keyed crops + exact 2× battle versions | CC BY 4.0 / CC0 per sheet |
| Portraits | OpenRTP `FaceSet/*.png` | 48×48 crops | CC0 (Garakuta) |
| Combat FX | OpenRTP `Battle/{Arrow,Blow,Sword1}.png` | green-keyed, trimmed frames | CC0 (Someoneman) |
| UI plates | OpenRTP `System/{System,Royal}.png`, `GameOver`, `Panorama`, `Title` | crops/resizes in `import_openrtp.py` | CC BY / OGA-BY (andwhyisit), CC0 (jetrotal, Francisco de la Peña, Tom Lemmens) |
| Music | OpenRTP MIDIs (vendored) | offline render, `tools/render_score.py` | CC BY 4.0 (Juan Manuel Nigretti), CC0 (Someoneman) |
| SFX, a few icons/sprites | repository-authored (`tools/gen_assets.py`, `tools/art_*.py`) | drawn in the same dialect, same grid/palette rules | CC0 (repository authors) |

That is the one pipeline: **vendored OpenRTP → import/adapt → runtime**.
`tools/build_art.py` only supplies in-dialect repository pieces and then
*defers* to `import_openrtp.py`, which runs last and writes the
OpenRTP-derived files. "Do not mix dialects" therefore means: no
photographic downscales, no second pixel-art family, no AI/procedural
replacement set — repository pieces must be indistinguishable in grid,
outline, light and palette family from the OpenRTP crop beside them, and
are individually listed in CREDITS.md (wolf, bat, wisp, serpent, mushroom,
Slime King crown, synthesized SFX, some icons).

Upstream references: EasyRPG's per-file author/license table
(`assets/source/openrtp/AUTHORS.md`, mirrored at
<https://github.com/EasyRPG/RTP/blob/master/AUTHORS.md>) and the OpenRTP
project notes at <https://blog.easyrpg.org/> — all OpenRTP material is
CC BY 4.0 or CC0 per asset, which is exactly why attribution in
CREDITS.md is load-bearing and must never be simplified away.

## 2. A2 — Technical contract (adapted to the dialect, on purpose)

The starter target suggested "16×24 characters, 4-frame walk". That is
**not** the OpenRTP/RM2k format, and forcing it is what caused the
replacement disaster. The contract below is the dialect's native format —
adapted from the target, not abandoned:

### 2.1 Grid and scale
- Tile grid: **16×16 px** (RM2k/3 chipset standard). Everything terrain is
  composed on this grid; atlases are exact multiples of 16.
- Virtual canvas 512×288; world is drawn at **1× native pixels** and
  presented with integer/oversampled scaling (`src/main.cpp`). Sprites are
  never drawn at fractional scale in the field.
- Character cell: **24×32 px** (1.5 × 2 tiles), feet anchored at bottom-
  centre of the cell (`drawCharSheet`, `src/render.cpp`).

```
        16px        24px cell
      |------|    |---------|
      +------+------+       +---+
      | tile | tile |       |   |  32px = 2 tiles tall
      +------+------+       |   |  feet on the bottom edge,
      | tile | tile |       +---+  horizontally centred
```

### 2.2 Characters and animation
- Sheet: **72×96 px = 3 frames × 3 rows** of 24×32. Columns are walk
  frames `[step, idle/centre, step]`; rows are `down, up, right`;
  **left is the right row mirrored** (native RM2k convention, done by
  `import_openrtp.py` at import time and by a negative-width source rect
  in the renderer).
- Walk = 3 frames/direction minimum in this dialect (target said 4; the
  source sheets have 3 and we do not fabricate frames). Idle = centre
  frame (the middle column); the renderer holds it when not walking.
- Reaction/emote: dialogue is the reaction channel — 48×48 FaceSet
  portrait + blinking gold cursor. No overhead emote sprite ships today;
  adding one is an *extension* (§2.5): 16×16, `assets/fx/`, gold-on-navy,
  ≤2 frames. Do not "meet" this by editing character sheets.
- Battle sprites: field sprite at **exactly 2×** (nearest-neighbour) in
  `*_battle.png`; boss exception `slime_king_battle.png` at 2.5×. The
  validator enforces the ratio per file pair.
- Portraits: **48×48** (`face_*.png`), one 64×64 hero plate
  (`face_knight_big.png`). Icons: **16×16**, always.

### 2.3 Outline, light, anti-aliasing
- **Selective 1px outline**: dark outline on the silhouette's outer edge
  and major form breaks only — not on every internal edge (that is how
  the OpenRTP sheets are drawn; match the sheet you crop from).
- **Light from upper-left**; shadows fall lower-right. Palette ramps
  hue-shift (cool shadows, warm highlights) via `rtp_sheet.ramp` /
  `landscape_art.ramp` — never a flat multiply/tint of a whole sprite.
- **No internal anti-aliasing, no fractional scaling, no downscaled
  photography.** Source sheets are used at native resolution; resizes
  are integer (2× battle sprites) or a documented plate resize
  (Title/GameOver/Panorama → 512×288).

### 2.4 Palette
Core palette = the terrain ramps already locked in
`tools/landscape_art.py` (every ramp ≤ 8 colours — validator-enforced;
today ≤ 6) + the UI ramp (§5). Character palettes are native to their
source sheets; in-dialect additions sample the nearest terrain/UI ramp
instead of inventing hues.

Terrain ramps (hex, dark→light), excerpt:

| Family | Ramp |
|---|---|
| meadow | `70a264 77a766 7eab68` |
| forest floor | `477657 4d7c59 54825c 5a865e 609063` |
| amber / leaves | `8b9055 959858 9c9e5c` · `b89956 bda35c c9ae62` |
| fen (mire) | `4c7570 537c72 598075 628775` |
| heath | `6e816a 7e8e75 8b987c 95a17f a8ae89` |
| dunes / beach | `d5b27e dab884 dfbe88` · `d1b886 dfc89b e9d5ac f0deb5` |
| frost / ice | `c8dce5 dce9ec e6efee` · `91bfce 9ac6d4 a4cfd9 adcfdc` |
| rock / cobble | `666f70 747f79 889081 9aa088 a8aa92` · `686e6b 7b827c 8d9285 9b9d8d b1ad98 bfbba2` |
| water / deep | `377e91 3c8497 438a9a 4d909e 5698a5` · `325e7d 356580 396c85 3e728b 447a90` |
| relief: crag / sandstone / blue stone | `4e5158…b2a18b` · `815f55…d6ad82` · `506479…cad6db` (6 each, in code) |

A style bible's job is to answer resolution, palette, outline, shading,
animation and proportions *before* the second asset exists (see
<https://www.sprite-ai.art/blog/2d-pixel-art-style-guide>) — this file is
that answer for this repository, and §6–§7 say how to work inside it.

## 3. A4 — Terrain as a system

Terrain is **composed, never stretched**: `tiles/landscape.png`
(256×1008 = 21 surfaces × 3 phases × 16 corner masks) + `tiles/relief.png`
(256×80). Each surface row carries the full 16-tile marching-squares set
(NW=1, NE=2, SW=4, SE=8 corner weights, `landscape_art.inside`) — the
16-mask corner system, not a single wallpaper tile (background:
<https://lobehub.com/skills/claude-dev-suite-claude-dev-suite-tile-design>;
RM2k chipsets are 16px tiles, <https://rpgmaker.fandom.com/wiki/Tileset>).
Individual 16×16 swatches in `assets/tiles/` are exports of the same
ramps for interior floors, not a second terrain system.

Per biome (renderer `Surface` enum + `world.cpp` prop selection — every
row already satisfies A4; new biomes must fill every column):

| Biome | Base + transitions | Path / water-or-dry | Elevation | Large props (≥3) | Small props (≥6) | Interaction / landmark |
|---|---|---|---|---|---|---|
| Meadow (Elmswick) | meadow, forest-floor masks | dirt path, water+deep | relief crag, rock ground | tree_0–2, pine_0–2, willow | bush ×2, stump, rock_0–2, mushroom, tuft/flower overlays | village buildings, well; castle |
| Forest (Briarwood) | forest floor, meadow masks | dirt, water | crag, rock | pine_0–2, tree_0–2 | mushroom, bush, stump, rocks | pavilion camp; tower |
| Amber (Amberwood) | amber, leaves masks | dirt, water | crag, rock | amber_oak, copper_oak, tree | mushroom, stump, bush, rocks | monument; manor |
| Fen (Mirefen) | mire (swamp), beach masks | soil path, water+deep+ripples bank | blue-stone relief, rock | willow, dead_tree | reeds, mushroom, rocks, lilies overlay | ruin_arch; temple |
| Heath (Stonelands) | heath, rock masks | dirt, water | crag + snow-crag edge, rock | pine_1–2, tree | boulder, bush_plain, rocks, crystal | statue, pillar; gate |
| Frost (Frozenreach) | snow, snow-crag masks | frost-track (dry path), ice (dry water) | blue-stone relief, rock | tree_snow, pine | crystal, rock_2, stump | cave_mouth; castle |
| Dunes (Sunscorch) | dune, ripples, mesa masks | ripples (dry water equiv.), beach edge | sandstone relief, mesa rock | yucca, dead_tree | rock_1, crystal, bush_plain | pavilion; tower |

Rules: ground families transition only through the mask atlas; props are
**foot-anchored** (bottom-centre at the tile's ground point) and
**y-sorted** by foot Y against characters (`render.cpp` draws props and
actors in foot order — a prop whose base is lower draws later/in front).
Landmarks must read as a silhouette at 1× (castle 98×84, tower, statue,
monument, cave mouth): if it doesn't read at 1×, it isn't a landmark.

## 4. A5 — Character readability

- The three heroes ship as **three distinct sheets** (validator checks
  the files differ): knight (armour + plume silhouette), mage (hat/robe),
  cleric (hood) — distinguishable at 1× by silhouette first, colour
  second. Any new hero/NPC follows silhouette → colour → detail, in
  that order.
- NPC roster (28 sheets): villager0–9 get *shape* variation from
  different People-sheet slots, not recolours of one slot; role NPCs
  (elder, mayor, guard, soldier, knight, merchant, smith, priest,
  farmer, ranger, woman, child) each use a distinct slot; animals
  (dog, cat, chicken, sheep, cow, horse) are Animal-sheet slots.
- Monsters: field sprite + 2× battle sprite (§2.2); related monsters
  must differ in silhouette (slime vs crowned Slime King: crown is a
  shape addition on MerchantShade's slime, not a tint).
- QA before merging a character change: view hero/NPC/monster sheets
  side by side at 1×/2×/3× (nearest-neighbour zoom of the 72×96 sheets);
  a new player must name knight/mage/cleric in under five seconds. If
  two sheets fail that test, change the *slot or silhouette*, never
  just the palette.

## 5. A6 — One UI finish: navy and gold

Exactly one window language, implemented in `src/core.cpp`:

- `drawWindow` — main windows: navy gradient body
  (`#1c2c76 → #0a1034`), 1px near-black outer line, 1px white inner
  line, 1px steel-blue (`#6076d0`) third line, hard square corners,
  2px offset drop shadow.
- `drawWindowBlue` — gold-trim variant (`#ffd682` inner line) for
  headers/counters.
- `drawPixelPanel` — the *only* primitive for menu rows, tabs, buttons
  and cards: square corners, navy fill, 1px dark outline, 1px inner
  border — **gold `#ffe896` when selected, steel-blue otherwise**.
  Selected text is `#ffecb0`; headings are gold `#ffe896`; body text
  white on navy.
- Nine-slice: window chrome is drawn as nested 1px frames (a procedural
  nine-slice at 1px units) so any rectangle size keeps 1px corners;
  `assets/ui/window_frame*.png` (System-sheet crops) remain the source
  reference for that geometry. Dialogue box = `drawWindow` at the
  bottom of the canvas with the 48×48 portrait inset; HUD and prompts
  (blinking gold triangle `drawCursor`) reuse the same colours/type.
- Type scale: Press Start 2P only, integer sizes **8 / 16 / 24 / 32**
  (loaded at exactly those sizes, point filtering). No other font, no
  fractional text sizes.
- **Banned:** `DrawRectangleRounded*`, pills, glass/blur, drop-shadow
  text effects beyond the 1px hard offset in `drawLine`. The validator
  greps `src/*.cpp` for the rounded calls; as of this bible there are
  zero (14 former call sites in battle/dialog/core now use
  `drawPixelPanel`).

## 6. Doing it right: adding or changing art

1. Start from the vendored sheet (`assets/source/openrtp/…`), check its
   author/license in `AUTHORS.md`, and add/extend the crop in
   `tools/import_openrtp.py` (or the relevant `tools/*_art.py` composite).
2. Keep the §2 contract: 16px grid, 24×32 cells, ≤8-colour ramps,
   selective outline, upper-left light, no AA, integer resizes only.
3. Wire the loader in `src/core.cpp` with a directory-qualified path;
   basenames must be unique among runtime assets.
4. Run `python3 tools/generate_manifest.py`, then
   `python3 tools/validate_assets.py`, then build. Update CREDITS.md if
   a new source sheet or repository piece is involved.
5. If the existing OpenRTP material genuinely cannot express something,
   the answer is an in-dialect addition (§1) or a bible amendment
   explaining why — **not** a replacement set (§0).

**Do / do-not, with real files:**

- ✅ Do: crop `CharSet/Actor1.png` → `sprites/hero_knight.png` (72×96).
- ✅ Do: palette-ramp a ChipSet cell into a new biome family in
  `landscape_art.py` (≤8 colours) and add its atlas row.
- ❌ Do not: draw a new 16×24/4-frame hero "to spec" and delete the
  72×96 sheets (that was PR #14).
- ❌ Do not: downscale `Panorama/Dawn2.png` (640×480) and call it a
  sprite; plates are resized once, documented, and used as plates.
- ❌ Do not: load a vendored source sheet directly at runtime; runtime
  only loads the adapted files in [ART_MANIFEST.md](../ART_MANIFEST.md).
- ❌ Do not: stretch one tile image across a biome ("wallpaper"), tint a
  whole sprite as a "variant", or add a rounded/glass UI element.

## 7. A7 — Validation and manifest

- [ART_MANIFEST.md](../ART_MANIFEST.md) is **generated** by
  `tools/generate_manifest.py` (path, byte size, dimensions, category,
  license/provenance for all **384** files under `assets/`) and is never
  hand-edited.
- `tools/validate_assets.py` (stdlib only) checks: duplicate runtime
  basenames; category dimensions (16×16 tiles/icons, 72×96 character
  sheets, 48×48 faces, 512×192 backdrops, 512×288 plates, fx/ui
  allow-lists, battle = 2× field); disk ↔ manifest parity including
  byte sizes; every C++ asset literal exists; no
  `DrawRectangleRounded*` in `src/*.cpp`; terrain ramps ≤ 8 colours;
  and the §0 OpenRTP guardrails. It is a CI step (`.github/workflows/
  build.yml`) and must pass before merge.

## 8. Workstream A index

| Req | Where satisfied |
|---|---|
| A1 one dialect, credited, no mixing | §1 audit + §0 guardrail; CREDITS.md per-sheet table; manifest provenance column |
| A2 technical contract | §2 (adapted to native OpenRTP format, with the deviation and its reason stated) |
| A3 art bible | this file |
| A4 terrain as a system | §3 (16-mask atlas, per-biome completeness table, anchoring/y-sort) |
| A5 character readability | §4 + distinct-hero validator check |
| A6 one UI finish | §5 (`drawWindow`/`drawWindowBlue`/`drawPixelPanel`, rounded UI removed) |
| A7 automatic validation + manifest | §7 (`tools/validate_assets.py`, generated `ART_MANIFEST.md`, CI gate) |
