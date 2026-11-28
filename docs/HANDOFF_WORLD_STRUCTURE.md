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

### Implementation update — 2026-11-28

This branch now has a **standalone MapDef data layer** in `src/map.hpp/.cpp`:
seed-only bounded atlas cells, routes, towns, interiors, fields and four dungeon
floors; reciprocal edge ports and door/stair portals; local reachability checks;
and canonical map fingerprints. `tools/verify_world.py` exercises generation,
links, scripted traversal and 201 eastward atlas cells. Its extended run passed
100 seeds and an atlas radius of 8. A versioned, checksummed `MapSessionSave`
payload also round-trips a generated-town identity, player tile and per-map
object flags.

This is **not yet runtime integration**. The legacy game modules still do not
call `MapStore`; the new map-session payload is not embedded in `core.cpp`'s
`SaveBlob`; no generated MapDef is rendered or playable in the game. The scoped
headless verifier passes, and `g++ -fsyntax-only src/*.cpp` passes, but the full
raylib target has not linked: CMake configuration is blocked by missing X11
development files, and the legacy world helpers still have unresolved
implementations. Do not describe the new world as playable.

Right now this game has (1) legacy, unbounded-world call sites, (2) no generated
town maps in the runtime, (3) good existing dungeons, and (4) the new bounded
MapDef generators, still isolated from gameplay.

---

## 1. What actually exists today (verified, not claimed)

| System | Reality | Where |
|---|---|---|
| The field the legacy player code expects | The modules still ask for arbitrary world-coordinate `sampleGround(tx,ty)` reads, but this checkout has no definition for that API and no active `MapId`; the game target therefore does not link. | `game.hpp`, call sites in `render.cpp`, `battle.cpp`, `music.cpp`, `sail.cpp`, `pieces.cpp` |
| Bounded map data | **Implemented as a standalone layer, not used by gameplay.** `MapId`, `MapDef`, `MapStore`, atlas cells, two-ended routes, towns/interiors, fields, dungeon floors, portals and reachability live in the new C++ module. | `src/map.hpp`, `src/map.cpp` |
| Map verifier / session payload | Determinism, boundary-port reciprocity, scripted traversal, 201-cell outward generation and a checksummed map-session save payload pass headlessly. The payload is not connected to `SaveBlob`. | `tools/verify_world.py`, `tools/verify_world.cpp`, `mapSessionSave*` |
| "Map pieces" | `PIECE_W×PIECE_H` = 44×26 tiles of **camera framing + seam dressing** (treeline/hedge/rock/gate) computed from a hash. They are not maps: no identity, no ports, no state, no generation of contents. | `pieces.cpp/.hpp` |
| Carcassonne overworld | A **100-plate** 3×3-tile generator and a Python AC-3 layout tool exist. Some query/entry functions are called, but `worldInit()` / `worldReset()` are never called and the plate data is not the renderer's terrain source; this is not a coherent active world. | `world.cpp`, `tools/build_overworld_layout.py`, `docs/overworld_assets/` |
| Towns | **There are none.** Entering a `OW_SITE_TOWN` icon calls `enterBuilding(0)`, which opens a single **15×11 shop interior** (`MODE_ROOM`). That is a stub, not a town. | `world.cpp:307-319`, `room.cpp` |
| Town / interior generators | Complete, seeded, with `--check` invariants (edge match, overlap, BFS door reachability, gate round-trips) — **in Python only, never integrated.** 45 town plates + 8 interior stamps. | `tools/build_town_layout.py`, `tools/build_interior_layout.py`, `docs/town_assets/` |
| Dungeons | **Real and good.** Seeded BSP + cellular weathering, 56×40, multi-floor, chests, vault, guardians, themes. This is the one part built to spec. | `dungeon.cpp` (~1700 lines) |
| Saving | Works, versioned, `SAVE_VERSION` in `core.cpp`. Has no concept of "which map am I in". | `core.cpp` |

### The disease

There are **three separate pieces, none of which is the complete active world**:

- `world.cpp` + plates + `atlas.cpp` + `regions.cpp`: a bounded, plate-based
  world with countries and sites, but no active runtime call path.
- Legacy field call sites: they request unbounded `sampleGround(tx,ty)` reads;
  the definition is missing in this checkout, so those modules do not link.
- `src/map.cpp`: the new pure, bounded `MapDef` generator and map graph. It is
  the target model, but still isolated from rendering, movement and `SaveBlob`.

The next step is to reconcile the runtime around `MapDef`, not add another
terrain generator or revive noise as the source of truth. Every previous pass
tried to make the noise field *look* like a JRPG world (biomes, coasts, "pieces",
seams, a travel map). That cannot supply explicit places and exits. This is the
same failure the human already documented as "the repo is a lie, this is not an
snes jrpg" in
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
problem: legacy world helpers still have no definitions (`sampleGround`,
`propAt`, `cellWalkable`, `inOpenSea`, and several rendering/village helpers),
while CMake configuration is blocked here by missing X11 development files.
The API/type drift in `world.hpp` has been reconciled enough for
`g++ -fsyntax-only src/*.cpp` to pass, but the game has not linked. Treat this
as "reconcile the runtime around the map model", not as "chase compiler errors".

### Phase 1 — MapId, MapDef, MapStore; retire the blob (data layer implemented)
- `src/map.hpp/.cpp` now provide pure bounded map generation, `MapStore`,
  canonical fingerprints, structural/reachability validation, reciprocal port
  and portal records, and versioned `MapSessionSave` payload primitives.
- The verifier covers determinism on 100 seeds, reciprocal routes/doors, a
  scripted town/route/interior journey, 201 outward atlas cells, and a
  generated-town session save round-trip. Those checks exercise map data only;
  `sampleGround(tx,ty)` still has no runtime adapter and the payload is not in
  `core.cpp`'s `SaveBlob`.
- The remaining migration is to make `sampleGround`/movement/rendering read the
  active `MapDef`, then add the save migration required by the game.
- `pieces.cpp` is still a legacy global-field dressing pass; decide whether to
  remove it or demote its useful pieces to local-map décor after runtime wiring.
  The map generator does not call it.

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
