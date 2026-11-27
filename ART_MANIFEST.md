# Art & Runtime Asset Manifest — A Place for Agents

> Authoritative catalog of every shipped runtime file under `assets/` (excluding offline `assets/source/` MIDIs).
> Automatically validated for grid alignment, unique basenames, palette budgets, licenses, and C++ loader parity by `python3 tools/validate_assets.py`.

## Summary

- **Total Shipped Runtime Files:** 279 (`250` PNGs, `13` music cues, `13` SFX, `1` TTF font, `2` provenance/license texts)
- **Visual Dialect:** Valley Hearth (16-bit SNES JRPG Pixel Art) — `docs/ART_BIBLE.md`
- **Single Art Pipeline Entry Point:** `tools/build_art.py` (`tools/snes.py` core palette & raster engine)
- **SFX Generator:** `tools/gen_assets.py` (CC0 1.0)
- **Music Renderer (Offline Opt-In):** `tools/render_score.py` using EasyRPG OpenRTP MIDIs (`assets/source/openrtp/Music/`)

## Complete Runtime Asset Table

| Path | Category | Dimensions / Format | Opaque Colors | Generator / Source | License |
| --- | --- | --- | --- | --- | --- |
| `assets/audio/SCORE.txt` | Score Provenance | `text` | - | `Repository documentation` | CC0 1.0 / CC BY 4.0 notes |
| `assets/audio/battle.ogg` | Music Score (OGG) | `32000Hz stereo` | - | `tools/render_score.py (Battle 1.mid (Juan Manuel Nigretti, 2010))` | CC BY 4.0 |
| `assets/audio/boss.ogg` | Music Score (OGG) | `32000Hz stereo` | - | `tools/render_score.py (Boss 3.mid (Someoneman, 2022))` | CC0 1.0 |
| `assets/audio/cancel.ogg` | Sound Effects (OGG) | `22050Hz mono` | - | `tools/gen_assets.py` | CC0 1.0 |
| `assets/audio/chapel.ogg` | Music Score (OGG) | `32000Hz stereo` | - | `tools/render_score.py (Church.mid (Juan Manuel Nigretti, 2010))` | CC BY 4.0 |
| `assets/audio/coin.ogg` | Sound Effects (OGG) | `22050Hz mono` | - | `tools/gen_assets.py` | CC0 1.0 |
| `assets/audio/confirm.ogg` | Sound Effects (OGG) | `22050Hz mono` | - | `tools/gen_assets.py` | CC0 1.0 |
| `assets/audio/cursor.ogg` | Sound Effects (OGG) | `22050Hz mono` | - | `tools/gen_assets.py` | CC0 1.0 |
| `assets/audio/door.ogg` | Sound Effects (OGG) | `22050Hz mono` | - | `tools/gen_assets.py` | CC0 1.0 |
| `assets/audio/dunes.ogg` | Music Score (OGG) | `32000Hz stereo` | - | `tools/render_score.py (Vehicle 1.mid (Juan Manuel Nigretti, 2010))` | CC BY 4.0 |
| `assets/audio/encounter.ogg` | Sound Effects (OGG) | `22050Hz mono` | - | `tools/gen_assets.py` | CC0 1.0 |
| `assets/audio/fen.ogg` | Music Score (OGG) | `32000Hz stereo` | - | `tools/render_score.py (Dungeon 1.mid (Juan Manuel Nigretti, 2010))` | CC BY 4.0 |
| `assets/audio/field.ogg` | Music Score (OGG) | `32000Hz stereo` | - | `tools/render_score.py (Field 1.mid (Juan Manuel Nigretti, 2010))` | CC BY 4.0 |
| `assets/audio/flee.ogg` | Sound Effects (OGG) | `22050Hz mono` | - | `tools/gen_assets.py` | CC0 1.0 |
| `assets/audio/gameover.ogg` | Music Score (OGG) | `32000Hz stereo` | - | `tools/render_score.py (Game Over 1.mid (Juan Manuel Nigretti, 2010))` | CC BY 4.0 |
| `assets/audio/heal.ogg` | Sound Effects (OGG) | `22050Hz mono` | - | `tools/gen_assets.py` | CC0 1.0 |
| `assets/audio/hearth.ogg` | Music Score (OGG) | `32000Hz stereo` | - | `tools/render_score.py (Sorrow.mid (Juan Manuel Nigretti, 2010))` | CC BY 4.0 |
| `assets/audio/hit.ogg` | Sound Effects (OGG) | `22050Hz mono` | - | `tools/gen_assets.py` | CC0 1.0 |
| `assets/audio/levelup.ogg` | Sound Effects (OGG) | `22050Hz mono` | - | `tools/gen_assets.py` | CC0 1.0 |
| `assets/audio/pass.ogg` | Music Score (OGG) | `32000Hz stereo` | - | `tools/render_score.py (Castle 1.mid (Juan Manuel Nigretti, 2010))` | CC BY 4.0 |
| `assets/audio/potion.ogg` | Sound Effects (OGG) | `22050Hz mono` | - | `tools/gen_assets.py` | CC0 1.0 |
| `assets/audio/spell.ogg` | Sound Effects (OGG) | `22050Hz mono` | - | `tools/gen_assets.py` | CC0 1.0 |
| `assets/audio/sword.ogg` | Sound Effects (OGG) | `22050Hz mono` | - | `tools/gen_assets.py` | CC0 1.0 |
| `assets/audio/title.ogg` | Music Score (OGG) | `32000Hz stereo` | - | `tools/render_score.py (Opening 2.mid (Juan Manuel Nigretti, 2010))` | CC BY 4.0 |
| `assets/audio/victory.ogg` | Music Score (OGG) | `32000Hz stereo` | - | `tools/render_score.py (JFanfare 1.mid (Juan Manuel Nigretti, 2010))` | CC BY 4.0 |
| `assets/audio/village.ogg` | Music Score (OGG) | `32000Hz stereo` | - | `tools/render_score.py (Town 1.mid (Juan Manuel Nigretti, 2010))` | CC BY 4.0 |
| `assets/audio/wood.ogg` | Music Score (OGG) | `32000Hz stereo` | - | `tools/render_score.py (Ending 2.mid (Juan Manuel Nigretti, 2010))` | CC BY 4.0 |
| `assets/backdrops/bg_amber.png` | Battle Backdrops | `512x192` | 12 | `tools/build_art.py -> tools/battle_scenery.py` | CC0 1.0 |
| `assets/backdrops/bg_castle.png` | Battle Backdrops | `512x192` | 12 | `tools/build_art.py -> tools/battle_scenery.py` | CC0 1.0 |
| `assets/backdrops/bg_dunes.png` | Battle Backdrops | `512x192` | 8 | `tools/build_art.py -> tools/battle_scenery.py` | CC0 1.0 |
| `assets/backdrops/bg_fen.png` | Battle Backdrops | `512x192` | 12 | `tools/build_art.py -> tools/battle_scenery.py` | CC0 1.0 |
| `assets/backdrops/bg_forest.png` | Battle Backdrops | `512x192` | 10 | `tools/build_art.py -> tools/battle_scenery.py` | CC0 1.0 |
| `assets/backdrops/bg_frost.png` | Battle Backdrops | `512x192` | 12 | `tools/build_art.py -> tools/battle_scenery.py` | CC0 1.0 |
| `assets/backdrops/bg_heath.png` | Battle Backdrops | `512x192` | 13 | `tools/build_art.py -> tools/battle_scenery.py` | CC0 1.0 |
| `assets/backdrops/bg_meadow.png` | Battle Backdrops | `512x192` | 13 | `tools/build_art.py -> tools/battle_scenery.py` | CC0 1.0 |
| `assets/fonts/OFL.txt` | License Text | `text` | - | `SIL Open Font License 1.1` | SIL OFL 1.1 |
| `assets/fonts/PressStart2P-Regular.ttf` | Bitmap-Style Font | `8/16/24px TTF` | - | `Cody Boisclair (zone38.net)` | SIL OFL 1.1 |
| `assets/fx/arrow.png` | Battle FX | `16x16` | 6 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/fx/fireball.png` | Battle FX | `16x16` | 6 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/fx/impact.png` | Battle FX | `32x32` | 4 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/fx/poof.png` | Battle FX | `32x32` | 5 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/fx/slash.png` | Battle FX | `32x32` | 6 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/fx/spark.png` | Battle FX | `16x16` | 3 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/fx/sparkle.png` | Battle FX | `16x16` | 3 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/icons/armor_0.png` | Item & Gear Icons | `16x16` | 5 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/icons/armor_1.png` | Item & Gear Icons | `16x16` | 4 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/icons/armor_2.png` | Item & Gear Icons | `16x16` | 6 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/icons/armor_3.png` | Item & Gear Icons | `16x16` | 6 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/icons/axe.png` | Item & Gear Icons | `16x16` | 5 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/icons/bomb.png` | Item & Gear Icons | `16x16` | 5 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/icons/book.png` | Item & Gear Icons | `16x16` | 8 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/icons/boots.png` | Item & Gear Icons | `16x16` | 5 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/icons/bow.png` | Item & Gear Icons | `16x16` | 4 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/icons/coin.png` | Item & Gear Icons | `16x16` | 5 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/icons/crown.png` | Item & Gear Icons | `16x16` | 7 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/icons/elixir.png` | Item & Gear Icons | `16x16` | 8 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/icons/ether.png` | Item & Gear Icons | `16x16` | 7 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/icons/feather.png` | Item & Gear Icons | `16x16` | 6 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/icons/food.png` | Item & Gear Icons | `16x16` | 6 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/icons/gem.png` | Item & Gear Icons | `16x16` | 4 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/icons/hi_potion.png` | Item & Gear Icons | `16x16` | 8 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/icons/icon_candle.png` | Item & Gear Icons | `16x16` | 6 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/icons/key.png` | Item & Gear Icons | `16x16` | 4 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/icons/mace.png` | Item & Gear Icons | `16x16` | 5 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/icons/potion.png` | Item & Gear Icons | `16x16` | 7 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/icons/remedy.png` | Item & Gear Icons | `16x16` | 7 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/icons/ring.png` | Item & Gear Icons | `16x16` | 6 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/icons/robe.png` | Item & Gear Icons | `16x16` | 6 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/icons/shield.png` | Item & Gear Icons | `16x16` | 5 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/icons/spear.png` | Item & Gear Icons | `16x16` | 5 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/icons/staff.png` | Item & Gear Icons | `16x16` | 5 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/icons/star_metal.png` | Item & Gear Icons | `16x16` | 4 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/icons/sword_0.png` | Item & Gear Icons | `16x16` | 6 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/icons/sword_1.png` | Item & Gear Icons | `16x16` | 6 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/icons/sword_2.png` | Item & Gear Icons | `16x16` | 6 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/sprites/amber_oak.png` | Props & Furniture | `32x48` | 10 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/sprites/archfiend.png` | Field Monsters | `32x32` | 9 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/sprites/archfiend_battle.png` | Battle Monsters | `48x48` | 9 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/sprites/bandit.png` | Field Monsters | `32x32` | 12 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/sprites/bandit_battle.png` | Battle Monsters | `48x48` | 12 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/sprites/barrel.png` | Props & Furniture | `16x16` | 7 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/sprites/basin.png` | Props & Furniture | `16x16` | 7 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/sprites/bat.png` | Field Monsters | `32x32` | 7 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/sprites/bat_battle.png` | Battle Monsters | `32x32` | 7 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/sprites/bed.png` | Props & Furniture | `16x32` | 8 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/sprites/bench.png` | Props & Furniture | `32x16` | 5 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/sprites/boulder.png` | Props & Furniture | `32x32` | 8 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/sprites/bush.png` | Props & Furniture | `16x16` | 8 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/sprites/bush_plain.png` | Props & Furniture | `16x16` | 6 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/sprites/campfire.png` | Props & Furniture | `16x16` | 8 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/sprites/candle.png` | Props & Furniture | `16x16` | 6 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/sprites/castle.png` | Buildings & Landmarks | `96x80` | 12 | `tools/build_art.py -> tools/village_art.py` | CC0 1.0 |
| `assets/sprites/cauldron.png` | Props & Furniture | `16x16` | 6 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/sprites/cave_mouth.png` | Props & Furniture | `48x48` | 16 | `tools/build_art.py -> tools/art_terrain.py` | CC0 1.0 |
| `assets/sprites/chest_closed.png` | Props & Furniture | `16x16` | 6 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/sprites/chest_open.png` | Props & Furniture | `16x16` | 6 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/sprites/cloud.png` | Props & Furniture | `64x32` | 4 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/sprites/copper_oak.png` | Props & Furniture | `32x48` | 9 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/sprites/counter.png` | Props & Furniture | `16x16` | 5 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/sprites/crate.png` | Props & Furniture | `16x16` | 6 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/sprites/crystal.png` | Props & Furniture | `16x16` | 7 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/sprites/dead_tree.png` | Props & Furniture | `32x48` | 6 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/sprites/drake.png` | Field Monsters | `32x32` | 7 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/sprites/drake_battle.png` | Battle Monsters | `48x48` | 7 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/sprites/dread_knight.png` | Field Monsters | `32x32` | 9 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/sprites/dread_knight_battle.png` | Battle Monsters | `48x48` | 9 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/sprites/face_bard.png` | Portraits (32x32) | `32x32` | 10 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/face_child.png` | Portraits (32x32) | `32x32` | 10 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/face_cleric.png` | Portraits (32x32) | `32x32` | 11 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/face_demon.png` | Portraits (32x32) | `32x32` | 6 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/face_dog.png` | Portraits (32x32) | `32x32` | 6 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/face_elder.png` | Portraits (32x32) | `32x32` | 10 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/face_ghost.png` | Portraits (32x32) | `32x32` | 6 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/face_guard.png` | Portraits (32x32) | `32x32` | 12 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/face_innkeeper.png` | Portraits (32x32) | `32x32` | 11 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/face_knight.png` | Portraits (32x32) | `32x32` | 12 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/face_mage.png` | Portraits (32x32) | `32x32` | 10 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/face_mayor.png` | Portraits (32x32) | `32x32` | 11 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/face_ranger.png` | Portraits (32x32) | `32x32` | 10 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/face_shopkeeper.png` | Portraits (32x32) | `32x32` | 10 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/face_skeleton.png` | Portraits (32x32) | `32x32` | 4 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/face_slime.png` | Portraits (32x32) | `32x32` | 8 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/face_smith.png` | Portraits (32x32) | `32x32` | 10 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/face_villager0.png` | Portraits (32x32) | `32x32` | 10 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/face_villager1.png` | Portraits (32x32) | `32x32` | 10 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/face_villager2.png` | Portraits (32x32) | `32x32` | 10 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/face_villager3.png` | Portraits (32x32) | `32x32` | 9 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/face_villager4.png` | Portraits (32x32) | `32x32` | 10 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/face_villager5.png` | Portraits (32x32) | `32x32` | 11 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/face_villager6.png` | Portraits (32x32) | `32x32` | 10 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/face_villager7.png` | Portraits (32x32) | `32x32` | 11 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/face_villager8.png` | Portraits (32x32) | `32x32` | 11 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/face_villager9.png` | Portraits (32x32) | `32x32` | 9 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/face_woman.png` | Portraits (32x32) | `32x32` | 10 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/faerie.png` | Field Monsters | `32x32` | 8 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/sprites/faerie_battle.png` | Battle Monsters | `32x32` | 8 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/sprites/fence.png` | Props & Furniture | `16x16` | 6 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/sprites/fence_gate.png` | Props & Furniture | `32x16` | 6 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/sprites/fence_post.png` | Props & Furniture | `16x16` | 5 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/sprites/flowerbed.png` | Props & Furniture | `32x16` | 10 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/sprites/forge_anvil.png` | Props & Furniture | `32x32` | 7 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/sprites/forge_hearth.png` | Props & Furniture | `48x48` | 8 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/sprites/gate.png` | Buildings & Landmarks | `48x48` | 11 | `tools/build_art.py -> tools/village_art.py` | CC0 1.0 |
| `assets/sprites/ghost.png` | Field Monsters | `32x32` | 6 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/sprites/ghost_battle.png` | Battle Monsters | `48x48` | 6 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/sprites/goblin.png` | Field Monsters | `32x32` | 13 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/sprites/goblin_battle.png` | Battle Monsters | `48x48` | 13 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/sprites/golem.png` | Field Monsters | `32x32` | 7 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/sprites/golem_battle.png` | Battle Monsters | `48x48` | 7 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/sprites/hero_cleric.png` | Character Sheets (16x24, 8x4) | `128x96` | 10 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/hero_knight.png` | Character Sheets (16x24, 8x4) | `128x96` | 14 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/hero_mage.png` | Character Sheets (16x24, 8x4) | `128x96` | 12 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/hornet.png` | Field Monsters | `32x32` | 10 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/sprites/hornet_battle.png` | Battle Monsters | `32x32` | 10 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/sprites/house_0.png` | Buildings & Landmarks | `64x64` | 16 | `tools/build_art.py -> tools/village_art.py` | CC0 1.0 |
| `assets/sprites/house_1.png` | Buildings & Landmarks | `64x64` | 16 | `tools/build_art.py -> tools/village_art.py` | CC0 1.0 |
| `assets/sprites/house_2.png` | Buildings & Landmarks | `64x64` | 17 | `tools/build_art.py -> tools/village_art.py` | CC0 1.0 |
| `assets/sprites/house_3.png` | Buildings & Landmarks | `64x64` | 13 | `tools/build_art.py -> tools/village_art.py` | CC0 1.0 |
| `assets/sprites/house_4.png` | Buildings & Landmarks | `64x64` | 15 | `tools/build_art.py -> tools/village_art.py` | CC0 1.0 |
| `assets/sprites/imp.png` | Field Monsters | `32x32` | 9 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/sprites/imp_battle.png` | Battle Monsters | `32x32` | 9 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/sprites/inn.png` | Buildings & Landmarks | `80x64` | 16 | `tools/build_art.py -> tools/village_art.py` | CC0 1.0 |
| `assets/sprites/keg.png` | Props & Furniture | `16x16` | 7 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/sprites/ladder.png` | Props & Furniture | `16x16` | 4 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/sprites/lantern.png` | Props & Furniture | `16x16` | 5 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/sprites/longtable.png` | Props & Furniture | `48x16` | 5 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/sprites/mailbox.png` | Props & Furniture | `16x16` | 7 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/sprites/manor.png` | Buildings & Landmarks | `80x80` | 14 | `tools/build_art.py -> tools/village_art.py` | CC0 1.0 |
| `assets/sprites/monument.png` | Props & Furniture | `32x32` | 9 | `tools/build_art.py -> tools/village_art.py` | CC0 1.0 |
| `assets/sprites/mushroom.png` | Props & Furniture | `16x16` | 7 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/sprites/naga.png` | Field Monsters | `32x32` | 6 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/sprites/naga_battle.png` | Battle Monsters | `48x48` | 6 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/sprites/night_rider.png` | Field Monsters | `32x32` | 8 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/sprites/night_rider_battle.png` | Battle Monsters | `48x48` | 8 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/sprites/noticeboard.png` | Props & Furniture | `16x16` | 6 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/sprites/npc_cat.png` | Character Sheets (16x24, 8x4) | `128x96` | 7 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/npc_chicken.png` | Character Sheets (16x24, 8x4) | `128x96` | 7 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/npc_child.png` | Character Sheets (16x24, 8x4) | `128x96` | 12 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/npc_cow.png` | Character Sheets (16x24, 8x4) | `128x96` | 7 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/npc_dog.png` | Character Sheets (16x24, 8x4) | `128x96` | 7 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/npc_elder.png` | Character Sheets (16x24, 8x4) | `128x96` | 13 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/npc_farmer.png` | Character Sheets (16x24, 8x4) | `128x96` | 14 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/npc_guard.png` | Character Sheets (16x24, 8x4) | `128x96` | 14 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/npc_horse.png` | Character Sheets (16x24, 8x4) | `128x96` | 5 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/npc_knight.png` | Character Sheets (16x24, 8x4) | `128x96` | 14 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/npc_mayor.png` | Character Sheets (16x24, 8x4) | `128x96` | 11 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/npc_merchant.png` | Character Sheets (16x24, 8x4) | `128x96` | 11 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/npc_priest.png` | Character Sheets (16x24, 8x4) | `128x96` | 10 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/npc_ranger.png` | Character Sheets (16x24, 8x4) | `128x96` | 13 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/npc_sheep.png` | Character Sheets (16x24, 8x4) | `128x96` | 6 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/npc_smith.png` | Character Sheets (16x24, 8x4) | `128x96` | 11 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/npc_soldier.png` | Character Sheets (16x24, 8x4) | `128x96` | 13 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/npc_villager0.png` | Character Sheets (16x24, 8x4) | `128x96` | 11 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/npc_villager1.png` | Character Sheets (16x24, 8x4) | `128x96` | 10 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/npc_villager2.png` | Character Sheets (16x24, 8x4) | `128x96` | 11 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/npc_villager3.png` | Character Sheets (16x24, 8x4) | `128x96` | 8 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/npc_villager4.png` | Character Sheets (16x24, 8x4) | `128x96` | 11 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/npc_villager5.png` | Character Sheets (16x24, 8x4) | `128x96` | 13 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/npc_villager6.png` | Character Sheets (16x24, 8x4) | `128x96` | 11 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/npc_villager7.png` | Character Sheets (16x24, 8x4) | `128x96` | 10 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/npc_villager8.png` | Character Sheets (16x24, 8x4) | `128x96` | 11 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/npc_villager9.png` | Character Sheets (16x24, 8x4) | `128x96` | 11 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/npc_woman.png` | Character Sheets (16x24, 8x4) | `128x96` | 11 | `tools/build_art.py -> tools/art_actors.py` | CC0 1.0 |
| `assets/sprites/orc.png` | Field Monsters | `32x32` | 13 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/sprites/orc_battle.png` | Battle Monsters | `48x48` | 13 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/sprites/pavilion.png` | Props & Furniture | `48x48` | 8 | `tools/build_art.py -> tools/village_art.py` | CC0 1.0 |
| `assets/sprites/pillar.png` | Props & Furniture | `16x32` | 6 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/sprites/pine_0.png` | Props & Furniture | `32x48` | 9 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/sprites/pine_1.png` | Props & Furniture | `32x48` | 9 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/sprites/pine_2.png` | Props & Furniture | `32x48` | 9 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/sprites/plate.png` | Props & Furniture | `16x16` | 6 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/sprites/porcupine.png` | Field Monsters | `32x32` | 6 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/sprites/porcupine_battle.png` | Battle Monsters | `32x32` | 6 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/sprites/potted_plant.png` | Props & Furniture | `16x16` | 9 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/sprites/reaper.png` | Field Monsters | `32x32` | 11 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/sprites/reaper_battle.png` | Battle Monsters | `48x48` | 10 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/sprites/reeds.png` | Props & Furniture | `16x16` | 6 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/sprites/rock_0.png` | Props & Furniture | `16x16` | 6 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/sprites/rock_1.png` | Props & Furniture | `16x16` | 6 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/sprites/rock_2.png` | Props & Furniture | `16x16` | 6 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/sprites/ruin_arch.png` | Props & Furniture | `32x32` | 7 | `tools/build_art.py -> tools/village_art.py` | CC0 1.0 |
| `assets/sprites/serpent.png` | Field Monsters | `32x32` | 7 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/sprites/serpent_battle.png` | Battle Monsters | `48x48` | 7 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/sprites/shade.png` | Field Monsters | `32x32` | 6 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/sprites/shade_battle.png` | Battle Monsters | `48x48` | 6 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/sprites/shelf.png` | Props & Furniture | `48x32` | 10 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/sprites/shop.png` | Buildings & Landmarks | `80x64` | 16 | `tools/build_art.py -> tools/village_art.py` | CC0 1.0 |
| `assets/sprites/sign.png` | Props & Furniture | `16x16` | 5 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/sprites/skeleton.png` | Field Monsters | `32x32` | 8 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/sprites/skeleton_battle.png` | Battle Monsters | `48x48` | 8 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/sprites/slime.png` | Field Monsters | `32x32` | 7 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/sprites/slime_battle.png` | Battle Monsters | `32x32` | 7 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/sprites/slime_king.png` | Field Monsters | `48x48` | 12 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/sprites/slime_king_battle.png` | Battle Monsters | `64x64` | 12 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/sprites/smithy.png` | Buildings & Landmarks | `80x64` | 14 | `tools/build_art.py -> tools/village_art.py` | CC0 1.0 |
| `assets/sprites/stair_down.png` | Props & Furniture | `16x16` | 6 | `tools/build_art.py -> tools/art_terrain.py` | CC0 1.0 |
| `assets/sprites/stair_up.png` | Props & Furniture | `16x16` | 8 | `tools/build_art.py -> tools/art_terrain.py` | CC0 1.0 |
| `assets/sprites/statue.png` | Props & Furniture | `32x32` | 7 | `tools/build_art.py -> tools/village_art.py` | CC0 1.0 |
| `assets/sprites/stool.png` | Props & Furniture | `16x16` | 5 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/sprites/stump.png` | Props & Furniture | `16x16` | 5 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/sprites/table.png` | Props & Furniture | `16x16` | 5 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/sprites/temple.png` | Buildings & Landmarks | `80x80` | 18 | `tools/build_art.py -> tools/village_art.py` | CC0 1.0 |
| `assets/sprites/tiger.png` | Field Monsters | `32x32` | 5 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/sprites/tiger_battle.png` | Battle Monsters | `48x48` | 5 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/sprites/torch.png` | Props & Furniture | `16x16` | 7 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/sprites/tower.png` | Buildings & Landmarks | `48x64` | 14 | `tools/build_art.py -> tools/village_art.py` | CC0 1.0 |
| `assets/sprites/tree_0.png` | Props & Furniture | `32x48` | 9 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/sprites/tree_1.png` | Props & Furniture | `32x48` | 9 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/sprites/tree_2.png` | Props & Furniture | `32x48` | 9 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/sprites/tree_snow.png` | Props & Furniture | `32x48` | 11 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/sprites/urn.png` | Props & Furniture | `16x16` | 5 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/sprites/well.png` | Props & Furniture | `32x32` | 12 | `tools/build_art.py -> tools/village_art.py` | CC0 1.0 |
| `assets/sprites/willow.png` | Props & Furniture | `32x48` | 9 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/sprites/wisp.png` | Field Monsters | `32x32` | 6 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/sprites/wisp_battle.png` | Battle Monsters | `32x32` | 6 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/sprites/wolf.png` | Field Monsters | `32x32` | 6 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/sprites/wolf_battle.png` | Battle Monsters | `48x48` | 6 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
| `assets/sprites/yucca.png` | Props & Furniture | `32x32` | 9 | `tools/build_art.py -> tools/art_objects.py` | CC0 1.0 |
| `assets/tiles/brick.png` | Tiles & Autotiles | `16x16` | 5 | `tools/build_art.py -> tools/art_terrain.py` | CC0 1.0 |
| `assets/tiles/brick_dark.png` | Tiles & Autotiles | `16x16` | 4 | `tools/build_art.py -> tools/art_terrain.py` | CC0 1.0 |
| `assets/tiles/bridge.png` | Tiles & Autotiles | `16x16` | 6 | `tools/build_art.py -> tools/art_terrain.py` | CC0 1.0 |
| `assets/tiles/carpet.png` | Tiles & Autotiles | `16x16` | 6 | `tools/build_art.py -> tools/art_terrain.py` | CC0 1.0 |
| `assets/tiles/flower_0.png` | Tiles & Autotiles | `16x16` | 6 | `tools/build_art.py -> tools/art_terrain.py` | CC0 1.0 |
| `assets/tiles/flower_1.png` | Tiles & Autotiles | `16x16` | 7 | `tools/build_art.py -> tools/art_terrain.py` | CC0 1.0 |
| `assets/tiles/landscape.png` | Tiles & Autotiles | `256x1008` | 45 | `tools/build_art.py -> tools/landscape_art.py` | CC0 1.0 |
| `assets/tiles/lilies.png` | Tiles & Autotiles | `16x16` | 7 | `tools/build_art.py -> tools/art_terrain.py` | CC0 1.0 |
| `assets/tiles/pebble.png` | Tiles & Autotiles | `16x16` | 5 | `tools/build_art.py -> tools/art_terrain.py` | CC0 1.0 |
| `assets/tiles/plank_floor.png` | Tiles & Autotiles | `16x16` | 3 | `tools/build_art.py -> tools/art_terrain.py` | CC0 1.0 |
| `assets/tiles/relief.png` | Tiles & Autotiles | `256x80` | 12 | `tools/build_art.py -> tools/landscape_art.py` | CC0 1.0 |
| `assets/tiles/stone_floor.png` | Tiles & Autotiles | `16x16` | 4 | `tools/build_art.py -> tools/art_terrain.py` | CC0 1.0 |
| `assets/tiles/tile_floor.png` | Tiles & Autotiles | `16x16` | 5 | `tools/build_art.py -> tools/art_terrain.py` | CC0 1.0 |
| `assets/tiles/tuft_0.png` | Tiles & Autotiles | `16x16` | 4 | `tools/build_art.py -> tools/art_terrain.py` | CC0 1.0 |
| `assets/tiles/tuft_1.png` | Tiles & Autotiles | `16x16` | 4 | `tools/build_art.py -> tools/art_terrain.py` | CC0 1.0 |
| `assets/tiles/tuft_2.png` | Tiles & Autotiles | `16x16` | 4 | `tools/build_art.py -> tools/art_terrain.py` | CC0 1.0 |
| `assets/tiles/turnips.png` | Tiles & Autotiles | `16x16` | 8 | `tools/build_art.py -> tools/art_terrain.py` | CC0 1.0 |
| `assets/title_bg.png` | UI & Screens | `512x288` | 24 | `tools/build_art.py -> tools/battle_scenery.py` | CC0 1.0 |
| `assets/ui/dawn.png` | UI & Screens | `512x288` | 13 | `tools/build_art.py -> tools/battle_scenery.py` | CC0 1.0 |
| `assets/ui/gameover_plate.png` | UI & Screens | `512x288` | 15 | `tools/build_art.py -> tools/battle_scenery.py` | CC0 1.0 |
| `assets/ui/window_frame.png` | UI & Screens | `32x32` | 7 | `tools/build_art.py -> tools/art_misc.py` | CC0 1.0 |
