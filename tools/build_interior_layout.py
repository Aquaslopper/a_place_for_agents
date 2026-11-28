#!/usr/bin/env python3
"""Generate a deterministic, multi-wing interior from the town stamp catalog.

Examples:
    python3 tools/build_interior_layout.py --seed 4444 --kind shop --check
    python3 tools/build_interior_layout.py --seed 1337 --kind inn --parent-map town:4444:hamlet --site-id interior:1337:inn

The layout snaps reusable 7x7 stamp furniture/wall bays around a clear two-tile
circulation spine, composes the north-facing three-row wall profile and cap
matrix, then checks room reachability and the clear two-by-two entry landing.
Outputs are design-time JSON/PNG previews under docs/town_assets/layouts/.
"""
from __future__ import annotations

import argparse
import json
import random
import sys
from collections import deque
from io import BytesIO
from pathlib import Path
from typing import Dict, Iterable, List, Sequence, Set, Tuple

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets"
TOWN_ASSETS = ROOT / "docs" / "town_assets"
CAP_DIR = TOWN_ASSETS / "interior_wall_caps"
STAMP_DIR = TOWN_ASSETS / "interior_stamps"
OUT_DIR = TOWN_ASSETS / "layouts"
TILE = 16
KINDS = ("shop", "inn", "smithy", "temple", "house", "manor")
SIZE_RANGES = {
    "shop": ((17, 19), (12, 14)),
    "inn": ((18, 20), (13, 15)),
    "smithy": ((17, 19), (12, 14)),
    "temple": ((20, 22), (14, 15)),
    "house": ((15, 17), (11, 12)),
    "manor": ((22, 24), (15, 16)),
}
KIND_THEME = {
    "shop": {"floor": ["wood", "tile"], "wall": ["purple_brick", "plaster", "blue_brick"]},
    "inn": {"floor": ["wood", "tile"], "wall": ["plaster", "purple_brick"]},
    "smithy": {"floor": ["stone"], "wall": ["dark_stone", "rubble"]},
    "temple": {"floor": ["stone", "tile"], "wall": ["pale_stone", "blue_brick"]},
    "house": {"floor": ["wood"], "wall": ["plaster", "purple_brick"]},
    "manor": {"floor": ["wood", "tile"], "wall": ["blue_brick", "pale_stone", "purple_brick"]},
}
FLOOR_INFO = {
    "wood": ("IN_FLOOR_WOOD", "plank_floor"),
    "tile": ("IN_FLOOR_TILE", "tile_floor"),
    "stone": ("IN_FLOOR_STONE", "stone_floor"),
    "carpet": ("IN_CARPET", "carpet"),
}
SEMANTIC_TAGS_BY_ROLE = {
    "keeper_counter": "IN_COUNTER", "counter_goods": "IN_COUNTER", "shop_counter": "IN_COUNTER",
    "wall_hearth": "IN_HEARTH", "north_wall_shelf": "IN_SHELF",
    "bookcase_left": "IN_SHELF", "bookcase_right": "IN_SHELF",
    "herb_display": "IN_SHELF", "weapon_display": "IN_RACK_WEAPONS",
    "armor_display": "IN_RACK_ARMOR", "bed_left": "IN_BED", "bed_right": "IN_BED",
    "bedside_table": "IN_TABLE", "study_table": "IN_TABLE", "dining_table": "IN_TABLE",
    "family_table": "IN_TABLE",
    "ritual_circle": "IN_ALTAR", "altar_statue": "IN_ALTAR",
    "stair_up": "IN_STAIR_UP", "stair_down": "IN_STAIR_DOWN",
    "upper_floor_stair": "IN_STAIR_UP", "family_chest": "IN_CHEST",
    "manor_throne": "IN_THRONE",
}


def load_rgba(path: Path) -> Image.Image:
    if not path.is_file():
        raise FileNotFoundError(f"Required input missing: {path.relative_to(ROOT)}")
    with Image.open(path) as source:
        return source.convert("RGBA")


WALL_ATLAS = load_rgba(ASSETS / "tiles" / "interior_wall_face_atlas.png")
TILES = {name: load_rgba(ASSETS / "tiles" / f"{name}.png") for name in
         ("plank_floor", "tile_floor", "stone_floor", "carpet")}


def load_caps() -> Tuple[Dict[Tuple[str, int], Image.Image], dict]:
    index_path = CAP_DIR / "index.json"
    if not index_path.is_file():
        raise FileNotFoundError(f"Wall-cap index missing: {index_path.relative_to(ROOT)}; run tools/build_interior_plates.py")
    index = json.loads(index_path.read_text(encoding="utf-8"))
    caps = {}
    for material in index["materials"]:
        name = material["name"]
        for mask in range(16):
            path = CAP_DIR / f"cap_{name}_{mask:02d}.png"
            caps[(name, mask)] = load_rgba(path)
    if len(caps) != len(index["materials"]) * 16:
        raise ValueError("Wall-cap matrix is incomplete")
    return caps, index


def load_stamps() -> Dict[str, dict]:
    index_path = STAMP_DIR / "index.json"
    if not index_path.is_file():
        raise FileNotFoundError(f"Interior stamp index missing: {index_path.relative_to(ROOT)}; run tools/build_interior_plates.py")
    index = json.loads(index_path.read_text(encoding="utf-8"))
    result = {}
    for entry in index["stamps"]:
        name = entry["name"]
        sidecar = STAMP_DIR / f"{name}.json"
        result[name] = json.loads(sidecar.read_text(encoding="utf-8"))
    return result


def face_cell(pair: int, column: int, lower: bool = False) -> Image.Image:
    row = pair * 2 + (1 if lower else 0)
    return WALL_ATLAS.crop((column * TILE, row * TILE, (column + 1) * TILE, (row + 1) * TILE))


def in_bounds(point: Tuple[int, int], width: int, height: int) -> bool:
    x, y = point
    return 0 <= x < width and 0 <= y < height


def generate_layout(seed: int, kind: str, *, site_id: str | None = None,
                    parent_map: str = "parent_map", parent_portal: str = "entry") -> dict:
    if kind not in KINDS:
        raise ValueError(f"--kind must be one of {', '.join(KINDS)}")
    rng = random.Random(seed)
    (wmin, wmax), (hmin, hmax) = SIZE_RANGES[kind]
    width = rng.randint(wmin, wmax)
    height = rng.randint(hmin, hmax)
    floor_key = rng.choice(KIND_THEME[kind]["floor"])
    wall_material = rng.choice(KIND_THEME[kind]["wall"])
    floor_tag, floor_asset = FLOOR_INFO[floor_key]
    wall_info = next(item for item in cap_cache_index["materials"] if item["name"] == wall_material)
    wall_pair = wall_info["pair"]
    center = width // 2
    # A mid-wall bay makes the room genuinely multi-wing without severing the
    # central two-cell aisle. Short houses use the open plan but keep the same
    # spine, door socket and wall profile.
    partition_y = min(max(4, height // 2), height - 7) if height >= 13 else None
    site_id = site_id or f"interior:{seed}:{kind}"

    semantic = [[floor_tag for _ in range(width)] for _ in range(height)]
    cap_masks: List[List[int | None]] = [[None for _ in range(width)] for _ in range(height)]
    face_rows = [[0 for _ in range(width)] for _ in range(height)]  # 1=upper, 2=base
    image = Image.new("RGBA", (width * TILE, height * TILE), (0, 0, 0, 0))
    floor_tile = TILES[floor_asset]
    for y in range(height):
        for x in range(width):
            image.alpha_composite(floor_tile, (x * TILE, y * TILE))

    solids: Set[Tuple[int, int]] = set()
    wall_cells: Set[Tuple[int, int]] = set()
    objects: List[dict] = []
    approach_targets: Dict[str, List[int]] = {}
    npcs: List[dict] = []
    chests: List[dict] = []
    switches: List[dict] = []

    def draw_wall_bay(top_y: int, *, opening: Set[int] | None = None) -> None:
        opening = opening or set()
        cap_plane = {(x, top_y) for x in range(1, width - 1) if x not in opening}
        pair = wall_material
        caps = cap_cache
        for x in range(1, width - 1):
            if x in opening:
                continue
            mask = 0
            if (x - 1, top_y) in cap_plane:
                mask |= 8
            if (x + 1, top_y) in cap_plane:
                mask |= 2
            image.alpha_composite(caps[(pair, mask)], (x * TILE, top_y * TILE))
            semantic[top_y][x] = "IN_WALL_CAP"
            cap_masks[top_y][x] = mask
            for dy, lower, row_id in ((1, False, 1), (2, True, 2)):
                y = top_y + dy
                if y >= height:
                    continue
                face = face_cell(wall_pair, x % 4, lower)
                image.alpha_composite(face, (x * TILE, y * TILE))
                semantic[y][x] = "IN_WALL_FACE"
                face_rows[y][x] = row_id
                solids.add((x, y))
                wall_cells.add((x, y))
            solids.add((x, top_y))
            wall_cells.add((x, top_y))

    # Outer north wall, side walls, a southern sill with a two-tile exit, and
    # one optional mid-room wall bay with a centered two-cell passage.
    draw_wall_bay(0)
    for y in range(height):
        for x in (0, width - 1):
            image.alpha_composite(face_cell(wall_pair, x % 4, lower=True),
                                  (x * TILE, y * TILE))
            semantic[y][x] = "IN_WALL_FACE"
            face_rows[y][x] = 2
            solids.add((x, y))
            wall_cells.add((x, y))
    if partition_y is not None:
        draw_wall_bay(partition_y, opening={center - 1, center})
    for x in range(1, width - 1):
        if x not in (center - 1, center):
            y = height - 1
            face = face_cell(wall_pair, x % 4, lower=True)
            image.alpha_composite(face, (x * TILE, y * TILE))
            semantic[y][x] = "IN_WALL_FACE"
            face_rows[y][x] = 2
            solids.add((x, y))
            wall_cells.add((x, y))

    # Clear carpeted entry landing and the north/south circulation spine.
    landing = [[x, y] for y in (height - 3, height - 2) for x in (center - 1, center)]
    spine_cells = {(x, y) for x in (center - 1, center) for y in range(3, height - 1)}
    if partition_y is not None:
        horizontal_spine_y = min(partition_y + 3, height - 4)
    else:
        horizontal_spine_y = max(4, height // 2)
    spine_cells.update((x, horizontal_spine_y) for x in range(1, width - 1))
    # Preserve semantic material tags for the lane without replacing the wall.
    for x, y in spine_cells:
        if semantic[y][x] in ("IN_WALL_CAP", "IN_WALL_FACE"):
            continue
        semantic[y][x] = "IN_CARPET" if y >= height - 3 else floor_tag
        tile = TILES["carpet"] if semantic[y][x] == "IN_CARPET" else floor_tile
        image.alpha_composite(tile, (x * TILE, y * TILE))

    # The south portal is a two-tile opening. Only the inner tile carries the
    # stable portal id; both cells are walkable and align with the landing.
    semantic[height - 1][center] = "IN_DOOR_EXIT"
    semantic[height - 1][center - 1] = floor_tag
    door_sprite = load_rgba(ASSETS / "sprites" / "interior_open_doorway.png")
    door_x = center * TILE + (TILE - door_sprite.width) // 2
    door_y = height * TILE - door_sprite.height
    objects.append({"asset": "interior_open_doorway", "anchor_tile": [center, height - 1],
                    "anchor_kind": "foot", "role": "door_art", "solid_cells": []})
    image.alpha_composite(door_sprite, (door_x, door_y))

    stamp_cache = load_stamps()
    used_solid: Set[Tuple[int, int]] = set()
    module_targets: Dict[str, List[int]] = {}

    def place_stamp(name: str, ox: int, oy: int, omit_roles: Set[str] | None = None,
                    target_prefix: str | None = None) -> None:
        omit_roles = omit_roles or set()
        stamp = stamp_cache[name]
        for obj in stamp["objects"]:
            if obj["role"] in omit_roles:
                continue
            new_obj = dict(obj)
            ax, ay = obj["anchor_tile"]
            new_obj["anchor_tile"] = [ax + ox, ay + oy]
            new_obj["source_stamp"] = name
            cells = [[x + ox, y + oy] for x, y in obj.get("solid_cells", [])]
            new_obj["solid_cells"] = cells
            semantic_tag = SEMANTIC_TAGS_BY_ROLE.get(obj["role"])
            if semantic_tag:
                new_obj["semantic_tag"] = semantic_tag
            for x, y in cells:
                point = (x, y)
                if not in_bounds(point, width, height):
                    raise ValueError(f"{kind}: {name}/{obj['role']} collision outside room at {point}")
                used_solid.add(point)
                if semantic_tag:
                    semantic[y][x] = semantic_tag
            objects.append(new_obj)
        for label, point in stamp["approach_targets"].items():
            px, py = point
            module_targets[f"{target_prefix or name}:{label}"] = [px + ox, py + oy]

    def place_prop(asset: str, anchor: Tuple[int, int], *, role: str,
                   solid_cells: Iterable[Tuple[int, int]] = (), target: str | None = None,
                   approach: Tuple[int, int] | None = None,
                   semantic_tag: str | None = None) -> None:
        cells = sorted({tuple(p) for p in solid_cells}, key=lambda p: (p[1], p[0]))
        semantic_tag = semantic_tag or SEMANTIC_TAGS_BY_ROLE.get(role)
        for x, y in cells:
            if not in_bounds((x, y), width, height):
                raise ValueError(f"{kind}: {role} collision outside room at {(x, y)}")
            used_solid.add((x, y))
            if semantic_tag:
                semantic[y][x] = semantic_tag
        placed_obj = {"asset": asset, "anchor_tile": list(anchor), "anchor_kind": "foot",
                      "role": role, "solid_cells": [list(p) for p in cells]}
        if semantic_tag:
            placed_obj["semantic_tag"] = semantic_tag
        objects.append(placed_obj)
        if target and approach:
            approach_targets[target] = list(approach)

    def add_stamp_landing_carpet() -> None:
        # Reuse the entry-mat floor material in the exact 2x2 clear landing.
        for gx, gy in map(tuple, landing):
            if semantic[gy][gx] not in ("IN_WALL_CAP", "IN_WALL_FACE"):
                semantic[gy][gx] = "IN_CARPET"
                image.alpha_composite(TILES["carpet"], (gx * TILE, gy * TILE))

    # Furniture stamps supply measured OpenRTP props and their collision/approach
    # cells. Their north wall pictures sit against the composed outer wall, while
    # floor furniture is arranged in service and sleeping wings.
    if kind == "shop":
        place_stamp("counter_bay", center - 3, 0, omit_roles={"counter_goods"}, target_prefix="keeper")
        place_stamp("shop_display", 0, 0, omit_roles={"shop_counter"}, target_prefix="display")
        npcs.append({"id": "keeper", "role": "shopkeeper", "tx": center + 1, "ty": 3,
                     "interaction_cell": [center, 4], "solid": False})
    elif kind == "inn":
        place_stamp("counter_bay", center - 3, 0, omit_roles={"counter_goods"}, target_prefix="keeper")
        place_stamp("bunk_wing", 0, 0, target_prefix="bunks")
        stair_x = width - 3
        place_prop("stair_up", (stair_x, 3), role="upper_floor_stair",
                   solid_cells={(stair_x, 3)}, target="stair_up_approach", approach=(stair_x - 1, 3))
        npcs.append({"id": "keeper", "role": "innkeeper", "tx": center + 1, "ty": 3,
                     "interaction_cell": [center, 4], "solid": False})
    elif kind == "smithy":
        place_stamp("hearth_bay", center - 3, 0, omit_roles=set(), target_prefix="forge")
        place_stamp("shop_display", 0, 0, omit_roles={"shop_counter"}, target_prefix="rack")
        npcs.append({"id": "smith", "role": "smith", "tx": center + 1, "ty": 3,
                     "interaction_cell": [center + 1, 4], "solid": False})
    elif kind == "temple":
        place_stamp("temple_nave", center - 3, 0, target_prefix="altar")
        npcs.append({"id": "priest", "role": "priest", "tx": center, "ty": 3,
                     "interaction_cell": [center, 4], "solid": False})
    elif kind == "house":
        place_stamp("bunk_wing", center - 3, 0, omit_roles={"bedside_table"}, target_prefix="beds")
        place_stamp("hearth_bay", width - 7, 0, omit_roles={"anvil"}, target_prefix="fireplace")
    elif kind == "manor":
        place_stamp("study_library", 1, 0, target_prefix="study")
        place_stamp("bunk_wing", width - 8, 0, omit_roles={"bedside_table"}, target_prefix="guest_wing")
        throne_x = center - 3
        place_prop("interior_throne", (throne_x, 3), role="manor_throne",
                   solid_cells={(throne_x, 3), (throne_x, 4)},
                   target="throne_approach", approach=(throne_x + 1, 4))

    # Seeded service/dining furnishings make the south wing legible without
    # narrowing the two-cell entrance spine or touching the protected landing.
    if kind in ("shop", "smithy"):
        place_prop("barrel", (2, height - 4), role="stock_barrel",
                   solid_cells={(2, height - 4)})
        place_prop("crate", (4, height - 4), role="stock_crate",
                   solid_cells={(4, height - 4)})
    elif kind in ("inn", "manor"):
        table_y = max(height - 5, partition_y + 3 if partition_y is not None else height - 5)
        place_prop("longtable", (2, table_y), role="dining_table",
                   solid_cells={(2, table_y), (3, table_y), (4, table_y)},
                   target="dining_table_approach", approach=(3, table_y + 1),
                   semantic_tag="IN_TABLE")
        for x in (2, 4):
            place_prop("stool", (x, table_y + 1), role="dining_stool",
                       solid_cells={(x, table_y + 1)})
    elif kind == "temple":
        place_prop("bench", (2, height - 4), role="nave_bench_west",
                   solid_cells={(2, height - 4)})
        place_prop("bench", (width - 3, height - 4), role="nave_bench_east",
                   solid_cells={(width - 3, height - 4)})
    elif kind == "house":
        place_prop("table", (2, height - 5), role="family_table",
                   solid_cells={(2, height - 5)}, semantic_tag="IN_TABLE")
        place_prop("potted_plant", (width - 4, height - 4), role="hearthside_plant")

    if kind in ("house", "manor"):
        chest_x, chest_y = width - 3, height - 4
        place_prop("chest_closed", (chest_x, chest_y), role="family_chest",
                   solid_cells={(chest_x, chest_y)}, target="chest_approach",
                   approach=(chest_x - 1, chest_y))
        chests.append({"id": "family_chest", "tx": chest_x, "ty": chest_y,
                       "asset": "chest_closed", "approach": [chest_x - 1, chest_y], "opened": False})

    # Copy the two-cell rug grammar from the `entry_mat` stamp into the room's
    # clear landing, then restore the north-to-south path on top of the fill.
    add_stamp_landing_carpet()
    for x, y in spine_cells:
        if semantic[y][x] not in ("IN_WALL_CAP", "IN_WALL_FACE"):
            if y >= height - 3:
                semantic[y][x] = "IN_CARPET"
                image.alpha_composite(TILES["carpet"], (x * TILE, y * TILE))

    # Transform collision objects into the map and reject illegal placements in
    # wall cells or the protected two-by-two entry landing.
    solids = set(solids) | used_solid
    landing_set = {tuple(p) for p in landing}
    wall_set = wall_cells
    solid_landing = solids & landing_set
    solid_wall = used_solid & wall_set
    walkable = [
        [semantic[y][x] not in ("IN_WALL_CAP", "IN_WALL_FACE", "IN_VOID") and (x, y) not in solids
         for x in range(width)]
        for y in range(height)
    ]
    start = (center, height - 2)
    reached = flood(walkable, start)

    # Keep only approach cells in the room and surface module targets in the
    # final map metadata. The seed never decides whether required targets exist.
    for key, point in module_targets.items():
        if in_bounds(tuple(point), width, height):
            approach_targets[key] = point
    if kind in ("shop", "inn"):
        approach_targets.setdefault("keeper_counter", [center, 4])
    if kind == "smithy":
        approach_targets.setdefault("anvil", [center + 1, 4])
    if kind == "temple":
        approach_targets.setdefault("altar", [center + 1, 4])
    if kind == "inn":
        approach_targets.setdefault("beds", [2, 4])
    if kind == "house":
        approach_targets.setdefault("bed", [center - 1, 4])
    if kind == "manor":
        approach_targets.setdefault("throne", [center - 2, 4])
    for npc in npcs:
        approach_targets[f"npc_{npc['id']}_interaction"] = list(npc["interaction_cell"])

    errors = []
    unreachable_count = 0
    if solid_landing:
        errors.append(f"solid prop in two-by-two entry landing: {sorted(solid_landing)}")
    if solid_wall:
        errors.append(f"solid furniture intersects wall cells: {sorted(solid_wall)}")
    if len(used_solid) != sum(1 for obj in objects for _ in obj.get("solid_cells", [])):
        # Duplicate cells may be deliberate in metadata but are a strong signal
        # of overlapping objects, so they are reported and must be corrected.
        errors.append("overlapping solid prop cells")
    if start not in reached:
        errors.append("door exit cannot reach its interior landing")
        unreachable_count += 1
    for npc in npcs:
        cell = (npc["tx"], npc["ty"])
        if not in_bounds(cell, width, height) or not walkable[cell[1]][cell[0]]:
            errors.append(f"NPC {npc['id']} is placed in a solid or out-of-bounds cell at {list(cell)}")
    for label, point in approach_targets.items():
        p = tuple(point)
        if not in_bounds(p, width, height) or p not in reached:
            errors.append(f"unreachable approach target {label} at {point}")
            unreachable_count += 1
    if any(not walkable[p[1]][p[0]] for p in landing):
        errors.append("entry landing is not fully walkable")

    # Put the room exit portal into the southern boundary and keep the other
    # gate cell open so the return transition has a two-tile socket.
    portal = {"id": "door_exit", "tag": "IN_DOOR_EXIT", "semantic_tag": "IN_DOOR_EXIT",
              "tx": center, "ty": height - 1, "target_map": parent_map,
              "target_portal": parent_portal}
    data = {
        "schema_version": 1,
        "kind": "interior",
        "site_id": site_id,
        "parent_map": parent_map,
        "seed": seed,
        "building_kind": kind,
        "width": width,
        "height": height,
        "tile_px": TILE,
        "floor_theme": floor_key,
        "wall_material": wall_material,
        "partition_row": partition_y,
        "layers": {
            "ground": semantic,
            "wall_cap_mask": cap_masks,
            "wall_face_row": face_rows,
            "walkable": walkable,
        },
        "solid": [list(p) for p in sorted(solids, key=lambda p: (p[1], p[0]))],
        "objects": objects,
        "portals": [portal],
        "npcs": npcs,
        "chests": chests,
        "switches": switches,
        "entry_landing": landing,
        "approach_targets": approach_targets,
        "validation": {"errors": errors, "unreachable_count": unreachable_count,
                       "geometry_error_count": len(errors) - unreachable_count,
                       "reachable_cells": len(reached)},
    }
    return {"data": data, "image": image}


def flood(walkable: Sequence[Sequence[bool]], start: Tuple[int, int]) -> Set[Tuple[int, int]]:
    height, width = len(walkable), len(walkable[0])
    if not in_bounds(start, width, height) or not walkable[start[1]][start[0]]:
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


def compose_room(seed: int, kind: str, *, site_id: str | None = None,
                 parent_map: str = "parent_map", parent_portal: str = "entry") -> dict:
    global cap_cache, cap_cache_index
    cap_cache, cap_cache_index = load_caps()
    room = generate_layout(seed, kind, site_id=site_id, parent_map=parent_map,
                           parent_portal=parent_portal)
    room["image"] = render_layout(room["data"])
    return room


def render_layout(data: dict) -> Image.Image:
    width, height = data["width"], data["height"]
    image = Image.new("RGBA", (width * TILE, height * TILE), (0, 0, 0, 0))
    floor_asset = FLOOR_INFO[data["floor_theme"]][1]
    floor = TILES[floor_asset]
    for y, row in enumerate(data["layers"]["ground"]):
        for x, semantic in enumerate(row):
            if semantic == "IN_WALL_CAP":
                mask = data["layers"]["wall_cap_mask"][y][x]
                cap = cap_cache[(data["wall_material"], mask)]
                image.alpha_composite(cap, (x * TILE, y * TILE))
            elif semantic == "IN_WALL_FACE":
                face_row = data["layers"]["wall_face_row"][y][x]
                material = data["wall_material"]
                pair = next(item["pair"] for item in cap_cache_index["materials"] if item["name"] == material)
                image.alpha_composite(face_cell(pair, x % 4, lower=(face_row != 1)), (x * TILE, y * TILE))
            elif semantic == "IN_CARPET":
                image.alpha_composite(TILES["carpet"], (x * TILE, y * TILE))
            else:
                image.alpha_composite(floor, (x * TILE, y * TILE))

    for obj in sorted(data["objects"], key=lambda item: (item["anchor_tile"][1], item["anchor_tile"][0], item.get("role", ""))):
        path = ASSETS / "sprites" / f"{obj['asset']}.png"
        sprite = load_rgba(path)
        ax, ay = obj["anchor_tile"]
        if obj.get("anchor_kind") == "center":
            x = ax * TILE + (TILE - sprite.width) // 2
            y = ay * TILE + (TILE - sprite.height) // 2
        else:
            x = ax * TILE + (TILE - sprite.width) // 2
            y = (ay + 1) * TILE - sprite.height
        if obj.get("rotate", 0):
            sprite = sprite.rotate(obj["rotate"], expand=True, resample=Image.Resampling.NEAREST)
        image.alpha_composite(sprite, (x, y))
    return image


def image_bytes(image: Image.Image) -> bytes:
    stream = BytesIO()
    image.save(stream, format="PNG")
    return stream.getvalue()


def json_bytes(data: dict) -> bytes:
    return (json.dumps(data, indent=2, sort_keys=False) + "\n").encode("utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--seed", type=int, default=4444)
    parser.add_argument("--kind", choices=KINDS, default="shop")
    parser.add_argument("--site-id", help="stable map ID (defaults to interior:<seed>:<kind>)")
    parser.add_argument("--parent-map", default="parent_map")
    parser.add_argument("--parent-portal", default="entry")
    parser.add_argument("--out", help="PNG preview path")
    parser.add_argument("--json", help="JSON layout path")
    parser.add_argument("--check", action="store_true", help="verify reachability, protected landing and deterministic regeneration")
    args = parser.parse_args()
    try:
        room = compose_room(args.seed, args.kind, site_id=args.site_id,
                            parent_map=args.parent_map, parent_portal=args.parent_portal)
        data, image = room["data"], room["image"]
        if args.check:
            errors = data["validation"]["errors"]
            assert not errors, "interior layout invalid: " + "; ".join(errors)
            again = compose_room(args.seed, args.kind, site_id=args.site_id,
                                 parent_map=args.parent_map, parent_portal=args.parent_portal)
            assert json_bytes(data) == json_bytes(again["data"]), "determinism check failed: JSON changed"
            assert image_bytes(image) == image_bytes(again["image"]), "determinism check failed: PNG changed"
        default_png = OUT_DIR / f"interior_{args.kind}_seed{args.seed}.png"
        png_path = Path(args.out) if args.out else default_png
        json_path = Path(args.json) if args.json else png_path.with_suffix(".json")
        if not png_path.is_absolute():
            png_path = ROOT / png_path
        if not json_path.is_absolute():
            json_path = ROOT / json_path
        png_path.parent.mkdir(parents=True, exist_ok=True)
        json_path.parent.mkdir(parents=True, exist_ok=True)
        image.save(png_path)
        json_path.write_bytes(json_bytes(data))
        validation = data["validation"]
        print(f"interior seed={args.seed} kind={args.kind} size={data['width']}x{data['height']} "
              f"floor={data['floor_theme']} wall={data['wall_material']} "
              f"solids={len(data['solid'])} targets={len(data['approach_targets'])} "
              f"unreachable={validation['unreachable_count']}")
        return 0
    except (ValueError, FileNotFoundError, AssertionError, RuntimeError, StopIteration) as exc:
        print(f"build_interior_layout: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
