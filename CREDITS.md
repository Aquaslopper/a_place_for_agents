# Credits & licenses

## Soundtrack

The 13 in-game cues are recordings of compositions from [EasyRPG/OpenRTP](https://github.com/EasyRPG/RTP/tree/993d88cbc78c658d348bbfa74a3b424d393d27e5/Music). **Juan Manuel Nigretti** ([Good Game Audio](https://soundcloud.com/goodgameaudio)) wrote the twelve CC BY pieces in 2010; **[Someoneman](https://github.com/Someoneman)** wrote *Boss 3* (2022, CC0). See [the cue sheet](assets/audio/SCORE.txt) for every original title, its use, duration and license. The original MIDIs are included.

`tools/render_score.py` renders these pieces offline with the **GeneralUser GS** instrument bank by **[S. Christian Collins](https://www.schristiancollins.com/generaluser.php)**, adds stereo echo, prepares loop/release tails and balances levels. The bank has its own license; it is not redistributed. The score is not covered by the original SFX's CC0 dedication.

## Pixel art

Source: [OpenRTP, revision `993d88c`](https://github.com/EasyRPG/RTP/tree/993d88cbc78c658d348bbfa74a3b424d393d27e5). The [vendored authors list](assets/source/openrtp/AUTHORS.md) gives licenses, attribution links and individual frame credits. CC BY here means [CC BY 4.0 International](https://creativecommons.org/licenses/by/4.0/); CC0 means [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/).

| Source | Credit and use in this game |
|---|---|
| `ChipSet/{Exterior,Interior,Dungeon,World}.png` | **[Jason Perry / finalbossblues](https://finalbossblues.itch.io/openrtp-tiles)** (CC0). Cropped 16 px textures and complete objects; palette swaps, transition masks, planted scenery, new full-size building facades and battlefield compositions. |
| `CharSet/Actor1.png` | **[Marina Navarro Travesset](https://muerteatartajo.blogspot.com)** (base) and **[VictorSena](https://vashmaker.blogspot.com/p/works.html)** (edits), CC BY. Color-keyed and reordered walking frames. |
| `CharSet/People1–5.png`, `Animal.png` | **VictorSena** (CC BY); **[Garakuta](http://garakutamaker.blog.fc2.com/)** (CC0 animal frames). People3 includes **Easychar** work by **R.C.O.**, **Jason Perry** and **Marina Navarro Travesset**; see upstream frame credits. Villagers, keepers and animals were cropped and reordered; the child was resized. |
| `CharSet/Monster1–3.png`, `Monster/Hornet.png` | **MerchantShade**, **Garakuta**, **Jetrotal**, **Jason Perry** (CC0 contributions); **VictorSena**, **R.C.O.** and **Orochii Zouveleki** (CC BY contributions). Field and battle frames were selected, keyed and scaled; the Slime King is a crowned adaptation of MerchantShade's slime. The wolf, bat, wisp and serpent are repository-drawn instead. |
| `CharSet/Object1.png`, `Object2.png` | **Tom Lemmens**, **Blarumyrran** (original chest, CC0) and **Verdant_Jack** (Object2, CC0). Cropped object frames and building doors. |
| `FaceSet/{Actor1,Actor2,People1,People2,Monster}.png` | **Garakuta**, CC0. Cropped character portraits. |
| `Battle/{Arrow,Blow,Sword1}.png` | **Someoneman**, CC0. Green-keyed, trimmed combat-effect frames. |
| `System/{System,Royal}.png` | **andwhyisit**, CC BY / OGA-BY. UI frame and gauge crops. |
| `Title/{Title1,Title2}.png` | **Francisco de la Peña**, CC0. Resized title plates. |
| `GameOver/Game Over.png` | **jetrotal**, CC0; **kou**, CC BY (koubit_001 font). Resized game-over plate. |
| `Panorama/{Dawn2,Night Sky1}.png`, `Picture/Cloud.png` | **Tom Lemmens**, CC BY / OGA-BY (panoramas); **DonMiguel**, CC0 (cloud). Resized backgrounds and trimmed cloud sprite. |

Additional original artwork and synthesized UI/combat sound effects in `tools/` are CC0 by the repository's authors. That dedication does **not** replace the licenses of any upstream material used in a composite.

## Visual references, not game assets

- [Potos Village](https://www.snesmaps.com/maps/SecretOfMana/002bPotosVillageMap.html) and the [Pandora–Potos–Water Palace path](https://www.snesmaps.com/maps/SecretOfMana/003bPandoraPotosWaterPalacePathMap.html), *Secret of Mana*, via SNESMaps. The study was about **walking scale and layout**: roads meet doors and cross streams at specific places; vegetation frames clearings; banks and cliff faces continue across tiles. No map or graphics from that game are distributed here.
- [Open RPG Fantasy Tilesets](https://finalbossblues.itch.io/openrtp-tiles) and the [RM2k/3 chipset template guide](https://www.deviantart.com/jojogape/art/RPG-Maker-2000-and-2003-Tileset-Template-v2-327699881). Distinguish overworld symbols, full exterior objects, interior furnishings and terrain blocks. The field atlas is a new corner-mask implementation, not an emulator of the original autotiler.

## Engine and font

- [raylib](https://github.com/raysan5/raylib) 5.5 — zlib/libpng license; fetched at build time.
- **Press Start 2P** by CodeMan38 — SIL Open Font License 1.1. See [OFL.txt](assets/fonts/OFL.txt).
