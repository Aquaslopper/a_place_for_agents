#!/usr/bin/env python3
"""Build interior room stamps and 16-mask wall-cap matrices from existing art.

Run from the repository root (or any directory):
    python3 tools/build_interior_plates.py
    python3 tools/build_interior_plates.py --check

Outputs stay under docs/town_assets/: eight 7x7-tile room stamps, semantic JSON
sidecars, six 16-mask cap families composed from the existing wall-face atlas
and floor swatches, plus review contact sheets. Runtime assets are never
modified. The north wall profile follows ART_BIBLE.md §10.1 exactly:
cap at y-2, upper face at y-1, base face at y, walkable floor begins at y+1.
"""
from __future__ import annotations

import argparse
import json
from collections import deque
from io import BytesIO
from pathlib import Path
from typing import Dict, Iterable, List, Sequence, Set, Tuple

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets"
OUT = ROOT / "docs" / "town_assets"
CAP_DIR = OUT / "interior_wall_caps"
STAMP_DIR = OUT / "interior_stamps"
TILE = 16
STAMP_W = 7
STAMP_H = 7
STAMP_PX = (STAMP_W * TILE, STAMP_H * TILE)
MASK_BITS = {"N": 1, "E": 2, "S": 4, "W": 8}
MASK_DIRECTIONS = (("N", 0, -1), ("E", 1, 0), ("S", 0, 1), ("W", -1, 0))

WALL_ATLAS = Image.open(ASSETS / "tiles" / "interior_wall_face_atlas.png").convert("RGBA")

# The six two-row face bands in interior_wall_face_atlas.png, ordered exactly
# as registered in ART_BIBLE.md §10. Their cap top surfaces reuse existing
# in-dialect floor tiles; exposed lips borrow native pixels from the same face.
MATERIALS = {
    "purple_brick": {"pair": 0, "cap_floor": "brick_dark"},
    "plaster": {"pair": 1, "cap_floor": "tile_floor"},
    "pale_stone": {"pair": 2, "cap_floor": "stone_floor"},
    "dark_stone": {"pair": 3, "cap_floor": "dungeon_floor"},
    "rubble": {"pair": 4, "cap_floor": "cave_floor_0"},
    "blue_brick": {"pair": 5, "cap_floor": "flagstone"},
}
FLOORS = {
    "wood": ("IN_FLOOR_WOOD", "plank_floor"),
    "tile": ("IN_FLOOR_TILE", "tile_floor"),
    "stone": ("IN_FLOOR_STONE", "stone_floor"),
    "carpet": ("IN_CARPET", "carpet"),
}

SPRITE_NAMES = (
    "interior_open_doorway", "interior_window_arch", "interior_window_shutters",
    "interior_window_stained", "interior_painting_small", "interior_painting_wide",
    "interior_corner_counter", "interior_bookcase_narrow", "interior_shop_bottles",
    "interior_shop_swords", "interior_shop_armor", "interior_shop_herbs",
    "interior_bed_side", "interior_piano", "interior_crystal_ball",
    "interior_royal_chair", "interior_throne", "interior_ritual_circle",
    "interior_stone_stairs", "counter", "shelf", "bed", "table", "longtable",
    "stool", "forge_hearth", "forge_anvil", "stair_up", "stair_down", "chest_closed",
    "barrel", "crate", "statue", "pillar", "urn", "lantern", "potted_plant",
)


def load_rgba(path: Path) -> Image.Image:
    if not path.is_file():
        raise FileNotFoundError(f"Required existing art is missing: {path.relative_to(ROOT)}")
    with Image.open(path) as src:
        return src.convert("RGBA")


TILES = {name: load_rgba(ASSETS / "tiles" / f"{name}.png") for name in {
    "brick_dark", "tile_floor", "stone_floor", "dungeon_floor", "cave_floor_0",
    "flagstone", "plank_floor", "carpet",
}}
SPRITES = {name: load_rgba(ASSETS / "sprites" / f"{name}.png") for name in SPRITE_NAMES}


def face_cell(material: str, column: int, lower: bool = False) -> Image.Image:
    pair = MATERIALS[material]["pair"]
    row = pair * 2 + (1 if lower else 0)
    return WALL_ATLAS.crop((column * TILE, row * TILE, (column + 1) * TILE, (row + 1) * TILE))


def wall_cap(material: str, mask: int) -> Image.Image:
    """Compose one top-surface cap cell; mask bits mean adjacent wall cells."""
    base_name = MATERIALS[material]["cap_floor"]
    cap = TILES[base_name].copy()
    upper = face_cell(material, 1, False)
    lower = face_cell(material, 1, True)
    # These 3px source strips preserve the palette/texture of the corresponding
    # OpenRTP face. Missing neighbours expose a small rim; joined edges stay
    # continuous and use the cap's native floor texture.
    strips = {
        "N": (upper.crop((0, 0, TILE, 3)), (0, 0)),
        "E": (upper.crop((TILE - 3, 0, TILE, TILE)), (TILE - 3, 0)),
        "S": (lower.crop((0, TILE - 3, TILE, TILE)), (0, TILE - 3)),
        "W": (upper.crop((0, 0, 3, TILE)), (0, 0)),
    }
    for direction, bit in MASK_BITS.items():
        if not mask & bit:
            piece, pos = strips[direction]
            cap.alpha_composite(piece, pos)
    return cap


def build_cap_catalog() -> Dict[Tuple[str, int], Image.Image]:
    return {(material, mask): wall_cap(material, mask)
            for material in MATERIALS for mask in range(16)}


def create_floor(width: int, height: int, floor_key: str) -> Image.Image:
    _tag, tile_name = FLOORS[floor_key]
    tile = TILES[tile_name]
    image = Image.new("RGBA", (width * TILE, height * TILE), (0, 0, 0, 0))
    for y in range(height):
        for x in range(width):
            image.alpha_composite(tile, (x * TILE, y * TILE))
    return image


def add_object(image: Image.Image, name: str, anchor: Tuple[int, int], *,
               anchor_kind: str = "foot", role: str = "prop") -> None:
    sprite = SPRITES[name]
    ax, ay = anchor
    if anchor_kind == "center":
        x = ax * TILE + (TILE - sprite.width) // 2
        y = ay * TILE + (TILE - sprite.height) // 2
    else:
        x = ax * TILE + (TILE - sprite.width) // 2
        y = (ay + 1) * TILE - sprite.height
    image.alpha_composite(sprite, (int(x), int(y)))


def compose_north_wall(image: Image.Image, semantic: List[List[str]],
                       cap_masks: List[List[int | None]], material: str,
                       caps: Dict[Tuple[str, int], Image.Image], width: int) -> None:
    """Stamp a seven-cell horizontal wall with the registered three-row profile."""
    wall_cells = {(x, 0) for x in range(width)}
    for x in range(width):
        mask = 0
        for direction, dx, dy in MASK_DIRECTIONS:
            if (x + dx, dy) in wall_cells:
                mask |= MASK_BITS[direction]
        image.alpha_composite(caps[(material, mask)], (x * TILE, 0))
        semantic[0][x] = "IN_WALL_CAP"
        cap_masks[0][x] = mask

        upper = face_cell(material, x % 4, False)
        base = face_cell(material, x % 4, True)
        image.alpha_composite(upper, (x * TILE, TILE))
        image.alpha_composite(base, (x * TILE, TILE * 2))
        semantic[1][x] = "IN_WALL_FACE"
        semantic[2][x] = "IN_WALL_FACE"


def stamp_data(name: str, floor_key: str, wall_material: str,
               caps: Dict[Tuple[str, int], Image.Image]) -> Tuple[Image.Image, dict]:
    semantic = [[FLOORS[floor_key][0] for _ in range(STAMP_W)] for _ in range(STAMP_H)]
    cap_masks: List[List[int | None]] = [[None for _ in range(STAMP_W)] for _ in range(STAMP_H)]
    image = create_floor(STAMP_W, STAMP_H, floor_key)
    compose_north_wall(image, semantic, cap_masks, wall_material, caps, STAMP_W)
    props: List[dict] = []
    solid: Set[Tuple[int, int]] = set()
    targets: Dict[str, List[int]] = {}
    portals: List[dict] = []

    def place(asset: str, at: Tuple[int, int], *, cells: Iterable[Tuple[int, int]] = (),
              role: str, anchor_kind: str = "foot") -> None:
        add_object(image, asset, at, anchor_kind=anchor_kind, role=role)
        cells_list = sorted({tuple(p) for p in cells}, key=lambda p: (p[1], p[0]))
        solid.update(cells_list)
        props.append({"asset": asset, "anchor_tile": list(at), "anchor_kind": anchor_kind,
                      "role": role, "solid_cells": [list(p) for p in cells_list]})

    if name == "entry_mat":
        # A real two-tile-wide exit socket, carpeted landing, and no solid prop.
        semantic[6][3] = "IN_DOOR_EXIT"
        portals.append({"id": "door_exit", "tag": "IN_DOOR_EXIT", "tx": 3, "ty": 6,
                        "target": "parent_map", "target_portal": "entry"})
        for y in (4, 5):
            for x in (2, 3, 4, 5):
                image.alpha_composite(TILES["carpet"], (x * TILE, y * TILE))
                semantic[y][x] = "IN_CARPET"
        # The doorway is visible at the south edge; it sits outside the clear
        # two-by-two landing, while the semantic portal remains on the floor cell.
        add_object(image, "interior_open_doorway", (3, 6), anchor_kind="foot", role="door_art")
        props.append({"asset": "interior_open_doorway", "anchor_tile": [3, 6],
                      "anchor_kind": "foot", "role": "door_art", "solid_cells": []})
        targets["parent_exit"] = [3, 6]

    elif name == "counter_bay":
        place("interior_window_arch", (1, 1), role="wall_window")
        place("interior_painting_small", (5, 1), role="wall_art")
        place("counter", (3, 3), cells={(3, 3)}, role="keeper_counter")
        place("interior_shop_bottles", (5, 3), cells={(5, 3)}, role="counter_goods")
        targets["counter_approach"] = [3, 4]

    elif name == "hearth_bay":
        place("forge_hearth", (2, 2), role="wall_hearth")
        place("forge_anvil", (5, 3), cells={(5, 3), (5, 4)}, role="anvil")
        targets["hearth_approach"] = [3, 4]
        targets["anvil_approach"] = [4, 4]

    elif name == "bunk_wing":
        place("bed", (1, 4), cells={(1, 3), (1, 4)}, role="bed_left")
        place("bed", (5, 4), cells={(5, 3), (5, 4)}, role="bed_right")
        place("table", (3, 3), cells={(3, 3)}, role="bedside_table")
        place("lantern", (0, 2), role="wall_lantern")
        targets["left_bed_approach"] = [2, 4]
        targets["right_bed_approach"] = [4, 4]

    elif name == "shop_display":
        place("shelf", (1, 2), role="north_wall_shelf")
        place("interior_shop_swords", (1, 3), cells={(1, 3)}, role="weapon_display")
        place("interior_shop_armor", (5, 3), cells={(5, 3)}, role="armor_display")
        place("interior_shop_herbs", (6, 3), cells={(6, 3)}, role="herb_display")
        place("counter", (3, 3), cells={(3, 3)}, role="shop_counter")
        targets["weapon_approach"] = [2, 4]
        targets["counter_approach"] = [3, 4]
        targets["armor_approach"] = [4, 4]

    elif name == "temple_nave":
        place("interior_ritual_circle", (1, 4), anchor_kind="center",
              cells={(0, 3), (1, 3), (2, 3), (0, 4), (1, 4), (2, 4), (0, 5), (1, 5), (2, 5)},
              role="ritual_circle")
        place("statue", (5, 3), cells={(5, 3)}, role="altar_statue")
        place("interior_window_stained", (3, 1), role="north_window")
        targets["circle_approach"] = [3, 4]
        targets["altar_approach"] = [4, 4]

    elif name == "study_library":
        place("shelf", (1, 2), role="north_wall_shelf")
        place("interior_bookcase_narrow", (0, 3), cells={(0, 3), (0, 4)}, role="bookcase_left")
        place("interior_bookcase_narrow", (6, 3), cells={(6, 3), (6, 4)}, role="bookcase_right")
        place("table", (4, 3), cells={(4, 3)}, role="study_table")
        place("interior_crystal_ball", (3, 3), cells={(3, 3)}, role="crystal_ball")
        targets["left_shelf_approach"] = [1, 4]
        targets["table_approach"] = [5, 4]
        targets["right_shelf_approach"] = [5, 3]

    elif name == "cellar_stair":
        for x, asset, role in ((1, "stair_up", "stair_up"), (5, "stair_down", "stair_down")):
            semantic[3][x] = "IN_STAIR_UP" if asset == "stair_up" else "IN_STAIR_DOWN"
            place(asset, (x, 3), cells={(x, 3)}, role=role)
        place("barrel", (0, 3), cells={(0, 3)}, role="cellar_barrel")
        place("crate", (6, 3), cells={(6, 3)}, role="cellar_crate")
        targets["stair_up_approach"] = [2, 4]
        targets["stair_down_approach"] = [4, 4]

    else:
        raise ValueError(f"Unknown interior stamp: {name}")

    sockets = {"S": [[3, 6], [4, 6]]}
    landing = [[x, y] for y in (4, 5) for x in (3, 4)]
    walkable = [
        [semantic[y][x] not in ("IN_WALL_CAP", "IN_WALL_FACE", "IN_VOID") and (x, y) not in solid
         for x in range(STAMP_W)]
        for y in range(STAMP_H)
    ]
    data = {
        "schema_version": 1,
        "name": name,
        "kind": "interior_stamp",
        "width": STAMP_W,
        "height": STAMP_H,
        "tile_px": TILE,
        "pixel_size": list(STAMP_PX),
        "wall_material": wall_material,
        "layers": {
            "ground": semantic,
            "wall_cap_mask": cap_masks,
            "wall_upper_face": [[x < STAMP_W for x in range(STAMP_W)] if y == 1 else [False] * STAMP_W
                                 for y in range(STAMP_H)],
            "wall_base_face": [[x < STAMP_W for x in range(STAMP_W)] if y == 2 else [False] * STAMP_W
                                for y in range(STAMP_H)],
            "walkable": walkable,
        },
        "solid": [list(p) for p in sorted(solid, key=lambda p: (p[1], p[0]))],
        "objects": props,
        "portals": portals,
        "sockets": sockets,
        "entry_landing": landing,
        "approach_targets": targets,
        "wall_profile": {"cap_row": 0, "upper_face_row": 1, "base_face_row": 2,
                         "floor_starts_row": 3},
    }
    return image, data


def make_all_stamps(caps: Dict[Tuple[str, int], Image.Image]) -> Dict[str, Tuple[Image.Image, dict]]:
    specs = {
        "entry_mat": ("wood", "plaster"),
        "counter_bay": ("wood", "purple_brick"),
        "hearth_bay": ("stone", "dark_stone"),
        "bunk_wing": ("wood", "plaster"),
        "shop_display": ("tile", "blue_brick"),
        "temple_nave": ("stone", "pale_stone"),
        "study_library": ("wood", "purple_brick"),
        "cellar_stair": ("stone", "rubble"),
    }
    return {name: stamp_data(name, floor, wall, caps)
            for name, (floor, wall) in specs.items()}


def reachable(width: int, height: int, walkable: Sequence[Sequence[bool]], start: Tuple[int, int]) -> Set[Tuple[int, int]]:
    if not (0 <= start[0] < width and 0 <= start[1] < height) or not walkable[start[1]][start[0]]:
        return set()
    seen = {start}
    queue = deque([start])
    while queue:
        x, y = queue.popleft()
        for nx, ny in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
            if 0 <= nx < width and 0 <= ny < height and walkable[ny][nx] and (nx, ny) not in seen:
                seen.add((nx, ny))
                queue.append((nx, ny))
    return seen


def verify(caps: Dict[Tuple[str, int], Image.Image], stamps: Dict[str, Tuple[Image.Image, dict]]) -> List[str]:
    errors: List[str] = []
    if len(caps) != len(MATERIALS) * 16:
        errors.append(f"wall cap count is {len(caps)}, expected {len(MATERIALS) * 16}")
    expected_stamps = {"entry_mat", "counter_bay", "hearth_bay", "bunk_wing", "shop_display",
                       "temple_nave", "study_library", "cellar_stair"}
    if set(stamps) != expected_stamps:
        errors.append(f"stamp names do not match expected set: {sorted(stamps)}")
    for material in MATERIALS:
        variants = [caps[(material, mask)] for mask in range(16)]
        if any(image.size != (TILE, TILE) for image in variants):
            errors.append(f"{material}: cap tile is not 16x16")
        if len({image.tobytes() for image in variants}) != 16:
            errors.append(f"{material}: expected 16 distinct adjacency masks")
    for name, (image, data) in stamps.items():
        if image.size != STAMP_PX:
            errors.append(f"{name}: image size {image.size}, expected {STAMP_PX}")
        solids = {tuple(p) for p in data["solid"]}
        landing = {tuple(p) for p in data["entry_landing"]}
        if solids & landing:
            errors.append(f"{name}: solid prop in 2x2 entry landing: {sorted(solids & landing)}")
        walk = data["layers"]["walkable"]
        reached = reachable(STAMP_W, STAMP_H, walk, (3, 6))
        if (4, 6) not in reached:
            errors.append(f"{name}: two-cell south socket is disconnected")
        for label, cell in data["approach_targets"].items():
            if tuple(cell) not in reached:
                errors.append(f"{name}: approach target {label} at {cell} is unreachable")
        for portal in data["portals"]:
            if not walk[portal["ty"]][portal["tx"]]:
                errors.append(f"{name}: portal {portal['id']} is not walkable")
        if data["wall_profile"] != {"cap_row": 0, "upper_face_row": 1, "base_face_row": 2, "floor_starts_row": 3}:
            errors.append(f"{name}: north wall profile diverges from ART_BIBLE §10.1")
    return errors


def cap_sheet(caps: Dict[Tuple[str, int], Image.Image]) -> Image.Image:
    scale, pad, label_w, label_h = 3, 5, 100, 22
    cols = 16
    rows = len(MATERIALS)
    cw, ch = TILE * scale, TILE * scale
    sheet = Image.new("RGB", (label_w + pad + cols * (cw + pad), pad + rows * (ch + label_h + pad)), (20, 27, 40))
    draw = ImageDraw.Draw(sheet)
    for row, material in enumerate(MATERIALS):
        y = pad + row * (ch + label_h + pad)
        draw.text((4, y + 6), material, fill=(240, 218, 163))
        for mask in range(16):
            x = label_w + pad + mask * (cw + pad)
            draw.text((x + 2, y + 2), f"{mask:02d}", fill=(189, 208, 232))
            tile = caps[(material, mask)].resize((cw, ch), Image.Resampling.NEAREST)
            sheet.paste(tile, (x, y + label_h), tile)
    return sheet


def stamp_sheet(stamps: Dict[str, Tuple[Image.Image, dict]]) -> Image.Image:
    cols, scale, pad, label_h = 4, 2, 8, 18
    cw, ch = STAMP_PX[0] * scale, STAMP_PX[1] * scale
    names = list(stamps)
    rows = (len(names) + cols - 1) // cols
    sheet = Image.new("RGB", (pad + cols * (cw + pad), pad + rows * (ch + label_h + pad)), (20, 27, 40))
    draw = ImageDraw.Draw(sheet)
    for idx, name in enumerate(names):
        r, c = divmod(idx, cols)
        x, y = pad + c * (cw + pad), pad + r * (ch + label_h + pad)
        draw.text((x + 2, y + 2), name, fill=(240, 218, 163))
        image = stamps[name][0].resize((cw, ch), Image.Resampling.NEAREST)
        sheet.paste(image, (x, y + label_h), image)
    return sheet


def image_bytes(image: Image.Image) -> bytes:
    stream = BytesIO()
    image.save(stream, format="PNG")
    return stream.getvalue()


def output_payloads(caps: Dict[Tuple[str, int], Image.Image],
                    stamps: Dict[str, Tuple[Image.Image, dict]]) -> Dict[Path, bytes]:
    payloads: Dict[Path, bytes] = {}
    for (material, mask), image in caps.items():
        payloads[CAP_DIR / f"cap_{material}_{mask:02d}.png"] = image_bytes(image)
    cap_index = {
        "schema_version": 1,
        "tile_px": TILE,
        "mask_bits": MASK_BITS,
        "mask_rule": "bit is set when a wall-cap neighbour exists in that cardinal direction",
        "materials": [{"name": name, **data} for name, data in MATERIALS.items()],
        "files": [f"cap_{material}_{mask:02d}.png" for material in MATERIALS for mask in range(16)],
    }
    payloads[CAP_DIR / "index.json"] = (json.dumps(cap_index, indent=2) + "\n").encode()
    for name, (image, data) in stamps.items():
        payloads[STAMP_DIR / f"{name}.png"] = image_bytes(image)
        payloads[STAMP_DIR / f"{name}.json"] = (json.dumps(data, indent=2) + "\n").encode()
    stamp_index = {
        "schema_version": 1,
        "tile_px": TILE,
        "stamp_size_tiles": [STAMP_W, STAMP_H],
        "stamp_size_px": list(STAMP_PX),
        "stamps": [stamps[name][1] for name in stamps],
    }
    payloads[STAMP_DIR / "index.json"] = (json.dumps(stamp_index, indent=2) + "\n").encode()
    payloads[OUT / "plates_walls.png"] = image_bytes(cap_sheet(caps))
    payloads[OUT / "interior_stamps.png"] = image_bytes(stamp_sheet(stamps))
    return payloads


def write_outputs(payloads: Dict[Path, bytes]) -> None:
    CAP_DIR.mkdir(parents=True, exist_ok=True)
    STAMP_DIR.mkdir(parents=True, exist_ok=True)
    expected = set(payloads)
    # Delete stale generated PNG/JSON outputs only inside the two generated dirs.
    for directory in (CAP_DIR, STAMP_DIR):
        for path in directory.iterdir():
            if path.is_file() and path.suffix.lower() in {".png", ".json"} and path not in expected:
                path.unlink()
    for path, content in payloads.items():
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)


def check_outputs(payloads: Dict[Path, bytes]) -> List[str]:
    errors: List[str] = []
    for path, expected in payloads.items():
        if not path.is_file() or path.read_bytes() != expected:
            errors.append(f"missing or stale: {path.relative_to(ROOT)}")
    for directory in (CAP_DIR, STAMP_DIR):
        expected_in_dir = {path for path in payloads if path.parent == directory}
        actual = {path for path in directory.glob("*.png")} | {path for path in directory.glob("*.json")}
        extras = sorted(path.name for path in actual - expected_in_dir)
        if extras:
            errors.append(f"stale generated files in {directory.relative_to(ROOT)}: {', '.join(extras)}")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true", help="verify cap masks, room navigation, and generated outputs")
    args = parser.parse_args()
    caps = build_cap_catalog()
    stamps = make_all_stamps(caps)
    errors = verify(caps, stamps)
    if errors:
        for error in errors:
            print(f"FAIL: {error}")
        return 1
    payloads = output_payloads(caps, stamps)
    if args.check:
        errors = check_outputs(payloads)
        if errors:
            for error in errors:
                print(f"FAIL: {error}")
            return 1
        print(f"Interior plates: OK — {len(caps)} wall-cap variants, {len(stamps)} room stamps, and reachability verified")
    else:
        write_outputs(payloads)
        print(f"Interior plates: wrote {len(caps)} wall-cap variants and {len(stamps)} room stamps under {OUT.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
