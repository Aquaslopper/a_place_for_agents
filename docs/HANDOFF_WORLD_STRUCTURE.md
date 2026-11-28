# Handoff: the world is the missing thing

**Status:** the game's *systems* mostly exist. Its *world* does not. This
document explains exactly what is there, what is fake, and what has to be built,
in order, with tests that make the claims checkable.

Read this whole file before touching `src/`. Then read
[`NEXT_AGENT_PROMPT.md`](NEXT_AGENT_PROMPT.md) for the mission statement.

Reference target: **Lufia II (SNES)**. 16-bit consols JRPG. That means, to the
letter:

1. a **miniature overworld** you walk around at compressed scale, with a row of
   icons (towns, caves, towers, castles, bridges) you can enter;
2. **towns** that are real, bounded, walkable maps with lots, streets, NPCs,
   shops and doors;
3. **dungeons** that are multi-floor, puzzle-shaped and treasure-bearing;
4. everything **procedurally generated from a seed**, and identical when
   regenerated.

Right now this game has (1) a fractal noise field with no boundaries, (2) no
town maps at all, (3) good dungeons, (4) three separate generators that never
touch the runtime.

---

## 1. What actually exists today (verified, not claimed)

| System | Reality | Where |
|---|---|---|
| The field the player walks on | **One infinite noise field.** `sampleGround(tx,ty)` is a pure function of world coordinates — there is no map, no boundary, no "you are now entering Mirefen". | `game.hpp:226`, `sampleGround` call sites in `render.cpp`, `battle.cpp`, `music.cpp`, `sail.cpp`, `pieces.cpp` |
| "Map pieces" | `PIECE_W×PIECE_H` = 44×26 tiles of **camera framing + seam dressing** (treeline/hedge/rock/gate) computed from a hash. They are not maps: no identity, no ports, no state, no generation of contents. | `pieces.cpp/.hpp` |
| Carcassonne overworld | A **100-plate** 3×3-tile overworld exists in `world.cpp`, and a working AC-3 edge-matching layout generator exists in Python. **It is dead code:** `worldInit()` / `worldReset()` are *never called* anywhere. Nothing but `worldEnterSiteAt()` is referenced. | `world.cpp`, `tools/build_overworld_layout.py`, `docs/overworld_assets/` |
| Towns | **There are none.** Entering a `OW_SITE_TOWN` icon calls `enterBuilding(0)`, which opens a single **15×11 shop interior** (`MODE_ROOM`). That is a stub, not a town. | `world.cpp:307-319`, `room.cpp` |
| Town / interior generators | Complete, seeded, with `--check` invariants (edge match, overlap, BFS door reachability, gate round-trips) — **in Python only, never integrated.** 45 town plates + 8 interior stamps. | `tools/build_town_layout.py`, `tools/build_interior_layout.py`, `docs/town_assets/` |
| Dungeons | **Real and good.** Seeded BSP + cellular weathering, 56×40, multi-floor, chests, vault, guardians, themes. This is the one part built to spec. | `dungeon.cpp` (~1700 lines) |
| Saving | Works, versioned, `SAVE_VERSION` in `core.cpp`. Has no concept of "which map am I in". | `core.cpp` |

### The disease

There are **two contradictory world models in the same source tree** and the
game runs on the wrong one:

- `world.cpp` + plates + `atlas.cpp` + `regions.cpp`: a **bounded, plate-based,
  site-bearing world** with countries, ranges, passes and roads. Correct shape,
  dead code.
- `sampleGround()`: **infinite noise**, which is what actually renders, collides
  and generates encounters.

Every previous pass tried to make the noise field *look* like a JRPG world
(biomes, coasts, "pieces", seams, a travel map). That cannot succeed. Noise has
no places in it. This is the same failure the human already documented as
"the repo is a lie, this is not an snes jrpg" in
[`HANDOFF_PROCEDURAL_JRPG_WORLD.md`](HANDOFF_PROCEDURAL_JRPG_WORLD.md) — and it
is still true. Do not add a fourth coat of paint to the noise.

---

## 2. The target architecture (build this, not something else)

Two coordinate spaces, **explicitly separate**. Everything else follows.

```
WORLD ATLAS  (the miniature overworld, compressed scale)
  a grid of world cells; each cell holds at most one Site
  Sites: town, castle, tower, cave, ruins, temple, bridge, camp
  party walks here with a miniature sprite; roaming is unbounded (new cells
  generate on demand from the world seed)
        │ enter icon
        ▼
LOCAL MAP  (a real, bounded map in full walking scale)
  kind: town | route | field | dungeon
  identity: (kind, seed, atlasCellX, atlasCellY)   ← deterministic, stable
  contents: tilemap, props, NPCs, doors, chests, enemies, edge ports
        │ exit / port
        ▼
back to WORLD ATLAS, party placed on the Site it came out of
```

### Contracts (these are the actual deliverable)

```cpp
// A map's identity. Deterministic: same struct -> same map, byte for byte.
struct MapId { MapKind kind; int32_t wcellX, wcellY; uint32_t seed; };

// A port is a two-way edge connection. Ports are declared by the generator and
// must be reciprocal: port A->B exists iff B->A exists.
struct Port { MapId to; uint8_t toIndex; uint8_t fromIndex; int tx, ty; };

struct MapDef { MapId id; int w, h; std::vector<GroundTile> tiles;
                std::vector<WorldProp> props; std::vector<Port> ports;
                std::vector<Portal> portals; /* doors -> interiors */ };

MapDef mapGenerate(MapId);      // pure; no globals, no visit order
void   mapEnter(MapId, int portIndex);
```

Rules that make or break this:

1. **`mapGenerate` is pure.** A function of `MapId` only. Same answer in the
   renderer, the collider, the encounter roller, the save file and the test
   harness. Today's `sampleGround` is pure but *infinite*; today's `world.cpp`
   regions are pure but unused. Keep purity, kill infinity.
2. **Every map is bounded and has explicit exits.** A town has gate ports; a
   route has ports at both ends; a dungeon has an entrance and a boss door.
   There is no invisible wall — the map ends where its port is.
3. **The overworld is the only unbounded thing.** Crossing a world cell
   generates that cell's site deterministically. That satisfies "no global
   cutoff, exploration continues" *without* an infinite walk on noise.
4. **Scale separation is a gameplay rule, not a rendering trick.** On the world
   atlas one tile ≈ a day's road; in a town one tile ≈ a paving stone. That is
   the "illusion of scale" the SNES used and it is why towns feel bigger than
   the overworld.
5. **One generator, one language, per map kind.** Generation must live in C++
   (runtime infinity demands it). The Python builders stay as **design tools and
   art previews** — do not let them become a second source of truth. Where a
   Python generator already encodes good invariants, port its *algorithm*.
6. **Save stores `MapId` + position**, plus per-map state (chests opened, doors
   unlocked, boss cleared) keyed by `MapId`. Change `SAVE_VERSION` and write a
   migration; old saves must load into a valid map.

---

## 3. The plan, in dependency order

Each phase ends in something runnable and verifiable. Do not skip to art.

### Phase 0 — a build that links
Prerequisite for every claim below: nothing can be verified on a red build.
CI on `main` currently fails at the **Build** step (Linux and Windows both).
That is parked by the human's instruction — do not spend a pass diagnosing it
for its own sake, but **the first phase of world work is to get a green local
build**, because every acceptance test here needs a binary. Known shape of the
problem: declarations in `game.hpp`/`world.hpp` without definitions
(`sampleGround`, `propAt`, `cellWalkable`, `inOpenSea` are called in 6+ TUs and
defined in none as committed), and `atlas.cpp`/`commons.cpp` referencing fields
and enum values that no longer exist (`SurveyTerrain::swamp`,
`LOCKET_SITE_TX`, `VD_STATUS_STONE`). Treat it as "reconcile the two world
models", not as "chase compiler errors".

### Phase 1 — MapId, MapDef, MapStore; retire the blob
- Add `src/map.hpp/.cpp` with the contracts above.
- `sampleGround(tx,ty)` becomes `mapGround(MapId, lx, ly)` — a lookup into the
  current `MapDef`. The name `sampleGround` may survive as a thin accessor while
  you migrate call sites; the *semantics* must stop being "noise at world
  coordinates".
- `pieces.cpp` is either deleted or demoted to a **local-map décor pass** (a
  forest map's interior composition). Seams between world cells stop existing;
  ports replace them.
- **Test:** `mapGenerate` on 10 seeds × 4 kinds, twice, byte-identical.

### Phase 2 — the world atlas (the Lufia overworld)
- Revive `world.cpp`'s plate world as the **atlas**, and call it. Use the
  100 plates and the AC-3 matcher that already exist; port
  `build_overworld_layout.py` to C++ or precompute-and-embed, but the atlas must
  generate new cells at runtime as the party walks outward.
- Render it at compressed scale, miniature party sprite, site icons, visible
  roads and ranges. Pressing `M` shows the same world, not a different one.
- **Test:** from the start cell, walk to any site in a 5×5 cell window on 3
  seeds; every site's icon is enterable.

### Phase 3 — towns (the biggest missing piece)
- Port `build_town_layout.py`'s algorithm: 8×8-tile district plates, edge
  matching, required civic lots (inn, shop, smithy, temple, manor), gate sockets,
  BFS reachability, overlap-free footprints. Tiers: hamlet / town / city.
- Entering a town icon **loads a town map**, not a shop interior. Doors then
  enter interiors (Phase 3b): port `build_interior_layout.py`'s stamps and
  three-row wall profile into `room.cpp` so interiors are generated, not five
  hardcoded rectangles.
- `enterBuilding(0)` must be deleted. It is the single clearest lie in the code.
- **Test:** for 20 seeds × 3 tiers: every gate reaches every door, every door
  round-trips, every building's interior generates and its exit returns to the
  correct doorstep.

### Phase 4 — routes, fields, forests, coasts
- New local-map kinds from the existing dressing vocabulary: a route between two
  atlas cells (ports at both ends, chokepoints, encounters), a forest field, a
  coast/beach field. These are the connective tissue that made SNES worlds feel
  continuous.
- Roads on the atlas must correspond to route maps at full scale.

### Phase 5 — dungeons (already good: connect, don't rewrite)
- `enterDungeon` already produces a real multi-floor dungeon from a site seed.
  Give it a `MapId`, a declared entrance port and a boss door, and let its exit
  return to the atlas cell.
- Then spend effort on the thing the code admits it lacks
  (`HANDOFF_MAP_GENERATORS.md` §2): **typed structures** (cave / tower / crypt /
  ruins / castle are all the same BSP box carver today) and **lock-and-key
  topology** (switches, push blocks, gates, chasm bridges) — the Lufia puzzle
  contract.

### Phase 6 — art, once, against the numbers
Only after structure. Read [`VISUAL_AUDIT.md`](VISUAL_AUDIT.md) first: the
measured gap vs a real Lufia II frame is **2228 colours/screen vs 70**, and
**26 colours per 16×16 tile vs 8**. Fix the ramp so ground tiles keep the
source's luminance spread (shipped grass: 10/255; source cell: 109/255), give
every sprite a 1px contour, and add the palette-budget check to `--check`.
Do not touch art before Phase 5; three passes have already tuned it and it did
not move, because the structure under it was noise.

### Phase 7 — verify, then rewrite the player docs
README still describes a world that does not exist. When the world is real,
rewrite it. Until then, mark the outdated sections.

---

## 4. Acceptance criteria (the handoff is not done until these pass)

Add `tools/verify_world.py` (or a `--verify` flag on the binary) that runs
headlessly and prints PASS/FAIL for:

1. **Determinism** — every map regenerates byte-identical from its `MapId`.
2. **Port reciprocity** — for every port in every generated map, the target map
   exists and has the matching port back.
3. **Reachability** — from the entry port, BFS reaches every door, chest, NPC
   objective and exit port. Zero unreachable placed objects.
4. **Cross-map traversal** — script a walk: town gate → route → atlas → another
   town's gate → interior → back. All four transitions in one run.
5. **Unbounded atlas** — walk 200 cells in one direction from the start; every
   cell generates a valid site-or-terrain without a hard edge or crash.
6. **Save round-trip** — save inside a generated town, quit, reload, and land in
   the same map at the same tile with the same per-map state.
7. **Art budget** — no town screen above 120 colours; no 16×16 tile above 12.

Any claim made in `GROWTH.md` must be backed by one of these runs. A screenshot
is not verification of a structure; a passing test is.

---

## 5. Traps that already consumed previous agents

- **Do not polish the noise field.** Biomes, coasts, "pieces", seams, travel-map
  changes and pass/coast removal have all been tried. None of it is a map
  system.
- **Do not call a Python preview "implemented".** `docs/overworld_assets/
  layout_seed4444.png` is a lovely map. It never runs in the game. Phase 2
  means *the binary generates and renders this*, not that a PNG exists.
- **Do not report a pass as done because a contact sheet looks better.**
  PRs #36 and #37 both claimed the art had moved; measured colour counts and
  per-tile budgets say it barely did. Measure, then claim.
- **Do not touch `src/` and `CMakeLists.txt` from a Python tool.** One handoff
  explicitly forbade it and a later pass did it anyway, which is how the tree
  ended up with two world models and a red build.
- **Do not restore the deleted-work list** from
  `HANDOFF_PLATE_LAYOUT_GENERATOR.md` verbatim — that constraint applied to one
  bounded tool pass, not to the repository's architecture.

## 6. Where the detail lives

| Need | File |
|---|---|
| Mission statement | [`NEXT_AGENT_PROMPT.md`](NEXT_AGENT_PROMPT.md) |
| Why the art reads modern, with numbers | [`VISUAL_AUDIT.md`](VISUAL_AUDIT.md) |
| The 16-bit art rules and crop registry | [`ART_BIBLE.md`](ART_BIBLE.md) |
| The original procedural-world mission | [`HANDOFF_PROCEDURAL_JRPG_WORLD.md`](HANDOFF_PROCEDURAL_JRPG_WORLD.md) |
| Town/dungeon generator specs and invariants | [`HANDOFF_MAP_GENERATORS.md`](HANDOFF_MAP_GENERATORS.md) |
| Overworld plate format and edge labels | [`HANDOFF_PLATE_LAYOUT_GENERATOR.md`](HANDOFF_PLATE_LAYOUT_GENERATOR.md) |
| Runtime asset names, sizes, coordinates | `ART_MANIFEST.md` |
| What worked and what didn't, pass by pass | `GROWTH.md` |
