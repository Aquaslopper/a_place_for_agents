# Credits & licenses

## OpenRTP pixel art

This game includes adapted artwork from [EasyRPG's OpenRTP](https://github.com/EasyRPG/RTP),
revision `993d88cbc78c658d348bbfa74a3b424d393d27e5`. The original source sheets are
in `assets/source/openrtp/`. `tools/import_openrtp.py` extracts, rearranges,
composes and scales portions to fit this game. See the upstream
[authors list](https://github.com/EasyRPG/RTP/blob/master/AUTHORS.md).

- `ChipSet/Exterior.png` and `ChipSet/World.png`: Jason Perry / finalbossblues,
  [Open RPG Fantasy Tilesets](https://finalbossblues.itch.io/openrtp-tiles),
  [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/).
- `CharSet/Actor1.png`: Marina Navarro Travesset (base) and VictorSena (edits),
  [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). Adapted here by
  removing the background and rearranging/cropping the animation frames.
  Sources: https://muerteatartajo.blogspot.com and
  https://vashmaker.blogspot.com/p/works.html.
- `FaceSet/Actor1.png`: Garakuta,
  [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/),
  http://garakutamaker.blog.fc2.com/. Cropped and resized for portraits.
- `CharSet/Monster1.png`, `Monster2.png`, `Monster3.png`: MerchantShade (slime,
  CC0); the source sheets also contain work by Garakuta, Jetrotal and Jason Perry
  (CC0), and R.C.O. ([CC BY 4.0](https://creativecommons.org/licenses/by/4.0/)).
  The game's whole bestiary — slime, imp, bat, hornet, wisp, wolf, skeleton,
  ghost, orc, serpent, golem, night rider and the Slime King — is cropped, scaled
  and, for battle, enlarged from these sheets. Sources:
  https://merchant-shade.itch.io/, http://garakutamaker.blog.fc2.com/,
  https://github.com/jetrotal and https://finalbossblues.itch.io/; R.C.O.
  credited in the upstream authors list.
- `CharSet/People1-5.png`, `CharSet/Animal.png`, `CharSet/Object1.png`,
  `CharSet/Object2.png`: village folk, animals and the object set, same upstream
  authors and licences as above; cropped and re-arranged into walking sheets and
  objects (chests, barrels, kegs, ladders, fences).
- `ChipSet/Interior.png`, `ChipSet/Dungeon.png`, `System.png`, `Royal.png`,
  `Title/Title1.png`, `Title/Title2.png`, `GameOver/Game Over.png`,
  `Panorama/Dawn2.png`, `Panorama/Night Sky1.png`, `Picture/Cloud.png`:
  Jason Perry / finalbossblues and the EasyRTP contributors listed upstream;
  `CC0 1.0` unless the upstream authors list says otherwise. Used for interiors,
  the window skin, the title backdrop, the game-over plate and drifting clouds.

The remaining icons, effects and fallback art are generated in-repository by
`tools/build_art.py` and `tools/art_*.py`. Music and sound effects are generated
by `tools/gen_assets.py`. Generated original assets are CC0 by the repository's
authors. The OpenRTP actor sheet and its derivatives retain the CC BY 4.0 terms
above; this CC0 statement does not apply to them.

## Engine
- [raylib](https://github.com/raysan5/raylib) 5.5 — zlib/libpng license;
  fetched at build time.

## Font
- **Press Start 2P** by CodeMan38 — SIL Open Font License 1.1.
  Bundled at `assets/fonts/PressStart2P-Regular.ttf`; see `assets/fonts/OFL.txt`.

### World-generation references

The [Open RPG Fantasy Tilesets page](https://finalbossblues.itch.io/openrtp-tiles)
identifies the Exterior and World sheets as RPG Maker 2000/2003 chipsets with a
16×16 tilebase. The [chipset template guide](https://www.deviantart.com/jojogape/art/RPG-Maker-2000-and-2003-Tileset-Template-v2-327699881)
describes which regions contain animated water and grouped terrain, rather than
independent repeatable tiles. The importer uses interior fills for repeating
surfaces and whole upper-layer objects for props; it does not implement the
original engine's full autotile system. Vegetation distribution uses clustered
noise and local spacing, following the general biome-aware placement principles
in [AutoBiomes](https://link.springer.com/article/10.1007/s00371-020-01920-7).
