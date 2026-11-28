# Handoff: Step-by-Step Procedural Map Pipeline (Towns, Interiors, Caves, Dungeons, Quest Maps)

**Date:** 2026-11-28 · **Branch:** `arena/51568ea4-a-place-for-agents`  
**Status:** Blueprint ready — no code changes made for these map tracks yet.  
**Follows:** `docs/HANDOFF_PROCEDURAL_JRPG_WORLD.md` and `docs/HANDOFF_PLATE_LAYOUT_GENERATOR.md` (Overworld plate builder + `tools/build_overworld_layout.py` completed).

---

## 1. Purpose & Workstyle

The overworld pipeline established a reliable four-step workflow that avoids wasted effort, pixel guessing, and broken integrations:

1. **Step 1 — Audit & Source (`reuse` first, crop only what is missing):** Check `assets/tiles/` and `assets/sprites/` before touching any source sheet. Never overwrite working OpenRTP runtime art. When a tile/prop does not exist at runtime yet, crop it from `assets/source/openrtp/` using visually verified grid coordinates, then run `generate_manifest.py` and `validate_assets.py`.
2. **Step 2 — Register in `docs/ART_BIBLE.md` (§10 / §11 / §12):** Document every reused runtime asset and every newly mapped `(col, row)` coordinate from zoomed grid crops (never guess coordinates), record the exact transparency key, state placement/wall-height rules, and lock the semantic tile vocabulary (`TN_*`, `IN_*`, `CV_*`/`DN_*`, `QM_*`).
3. **Step 3 — Build Modular Plate / Stamp Catalogs (`tools/build_*_plates.py` → `docs/<domain>_assets/`):** Assemble coherent tiles, autotile wall matrices, and modular room/lot stamps with explicit edge labels / door sockets, plus contact-sheet PNGs and `index.json` for visual review **before** writing any layout generator.
4. **Step 4 — Build Deterministic Seeded Randomizers & Verifiers (`tools/build_*_layout.py`):** Self-contained Python tools matching `tools/build_overworld_layout.py` (`--seed`, `--check`, byte-identical JSON + PNG preview, and domain invariants).
5. **Step 5 — C++ Game Integration (only after Steps 1–4 pass visually and structurally):** Replace `src/room.cpp`, expand `src/dungeon.cpp`, wire towns + quest sites + `tools/build_overworld_layout.py` logic behind one shared `MapInstance` / portal system, and restore `CMakeLists.txt`.

> **Important house discipline:** Run **one dictated track/step at a time**. Do not jump straight to C++ rewrites (`src/` / `CMakeLists.txt` remain intentionally paused with deleted `src/world.*` and `src/overworld.cpp` until the procedural map tools are validated). Keep `tools/validate_assets.py` passing at all times, and never create test suites, `AGENTS.md`, or `build.sh`.

---

## 2. Shared Ground Truth: Keys, Guardrails & Existing Runtime Assets

### 2.1 Verified Source Sheets & Transparency Keys

Every coordinate in this document was verified against `480×256` (`30×16` cells of `16×16`) OpenRTP sheets and `288×256` (`12×8` cells of `24×32`) Object sheets on disk:

| Source Sheet | Dimensions | Transparency Key (`RGB`) | Notes |
|---|---|---|---|
| `assets/source/openrtp/ChipSet/Exterior.png` | `480×256` | `(255, 103, 139)` (`KEY_PINK`) | Primary town wall/roof/sign/prop sheet |
| `assets/source/openrtp/ChipSet/retro_Exterior.png` | `480×256` | `(254, 103, 138)` (`KEY_PINK2`) | **Note 1-LSB shift vs. `KEY_PINK`** — key with tolerance `<= 12` |
| `assets/source/openrtp/ChipSet/Interior.png` | `480×256` | `(255, 103, 139)` (`KEY_PINK`) | Walls, hearths, counters, shop goods, beds, altars, thrones |
| `assets/source/openrtp/ChipSet/Dungeon.png` | `480×256` | `(255, 103, 139)` (`KEY_PINK`) | Cavern walls/cliffs, chasms, lava, rails, bars, crystals, seals |
| `assets/source/openrtp/ChipSet/retro_Dungeon.png` | `480×256` | `(255, 103, 139)` (`KEY_PINK`) | Retro palette counterpart of `Dungeon.png` |
| `assets/source/openrtp/CharSet/Object1.png` | `288×256` | `(254, 103, 138)` (`KEY_PINK2`) | Swinging wooden/iron/steel/jail doors + red/brown treasure chests |
| `assets/source/openrtp/CharSet/Object2.png` | `288×256` | `(255, 103, 139)` (`KEY_PINK`) | Lufia-II-style switches, levers, pressure pads, push blocks, minecarts |
| `assets/source/overworld/armm1998_zelda_like/` | `Overworld/Inner/cave/objects` | Native alpha (check black BG) | Supplementary CC0 only (`ART_BIBLE.md` §9.6); prefer OpenRTP first |

### 2.2 Validator & Manifest Gotchas (`tools/validate_assets.py`)

Before saving new runtime files into `assets/`, note what `tools/validate_assets.py` enforces:
- **Tiles (`assets/tiles/*.png`) must be `16×16`** unless explicitly added to `TILE_EXCEPTIONS` in `tools/validate_assets.py` (currently only `flower_0.png`, `flower_1.png`, `landscape.png`, `relief.png`). During Step 3 plate prototyping, save composite plates and preview contact sheets in `docs/<domain>_assets/` — only put `16×16` tiles or explicit `ART_BIBLE.md`-documented atlases into `assets/tiles/`.
- **Basenames across runtime `assets/` (excluding `assets/source/`) must be globally unique.**
- **Character sheets** (`sprites/hero_*.png`, `sprites/npc_*.png`) must be `72×96`; **faces** (`sprites/face_*.png`) `48×48`; **icons** `16×16`.
- **After ANY file addition/edit under `assets/`:** run `python3 tools/generate_manifest.py` then `python3 tools/validate_assets.py` (currently `344` runtime / `424` total files).

### 2.3 Existing Runtime Asset Audit (DO NOT Re-Extract What Already Exists!)

The codebase already contains **344 runtime assets** generated by `tools/import_openrtp.py`, `tools/landscape_art.py`, `tools/village_art.py`, and `tools/art_extra.py`. Reuse these verbatim:

| Domain | Already Ready in `assets/tiles/` | Already Ready in `assets/sprites/` |
|---|---|---|
| **Towns & Exterior** | `landscape.png` (`256×1008`, 21 surfaces × 3 phases × 16 corner masks: meadow, wood, amber, heath, fen, dunes, snow, ice, crag, path, soil, cobble, court, leaves, ripples, mesa, frost_track, snow_crag), `relief.png` (`256×80`), `cobble.png`, `flagstone.png`, `dirt.png`, `soil.png`, `bridge.png`, `turnips.png`, `lilies.png`, `tuft_0..2.png`, `pebble.png`, `flower_0..2.png`, `grass_0..3.png`, `grass_dark.png`, `sand.png`, `beach.png`, `snow.png`, `water_0..1.png`, `deep_water.png` | Buildings: `house_0..4` (`62×54` to `66×60`), `shop` (`70×61`), `inn` (`74×66`), `temple` (`76×78`), `manor` (`78×68`), `smithy` (`76×64`), `castle` (`98×84`), `tower` (`33×56`), `gate` (`32×32`).<br>Town props: `well`, `sign`, `bench`, `torch`, `campfire`, `cauldron`, `basin`, `flowerbed`, `mailbox`, `noticeboard`, `fence`, `fence_post`, `fence_gate`, `barrel`, `crate`, `chest_closed`, `chest_open`, `ladder`, `statue`, `pillar`.<br>Flora: `tree_0..2`, `pine_0..2`, `amber_oak`, `copper_oak`, `willow`, `tree_snow`, `dead_tree`, `bush`, `bush_plain`, `stump`, `rock_0..2`, `boulder`.<br>Actors: all 28 `npc_*.png` sheets + 30 `face_*.png` portraits. |
| **Interiors** | `plank_floor.png`, `tile_floor.png`, `stone_floor.png`, `brick.png`, `brick_dark.png`, `carpet.png` | `counter` (`16×15`), `shelf` (`48×32`), `table` (`16×16`), `longtable` (`46×16`), `bed` (`16×28`), `stool` (`8×9`), `barrel`, `keg`, `crate`, `lantern`, `candle`, `plate`, `potted_plant`, `urn`, `statue`, `pillar`, `forge_hearth` (`48×48`), `forge_anvil` (`32×32`), `stair_down` (`16×16`), `stair_up` (`16×16`) |
| **Caves & Dungeons** | `cave_floor_0..3.png`, `cave_rock_0..3.png`, `dungeon_floor.png`, `flagstone.png`, `cobble.png`, `stone_floor.png`, `dirt.png`, `swamp.png`, `sand.png`, `ice.png`, `ice_crack.png`, `snow.png`, `arcane.png`, `brick.png`, `brick_dark.png` | `cave_mouth` (`48×48`), `stair_down`, `stair_up`, `cave_stalagmite` (`15×27`), `cave_stalagmite_small` (`15×14`), `cave_crystal_blue` (`12×25`), `cave_crystal_cluster` (`29×31`), `cave_rock_pile` (`29×24`), `cave_boulder` (`29×28`), `crystal`, `boulder`, `torch`, `chest_closed`, `chest_open`, plus `pillar`, `statue`, `urn`, `barrel`, `crate`, `reeds`, `mushroom` |
| **Quest / Special Sites** | All town + dungeon floors above | `monument` (`46×47`), `pavilion` (`46×47`), `ruin_arch` (`36×35`), `tidewell` (`40×30`), `tidewell_dry` (`40×30`), `tide_stone` (`26×36`), `commons_stone*` (`30×46`), `gate`, `castle`, `temple`, `tower`, `cave_mouth`, `boat` (`72×96`) |

---

## 3. Track 1 — Towns & Building Interiors

### Why Towns and Interiors Move Together
In `src/room.cpp`, interiors are currently **hardcoded `15×11`–`17×12` static rooms** (`ROOM_SHOP`, `ROOM_INN`, `ROOM_TEMPLE`, `ROOM_HOUSE`, `ROOM_FORGE`), and towns previously lived inside the continuous `world.cpp` village pass. In the discrete JRPG architecture, every `town_*` or `city_*` plate on the overworld becomes a discrete **Town Map** whose doors act as bidirectional portals into seeded **Interior Maps**.

### Step 1.1 — Missing Art Extraction (Verify Before Cropping)
**What is missing from runtime `assets/`:**
- **Town perimeter & civic detail (`Exterior.png`, key `(255,103,139)`):**
  - Town battlements / gatehouse & stone towers: cols `18..20, rows 0..3` (gatehouse), cols `21..23, rows 0..4` (wall + steps), cols `24..27, rows 0..1` (turrets), cols `18..23, row 4` (battlement tops).
  - Town stone/plaster/timber building wall panels & shingled roofs (for modular walled compounds/halls): cols `12..17, rows 0..15`.
  - Trade shop hanging signs (`INN`, `PUB`, sword/armor/potion signs): cols `21..23, rows 12..15`.
  - Town market stalls/goods & banners & wall ivy: market crates `(21..23, 6..7)`, banners `(28..29, 4..7)`, climbing ivy `(25, 8..11)`, stone double-gate `(27..29, 8..9)`.
- **True 2.5D interior walls & wing props (`Interior.png`, key `(255,103,139)`):**
  - **Missing completely today:** Interior rooms currently have flat `brick` walls without top caps, wall-mounted trim, or dark interior doorways. Crop 1-tile cap + 2-tile wall faces from `cols 15..17, rows 0..11` (purple brick, plaster, light stone, dark stone, blue brick), dark exit doorway `(26, 2..5)`, stone hearth `(12..14, 10..15)`, wall windows & framed paintings & stained glass `(24..26, 1..5)`, curtains `(22..23, 4..6)` & `(18..19, 10..12)`.
  - **Missing multi-tile interior furnishings:** L-shaped/corner counters `(18..20, 13..15)`, shop weapon/armor/herb racks `(18..22, 8..11)`, tall bookcases `(18..20, 0..2)` & `(27..29, 4..5)`, double/inn beds & piano & crystal ball `(24..29, 6..13)`, royal chair & throne `(26..29, 14..15)`, interior stone stairs `(21, 3..5)`, ritual circle `(21..23, 12..14)`.
  - **Animated doors (`Object1.png`, key `(254,103,138)`):** wooden & iron-trimmed swinging doors (`cols 0..5, rows 0..2`).

### Step 1.2 — Add `§10 Town & Interior Tile Registry` to `docs/ART_BIBLE.md`
Document:
1. Every reused building (`house_0..4`, `shop`, `inn`, `temple`, `manor`, `smithy`, `castle`), its **exact tile footprint** (e.g., `house_0` `62×54` occupies `4×2` solid base + `2` tiles roof overhang north, door at bottom-center `(tx, ty)`), and the **2-tile doorstep apron** rule.
2. **Interior 2.5D wall rule:** Every north-facing interior wall renders **1 top-cap tile + 2 front-face tiles** (`cap` at `y-2`, `upper` at `y-1`, `base` at `y`). Wall props (windows, paintings, shelves, hearths) mount on `y-1`/`y`; floors start at `y+1`.
3. Semantic vocabularies:
   - `TN_GRASS, TN_WOOD_FLOOR, TN_PATH, TN_COBBLE, TN_COURT, TN_WATER, TN_CANAL, TN_BRIDGE, TN_GARDEN, TN_WALL, TN_GATE, TN_TREE, TN_WELL, TN_SIGN, TN_LAMP, TN_MARKET, TN_LOT_DOOR`
   - `IN_VOID, IN_WALL_CAP, IN_WALL_FACE, IN_FLOOR_WOOD, IN_FLOOR_TILE, IN_FLOOR_STONE, IN_CARPET, IN_DOOR_EXIT, IN_STAIR_UP, IN_STAIR_DOWN, IN_COUNTER, IN_HEARTH, IN_SHELF, IN_RACK_WEAPONS, IN_RACK_ARMOR, IN_BED, IN_TABLE, IN_ALTAR, IN_THRONE, IN_CHEST`

### Step 1.3 — Build Town & Interior Plate/Stamp Libraries
- **Critical scale lesson (do NOT copy overworld `3×3` blindly for towns):**
  A `house_0` sprite (`62×54` px) is ~`4×3.4` tiles, and `temple` (`76×78`) / `castle` (`98×84`) are `5×5` to `6×5.5` tiles. A `3×3`-tile plate cannot contain a single house!
  Instead, use **8×8-tile district plates** (`128×128` px) for towns:
  - **Connector/Street Plates** (Carcassonne-matched 8-cell edges of `{grass, road, cobble, water, wall}`): `street_ns`, `street_ew`, `street_corner_ne/nw/se/sw`, `street_t_n/e/s/w`, `street_cross`, `plaza_well`, `plaza_fountain`, `canal_bridge_h/v`, `gate_n/s/e/w`.
  - **District Lot Plates** (`8×8`, guaranteed road edge on the South — or configurable entrance side — plus 2-tile alley margin around the building footprint): `lot_house_0..4`, `lot_shop`, `lot_inn`, `lot_smithy`, `lot_temple`, `lot_manor`, `lot_market_stalls`, `lot_garden_turnips`, `lot_park_grove`.
- **Builder scripts & previews:**
  - `tools/build_town_plates.py` → `docs/town_assets/plates/<name>.png`, `index.json`, `plates_streets.png`, `plates_lots.png`.
  - `tools/build_interior_plates.py` → modular room chunks/stamps (`entry_mat`, `counter_bay`, `hearth_bay`, `bunk_wing`, `shop_display`, `temple_nave`, `study_library`, `cellar_stair`) + 16-mask wall caps in `docs/town_assets/`.

### Step 1.4 — Seeded Generators & `--check` Verifiers
- **`tools/build_town_layout.py --seed S --tier {hamlet,town,city} --biome {temperate,snow,sand} --gates N,E,S,W --check`:**
  - Grid sizes: `hamlet` (`3×3` district plates = `24×24` tiles), `town` (`5×4` = `40×32` tiles), `city` (`6×5` = `48×40` tiles).
  - Placement algorithm: Carcassonne/AC-3 edge-matching on `8×8` plates (reusing the exact backtracking + terrain-normalization lessons from `tools/build_overworld_layout.py`), seeded hard constraints for required civic lots (`inn` + `shop` always present; `smithy` + `temple` on `town`/`city`; `castle`/`manor` on `city`), and perimeter wall/treeline with gates locked to the overworld road ports.
  - **`--check` invariants:**
    1. Every side-port matches (`side_match_violations == 0`).
    2. Building footprints + roof/alley buffers never overlap (`overlap_count == 0`).
    3. Every required civic building is placed (`missing_required_lots == 0`).
    4. BFS on walkable ground from **every active gate** reaches **100% of building door aprons, plaza well/noticeboard, and every other active gate**.
- **`tools/build_interior_layout.py --seed S --kind {shop,inn,smithy,temple,house,manor} --check`:**
  - Composes multi-wing rooms (`15×11` to `24×16`) deterministically by snapping interior wall bays + furniture zones around a walkable circulation spine.
  - **`--check` invariants:** unobstructed BFS from `door_exit` to keeper NPC counter, any stairs/beds/chests, and zero solid props placed inside the 2-tile entry landing.

---

## 4. Track 2 — Caves & Multi-Floor Dungeons

### Build on What Exists in `src/dungeon.cpp`
`src/dungeon.cpp` **already** has a deterministic `56×40` multi-floor generator (BSP rooms + corridors + `weatherCavern` cellular shaping across `TH_MOSSY..TH_CRYPT` + sconces + chests + BFS reachability check + final-floor vault) and already loads `cave_floor_0..3`, `cave_rock_0..3`, `caveFormation[0..5]`.

What it **does not yet have** (and why dungeons feel repetitive compared to *Lufia II*):
1. **True 2.5D cliff/masonry wall geometry:** `dungeon.cpp` currently stamps flat square `brick` or `cave_rock_*` tiles instead of top-lip + vertical wall faces + chasm overhangs.
2. **Typed dungeon structures:** caves, towers, crypts/ruins, and castles all run the exact same BSP box carver.
3. **Progression puzzles & lock-and-key topology:** there are no switches, levers, push-boulders, locked gates, or chasm bridges tied to a dependency graph.
4. **Two-ended mountain-pass tunnels:** `cave_in_mountain` on the overworld needs a deterministic **West↔East (or N↔S) traversal cavern**, not just a dead-end loot hole.

### Step 2.1 — Missing Art Extraction
- **From `Dungeon.png` (`30×16`, key `(255,103,139)`) & `retro_Dungeon.png`:**
  - Autotiled cavern/crypt wall & pit blocks: grey cavern stone `(6..11, 4..7)`, brown volcanic/earth walls `(6..11, 8..11)`, obsidian/abyss & crypt walls `(6..11, 12..15)`.
  - 2.5D cliff faces & raised ridges: earth cliff faces + wooden chasm bridge `(12..17, 4..7)`, snow/ice cliff faces `(12..17, 8..12)`, demonic shrine face `(15..17, 0..3)`.
  - Hazards & traversal mechanics: bubbling lava autotile `(3..5, 8..11)`, arrow/warp floor pads `(22..23, 4..6)`, minecart tracks & wheel `(24..29, 0..3)`, iron bars/portcullis `(24..29, 6..7)` & `(24..26, 14..15)`, floor vents/fissures `(24..29, 8..9)`, crystal spires `(27..29, 2..3)` & `(20..21, 10..11)`, boss magic circle & throne `(21..23, 14..15)` & `(27..29, 14..15)`.
- **From `CharSet/Object2.png` (`288×256`, key `(255,103,139)`) & `Object1.png` (`(254,103,138)`):**
  - Ruby toggle floor switches (`cols 0..2, rows 0..3`), gold levers (`cols 3..5, rows 0..3`), stone floor buttons/pressure plates (`cols 6..11, rows 0..3`), iron/stone/ice pushable blocks & boulders (`cols 0..8, rows 4..7`), minecarts (`cols 9..11, rows 4..7`), locked cell/fortress doors (`Object1.png` `cols 6..11, rows 0..6`).

### Step 2.2 — Add `§11 Cave & Dungeon Tile Registry` to `docs/ART_BIBLE.md`
Document:
1. Exact `(col, row)` coords per dungeon biome (`cave_earth`, `cave_ice`, `cave_lava`, `crypt_stone`, `ruins_moss`, `tower_keep`).
2. **2.5D North-Wall Rule:** Any `WALL` cell whose southern neighbour is walkable (`FLOOR`/`BRIDGE`/`LAVA`) draws a **wall-face** tile (`2` vertical cliff rows for caves/castles or `1`–`2` masonry rows for crypts/towers); interior solid rock draws `WALL_CAP` with an autotiled inner lip (`4-bit` N/E/S/W wall mask).
3. Semantic vocab (`CV_*` / `DN_*`): `DN_VOID, DN_WALL_CAP, DN_WALL_FACE, DN_FLOOR, DN_CHASM, DN_LAVA, DN_WATER, DN_BRIDGE, DN_RAIL, DN_STAIR_UP, DN_STAIR_DOWN, DN_DOOR_LOCKED, DN_DOOR_SWITCH, DN_SWITCH_FLOOR, DN_LEVER, DN_PUSH_BLOCK, DN_WARP_PAD, DN_CHEST, DN_HEAL_SPRING, DN_BOSS_ALTAR`.

### Step 2.3 — Build Dungeon Room Stamp & Wall Matrix Catalog (`tools/build_dungeon_plates.py`)
Create two clean assets/docs sets in `docs/dungeon_assets/`:
1. **16-mask Wall-Cap & Cliff-Face Matrix** per theme (`cave`, `ice`, `lava`, `crypt`, `tower`, `ruins`) — verified visually in `plates_walls.png`.
2. **Modular Room Plates / Stamps (`9×9` and `11×9` tiles with explicit N/E/S/W 2-tile doorway sockets):**
   - `room_entry_stair`, `room_exit_stair`, `room_portal_tunnel` (for pass-through caves), `room_corridor_*`, `room_pillar_hall`, `room_chasm_bridge`, `room_switch_gate` (Lufia-style switch opens barred gate), `room_push_boulder` (push stone onto pressure plate or into chasm gap), `room_minecart_spur`, `room_treasure_vault`, `room_healing_spring`, `room_boss_sanctum`.

### Step 2.4 — Seeded Multi-Floor Dungeon Generator (`tools/build_dungeon_layout.py`)
- **CLI:** `--seed S --kind {cave,tower,ruins,castle} --theme {mossy,earth,frost,lava,crypt} --floors N --pass-ports W,E --check`
- **4 Distinct Floor Topologies:**
  - `cave`: organic cellular-automata cavern carved around a spine of puzzle/chamber stamps (`48×36` per floor; if `--pass-ports` is set for `cave_in_mountain`, Floor 0 guarantees a traversable path from West portal to East portal).
  - `tower`: compact square/octagonal floors (`26×22`) with perimeter battlements/windows, ascending central/corner stair alignment (`stair_up(F+1)` lines up with `stair_down(F)`!).
  - `ruins` / `castle`: axial symmetrical rooms + colonnades + central warp/ritual wing (`44×32` or `56×40`).
- **Lock-and-Key Progression Graph (Zero Softlocks!):**
  1. Build a room tree rooted at `Entry`, pick longest path to `StairDown` / `BossSanctum`, and keep 1–3 side branches for optional treasure/puzzle rooms.
  2. Any locked gate (`DN_DOOR_LOCKED`, `DN_DOOR_SWITCH`) may ONLY be placed on a tree edge whose required **key / lever / push-block plate** is placed strictly in the **upstream reachable component** from `Entry`.
  3. **`--check` stateful reachability simulator:** Start at `Entry` with `opened_switches = set()`, `keys = 0`; repeatedly flood-fill walkable cells, trigger reachable switches/push-plates, collect keys, unlock reachable matching doors, and assert:
     - `StairDown` / `BossAltar` (and `ExitPortal` for pass-caves) are reached.
     - **100% of chests** are reachable after solving upstream switches.
     - Zero wall-face geometry errors (`WALL` north of `FLOOR` always has a `WALL_FACE` tile).

---

## 5. Track 3 — Quest Maps & Special Sites

### What Makes a Quest Map Distinct
Overworld special plates (`temple`, `monument`, `camp`, `gate_pass`, `bridge_over_water`, `volcano`, `colosseum`) and story set-pieces (`Commons`, `Tidewell Isles`, `Boss Vaults`) are **narrative anchors**. They shouldn't look like random BSP dungeons OR static copy-pastes: each site has a **composed focal landmark** surrounded by seeded environmental framing and explicit portal alignment.

### Step 3.1 & 3.2 — Audit & Art Bible `§12 Quest & Set-Piece Registry`
- **Reuse existing landmarks first:** `monument` (`46×47`), `pavilion` (`46×47`), `ruin_arch` (`36×35`), `tidewell` / `tidewell_dry` (`40×30`), `tide_stone` (`26×36`), `commons_stone*` (`30×46`), `gate` (`32×32`), `temple` (`76×78`), `castle` (`98×84`), `campfire`, `cauldron`.
- **Add only missing set-piece crops:** `Exterior.png` colosseum arena/archway `(24..25,14..15)` + banners `(28..29,4..7)`; `Interior.png` summoning circle `(21..23,12..14)` + altar/organ `(24..25,14..15)`; `Dungeon.png` magma caldera rim/demonic gate `(12..17,0..3)` + prison gate `(24..26,14..15)`.
- Document semantic `QM_*` tags & **Two-Portal Transporter Contract** in `docs/ART_BIBLE.md` §12.

### Step 3.3 & 3.4 — Quest Site Stamps (`tools/build_quest_plates.py`) & Generator (`tools/build_quest_layout.py`)
Implement seeded generators for each overworld special plate archetype (`24×18` to `36×28` tiles):

| Overworld Plate | Map Archetype | Key Invariants (`--check`) |
|---|---|---|
| `gate_pass` | Mountain defile checkpoint: cliff flanks + central guardhouse + lever-opened portcullis | `West/East` (or `North/South`) pass portals exist & connect once gate lever is pulled; lever sits on player-accessible side |
| `bridge_over_water` | Approach causeway + wooden/stone bridge spans + mid-river islet dock/ferryman | Both banks connect across bridge deck; water flanks blocked cleanly |
| `temple` / `monument` | Consecrated stone court + colonnade + flanking braziers + central rune/tidewell + side reliquary | Approach path `south_entry -> shrine_altar -> quest_npc` unobstructed; reliquary chest reachable |
| `camp` | Timber palisade + central `campfire` + 2–4 `pavilion` tents + crates + patrol/quest NPC | Perimeter entrances match trail side; tents & fires never block walkways |
| `colosseum` | Walled arena ring + spectator risers + twin champion gates + prize dais | Combat ring clear (`>= 10×8` unobstructed); entry gate -> challenger NPC -> exit gate clear |
| `volcano` | Basalt ledges over lava (`Dungeon.png` magma) + narrow stone crossings + magma boss altar | Entry ledge -> puzzle/bridge route -> caldera boss altar reachable with zero chasm softlocks |

---

## 6. Step 5 (Final C++ Integration Contract — Later Phase)

Once all four standalone Python generators (`build_overworld_layout.py`, `build_town_layout.py` + `build_interior_layout.py`, `build_dungeon_layout.py`, `build_quest_layout.py`) pass `--check` across 50+ seeds and their rendered PNG previews look right:

1. **Shared Map JSON / Struct Contract:** Every generator emits the same schema (or mirrors the exact C++ deterministic RNG + tile rules):
   ```json
   {
     "version": 1,
     "kind": "town | interior | cave | dungeon | tower | ruins | quest",
     "seed": 4444,
     "site_id": "overworld:12,5",
     "width": 40,
     "height": 32,
     "layers": { "ground": [], "wall_face": [], "wall_cap": [], "deco": [] },
     "solid": [],
     "portals": [{ "id": "gate_w", "tx": 0, "ty": 15, "target_map": "overworld", "target_portal": "12,5:w" }],
     "npcs": [],
     "chests": [],
     "switches": []
   }
   ```
2. **Transporter Wiring (`Overworld ↔ Submap`):** Every special plate on the overworld grid (`town_*`, `castle_*`, `cave`, `cave_in_mountain`, `tower`, `temple`, `ruins`, `camp`, `gate_pass`, `bridge_over_water`, `volcano`, `colosseum`) stores a deterministic `site_id = "<kind>:<px>,<py>"` and portal bindings so stepping onto the plate (or entering from a mountain side) transitions to that map and exits back onto the exact overworld plate coordinates.
3. **Recommended Execution Order:**
   1. ~~Fix the known `water_*` source-crop bug~~ — **DONE**: `water_*` now compose the shipped `assets/tiles/water_0.png` with a `beach.png` lip instead of the retro sheet's black boundary block.
   2. Track 1: Towns + Interiors (Steps 1.1 → 1.4).
   3. Track 2: Caves + Multi-Floor Dungeons (Steps 2.1 → 2.4).
   4. Track 3: Quest Maps & Pass Transporters (Steps 3.1 → 3.4).
   5. Track 4 (Step 5): Unify C++ `MapSystem` (`src/` + `CMakeLists.txt` restoration), verify save/load & cross-map transitions end-to-end, and update `README.md` / `ART_BIBLE.md`.
