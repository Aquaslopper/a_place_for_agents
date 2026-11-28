#!/usr/bin/env python3
"""Generate a deterministic, edge-matched town from the 8x8 plate catalog.

Examples:
    python3 tools/build_town_layout.py --seed 4444 --tier hamlet --biome temperate --gates N,E,S,W --check
    python3 tools/build_town_layout.py --seed 1337 --tier city --biome snow --gates N,W

Town grids are 3x3 / 5x4 / 6x5 district plates (24x24 / 40x32 / 48x40
walk tiles). A seeded AC-3/backtracking solver chooses among Carcassonne-matched
plate variants. A connected street spine joins every active gate, required
civic lot, building doorstep and the central well/noticeboard. Outer wall cells
are an explicit boundary overlay; only requested gate sockets remain open.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import random
import sys
from collections import deque
from io import BytesIO
from pathlib import Path
from typing import Dict, List, Sequence, Set, Tuple

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets"
PLATES_DIR = ROOT / "docs" / "town_assets" / "plates"
OUT_DIR = ROOT / "docs" / "town_assets" / "layouts"
TILE = 16
PLATE_TILES = 8
TIERS = {"hamlet": (3, 3), "town": (5, 4), "city": (6, 5)}  # cols, rows
REQUIRED = {
    "hamlet": ["lot_inn", "lot_shop"],
    "town": ["lot_inn", "lot_shop", "lot_smithy", "lot_temple"],
    # A manor is the city seat. The spec calls for a castle/manor, so the
    # verified `manor` footprint is the safe 8x8-lot choice (castle is 7x3).
    "city": ["lot_inn", "lot_shop", "lot_smithy", "lot_temple", "lot_manor"],
}
BIOME_GROUND = {"temperate": "grass_0", "snow": "snow", "sand": "sand"}
TOWN_GROUND_TAGS = {
    "grass": "TN_GRASS", "road": "TN_PATH", "cobble": "TN_COBBLE",
    "water": "TN_WATER", "wall": "TN_WALL", "soil": "TN_GARDEN",
    "bridge": "TN_BRIDGE",
}
BIOME_DECOR = {
    "temperate": {"trees": ["tree_0", "tree_1", "tree_2"], "shrub": "bush", "accent": "flowerbed", "accent_role": "flowerbed"},
    "snow": {"trees": ["pine_0", "pine_1", "pine_2", "tree_snow"], "shrub": "bush_plain", "accent": "rock_0", "accent_role": "snow_rock"},
    "sand": {"trees": ["dead_tree"], "shrub": "bush_plain", "accent": "rock_0", "accent_role": "desert_rock"},
}
OPPOSITE = {"N": "S", "E": "W", "S": "N", "W": "E"}
DELTA = {"N": (0, -1), "E": (1, 0), "S": (0, 1), "W": (-1, 0)}


def load_rgba(path: Path) -> Image.Image:
    if not path.is_file():
        raise FileNotFoundError(f"Required input missing: {path.relative_to(ROOT)}")
    with Image.open(path) as src:
        return src.convert("RGBA")


GROUND_TILES = {
    name: load_rgba(ASSETS / "tiles" / f"{name}.png")
    for name in ("grass_0", "snow", "sand", "dirt", "cobble", "water_0", "soil", "bridge")
}
MASONRY = load_rgba(ASSETS / "tiles" / "town_masonry_atlas.png").crop((16, 16, 32, 32))


def load_catalog(path: Path = PLATES_DIR / "index.json") -> Dict[str, dict]:
    if not path.is_file():
        raise FileNotFoundError(f"Town plate catalog missing: {path.relative_to(ROOT)}; run tools/build_town_plates.py")
    data = json.loads(path.read_text(encoding="utf-8"))
    if data.get("schema_version") != 1 or data.get("grid_tiles") != PLATE_TILES:
        raise ValueError("Unsupported town plate catalog schema")
    catalog = {item["name"]: item for item in data["plates"]}
    if data.get("plate_count") != len(catalog):
        raise ValueError("Town plate index count does not match its entries")
    return catalog


def parse_gates(text: str) -> List[str]:
    if text.strip().lower() in ("none", "-"):
        return []
    gates = [part.strip().upper() for part in text.replace(";", ",").split(",") if part.strip()]
    if any(g not in "NESW" or len(g) != 1 for g in gates):
        raise ValueError("--gates accepts a comma-separated subset of N,E,S,W")
    if len(gates) != len(set(gates)):
        raise ValueError("--gates cannot repeat a direction")
    return [g for g in "NESW" if g in gates]


def stable_seed(*parts: object) -> int:
    payload = "\x1f".join(str(part) for part in parts).encode("utf-8")
    return int.from_bytes(hashlib.sha256(payload).digest()[:8], "big")


def required_lots_for(tier: str) -> List[str]:
    if tier not in REQUIRED:
        raise ValueError(f"Unknown town tier {tier!r}")
    return list(REQUIRED[tier])


def optional_lot_pool(biome: str) -> List[str]:
    return {
        "temperate": [f"lot_house_{i}" for i in range(5)] + ["lot_market_stalls", "lot_garden_turnips", "lot_park_grove"],
        "snow": ["lot_house_1", "lot_house_4", "lot_park_grove"],
        "sand": ["lot_house_2", "lot_house_0", "lot_garden_turnips", "lot_park_grove"],
    }[biome]


def cell_domain(plan_name: str, catalog: Dict[str, dict], biome: str,
                required: Set[str]) -> Set[str]:
    """Return the legal seeded domain for one structural plan cell."""
    if plan_name == "lot_optional":
        return {name for name in optional_lot_pool(biome)
                if name in catalog and name not in required}
    item = catalog[plan_name]
    category = item["category"]
    if category == "street":
        ports = set(item.get("road_ports", []))
        return {name for name, candidate in catalog.items()
                if candidate["category"] == "street" and set(candidate.get("road_ports", [])) == ports}
    if category == "gate":
        ports = set(item.get("road_ports", []))
        side = item.get("gate_side")
        return {name for name, candidate in catalog.items()
                if candidate["category"] == "gate" and candidate.get("gate_side") == side
                and set(candidate.get("road_ports", [])) == ports}
    return {plan_name}


def solve_edge_grid(plan: Sequence[Sequence[str]], catalog: Dict[str, dict],
                    seed: int, biome: str, required: Set[str]) -> Tuple[List[List[str]], int]:
    """Solve the plan's edge domains with AC-3 propagation and seeded backtracking."""
    rows, cols = len(plan), len(plan[0])
    domains: Dict[Tuple[int, int], Set[str]] = {}
    for r in range(rows):
        for c in range(cols):
            domain = cell_domain(plan[r][c], catalog, biome, required)
            if not domain:
                raise RuntimeError(f"No candidate plate for planned cell ({r},{c}) {plan[r][c]}")
            domains[(r, c)] = set(domain)

    all_arcs = []
    for r in range(rows):
        for c in range(cols):
            for side, (dx, dy) in DELTA.items():
                nc, nr = c + dx, r + dy
                if 0 <= nc < cols and 0 <= nr < rows:
                    all_arcs.append(((r, c), (nr, nc), side))

    def ac3(current: Dict[Tuple[int, int], Set[str]]) -> bool:
        queue = deque(all_arcs)
        while queue:
            cell, neighbor, side = queue.popleft()
            opposite = OPPOSITE[side]
            compatible = {
                candidate for candidate in current[cell]
                if any(catalog[candidate]["edge_labels"][side] == catalog[other]["edge_labels"][opposite]
                       for other in current[neighbor])
            }
            if compatible == current[cell]:
                continue
            if not compatible:
                return False
            current[cell] = compatible
            # Re-enqueue every neighboring arc into the revised cell. This is
            # standard AC-3 revision with a compact, deterministic queue.
            r, c = cell
            for other_side, (dx, dy) in DELTA.items():
                nr, nc = r + dy, c + dx
                other = (nr, nc)
                if 0 <= nr < rows and 0 <= nc < cols and other != neighbor:
                    queue.append((other, cell, OPPOSITE[other_side]))
        return True

    rng = random.Random(stable_seed(seed, biome, "town-ac3"))
    backtracks = 0

    def search(current: Dict[Tuple[int, int], Set[str]]) -> Dict[Tuple[int, int], Set[str]] | None:
        nonlocal backtracks
        unresolved = [cell for cell, domain in current.items() if len(domain) > 1]
        if not unresolved:
            return current
        # Minimum remaining values; row-major breaks ties for stable layouts.
        cell = min(unresolved, key=lambda p: (len(current[p]), p[0], p[1]))
        choices = sorted(current[cell])
        rng.shuffle(choices)
        for choice in choices:
            child = {point: set(domain) for point, domain in current.items()}
            child[cell] = {choice}
            if ac3(child):
                result = search(child)
                if result is not None:
                    return result
            backtracks += 1
        return None

    if not ac3(domains):
        raise RuntimeError("Town plate domains are arc-inconsistent")
    solved = search(domains)
    if solved is None:
        raise RuntimeError(f"No edge-matched town solution for seed={seed}")
    return [[next(iter(solved[(r, c)])) for c in range(cols)] for r in range(rows)], backtracks


def make_plate_grid(seed: int, tier: str, biome: str, gates: Sequence[str],
                    catalog: Dict[str, dict]) -> Tuple[List[List[str]], int]:
    cols, rows = TIERS[tier]
    main_row = rows // 2
    center = cols // 2
    active = set(gates)
    required_list = required_lots_for(tier)
    rng = random.Random(stable_seed(seed, tier, "civic-lots"))
    rng.shuffle(required_list)
    required = set(required_list)
    plan = [["green_open" for _ in range(cols)] for _ in range(rows)]

    # The north district row holds civic lots. Each South port feeds a paired
    # road branch below; an active north gate takes the centered slot.
    civic_slots = [c for c in range(cols) if not ("N" in active and c == center)]
    if len(civic_slots) < len(required_list):
        raise ValueError(f"{tier} grid cannot fit its required civic lots with gates={','.join(gates)}")
    for c, lot in zip(civic_slots, required_list):
        plan[0][c] = lot
    for c in civic_slots[len(required_list):]:
        plan[0][c] = "lot_optional"
    if "N" in active:
        plan[0][center] = "gate_n"

    # Paired street spurs join civic door sockets to the east-west street. For
    # a 3x3 hamlet the civic row feeds directly into the main street.
    if main_row > 1:
        for c in range(cols):
            plan[1][c] = "street_ns"

    # A central well plaza is the four-way node for the whole town.
    for c in range(cols):
        if c == center:
            plan[main_row][c] = "plaza_well"
        elif c == 0:
            plan[main_row][c] = "gate_w_n" if "W" in active else "street_corner_ne"
        elif c == cols - 1:
            plan[main_row][c] = "gate_e_n" if "E" in active else "street_corner_nw"
        else:
            plan[main_row][c] = "street_t_n"

    # Continue a south spur only to an active exit. Otherwise cap it with a
    # one-sided connector whose outer edge stays closed by the town wall.
    if "S" in active:
        for r in range(main_row + 1, rows):
            plan[r][center] = "gate_s" if r == rows - 1 else "street_ns"
    else:
        plan[main_row + 1][center] = "street_end_n"

    solved, backtracks = solve_edge_grid(plan, catalog, seed, biome, required)
    return solved, backtracks


def town_object_tag(obj: dict) -> str | None:
    role, asset = obj.get("role", ""), obj.get("asset", "")
    if role == "town_gate":
        return "TN_GATE"
    if role in ("building_sign", "plaza_noticeboard"):
        return "TN_SIGN"
    if role in ("market_goods", "market_stall"):
        return "TN_MARKET"
    if role == "plaza_feature":
        return "TN_WELL" if asset == "well" else "TN_WATER"
    if role in ("park_tree", "park_bush") or asset.startswith(("tree_", "pine_", "bush")):
        return "TN_TREE"
    if asset in ("flowerbed", "turnips") or role.startswith(("flowerbed", "garden_")):
        return "TN_GARDEN"
    if role in ("garden_bench", "commons_bench", "park_bench"):
        return "TN_GARDEN"
    return None


def semantic_ground_layer(grid: Sequence[Sequence[str]], catalog: Dict[str, dict]) -> List[List[str]]:
    width, height = len(grid[0]) * PLATE_TILES, len(grid) * PLATE_TILES
    result = [["TN_GRASS" for _ in range(width)] for _ in range(height)]
    for r, row in enumerate(grid):
        for c, name in enumerate(row):
            plate = catalog[name]
            ox, oy = c * PLATE_TILES, r * PLATE_TILES
            for ly, ground_row in enumerate(plate["ground"]):
                for lx, kind in enumerate(ground_row):
                    tag = TOWN_GROUND_TAGS.get(kind)
                    if kind == "cobble" and plate.get("category") == "plaza":
                        tag = "TN_COURT"
                    elif kind == "road" and plate.get("category") == "canal_bridge":
                        tag = "TN_BRIDGE"
                    elif kind == "water" and plate.get("category") in ("waterway", "canal_bridge"):
                        tag = "TN_CANAL"
                    if tag is None:
                        raise ValueError(f"No town semantic tag for ground {kind!r} in {name}")
                    result[oy + ly][ox + lx] = tag
    return result


def build_semantic_layers(grid: Sequence[Sequence[str]], catalog: Dict[str, dict],
                          gates: Sequence[str], biome: str, seed: int, tier: str) -> dict:
    cols, rows = len(grid[0]), len(grid)
    width, height = cols * PLATE_TILES, rows * PLATE_TILES
    ground = [["grass" for _ in range(width)] for _ in range(height)]
    solids: Set[Tuple[int, int]] = set()
    buildings: List[dict] = []
    plaza_targets: List[dict] = []
    placements: List[dict] = []
    decorations: List[dict] = []
    occupied_keepouts: List[Tuple[str, Set[Tuple[int, int]]]] = []

    def decorate_open_plate(r: int, c: int) -> None:
        """Seed compact park/commons props inside a grass-only district plate."""
        rng = random.Random(stable_seed(seed, tier, biome, r, c, "open-plate"))
        pool = BIOME_DECOR[biome]
        tree_sites = [(1, 1), (6, 1), (1, 6), (6, 6)]
        shrub_sites = [(3, 2), (4, 2), (3, 5), (4, 5)]

        def add(asset: str, x: int, y: int, role: str) -> None:
            point = (c * PLATE_TILES + x, r * PLATE_TILES + y)
            if point in solids:
                raise ValueError(f"Decoration collision at {point} in {tier} plate ({r},{c})")
            solids.add(point)
            decorations.append({
                "asset": asset,
                "role": role,
                "semantic_tag": town_object_tag({"asset": asset, "role": role}),
                "anchor_tile": [point[0], point[1]],
                "anchor_kind": "foot",
                "solid_cells": [[point[0], point[1]]],
                "plate_row": r,
                "plate_col": c,
            })

        style = rng.choice(("grove", "garden", "commons"))
        if style == "grove":
            sites = rng.sample(tree_sites, 2)
            for index, (x, y) in enumerate(sites):
                add(rng.choice(pool["trees"]), x, y, f"park_tree_{index}")
            x, y = rng.choice(shrub_sites)
            add(pool["shrub"], x, y, "park_shrub")
        elif style == "garden":
            add("bench", 3, 6, "garden_bench")
            add(pool["accent"], 1, 2, f"{pool['accent_role']}_west")
            add(pool["accent"], 6, 2, f"{pool['accent_role']}_east")
        else:
            x, y = rng.choice(tree_sites)
            add(rng.choice(pool["trees"]), x, y, "commons_tree")
            add("bench", 3, 6, "commons_bench")
            if rng.random() < 0.65:
                sx, sy = rng.choice([point for point in shrub_sites if abs(point[0] - x) + abs(point[1] - y) > 2])
                add(pool["shrub"], sx, sy, "commons_shrub")

    for r in range(rows):
        for c in range(cols):
            name = grid[r][c]
            plate = catalog[name]
            ox, oy = c * PLATE_TILES, r * PLATE_TILES
            for ly, row in enumerate(plate["ground"]):
                for lx, kind in enumerate(row):
                    ground[oy + ly][ox + lx] = kind
            for lx, ly in plate.get("solids", []):
                solids.add((ox + lx, oy + ly))
            placed_objects = []
            for obj in plate.get("objects", []):
                placed_obj = dict(obj)
                ax, ay = obj["anchor_tile"]
                placed_obj["plate_anchor_tile"] = [ax, ay]
                placed_obj["anchor_tile"] = [ox + ax, oy + ay]
                if "solid_cells" in obj:
                    placed_obj["solid_cells"] = [[ox + x, oy + y] for x, y in obj["solid_cells"]]
                if "solid_cell" in obj:
                    x, y = obj["solid_cell"]
                    placed_obj["solid_cells"] = [[ox + x, oy + y]]
                    placed_obj.pop("solid_cell")
                semantic_tag = town_object_tag(obj)
                if semantic_tag:
                    placed_obj["semantic_tag"] = semantic_tag
                placed_objects.append(placed_obj)
            local_sockets = plate.get("sockets", {})
            global_sockets = {
                side: [[ox + x, oy + y] for x, y in points]
                for side, points in local_sockets.items()
            }
            placement = {"row": r, "col": c, "plate": name,
                         "site_id": f"{tier}:{seed}:{name}:{r},{c}",
                         "edge_labels": plate["edge_labels"], "sockets": global_sockets,
                         "plate_sockets": local_sockets, "objects": placed_objects}
            placements.append(placement)
            if name == "green_open":
                decorate_open_plate(r, c)
            if plate.get("category") == "lot" and plate.get("building"):
                footprint = plate["footprint"]
                keepout = plate["keepout"]
                footprint_cells = {
                    (ox + x, oy + y)
                    for y in range(footprint["y"], footprint["y"] + footprint["height"])
                    for x in range(footprint["x"], footprint["x"] + footprint["width"])
                }
                keepout_cells = {
                    (ox + x, oy + y)
                    for y in range(keepout["y"], keepout["y"] + keepout["height"])
                    for x in range(keepout["x"], keepout["x"] + keepout["width"])
                }
                if not footprint_cells.issubset(keepout_cells):
                    raise ValueError(f"{name}: building footprint escapes its declared roof/alley keepout")
                interior_seed = stable_seed(seed, tier, plate["building"], r, c)
                site_id = f"interior:{interior_seed}"
                interior_kind = "house" if plate["building"].startswith("house_") else plate["building"]
                buildings.append({
                    "kind": plate["building"],
                    "interior_kind": interior_kind,
                    "interior_seed": interior_seed,
                    "plate": name,
                    "site_id": site_id,
                    "plate_row": r,
                    "plate_col": c,
                    "footprint": {"x": ox + footprint["x"], "y": oy + footprint["y"],
                                  "width": footprint["width"], "height": footprint["height"]},
                    "door": {"tx": ox + plate["door"][0], "ty": oy + plate["door"][1],
                             "portal_id": plate["door_id"], "semantic_tag": "TN_LOT_DOOR"},
                    "apron": [[ox + x, oy + y] for x, y in plate["apron"]],
                })
                occupied_keepouts.append((name, keepout_cells))
            if name == "plaza_well":
                for approach in ((2, 3), (5, 3), (3, 2), (4, 2), (2, 2), (1, 3)):
                    plaza_targets.append({"label": "well_or_noticeboard", "tx": ox + approach[0], "ty": oy + approach[1]})

    # Perimeter wall overlay leaves only active gates open. Gate sockets themselves
    # are two tiles wide and already edge-labelled by the plate catalog.
    perimeter: Set[Tuple[int, int]] = set()
    gate_openings: Dict[str, List[Tuple[int, int]]] = {}
    center_col, main_row = cols // 2, rows // 2
    for x in range(width):
        perimeter.add((x, 0))
        perimeter.add((x, height - 1))
    for y in range(height):
        perimeter.add((0, y))
        perimeter.add((width - 1, y))
    if "N" in gates:
        gate_openings["N"] = [(center_col * PLATE_TILES + x, 0) for x in (3, 4)]
    if "S" in gates:
        gate_openings["S"] = [(center_col * PLATE_TILES + x, height - 1) for x in (3, 4)]
    if "W" in gates:
        gate_openings["W"] = [(0, main_row * PLATE_TILES + y) for y in (3, 4)]
    if "E" in gates:
        gate_openings["E"] = [(width - 1, main_row * PLATE_TILES + y) for y in (3, 4)]
    for opening in gate_openings.values():
        perimeter.difference_update(opening)

    walkable = [
        [ground[y][x] not in ("water", "wall") and (x, y) not in solids and (x, y) not in perimeter
         for x in range(width)]
        for y in range(height)
    ]

    gate_starts: Dict[str, Tuple[int, int]] = {}
    if "N" in gates:
        gate_starts["N"] = (center_col * PLATE_TILES + 3, 1)
    if "S" in gates:
        gate_starts["S"] = (center_col * PLATE_TILES + 3, height - 2)
    if "W" in gates:
        gate_starts["W"] = (1, main_row * PLATE_TILES + 3)
    if "E" in gates:
        gate_starts["E"] = (width - 2, main_row * PLATE_TILES + 3)

    portals = []
    for side in gates:
        tx, ty = gate_starts[side]
        portals.append({"id": f"gate_{side.lower()}", "side": side,
                        "tx": tx, "ty": ty, "semantic_tag": "TN_GATE",
                        "target_map": "overworld",
                        "target_portal": f"{side.lower()}_approach"})
    for building in buildings:
        door = building["door"]
        portals.append({"id": door["portal_id"], "tx": door["tx"], "ty": door["ty"],
                        "semantic_tag": "TN_LOT_DOOR", "target_map": building["site_id"],
                        "target_portal": "door_exit", "building_kind": building["kind"]})

    return {
        "width": width,
        "height": height,
        "ground": ground,
        "solid": [[x, y] for x, y in sorted(solids, key=lambda p: (p[1], p[0]))],
        "perimeter_wall": [[x, y] for x, y in sorted(perimeter, key=lambda p: (p[1], p[0]))],
        "walkable": walkable,
        "gate_starts": {side: list(p) for side, p in gate_starts.items()},
        "gate_openings": {side: [list(p) for p in points] for side, points in gate_openings.items()},
        "portals": portals,
        "buildings": buildings,
        "plaza_targets": plaza_targets,
        "placements": placements,
        "decorations": decorations,
        "keepouts": occupied_keepouts,
        "tier": tier,
        "biome": biome,
    }


def flood(walkable: Sequence[Sequence[bool]], start: Tuple[int, int]) -> Set[Tuple[int, int]]:
    height, width = len(walkable), len(walkable[0])
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


def check_grid_edges(grid: Sequence[Sequence[str]], catalog: Dict[str, dict]) -> List[str]:
    errors = []
    for r, row in enumerate(grid):
        for c, name in enumerate(row):
            edges = catalog[name]["edge_labels"]
            if c + 1 < len(row):
                other = row[c + 1]
                if edges["E"] != catalog[other]["edge_labels"]["W"]:
                    errors.append(f"H ({r},{c}) {name}.E != {other}.W")
            if r + 1 < len(grid):
                other = grid[r + 1][c]
                if edges["S"] != catalog[other]["edge_labels"]["N"]:
                    errors.append(f"V ({r},{c}) {name}.S != {other}.N")
    return errors


def validate_layout(grid: Sequence[Sequence[str]], layers: dict, catalog: Dict[str, dict], gates: Sequence[str]) -> dict:
    errors = check_grid_edges(grid, catalog)
    missing_required = []
    required = required_lots_for(layers["tier"])
    flat = [name for row in grid for name in row]
    for name in required:
        if flat.count(name) != 1:
            missing_required.append(name)

    overlap_count = 0
    keepouts = layers["keepouts"]
    for i, (_name_a, a) in enumerate(keepouts):
        for _name_b, b in keepouts[i + 1:]:
            if a & b:
                overlap_count += 1
    if missing_required:
        errors.append(f"missing required lots: {', '.join(missing_required)}")
    if overlap_count:
        errors.append(f"building/roof keepout overlap count: {overlap_count}")

    rows, cols = len(grid), len(grid[0])
    center_col, main_row = cols // 2, rows // 2
    expected_gate_cells = {
        "N": (0, center_col), "S": (rows - 1, center_col),
        "W": (main_row, 0), "E": (main_row, cols - 1),
    }
    expected_openings = {
        "N": [[center_col * PLATE_TILES + x, 0] for x in (3, 4)],
        "S": [[center_col * PLATE_TILES + x, rows * PLATE_TILES - 1] for x in (3, 4)],
        "W": [[0, main_row * PLATE_TILES + y] for y in (3, 4)],
        "E": [[cols * PLATE_TILES - 1, main_row * PLATE_TILES + y] for y in (3, 4)],
    }
    if set(layers["gate_openings"]) != set(gates):
        errors.append("perimeter gate openings do not match requested active gates")
    if set(layers["gate_starts"]) != set(gates):
        errors.append("gate start cells do not match requested active gates")
    perimeter_cells = {tuple(point) for point in layers["perimeter_wall"]}
    boundary_cells = ({(x, 0) for x in range(cols * PLATE_TILES)}
                      | {(x, rows * PLATE_TILES - 1) for x in range(cols * PLATE_TILES)}
                      | {(0, y) for y in range(rows * PLATE_TILES)}
                      | {(cols * PLATE_TILES - 1, y) for y in range(rows * PLATE_TILES)})
    all_openings = {tuple(point) for points in layers["gate_openings"].values() for point in points}
    if perimeter_cells != boundary_cells - all_openings:
        errors.append("perimeter wall does not seal every non-gate boundary cell")
    for side in "NESW":
        r, c = expected_gate_cells[side]
        plate = catalog[grid[r][c]]
        if side in gates:
            if plate.get("category") != "gate" or plate.get("gate_side") != side:
                errors.append(f"active {side} exit is not backed by its matching gate plate")
            opening = layers["gate_openings"].get(side, [])
            if [list(point) for point in opening] != expected_openings[side] or len(opening) != 2:
                errors.append(f"active {side} exit does not expose its two-tile boundary socket")
            if any(tuple(point) in perimeter_cells for point in opening):
                errors.append(f"active {side} boundary socket is covered by the perimeter wall")
        elif any(tuple(point) not in perimeter_cells for point in expected_openings[side]):
            errors.append(f"inactive {side} boundary socket is not sealed by the perimeter wall")

    targets: List[Tuple[str, Tuple[int, int]]] = []
    for building in layers["buildings"]:
        targets.append((f"{building['kind']}_door", (building["door"]["tx"], building["door"]["ty"])))
        for i, point in enumerate(building["apron"]):
            targets.append((f"{building['kind']}_apron_{i}", tuple(point)))
    for target in layers["plaza_targets"]:
        targets.append((target["label"], (target["tx"], target["ty"])))

    gate_reachability = {}
    for side, start in layers["gate_starts"].items():
        reached = flood(layers["walkable"], tuple(start))
        unreachable = [label for label, point in targets if point not in reached]
        other_gates = [other for other, coord in layers["gate_starts"].items()
                       if other != side and tuple(coord) not in reached]
        gate_reachability[side] = {"targets_reached": len(targets) - len(unreachable),
                                   "targets_total": len(targets),
                                   "unreachable_targets": unreachable,
                                   "unreachable_gates": other_gates}
        if unreachable:
            errors.append(f"gate {side}: unreachable targets {', '.join(unreachable)}")
        if other_gates:
            errors.append(f"gate {side}: disconnected active gates {', '.join(other_gates)}")

    if not gates:
        errors.append("town must have at least one active gate")
    return {
        "side_match_violations": len(check_grid_edges(grid, catalog)),
        "overlap_count": overlap_count,
        "missing_required_lots": missing_required,
        "gate_reachability": gate_reachability,
        "unreachable_target_count": sum(len(v["unreachable_targets"]) for v in gate_reachability.values()),
        "errors": errors,
    }


def build_layout(seed: int, tier: str, biome: str, gates: Sequence[str], catalog: Dict[str, dict]) -> dict:
    if tier not in TIERS:
        raise ValueError(f"--tier must be one of {', '.join(TIERS)}")
    if biome not in BIOME_GROUND:
        raise ValueError(f"--biome must be one of {', '.join(BIOME_GROUND)}")
    if not gates:
        raise ValueError("At least one active gate is required")
    grid, backtracks = make_plate_grid(seed, tier, biome, gates, catalog)
    layers = build_semantic_layers(grid, catalog, gates, biome, seed, tier)
    cols, rows = len(grid[0]), len(grid)
    validation = validate_layout(grid, layers, catalog, gates)
    semantic_ground = semantic_ground_layer(grid, catalog)
    for building in layers["buildings"]:
        door = building["door"]
        semantic_ground[door["ty"]][door["tx"]] = "TN_LOT_DOOR"
    for tx, ty in layers["gate_starts"].values():
        semantic_ground[ty][tx] = "TN_GATE"
    return {
        "schema_version": 1,
        "kind": "town",
        "site_id": f"town:{seed}:{tier}",
        "seed": seed,
        "tier": tier,
        "biome": biome,
        "gates": list(gates),
        "district_cols": cols,
        "district_rows": rows,
        "width": cols * PLATE_TILES,
        "height": rows * PLATE_TILES,
        "backtracks": backtracks,
        "grid": [list(row) for row in grid],
        "placements": layers["placements"],
        "decorations": layers["decorations"],
        "layers": {
            "ground": semantic_ground,
            "solid": layers["solid"],
            "perimeter_wall": layers["perimeter_wall"],
            "gate_openings": layers["gate_openings"],
            "walkable": layers["walkable"],
        },
        "buildings": layers["buildings"],
        "plaza_targets": layers["plaza_targets"],
        "gate_starts": layers["gate_starts"],
        "portals": layers["portals"],
        "validation": validation,
    }


def render_layout(layout: dict, catalog: Dict[str, dict]) -> Image.Image:
    cols, rows = layout["district_cols"], layout["district_rows"]
    width, height = layout["width"], layout["height"]
    image = Image.new("RGBA", (width * TILE, height * TILE), (0, 0, 0, 0))
    base_tile = GROUND_TILES[BIOME_GROUND[layout["biome"]]]
    placements = layout["placements"]
    for placement in placements:
        r, c, name = placement["row"], placement["col"], placement["plate"]
        plate = catalog[name]
        ox, oy = c * PLATE_TILES, r * PLATE_TILES
        for ly, row in enumerate(plate["ground"]):
            for lx, kind in enumerate(row):
                if kind == "grass":
                    tile = base_tile
                elif kind == "wall":
                    tile = MASONRY
                else:
                    tile = GROUND_TILES[{"road": "dirt", "cobble": "cobble", "water": "water_0",
                                         "soil": "soil", "bridge": "bridge"}[kind]]
                image.alpha_composite(tile, ((ox + lx) * TILE, (oy + ly) * TILE))

    for x, y in layout["layers"]["perimeter_wall"]:
        image.alpha_composite(MASONRY, (x * TILE, y * TILE))

    props = []
    for placement in placements:
        for obj in placement.get("objects", []):
            global_ax, global_ay = obj["anchor_tile"]
            props.append((global_ay, global_ax, obj, global_ax, global_ay))
    for obj in layout.get("decorations", []):
        global_ax, global_ay = obj["anchor_tile"]
        props.append((global_ay, global_ax, obj, global_ax, global_ay))
    props.sort(key=lambda item: (item[0], item[1], item[2].get("role", "")))
    sprite_cache: Dict[Tuple[str, str], Image.Image] = {}
    for _base_y, _base_x, obj, anchor_x, anchor_y in props:
        asset = obj["asset"]
        kind = "tiles" if obj.get("asset_kind") == "tile" else "sprites"
        key = (kind, asset)
        if key not in sprite_cache:
            sprite_cache[key] = load_rgba(ASSETS / kind / f"{asset}.png")
        sprite = sprite_cache[key]
        global_ax = anchor_x * TILE
        global_ay = anchor_y * TILE
        if obj.get("anchor_kind") == "center":
            x = global_ax + (TILE - sprite.width) // 2
            y = global_ay + (TILE - sprite.height) // 2
        else:
            x = global_ax + (TILE - sprite.width) // 2
            y = global_ay + TILE - sprite.height
        if obj.get("rotate", 0):
            sprite = sprite.rotate(obj["rotate"], expand=True, resample=Image.Resampling.NEAREST)
        image.alpha_composite(sprite, (int(x), int(y)))
    return image


def image_bytes(image: Image.Image) -> bytes:
    stream = BytesIO()
    image.save(stream, format="PNG")
    return stream.getvalue()


def json_bytes(layout: dict) -> bytes:
    return (json.dumps(layout, indent=2, sort_keys=False) + "\n").encode("utf-8")


def resolve_output(path_text: str | None, default: Path) -> Path:
    path = Path(path_text) if path_text else default
    return path if path.is_absolute() else ROOT / path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--seed", type=int, default=4444, help="deterministic seed (default: 4444)")
    parser.add_argument("--tier", choices=tuple(TIERS), default="hamlet")
    parser.add_argument("--biome", choices=tuple(BIOME_GROUND), default="temperate")
    parser.add_argument("--gates", default="N,E,S,W", help="active town gates: comma-separated N,E,S,W (default: all)")
    parser.add_argument("--out", help="PNG preview path")
    parser.add_argument("--json", help="JSON layout path")
    parser.add_argument("--check", action="store_true", help="verify edge matches, reachability, overlap, and byte-stable regeneration")
    args = parser.parse_args()
    try:
        gates = parse_gates(args.gates)
        catalog = load_catalog()
        layout = build_layout(args.seed, args.tier, args.biome, gates, catalog)
        image = render_layout(layout, catalog)
        if args.check:
            errors = layout["validation"]["errors"]
            assert not errors, "town layout invalid: " + "; ".join(errors)
            layout_again = build_layout(args.seed, args.tier, args.biome, gates, catalog)
            image_again = render_layout(layout_again, catalog)
            assert json_bytes(layout) == json_bytes(layout_again), "determinism check failed: JSON changed"
            assert image_bytes(image) == image_bytes(image_again), "determinism check failed: PNG changed"

        gate_key = "".join(gates) or "none"
        default_png = OUT_DIR / f"town_{args.tier}_{args.biome}_gates-{gate_key}_seed{args.seed}.png"
        png_path = resolve_output(args.out, default_png)
        json_path = resolve_output(args.json, png_path.with_suffix(".json"))
        png_path.parent.mkdir(parents=True, exist_ok=True)
        json_path.parent.mkdir(parents=True, exist_ok=True)
        image.save(png_path)
        json_path.write_bytes(json_bytes(layout))

        val = layout["validation"]
        print(
            f"town seed={args.seed} tier={args.tier} biome={args.biome} grid={layout['district_cols']}x{layout['district_rows']} "
            f"({layout['width']}x{layout['height']} tiles) gates={','.join(gates)} "
            f"plates={len(layout['placements'])} backtracks={layout['backtracks']} "
            f"side_violations={val['side_match_violations']} overlaps={val['overlap_count']} "
            f"missing={len(val['missing_required_lots'])} unreachable={val['unreachable_target_count']}"
        )
        return 0
    except (ValueError, FileNotFoundError, AssertionError, RuntimeError) as exc:
        print(f"build_town_layout: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
