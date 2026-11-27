# Worldmap/Overworld tileset — accompanying description

Source page: https://opengameart.org/content/worldmapoverworld-tileset
Author: MrBeast (commissioned by OpenGameArt.org). License: CC-BY 3.0.

## Publisher description
"Yet another tileset commissioned by OGA. 16x16 tileset for a worldmap."

## Usage notes gathered from the source page comments
- **mountains.png** is arranged as a **3x3 grid** of mountain cells. Use the
  **middle tile** of each 3x3 block as the repeating mountain cell (author
  Kemono: "The mountains are arranged in a 3x3 grid, use the middle tile and
  it should come out fine."). Grey and snow-capped variants are provided.
- **water.png** is a **32x32** animated water/coast strip (larger than 16x16 so
  the water is less repetitive). It ships **15 frames**; loop it 1→15 then
  15→1 (ping-pong) to avoid a jump, or treat it as a 16-frame cycle.
- **tileset.png** holds the 16x16 worldmap terrain: grass / sand / snow coast
  transitions, forest, roads, bridges, and town / castle / tower icons.

## Files
- tileset.png  (256x336)
- mountains.png (192x160) — 3x3 mountain autotile blocks
- water.png    (480x48)  — animated water/coast

## Provenance note
This same MrBeast worldmap art is already vendored inside OpenRTP
`ChipSet/retro_World.png` (2014), whose AUTHORS.md credits MrBeast [CC-BY],
Stephen Challener (Redshrike) [CC-BY], Carl Olsson (surt) [CC0], Ogrebane [CC0],
Garrett Wessner (Gwes) [CC0], Dmytro Kushnariov (jikmok) [CC0].
