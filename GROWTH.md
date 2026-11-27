# Growth chronicle

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
