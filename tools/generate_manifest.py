#!/usr/bin/env python3
"""generate_manifest.py — rebuild ART_MANIFEST.md from the files on disk.

    python3 tools/generate_manifest.py

The manifest is *generated*, never hand-edited: path, byte size, PNG
dimensions and category are read from disk; the license/provenance note
comes from the rules below, which mirror CREDITS.md. If you add or rename
an asset, re-run this and commit the result — tools/validate_assets.py
fails when disk and manifest disagree.

PROVENANCE POLICY (docs/ART_BIBLE.md A1): the runtime art is derived from
EasyRPG/OpenRTP (vendored at assets/source/openrtp/, revision 993d88c)
plus a small number of repository-authored pieces drawn in the same
dialect, listed as exceptions below and credited CC0 in CREDITS.md.
Nothing here may ever be relabelled "100% original": provenance follows
the source sheet, not the last tool that touched the file.
"""
import pathlib
import struct

ROOT = pathlib.Path(__file__).resolve().parent.parent
ASSETS = ROOT / "assets"

OPENRTP = "EasyRPG/OpenRTP `993d88c` (vendored `assets/source/openrtp/`)"
CHIPSET = f"{OPENRTP} ChipSet crop/composite — CC0 (Jason Perry / finalbossblues)"
# Walking-scale buildings are composed from the SNES-retro Exterior chipset,
# a multi-author CC BY / CC0 mix (see assets/source/openrtp/AUTHORS.md).
RETRO_BUILDING = (f"{OPENRTP} ChipSet/retro_Exterior.png composite — CC BY / CC0 mix "
                  "(MrBeast, Redshrike, surt, Ogrebane, Gwes, jikmok, Tom Lemmens, "
                  "marianognu; see `AUTHORS.md`)")
RETRO_BUILDINGS = {"house_0", "house_1", "house_2", "house_3", "house_4",
                   "shop", "inn", "temple", "manor", "smithy", "castle", "tower"}
# 10.3 retro pass: terrain textures, flora, props and cave art now crop or
# composite the SNES-retro chipsets instead of the modern chipsets.
RETRO_CHIPSET = (f"{OPENRTP} ChipSet/retro_* chipsets crop/composite — CC BY / CC0 mix "
                 "(MrBeast, Redshrike, surt, Ogrebane, Gwes, jikmok, Tom Lemmens, "
                 "marianognu; see `AUTHORS.md`)")
RETRO_TILES = {"landscape.png", "relief.png", "cobble.png", "flagstone.png",
               "dirt.png", "sand.png", "beach.png", "swamp.png", "tundra.png",
               "rock_hill.png", "rock_hill2.png", "soil.png", "snow.png",
               "snow_drift.png", "ice.png", "ice_crack.png", "grass_0.png",
               "grass_1.png", "grass_2.png", "grass_3.png", "grass_dark.png",
               "tile_floor.png", "stone_floor.png", "dungeon_floor.png",
               "cave_floor_0.png", "cave_floor_1.png", "cave_floor_2.png",
               "cave_floor_3.png", "cave_rock_0.png", "cave_rock_1.png",
               "cave_rock_2.png", "cave_rock_3.png", "flower_0.png", "flower_1.png",
               "town_facade_atlas.png", "town_masonry_atlas.png",
               "town_battlement_atlas.png", "town_shingles_red_atlas.png",
               "town_shingles_blue_atlas.png"}
RETRO_SPRITES = {"tree_0", "tree_1", "tree_2", "amber_oak", "copper_oak",
                 "willow", "pine_0", "pine_1", "pine_2", "tree_snow",
                 "bush", "bush_plain", "torch", "campfire", "fence",
                 "fence_post", "fence_gate", "noticeboard", "barrel",
                 "longtable", "rock_0", "rock_1", "crystal", "boulder",
                 "cave_stalagmite", "cave_stalagmite_small", "cave_crystal_blue",
                 "cave_crystal_cluster", "cave_rock_pile", "cave_boulder",
                 "town_wall_ivy", "town_market_flowers_orange",
                 "town_market_flowers_red"}
# Procedural sea tiles: in-dialect composition on the locked 2.4 water ramps.
SEA_TILES = {"water_0.png", "water_1.png", "deep_water.png"}
CHARSET_BY = f"{OPENRTP} CharSet crop — CC BY 4.0 / CC0 per-sheet (see `AUTHORS.md`: VictorSena, Marina Navarro Travesset, Garakuta, MerchantShade et al.)"
FACESET = f"{OPENRTP} FaceSet crop — CC0 (Garakuta)"
MONSTER = f"{OPENRTP} CharSet/Monster crop — CC BY 4.0 / CC0 per-sheet (MerchantShade, VictorSena, R.C.O., Orochii Zouveleki et al.)"
REPO = "Repository-authored in the OpenRTP dialect (`tools/`) — CC0"
MUSIC = "OpenRTP MIDI recording (offline render, `tools/render_score.py`) — CC BY 4.0 (Juan Manuel Nigretti); `boss.ogg` CC0 (Someoneman); see `assets/audio/SCORE.txt`"

# Repository-drawn field/battle sprites (CREDITS.md) — everything else in
# sprites/ is an OpenRTP crop or ChipSet composite.
REPO_SPRITES = {"wolf", "bat", "wisp", "serpent", "mushroom", "slime_king"}
# Every field enemy has a *_battle.png counterpart; that pair is how the
# generator recognises the enemy family without a hand-kept list.
BATTLE_BASES = {p.stem.replace("_battle", "") for p in (ASSETS / "sprites").glob("*_battle.png")}


def png_size(path):
    data = path.read_bytes()[:24]
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        return None
    return struct.unpack(">II", data[16:24])


def category(rel):
    parts = rel.parts
    if parts[0] == "source":
        return "vendored source (not loaded at runtime)"
    if len(parts) == 1:
        return "title plate"
    return {
        "tiles": "terrain tile / atlas", "sprites": "sprite / sheet",
        "icons": "item icon", "fx": "combat effect", "ui": "UI plate",
        "backdrops": "battle backdrop", "audio": "music / sound effect",
        "fonts": "font",
    }.get(parts[0], parts[0])


def provenance(rel):
    parts = rel.parts
    name = rel.name
    stem = rel.stem
    if parts[0] == "source":
        return f"{OPENRTP} — per-file author/license in `assets/source/openrtp/AUTHORS.md` (CC BY 4.0 / CC0)"
    top = parts[0] if len(parts) > 1 else ""
    if top == "audio":
        return "Sound effect synthesized in-repo (`tools/gen_assets.py`) — CC0" if name not in (
            "SCORE.txt",) and rel.suffix == ".ogg" and name in {
            "cursor.ogg", "confirm.ogg", "cancel.ogg", "sword.ogg", "hit.ogg",
            "spell.ogg", "heal.ogg", "coin.ogg", "potion.ogg", "door.ogg",
            "encounter.ogg", "levelup.ogg", "flee.ogg"} else (
            MUSIC if rel.suffix == ".ogg" else "Cue sheet for the OpenRTP score (CC BY 4.0 / CC0 cues) — see file")
    if top == "fonts":
        return "Press Start 2P by CodeMan38 — SIL Open Font License 1.1 (`OFL.txt`)" if name.endswith(".ttf") \
            else "SIL Open Font License 1.1 text for Press Start 2P"
    if top == "sprites" and name.startswith("interior_door_"):
        return f"{OPENRTP} CharSet/Object1.png door poses — CC BY 4.0 (Tom Lemmens)"
    if top in ("tiles", "sprites") and name.startswith(("town_", "interior_")):
        if name in RETRO_TILES or stem in RETRO_SPRITES:
            return RETRO_CHIPSET
        sheet = "Exterior" if name.startswith("town_") else "Interior"
        return f"{OPENRTP} ChipSet/{sheet}.png crop — CC0 (Jason Perry / finalbossblues)"
    if top == "tiles":
        if name in RETRO_TILES:
            return RETRO_CHIPSET
        if name in SEA_TILES:
            return REPO + " (procedural sea on the ART_BIBLE 2.4 water ramps)"
        return CHIPSET
    if top == "backdrops":
        if stem in ("meadow", "fen"):
            return CHIPSET + "; embeds a retro_Exterior building (CC BY / CC0 mix, see AUTHORS.md)"
        return CHIPSET
    if top == "ui":
        if name.startswith(("window", "gauge", "digits")):
            return f"{OPENRTP} System crop — CC BY 4.0 / OGA-BY (andwhyisit)"
        if name == "gameover_plate.png":
            return f"{OPENRTP} GameOver plate — CC0 (jetrotal) / CC BY 4.0 font (kou)"
        return f"{OPENRTP} Panorama crop — CC BY 4.0 / OGA-BY (Tom Lemmens)"
    if top == "fx":
        return f"{OPENRTP} Battle effect crop — CC0 (Someoneman)"
    if top == "icons":
        return f"{OPENRTP} ChipSet (Interior/Dungeon) crop, some repository-drawn in-dialect — CC0"
    if top == "sprites":
        if name.startswith("face_"):
            return FACESET
        if name.startswith(("hero_", "npc_")):
            return CHARSET_BY
        base = stem.replace("_battle", "")
        if name.endswith("_battle.png") or base in BATTLE_BASES:
            return REPO if base in REPO_SPRITES else MONSTER
        if stem in RETRO_BUILDINGS:
            return RETRO_BUILDING  # SNES-retro walking-scale architecture
        if stem in RETRO_SPRITES:
            return RETRO_CHIPSET  # 10.3 retro pass: flora, props, cave art
        return CHIPSET  # props, landmarks: ChipSet crops/composites
    if name == "title_atlas.png":
        return f"{OPENRTP} Title plate — CC0 (Francisco de la Peña)"
    if name == "title_bg.png":
        return REPO
    return "See CREDITS.md"


def main():
    files = [p for p in sorted(ASSETS.rglob("*")) if p.is_file()]
    runtime = [p for p in files if "source" not in p.relative_to(ASSETS).parts]
    lines = [
        "# ART_MANIFEST.md — asset manifest (GENERATED, do not hand-edit)",
        "",
        "Generated by `tools/generate_manifest.py` from the files on disk; "
        "checked by `tools/validate_assets.py` (disk ↔ manifest parity is a CI gate).",
        "",
        f"**{len(files)} files** under `assets/` — {len(runtime)} runtime-ready files "
        f"plus {len(files) - len(runtime)} vendored source files "
        "kept for provenance and regeneration (`tools/import_openrtp.py`).",
        "",
        "Provenance follows the *source sheet*, never the last tool that touched a "
        "file. The one dialect is **EasyRPG/OpenRTP** (`assets/source/openrtp/`, rev "
        "`993d88c`); repository-authored pieces are in-dialect additions credited CC0 "
        "in [CREDITS.md](CREDITS.md) — they do not replace, and may not be relabelled "
        "as, the OpenRTP art. Full contract: [docs/ART_BIBLE.md](docs/ART_BIBLE.md).",
        "",
        "| Path | Size | Dimensions / note | License / provenance |",
        "|---|---|---|---|",
    ]
    for p in files:
        rel = p.relative_to(ROOT)
        arel = p.relative_to(ASSETS)
        wh = png_size(p) if p.suffix == ".png" else None
        dims = f"{wh[0]}×{wh[1]} png · {category(arel)}" if wh else category(arel)
        lines.append(f"| `{rel.as_posix()}` | {p.stat().st_size} B | {dims} | {provenance(arel)} |")
    lines += [
        "",
        "Music note: `victory.ogg` is a fanfare render (`JFanfare 1`) and is covered "
        "by the CC BY 4.0 note above; the 13-cue mapping is in `assets/audio/SCORE.txt`.",
        "",
    ]
    (ROOT / "ART_MANIFEST.md").write_text("\n".join(lines))
    print(f"wrote ART_MANIFEST.md with {len(files)} entries")


if __name__ == "__main__":
    main()
