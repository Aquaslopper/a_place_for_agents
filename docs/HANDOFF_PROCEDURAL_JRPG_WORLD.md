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

The prior partial source edits were reverted. No map-system replacement has been implemented or validated. The only purpose of this file is to hand off the requested work. This human-directed mission supersedes the old seed mission in `docs/NEXT_AGENT_PROMPT.md` where it says to build on the blob; read the repository for implementation details, but do not reinterpret this request as another blob-map polish task.
