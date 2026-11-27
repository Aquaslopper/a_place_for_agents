#!/usr/bin/env python3
"""Generate autotile terrain atlas `tiles/landscape.png` (256x1008) and
elevation escarpment atlas `tiles/relief.png` (256x80) strictly from the
Valley Hearth <=8-color per-biome ramps.
"""
from __future__ import annotations

import math
from snes import (
    INK_DEEP, SLATE_DARK, SLATE_MID, SLATE_LIGHT, STONE_WARM, CREAM_SUN,
    UMBER_DARK, TIMBER_MID, OCHRE_MID, OCHRE_LIGHT, GOLD_SHADE, GOLD_SUN,
    LEAF_SHADOW, LEAF_DARK, LEAF_MID, LEAF_LIGHT, LEAF_SUN,
    WATER_DEEP, WATER_MID, WATER_LIGHT, WATER_FOAM,
    BIOME_RAMPS, canvas, px, rgba, h2, save_png,
)

TILE = 16

# Matches `enum Surface` in src/render.cpp exactly:
# 0 S_DEEP, 1 S_WATER, 2 S_BEACH, 3 S_MEADOW, 4 S_WOOD, 5 S_AMBER, 6 S_HEATH,
# 7 S_FEN, 8 S_DUNES, 9 S_SNOW, 10 S_ICE, 11 S_CRAG, 12 S_PATH, 13 S_SOIL,
# 14 S_COBBLE, 15 S_COURT, 16 S_LEAVES, 17 S_RIPPLES, 18 S_MESA,
# 19 S_FROST_TRACK, 20 S_SNOW_CRAG


def _corner_inside(mask: int, x: int, y: int, organic: int = 1, seed: int = 0) -> bool:
    """Smooth 16-mask marching-squares membership for pixel (x, y) in 0..15.
    Bit 0 = NW, 1 = NE, 2 = SW, 3 = SE (matching render.cpp order).
    """
    if mask == 15:
        return True
    if mask == 0:
        return False
    nw = 1.0 if (mask & 1) else 0.0
    ne = 1.0 if (mask & 2) else 0.0
    sw = 1.0 if (mask & 4) else 0.0
    se = 1.0 if (mask & 8) else 0.0

    # Bilinear interpolation over pixel centers (0.5..15.5) shifted to corner weights
    u = (x + 0.5) / 16.0
    v = (y + 0.5) / 16.0
    w = (1.0 - u) * (1.0 - v) * nw + u * (1.0 - v) * ne + (1.0 - u) * v * sw + u * v * se
    if organic:
        # Subtle pixel-locked organic edge nibble that vanishes on inner/outer boundaries
        jitter = (h2(x, y, seed) - 0.5) * 0.14
        w += jitter
    return w >= 0.48


def _paint_surface_cell(
    im, ox: int, oy: int, s_id: int, phase: int, mask: int
) -> None:
    """Paint one 16x16 autotile cell at (ox, oy) using strict biome ramp colors."""
    # Precompute membership mask in a padded 18x18 grid to place clean 1px rims
    inside = [[False] * 16 for _ in range(16)]
    organic = 0 if s_id in (14, 15, 13, 11, 18, 20) else 1
    for y in range(16):
        for x in range(16):
            inside[y][x] = _corner_inside(mask, x, y, organic=organic, seed=s_id * 37 + 5)

    def edge_ul(x: int, y: int) -> bool:
        return (x > 0 and not inside[y][x - 1]) or (y > 0 and not inside[y - 1][x])

    def edge_br(x: int, y: int) -> bool:
        return (x < 15 and not inside[y][x + 1]) or (y < 15 and not inside[y + 1][x])

    for y in range(16):
        for x in range(16):
            if not inside[y][x]:
                continue

            col = _surface_pixel(s_id, phase, x, y)

            # Apply clean 1px selective edge framing on transitions (mask < 15)
            if mask != 15:
                if edge_br(x, y):
                    col = _edge_shadow(s_id, col)
                elif edge_ul(x, y):
                    col = _edge_highlight(s_id, col)

            px(im, ox + x, oy + y, col)


def _surface_pixel(s_id: int, phase: int, x: int, y: int) -> tuple[int, int, int]:
    """Quiet, cohesive 16x16 base fill strictly within each surface's <=8-color ramp."""
    r_meadow = BIOME_RAMPS["meadow"]
    r_forest = BIOME_RAMPS["forest"]
    r_amber  = BIOME_RAMPS["amber"]
    r_fen    = BIOME_RAMPS["fen"]
    r_heath  = BIOME_RAMPS["heath"]
    r_frost  = BIOME_RAMPS["frost"]
    r_dunes  = BIOME_RAMPS["dunes"]
    r_dung   = BIOME_RAMPS["dungeon"]

    v = h2((x + phase * 5) & 15, (y + phase * 3) & 15, s_id * 97 + phase)
    v2 = h2(x >> 1, y >> 1, s_id * 31 + phase)

    # 0: S_DEEP (3 animated ripples)
    if s_id == 0:
        wave = math.sin((x + phase * 5) * 0.42 + y * 0.28)
        if wave > 0.84 and ((x + y + phase) % 5 == 0):
            return WATER_MID
        return WATER_DEEP

    # 1: S_WATER (3 animated river shimmer frames)
    if s_id == 1:
        band = math.sin((x + phase * 5) * 0.46 + math.cos(y * 0.55) * 0.7)
        if band > 0.88 and ((y + phase) % 3 == 0):
            return WATER_FOAM
        if band > 0.45:
            return WATER_LIGHT
        if band < -0.68:
            return WATER_DEEP
        return WATER_MID

    # 2: S_BEACH (warm sand shore bank)
    if s_id == 2:
        if v > 0.87:
            return r_dunes[6]  # pale sand speck
        if v < 0.14:
            return r_dunes[4]  # ochre grain
        return r_dunes[5]

    # 3: S_MEADOW (quiet sage-emerald valley grass)
    if s_id == 3:
        # Very calm base: mostly LEAF_MID with sparse soft LEAF_LIGHT / LEAF_DARK blades
        if v > 0.90:
            return r_meadow[4]  # LEAF_LIGHT
        if v < 0.11:
            return r_meadow[2]  # LEAF_DARK
        if v2 > 0.84 and (x & 1) == 0:
            return r_meadow[4]
        return r_meadow[3]      # LEAF_MID

    # 4: S_WOOD (deep forest moss turf)
    if s_id == 4:
        if v > 0.88:
            return r_forest[4]
        if v < 0.15:
            return r_forest[2]
        return r_forest[3]

    # 5: S_AMBER (golden autumn meadow)
    if s_id == 5:
        if v > 0.88:
            return r_amber[5]
        if v < 0.14:
            return r_amber[3]
        return r_amber[4]

    # 6: S_HEATH (windswept plum-sage highland turf)
    if s_id == 6:
        if v > 0.93:
            return r_heath[6]  # quiet highland moss blade
        if v < 0.08:
            return r_heath[2]  # soft shadow
        if v2 > 0.91 and (x & 1) == 0:
            return r_heath[3]  # sparse purple heather sprig
        return r_heath[5]      # quiet sage base

    # 7: S_FEN (moody olive-moss marsh)
    if s_id == 7:
        if v > 0.87:
            return r_fen[5]
        if v < 0.16:
            return r_fen[2]
        if v2 > 0.86:
            return r_fen[3]
        return r_fen[4]

    # 8: S_DUNES (quiet sunlit sand)
    if s_id == 8:
        if v > 0.90:
            return r_dunes[6]
        if v < 0.10:
            return r_dunes[4]
        return r_dunes[5]

    # 9: S_SNOW (crisp glacier-tinted snow)
    if s_id == 9:
        if v > 0.85:
            return r_frost[6]
        if v < 0.12:
            return r_frost[4]
        return r_frost[5]

    # 10: S_ICE (translucent glacier sheet with subtle diagonal glint)
    if s_id == 10:
        diag = (x - y + phase * 3) & 15
        if diag in (2, 3) and v > 0.35:
            return r_frost[5]
        if diag == 10 and v > 0.55:
            return r_frost[6]
        if v < 0.14:
            return r_frost[2]
        return r_frost[3]

    # 11: S_CRAG (slate mountain ridge top)
    if s_id == 11:
        if (y % 8 == 0 and v > 0.35) or v < 0.12:
            return SLATE_DARK
        if v > 0.84:
            return SLATE_LIGHT
        return SLATE_MID

    # 12: S_PATH (warm golden-ochre packed village road)
    if s_id == 12:
        if v > 0.88:
            return r_dunes[5]
        if v < 0.15:
            return r_meadow[6]  # OCHRE_MID
        return r_meadow[7]      # OCHRE_LIGHT

    # 13: S_SOIL (tilled dark earth furrows)
    if s_id == 13:
        furrow = y % 4
        if furrow == 0:
            return UMBER_DARK
        if furrow == 1:
            return OCHRE_MID
        return TIMBER_MID

    # 14: S_COBBLE (rounded village plaza stones)
    if s_id == 14:
        row = y // 4
        sx = (x + (row & 1) * 4) % 8
        sy = y % 4
        if sx == 0 or sy == 0:
            return SLATE_DARK
        if sx == 1 or sy == 1:
            return STONE_WARM if ((row + x // 8) & 1) else SLATE_LIGHT
        return SLATE_MID if ((row + x // 8) % 3 == 0) else STONE_WARM

    # 15: S_COURT (clean cut staggered 16x8 limestone flagstones)
    if s_id == 15:
        row = y // 8
        bx = (x + (row & 1) * 8) % 16
        by = y % 8
        if bx == 0 or by == 0:
            return SLATE_MID
        if bx == 1 or by == 1:
            return CREAM_SUN
        return STONE_WARM if ((row + (x + (row & 1) * 8) // 16) & 1) else SLATE_LIGHT

    # 16: S_LEAVES (fallen autumn leaf carpet)
    if s_id == 16:
        if v > 0.82:
            return r_amber[6]  # bright gold leaf
        if v < 0.18:
            return r_amber[2]  # russet leaf shadow
        if v2 > 0.70:
            return r_amber[4]
        return r_amber[5]

    # 17: S_RIPPLES (wind-carved desert sand waves)
    if s_id == 17:
        ridge = (y + int(round(math.sin(x * 0.39 + phase * 1.1) * 1.3))) % 6
        if ridge == 0:
            return r_dunes[6]
        if ridge == 1:
            return r_dunes[4]
        return r_dunes[5]

    # 18: S_MESA (terracotta desert plateau top)
    if s_id == 18:
        if v > 0.85:
            return r_dunes[4]
        if v < 0.15 or y % 8 == 0:
            return r_dunes[2]
        return r_dunes[3]

    # 19: S_FROST_TRACK (snow-dusted slate trail)
    if s_id == 19:
        if v > 0.82:
            return r_frost[5]
        if v < 0.18:
            return r_frost[2]
        return r_frost[3]

    # 20: S_SNOW_CRAG (snow-capped alpine plateau top)
    if v > 0.78:
        return r_frost[6]
    if v < 0.18:
        return r_frost[2]
    return r_frost[4]


def _edge_highlight(s_id: int, col: tuple[int, int, int]) -> tuple[int, int, int]:
    if s_id in (0, 1):
        return WATER_FOAM
    if s_id in (2, 8, 12, 17):
        return BIOME_RAMPS["dunes"][6]
    if s_id in (3, 4, 7):
        return LEAF_SUN if s_id == 3 else BIOME_RAMPS["forest"][5]
    if s_id in (5, 16):
        return GOLD_SUN
    if s_id == 6:
        return STONE_WARM
    if s_id in (9, 10, 19, 20):
        return BIOME_RAMPS["frost"][6]
    if s_id in (11, 14, 15):
        return CREAM_SUN
    if s_id == 18:
        return OCHRE_LIGHT
    return CREAM_SUN


def _edge_shadow(s_id: int, col: tuple[int, int, int]) -> tuple[int, int, int]:
    if s_id in (0, 1):
        return WATER_DEEP
    if s_id in (2, 8, 12, 17, 13):
        return UMBER_DARK
    if s_id in (3, 4):
        return LEAF_SHADOW
    if s_id in (5, 16):
        return BIOME_RAMPS["amber"][2]
    if s_id == 6:
        return BIOME_RAMPS["heath"][1]
    if s_id == 7:
        return BIOME_RAMPS["fen"][1]
    if s_id in (9, 10, 19, 20):
        return BIOME_RAMPS["frost"][2]
    if s_id == 18:
        return UMBER_DARK
    return SLATE_DARK


def _paint_relief_cell(im, ox: int, oy: int, row: int, mask: int) -> None:
    """Paint elevation lip / cliff face matching 16-mask marching squares."""
    inside = [[False] * 16 for _ in range(16)]
    for y in range(16):
        for x in range(16):
            inside[y][x] = _corner_inside(mask, x, y, organic=0, seed=row * 17)

    # Row 0: temperate shoreline undercut (drawn along dry boundary above water)
    # Row 1: frost bank undercut
    # Row 2: slate crag escarpment
    # Row 3: desert mesa terracotta escarpment
    # Row 4: snow crag escarpment
    if row in (0, 1):
        rim_col = OCHRE_MID if row == 0 else BIOME_RAMPS["frost"][3]
        sh_col = UMBER_DARK if row == 0 else BIOME_RAMPS["frost"][1]
        for y in range(15):
            for x in range(16):
                if inside[y][x] and not inside[y + 1][x]:
                    px(im, ox + x, oy + y, rim_col)
                    if y + 1 < 16:
                        px(im, ox + x, oy + y + 1, sh_col)
        return

    # Cliff rows 2, 3, 4: draw crisp 3D cliff face below exposed bottom edge of elevated plateau
    if row == 2:
        cap, mid, dk = SLATE_LIGHT, SLATE_MID, SLATE_DARK
    elif row == 3:
        cap, mid, dk = OCHRE_LIGHT, BIOME_RAMPS["dunes"][3], BIOME_RAMPS["dunes"][2]
    else:
        cap, mid, dk = BIOME_RAMPS["frost"][6], SLATE_MID, SLATE_DARK

    for x in range(16):
        for y in range(16):
            if not inside[y][x]:
                continue
            # Check south exposure
            south_open = (y == 15) or (not inside[y + 1][x])
            north_open = (y == 0) or (not inside[y - 1][x])
            if north_open:
                px(im, ox + x, oy + y, cap)
            if south_open:
                # 5px stepped south-facing escarpment downward where space permits (or lower rows)
                px(im, ox + x, oy + y, cap)
                for dy in range(1, 6):
                    yy = y + dy
                    if yy >= 16:
                        break
                    if dy <= 2:
                        c = mid if ((x + dy) % 4 != 0) else dk
                    elif dy <= 4:
                        c = dk if ((x ^ dy) & 1) else mid
                    else:
                        c = INK_DEEP
                    px(im, ox + x, oy + yy, c)


def generate_landscape() -> None:
    surfaces = 21
    im = canvas(16 * TILE, surfaces * 3 * TILE)
    for s in range(surfaces):
        for phase in range(3):
            oy = (s * 3 + phase) * TILE
            for mask in range(16):
                _paint_surface_cell(im, mask * TILE, oy, s, phase, mask)
    save_png(im, "tiles/landscape.png")

    rel = canvas(16 * TILE, 5 * TILE)
    for row in range(5):
        for mask in range(16):
            _paint_relief_cell(rel, mask * TILE, row * TILE, row, mask)
    save_png(rel, "tiles/relief.png")


if __name__ == "__main__":
    generate_landscape()
    print("Generated tiles/landscape.png and tiles/relief.png")
