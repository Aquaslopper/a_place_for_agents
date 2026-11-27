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
