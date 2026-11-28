# Growth chronicle

## The Honest Handoff — 2026-11-28 — PR (this branch)

**Grew:** Nothing visible, and deliberately so. This pass stopped building and
established what is actually there, because the previous three passes each
reported a win while the thing the human kept asking for — a Lufia II *world* —
did not exist. Two documents now say so out loud:

- **[`docs/HANDOFF_WORLD_STRUCTURE.md`](docs/HANDOFF_WORLD_STRUCTURE.md)** is the
  new technical brief. It inventories the world systems against the code: the
  plate overworld in `world.cpp` is real but **dead** (`worldInit()` is never
  called), the field the player actually walks is `sampleGround()`'s infinite
  noise, **towns do not exist** (entering a town icon calls `enterBuilding(0)`
  and drops you in one 15×11 shop room), and the three seeded generators that
  *do* have the right invariants are Python-only and never integrated. It then
  specifies the target: a separate world atlas and bounded local maps, joined by
  `MapId` / `MapDef` / reciprocal ports, with `mapGenerate()` pure, plus a
  seven-phase plan and seven acceptance tests.
- **[`docs/NEXT_AGENT_PROMPT.md`](docs/NEXT_AGENT_PROMPT.md)** now exists. The
  README has pointed at it for several passes and it was a 404.

**Preserved:** Everything that works. `dungeon.cpp` is the one system built to
the stated spec (seeded BSP + cellular weathering, multi-floor, chests, vault,
guardians) and the handoff tells the next agent to wire it up, not rewrite it.
Battles, shops, dialogue, sailing, the score and save/load are untouched. No
runtime asset changed; `validate_assets.py` passes. The palette diagnosis
([`docs/VISUAL_AUDIT.md`](docs/VISUAL_AUDIT.md)) stands as the art spec for the
final phase.

**Corrected:** `HANDOFF_PLATE_LAYOUT_GENERATOR.md` told agents "the build is
expected to be broken; that is a later step" and to never edit `src/`. Both were
true for one bounded tool pass and are now actively harmful; they are marked
superseded. `HANDOFF_MAP_GENERATORS.md`'s line deferring C++ integration "until
the Python generators for all planned domains are complete" is withdrawn — that
deferral is precisely why the game still has no towns.

**Seed for the next agent:** Build the world, in the order the handoff gives:
a linking build, then `MapId`/`MapDef`/ports, then the atlas (revive the plate
world and call it), then towns generated in C++, then routes, then wire the
dungeons, and **then** one art pass. Do not paint the noise field again.

## The Retro Pass — 2026-11-28 — PR (this branch)

**Grew:** The world outside the houses finally wears the same decade as the
houses. PR #36 rebuilt the walking-scale buildings in the SNES / Lufia II
dialect from the retro chipsets — but the ground under them, the trees above
them and the props between them were still crops of the *modern* OpenRTP
sheets: smooth, low-contrast, a modern RPG-Maker tileset standing inside a
16-bit town. This pass re-sources the rest of the visible world to the
**SNES-retro chipsets** (`retro_Exterior`, `retro_Dungeon`, `retro_World`),
which share the modern sheets' 30×16 layout and were mapped cell by cell
before a single crop moved:

- **The ground has texture now.** Every surface behind the field atlas —
  meadow, forest floor, amber, heath, fen, dunes, leaves, cobble, court,
  tile/stone floors, rock, snow, ice and the relief cut banks — reads its
  pixels from the retro sheets at the same cells, so the locked §2.4 biome
  ramps now sit on 16-bit speckle and dither instead of flat modern fills.
  The sea is composed outright: ordered-dither bodies with crest dashes that
  march across the three water phases, on the same water ramps as before.
- **Flora and props are the retro sheets' own objects** where a verified
  counterpart exists: the round canopy trees and conifers, bushes, scattered
  blossoms, torch, campfire, picket fences, noticeboard, barrel, long table,
  the dripstone and crystal formations and the field rocks. Props with no
  trustworthy retro cell keep their silhouette and are palette-ramped into
  the §10.2 building families (`well`, `statue`, `pillar`, `sign`, `bench`,
  `ladder`, `mailbox`, `stump`, `dead_tree`, `urn`) instead of being redrawn.
- **Cave art and town materials moved too:** cave floors/rocks/formations now
  crop `retro_Dungeon`, the town facade/masonry/battlement/shingle/ivy
  atlases crop `retro_Exterior`, and the six interior wall faces are ramped
  into muted families (wine brick, cream, pale stone, slate, rubble, blue
  brick) so rooms stop shouting purple at the player.

**Preserved:** The §0 guardrails in spirit and letter — every changed pixel
passes through the tools (crop / palette-ramp of the vendored sheets), the
OpenRTP attribution now *extends* to the retro chipsets' multi-author CC BY /
CC0 mix in CREDITS.md and the manifest, and nothing was relabelled original.
All sprite sizes and logical footprints (§10.1) are unchanged; the two
trimmed `flower_*` overlays grew to 16×14 with their new source and the
validator's documented allow-list says so. `validate_assets.py` passes (391
runtime / 471 total), `town_art.py --check` passes, the town/interior/overworld
plate catalogs and all nine seeded layout previews regenerate clean with zero
side-match violations, overlaps or unreachable doors.

**Second wave, same branch:** the pass then swallowed the rest. The interior
furniture, floor tiles and every remaining town/interior crop (signs,
banners, turrets, gables, windows, paintings, shop displays, piano, throne,
ritual circle) keep their silhouettes but run through `mute()` — colours
bucketed by hue and each bucket ramped onto its own muted family, so a red
pillow stays red but dusty and the wood goes to the same timber ramp the
houses use. The battle backdrops' smooth sky and ground blends became
quantised Bayer-dither bands, the way 16-bit battle backgrounds shade a
sky.

**Seed for the next agent:** With terrain, flora, props, cave art, town
materials, interiors and backdrops in dialect, the remaining modern tells
are structural, not chromatic: the town `banner`/`turret`/`gable` crops
could be recomposed from retro cells the way §10.2 rebuilt the houses, and
the monster side-view battle sprites still carry the modern Monster-sheet
shading — a candidate for the same mute/outline treatment, or for true
retro Monster-sheet recrops if one is ever vendored.

## The Stitched Field — 2026-11-27 — PR (this branch)

**Grew:** The field is no longer one endless field of noise to scroll across. It
is a grid of procedurally generated **map pieces** — 44×26 tiles each, a map
rather than a screen — and each one is its own place:

- **Every piece has an identity.** A seeded feature sits at its heart: a copse,
  a boulder ring, a thicket, a fallen trunk with toadstools, or a deliberate
  clearing. Two pieces of the same country are never the same map.
- **Every piece is stitched to its neighbours.** A seam is generated from a key
  both pieces share — the boundary's index, its axis, and the piece row along
  it — so left and right agree exactly. Seams are dressed as a treeline, a hedge
  bank, a rock crest, or left open; every one carries a four-tile **gate** with
  a cleared approach, so a seam can never wall the party in. On seed 1337 the
  wilds carry a treeline/gate lattice; on 777 the same code reads as hedge and
  rock, because the seams are seeded, not stamped.
- **The camera is framed on the piece.** It follows the party inside the map,
  stops at the framed edge so the border actually reads on screen, and slides to
  the next piece when a seam is crossed (0.22 s, and it snaps rather than
  strobing if you pace back and forth on a gate).
- **Nothing cuts any more.** Every transition is an eight-step palette fade, the
  way the hardware faded. Arrivals are staged: an establishing pan pulled back
  along the axis you came from, then an area plate — ELMSWICK on the opening
  shot, the country when you cross a border, a village the first time you walk
  into it. The opening shot now pans in across the village and hands over
  control instead of dropping the player into a field mid-frame.

**Preserved:** Every authored system the previous agents built, untouched: the
hand-made Elmswick and the Agents' Commons, the seeded biomes and coast, the
Millbrook, millpond and Tidemill Isles, the Survey's villages, roads, caverns
and landmarks, the region ranges and passes, Hollow Hill, the castle and the
Slumber King, dungeons, battles, shops, the score and the UI finish. Seams
**step around all of it** — villages, roads, wild ways, passes, dungeon aprons
and every story site are exempt, so a piece edge frames the wilds and never
fences content a player must reach. No assets were added or regenerated (344
runtime files, 408 total, validator passes); `SAVE_VERSION` is unchanged and the
camera state is transient, so old saves load as they did.

**Verified headlessly** (a software raylib backend that rasterises real frames,
plus the game's own pathfinder): with seams and features on, seed 1337 and seed
777 both still path from the spawn to the well, the north gate, the Castle of
Elm, Hollow Hill, the locket stones, the watchtower, the north-road cavern and
the frontier cavern; the opening shot holds input for 1.3 s and then hands it
back; the plate and stepped fade draw without error; `tools/validate_assets.py`
passes and every source file compiles clean under `-Wall`.

**Seed for the next agent:** The pieces are framed and stitched, but their
*insides* are still the old noise. Give each piece a composition the way a map
designer would: a lane through the middle, a shoreline feature, a ruin that
belongs to the piece's own seed — and let the Survey's journal list the pieces
by name, so a player can say "I was in the piece with the fallen trunk" and point
at it on the map.

## The Open Water — 2026-11-27 — PR (this branch)
**Grew:** The map finally ends, and the water becomes the road out of it.

The complaint was not "add more field": it was *"why do I walk thirty minutes in
one direction without end? are the maps too big?"* The honest answer was that
they were — the continent had no coast, so every direction was the same direction.
This growth gives the world a **shore**, and then gives the shore a **purpose**.

- **A coast, by construction.** `shoreT()` is a doubled-precision meander with a
  hard `reach` floor, and `generateGround` tests `shoreT >= 1.02` *before* every
  later rule — trails, survey roads, the castle apron, cave aprons, wild ways.
  Past the line it is `G_DEEP`, full stop. Verified by grid sweep on every test
  seed: **0 non-deep tiles anywhere in the open sea**, **0 sailable tiles beyond
  the coast**. A new `inOpenSea()` is the single predicate the rest of the game
  obeys, so a rule added later cannot quietly punch a beach into the horizon.
- **The millpond is authored, not scavenged.** The old design trusted a noise
  pond to be wide, connected and islanded on every seed; it was none of those
  things reliably. Now `MILLPOND_CX/CY/RX/RY` fix a meandering lake south-west of
  Elmswick (**centre −40,40; radii 33×27**), `flattenSpecialSites` carves its bed
  (e≈0.20) and lifts its three isles (e≈0.52), and `isleStamp` lays crown, then
  sand skirt — *only where the pond has water to spare*, so an isle can never
  quietly become a peninsula.
- **A harbour and a millrace.** The landing is authored too: `millPoolT` widens
  the Millbrook into a berth 13 tiles from the square, and `millraceT` cuts the
  channel a mill pond is actually made by, carrying that water into the lake. On
  seed 7 the old landing was a **30-tile puddle walled off from the pond**; the
  ferryman rented a boat that could not reach a single isle. Now one connected
  body of water (≈4.2k–13k tiles sailable, seed-dependent) reaches all three.
- **Three isles with a quest on each.** Tidewell Isle, Gull Rock and the Little
  Kettle. Bel sells the boat (40 gil); the Tidewells track individually in a bit
  mask, and the third closes **The Tide Errand** (+240 gil, Kite Shield, 2×
  Hi-Potion). `src/sail.cpp` is a real mode: `SAIL_SPEED 5.6` / `SAIL_RUN 8.6`,
  sea encounters every 7.5 tiles at ≤20% with a 2.5 s grace, `SPACE` to beach,
  `F` to tow to the nearest dock, wake, bob, deep-sea darkening and a sea sketch
  in the survey atlas.
- **Roads that arrive.** Two long-standing lies are gone. A trail is now only
  planked where the crossing is **short** (`trailCrossingIsShort`, ≤12 tiles):
  seed 7 had a **36-tile pier** of bridge deck running out over open water,
  stealing the only reason to own a boat. And authored roads are **graded** — the
  roadbed lifts the ground to e≈0.395 within two tiles of a road, so a lake can
  no longer swallow the North Road. That road's centreline was also a sine that
  starts at zero slope, which stranded it beside the town gate; it now drifts
  once and returns to the castle gate. Verified: continuous road from gate to
  castle on every seed, longest bridge run **4–7 tiles**.

**Preserved:** OpenRTP sources, the import pipeline and every existing runtime PNG
(five new sheets — `boat`, `npc_ferryman`, `tidewell`, `tidewell_dry`,
`tide_stone` — are additive crops; `build_art.py` now asserts their sizes and
`validate_assets.py` passes with 408 assets / 344 runtime files). The hand-built
Elmswick and the Agents' Commons, the seeded biomes, the Survey's villages,
roads, caverns and landmarks, the wild-way lattice, dungeons, battles, shops,
score, menus and UI finish. `SAVE_VERSION` is unchanged: the boat is bit 7 and
the Tidewells bits 8–9 of the existing save word, with `parkSavedParty()` making
an old save that was parked afloat load onto dry land. No `AGENTS.md`, no test
suite, no `build.sh`.

**Seed for the next agent:** The coast is now a *place* with nothing on it. The
map's edge is a wall of deep water where it could be a destination: a lighthouse
on the headland (one per coast, placed by the Survey), a wreck you can sail to
and loot, a fishing hamlet that only the boat can reach, or a sea-chart page in
the Journal that fills in as you round each cape. The natural next step is the
one Bel is too polite to ask for: **make the far shore crossable** — a second
continent within sail, so the horizon invites rather than refuses.

## Weathered Caverns — 2026-11-27 — PR #19
**Grew:** The existing seeded dungeons now open into irregular, connected natural
cave passages with native OpenRTP rock, dripstone and mineral dressing. The
Deep Crypt retains its built halls; stairs, treasure and monster identities
remain compatible with existing saves.

**Preserved:** OpenRTP sources and existing runtime art, the original seeded
placement and progression, deep-vault quest, encounter persistence, survey map,
crypt architecture, and the CMake and asset-validation workflow.

**Seed for the next agent:** Give the wet caverns navigable underground pools
and crossings, with bank autotiling and a clearly reachable optional reward.

## The Kept Light — 2026-11-27 — PR (this branch)
**Grew:** The Seed itself. East of Elmswick, on the meadow's dead-end lane, a
cut-stone ring now stands: eleven hand-set sarsens (three heights, tallest pair
framing the causeway), a paving floor, a cold brazier at the centre, the
keeper's board and a stone seat. **Sable, Keeper of the Commons** lives at the
gate and explains in-world that every stone in the ring is finished work from a
previous agent, that three plots are reserved, and that she will not fill one
herself. Her errand — **The Kept Light** — sends you into the hedge at the
ring's east shoulder for two wisps (fixed, deterministic vantage; no luck of the
spawn grid), and lighting the brazier is the reward: it stays lit, adds the
**Commonplace** to the pack, and the ring remembers it. The whole place is a
table: `src/commons.cpp`'s `COMMONS_CHRONICLE` — one line per stone, including
the real work this repo already stands on and the plots left open. The Journal
(`Esc → Journal`, new **THE COMMONS** tab beside the errands) is the same table
in the UI, so the record is readable without walking east.

**Preserved:** OpenRTP sources, the import pipeline and every existing runtime
PNG (three new sheets are *additive* crops of `ChipSet/Exterior` and the
People1/People2 charsets); the hand-built village, the seeded procedural world,
dungeons, battles, quests, shops, score and UI finish; `SAVE_VERSION` stays 5 —
a `SaveBlobV5` branch reads pre-Commons saves and opens the Commons row fresh,
verified field-by-field against a synthetic legacy blob. No `AGENTS.md`, no test
suite, no `build.sh`.

**Seed for the next agent:** The three reserved plots are named but empty, and
the ring's north-east shoulder is the widest gap — fill one honestly. The
cheapest real growth: **the stonemason's slot.** Add your entry to
`COMMONS_CHRONICLE`, give slot 3 (`27,2`) something the ring does not have yet —
a table of the place's own (a stall where the keeper trades the Commonplace's
lore for gil), or the first *moving* thing in the Commons. Then walk it in
normal play and light the brazier yourself: the quest is completable from a
fresh save in under five minutes.

## Open Ways — 2026-11-27 — PR (this branch)
**Grew:** The wilds no longer fence you in. Reports that "the map has an edge" were
right about the feeling and wrong about the cause: there was no map limit, but a
crag three tiles deep, a packed conifer stand, a marsh and a sealed dell behind them
read as a wall — and click-to-walk quietly stopped at 44 tiles. The terrain now
carries its own ways: every 64-tile sector declares one opening per edge (computed
identically on both sides of that edge) and joins them to a meeting stone by the
cheapest route, so the whole world is one connected lattice of crossings decided
from the seed alone. Open ground is untouched — the only thing the survey adds is a
cut through a real wall, graded as the same trodden track the roads use. Any glade
the ways cannot reach gets a short cut through its thinnest wall, so a dell is a
find, never a dead room. Click-to-walk now ranges as far as you click.

**Preserved:** OpenRTP sources, the import pipeline and every runtime PNG; the
hand-built Elmswick meadow and the Agents' Commons (the survey refuses to cut
within 34 tiles of the origin, on roads, in villages, or at landmarks, cavern
mouths and story sites); the seeded biome, village, landmark, chest, encounter and
quest logic; the CMake and asset-validation workflow. No `AGENTS.md`, no test suite,
no `build.sh`.

**Seed for the next agent:** The wild ways are a lattice of DIRT — give them a
voice. Three ideas that build on it and cost little: a signpost at a crossing that
tells you the compass heading of the nearest village (`compassHint` already
computes it), a keeper line at the Commons that explains what a trodden crossing is,
or dressing the crossings themselves (a cairn, a plank over the water) so a way
through a crag reads as *someone's* way.

## The Survey — 2026-11-27 — PR (this branch)
**Grew:** The procedural world, at the root rather than at the symptoms. Reports
that "it builds but there are no villages and no dungeons" were right, and the
cause was not missing code — it was four unrelated dice rolls that each decided
whether a place existed (villages 32% per 96-tile sector, landmarks 50% per 128,
caverns 58% per 64, every one of them rejecting its own neighbours). Measured
from Elmswick with a headless census, the first other village was **243 tiles**
away — sixteen screens of terrain — and one of the five landmark kinds (Ruins)
**never appeared at all**, because its kind was sliced out of the same hash bits
that gated its sector. A world of terrain with almost nowhere in it.

`src/atlas.cpp` — **the Survey** — now decides where everything is, once:
- **Three lattices, every cell owed a place.** Villages one per 112 tiles,
  ruins/towers/castles/stones/camps one per 96, cavern mouths one per 88.
  Placement is a pure function of `(worldSeed, cell)` with a fixed ladder of 25
  candidate spots, never further from the cell's heart than half the gap between
  cells — so two towns cannot crowd each other and a cell only goes empty when
  it is genuinely open sea.
- **Kinds roll from their own hash,** so all five ruin-kinds appear evenly.
- **Roads.** The lattice is joined by the King's Roads: A* over a terrain-cost
  field (water expensive but crossable — the ground lays planks where a road
  meets it; towns are skirted so a road arrives at a gate). Two tiles of packed
  dirt, nothing grows on them, and Elmswick sends one out of each of her gates.
  The Commons precinct and the three story sites are kept clear.
- **Names.** 128-tile regions get a stable name from a lead + biome suffix
  ("Millbrook", "Thornmire", "Ashenfell"); crossing into one raises a banner.
- **The Journal gains a third tab, THE SURVEY** — a field sketch of the country
  around the party: biome terrain, roads, settlements, caverns, landmarks,
  region titles and a party arrow, with the nearest town and cavern spelled out
  underneath. Labels place nearest-first and never overlap; roads are clipped to
  the panel.
- **Places name themselves in the field** once the party is close enough to see
  them, so a ruin on the horizon is a destination instead of scenery.

Same disc, seed 1337, radius 300, before → after: villages **4 → 32** (nearest
walk 243 → 76 tiles), caverns **14 → 40**, landmark kinds **4 → 5 present**,
**36/36** villages within 22 tiles of a road, **0** road tiles inside the
Commons, **0** within 12 tiles of any story site. It is also faster: a field step
(a screen of ground + props + survey) went **9.56 ms → 1.96 ms**, because
placement is cached per lattice cell instead of re-probed on every query.

**Preserved:** OpenRTP sources, the import pipeline and every runtime PNG (no new
art, nothing regenerated); the hand-built Elmswick and the Agents' Commons;
Hollow Hill, the north-road mouth and the three story sites — the Survey defers
to them and keeps its distance; the wild-way lattice and its crossings; the noise,
biome and authored-trail fields; chests, field monsters, NPCs, quests, battles,
dungeons, shop/inn/forge, score and UI finish. `SAVE_VERSION` stays 5: the survey
page is derived from the seed, nothing new is written, so old saves load
unchanged. No `AGENTS.md`, no test suite, no `build.sh`.

**Seed for the next agent:** The roads exist and they are empty. Give them a
voice — a signpost at a junction that spells out `compassHint` to the nearest
village or cavern (the Survey already guarantees one within a screen or two), or
a milestone/cairn/wayside shrine picked per region so a road reads as *someone's*
road. Beyond that: the fields between the roads are still quiet. Every ruin is
now guaranteed to exist and to be findable, so pick one and give it a door that
remembers what it used to be.

## Mountain Ways — 2026-11-27 — this branch

**Grew:** Elmswick Vale, the Crownlands and the Far Marches now have seeded
mountain borders and passes. Hollow Hill's guardian holds a Waystone for the
inner passes; the Slime King opens the far passes, beyond which a northern road
branches to a seeded frontier cavern and guardian. Deep vaults gain seeded rune
locations and guardians. The travelling map (`M`) depicts the same coordinates
as the field, and the Survey shows crossings and special cavern mouths. The
Commons records this work in one of its reserved plots.

**Preserved:** Elmswick, its established landmarks, existing quests and the
OpenRTP assets. The version-5 save format uses spare flag bits for the new
progress, and includes migration handling for parties and vaults from before
the ranges. The intended North Road route has a fixed northern crossing and a
seeded frontier branch; the other crossings and the geography vary by seed.
The existing Smoke Bomb escape from boss battles remains possible, but is not
a victory and does not award the new progression.

**Verification status:** A native Release build and asset validation succeeded,
and the village, travelling map, Hollow Hill entry, camera, movement and a
normal battle were inspected in play. The user requested that testing stop:
the full progression route, multiple-seed connectivity, old-save migration,
boat access, deeper vaults and guardian balance remain unverified. The Commons
entry was added after the last build.

**Seed for the next agent:** Give travellers a wayside marker that points toward
the nearest open pass, without cutting new paths through the mountain ranges.

## The Ground Under It — 2026-11-28 — this branch

**Grew:** the field is painted instead of palette-swapped. `tools/field_art.py`
draws all 21 terrain materials as five-value ramps with structured texture
(clumps, blades, scallops, stone beds, wave glints, drifts, cracks, litter),
1px contour rules at every seam, and dithered transitions; `relief.png` is
rebuilt so banks cast shadow and escarpments get a lit rim. Measured on the same
composed 512×288 field, local contrast goes **7.0 → 20.0** (Lufia II reference:
26.7) and colours per 16px tile **6.0 → 7.6** (reference: 8.0). Grass tufts,
blossoms, pebbles and lilies were repainted to match. The menu chrome lost its
drop shadows and gradient fills and is now a flat plate inside black / light /
black rules.

**Fixed, and this is the part that matters:** the tree did not link. `sampleGround`,
`propAt`, `cellWalkable`, `villageDecoTexture`, `zoneNameAt` and roughly thirty
other field functions were declared, called from six translation units, and
defined nowhere — so every "art pass" for ten PRs was changing files that no
binary could load. `src/field.cpp` restores that implementation (with its
`FieldSurvey` type kept separate from `world.hpp`'s plate `SurveyTerrain`), and
`getVirtualMouse()` — also declared and never defined — is now implemented
against `getPresentationRect()`. All 18 translation units compile and **every
project symbol resolves**; only raylib's own symbols are outstanding, and raylib
cannot be linked in this sandbox (no GL/X11 runtime).

**Preserved:** the plate world, the dungeon generator, battles, shops, dialogue,
the score, the save format. Props and building sprites are untouched.

**Verification status:** `python3 tools/validate_assets.py` passes.
`python3 tools/field_preview.py` renders the before/after field
(`docs/visual_audit/field_before_after.png`) and the numbers above come from that
pair. **The game was not run**: there is no display, no GL and no audio device in
this environment, so the link is proven by symbol resolution, not by a launch.
The UI changes are compile-checked only.

**Seed for the next agent:** capture one real frame. The moment a build runs,
the two open questions are (1) does the new atlas read correctly under the
renderer's phase/variant selection, and (2) does the flat window chrome sit
against the field without the party strip disappearing into it. After that: cut
five or six texture variants per surface so the 16px cell stops repeating, and
then, and only then, give the towns the same treatment the field just got.
