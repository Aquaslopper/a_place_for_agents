# Growth chronicle

## 2026-11-28 — the world, actually

The overworld was an infinite noise field with a lattice of guaranteed places
bolted on. It is now a **designed-geography generator** in
`src/overworld_gen.cpp`: 320×320-tile continents built from authored land
blobs, tapered wavy mountain ranges, stylised river splines, lakes, biome
fields and 2×2 forest copses. The start region keeps its authored pins
(Elmswick, the Castle of Elm, Hollow Hill, the Standing Stones, the Old
Watchtower, Oakford, Saltmere, Millbrook, Briarwick, Gull Rock, Eastwatch,
Thornbury, Fenwick) plus the boat-only Tidewell isles; other continents get
seeded hamlets, dungeons and a lake.

Roads are A*-routed between sites with a turn penalty, so they run in country
straights instead of one-tile staircases; water becomes a bridge and a range
becomes a pass. Site pads are round clearings and **preserve the road**, so a
road enters a town gate instead of stopping two tiles short. Rivers are
stroked segment-by-segment (point-sampled splines left dry gaps and produced
zero bridges); river banks are grass, not beach; beaches only face the sea.
Mountain ranges are 2.15 half-width, so they read as ridges rather than a
wall of bricks.

The runtime field renderer, collision, encounters, music mapping, the [M]
travelling map and the Journal's **Survey** page all read `owSampleTile` /
`owSitesInRect` from this one generator. The Survey page was rewritten: it
used to draw the retired noise field and disagreed with the ground under the
party's feet. Saves are v7; v4–v6 saves migrate and wake at Elmswick because
their world coordinates no longer describe the new atlas.

Verified headlessly (no GPU): `tools/verify_lufia_world.cpp` (homeland,
54 continents across 6 seeds, all sites walkable and reachable on foot,
Tidewell isles boat-only, roads/bridges/passes, outward continents, 106 ms
cold continent), `tools/verify_world.py`, `tools/link_check.py` and
`tools/validate_assets.py`. Previews in `docs/screenshots/` are rendered by
`tools/preview_world.py` from the real tile atlas and the real dump, not a
mock-up.
