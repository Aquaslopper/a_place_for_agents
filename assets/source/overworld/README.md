# Overworld asset sources (step 1)

Free-to-use overworld / world-map tilesets gathered to build a Lufia 2-style
SNES overworld. These are **source sheets only** — no runtime tiles have been
generated from them yet. Cropping/adapting into `assets/tiles/` is a later step.

Reference for the target look: Lufia II (SNES) overworld — deep blue sea with
lighter dithered coast, bright grass, dark forest clumps, tan/grey mountain
chains, desert + snow patches, and tiny town/castle/cave icons.

## Packs

### 1. OpenRTP World chipset (already vendored) — PRIMARY
- Location: `assets/source/openrtp/ChipSet/World.png` and `retro_World.png`
- License: OpenRTP / EasyRTP (see repo `CREDITS.md`)
- Why: this is RPG Maker 2000's world-map chipset and the closest match to the
  Lufia 2 overworld. Contains sea + wave animation, grass, forest, mountain
  ranges, desert, snow, swamp, and small town / castle / cave / tower / bridge
  icons — exactly the SNES overworld vocabulary.

### 2. ArMM1998 — Zelda-like tilesets and sprites (CC0)
- Location: `armm1998_zelda_like/`
- Source: https://opengameart.org/content/zelda-like-tilesets-and-sprites
- License: CC0 1.0 (public domain)
- Files: Overworld.png (640x576), objects.png, character.png, NPC_test.png,
  cave.png, Inner.png, font.png, log.png
- Why: a full CC0 SNES-style overworld + object + character sheet. More
  Zelda:ALttP-flavoured than Lufia, but excellent supplementary props, buildings,
  characters and cave entrances.

### 3. MrBeast — Worldmap/Overworld tileset (CC-BY 3.0)
- Location: `mrbeast_worldmap/`
- Source: https://opengameart.org/content/worldmapoverworld-tileset
- License: CC-BY 3.0 (attribution required — see CREDITS.md in that folder)
- Files: tileset.png, mountains.png, water.png
- Why: a dedicated worldmap tileset. The mountain ranges and animated
  water/coast strip match the Lufia 2 overworld structure very closely.

## Previews
See `docs/overworld_assets/*_preview.png` for upscaled contact sheets of each
source, and the Lufia 2 reference in `image-search/`.
