# Handoff: replace the blob with a procedural JRPG map world! 
Current status: the repo is a lie, this is not an snes jrpg.

**Mission:** Build the actual world structure an actual SNES JRPG world: many distinct, connected, procedurally generated SNES-style JRPG maps. This is a structural replacement task, not another pass over the existing continuous terrain field.

## Required outcome

- The game consists of discrete maps with their own layouts and identities—towns, forests, routes, deserts, caves, cities, fields, regions,...etc and other places—not one global noise field presented through camera-sized “pieces.”
- Maps are generated deterministically from stable map identities/seeds. Neighboring maps connect through real, compatible exits and transitions; crossing one loads the connected map and preserves the player’s place and game state.
- Exploration continues by generating or resolving new maps as the player travels. There is no artificial outer wall, finite continent edge, or progression gate that exists just to stop outward exploration. Local obstacles and story content are not fine, queest are ok. No Limitation.
- The system is real gameplay, not a diagram, atlas-only view, visual overlay, or renamed chunk system. Integrate generation, rendering, collision, movement, encounters, NPCs/sites, saving/loading, and transitions so the result is playable end to end:
The Core Gameplay Loop:
Town Exploration & Preparation.
You arrive at a new settlement to advance the plot by talking to NPCs, resting at inns to restore HP/MP, and spending gold on upgraded weapons, armor, and healing items.
Overworld Traversal. 
Exiting a town places your party on a scaled-down world map—often featuring Mode 7 graphical effects—where you navigate between locations while enduring random enemy encounters (though games like Chrono Trigger display visible enemies directly on the field). 
Dungeon Crawling. 
You enter multi-floor caves, towers, or ruins to find treasure chests, solve environmental puzzles (such as in Lufia II), and manage dwindling health and magic reserves. 
Boss Battles. 
Reaching the end of a dungeon triggers a major boss fight that tests your party setup and resource management, rewarding you with experience points (EXP), currency, and key story items to unlock the next region. 


## Do not repeat the failed approach

Do not treat terrain-noise edits, removal of coast/pass checks, a travel-map change, or the existing 44×26 camera pieces as the requested map system. The current source still has the blob architecture; these are not independent authored/generated maps with explicit connections. Remove it. Do not claim the world is unbounded or the replacement works until it has been built and exercised across several map transitions.

How SNES Maps Were Built
Tilemaps and Reusable Assets. 
Instead of drawing unique artwork for an entire level, developers built a small palette of 8x8 or 16x16 pixel tiles (grass, stone, walls, water) and arranged them on a grid to quickly assemble towns and dungeons without eating up cartridge memory.
Screen-by-Screen & Door Transitions.
Dungeons and towns were divided into discrete, bite-sized rooms connected by doorways, staircases, or screen-edge fadeouts, allowing the console to clear and load new tile data in a split second.
The Illusion of Scale via the Overworld. 
World maps used miniature sprites and compressed distances so a 30-second walk between two towns felt like a multi-day journey across a continent.
Natural Chokepoints. 
Mountains, rivers, broken bridges, and oceans acted as strict boundaries that kept these small maps tightly curated, gradually opening up as you acquired ships, airships, or key items.


## Completion bar

Deliver a coherent, playable change. Build the game, check online sources for resources or knowledge on actual SNES map systems and world build then verify actual traversal across map boundaries in more than one direction, deterministic regeneration of visited maps, valid exits on both sides of every transition, and continued outward exploration without a global cutoff, make sure the maps do procedual generate and do not build the map on your own. Check that save/load and existing core play still work. Update the player-facing docs so they describe the implemented system, not the old deleted blob.

## Handoff state

The prior partial source edits were reverted. The map-system replacement (the generator) is NOT yet implemented or validated. However, the groundwork steps 1–3 below ARE done and persisted. This human-directed mission supersedes the old seed mission in `docs/NEXT_AGENT_PROMPT.md` where it says to build on the blob; read the repository for implementation details, but do not reinterpret this request as another blob-map polish task.

---

## Work completed (steps 1–3) — build on this, do not redo it

**Scope discipline:** Stay focused on the requested deliverable. Complete it end-to-end, including necessary validation. Avoid adjacent tasks and lengthy speculative planning, but do not split bounded work into micro-steps or pause for approval between them. Follow necessary technical dependencies rather than an artificial one-step-at-a-time limit. Steps 1–3 are DONE and persisted in the repo; the procedural generator itself (step 4+) is NOT yet written. Everything here is reproducible from the committed files. Read this whole section before touching anything.

### Step 1 — Overworld assets obtained (source-only, vendored, credited)

Goal: gather free-to-use art that reads like the Lufia II overworld. None of this is runtime art yet — these are SOURCE sheets the importer/generator crops from.

| Pack | Source | License | Vendored path |
|---|---|---|---|
| OpenRTP World chipsets | already in repo | CC0 (`World.png`, Jason Perry / finalbossblues 2022) · CC-BY+CC0 mix (`retro_World.png`, 2014 composite: MrBeast/Redshrike/surt/Ogrebane/Gwes/jikmok) | `assets/source/openrtp/ChipSet/World.png`, `retro_World.png` — both 480×256 |
| MrBeast — Worldmap/Overworld | github `castle-engine/castle-engine` mirror `examples/tiled/map_viewer/data/maps/animations/worldmapoverworld-tileset/` (orig OGA `worldmapoverworld-tileset`) | CC-BY 3.0 | `assets/source/overworld/mrbeast_worldmap/` — tileset.png 256×336, mountains.png 192×160, water.png 480×48 |
| ArMM1998 — Zelda-like | github `ggdaltoso/gbadev` `assets/zelda-like/` (orig OGA `zelda-like-tilesets-and-sprites`) | CC0 | `assets/source/overworld/armm1998_zelda_like/` — Overworld.png 640×576, objects/character/NPC_test/cave/Inner/font/log |

How they were fetched (sandbox outbound is limited to github / codeload / api.github / npm / pypi hosts):
- ArMM1998: `curl codeload.github.com/ggdaltoso/gbadev/tar.gz/...` → extract → `cp` the 8 PNGs + CREDITS.md.
- MrBeast: GitHub contents API (`api.github.com/repos/castle-engine/castle-engine/contents/...`) returning base64 PNGs, decoded with urllib.
- Previews / labelled zoom grids rendered with Pillow into `docs/overworld_assets/`.

Required follow-ups that were done (REPEAT after any asset change):
- `CREDITS.md` — "Overworld source sheets" section added.
- `assets/source/overworld/README.md` plus a `DESCRIPTION.md` beside EACH pack (source URL, author, license, publisher description, usage notes). The human insisted the pack descriptions be saved, not just the PNGs.
- `python3 tools/generate_manifest.py` → `ART_MANIFEST.md` (now **424 entries**).
- `python3 tools/validate_assets.py` → OK (**344 runtime / 424 total**). Adding any file under `assets/` breaks the validator until the manifest is regenerated.

### Step 2 — Art bible §9 "Overworld tile registry" written

`docs/ART_BIBLE.md` §9 is the authoritative tile knowledge. It was grounded in:
- the RM2k/2003 chipset spec (RPG Maker Forums "Chipset composition" thread 41286, Steam guide 277343853, jojogape DeviantArt template d5f3quh): 480×256 = 30×16 cells, lower/upper layers, 3×4 autotiles, the special water boundary region;
- the pack source-page descriptions (OGA) saved as DESCRIPTION.md;
- viewed zoom crops of both World sheets — NOT pixel-guessing (the human rejected guessing twice).

Sections: §9.1 mechanics · §9.2 the two chipsets · §9.3 the shared 30×16 region map · §9.4 terrain autotile blocks (exact col×row) · §9.5 site/landmark icon cells · §9.6 supplementary packs · §9.7 the `OW_*` semantic vocabulary the generator + renderer will share. **READ §9 before touching tiles; do not re-derive coordinates.**

CORRECTION made this session: §9.1 originally listed only the pink key `(255,103,139)`. The retro sheet actually keys with **magenta `(224,103,191)`**. §9.1 now lists both. Use the right key per sheet or icons get pink/magenta boxes.

### Step 3 — The 3×3 plate library (100 plates) built

Reference (LEARN-ONLY, never shipped): the Lufia II overworld by Rick Bruns, 4096×4096, at
<https://www.snesmaps.com/maps/Lufia2/00Lufia2OverworldBG.html> (background-only) and
`00Lufia2Overworld.html` (labelled with site names). The human cut 4 example 3×3 crops to show the format. We study composition only; **all plate art comes from `retro_World.png`.** Do NOT cut the Lufia map into runtime art.

These reference rips are copyrighted commercial game art, so they are **gitignored, not committed** (`image-search/`, plus the side-by-side `comparison_lufia2_vs_openrtp.png`). Re-fetch from the URLs above if you need to study them again; do not add them to the repo or to runtime assets.

Builder: **`tools/build_overworld_plates.py`** (run from repo root). Output:
- `docs/overworld_assets/plates/<name>.png` — 100 plates, 48×48 each (3×3 of 16×16).
- `docs/overworld_assets/plates/index.json` — canonical name list (100). The folder is kept in sync with this index (stale plates are deleted on each run).
- `docs/overworld_assets/plates_terrain.png` (75 terrain) and `plates_sites.png` (25 site/special) contact sheets.

Method (full detail in the script docstring):
1. `cell(c,r)` crops a 16×16 cell and keys out magenta `(224,103,191)` ±12.
2. `island(c0,r0)` = the 3×3 below the preview row of a standard 3×4 autotile = a coherent terrain island on grass. Works for forest(6,4), sand(9,0), snow(9,4), dirt(6,0), mountain(3,12).
3. `render_mask(mask, island, isolated)` composes any 3×3 binary shape (full / pond / top / bottom / left / right / halves / corners / channels / cross) by choosing per-cell edge/corner tiles from the island; off-plate neighbours clamp to the edge so terrain continues across plate borders.
4. Site plates = grass base + upper-layer icon (§9.5 coords) stamped foot-anchored.
5. Special transporter plates: `bridge_over_water`, `cave_in_mountain`, `gate_pass`.

Composition lessons taken from Lufia (apply when authoring more plates):
- a plate is 3×3; the centre often holds the feature (forest clump, bridge, tree), the 8 neighbours blend to the edges;
- each SIDE carries one terrain type that must equal the neighbouring plate's opposite side (the Carcassonne rule);
- transitions (grass↔water/sand/forest) happen INSIDE a plate via autotile edges so the border itself is a clean single terrain;
- rare site icons sit as a single entry point.

**FIXED — WATER PLATES (was: "water plates are wrong")**
- The old `water_*` plates rendered as dark bands + white specks: the retro sheet's water region (cols 0–5, rows 0–7) is the RM2k *boundary* autotile, and its open-sea tile is a flat `(28,41,46)` black that does not read as sea.
- Fix, shipped: the water plates no longer crop water from the sheet. They use the water the game already ships — `assets/tiles/water_0.png` (the OpenRTP-derived field water from `landscape_art.py`, ramp `#377e91 #3c8497 #438a9a #4d909e #5698a5`), with a 3px `beach.png` lip on every land-facing side. `deep_water.png` is there for open-sea plates.
- `render_mask` is unchanged and `water_*` keep their names and edge labels, so `tools/build_overworld_layout.py` needs no change.

### Step 4+ — what the next agent builds (the generator)

The human's design (dictated, not yet implemented):
- **Carcassonne-style edge-matching:** each 3×3 plate has 4 labelled sides (grass / dirt-road / water / mountain / forest …). A plate may be placed only where touching sides match, so roads/coasts/forests continue cleanly (no "spaghetti"). Seeded RNG picks among the VALID plates; same seed → same world. This is constraint-based tiling / Wave Function Collapse.
- **Rare special plates** (town, dungeon, tower, deep forest) with extra placement rules: town within N tiles of start, dungeon only beside mountain, minimum town spacing, etc.
- **Special plates are TRANSPORTERS:** the icon is an entry point into the map beneath (town plate → town map; exit → back to the overworld at the same plate).
- **NEVER-BLOCKED rule:** the player must never be trapped by a mountain wall. Whenever a walkable path cannot escape, place a bridge or tunnel plate that transports the player to the map beneath and out the other side.
- Then the full mission above: town → overworld → multi-floor dungeons → boss loop, remove the blob, build with the existing CMake, exercise several transitions + save/load, update player-facing docs.

Parameters to pin down when building: the edge-type alphabet, the plate library (these 100 + more), the matching/transition table, special-plate rarity + spacing rules.

### Reproduction / gotchas checklist
- Run builder: `python3 tools/build_overworld_plates.py` (from repo root). Needs Pillow + numpy (`pip install --break-system-packages pillow numpy`).
- After ANY asset change: `python3 tools/generate_manifest.py` then `python3 tools/validate_assets.py`.
- Transparency keys: retro = `(224,103,191)`; World.png = `(255,103,139)`; cyan `(0,147,146)` also appears.
- Build is currently BROKEN as checked out: `CMakeLists.txt` references missing `src/world.cpp`, `src/overworld.cpp`, `src/world.hpp`. That is a later step.
- `ART_BIBLE.md` §7 still says "384 files" (stale; actual 424) — cosmetic.
- Do NOT cut the Lufia map into runtime art; it is reference only.
- Do NOT repeat the failed blob approach (see top of this file).
