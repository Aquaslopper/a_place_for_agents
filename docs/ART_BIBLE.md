# Art Bible — A Place for Agents

> Single coherent visual specification for all runtime terrain, props, landmarks, characters, portraits, monsters, icons, battle effects, backdrops, and UI chrome.

---

## 1. Visual Dialect & Reference Anchor (A1)

- **Committed Dialect:** **Valley Hearth (16-bit SNES JRPG Pixel Art)** — crisp `16×16` world grid, `16×24` field characters, quiet walkable turf, rich sunlit foliage clusters, cool slate-blue roofs with warm limestone/timber masonry, and deep navy-and-gold UI windows.
- **Reference Anchor:** Warm golden late-afternoon upper-left sunlight (`#F2C968`, `#EFE4C6`) contrasting against cool slate-indigo shadows (`#141824`, `#242D3E`, `#3F4F66`) and calm river teals (`#3A6477`, `#5C919E`).
- **Pipeline Exclusivity:** **100% repository-authored coherent pixel art** built through `tools/build_art.py` using `tools/snes.py`.
  - **Forbidden:** Mixing unrelated OpenRTP visual sheets/portraits/titles with repository-drawn sprites, bicubic/bilinear photo downscales, smooth alpha gradients on pixel sprites, rounded vector UI pills, or non-integer pixel scales (`0.56×`, `0.62×`, `1.45×`, etc.).
  - **Audio vs. Visual Boundary:** Vendored EasyRPG OpenRTP files in `assets/source/openrtp/` supply **soundtrack MIDI compositions only** (`assets/audio/*.ogg`). Zero OpenRTP graphics enter `assets/` at runtime.

---

## 2. Technical Contract (A2)

| Rule | Specification |
| --- | --- |
| **Base Tile Grid** | `16×16` px (`TILE = 16`), rendered on `512×288` virtual pixel framebuffer (`TEXTURE_FILTER_POINT`). |
| **Character Sheet Base** | `16×24` px frame cell inside `128×96` px sheets (`8 columns × 4 rows`). |
| **Character Directions** | Row 0 = Down, Row 1 = Up, Row 2 = Left (native UL light), Row 3 = Right (native UL light). |
| **Character Animation** | Cols `0–1` = Idle (2 frames), Cols `2–5` = Walk (4-frame stride cycle), Col `6` = Reaction Emote, Col `7` = Action / Ready. |
| **Display Scale** | Strict **integer `1×`** inside virtual canvas across field, interior, dungeon, battle, portraits (`32×32`), icons (`16×16`), and clouds (`64×32`). |
| **PNG Grid Alignment** | Every runtime PNG dimension is an exact multiple of `16` px (with `128×96` sheets cleanly holding `8×4` `16×24` character cells on a `16px`-aligned atlas). |
| **Lighting Vector** | Consistent **upper-left `(-1, -1)`** sunlight (~315°). Shadows shift cool toward indigo/plum; highlights shift warm toward gold/cream. |
| **Outline Rule** | **1px selective outline**: deep cool ink (`#141824`) or dark ramp shadow on bottom/right and outer silhouette boundaries vs. ground; exposed upper-left highlights use warm ramp steps or toned edge lines — never pure `#000000` sticker cutouts or uniform internal gridlines. |
| **Anti-Aliasing** | **Zero** internal anti-aliasing, feathering, or soft gradient blurs on sprites, tiles, icons, portraits, FX, or backdrops. |

---

## 3. Master Core Palette (32 Colors) & Per-Biome Ramps (≤8 Colors)

### 3.1 Master Core Palette (32 Colors)

| Group | Name | Hex | Semantic Role |
| --- | --- | --- | --- |
| **Ink & Slate** | `INK_DEEP` | `#141824` | Primary selective outline, drop shadows, deep recesses |
| | `SLATE_DARK` | `#242D3E` | Cool rock/roof/armor shadow |
| | `SLATE_MID` | `#3F4F66` | Slate roof shingles, castle masonry, iron |
| | `SLATE_LIGHT` | `#677B92` | Lit slate roof rim, steel highlight, bridge masonry |
| | `STONE_WARM` | `#A89D8A` | Warm weathered limestone, flagstone midtone |
| | `CREAM_SUN` | `#EFE4C6` | Sunlit stucco walls, parchment, bright UI copy |
| **Timber & Gold** | `UMBER_DARK` | `#382520` | Bark crevice, warm wood outline/shadow |
| | `TIMBER_MID` | `#684430` | Cedar beams, trunks, chests, furniture |
| | `OCHRE_MID` | `#9C6839` | Packed earth rim, leather, oak midtone |
| | `OCHRE_LIGHT` | `#CF944E` | Sunlit dirt road, straw, brass |
| | `GOLD_SHADE` | `#C98A2C` | Antique gold bevel shadow, coins, trim |
| | `GOLD_SUN` | `#F2C968` | Sunlit gold UI border, crown, lantern flame |
| | `SUN_WHITE` | `#FFF3CA` | Hot flame core, metallic specular, star glint |
| **Foliage** | `LEAF_SHADOW` | `#1B3029` | Deep pine/canopy crevice |
| | `LEAF_DARK` | `#2F5238` | Shaded forest/meadow canopy |
| | `LEAF_MID` | `#527D3F` | Sage-emerald meadow grass & foliage midtone |
| | `LEAF_LIGHT` | `#82A84B` | Sunlit olive turf & tree crown |
| | `LEAF_SUN` | `#B6CC62` | Upper-left sunlit lime foliage highlight |
| **Water & Sky** | `WATER_DEEP` | `#213B4F` | Deep lake/river basin |
| | `WATER_MID` | `#3A6477` | Cool slate-teal river |
| | `WATER_LIGHT` | `#5C919E` | Sunlit shallow edge & glacier cyan |
| | `WATER_FOAM` | `#A4CED1` | Wave crest, frost highlight, pale mist |
| **Crimson** | `CRIMSON_DARK` | `#6E2230` | Deep wine cloak, brick shadow |
| | `CRIMSON_MID` | `#B53B3B` | Warm terracotta tile, banner, blossom |
| | `CORAL_LIGHT` | `#E57A5E` | Sunlit terracotta highlight, blush |
| **UI & Arcane** | `NAVY_UI` | `#192344` | Canonical UI window fill |
| | `INDIGO_ROYAL` | `#354B87` | Mage cloak, active UI selection fill |
| | `LAVENDER` | `#8B7EC8` | Arcane crystal, twilight glow |
| **Skin & Rose** | `SKIN_LIGHT` | `#F2C9A5` | Fair sunlit portrait/sprite skin |
| | `SKIN_WARM` | `#D2906C` | Warm tan skin & fair-skin shadow |
| | `SKIN_DEEP` | `#8B5442` | Rich deep skin tone / warm shadow |
| | `ROSE_CHEEK` | `#D87A70` | Warm cheek/lip accent, flower center |

### 3.2 Per-Biome Ramps (Strictly ≤8 Colors Each, Including `INK_DEEP`)

- **Meadow (`BI_MEADOW`):** `#141824`, `#1B3029`, `#2F5238`, `#527D3F`, `#82A84B`, `#B6CC62`, `#9C6839`, `#CF944E` (8 colors)
- **Forest (`BI_FOREST`):** `#141824`, `#142423`, `#1E3A2E`, `#2F5238`, `#3F7045`, `#65934C`, `#382520`, `#684430` (8 colors)
- **Amber (`BI_AMBER`):** `#141824`, `#2B1E28`, `#5E2D29`, `#8F462D`, `#BF6C34`, `#DE9B42`, `#F2C968`, `#527D3F` (8 colors)
- **Fen (`BI_FEN`):** `#141824`, `#182128`, `#273A37`, `#3C5848`, `#597553`, `#7F9462`, `#A3B27D`, `#213B4F` (8 colors)
- **Heath (`BI_HEATH`):** `#141824`, `#242438`, `#423C58`, `#655675`, `#8B7188`, `#586D4D`, `#7B9162`, `#A89D8A` (8 colors)
- **Frost (`BI_FROST`):** `#141824`, `#1C2A3D`, `#34506B`, `#5B839E`, `#93BDD0`, `#CCE3EC`, `#EEF6F9`, `#2F5238` (8 colors)
- **Dunes (`BI_DUNES`):** `#141824`, `#382520`, `#75472E`, `#A86C3C`, `#CF944E`, `#E8C074`, `#F5DFB0`, `#647D4A` (8 colors)
- **Dungeon (`DUNGEON`):** `#141824`, `#202738`, `#343F56`, `#4F5D75`, `#77859A`, `#A89D8A`, `#C98A2C`, `#F2C968` (8 colors)

---

## 4. Autotile, Quiet Terrain & Biome Completeness (A4)

### 4.1 Marching-Squares 16-Mask Autotile Layout

- `assets/tiles/landscape.png` (`256×1008`): 16 corner masks horizontally (`mask 0..15`, bit `0=NW, 1=NE, 2=SW, 3=SE`) × 21 surface types × 3 rows (variants or water animation frames).
- `assets/tiles/relief.png` (`256×80`): 16 masks × 5 elevation rim treatments (`0` temperate bank lip, `1` frost bank lip, `2` slate crag cliff face, `3` mesa sandstone cliff face, `4` alpine snow cliff face).
- **Quiet Terrain Rule:** Walkable interior tiles (`mask 15`) cap internal contrast to 2–3 muted neighboring steps so heroes, enemies, paths, and interactables pop effortlessly. Edge transitions happen cleanly inside `mask < 15` borders.

### 4.2 Biome Completeness Matrix (All 7 Overworld Biomes + Dungeon)

| Biome | Base | ≥3 Transitions | Path / Road | Water / Dry Eq. | 2 Elevation Treatments | ≥3 Large Props | ≥6 Small Props | Interaction Prop | Readable 1× Landmark |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| **Meadow** | `S_MEADOW` | `S_WOOD`, `S_BEACH`, `S_COBBLE`, `S_CRAG`, `S_PATH` | `S_PATH` + `bridge` | `S_WATER` + `S_DEEP` | `relief[0]` bank + `relief[2]` slate crag | `tree_0..2`, `willow`, `boulder` | `bush`, `bush_plain`, `rock_0..2`, `stump`, `tuft_0..2`, `flower_0..1`, `reeds`, `lilies` | `chest_closed`, `sign`, `well`, `cave_mouth` | `house_0..4`, `manor`, `castle`, `tower` |
| **Forest** | `S_WOOD` | `S_MEADOW`, `S_BEACH`, `S_CRAG`, `S_PATH` | `S_PATH` + `bridge` | `S_WATER` + `S_DEEP` | `relief[0]` bank + `relief[2]` slate crag | `pine_0..2`, `tree_0..2`, `boulder` | `mushroom`, `bush`, `bush_plain`, `stump`, `rock_0..2`, `tuft_0..2`, `flower_0..1` | `chest_closed`, `cave_mouth`, `monument` | `tower`, `ruin_arch`, `cave_mouth` |
| **Amber** | `S_AMBER` | `S_LEAVES`, `S_BEACH`, `S_CRAG`, `S_PATH` | `S_PATH` + `bridge` | `S_WATER` + `S_DEEP` | `relief[0]` bank + `relief[2]` slate crag | `amber_oak`, `copper_oak`, `boulder` | `mushroom`, `stump`, `bush`, `bush_plain`, `rock_0..2`, `tuft_0..2` | `chest_closed`, `monument` (Locket stones), `cave_mouth` | `monument` + `ruin_arch`, `pavilion` |
| **Fen** | `S_FEN` | `S_WOOD`, `S_WATER`, `S_CRAG`, `S_PATH` | `S_PATH` + `bridge` | `S_WATER` + `S_DEEP` | `relief[0]` mire bank + `relief[2]` mossy crag | `willow`, `dead_tree`, `boulder` | `reeds`, `lilies`, `mushroom`, `stump`, `bush_plain`, `rock_0..1`, `tuft_0` | `chest_closed`, `cauldron`, `cave_mouth` | `tower` (Watchtower keep), `ruin_arch` |
| **Heath** | `S_HEATH` | `S_WOOD`, `S_BEACH`, `S_CRAG`, `S_PATH` | `S_PATH` + `bridge` | `S_WATER` + `S_DEEP` | `relief[0]` shore + `relief[2]` granite crag | `pine_1..2`, `boulder`, `dead_tree` | `rock_0..2`, `bush_plain`, `stump`, `pebble`, `tuft_1..2` | `chest_closed`, `cave_mouth`, `monument` | `castle`, `tower`, `ruin_arch` |
| **Frost** | `S_SNOW` | `S_ICE`, `S_FROST_TRACK`, `S_SNOW_CRAG`, `S_WATER` | `S_FROST_TRACK` + `bridge` | `S_ICE` + glacial `S_WATER` | `relief[1]` ice bank + `relief[4]` snow cliff | `tree_snow`, `pine_2`, `boulder` | `rock_0`, `rock_2`, `crystal`, `stump`, `bush_plain`, `pebble` | `chest_closed`, `crystal`, `cave_mouth` | `castle`, `tower`, `monument` |
| **Dunes** | `S_SAND` | `S_RIPPLES`, `S_BEACH`, `S_MESA`, `S_PATH` | `S_PATH` | `S_RIPPLES` (dry ripple basin) + oasis `S_WATER` | `relief[0]` dune ridge + `relief[3]` mesa cliff | `yucca`, `boulder`, `dead_tree` | `rock_0..2`, `crystal`, `stump`, `pebble`, `tuft_2` | `chest_closed`, `urn`, `cave_mouth` | `ruin_arch`, `pavilion`, `monument` |
| **Dungeon** | `stone_floor` | `brick`, `brick_dark`, south wall trim | `stone_floor` corridors | torch/darkness pits | raised wall cap (`brick_dark` + top bevel) | `pillar`, `statue`, `longtable`, `shelf`, `chest_closed` | `barrel`, `crate`, `urn`, `torch`, `candle`, `lantern`, `keg`, `stool`, `mushroom`, `crystal` | `chest_closed`, `stair_up`, `stair_down` | Vault rooms (`statue` + `pillar` + `chest`) |

---

## 5. Silhouette & Readability Contract (A5)

- **1× Silhouette Verification:** Every hero, NPC, animal, monster, landmark, building, and item icon must be recognizable from its opaque alpha silhouette alone against both `#141824` (dark) and `#EFE4C6` (light).
- **Distinct Actor Shapes:**
  - **Aldric (Knight):** Crested silver helm, broad shoulder pauldrons, crimson cloak, kite shield.
  - **Iris (Mage):** Tall pointed indigo brimmed hat with gold band, flowing robe, crystal-tipped staff.
  - **Pip (Cleric):** Hooded teal-emerald cowl, cream tabard with gold trim, winged mace.
- **Walkable vs. Blocked Contrast:** Obstacles carry dark grounded base ellipses/contact shadows plus raised top caps caught by upper-left sunlight; walkable surfaces remain flat and calm.

---

## 6. Unified UI Language (A6)

- **Single Window Skin:** `assets/ui/window_frame.png` (`32×32`, `8px` nine-slice corners/edges, `16px` center) in deep navy (`#192344`) with 1px `#141824` outer rim, `#F2C968` upper-left sunlit gold bevel, `#C98A2C` lower-right antique gold shade, and `SUN_WHITE` corner rivets.
- **Prompt & Selection Language:** All prompts (`drawWindowBlue`), menu buttons, shop tabs, inn cards, battle slots, and toasts share the exact same crisp rectangular navy-and-gold pixel aesthetic (`drawPixelPanel`). Zero rounded vector pills (`DrawRectangleRounded`).
- **Type Scale:** `PressStart2P-Regular.ttf` strictly at `8px` (HUD, dialog, lists, labels), `16px` (section headers), and `24px` (Title & Game Over display), with `TEXTURE_FILTER_POINT` and `1px` `#141824` drop shadow.
- **Icon & Portrait Scale:** All `31` icons render at native `16×16`; all `28` portraits render at native `32×32`.

---

## 7. Asset Manifest & Automated Validation (A7)

- All runtime assets are cataloged in `ART_MANIFEST.md` with category, dimensions, pixel budget, generator source, and license.
- Run `python3 tools/validate_assets.py` (or `.venv/bin/python tools/validate_assets.py`) before committing any asset change. It verifies:
  1. Strict `16px` grid-multiple PNG dimensions and exact category size contracts.
  2. Zero duplicate runtime basenames or stems across `assets/`.
  3. Zero unreferenced or stray runtime assets (100% parity with `ART_MANIFEST.md` and `src/core.cpp`).
  4. Non-empty license & source notes for every runtime asset.
  5. Bounded pixel-art color budgets and zero non-pixel smooth blurs.
