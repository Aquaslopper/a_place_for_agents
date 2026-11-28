# NEXT AGENT — read this first

You are continuing **A Place for Agents**, a C++17 / raylib SNES-style JRPG.

**The one-line brief: build the world.**

---

## 0. Read in this order, then start

1. **This file** (mission, rules, done-criteria).
2. [`HANDOFF_WORLD_STRUCTURE.md`](HANDOFF_WORLD_STRUCTURE.md) — the full state of
   the world systems, the target architecture, and the phase plan. This is your
   technical brief. Do not skip it.
3. [`VISUAL_AUDIT.md`](VISUAL_AUDIT.md) — why the art does not look 16-bit yet,
   with measurements. Relevant only at the art phase, but read it early so you
   do not repeat a failed pass.
4. [`ART_BIBLE.md`](ART_BIBLE.md) — the art law. §0 outranks everything.

Repo root files you will need: `CMakeLists.txt`, `ART_MANIFEST.md` (asset names,
sizes, crop coordinates), `GROWTH.md` (what previous passes did and claimed).

---

## 1. The mission

Make this game have **a real world**, the way Lufia II did:

> **A procedurally generated 16-bit JRPG world:** a miniature overworld you walk
> at compressed scale, dotted with town, castle, cave, tower and ruin icons.
> Enter an icon and you load a **real, bounded, generated map** — a town with
> streets, houses, an inn, a shop and NPCs; a route between places; a
> multi-floor dungeon with chests, puzzles and a guardian. Leave, and you are
> back on the overworld where you left it. Every map is generated from a seed,
> and the same seed always gives the same map. The world never ends: walk far
> enough and new countries generate.

Everything else — art, story, UI, music — is already largely built and is
**not** your job this pass. The world is what is missing.

---

## 2. Non-negotiables

1. **Structure before paint.** Do not spend this pass on tiles, palettes or
   sprites. Three previous passes did that and the game still does not look like
   a 16-bit JRPG, because the thing under the art is a noise field. Build the
   world first. Art gets one pass at the end, against the measured budget in
   `VISUAL_AUDIT.md`.
2. **Kill the infinite noise field.** `sampleGround(tx, ty)` returning terrain
   for any coordinate is the disease. The player must be on *a map*, with an
   identity, a size, and exits. See `HANDOFF_WORLD_STRUCTURE.md` §1–2.
3. **One world model.** Today `world.cpp` (plates, sites, countries, roads) and
   `sampleGround` (noise) both exist and contradict each other; the game runs on
   the noise. Pick the plate world, make it real, delete the other. Do not add
   a third.
4. **`mapGenerate(MapId)` must be pure.** Same `MapId` → same map, in the
   renderer, the collider, the save file and the test. No globals, no visit
   order, no "generate once at load".
5. **Bounded maps, explicit ports.** No invisible walls. A map ends where its
   exit is. Ports are reciprocal: if A exits to B, B exits to A.
6. **The overworld is the only unbounded thing.** New atlas cells generate as
   the party walks outward. That is how you satisfy "no global cutoff" — not by
   infinite terrain.
7. **Generation lives in C++.** The Python builders in `tools/` are design tools
   and preview renderers. They are excellent references (invariants, algorithms,
   plate catalogues) and must not become a second source of truth. Port their
   algorithms, don't call them at runtime.
8. **Save must survive the change.** Store `MapId` + position + per-map state
   (chests, doors, bosses). Bump `SAVE_VERSION` in `core.cpp` and migrate.
9. **Verify, then claim.** Every claim in `GROWTH.md` and your final message must
   be backed by a passing check from §4. A pretty contact sheet is not a
   passing check — that is exactly how PRs #36 and #37 reported success while
   changing almost nothing measurable.
10. **Do not regress what works.** The dungeon generator (`dungeon.cpp`), the
    battle system, shops, dialogue, the score and the save system are good.
    Wire them to the new maps; do not rewrite them.

---

## 3. Do this, in this order

Full detail, file-by-file, is in `HANDOFF_WORLD_STRUCTURE.md` §3. In short:

| Phase | Deliverable | Done when |
|---|---|---|
| **0** | A build that links locally | the binary compiles and runs |
| **1** | `MapId`, `MapDef`, `MapStore`; retire the blob | maps generate deterministically from an id |
| **2** | the **world atlas**: revive the plate world, call it, render it at compressed scale, make icons enterable | you can walk the overworld and enter sites |
| **3** | **towns**: port the town-layout algorithm to C++; delete `enterBuilding(0)`; generated interiors | town icons load real town maps |
| **4** | **routes / fields / coasts** as local maps with ports at both ends | a road on the atlas is a walkable route at full scale |
| **5** | **dungeons**: give the existing generator a `MapId` + ports; then add typed structures and lock-and-key puzzles | a dungeon is entered from and exits to the atlas |
| **6** | **one art pass**: 5-step ramps, 1px contours, palette budget | art budget checks pass |
| **7** | rewrite `README.md` to describe the world that now exists | docs match code |

Phase 0 is not optional and not a distraction: CI on `main` fails at the Build
step (Linux and Windows both), several functions are declared and called across
six translation units without a definition, and `atlas.cpp`/`commons.cpp`
reference fields and enum values that no longer exist. You cannot verify a
world you cannot compile. Fix it as part of reconciling the two world models —
not as an isolated compiler-error chase.

---

## 4. Definition of done

Ship a `--verify` mode (or `tools/verify_world.py`) that runs headlessly and
prints PASS/FAIL for all of:

- [ ] **Determinism** — each map regenerates byte-identical from its `MapId`.
- [ ] **Port reciprocity** — every port's target map exists and points back.
- [ ] **Reachability** — from the entry, BFS reaches every door, chest and exit.
- [ ] **Cross-map traversal** — town gate → route → atlas → another town's gate
      → interior → back, in one scripted run.
- [ ] **Unbounded atlas** — 200 cells outward from spawn, no crash, no hard edge.
- [ ] **Save round-trip** — save in a generated town, reload, same map and tile.
- [ ] **Art budget** — no town screen > 120 colours, no 16×16 tile > 12.
- [ ] Existing play still works: battles, shops, dialogue, sailing, the score.

Report honestly. If a phase is half-done, say which one and why. A truthful
"Phase 3 is 60% done, here is what is missing" is worth more than a green
summary that the next agent has to un-believe.

---

## 5. Working agreements

- **Never delete the repository root or `.git`.** Work on the current branch.
- **Do not commit generated art or large binaries** outside the existing
  conventions; `assets/` is registered, `docs/*_assets/` is design-time.
- **Keep `python3 tools/validate_assets.py` green.** It checks dimensions,
  manifest parity and the OpenRTP guardrails.
- **Attribution is not optional.** Anything sourced from OpenRTP / MrBeast /
  ArMM1998 goes in `CREDITS.md` and the manifest. Commercial rips (Lufia II
  maps) are study-only, gitignored, never shipped — see `CREDITS.md`.
- **Write `GROWTH.md` at the end**: what grew, what was preserved, what the next
  seed is. Match the existing voice; keep it honest.

---

## 6. Reference: what a Lufia II world actually is

Study these before designing. They are why the target is shaped the way it is.

- **The overworld** is a *map*, not terrain: hand-authored continents, mountain
  chains as walls, rivers with bridges, and a sprite-per-place vocabulary
  (town, cave, tower, castle, shrine, bridge). Travel is compressed — a few
  seconds between towns reads as a day's journey.
- **Towns** are bounded maps of a few screens, entered from a specific tile,
  with a gate you exit through. Streets connect lots; each lot is a building
  with a doorstep. Interiors are separate small maps.
- **Dungeons** are the puzzle game: switches, push blocks, movable platforms,
  chasm bridges, locked doors, and a boss at the bottom with a key item that
  opens the next region. Capsule Monsters aside, this is the identity.
- **Progression by capability**, not by wall: you get the bridge, the boat, the
  pass opened — and the map that was always there becomes crossable.

Free-to-use art that reads this way is already vendored in
`assets/source/openrtp/ChipSet/` (`World.png`, `retro_World.png`,
`retro_Exterior.png`, `retro_Dungeon.png`) and
`assets/source/overworld/`. The 100 overworld plates and 45 town plates are
already built. Use them.
