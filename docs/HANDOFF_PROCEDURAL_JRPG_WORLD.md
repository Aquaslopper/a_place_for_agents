# Handoff: replace the blob with a procedural JRPG map world

**Mission:** Build the actual world structure the player asked for: many distinct, connected, procedurally generated SNES-style JRPG maps. This is a structural replacement task, not another pass over the existing continuous terrain field.

## Required outcome

- The game consists of discrete maps with their own layouts and identities—towns, forests, routes, deserts, caves, and other places—not one global noise field presented through camera-sized “pieces.”
- Maps are generated deterministically from stable map identities/seeds. Neighboring maps connect through real, compatible exits and transitions; crossing one loads the connected map and preserves the player’s place and game state.
- Exploration continues by generating or resolving new maps as the player travels. There is no artificial outer wall, finite continent edge, or progression gate that exists just to stop outward exploration. Local obstacles and story content are fine; they must not fake an outer limit.
- The system is real gameplay, not a diagram, atlas-only view, visual overlay, or renamed chunk system. Integrate generation, rendering, collision, movement, encounters, NPCs/sites, saving/loading, and transitions so the result is playable end to end.

## Do not repeat the failed approach

Do not treat terrain-noise edits, removal of coast/pass checks, a travel-map change, or the existing 44×26 camera pieces as the requested map system. The current source still has the blob architecture; these are not independent authored/generated maps with explicit connections. Replace or retire that architecture as needed, preserving useful game content and systems where practical. Do not claim the world is unbounded or the replacement works until it has been built and exercised across several map transitions.

## Completion bar

Deliver a coherent, playable change. Build the game, then verify actual traversal across map boundaries in more than one direction, deterministic regeneration of visited maps, valid exits on both sides of every transition, and continued outward exploration without a global cutoff. Check that save/load and existing core play still work. Update the player-facing docs so they describe the implemented system, not the old blob.

## Handoff state

The prior partial source edits were reverted. No map-system replacement has been implemented or validated. The only purpose of this file is to hand off the requested work. This human-directed mission supersedes the old seed mission in `docs/NEXT_AGENT_PROMPT.md` where it says to build on the blob; read the repository for implementation details, but do not reinterpret this request as another blob-map polish task.
