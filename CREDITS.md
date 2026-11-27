# Credits & Asset Provenance

All runtime assets are inventoried in `ART_MANIFEST.md` and validated by `tools/validate_assets.py`.

## Visual Art & SFX (CC0 1.0)

- **16-bit Pixel Art (`assets/tiles/`, `assets/sprites/`, `assets/icons/`, `assets/fx/`, `assets/backdrops/`, `assets/ui/`, `assets/title_bg.png`):**
  100% original repository-authored pixel art generated via the unified `tools/build_art.py` pipeline under `tools/snes.py`'s 32-color Valley Hearth palette and ≤8-color biome ramps.
  Dedicated to the public domain under **CC0 1.0 Universal**: https://creativecommons.org/publicdomain/zero/1.0/
- **Sound Effects (`assets/audio/{cursor,confirm,cancel,sword,hit,spell,heal,coin,potion,door,encounter,levelup,flee}.ogg`):**
  Synthesized offline by `tools/gen_assets.py`. Dedicated under **CC0 1.0 Universal**.

## Music Score (`assets/audio/*.ogg`)

All 13 music cues are offline Ogg Vorbis recordings rendered by `tools/render_score.py` using the GeneralUser GS v1.471 SoundFont (`GeneralUser GS SoundFont License v2.0`, S. Christian Collins). See `assets/audio/SCORE.txt` for full cue mappings and playback details.

- **12 Cues (`title`, `village`, `field`, `wood`, `fen`, `pass`, `dunes`, `hearth`, `chapel`, `battle`, `victory`, `gameover`):**
  Original MIDIs (`Opening 2`, `Town 1`, `Field 1`, `Ending 2`, `Dungeon 1`, `Castle 1`, `Vehicle 1`, `Sorrow`, `Church`, `Battle 1`, `JFanfare 1`, `Game Over 1`) by **Juan Manuel Nigretti** (2010, EasyRPG OpenRTP `993d88c`), licensed under **CC BY 4.0**: https://creativecommons.org/licenses/by/4.0/
- **Boss Cue (`boss.ogg`):**
  Original MIDI (`Boss 3`) by **Someoneman** (2022, EasyRPG OpenRTP `993d88c`), dedicated under **CC0 1.0 Universal**.

## Typography

- **Press Start 2P (`assets/fonts/PressStart2P-Regular.ttf`):**
  By Cody Boisclair (`cody@zone38.net`), licensed under the **SIL Open Font License 1.1** (`assets/fonts/OFL.txt`).

## Engine & Libraries

- **raylib 5.5:** Copyright (c) Ramon Santamaria (`raysan5`), licensed under **zlib/libpng**.
