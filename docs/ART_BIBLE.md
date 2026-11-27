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

---

## 9. Overworld tile registry (the World chipsets)

**Status: authoritative for the SNES-style overworld.** The overworld is a
scaled-down world map in the manner of *Lufia II* / *Final Fantasy VI* /
*Dragon Quest*: deep-blue sea, bright grass, dark forest clumps, mountain
chains, desert and snow patches, and miniature town/castle/cave icons. It is
built **only** from the OpenRTP World chipsets below (plus the credited
supplementary packs). Coordinates are `(column, row)` in 16×16 cells, origin
top-left, on the 480×256 sheet.

### 9.1 How an OpenRTP / RPG Maker 2000 chipset works

The World sheets are standard RM2k/2003 chipsets, and they are read the same
way the engine reads them:

- **Sheet = 480×256 px = 30 columns × 16 rows of 16×16 tiles.**
- **Two layers.** A *lower layer* (terrain the party walks on: sea, grass,
  forest, mountain, roads) and an *upper layer* (objects drawn over the party:
  trees, houses, castles, caves). Upper-layer cells use a colour key for
  transparency: `World.png` keys with pink `(255,103,139)`, but
  `retro_World.png` keys with **magenta `(224,103,191)`** (cyan `(0,147,146)`
  also occurs). The first upper-layer cell is blank. Use the right key per
  sheet or every icon gets a pink/magenta box behind it.
- **Autotiles are 3×4 blocks.** The engine expands a single autotile choice
  into bordered terrain: row 0 of the block is the editor/preview row, and the
  3×3 below it holds the edges, corners and centre. This is the *same*
  corner-mask idea as the field atlas in §3 — the overworld reuses it, it does
  not invent a second terrain system.
- **Water autotile region** (top-left): the first 3×4 blocks hold the coast
  boundary, a shared centre holds open/deep water, and a further block holds
  4-frame animated water (waterfall/rapids). Blue = water, grey-blue = deep
  water, the animated columns cycle.
- **Single tiles** (lower layer) are individual 16×16 cells that neither
  autotile nor animate (world-map bushes, settlement markers, rocks).

Reference sheets for this layout: jojogape's *RPG Maker 2000/2003 Tileset
Template v2* (DeviantArt d5f3quh) and the RPG Maker Forums "Chipset
composition" tutorial. The vendored `assets/source/openrtp/AUTHORS.md` is the
per-file author/license table.

### 9.2 The two world chipsets

| Sheet | Author / year | License | Character |
|---|---|---|---|
| `ChipSet/World.png` | Jason Perry (finalbossblues), 2022 | CC0 | Modern OpenRTP world chipset; bright, saturated |
| `ChipSet/retro_World.png` | MrBeast + Redshrike + surt + Ogrebane + Gwes + jikmok, 2014 | CC-BY / CC0 mix | **SNES-retro** world chipset; the Lufia-2-era look. Primary overworld source |

`retro_World.png` is literally composed of the MrBeast *Worldmap/Overworld*
tileset plus surt's town/forest tiles (see its AUTHORS.md row), which is why it
reads as a 16-bit console overworld rather than a modern tileset. Both sheets
share the identical 30×16 RM2k layout below, so a crop coordinate works on
either.

### 9.3 Region map (shared 30×16 layout)

| Region (cols × rows) | Contents |
|---|---|
| 0–5 × 0–7 | Water autotiles: coast boundary, open/deep sea, animated water |
| 6–8 × 0–3 | Dirt-on-grass autotile (paths / dry earth) |
| 9–11 × 0–3 | Sand-on-grass autotile (desert / beach) |
| 6–8 × 4–7 | Swamp-on-grass (World) / forest-on-grass (retro) autotile |
| 9–11 × 4–7 | Snow-on-grass autotile |
| 0–5 × 8–11 | Grass (bright meadow; cols 3–5 a darker variant) |
| 6–11 × 8–11 | Ice mountains (6–8) and snow-capped mountains (9–11) |
| 0–5 × 12–15 | Forest clumps (0–2) and brown hills/mountains (3–5) |
| 12–17 × 0–15 | Walls / floors / interior (NOT overworld terrain) |
| 18–23 × 0–7 | Mountain-range zigzags (rows 0–1) and raised plateaus (rows 2–7) |
| 18–29 × 8–15 | Upper-layer objects: trees, settlements, castles, dungeons, landmarks |

### 9.4 Terrain autotiles (lower layer)

| Terrain | Source block | Overworld meaning |
|---|---|---|
| Deep sea | cols 0–2, rows 3–7 | impassable open ocean (boat only) |
| Coast / shallow | rows 0–2 | water meeting land; foam edge |
| Animated water | cols 3–5, rows 3–7 | waterfall / rapids (4-frame cycle) |
| Grass | cols 0–5, rows 8–11 | walkable meadow (the default land) |
| Forest | cols 0–2, rows 12–15 | walkable-or-blocking woodland clumps |
| Dirt / path | cols 6–8, rows 0–3 | roads and dry earth |
| Sand / desert | cols 9–11, rows 0–3 | desert and beach |
| Swamp | cols 6–8, rows 4–7 (World) | mire / fen |
| Snow | cols 9–11, rows 4–7 | snowfield |
| Brown mountain | cols 3–5, rows 12–15; cols 9–11, rows 8–11 | impassable range |
| Ice mountain | cols 6–8, rows 8–11 | impassable frozen peak |
| Mountain range | cols 18–23, rows 0–1 | zigzag range silhouette on grass/snow |
| Plateau / mesa | cols 18–23, rows 2–7 | raised grass/dirt/snow/rock with cliff edge |

### 9.5 Overworld site & landmark icons (upper layer, `World.png` coords)

These are the miniature markers placed on the world map; each is cropped as a
unit (exact bbox finalized at import). They read as a silhouette at 1× (§3).

| Icon | Cell(s) | Size | Overworld role |
|---|---|---|---|
| Round tree | (19,8) | 1×1 | woodland dressing |
| Dead tree | (20,8) | 1×1 | dressing |
| Large tree | (18–19,10–11) | 2×2 | forest landmark |
| Flower bush / bush / palm | (18,9)/(19,9)/(20,9) | 1×1 | dressing |
| Village house, red roof | (22,8) | 1×1 | **town** marker (temperate) |
| Village house, snow roof | (23,8) | 1×1 | **town** marker (snow) |
| Village house, wooden | (22,9) | 1×1 | **town** marker |
| Village house, snow II | (23,9) | 1×1 | **town** marker (snow) |
| Grey castle | (20–21,10–11) | 2×2 | **castle / city** marker |
| Blue castle | (22–23,10–11) | 2×2 | **castle / city** marker |
| Tent / pavilion | (18–19,12–13) | 2×2 | camp |
| Round tower | (20,12–13) | 1×2 | **tower** dungeon marker |
| Monument / statue | (21,12–13) | 1×2 | landmark |
| Temple / shrine | (22,12–13) | 1×2 | temple |
| Cave entrance | (23,12–13) | 1×2 | **cave / dungeon** marker (dark arch) |
| Volcano | (18–19,14–15) | 2×2 | landmark |
| Ruins | (20–21,14–15) | 2×2 | ruin dungeon marker |
| Gate | (22,14–15) | 1×2 | pass / gate |
| Bridge | (23,14–15) | 1×2 | river crossing |
| Colosseum | (24–25,14–15) | 2×2 | landmark |

`retro_World.png` carries the same icon vocabulary in the same cells with the
retro palette (blue/red-roof houses, grey + gold castles, dark-arch cave).

### 9.6 Supplementary overworld packs (credited, source-only)

Vendored under `assets/source/overworld/`; descriptions saved beside each pack.

- **MrBeast — Worldmap/Overworld** (CC-BY 3.0): `tileset.png` (grass/sand/snow
  coast transitions, forest, roads, bridges, town/castle/tower icons),
  `mountains.png` (3×3 mountain blocks — use the centre cell), `water.png`
  (32×32 animated coast, 15 frames, ping-pong loop). Already inside
  `retro_World.png`.
- **ArMM1998 — Zelda-like** (CC0): `Overworld.png` (40 tiles/row), `objects.png`,
  `character.png`, `NPC_test.png`, `cave.png`, `Inner.png`, `font.png`, `log.png`.
  Supplementary props/characters; more ALttP-flavoured than Lufia.

### 9.7 Semantic overworld terrain (generation vocabulary)

The procedural overworld maps each tile to one of these semantics, then draws
it with the chipset cell above. This is the contract the map generator and the
overworld renderer will share (defined in code at the implementation step):

`OW_DEEP, OW_SEA, OW_GRASS, OW_FOREST, OW_DIRT, OW_SAND, OW_SWAMP, OW_SNOW,
OW_MOUNTAIN, OW_ICE, OW_ROAD, OW_BRIDGE, OW_TOWN, OW_CITY, OW_CAVE, OW_TOWER,
OW_TEMPLE, OW_RUINS, OW_CAMP, OW_VOLCANO, OW_BRIDGE_GATE`.

Rules: terrain families transition only through the autotile corner masks
(§9.1); site icons are foot-anchored and y-sorted like field props (§3); the
overworld draws the party at miniature scale and compressed distance, the way
a 16-bit world map does. No runtime tile is generated yet — this section is the
registry the import step will crop against.
