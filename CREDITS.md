# Credits & licenses

## Soundtrack

The 13 in-game cues are recordings of compositions from [EasyRPG/OpenRTP](https://github.com/EasyRPG/RTP/tree/993d88cbc78c658d348bbfa74a3b424d393d27e5/Music). **Juan Manuel Nigretti** ([Good Game Audio](https://soundcloud.com/goodgameaudio)) wrote the twelve CC BY pieces in 2010; **[Someoneman](https://github.com/Someoneman)** wrote *Boss 3* (2022, CC0). See [the cue sheet](assets/audio/SCORE.txt) for every original title, its use, duration and license. The original MIDIs are included.

`tools/render_score.py` renders these pieces offline with the **GeneralUser GS** instrument bank by **[S. Christian Collins](https://www.schristiancollins.com/generaluser.php)**, adds stereo echo, prepares loop/release tails and balances levels. The bank has its own license; it is not redistributed. The score is not covered by the original SFX's CC0 dedication.

## Pixel art

**One dialect, locked (Workstream A1):** the game's pixel art is
[OpenRTP](https://github.com/EasyRPG/RTP) and repository pieces drawn in
the same dialect to extend it — see [docs/ART_BIBLE.md](docs/ART_BIBLE.md)
§0–§1. OpenRTP-derived runtime files must never be replaced by a
regenerated art set, and this attribution must never be removed or
rewritten as "original" art; `tools/validate_assets.py` enforces both in
CI. Per-file provenance for every shipped file is in
[ART_MANIFEST.md](ART_MANIFEST.md).

Source: [OpenRTP, revision `993d88c`](https://github.com/EasyRPG/RTP/tree/993d88cbc78c658d348bbfa74a3b424d393d27e5). The [vendored authors list](assets/source/openrtp/AUTHORS.md) gives licenses, attribution links and individual frame credits. CC BY here means [CC BY 4.0 International](https://creativecommons.org/licenses/by/4.0/); CC0 means [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/).

| Source | Credit and use in this game |
|---|---|
| `ChipSet/{Exterior,Interior,Dungeon,World}.png` | **[Jason Perry / finalbossblues](https://finalbossblues.itch.io/openrtp-tiles)** (CC0). Cropped 16 px textures and complete objects; palette swaps, transition masks, planted scenery, the town gate, and battlefield compositions. |
| `ChipSet/retro_Exterior.png` | **MrBeast** (CC BY), **Stephen Challener / Redshrike** (CC BY, commissioned by OpenGameArt), **Carl Olsson / surt** (CC0), **Ogrebane** (CC0), **Garrett Wessner / Gwes** (CC0), **Dmytro Kushnariov / jikmok** (CC0), **Tom Lemmens** (CC0) and **marianognu** (WTFPL); per-file links in the [vendored authors list](assets/source/openrtp/AUTHORS.md). The SNES-retro Exterior chipset. `tools/village_art.py` composes the walking-scale buildings (`house_0..4`, `shop`, `inn`, `temple`, `manor`, `smithy`, `castle`, `tower`) from its dithered shingle, half-timber, plaster and stone cells, palette-ramped to muted roof families; the `meadow`/`fen` battle backdrops embed a miniature `house_0`/`tower`. Since the ART_BIBLE 10.3 retro pass it also supplies the field ground textures (meadow/forest/amber/heath/fen/dunes, cobble, court, tile/stone floors), the flora (`tree_*`, `pine_*`, `willow`, `tree_snow`, `bush*`, `flower_*`) and the props re-sourced to its own 16-bit objects (`torch`, `campfire`, `fence*`, `noticeboard`, `barrel`, `longtable`), plus the `town_*` material atlases. |
| `ChipSet/retro_Dungeon.png` | Same multi-author SNES-retro pack as `retro_Exterior.png` (per-file links in [AUTHORS.md](assets/source/openrtp/AUTHORS.md)). Since the 10.3 retro pass it supplies the cave/dungeon floor and rock tiles, the dripstone and crystal formations, the field rocks (`rock_0/1`, `boulder`, `crystal`) and the rock/snow/ice ground textures, palette-ramped to the locked biome ramps. |
| `ChipSet/retro_World.png` | Same pack (`MrBeast`, `Redshrike`, `surt`, `Ogrebane`, `Gwes`, `jikmok`; see [AUTHORS.md](assets/source/openrtp/AUTHORS.md)). The SNES-retro world chipset: the overworld plate library (ART_BIBLE §9) and, since the 10.3 retro pass, the earth/heath/relief-crag texture cells behind the field terrain atlas. |
| `CharSet/Actor1.png` | **[Marina Navarro Travesset](https://muerteatartajo.blogspot.com)** (base) and **[VictorSena](https://vashmaker.blogspot.com/p/works.html)** (edits), CC BY. Color-keyed and reordered walking frames. |
| `CharSet/People1–5.png`, `Animal.png` | **VictorSena** (CC BY); **[Garakuta](http://garakutamaker.blog.fc2.com/)** (CC0 animal frames). People3 includes **Easychar** work by **R.C.O.**, **Jason Perry** and **Marina Navarro Travesset**; see upstream frame credits. The expanded generated-village roster, ranger, and cow/horse farm animals are cropped and reordered; the child was resized. |
| `CharSet/Monster1–3.png`, `Monster/Hornet.png` | **MerchantShade**, **Garakuta**, **Jetrotal**, **Jason Perry** (CC0 contributions); **VictorSena**, **R.C.O.** and **Orochii Zouveleki** (CC BY contributions). Field and side-view battle frames were selected, keyed and scaled, including previously unused bandit, goblin, reaper, faerie, drake, naga, shade, dread knight and archfiend slots. The Slime King is a crowned adaptation of MerchantShade's slime; tiger and porcupine use OpenRTP Animal frames. The wolf, bat, wisp and serpent are repository-drawn instead. |
| `CharSet/Object1.png`, `Object2.png` | **Tom Lemmens**, **Blarumyrran** (original chest, CC0) and **Verdant_Jack** (Object2, CC0). Cropped object frames and building doors. |
| `FaceSet/{Actor1,Actor2,People1,People2,Monster}.png` | **Garakuta**, CC0. Cropped character portraits. |
| `Battle/{Arrow,Blow,Sword1}.png` | **Someoneman**, CC0. Green-keyed, trimmed combat-effect frames. |
| `System/{System,Royal}.png` | **andwhyisit**, CC BY / OGA-BY. UI frame and gauge crops. |
| `Title/{Title1,Title2}.png` | **Francisco de la Peña**, CC0. Resized title plates. |
| `GameOver/Game Over.png` | **jetrotal**, CC0; **kou**, CC BY (koubit_001 font). Resized game-over plate. |
| `Panorama/{Dawn2,Night Sky1}.png`, `Picture/Cloud.png` | **Tom Lemmens**, CC BY / OGA-BY (panoramas); **DonMiguel**, CC0 (cloud). Resized backgrounds and trimmed cloud sprite. |

Additional original artwork and synthesized UI/combat sound effects in `tools/` are CC0 by the repository's authors. That dedication does **not** replace the licenses of any upstream material used in a composite.

The additional `cave_floor_*`, `cave_rock_*` tiles and `cave_stalagmite*`,
`cave_crystal*`, `cave_rock_pile` and `cave_boulder` sprites are
native-resolution crops of the SNES-retro `ChipSet/retro_Dungeon.png`
(originally cropped from Jason Perry's CC0 `ChipSet/Dungeon.png`; the 10.3
retro pass re-sourced them to the retro sheet at the same cells), extracted
by `build_cave_art()` in `tools/import_openrtp.py`. Existing OpenRTP art is
preserved. The sea tiles (`water_0/1`, `deep_water`) are a repository
composition in the same dialect (ordered dither + crest dashes on the
ART_BIBLE 2.4 water ramps, `tools/landscape_art.sea_frame`) — CC0.

## Procedural town and interior source crops

`tools/town_art.py` crops Jason Perry's CC0 `ChipSet/Exterior.png` and
`ChipSet/Interior.png` at native resolution and removes the colour key.
Since the 10.3 retro pass the town material atlases (`town_facade_atlas`,
`town_masonry_atlas`, `town_battlement_atlas`, `town_shingles_*`,
`town_wall_ivy`, `town_market_flowers_*`) crop the SNES-retro
`ChipSet/retro_Exterior.png` at the same cells instead, and the
`interior_wall_face_atlas` faces are palette-ramped into the muted 10.3
families. The
three `interior_door_*` pose strips are keyed crops of **[Tom Lemmens](https://github.com/lemtom)**'s
`CharSet/Object1.png` doors (CC BY 4.0 under the vendored `COPYING`; the
CC0 chest attribution on that sheet does not apply to the doors).

## Overworld source sheets (gathered for the Lufia 2-style world map)

Source sheets vendored under `assets/source/overworld/` for the upcoming
SNES-style overworld. Not yet adapted into runtime tiles.

- **ArMM1998 — Zelda-like tilesets and sprites**, CC0 1.0 (public domain).
  [OpenGameArt](https://opengameart.org/content/zelda-like-tilesets-and-sprites).
  Overworld, objects, characters, NPCs and cave sheets.
- **MrBeast — Worldmap/Overworld tileset**, CC-BY 3.0, commissioned by
  [OpenGameArt](https://opengameart.org/content/worldmapoverworld-tileset).
  Worldmap terrain, mountain ranges and animated water/coast strip.
  Attribution: "Worldmap/Overworld tileset" by MrBeast, commissioned by
  OpenGameArt.org, licensed under CC-BY 3.0.
- The primary overworld chipset remains the vendored OpenRTP
  `ChipSet/World.png` / `retro_World.png` (see above).

## Visual references, not game assets

- [Elcid](https://www.snesmaps.com/maps/Lufia2/01Elcid.html) and the [Lufia II map index](https://www.snesmaps.com/maps/Lufia2/Lufia2.html), *Lufia II: Rise of the Sinistrals*, via SNESMaps, plus the [PureZC "Lufia 2 Houses" tile rip](https://www.purezc.net/index.php?page=tiles&id=1368). The study was about **what a house looks like in a SNES JRPG**: steep pitched roofs of dithered/checker shingles in muted dusty clay/slate/ochre/moss/wine, half-timber cream-plaster walls on a stone foundation, tall two-pane pale-blue windows, a recessed timber door, dormer gables on the larger houses, and hanging gold-pictogram trade signs (no baked INN/PUB lettering). This drove the retro-dialect rebuild of `tools/village_art.py`. No map or graphics from that game are distributed here; the reference rips live only in the gitignored `image-search/` folder.
- [Potos Village](https://www.snesmaps.com/maps/SecretOfMana/002bPotosVillageMap.html) and the [Pandora–Potos–Water Palace path](https://www.snesmaps.com/maps/SecretOfMana/003bPandoraPotosWaterPalacePathMap.html), *Secret of Mana*, via SNESMaps. The study was about **walking scale and layout**: roads meet doors and cross streams at specific places; vegetation frames clearings; banks and cliff faces continue across tiles. No map or graphics from that game are distributed here.
- [NPS: Solution Caves](https://www.nps.gov/subjects/caves/solution-caves.htm) and [NPS: Cave Geology in Depth](https://www.nps.gov/grba/planyourvisit/cave-geology-in-depth.htm): water follows fractures, widens passages, and leaves dripstone and mineral formations. These informed the chamber/passage edges and cave dressing, not any copied graphics.
- [Generating layout for complex cave-like levels](https://yadda.icm.edu.pl/baztech/element/bwmeta1.element.baztech-1a626cf9-2d16-40a1-a6d0-a01bc4361c12/c/Izabella_Antoniuk17.pdf): an authored schematic backbone combined with naturalistic generation and explicit connectivity. Here the existing seeded BSP network supplies the stable backbone; the weathering pass adds irregularity while preserving objective locations and connected travel.
- [EasyAdditions: TileSheet #0000](https://kotatsuakira.itch.io/easyadd-tilesheet0000) is another CC0 OpenRTP-compatible resource identified during the survey, but is **not** included in the game.
- [Open RPG Fantasy Tilesets](https://finalbossblues.itch.io/openrtp-tiles) and the [RM2k/3 chipset template guide](https://www.deviantart.com/jojogape/art/RPG-Maker-2000-and-2003-Tileset-Template-v2-327699881). Distinguish overworld symbols, full exterior objects, interior furnishings and terrain blocks. The field atlas is a new corner-mask implementation, not an emulator of the original autotiler.

## Engine and font

- [raylib](https://github.com/raysan5/raylib) 5.5 — zlib/libpng license; fetched at build time.
- **Press Start 2P** by CodeMan38 — SIL Open Font License 1.1. See [OFL.txt](assets/fonts/OFL.txt).
