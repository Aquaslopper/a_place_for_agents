#!/usr/bin/env python3
"""Build 8x8-tile town district plates from the repository's existing art.

Run from any directory:
    python3 tools/build_town_plates.py
    python3 tools/build_town_plates.py --check

The 128x128 PNGs are design-time plates (not runtime assets). Every plate has
an 8-cell edge signature and tile-semantic grid in its JSON sidecar. Roads use
a two-cell centered socket; lots have one guaranteed two-cell South socket and
a clear two-by-two doorstep apron. Gate plates carry an outward road socket and
an inward road socket; their other six edge cells are wall.

No source art is cropped here. The builder only reuses the registered runtime
OpenRTP tiles/sprites documented in ART_BIBLE.md §10, at native pixel size.
"""
from __future__ import annotations

import argparse
import json
import shutil
from collections import deque
from pathlib import Path
from typing import Dict, Iterable, List, Sequence, Set, Tuple

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets"
OUT = ROOT / "docs" / "town_assets"
PLATES_DIR = OUT / "plates"
TILE = 16
GRID = 8
PX = TILE * GRID
EDGE_TYPES = {"grass", "road", "cobble", "water", "wall"}
PORT = ("grass", "grass", "road", "road", "grass", "grass", "grass", "grass")


def load_image(folder: str, name: str) -> Image.Image:
    path = ASSETS / folder / f"{name}.png"
    if not path.is_file():
        raise FileNotFoundError(f"Required registered art is missing: {path.relative_to(ROOT)}")
    with Image.open(path) as source:
        image = source.convert("RGBA")
    return image


TILES: Dict[str, Image.Image] = {
    name: load_image("tiles", name)
    for name in ("grass_0", "grass_1", "grass_2", "grass_3", "dirt", "cobble",
                 "flagstone", "water_0", "beach", "bridge", "turnips", "soil")
}
SPRITES: Dict[str, Image.Image] = {
    name: load_image("sprites", name)
    for name in (
        "house_0", "house_1", "house_2", "house_3", "house_4", "shop", "inn",
        "smithy", "temple", "manor", "well", "basin", "town_gate_double",
        "town_round_turret", "town_market_flowers_orange", "town_market_flowers_red",
        "town_market_sacks", "tree_0", "tree_1", "tree_2", "bush", "bush_plain",
        "flowerbed", "fence", "fence_post", "fence_gate", "noticeboard", "bench",
        "crate", "barrel", "sign", "town_sign_goods", "town_sign_inn",
        "town_sign_weapon", "town_sign_tankard", "town_sign_armor",
    )
}
MASONRY = load_image("tiles", "town_masonry_atlas")


# Footprints follow ART_BIBLE.md §10.1. These are logical solid ground cells,
# not the transparent roof/sprite envelope.
BUILDINGS: Dict[str, Tuple[str, int, int]] = {
    "house_0": ("house_0", 4, 2),
    "house_1": ("house_1", 4, 2),
    "house_2": ("house_2", 4, 2),
    "house_3": ("house_3", 4, 2),
    "house_4": ("house_4", 4, 2),
    "shop": ("shop", 5, 2),
    "inn": ("inn", 5, 2),
    "smithy": ("smithy", 5, 2),
    "temple": ("temple", 5, 3),
    "manor": ("manor", 5, 2),
}


def new_grid(fill: str = "grass") -> List[List[str]]:
    return [[fill for _ in range(GRID)] for _ in range(GRID)]


def put(grid: List[List[str]], x: int, y: int, kind: str) -> None:
    if not (0 <= x < GRID and 0 <= y < GRID):
        raise ValueError(f"tile outside {GRID}x{GRID} plate: {(x, y)}")
    grid[y][x] = kind


def paint_route(grid: List[List[str]], cells: Iterable[Tuple[int, int]], kind: str = "road") -> None:
    for x, y in cells:
        put(grid, x, y, kind)


def centered_edge_profile(edge: str) -> Tuple[str, ...]:
    """The two-cell-wide centered walk socket used by town streets and lots."""
    return PORT


def edge_signatures(grid: Sequence[Sequence[str]]) -> Dict[str, List[str]]:
    return {
        "N": list(grid[0]),
        "E": [row[GRID - 1] for row in grid],
        "S": list(grid[GRID - 1]),
        "W": [row[0] for row in grid],
    }


def draw_plate(grid: Sequence[Sequence[str]], objects: Sequence[dict]) -> Image.Image:
    image = Image.new("RGBA", (PX, PX), (0, 0, 0, 0))
    for y, row in enumerate(grid):
        for x, kind in enumerate(row):
            if kind == "wall":
                tile = MASONRY.crop((16, 16, 32, 32))
            else:
                tex_name = {
                    "grass": "grass_0",
                    "road": "dirt",
                    "cobble": "cobble",
                    "water": "water_0",
                    "soil": "soil",
                }.get(kind)
                if tex_name is None:
                    raise ValueError(f"No ground texture registered for semantic {kind!r}")
                tile = TILES[tex_name]
            image.alpha_composite(tile, (x * TILE, y * TILE))

    for obj in objects:
        name = obj["asset"]
        sprite = TILES[name] if obj.get("asset_kind") == "tile" else SPRITES[name]
        ax, ay = obj["anchor_tile"]
        if obj.get("anchor_kind", "foot") == "center":
            x = ax * TILE + (TILE - sprite.width) // 2
            y = ay * TILE + (TILE - sprite.height) // 2
        else:
            # Foot-anchored: the sprite base sits on the bottom edge of anchor tile.
            x = ax * TILE + (TILE - sprite.width) // 2
            y = (ay + 1) * TILE - sprite.height
        if obj.get("rotate", 0):
            sprite = sprite.rotate(obj["rotate"], expand=True, resample=Image.Resampling.NEAREST)
        image.alpha_composite(sprite, (int(x), int(y)))
    return image


def walkable_component(grid: Sequence[Sequence[str]], solids: Set[Tuple[int, int]], start: Tuple[int, int]) -> Set[Tuple[int, int]]:
    if start in solids or grid[start[1]][start[0]] in ("water", "wall"):
        return set()
    seen = {start}
    queue = deque([start])
    while queue:
        x, y = queue.popleft()
        for nx, ny in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
            if not (0 <= nx < GRID and 0 <= ny < GRID) or (nx, ny) in seen or (nx, ny) in solids:
                continue
            if grid[ny][nx] not in ("water", "wall"):
                seen.add((nx, ny))
                queue.append((nx, ny))
    return seen


def add_plate(plates: Dict[str, dict], name: str, grid: List[List[str]],
              objects: List[dict], *, category: str, sockets: Dict[str, List[Tuple[int, int]]],
              socket_kinds: Dict[str, str] | None = None,
              edge_defaults: Dict[str, str] | None = None,
              solids: Iterable[Tuple[int, int]] = (), metadata: dict | None = None,
              check_connectivity: bool = True) -> None:
    solids_set = {tuple(p) for p in solids}
    socket_kinds = socket_kinds or {}
    edge_defaults = edge_defaults or {}
    for side, cells in sockets.items():
        if len(cells) != 2:
            raise ValueError(f"{name}: {side} socket must be two cells wide")
    signatures = edge_signatures(grid)
    expected = {}
    for side in ("N", "E", "S", "W"):
        base = edge_defaults.get(side, "grass")
        expected[side] = list(base) if isinstance(base, (list, tuple)) else [base] * GRID
        if len(expected[side]) != GRID:
            raise ValueError(f"{name}: {side} edge default must have {GRID} labels")
    for side, cells in sockets.items():
        for x, y in cells:
            edge_index = x if side in "NS" else y
            expected[side][edge_index] = socket_kinds.get(side, "road")
        if signatures[side] != expected[side]:
            raise AssertionError(f"{name}: {side} edge {signatures[side]} does not match declared socket {expected[side]}")
    for side, sig in signatures.items():
        if any(kind not in EDGE_TYPES for kind in sig):
            raise ValueError(f"{name}: unsupported {side} edge label")

    plate = {
        "name": name,
        "category": category,
        "grid_tiles": GRID,
        "pixel_size": [PX, PX],
        "ground": grid,
        "edge_labels": signatures,
        "sockets": {side: [list(p) for p in cells] for side, cells in sockets.items()},
        "socket_kinds": {side: socket_kinds.get(side, "road") for side in sockets},
        "solids": [list(p) for p in sorted(solids_set, key=lambda p: (p[1], p[0]))],
        "objects": objects,
    }
    if metadata:
        plate.update(metadata)
    component: Set[Tuple[int, int]] = set()
    if check_connectivity:
        walk_sockets = [
            (side, cells) for side, cells in sockets.items()
            if socket_kinds.get(side, "road") not in ("water", "wall")
        ]
        start = tuple(walk_sockets[0][1][0]) if walk_sockets else (3, 3)
        component = walkable_component(grid, solids_set, start)
        for side, cells in walk_sockets:
            if not all(tuple(p) in component for p in cells):
                raise AssertionError(f"{name}: disconnected socket {side}")
    plate["walkable_from_first_socket"] = [list(p) for p in sorted(component, key=lambda p: (p[1], p[0]))]
    plates[name] = {"image": draw_plate(grid, objects), "json": plate}


def road_cells_for(ports: Set[str]) -> Set[Tuple[int, int]]:
    """Rasterize a two-tile-wide centered connector with named edge ports."""
    cells: Set[Tuple[int, int]] = set()
    if "N" in ports:
        cells.update((x, y) for x in (3, 4) for y in range(0, 4))
    if "S" in ports:
        cells.update((x, y) for x in (3, 4) for y in range(4, 8))
    if "W" in ports:
        cells.update((x, y) for y in (3, 4) for x in range(0, 4))
    if "E" in ports:
        cells.update((x, y) for y in (3, 4) for x in range(4, 8))
    # A center tile pair binds the arms for corners, T junctions and dead-ends.
    if ports:
        cells.update((x, y) for x in (3, 4) for y in (3, 4))
    return cells


def connector_name(ports: Set[str]) -> str:
    keys = {
        frozenset(("N", "S")): "street_ns",
        frozenset(("E", "W")): "street_ew",
        frozenset(("N", "E")): "street_corner_ne",
        frozenset(("N", "W")): "street_corner_nw",
        frozenset(("S", "E")): "street_corner_se",
        frozenset(("S", "W")): "street_corner_sw",
        frozenset(("N", "E", "W")): "street_t_n",
        frozenset(("S", "E", "W")): "street_t_s",
        frozenset(("N", "S", "E")): "street_t_e",
        frozenset(("N", "S", "W")): "street_t_w",
        frozenset(("N", "E", "S", "W")): "street_cross",
        frozenset(("N",)): "street_end_n",
        frozenset(("E",)): "street_end_e",
        frozenset(("S",)): "street_end_s",
        frozenset(("W",)): "street_end_w",
    }
    return keys[frozenset(ports)]


def make_street_plates(plates: Dict[str, dict]) -> None:
    connector_sets = [
        {"N", "S"}, {"E", "W"},
        {"N", "E"}, {"N", "W"}, {"S", "E"}, {"S", "W"},
        {"N", "E", "W"}, {"S", "E", "W"},
        {"N", "S", "E"}, {"N", "S", "W"}, {"N", "E", "S", "W"},
        {"N"}, {"E"}, {"S"}, {"W"},
    ]
    for ports in connector_sets:
        name = connector_name(ports)
        grid = new_grid()
        cells = road_cells_for(ports)
        paint_route(grid, cells)
        sockets = {side: [(3, 0), (4, 0)] for side in ()}  # type marker; replaced below
        edge_sockets = {
            "N": [(3, 0), (4, 0)], "E": [(7, 3), (7, 4)],
            "S": [(3, 7), (4, 7)], "W": [(0, 3), (0, 4)],
        }
        sockets = {side: edge_sockets[side] for side in sorted(ports)}
        add_plate(plates, name, grid, [], category="street", sockets=sockets,
                  metadata={"road_ports": sorted(ports), "road_width_tiles": 2})


def make_plaza(plates: Dict[str, dict], name: str, prop: str) -> None:
    grid = new_grid()
    # A paved square around a central fixture. Four centered road sockets feed
    # a two-cell ring so the feature never cuts the N/E/S/W circulation loop.
    for y in range(1, 7):
        for x in range(1, 7):
            put(grid, x, y, "cobble")
    route: Set[Tuple[int, int]] = set()
    route.update((x, y) for x in (3, 4) for y in (0, 3))
    route.update((x, y) for y in (3, 4) for x in (0, 3))
    # The ring at x/y=2 and 5 bypasses the center fixture.
    route.update((x, y) for x in range(2, 6) for y in (2, 5))
    route.update((x, y) for y in range(2, 6) for x in (2, 5))
    # Short spokes join the edge sockets to the ring.
    route.update((x, y) for x in (3, 4) for y in range(0, 3))
    route.update((x, y) for x in (3, 4) for y in range(5, 8))
    route.update((x, y) for y in (3, 4) for x in range(0, 3))
    route.update((x, y) for y in (3, 4) for x in range(5, 8))
    paint_route(grid, route, "road")
    # Convert the open central court to cobble again, leaving the walk ring clear.
    for y in range(2, 6):
        for x in range(2, 6):
            if (x, y) not in route:
                put(grid, x, y, "cobble")
    objects = [{"asset": prop, "anchor_tile": [4, 4], "anchor_kind": "foot", "role": "plaza_feature"}]
    # Well is a 2x2 logical obstacle; the ring has four continuous sides.
    solid = {(3, 3), (4, 3), (3, 4), (4, 4)} if prop == "well" else {(4, 4)}
    if prop == "well":
        objects.append({"asset": "noticeboard", "anchor_tile": [1, 2], "anchor_kind": "foot",
                        "role": "plaza_noticeboard", "solid_cell": [1, 2]})
        solid.add((1, 2))
    sockets = {
        "N": [(3, 0), (4, 0)], "E": [(7, 3), (7, 4)],
        "S": [(3, 7), (4, 7)], "W": [(0, 3), (0, 4)],
    }
    add_plate(plates, name, grid, objects, category="plaza", sockets=sockets, solids=solid,
              metadata={"road_ports": ["N", "E", "S", "W"], "feature": prop,
                        "feature_cells": [list(p) for p in sorted(solid)],
                        "noticeboard_cell": [1, 2] if prop == "well" else None})


def make_canal_plates(plates: Dict[str, dict]) -> None:
    # Straight channels make the two bridge plates useful as actual connectors.
    for name, axis in (("canal_ns", "ns"), ("canal_ew", "ew")):
        grid = new_grid()
        if axis == "ns":
            for y in range(GRID):
                for x in (3, 4):
                    put(grid, x, y, "water")
            sockets = {"N": [(3, 0), (4, 0)], "S": [(3, 7), (4, 7)]}
        else:
            for y in (3, 4):
                for x in range(GRID):
                    put(grid, x, y, "water")
            sockets = {"E": [(7, 3), (7, 4)], "W": [(0, 3), (0, 4)]}
        channel_sides = {side: "water" for side in sockets}
        add_plate(plates, name, grid, [], category="waterway", sockets=sockets,
                  socket_kinds=channel_sides, check_connectivity=False,
                  metadata={"channel_axis": axis, "walkable_ports": []})

    for name, channel_axis in (("canal_bridge_h", "ns"), ("canal_bridge_v", "ew")):
        grid = new_grid()
        if channel_axis == "ns":
            for y in range(GRID):
                for x in (3, 4):
                    put(grid, x, y, "water")
            # An east-west bridge spans the north-south canal.
            for x in range(GRID):
                for y in (3, 4):
                    put(grid, x, y, "road")
            sockets = {"N": [(3, 0), (4, 0)], "S": [(3, 7), (4, 7)],
                       "E": [(7, 3), (7, 4)], "W": [(0, 3), (0, 4)]}
            objects = []
        else:
            for y in (3, 4):
                for x in range(GRID):
                    put(grid, x, y, "water")
            for y in range(GRID):
                for x in (3, 4):
                    put(grid, x, y, "road")
            sockets = {"N": [(3, 0), (4, 0)], "S": [(3, 7), (4, 7)],
                       "E": [(7, 3), (7, 4)], "W": [(0, 3), (0, 4)]}
            objects = []
        # Bridge texture is laid into road/deck cells. Distinguish water and deck
        # in the semantic grid; the renderer selects `bridge.png` where both
        # direction bands meet. The cross-axis edge profiles remain unambiguous.
        plate = {
            "name": name,
            "category": "canal_bridge",
            "grid_tiles": GRID,
            "pixel_size": [PX, PX],
            "ground": grid,
            "edge_labels": edge_signatures(grid),
            "sockets": {side: [list(p) for p in cells] for side, cells in sockets.items()},
            "solids": [],
            "objects": objects,
            "channel_axis": channel_axis,
            "bridge_axis": "ew" if channel_axis == "ns" else "ns",
            "road_ports": ["E", "W"] if channel_axis == "ns" else ["N", "S"],
            "water_ports": ["N", "S"] if channel_axis == "ns" else ["E", "W"],
        }
        # For the bridge crossing, semantic overlays use the road type at the
        # intersection. Re-label just the water-channel ports as water; bridge
        # decks fill road sockets on the perpendicular pair.
        image = draw_plate(grid, objects)
        bridge = TILES["bridge"]
        if channel_axis == "ns":
            for x in range(GRID):
                for y in (3, 4):
                    image.alpha_composite(bridge, (x * TILE, y * TILE))
            plate["edge_labels"] = {
                "N": ["grass", "grass", "grass", "water", "water", "grass", "grass", "grass"],
                "E": ["grass", "grass", "grass", "road", "road", "grass", "grass", "grass"],
                "S": ["grass", "grass", "grass", "water", "water", "grass", "grass", "grass"],
                "W": ["grass", "grass", "grass", "road", "road", "grass", "grass", "grass"],
            }
        else:
            for y in range(GRID):
                for x in (3, 4):
                    bridge_v = bridge.rotate(90, expand=False, resample=Image.Resampling.NEAREST)
                    image.alpha_composite(bridge_v, (x * TILE, y * TILE))
            plate["edge_labels"] = {
                "N": ["grass", "grass", "grass", "road", "road", "grass", "grass", "grass"],
                "E": ["grass", "grass", "grass", "water", "water", "grass", "grass", "grass"],
                "S": ["grass", "grass", "grass", "road", "road", "grass", "grass", "grass"],
                "W": ["grass", "grass", "grass", "water", "water", "grass", "grass", "grass"],
            }
        plates[name] = {"image": image, "json": plate}


def make_gate_plates(plates: Dict[str, dict]) -> None:
    specs = [
        ("gate_n", "N", {"N", "S"}),
        ("gate_s", "S", {"N", "S"}),
        ("gate_w", "W", {"E", "W"}),
        ("gate_e", "E", {"E", "W"}),
        # Boundary gates can also be a junction with a north/south feeder.
        ("gate_w_n", "W", {"N", "E", "W"}),
        ("gate_w_s", "W", {"E", "S", "W"}),
        ("gate_w_ns", "W", {"N", "E", "S", "W"}),
        ("gate_e_n", "E", {"N", "E", "W"}),
        ("gate_e_s", "E", {"E", "S", "W"}),
        ("gate_e_ns", "E", {"N", "E", "S", "W"}),
    ]
    edge_sockets = {
        "N": [(3, 0), (4, 0)], "E": [(7, 3), (7, 4)],
        "S": [(3, 7), (4, 7)], "W": [(0, 3), (0, 4)],
    }
    for name, side, ports in specs:
        grid = new_grid()
        if side in "NS":
            boundary = 0 if side == "N" else GRID - 1
            for x in range(GRID):
                if x not in (0, 3, 4, GRID - 1):
                    put(grid, x, boundary, "wall")
            anchor = [3, 1 if side == "N" else 6]
            rotation = 0
        else:
            boundary = 0 if side == "W" else GRID - 1
            for y in range(GRID):
                if y not in (0, 3, 4, GRID - 1):
                    put(grid, boundary, y, "wall")
            anchor = [1 if side == "W" else 6, 3]
            rotation = 90 if side in "EW" else 0
        paint_route(grid, road_cells_for(ports))
        sockets = {direction: edge_sockets[direction] for direction in sorted(ports)}
        objects = [{"asset": "town_gate_double", "anchor_tile": anchor,
                    "anchor_kind": "foot", "rotate": rotation, "role": "town_gate"}]
        gate_edge = ["grass"] * GRID
        for idx in (1, 2, 5, 6):
            gate_edge[idx] = "wall"
        for idx in (3, 4):
            gate_edge[idx] = "road"
        add_plate(plates, name, grid, objects, category="gate", sockets=sockets,
                  edge_defaults={side: gate_edge},
                  metadata={"gate_side": side, "road_ports": sorted(ports),
                            "wall_edge": side, "outward_port_width": 2})


def make_green_open(plates: Dict[str, dict]) -> None:
    grid = new_grid()
    add_plate(plates, "green_open", grid, [], category="open_ground", sockets={})


def make_lot(plates: Dict[str, dict], name: str, building_key: str,
             sign: str | None = None) -> None:
    sprite_name, fw, fh = BUILDINGS[building_key]
    grid = new_grid()
    # Place the logical base footprint at rows 3..4 (or 2..4 for a three-row
    # structure); the sprite's foot anchor is the south edge at y=5.
    fx = (GRID - fw) // 2
    base_y = 5
    fy = base_y - fh
    solids = {(x, y) for x in range(fx, fx + fw) for y in range(fy, base_y)}
    door_x = fx + fw // 2
    door = (door_x, base_y)
    apron = {(x, y) for x in (3, 4) for y in (base_y, base_y + 1)}
    if solids & apron:
        raise AssertionError(f"{name}: solid footprint intersects doorstep apron")
    # A clear, two-cell path from the two-cell door apron to the South socket.
    paint_route(grid, ((x, y) for x in (3, 4) for y in (5, 6, 7)), "road")
    image_obj = {"asset": sprite_name, "anchor_tile": [4, base_y - 1],
                 "anchor_kind": "foot", "role": "building", "building_key": building_key}
    objects = [image_obj]
    if sign:
        # A sign is placed on the far west grass margin, outside both apron and
        # the logical footprint; its collision cell remains advisory metadata.
        objects.append({"asset": sign, "anchor_tile": [0, 5], "anchor_kind": "foot",
                        "role": "building_sign", "solid_cell": [0, 5]})
        solids.add((0, 5))
    sockets = {"S": [(3, 7), (4, 7)]}
    nameplate = {
        "entry_side": "S",
        "door": list(door),
        "door_id": f"{name}:door",
        "apron": [list(p) for p in sorted(apron, key=lambda p: (p[1], p[0]))],
        "footprint": {"x": fx, "y": fy, "width": fw, "height": fh},
        "keepout": {"x": max(0, fx - 1), "y": max(0, fy - 2),
                    "width": min(GRID, fx + fw + 1) - max(0, fx - 1),
                    "height": min(GRID, base_y + 2) - max(0, fy - 2)},
        "building": building_key,
        "road_ports": ["S"],
    }
    add_plate(plates, name, grid, objects, category="lot", sockets=sockets,
              solids=solids, metadata=nameplate)
    component = set(map(tuple, plates[name]["json"]["walkable_from_first_socket"]))
    if not set(apron).issubset(component) or door not in component:
        raise AssertionError(f"{name}: door apron is not reachable from its South socket")


def make_special_lots(plates: Dict[str, dict]) -> None:
    # Market lot: two small produce stalls flank a central dirt lane. The South
    # port, not the stalls, is the lot's portal into the town street network.
    grid = new_grid()
    paint_route(grid, ((x, y) for x in (3, 4) for y in (4, 5, 6, 7)), "road")
    objects = [
        {"asset": "town_market_flowers_orange", "anchor_tile": [1, 3], "role": "market_stall"},
        {"asset": "town_market_flowers_red", "anchor_tile": [6, 3], "role": "market_stall"},
        {"asset": "town_market_sacks", "anchor_tile": [1, 5], "role": "market_goods"},
        {"asset": "crate", "anchor_tile": [6, 5], "role": "market_goods"},
    ]
    market_solids = {(1, 3), (6, 3), (1, 5), (6, 5)}
    add_plate(plates, "lot_market_stalls", grid, objects, category="lot",
              sockets={"S": [(3, 7), (4, 7)]}, solids=market_solids,
              metadata={"entry_side": "S", "door": None, "apron": [[3, 5], [4, 5], [3, 6], [4, 6]],
                        "building": None, "lot_kind": "market", "road_ports": ["S"]})

    # Garden beds are tilled cells north of a clear, centered entry aisle.
    grid = new_grid()
    for y in (2, 3, 4):
        for x in (1, 2, 5, 6):
            put(grid, x, y, "cobble" if y == 4 else "grass")
    for y in (2, 3):
        for x in (1, 2, 5, 6):
            put(grid, x, y, "soil")
    for x in (1, 2, 5, 6):
        put(grid, x, 3, "soil")
    paint_route(grid, ((x, y) for x in (3, 4) for y in (4, 5, 6, 7)), "road")
    objects = []
    for y in (2, 3):
        for x in (1, 2, 5, 6):
            # Turnip plants use existing 16x16 runtime art; the soil is the
            # ground tile, not a second crop or a new asset.
            objects.append({"asset": "turnips", "asset_kind": "tile",
                            "anchor_tile": [x, y], "anchor_kind": "center", "role": "turnip_crop"})
            put(grid, x, y, "soil")
    add_plate(plates, "lot_garden_turnips", grid, objects, category="lot",
              sockets={"S": [(3, 7), (4, 7)]},
              solids={(x, y) for y in (2, 3) for x in (1, 2, 5, 6)},
              metadata={"entry_side": "S", "door": None, "apron": [[3, 5], [4, 5], [3, 6], [4, 6]],
                        "building": None, "lot_kind": "garden", "crop_asset": "tiles/turnips.png",
                        "road_ports": ["S"]})

    # A small grove lot uses only the existing tree/bush sprites. Trees sit on
    # the northern half; the unobstructed central lane stays connected.
    grid = new_grid()
    paint_route(grid, ((x, y) for x in (3, 4) for y in range(3, 8)), "road")
    objects = [
        {"asset": "tree_0", "anchor_tile": [1, 3], "role": "park_tree"},
        {"asset": "tree_2", "anchor_tile": [6, 3], "role": "park_tree"},
        {"asset": "bush_plain", "anchor_tile": [1, 5], "role": "park_bush"},
        {"asset": "flowerbed", "anchor_tile": [6, 5], "role": "park_flowerbed"},
        {"asset": "bench", "anchor_tile": [4, 3], "role": "park_bench"},
    ]
    solids = {(1, 3), (6, 3), (1, 5), (6, 5), (4, 3)}
    add_plate(plates, "lot_park_grove", grid, objects, category="lot",
              sockets={"S": [(3, 7), (4, 7)]}, solids=solids,
              metadata={"entry_side": "S", "door": None, "apron": [[3, 5], [4, 5], [3, 6], [4, 6]],
                        "building": None, "lot_kind": "park", "road_ports": ["S"]})



def build_catalog() -> Dict[str, dict]:
    plates: Dict[str, dict] = {}
    make_street_plates(plates)
    make_green_open(plates)
    make_plaza(plates, "plaza_well", "well")
    make_plaza(plates, "plaza_fountain", "basin")
    make_canal_plates(plates)
    make_gate_plates(plates)
    for key, sign in (
        ("house_0", None), ("house_1", None), ("house_2", None), ("house_3", None), ("house_4", None),
        ("shop", "town_sign_goods"), ("inn", "town_sign_inn"), ("smithy", "town_sign_weapon"),
        ("temple", None), ("manor", "town_sign_armor"),
    ):
        make_lot(plates, f"lot_{key}", key, sign)
    make_special_lots(plates)
    return plates


def render_contact_sheet(plates: Dict[str, dict], category: str, filename: Path) -> Image.Image:
    selected = [(name, item["image"]) for name, item in plates.items()
                if item["json"]["category"] in category.split(",")]
    cols = 4 if category == "lot" else 5
    scale = 2
    pad, label_h = 8, 18
    cell_w, cell_h = PX * scale, PX * scale + label_h
    rows = (len(selected) + cols - 1) // cols
    sheet = Image.new("RGB", (pad + cols * (cell_w + pad), pad + rows * (cell_h + pad)), (22, 29, 43))
    draw = ImageDraw.Draw(sheet)
    for idx, (name, image) in enumerate(selected):
        r, c = divmod(idx, cols)
        x, y = pad + c * (cell_w + pad), pad + r * (cell_h + pad)
        draw.text((x + 2, y + 2), name, fill=(242, 222, 168))
        enlarged = image.resize((cell_w, cell_w), Image.Resampling.NEAREST)
        sheet.paste(enlarged, (x, y + label_h), enlarged)
    return sheet


def serializable_index(plates: Dict[str, dict]) -> dict:
    return {
        "schema_version": 1,
        "grid_tiles": GRID,
        "pixel_size": [PX, PX],
        "edge_alphabet": sorted(EDGE_TYPES),
        "socket_profile": {"width_tiles": 2, "offset_tiles": 3, "type": "road"},
        "plate_count": len(plates),
        "plates": [plates[name]["json"] for name in sorted(plates)],
    }


def verify_catalog(plates: Dict[str, dict]) -> List[str]:
    problems: List[str] = []
    expected_names = {
        "street_ns", "street_ew", "street_corner_ne", "street_corner_nw", "street_corner_se", "street_corner_sw",
        "street_t_n", "street_t_e", "street_t_s", "street_t_w", "street_cross",
        "plaza_well", "plaza_fountain", "canal_bridge_h", "canal_bridge_v", "gate_n", "gate_s", "gate_e", "gate_w",
        "gate_w_n", "gate_w_s", "gate_w_ns", "gate_e_n", "gate_e_s", "gate_e_ns", "green_open",
        "lot_market_stalls", "lot_garden_turnips", "lot_park_grove",
    } | {f"street_end_{d.lower()}" for d in "NESW"} | {"canal_ns", "canal_ew"} | {f"lot_house_{i}" for i in range(5)} | {
        "lot_shop", "lot_inn", "lot_smithy", "lot_temple", "lot_manor"
    }
    if set(plates) != expected_names:
        problems.append(f"catalog name mismatch: expected {len(expected_names)}, got {len(plates)}")
    for name, item in plates.items():
        image, data = item["image"], item["json"]
        if image.size != (PX, PX):
            problems.append(f"{name}: image size {image.size}, expected {(PX, PX)}")
        if len(data["ground"]) != GRID or any(len(row) != GRID for row in data["ground"]):
            problems.append(f"{name}: ground layer is not {GRID}x{GRID}")
        if data.get("category") == "lot" and data.get("building"):
            if data["entry_side"] != "S":
                problems.append(f"{name}: building lot must use the south entry")
            if len(data["apron"]) != 4:
                problems.append(f"{name}: doorstep apron is not 2x2")
            for p in data["apron"]:
                if p in data["solids"]:
                    problems.append(f"{name}: solid prop overlaps apron at {p}")
        sig = data["edge_labels"]
        if any(len(sig[s]) != GRID for s in "NESW"):
            problems.append(f"{name}: edge signature must have {GRID} cells")
        if any(label not in EDGE_TYPES for side in "NESW" for label in sig[side]):
            problems.append(f"{name}: unsupported edge label")
    # Ensure at least one compatible opposite socket exists for each non-wall
    # edge profile. Layout generators can enumerate this compatibility table.
    for name, item in plates.items():
        sig = item["json"]["edge_labels"]
        for side, opposite in (("N", "S"), ("E", "W"), ("S", "N"), ("W", "E")):
            profile = sig[side]
            if "wall" in profile:
                continue  # perimeter/gate edge is intentionally an outer boundary
            if any(other["json"]["edge_labels"][opposite] == profile
                   for other_name, other in plates.items() if other_name != name or opposite == side):
                continue
            problems.append(f"{name}.{side}: no plate has a compatible {opposite} edge")
    return problems


def write_catalog(plates: Dict[str, dict]) -> None:
    PLATES_DIR.mkdir(parents=True, exist_ok=True)
    # Keep the generated directory canonical; do not leave stale plate art.
    expected_pngs = {f"{name}.png" for name in plates}
    expected_jsons = {f"{name}.json" for name in plates}
    for path in PLATES_DIR.iterdir():
        if path.is_file() and path.suffix == ".png" and path.name not in expected_pngs:
            path.unlink()
        elif path.is_file() and path.suffix == ".json" and path.name not in expected_jsons | {"index.json"}:
            path.unlink()
    for name in sorted(plates):
        item = plates[name]
        item["image"].save(PLATES_DIR / f"{name}.png")
        (PLATES_DIR / f"{name}.json").write_text(json.dumps(item["json"], indent=2) + "\n", encoding="utf-8")
    index = serializable_index(plates)
    (PLATES_DIR / "index.json").write_text(json.dumps(index, indent=2) + "\n", encoding="utf-8")
    street_sheet = render_contact_sheet(plates, "street,plaza,gate,waterway,canal_bridge", OUT / "plates_streets.png")
    lot_sheet = render_contact_sheet(plates, "lot", OUT / "plates_lots.png")
    street_sheet.save(OUT / "plates_streets.png")
    lot_sheet.save(OUT / "plates_lots.png")


def check_outputs(plates: Dict[str, dict]) -> List[str]:
    problems = verify_catalog(plates)
    expected: Dict[Path, bytes] = {}
    for name, item in plates.items():
        expected[PLATES_DIR / f"{name}.png"] = image_bytes(item["image"])
        expected[PLATES_DIR / f"{name}.json"] = (json.dumps(item["json"], indent=2) + "\n").encode()
    expected[PLATES_DIR / "index.json"] = (json.dumps(serializable_index(plates), indent=2) + "\n").encode()
    expected[OUT / "plates_streets.png"] = image_bytes(render_contact_sheet(plates, "street,plaza,gate,waterway,canal_bridge", OUT / "plates_streets.png"))
    expected[OUT / "plates_lots.png"] = image_bytes(render_contact_sheet(plates, "lot", OUT / "plates_lots.png"))
    for path, content in expected.items():
        if not path.is_file() or path.read_bytes() != content:
            problems.append(f"missing or stale: {path.relative_to(ROOT)}")
    actual = {p for p in PLATES_DIR.glob("*.png")} | {p for p in PLATES_DIR.glob("*.json")}
    if actual != {p for p in expected if p.parent == PLATES_DIR}:
        extras = sorted(p.name for p in actual - {p for p in expected if p.parent == PLATES_DIR})
        if extras:
            problems.append(f"stale generated files: {', '.join(extras)}")
    return problems


def image_bytes(image: Image.Image) -> bytes:
    from io import BytesIO
    stream = BytesIO()
    image.save(stream, format="PNG")
    return stream.getvalue()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true", help="verify catalog invariants and committed generated outputs")
    args = parser.parse_args()
    plates = build_catalog()
    problems = verify_catalog(plates)
    if problems:
        for problem in problems:
            print(f"FAIL: {problem}")
        return 1
    if args.check:
        problems = check_outputs(plates)
        if problems:
            for problem in problems:
                print(f"FAIL: {problem}")
            return 1
        print(f"Town plates: OK — {len(plates)} 8x8 plates, edge sockets, lot aprons, and generated outputs verified")
    else:
        write_catalog(plates)
        print(f"Town plates: wrote {len(plates)} 8x8 plates to {PLATES_DIR.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
