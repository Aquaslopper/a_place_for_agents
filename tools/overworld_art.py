#!/usr/bin/env python3
"""overworld_art.py — export runtime SNES overworld tileset and miniature site icons.

Sources (all vendored & credited in CREDITS.md and assets/source/overworld/):
  * assets/source/overworld/mrbeast_worldmap/tileset.png   (CC-BY 3.0, MrBeast)
  * assets/source/overworld/mrbeast_worldmap/water.png     (CC-BY 3.0, MrBeast)
  * assets/source/overworld/mrbeast_worldmap/mountains.png (CC-BY 3.0, MrBeast)
  * assets/source/openrtp/ChipSet/retro_World.png          (CC-BY / CC0 mix)

Produces:
  * assets/tiles/ow_atlas.png   (256x256 = 16x16 grid of 16x16 overworld tiles)
  * assets/sprites/ow_*.png     (14 miniature overworld site icons)

Run from the repository root:
    python3 tools/overworld_art.py
    python3 tools/generate_manifest.py
    python3 tools/validate_assets.py
"""
from __future__ import annotations

import pathlib
from PIL import Image, ImageDraw

ROOT = pathlib.Path(__file__).resolve().parents[1]
MRBEAST = ROOT / "assets" / "source" / "overworld" / "mrbeast_worldmap"
RETRO_WORLD = ROOT / "assets" / "source" / "openrtp" / "ChipSet" / "retro_World.png"
OUT_TILES = ROOT / "assets" / "tiles"
OUT_SPRITES = ROOT / "assets" / "sprites"


def cell(sheet: Image.Image, col: int, row: int, w: int = 1, h: int = 1) -> Image.Image:
    return sheet.crop((col * 16, row * 16, (col + w) * 16, (row + h) * 16)).convert("RGBA")


def key_magenta(img: Image.Image) -> Image.Image:
    out = img.copy().convert("RGBA")
    px = out.load()
    w, h = out.size
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            if a > 0 and abs(r - 224) < 16 and abs(g - 103) < 16 and abs(b - 191) < 16:
                px[x, y] = (0, 0, 0, 0)
    return out


def retro_cell(rw: Image.Image, col: int, row: int, w: int = 1, h: int = 1) -> Image.Image:
    return key_magenta(cell(rw, col, row, w, h))


def extract_island_3x3(sheet: Image.Image, c0: int, r0: int,
                       key_fn=None) -> list[list[Image.Image]]:
    grid = []
    for dr in range(3):
        row = []
        for dc in range(3):
            t = cell(sheet, c0 + dc, r0 + dr)
            if key_fn is not None:
                t = key_fn(t)
            row.append(t)
        grid.append(row)
    return grid


def assemble_16mask(isl: list[list[Image.Image]]) -> list[Image.Image]:
    """Assemble 16 cardinal-bitmask (N=1, E=2, S=4, W=8) 16x16 tiles from a 3x3 island.

    Stitches the 3x3 island into a 48x48 blob, snaps to its actual opaque bbox
    so rounded coastlines/canopies sit right at the tile boundary (not inset in
    transparent padding), then composes every mask from matching 8x8 quadrants.
    """
    blob = Image.new("RGBA", (48, 48), (0, 0, 0, 0))
    for r in range(3):
        for c in range(3):
            blob.paste(isl[r][c], (c * 16, r * 16))

    bb = blob.getbbox()
    if not bb:
        return [Image.new("RGBA", (16, 16), (0, 0, 0, 0)) for _ in range(16)]
    x0, y0, x1, y1 = bb
    # Ensure at least 16x16 span around center
    cx, cy = 24, 24
    x0 = min(x0, cx - 8)
    y0 = min(y0, cy - 8)
    x1 = max(x1, cx + 8)
    y1 = max(y1, cy + 8)

    # 8x8 quadrant slices anchored to the true visual shoreline / silhouette edge
    q_nw_c = blob.crop((x0,     y0,     x0 + 8, y0 + 8))
    q_ne_c = blob.crop((x1 - 8, y0,     x1,     y0 + 8))
    q_sw_c = blob.crop((x0,     y1 - 8, x0 + 8, y1))
    q_se_c = blob.crop((x1 - 8, y1 - 8, x1,     y1))

    q_n_l = blob.crop((cx - 8, y0,     cx,     y0 + 8))
    q_n_r = blob.crop((cx,     y0,     cx + 8, y0 + 8))
    q_s_l = blob.crop((cx - 8, y1 - 8, cx,     y1))
    q_s_r = blob.crop((cx,     y1 - 8, cx + 8, y1))

    q_w_t = blob.crop((x0,     cy - 8, x0 + 8, cy))
    q_w_b = blob.crop((x0,     cy,     x0 + 8, cy + 8))
    q_e_t = blob.crop((x1 - 8, cy - 8, x1,     cy))
    q_e_b = blob.crop((x1 - 8, cy,     x1,     cy + 8))

    c_nw = blob.crop((cx - 8, cy - 8, cx,     cy))
    c_ne = blob.crop((cx,     cy - 8, cx + 8, cy))
    c_sw = blob.crop((cx - 8, cy,     cx,     cy + 8))
    c_se = blob.crop((cx,     cy,     cx + 8, cy + 8))

    tiles: list[Image.Image] = []
    for mask in range(16):
        has_n = bool(mask & 1)
        has_e = bool(mask & 2)
        has_s = bool(mask & 4)
        has_w = bool(mask & 8)

        out = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
        out.paste(c_nw if (has_n and has_w) else (q_n_l if (not has_n and has_w) else (q_w_t if (has_n and not has_w) else q_nw_c)), (0, 0))
        out.paste(c_ne if (has_n and has_e) else (q_n_r if (not has_n and has_e) else (q_e_t if (has_n and not has_e) else q_ne_c)), (8, 0))
        out.paste(c_sw if (has_s and has_w) else (q_s_l if (not has_s and has_w) else (q_w_b if (has_s and not has_w) else q_sw_c)), (0, 8))
        out.paste(c_se if (has_s and has_e) else (q_s_r if (not has_s and has_e) else (q_e_b if (has_s and not has_e) else q_se_c)), (8, 8))
        tiles.append(out)
    return tiles


def key_grass_bg(tile: Image.Image, ref_grass: Image.Image) -> Image.Image:
    """Turn MrBeast/retro grass background pixels transparent on overlay islands."""
    out = tile.copy().convert("RGBA")
    px = out.load()
    # Sample reference grass colors
    gpx = ref_grass.convert("RGBA").load()
    grass_cols = {(gpx[x, y][0], gpx[x, y][1], gpx[x, y][2]) for y in range(16) for x in range(16)}
    for y in range(16):
        for x in range(16):
            r, g, b, a = px[x, y]
            if a == 0:
                continue
            if (r, g, b) in grass_cols:
                px[x, y] = (0, 0, 0, 0)
    return out


def make_road_tiles() -> list[Image.Image]:
    """Build 16 cardinal-bitmask SNES overworld road tiles (warm cobble/dirt path)."""
    # 4-color warm road palette matching MrBeast & retro_World earth tones
    c_outline = (74, 54, 38, 255)
    c_edge    = (146, 114, 76, 255)
    c_body    = (196, 162, 112, 255)
    c_hi      = (224, 196, 146, 255)

    tiles: list[Image.Image] = []
    for mask in range(16):
        has_n = bool(mask & 1)
        has_e = bool(mask & 2)
        has_s = bool(mask & 4)
        has_w = bool(mask & 8)

        # Build a boolean road-body grid (16x16) first, then add 1px outline & shading
        body = [[False] * 16 for _ in range(16)]
        # Central hub (rounded 6x6 from 5..10)
        for y in range(5, 11):
            for x in range(5, 11):
                if (x in (5, 10)) and (y in (5, 10)):
                    continue
                body[y][x] = True
        if has_n:
            for y in range(0, 7):
                for x in range(6, 10):
                    body[y][x] = True
        if has_s:
            for y in range(9, 16):
                for x in range(6, 10):
                    body[y][x] = True
        if has_w:
            for y in range(6, 10):
                for x in range(0, 7):
                    body[y][x] = True
        if has_e:
            for y in range(6, 10):
                for x in range(9, 16):
                    body[y][x] = True

        # Smooth elbows when connecting two orthogonal directions
        if has_n and has_e:
            body[5][10] = True
        if has_n and has_w:
            body[5][5] = True
        if has_s and has_e:
            body[10][10] = True
        if has_s and has_w:
            body[10][5] = True

        out = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
        px = out.load()
        for y in range(16):
            for x in range(16):
                if body[y][x]:
                    # Determine if on inner border of body
                    nbr_out = False
                    for dy, dx in ((-1, 0), (1, 0), (0, -1), (0, 1)):
                        ny, nx = y + dy, x + dx
                        if 0 <= ny < 16 and 0 <= nx < 16 and not body[ny][nx]:
                            nbr_out = True
                            break
                    if nbr_out:
                        px[x, y] = c_edge
                    elif ((x + y * 3) % 7 == 0) or (x in (7, 8) and y in (7, 8)):
                        px[x, y] = c_hi
                    else:
                        px[x, y] = c_body
                else:
                    # 1px dark outline around road body
                    adj = False
                    for dy, dx in ((-1, 0), (1, 0), (0, -1), (0, 1)):
                        ny, nx = y + dy, x + dx
                        if 0 <= ny < 16 and 0 <= nx < 16 and body[ny][nx]:
                            adj = True
                            break
                    if adj:
                        px[x, y] = c_outline
        tiles.append(out)
    return tiles


def make_bridge_tile(horizontal: bool) -> Image.Image:
    """Crisp 16x16 SNES stone/timber overworld bridge over ocean/river."""
    out = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
    px = out.load()
    c_dark   = (48, 38, 34, 255)
    c_rail   = (156, 144, 128, 255)
    c_railhi = (212, 204, 188, 255)
    c_plank0 = (164, 124, 78, 255)
    c_plank1 = (138, 100, 62, 255)
    c_post   = (108, 96, 86, 255)
    if horizontal:
        for x in range(16):
            px[x, 2] = c_dark
            px[x, 3] = c_railhi if (x % 4 != 0) else c_post
            px[x, 4] = c_rail if (x % 4 != 0) else c_dark
            px[x, 5] = c_dark
            for y in range(6, 10):
                px[x, y] = c_plank0 if (x % 3 != 0) else c_plank1
            px[x, 10] = c_dark
            px[x, 11] = c_railhi if (x % 4 != 0) else c_post
            px[x, 12] = c_rail if (x % 4 != 0) else c_dark
            px[x, 13] = c_dark
    else:
        for y in range(16):
            px[2, y] = c_dark
            px[3, y] = c_railhi if (y % 4 != 0) else c_post
            px[4, y] = c_rail if (y % 4 != 0) else c_dark
            px[5, y] = c_dark
            for x in range(6, 10):
                px[x, y] = c_plank0 if (y % 3 != 0) else c_plank1
            px[10, y] = c_dark
            px[11, y] = c_railhi if (y % 4 != 0) else c_post
            px[12, y] = c_rail if (y % 4 != 0) else c_dark
            px[13, y] = c_dark
    return out


def make_swamp_island(rw: Image.Image) -> list[list[Image.Image]]:
    """Build a dark marsh/mire 3x3 island tinted cleanly into the SNES palette."""
    # Use retro_World dirt/marsh block (6, 1..3) shifted into mossy mire-teal
    isl = extract_island_3x3(rw, 6, 1)
    gref = cell(rw, 1, 10)
    out_isl = []
    for row in isl:
        out_row = []
        for t in row:
            t2 = key_grass_bg(t, gref)
            px = t2.load()
            for y in range(16):
                for x in range(16):
                    r, g, b, a = px[x, y]
                    if a == 0:
                        continue
                    lum = (r * 3 + g * 5 + b * 2) // 10
                    if lum < 75:
                        px[x, y] = (40, 58, 48, 255)
                    elif lum < 115:
                        px[x, y] = (58, 86, 62, 255)
                    elif lum < 160:
                        px[x, y] = (78, 114, 74, 255)
                    else:
                        px[x, y] = (108, 138, 92, 255)
            out_row.append(t2)
        out_isl.append(out_row)
    return out_isl


def darken_deep_water(tile: Image.Image) -> Image.Image:
    out = tile.copy().convert("RGBA")
    px = out.load()
    for y in range(16):
        for x in range(16):
            r, g, b, a = px[x, y]
            px[x, y] = (max(0, int(r * 0.72)), max(0, int(g * 0.76)), max(0, int(b * 0.88)), a)
    return out


def trim_rgba(img: Image.Image) -> Image.Image:
    bb = img.getbbox()
    return img.crop(bb) if bb else img


def build_overworld_art() -> None:
    ts = Image.open(MRBEAST / "tileset.png").convert("RGBA")
    wt = Image.open(MRBEAST / "water.png").convert("RGBA")
    rw = Image.open(RETRO_WORLD).convert("RGBA")

    atlas = Image.new("RGBA", (256, 256), (0, 0, 0, 0))

    def put(col: int, row: int, tile: Image.Image) -> None:
        atlas.paste(tile, (col * 16, row * 16))

    # ---- Row 0: Animated ocean water, deep ocean, bridges, pass, details
    for f in range(4):
        wframe = wt.crop((f * 96, 0, f * 96 + 16, 16)).convert("RGBA")
        put(f, 0, wframe)
        put(4 + f, 0, darken_deep_water(wframe))

    put(8, 0, make_bridge_tile(horizontal=True))
    put(9, 0, make_bridge_tile(horizontal=False))
    # Mountain pass arch road tile
    pass_tile = cell(ts, 1, 1)  # lush grass base
    road_v = make_road_tiles()[1 | 4]  # N-S road
    pass_tile.alpha_composite(road_v)
    put(10, 0, pass_tile)
    pass_tile_h = cell(ts, 1, 1)
    pass_tile_h.alpha_composite(make_road_tiles()[2 | 8])  # E-W road
    put(11, 0, pass_tile_h)

    # Biome accents on land (grass meadow speck, cactus, dune, snow drift)
    grass_v = cell(ts, 1, 1)
    d = ImageDraw.Draw(grass_v)
    d.point((4, 5), fill=(154, 198, 86, 255))
    d.point((5, 5), fill=(154, 198, 86, 255))
    d.point((11, 11), fill=(224, 214, 124, 255))
    d.point((12, 10), fill=(154, 198, 86, 255))
    put(12, 0, grass_v)
    put(13, 0, cell(ts, 10, 20))  # desert cacti overlay
    put(14, 0, cell(ts, 10, 19))  # sand dune accent
    put(15, 0, cell(ts, 7, 1))    # clean snow interior

    # ---- Rows 1..3: Grass, Sand, Snow Coastline 16-masks (over animated water!)
    grass_coast_isl = extract_island_3x3(ts, 0, 0)
    sand_coast_isl  = extract_island_3x3(ts, 3, 0)
    snow_coast_isl  = extract_island_3x3(ts, 6, 0)
    for m, t in enumerate(assemble_16mask(grass_coast_isl)):
        put(m, 1, t)
    for m, t in enumerate(assemble_16mask(sand_coast_isl)):
        put(m, 2, t)
    for m, t in enumerate(assemble_16mask(snow_coast_isl)):
        put(m, 3, t)

    # ---- Rows 4..5: Sand-on-Grass & Snow-on-Grass transitions
    ts_grass_ref = cell(ts, 1, 1)
    sand_on_grass_isl = extract_island_3x3(ts, 9, 0, lambda t: key_grass_bg(t, ts_grass_ref))
    snow_on_grass_isl = extract_island_3x3(ts, 12, 0, lambda t: key_grass_bg(t, ts_grass_ref))
    for m, t in enumerate(assemble_16mask(sand_on_grass_isl)):
        put(m, 4, t)
    for m, t in enumerate(assemble_16mask(snow_on_grass_isl)):
        put(m, 5, t)

    # ---- Rows 6..8: Forest canopies (Leafy, Pine, Snowy Pine — transparent outside!)
    leaf_isl = extract_island_3x3(ts, 0, 10)
    pine_isl = extract_island_3x3(ts, 3, 10)
    snow_for_isl = extract_island_3x3(ts, 6, 10)
    for m, t in enumerate(assemble_16mask(leaf_isl)):
        put(m, 6, t)
    for m, t in enumerate(assemble_16mask(pine_isl)):
        put(m, 7, t)
    for m, t in enumerate(assemble_16mask(snow_for_isl)):
        put(m, 8, t)

    # ---- Rows 9..10: Brown Mountain Ranges & Snow-Capped Mountain Ranges
    rw_grass_ref = cell(rw, 1, 10)
    brown_mtn_isl = extract_island_3x3(rw, 3, 13, lambda t: key_grass_bg(t, rw_grass_ref))
    # Use the tight 64x64 mountain range blocks in mountains.png (0..64, 96..160
    # and 96..160, 96..160) — sample inner filled peaks so every mask tile is solid.
    mt = Image.open(MRBEAST / "mountains.png").convert("RGBA")
    grey_block = mt.crop((0, 96, 64, 160))
    snow_block = mt.crop((96, 96, 160, 160))
    grey_mtn_isl = [
        [grey_block.crop((8,  8, 24, 24)), grey_block.crop((24,  8, 40, 24)), grey_block.crop((40,  8, 56, 24))],
        [grey_block.crop((8, 24, 24, 40)), grey_block.crop((24, 24, 40, 40)), grey_block.crop((40, 24, 56, 40))],
        [grey_block.crop((8, 40, 24, 56)), grey_block.crop((24, 40, 40, 56)), grey_block.crop((40, 40, 56, 56))],
    ]
    snow_mtn_isl = [
        [snow_block.crop((8,  8, 24, 24)), snow_block.crop((24,  8, 40, 24)), snow_block.crop((40,  8, 56, 24))],
        [snow_block.crop((8, 24, 24, 40)), snow_block.crop((24, 24, 40, 40)), snow_block.crop((40, 24, 56, 40))],
        [snow_block.crop((8, 40, 24, 56)), snow_block.crop((24, 40, 40, 56)), snow_block.crop((40, 40, 56, 56))],
    ]
    for m, t in enumerate(assemble_16mask(brown_mtn_isl)):
        put(m, 9, t)
    for m, t in enumerate(assemble_16mask(snow_mtn_isl)):
        put(m, 10, t)

    # ---- Row 11: 16-mask Overworld Roads
    for m, t in enumerate(make_road_tiles()):
        put(m, 11, t)

    # ---- Row 12: Grey Alpine Mountain Range 16-mask (for rocky highlands!)
    for m, t in enumerate(assemble_16mask(grey_mtn_isl)):
        put(m, 12, t)

    # ---- Row 13: Swamp/Mire 16-mask (transparent outside!)
    swamp_isl = make_swamp_island(rw)
    for m, t in enumerate(assemble_16mask(swamp_isl)):
        put(m, 13, t)

    # ---- Row 14: Inner-corner coastline tiles (from MrBeast rows 3-4)
    # Grass inner corners (0,3)-(1,4), Sand (3,3)-(4,4), Snow (6,3)-(7,4)
    put(0, 14, cell(ts, 0, 3))
    put(1, 14, cell(ts, 1, 3))
    put(2, 14, cell(ts, 0, 4))
    put(3, 14, cell(ts, 1, 4))
    put(4, 14, cell(ts, 3, 3))
    put(5, 14, cell(ts, 4, 3))
    put(6, 14, cell(ts, 3, 4))
    put(7, 14, cell(ts, 4, 4))
    put(8, 14, cell(ts, 6, 3))
    put(9, 14, cell(ts, 7, 3))
    put(10, 14, cell(ts, 6, 4))
    put(11, 14, cell(ts, 7, 4))
    # Foothills / sandy hills (from retro_World)
    put(12, 14, key_grass_bg(cell(rw, 3, 12), rw_grass_ref))
    put(13, 14, key_grass_bg(cell(rw, 5, 12), rw_grass_ref))
    put(14, 14, cell(ts, 13, 11))  # MrBeast stone road bridge H
    put(15, 14, cell(ts, 14, 11))  # MrBeast stone road bridge V

    # ---- Row 15: Extra shoreline foam waves (animated surf from MrBeast rows 5..9, cols 12..15)
    for f in range(4):
        put(f, 15, cell(ts, 12 + f, 5))
        put(4 + f, 15, cell(ts, 12 + f, 6))
        put(8 + f, 15, cell(ts, 12 + f, 7))
        put(12 + f, 15, cell(ts, 12 + f, 8))

    OUT_TILES.mkdir(parents=True, exist_ok=True)
    atlas.save(OUT_TILES / "ow_atlas.png")

    # ---- Miniature Overworld Site Icons (assets/sprites/ow_*.png)
    OUT_SPRITES.mkdir(parents=True, exist_ok=True)
    icons: dict[str, Image.Image] = {
        "ow_hamlet.png":      trim_rgba(cell(ts, 0, 15, 2, 3)),
        "ow_town.png":        trim_rgba(cell(ts, 2, 15, 3, 3)),
        "ow_town_red.png":    retro_cell(rw, 22, 8, 1, 1),
        "ow_town_snow.png":   retro_cell(rw, 23, 8, 1, 1),
        "ow_castle_grey.png": trim_rgba(cell(ts, 6, 18, 2, 3)),
        "ow_castle_blue.png": trim_rgba(retro_cell(rw, 22, 10, 2, 2)),
        "ow_cave_grass.png":  trim_rgba(cell(ts, 5, 15, 1, 1)),
        "ow_cave_sand.png":   trim_rgba(cell(ts, 5, 16, 1, 1)),
        "ow_cave_snow.png":   trim_rgba(cell(ts, 5, 17, 1, 1)),
        "ow_tower.png":       trim_rgba(cell(ts, 9, 18, 1, 3)),
        "ow_temple.png":      trim_rgba(retro_cell(rw, 22, 12, 1, 2)),
        "ow_ruins.png":       trim_rgba(retro_cell(rw, 20, 14, 2, 2)),
        "ow_camp.png":        trim_rgba(retro_cell(rw, 18, 12, 2, 2)),
        "ow_gate_pass.png":   trim_rgba(retro_cell(rw, 22, 14, 1, 2)),
    }
    for name, img in icons.items():
        img.save(OUT_SPRITES / name)
        print(f"  wrote sprites/{name:20s} {img.size[0]}x{img.size[1]}")
    print(f"wrote tiles/ow_atlas.png {atlas.size[0]}x{atlas.size[1]} + {len(icons)} site icons")


if __name__ == "__main__":
    build_overworld_art()
