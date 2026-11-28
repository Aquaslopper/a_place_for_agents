#!/usr/bin/env python3
"""Build a seeded, Carcassonne edge-matched overworld grid from the 3x3 plates.

RUN FROM THE REPO ROOT:
    python3 tools/build_overworld_layout.py --seed 4444
    python3 tools/build_overworld_layout.py --seed 1337 --cols 16 --rows 12 --check

Reads the 100 48x48 px (3x3 tile) plates in `docs/overworld_assets/plates/`
(`index.json`), derives 4-tuple edge labels `(N, E, S, W)` from each plate's
3x3 terrain mask, solves a coherent biome-biased Carcassonne tiling in row-major
order with arc-consistency (AC-3) lookahead and backtracking, punches rare
site plates into grass cells and swaps transporter plates onto matching bases,
and writes both a PNG preview and a deterministic JSON layout.

EDGE LABELING
-------------
Each plate side is labelled by the 3-cell sequence along that edge, ordered
left-to-right for N/S and top-to-bottom for E/W, with each cell equal to the
plate's terrain id or `'grass'`.
- `grass_plain` and all 22 site plates: all four sides are `('grass','grass','grass')`.
- `<terrain>_<mask>`: derived from `MASKS[mask]`.
- Transporters inherit their base plate edges:
    `bridge_over_water` -> `water_vchannel`
    `cave_in_mountain`  -> `mountain_full`
    `gate_pass`         -> `mountain_hchannel`
Two adjacent plates match iff `left.E == right.W` and `top.S == bottom.N`.
Grid outer borders are unconstrained (open boundary).

BIOME WEIGHTING
---------------
A seeded spatial biome guide shapes a grass-dominated continent (~45-65% grass)
with coherent terrain zones whose band orientations never cross-block each
other (since mixed non-grass plates do not exist):
- Polar/edge `snow` and `sand` caps or coastal strips aligned with the seed's
  primary axis.
- A continuous `mountain` ridge/massif (yielding `gate_pass` on 1-row ridges
  and `cave_in_mountain` inside multi-row massifs) plus isolated peaks.
- Woodland `forest` groves and inland `water` lakes (both have all 17 masks,
  so they close cleanly anywhere on grass) plus an optional river channel
  (`water_vchannel` with `bridge_over_water`) and `dirt` trade road.
At each cell `(r, c)`, valid candidates are scored by:
1. Per-terrain family normalization so multi-mask terrains (`forest`, `water`)
   do not drown out `grass_plain`;
2. Target terrain match (weight 40.0) + continuity with already-placed N/W
   neighbours (2.5), with `grass` fallback at 0.25 and unrelated terrains at
   0.005;
3. 4-side edge alignment (`N, E, S, W`) against neighbouring biome targets
   (mismatch penalized `x0.001` un-normalized), preventing edge-band plates
   like `snow_bottom` or `snow_right` from flooding across the rest of the map;
4. Anti-chaining on `*_pond` plates (`x0.0005` when adjacent to an identical
   pond) so isolated peaks/ponds stay sparse rather than forming dotted lines.
"""
from __future__ import annotations

import argparse
import json
import random
from collections import Counter, defaultdict
from pathlib import Path
from typing import Dict, List, Optional, Set, Tuple

from PIL import Image

PLATES_DIR = Path("docs/overworld_assets/plates")

# Exact 3x3 binary masks from tools/build_overworld_plates.py
MASKS: Dict[str, List[List[int]]] = {
    "full": [[1, 1, 1], [1, 1, 1], [1, 1, 1]],
    "pond": [[0, 0, 0], [0, 1, 0], [0, 0, 0]],
    "top": [[1, 1, 1], [0, 0, 0], [0, 0, 0]],
    "bottom": [[0, 0, 0], [0, 0, 0], [1, 1, 1]],
    "left": [[1, 0, 0], [1, 0, 0], [1, 0, 0]],
    "right": [[0, 0, 1], [0, 0, 1], [0, 0, 1]],
    "top2": [[1, 1, 1], [1, 1, 1], [0, 0, 0]],
    "bottom2": [[0, 0, 0], [1, 1, 1], [1, 1, 1]],
    "left2": [[1, 1, 0], [1, 1, 0], [1, 1, 0]],
    "right2": [[0, 1, 1], [0, 1, 1], [0, 1, 1]],
    "nw": [[1, 1, 0], [1, 1, 0], [0, 0, 0]],
    "ne": [[0, 1, 1], [0, 1, 1], [0, 0, 0]],
    "sw": [[0, 0, 0], [1, 1, 0], [1, 1, 0]],
    "se": [[0, 0, 0], [0, 1, 1], [0, 1, 1]],
    "vchannel": [[0, 1, 0], [0, 1, 0], [0, 1, 0]],
    "hchannel": [[0, 0, 0], [1, 1, 1], [0, 0, 0]],
    "cross": [[0, 1, 0], [1, 1, 1], [0, 1, 0]],
}

TERRAINS = ("water", "forest", "sand", "snow", "dirt", "mountain")

TRANSPORTER_BASES: Dict[str, str] = {
    "bridge_over_water": "water_vchannel",
    "cave_in_mountain": "mountain_full",
    "gate_pass": "mountain_hchannel",
}

SITE_PLATES: Set[str] = {
    "tree_round", "tree_dead", "tree_large", "bush_flower", "cactus", "palm",
    "town_red", "town_snow", "town_wood", "town_snow2",
    "castle_grey", "castle_blue",
    "cave", "tower", "temple", "monument", "camp", "ruins",
    "volcano", "gate", "bridge", "colosseum",
}

TOWN_PLATES = ("town_red", "town_wood", "town_snow", "town_snow2")

EdgeSeq = Tuple[str, str, str]
PlateEdges = Tuple[EdgeSeq, EdgeSeq, EdgeSeq, EdgeSeq]  # (N, E, S, W)

GRASS_EDGE: EdgeSeq = ("grass", "grass", "grass")


def mask_edges(terrain: str, mask: List[List[int]]) -> PlateEdges:
    grid = [[terrain if cell else "grass" for cell in row] for row in mask]
    north = (grid[0][0], grid[0][1], grid[0][2])
    east = (grid[0][2], grid[1][2], grid[2][2])
    south = (grid[2][0], grid[2][1], grid[2][2])
    west = (grid[0][0], grid[1][0], grid[2][0])
    return (north, east, south, west)


def edge_terrain(seq: EdgeSeq) -> str:
    for cell in seq:
        if cell != "grass":
            return cell
    return "grass"


def load_plate_catalog(
    plates_dir: Path = PLATES_DIR,
) -> Tuple[List[str], Dict[str, PlateEdges], Dict[str, str], Dict[str, str]]:
    index_path = plates_dir / "index.json"
    data = json.loads(index_path.read_text(encoding="utf-8"))
    names: List[str] = list(data["names"])

    edges: Dict[str, PlateEdges] = {}
    terrain_of: Dict[str, str] = {}
    mask_of: Dict[str, str] = {}

    for name in names:
        if name == "grass_plain":
            edges[name] = (GRASS_EDGE, GRASS_EDGE, GRASS_EDGE, GRASS_EDGE)
            terrain_of[name] = "grass"
            mask_of[name] = "plain"
        elif name in TRANSPORTER_BASES:
            base = TRANSPORTER_BASES[name]
            terr, mname = base.split("_", 1)
            edges[name] = mask_edges(terr, MASKS[mname])
            terrain_of[name] = terr
            mask_of[name] = mname
        elif name in SITE_PLATES:
            edges[name] = (GRASS_EDGE, GRASS_EDGE, GRASS_EDGE, GRASS_EDGE)
            terrain_of[name] = "site"
            mask_of[name] = "site"
        else:
            terr, mname = name.split("_", 1)
            if terr not in TERRAINS or mname not in MASKS:
                raise ValueError(f"Unrecognized plate name in index.json: {name}")
            edges[name] = mask_edges(terr, MASKS[mname])
            terrain_of[name] = terr
            mask_of[name] = mname

    return names, edges, terrain_of, mask_of


def build_biome_target(rng: random.Random, rows: int, cols: int) -> List[List[str]]:
    """Create a deterministic spatial biome guide map on a grass continent."""
    target = [["grass" for _ in range(cols)] for _ in range(rows)]
    horiz_axis = rng.random() < 0.55  # E-W bands vs N-S bands

    if horiz_axis:
        # Northern snow cap (1-2 rows)
        if rows >= 6 and rng.random() < 0.78:
            snow_h = 1 if rows < 10 else rng.choice((1, 2))
            for r in range(snow_h):
                for c in range(cols):
                    target[r][c] = "snow"

        # Southern desert sand strip (1-2 rows)
        if rows >= 6 and rng.random() < 0.72:
            sand_h = 1 if rows < 10 else rng.choice((1, 2))
            for r in range(rows - sand_h, rows):
                for c in range(cols):
                    if target[r][c] == "grass":
                        target[r][c] = "sand"

        # Mountain ridge (1 row -> mountain_hchannel/gate_pass) or massif (3 rows -> cave_in_mountain)
        if rows >= 7:
            open_band_rows = [
                r for r in range(2, rows - 2)
                if all(target[r][c] == "grass" for c in range(cols))
            ]
            if open_band_rows:
                # Prefer a 1-row ridge with gate_pass, or 3-row massif when plenty of room
                if rows >= 11 and rng.random() < 0.45:
                    triples = [r for r in open_band_rows if (r + 1) in open_band_rows and (r + 2) in open_band_rows]
                    if triples:
                        mr = rng.choice(triples)
                        for rr in (mr, mr + 1, mr + 2):
                            for c in range(cols):
                                target[rr][c] = "mountain"
                if not any(target[r][0] == "mountain" for r in range(rows)):
                    mr = rng.choice(open_band_rows)
                    for c in range(cols):
                        target[mr][c] = "mountain"

        # Parallel dirt trade road along an open grass row (1 row away from mountain if possible)
        if rows >= 7 and rng.random() < 0.78:
            open_rows = [
                r for r in range(1, rows - 1)
                if all(target[r][c] == "grass" for c in range(cols))
                and all(target[r + dr][0] == "grass" for dr in (-1, 1) if 0 <= r + dr < rows)
            ]
            if open_rows:
                dr = rng.choice(open_rows)
                for c in range(cols):
                    target[dr][c] = "dirt"
    else:
        # Western snow highland & eastern desert edge
        if cols >= 8 and rng.random() < 0.75:
            snow_w = 1 if cols < 12 else rng.choice((1, 2))
            for r in range(rows):
                for c in range(snow_w):
                    target[r][c] = "snow"

        if cols >= 8 and rng.random() < 0.70:
            sand_w = 1 if cols < 12 else rng.choice((1, 2))
            for r in range(rows):
                for c in range(cols - sand_w, cols):
                    if target[r][c] == "grass":
                        target[r][c] = "sand"

        # N-S River channel (1 col wide -> water_vchannel -> bridge_over_water!)
        if cols >= 7:
            open_cols = [
                c for c in range(2, cols - 2)
                if all(target[r][c] == "grass" for r in range(rows))
                and all(target[0][c + dc] == "grass" for dc in (-1, 1))
            ]
            if open_cols:
                wc = rng.choice(open_cols)
                for r in range(rows):
                    target[r][wc] = "water"

        # Parallel N-S mountain spine (or 3-col massif) and optional dirt road
        if cols >= 9:
            open_cols = [
                c for c in range(2, cols - 2)
                if all(target[r][c] == "grass" for r in range(rows))
                and all(target[0][c + dc] == "grass" for dc in (-1, 1))
            ]
            if open_cols:
                mc = rng.choice(open_cols)
                for r in range(rows):
                    target[r][mc] = "mountain"
                open_cols2 = [c for c in open_cols if abs(c - mc) >= 3]
                if open_cols2 and rng.random() < 0.75:
                    dc_road = rng.choice(open_cols2)
                    for r in range(rows):
                        target[r][dc_road] = "dirt"

    # Helper: grass-only patch for 17-mask blobs (forest & water lakes)
    def stamp_blob(terr: str, min_w: int, max_w: int, min_h: int, max_h: int) -> None:
        bw = rng.randint(min_w, min(max_w, max(min_w, cols - 2)))
        bh = rng.randint(min_h, min(max_h, max(min_h, rows - 2)))
        cands = []
        interior = []
        for r0 in range(0, max(1, rows - bh + 1)):
            for c0 in range(0, max(1, cols - bw + 1)):
                if all(
                    target[r][c] == "grass"
                    for r in range(r0, min(rows, r0 + bh))
                    for c in range(c0, min(cols, c0 + bw))
                ):
                    cands.append((r0, c0))
                    if 0 < r0 and r0 + bh < rows and 0 < c0 and c0 + bw < cols:
                        interior.append((r0, c0))
        pool = interior if interior else cands
        if not pool:
            return
        r0, c0 = rng.choice(pool)
        for r in range(r0, min(rows, r0 + bh)):
            for c in range(c0, min(cols, c0 + bw)):
                target[r][c] = terr

    # 1-2 woodland forest groves + 1 inland water lake
    n_forest = 2 if rows * cols >= 90 else 1
    for _ in range(n_forest):
        stamp_blob("forest", 2, 4, 2, 3)
    stamp_blob("water", 2, 3, 2, 3)

    # Occasional isolated mountain foothill peak on open grass
    if rows * cols >= 80:
        grass_open = [
            (r, c) for r in range(1, rows - 1) for c in range(1, cols - 1)
            if target[r][c] == "grass"
            and all(target[r + dr][c + dc] == "grass" for dr, dc in ((-1, 0), (1, 0), (0, -1), (0, 1)))
        ]
        if grass_open:
            pr, pc = rng.choice(grass_open)
            target[pr][pc] = "mountain"

    return target


def solve_terrain_grid(
    rng: random.Random,
    rows: int,
    cols: int,
    terrain_plates: List[str],
    edges: Dict[str, PlateEdges],
    terrain_of: Dict[str, str],
    mask_of: Dict[str, str],
    target: List[List[str]],
) -> Tuple[List[List[str]], int]:
    """Solve Carcassonne edge-matched grid in row-major order with AC-3 + backtracking."""
    compat: Dict[str, Dict[str, Set[str]]] = {
        d: {p: set() for p in terrain_plates} for d in ("N", "E", "S", "W")
    }
    for p in terrain_plates:
        p_n, p_e, p_s, p_w = edges[p]
        for q in terrain_plates:
            q_n, q_e, q_s, q_w = edges[q]
            if q_s == p_n:
                compat["N"][p].add(q)
            if q_w == p_e:
                compat["E"][p].add(q)
            if q_n == p_s:
                compat["S"][p].add(q)
            if q_e == p_w:
                compat["W"][p].add(q)

    def propagate(dom: List[List[Set[str]]], queue: List[Tuple[int, int]]) -> bool:
        in_q = set(queue)
        head = 0
        while head < len(queue):
            r, c = queue[head]
            head += 1
            in_q.discard((r, c))
            cur = dom[r][c]
            if not cur:
                return False
            for dr, dc, dme in ((-1, 0, "N"), (1, 0, "S"), (0, -1, "W"), (0, 1, "E")):
                nr, nc = r + dr, c + dc
                if 0 <= nr < rows and 0 <= nc < cols:
                    allowed: Set[str] = set()
                    for p in cur:
                        allowed |= compat[dme][p]
                    nxt = dom[nr][nc] & allowed
                    if not nxt:
                        return False
                    if len(nxt) < len(dom[nr][nc]):
                        dom[nr][nc] = nxt
                        if (nr, nc) not in in_q:
                            queue.append((nr, nc))
                            in_q.add((nr, nc))
        return True

    init_dom = [[set(terrain_plates) for _ in range(cols)] for _ in range(rows)]
    if not propagate(init_dom, [(r, c) for r in range(rows) for c in range(cols)]):
        raise RuntimeError("No valid global arc-consistent domain for plate set")

    backtracks = 0

    def rank_candidates(
        r: int,
        c: int,
        cands: List[str],
        cur_dom: List[List[Set[str]]],
    ) -> List[str]:
        tgt = target[r][c]
        # Outside the map boundary, continue the cell's own target so border bands sit flush
        tgt_n = target[r - 1][c] if r > 0 else tgt
        tgt_e = target[r][c + 1] if c + 1 < cols else tgt
        tgt_s = target[r + 1][c] if r + 1 < rows else tgt
        tgt_w = target[r][c - 1] if c > 0 else tgt

        nb_plates: List[str] = []
        nb_terrs: Set[str] = set()
        for dr, dc in ((-1, 0), (0, -1)):
            nr, nc = r + dr, c + dc
            if 0 <= nr < rows and 0 <= nc < cols and len(cur_dom[nr][nc]) == 1:
                np = next(iter(cur_dom[nr][nc]))
                nb_plates.append(np)
                nt = terrain_of[np]
                if nt != "grass":
                    nb_terrs.add(nt)

        by_terr: Dict[str, List[str]] = defaultdict(list)
        for p in cands:
            by_terr[terrain_of[p]].append(p)

        terr_weights: Dict[str, float] = {}
        for t in by_terr:
            if t == tgt:
                tw = 40.0
            elif t in nb_terrs:
                tw = 2.5
            elif t == "grass":
                tw = 0.25
            else:
                tw = 0.005
            terr_weights[t] = tw

        cand_weights: Dict[str, float] = {}
        for t, plist in by_terr.items():
            # Equalize family size first so 17-mask terrains don't overwhelm grass_plain,
            # then apply structural & 4-side target-edge alignment penalties un-normalized.
            base_share = terr_weights[t] / max(1, len(plist))
            want_n = t if tgt_n == t else "grass"
            want_e = t if tgt_e == t else "grass"
            want_s = t if tgt_s == t else "grass"
            want_w = t if tgt_w == t else "grass"

            for p in plist:
                m = mask_of[p]
                p_n, p_e, p_s, p_w = edges[p]
                et_n = edge_terrain(p_n)
                et_e = edge_terrain(p_e)
                et_s = edge_terrain(p_s)
                et_w = edge_terrain(p_w)

                mw = 1.0
                if t != "grass":
                    if m == "pond":
                        adj_same_pond = any(
                            mask_of[np] == "pond" and terrain_of[np] == t for np in nb_plates
                        )
                        if adj_same_pond:
                            mw = 0.0005
                    elif m in ("top2", "bottom2", "left2", "right2"):
                        mw = 2.2
                    elif m in ("top", "bottom", "left", "right"):
                        mw = 1.4
                    elif m == "cross":
                        mw = 0.6
                    else:
                        mw = 2.0

                for got, want in ((et_n, want_n), (et_e, want_e), (et_s, want_s), (et_w, want_w)):
                    if got != want:
                        mw *= 0.001

                cand_weights[p] = base_share * mw

        ordered: List[str] = []
        remaining = [(p, cand_weights[p]) for p in cands]
        while remaining:
            tot = sum(w for _, w in remaining)
            pick = rng.uniform(0.0, tot)
            acc = 0.0
            for i, (p, w) in enumerate(remaining):
                acc += w
                if acc >= pick or i == len(remaining) - 1:
                    ordered.append(p)
                    remaining.pop(i)
                    break
        return ordered

    def solve_cell(cell_idx: int, cur_dom: List[List[Set[str]]]) -> Optional[List[List[Set[str]]]]:
        nonlocal backtracks
        if cell_idx == rows * cols:
            return cur_dom
        r, c = divmod(cell_idx, cols)
        cands = sorted(cur_dom[r][c])
        if r > 0:
            n_plate = next(iter(cur_dom[r - 1][c]))
            cands = [p for p in cands if edges[p][0] == edges[n_plate][2]]
        if c > 0:
            w_plate = next(iter(cur_dom[r][c - 1]))
            cands = [p for p in cands if edges[p][3] == edges[w_plate][1]]
        if not cands:
            return None

        if len(cands) == 1 and len(cur_dom[r][c]) == 1:
            return solve_cell(cell_idx + 1, cur_dom)

        for choice in rank_candidates(r, c, cands, cur_dom):
            next_dom = [[set(cell) for cell in row] for row in cur_dom]
            next_dom[r][c] = {choice}
            if propagate(next_dom, [(r, c)]):
                res = solve_cell(cell_idx + 1, next_dom)
                if res is not None:
                    return res
            backtracks += 1
        return None

    solved = solve_cell(0, init_dom)
    if solved is None:
        raise RuntimeError(f"Failed to tile {cols}x{rows} grid")

    grid = [[next(iter(solved[r][c])) for c in range(cols)] for r in range(rows)]
    return grid, backtracks


def place_special_plates(
    rng: random.Random,
    grid: List[List[str]],
    terrain_of: Dict[str, str],
    mask_of: Dict[str, str],
    available: Set[str],
) -> Tuple[List[Dict[str, object]], List[Dict[str, object]]]:
    """Punch rare site plates into grass_plain cells and swap transporters onto matching bases."""
    rows = len(grid)
    cols = len(grid[0])
    transporters: List[Dict[str, object]] = []
    sites: List[Dict[str, object]] = []

    # 1. Transporter plates replace exact base plates (same side labels => always legal)
    base_to_transporter = {v: k for k, v in TRANSPORTER_BASES.items() if k in available}
    for base_name, trans_name in sorted(base_to_transporter.items()):
        coords = [(r, c) for r in range(rows) for c in range(cols) if grid[r][c] == base_name]
        if not coords:
            continue
        coords.sort(key=lambda rc: (
            0 if (0 < rc[0] < rows - 1 and 0 < rc[1] < cols - 1) else 1,
            rng.random(),
        ))
        picked: List[Tuple[int, int]] = []
        limit = 2 if len(coords) >= 8 else 1
        for r, c in coords:
            if all(abs(r - pr) + abs(c - pc) >= 4 for pr, pc in picked):
                grid[r][c] = trans_name
                picked.append((r, c))
                transporters.append({"plate": trans_name, "row": r, "col": c, "replaced": base_name})
                if len(picked) >= limit:
                    break

    def neighbours(r: int, c: int) -> List[Tuple[int, int]]:
        out = []
        for dr, dc in ((-1, 0), (1, 0), (0, -1), (0, 1)):
            nr, nc = r + dr, c + dc
            if 0 <= nr < rows and 0 <= nc < cols:
                out.append((nr, nc))
        return out

    def has_adj_terrain(r: int, c: int, terr: str, exclude_pond: bool = False) -> bool:
        for nr, nc in neighbours(r, c):
            p = grid[nr][nc]
            if terrain_of.get(p) == terr:
                if exclude_pond and mask_of.get(p) == "pond":
                    continue
                return True
        return False

    def grass_cells(prefer_interior: bool = True) -> List[Tuple[int, int]]:
        cells = [(r, c) for r in range(rows) for c in range(cols) if grid[r][c] == "grass_plain"]
        rng.shuffle(cells)
        if prefer_interior:
            cells.sort(key=lambda rc: 0 if (0 < rc[0] < rows - 1 and 0 < rc[1] < cols - 1) else 1)
        return cells

    # 2. Towns: spacing >= 4 plates apart (Euclidean >= 4)
    placed_towns: List[Tuple[int, int]] = []
    placed_sites: List[Tuple[int, int]] = []
    area = rows * cols
    max_towns = max(1, min(4, area // 48))

    for r, c in grass_cells():
        if len(placed_towns) >= max_towns:
            break
        if any(((r - tr) ** 2 + (c - tc) ** 2) ** 0.5 < 4.0 for tr, tc in placed_towns):
            continue
        if has_adj_terrain(r, c, "snow", exclude_pond=True):
            choices = [p for p in ("town_snow", "town_snow2") if p in available]
        else:
            choices = [p for p in ("town_red", "town_wood") if p in available]
        if not choices:
            choices = [p for p in TOWN_PLATES if p in available]
        if not choices:
            break
        town = rng.choice(choices)
        grid[r][c] = town
        placed_towns.append((r, c))
        placed_sites.append((r, c))
        sites.append({"plate": town, "row": r, "col": c})

    # 3. Cave & tower: only adjacent to a mountain-labelled plate if one falls out naturally
    for special in ("cave", "tower"):
        if special not in available:
            continue
        m_adj = [
            (r, c) for r, c in grass_cells()
            if has_adj_terrain(r, c, "mountain")
            and all(abs(r - sr) + abs(c - sc) >= 2 for sr, sc in placed_sites)
        ]
        if m_adj:
            r, c = m_adj[0]
            grid[r][c] = special
            placed_sites.append((r, c))
            sites.append({"plate": special, "row": r, "col": c})

    # 4. Castles & major landmarks (rare, well-spaced)
    major_pool = [
        p for p in ("castle_blue", "castle_grey", "temple", "ruins", "camp", "monument", "colosseum")
        if p in available
    ]
    rng.shuffle(major_pool)
    max_majors = max(1, min(4, area // 45))
    for special in major_pool[:max_majors]:
        cands = [
            (r, c) for r, c in grass_cells()
            if all(abs(r - sr) + abs(c - sc) >= 3 for sr, sc in placed_sites)
        ]
        if not cands:
            continue
        r, c = cands[0]
        grid[r][c] = special
        placed_sites.append((r, c))
        sites.append({"plate": special, "row": r, "col": c})

    # 5. Sparse nature dressing on remaining open grass
    max_nature = max(1, min(5, area // 32))
    nature_placed = 0
    for r, c in grass_cells(prefer_interior=False):
        if nature_placed >= max_nature:
            break
        if any(abs(r - sr) + abs(c - sc) < 2 for sr, sc in placed_sites):
            continue
        if has_adj_terrain(r, c, "sand"):
            pool = [p for p in ("palm", "cactus", "tree_dead") if p in available]
        elif has_adj_terrain(r, c, "forest"):
            pool = [p for p in ("tree_round", "tree_large", "bush_flower") if p in available]
        else:
            pool = [p for p in ("tree_round", "bush_flower", "tree_large", "tree_dead") if p in available]
        if not pool:
            continue
        pick = rng.choice(pool)
        grid[r][c] = pick
        placed_sites.append((r, c))
        sites.append({"plate": pick, "row": r, "col": c})
        nature_placed += 1

    return sites, transporters


def verify_grid(
    grid: List[List[str]],
    edges: Dict[str, PlateEdges],
) -> List[str]:
    """Verify every horizontal and vertical neighbour pair satisfies the Carcassonne rule."""
    violations: List[str] = []
    rows = len(grid)
    cols = len(grid[0]) if rows else 0

    for r in range(rows):
        for c in range(cols):
            p = grid[r][c]
            if p not in edges:
                violations.append(f"({r},{c}): unknown plate {p!r}")
                continue
            _, p_e, p_s, _ = edges[p]
            if c + 1 < cols:
                q = grid[r][c + 1]
                q_w = edges[q][3]
                if p_e != q_w:
                    violations.append(f"H ({r},{c}) {p}.E={p_e} != ({r},{c+1}) {q}.W={q_w}")
            if r + 1 < rows:
                q = grid[r + 1][c]
                q_n = edges[q][0]
                if p_s != q_n:
                    violations.append(f"V ({r},{c}) {p}.S={p_s} != ({r+1},{c}) {q}.N={q_n}")
    return violations


def render_grid_png(
    grid: List[List[str]],
    out_path: Path,
    plates_dir: Path = PLATES_DIR,
) -> Tuple[int, int]:
    rows = len(grid)
    cols = len(grid[0])
    cache: Dict[str, Image.Image] = {}
    canvas = Image.new("RGBA", (cols * 48, rows * 48))

    for r in range(rows):
        for c in range(cols):
            name = grid[r][c]
            if name not in cache:
                cache[name] = Image.open(plates_dir / f"{name}.png").convert("RGBA")
            canvas.paste(cache[name], (c * 48, r * 48))

    out_path.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(out_path)
    return canvas.size


def terrain_summary_counts(
    grid: List[List[str]],
    terrain_of: Dict[str, str],
) -> Dict[str, int]:
    counts: Counter[str] = Counter()
    for row in grid:
        for plate in row:
            if plate == "grass_plain":
                counts["grass"] += 1
            elif plate in TRANSPORTER_BASES:
                counts[terrain_of[TRANSPORTER_BASES[plate]]] += 1
            elif plate in SITE_PLATES:
                counts["site"] += 1
            else:
                counts[terrain_of[plate]] += 1
    return dict(sorted(counts.items()))


def build_layout(
    seed: int,
    cols: int = 16,
    rows: int = 12,
    plates_dir: Path = PLATES_DIR,
) -> Dict[str, object]:
    names, edges, terrain_of, mask_of = load_plate_catalog(plates_dir)
    available = set(names)
    terrain_plates = [
        n for n in names if n not in SITE_PLATES and n not in TRANSPORTER_BASES
    ]

    max_attempts = 8
    total_backtracks = 0
    grid: Optional[List[List[str]]] = None
    attempts = 0
    for attempt in range(max_attempts):
        attempts = attempt + 1
        attempt_rng = random.Random(seed if attempt == 0 else (seed * 1000003 + attempt))
        target = build_biome_target(attempt_rng, rows, cols)
        try:
            grid, bt = solve_terrain_grid(
                attempt_rng, rows, cols, terrain_plates,
                edges, terrain_of, mask_of, target,
            )
            total_backtracks += bt
            rng = attempt_rng
            break
        except RuntimeError:
            total_backtracks += 1
    if grid is None:
        raise RuntimeError(f"Could not solve {cols}x{rows} layout for seed={seed}")

    sites, transporters = place_special_plates(rng, grid, terrain_of, mask_of, available)
    violations = verify_grid(grid, edges)
    if violations:
        raise AssertionError(f"Edge-compatibility check failed with {len(violations)} violation(s): {violations[0]}")

    counts = terrain_summary_counts(grid, terrain_of)
    return {
        "seed": seed,
        "cols": cols,
        "rows": rows,
        "tiles_wide": cols * 3,
        "tiles_tall": rows * 3,
        "attempts": attempts,
        "backtracks": total_backtracks,
        "violations": len(violations),
        "terrain_counts": counts,
        "sites": sites,
        "transporters": transporters,
        "grid": grid,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate a seeded Carcassonne overworld plate layout.")
    parser.add_argument("--seed", type=int, default=4444, help="Deterministic RNG seed (default: 4444)")
    parser.add_argument("--cols", type=int, default=16, help="Grid width in 3x3 plates (default: 16)")
    parser.add_argument("--rows", type=int, default=12, help="Grid height in 3x3 plates (default: 12)")
    parser.add_argument("--out", type=str, default=None, help="Output PNG path (default: docs/overworld_assets/layout_seed<seed>.png)")
    parser.add_argument("--json", type=str, default=None, help="Output JSON path (default: <out stem>.json)")
    parser.add_argument("--check", action="store_true", help="Run explicit edge-compatibility and determinism verification")
    args = parser.parse_args()

    if args.cols < 1 or args.rows < 1:
        raise SystemExit("--cols and --rows must be >= 1")

    out_png = Path(args.out) if args.out else Path(f"docs/overworld_assets/layout_seed{args.seed}.png")
    out_json = Path(args.json) if args.json else out_png.with_suffix(".json")

    layout = build_layout(seed=args.seed, cols=args.cols, rows=args.rows)
    if args.check:
        _, edges, _, _ = load_plate_catalog(PLATES_DIR)
        violations = verify_grid(layout["grid"], edges)  # type: ignore[arg-type]
        assert not violations, f"Edge check failed: {violations}"
        layout2 = build_layout(seed=args.seed, cols=args.cols, rows=args.rows)
        assert layout == layout2, "Determinism check failed: repeated run differed"

    px_w, px_h = render_grid_png(layout["grid"], out_png)  # type: ignore[arg-type]
    out_json.parent.mkdir(parents=True, exist_ok=True)
    out_json.write_text(json.dumps(layout, indent=2) + "\n", encoding="utf-8")

    counts_str = ", ".join(f"{k}={v}" for k, v in layout["terrain_counts"].items())  # type: ignore[union-attr]
    print(
        f"seed={args.seed} grid={args.cols}x{args.rows} ({args.cols*3}x{args.rows*3} tiles, {px_w}x{px_h} px) "
        f"backtracks={layout['backtracks']} violations={layout['violations']} | {counts_str}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
