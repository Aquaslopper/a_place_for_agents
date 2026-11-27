#!/usr/bin/env python3
"""Generate pure crisp 16-bit pixel-art battle backdrops (512x192) and
512x288 title / dawn / game-over scenic plates strictly from the Valley Hearth
Core Palette & Biome Ramps (zero bicubic scaling, zero alpha haze blends).
"""
from __future__ import annotations

import math
from snes import (
    INK_DEEP, SLATE_DARK, SLATE_MID, SLATE_LIGHT, STONE_WARM, CREAM_SUN,
    UMBER_DARK, TIMBER_MID, OCHRE_MID, OCHRE_LIGHT, GOLD_SHADE, GOLD_SUN, SUN_WHITE,
    LEAF_SHADOW, LEAF_DARK, LEAF_MID, LEAF_LIGHT, LEAF_SUN,
    WATER_DEEP, WATER_MID, WATER_LIGHT, WATER_FOAM,
    CRIMSON_DARK, CRIMSON_MID, CORAL_LIGHT,
    NAVY_UI, INDIGO_ROYAL, LAVENDER,
    BIOME_RAMPS, canvas, px, rect, hline, vline, line, ell, shaded_ell,
    h2, save_png,
)

BAYER2 = ((0, 2), (3, 1))


def _dither_sky(im, y0: int, y1: int, bands: list[tuple[int, int, int]]) -> None:
    n = len(bands)
    span = max(1, y1 - y0)
    for y in range(y0, y1 + 1):
        t = (y - y0) / span * (n - 1)
        idx = min(n - 2, int(t))
        frac = t - idx
        c0, c1 = bands[idx], bands[idx + 1]
        for x in range(im.width):
            thresh = (BAYER2[y & 1][x & 1] + 0.5) / 4.0
            px(im, x, y, c1 if frac > thresh else c0)


def _pixel_clouds(im, y_base: int, seed: int, lit=CREAM_SUN, mid=WATER_FOAM, sh=SLATE_LIGHT) -> None:
    for i in range(5):
        cx = int(48 + i * 104 + h2(i, 1, seed) * 26)
        cy = y_base + int(h2(i, 2, seed) * 14)
        w = 30 + int(h2(i, 3, seed) * 22)
        for dx in range(-w, w + 1):
            curve = math.cos(dx / max(1, w) * 1.48)
            hh = max(1, int(curve * (8 + h2(dx >> 2, i, seed) * 5)))
            for dy in range(-hh, 3):
                c = lit if dy < -hh // 2 else (mid if dy < 1 else sh)
                px(im, cx + dx, cy + dy, c)


def _paint_peak(
    im, peak_x: int, peak_y: int, base_y: int, half_w: int,
    col_lit, col_sh, snow_cap: bool = False, seed: int = 1
) -> None:
    """Paint one mountain peak with a realistic diagonal ridge line (no vertical seam)."""
    span_y = max(1, base_y - peak_y)
    for y in range(max(0, peak_y), min(im.height, base_y + 12)):
        t = (y - peak_y) / span_y
        # Left and right slopes curve gently concave with subtle rock crags
        jitter_l = int((h2(y >> 1, 1, seed) - 0.5) * 4)
        jitter_r = int((h2(y >> 1, 2, seed) - 0.5) * 4)
        lx = int(peak_x - (t ** 0.92) * half_w) + jitter_l
        rx = int(peak_x + (t ** 0.92) * half_w * 1.08) + jitter_r
        # Ridge spur weaves slightly right as it descends -> organic faceted mountain!
        ridge_x = int(peak_x + math.sin(t * 2.8 + seed) * (half_w * 0.14) + t * (half_w * 0.10))
        snow_limit = t < (0.30 + math.sin((peak_x + y) * 0.25) * 0.04)
        for x in range(max(0, lx), min(im.width, rx + 1)):
            lit = x < ridge_x
            if snow_cap and snow_limit:
                c = SUN_WHITE if (lit and t < 0.18) else (CREAM_SUN if lit else WATER_FOAM)
            else:
                c = col_lit if lit else col_sh
            px(im, x, y, c)


def _ridge_range(
    im, base_y: int, peaks: list[tuple[int, int, int]],
    col_lit, col_sh, snow_cap: bool = False, seed: int = 1
) -> None:
    for idx, (px_c, py_c, hw) in enumerate(peaks):
        _paint_peak(im, px_c, py_c, base_y, hw, col_lit, col_sh, snow_cap=snow_cap, seed=seed + idx * 17)


def _tree_horizon(im, y_base: int, seed: int, ramp: list[tuple[int, int, int]], conifer: bool = False) -> None:
    # Solid distant treeline backing band so no gaps peek between silhouettes
    rect(im, 0, y_base - 8, im.width, 12, ramp[0])
    x = -4
    while x < im.width + 6:
        step = 10 + int(h2(x, 4, seed) * 9)
        hh = 14 + int(h2(x, 7, seed) * 14)
        cx = x + step // 2
        if conifer:
            for dy in range(hh):
                tier = 0.72 if (dy % 5 == 4) else 1.0
                hw = max(1, int((dy / max(1, hh)) * 7 * tier))
                y = y_base - hh + dy
                for tx in range(cx - hw, cx + hw + 1):
                    c = ramp[3] if tx < cx else (ramp[2] if tx == cx else ramp[1])
                    if dy % 5 == 4:
                        c = ramp[0]
                    px(im, tx, y, c)
        else:
            ell(im, cx, y_base - hh // 2, step * 0.65, hh * 0.56, ramp[1])
            ell(im, cx - 2, y_base - hh // 2 - 2, step * 0.45, hh * 0.42, ramp[2])
            ell(im, cx - 3, y_base - hh // 2 - 3, step * 0.25, hh * 0.26, ramp[3])
        x += step


def _ground_stage(
    im, y_ground: int, ramp: list[tuple[int, int, int]],
    road_y: int | None = None, water_y: int | None = None, seed: int = 1
) -> None:
    """Calm SNES perspective floor: smooth shaded strata + small clustered 3px tufts (zero speckling)."""
    for y in range(y_ground, im.height):
        depth = (y - y_ground) / max(1, im.height - y_ground)
        # Subtle undulating strata band color
        band = int(depth * 3.0)
        for x in range(im.width):
            if water_y is not None and water_y <= y <= water_y + 18:
                wy = y - water_y
                if wy == 0:
                    c = WATER_FOAM
                elif wy < 6:
                    c = WATER_LIGHT
                elif wy == 9 and ((x // 5 + y) % 4 == 0):
                    c = WATER_FOAM
                else:
                    c = WATER_MID
            elif road_y is not None and abs(y - road_y) <= 9 + int(math.sin(x * 0.025) * 3):
                dy = y - road_y
                if abs(dy) >= 8:
                    c = OCHRE_MID
                else:
                    c = OCHRE_LIGHT if (dy < 3 or h2(x >> 2, y >> 1, seed + 5) > 0.35) else OCHRE_MID
            else:
                wave = math.sin(x * 0.025 + depth * 2.0) * 0.08
                c = ramp[2] if (depth + wave < 0.18) else (ramp[3] if band < 2 else ramp[min(4, len(ramp) - 1)])
            px(im, x, y, c)

    # Clean clustered 3px vegetation blades / pebbles (no confetti!)
    for i in range(28):
        tx = int(h2(i, 1, seed + 99) * (im.width - 14)) + 7
        ty = y_ground + 7 + int(h2(i, 2, seed + 99) * max(10, im.height - y_ground - 13))
        if road_y and abs(ty - road_y) <= 12:
            continue
        if water_y and water_y - 2 <= ty <= water_y + 20:
            continue
        px(im, tx - 1, ty, ramp[1])
        px(im, tx, ty, ramp[1])
        px(im, tx + 1, ty, ramp[1])
        px(im, tx - 1, ty - 1, ramp[min(4, len(ramp) - 1)])
        px(im, tx + 1, ty - 1, ramp[3])


def _draw_mini_cottage(im, bx: int, by: int, roof_col=SLATE_MID, roof_hi=SLATE_LIGHT, chimney: bool = True) -> None:
    rect(im, bx, by - 14, 26, 14, CREAM_SUN)
    rect(im, bx + 17, by - 14, 9, 14, STONE_WARM)
    hline(im, bx, bx + 25, by - 1, UMBER_DARK)
    if chimney:
        rect(im, bx + 18, by - 25, 4, 7, SLATE_DARK)
        px(im, bx + 19, by - 27, CREAM_SUN)
        px(im, bx + 21, by - 29, STONE_WARM)
    for dy in range(11):
        hw = 5 + dy
        for x in range(bx + 13 - hw, bx + 13 + hw + 1):
            px(im, x, by - 25 + dy, roof_hi if x < bx + 13 else roof_col)
    hline(im, bx + 1, bx + 24, by - 15, INK_DEEP)
    rect(im, bx + 4, by - 10, 4, 4, GOLD_SUN)
    rect(im, bx + 18, by - 10, 4, 4, GOLD_SUN)
    rect(im, bx + 11, by - 8, 4, 8, TIMBER_MID)


def _draw_battle_castle(im) -> None:
    _dither_sky(im, 0, 96, [NAVY_UI, INDIGO_ROYAL, SLATE_MID, LAVENDER, CORAL_LIGHT])
    _pixel_clouds(im, 16, seed=77, lit=CREAM_SUN, mid=LAVENDER, sh=SLATE_MID)
    _ridge_range(im, 108, [(70, 56, 75), (440, 62, 80)], SLATE_MID, SLATE_DARK, seed=8)
    # Castle wall & central keep
    rect(im, 128, 58, 256, 54, SLATE_MID)
    rect(im, 208, 28, 96, 84, STONE_WARM)
    rect(im, 268, 28, 36, 84, SLATE_LIGHT)
    # Flanking turrets (exact symmetric bounds)
    for tx in (128, 176, 312, 360):
        rect(im, tx, 42, 24, 70, SLATE_LIGHT if tx < 256 else SLATE_MID)
        for dy in range(14):
            hw = max(1, dy)
            hline(im, tx + 12 - hw, tx + 11 + hw, 28 + dy, INDIGO_ROYAL if dy < 7 else NAVY_UI)
    # Crenellations cleanly aligned along wall tops (skipping taller central keep)
    for cx in range(128, 384, 8):
        if cx % 16 == 0 and (cx < 208 or cx >= 304):
            rect(im, cx, 53, 5, 5, STONE_WARM if cx < 256 else SLATE_MID)
    for cx in range(208, 304, 8):
        if cx % 16 == 0:
            rect(im, cx, 23, 5, 5, CREAM_SUN if cx < 268 else STONE_WARM)
    # Lit stained-glass keep windows & banners
    for wx in (224, 280):
        rect(im, wx, 42, 8, 14, INDIGO_ROYAL)
        rect(im, wx + 2, 44, 4, 10, GOLD_SUN)
    for bx in (156, 348):
        rect(im, bx, 64, 8, 20, CRIMSON_MID)
        hline(im, bx, bx + 7, 64, GOLD_SUN)
    # Grand portcullis gate
    rect(im, 240, 74, 32, 38, INK_DEEP)
    for gx in range(244, 270, 6):
        vline(im, gx, 76, 111, SLATE_DARK)
    for gy in range(82, 108, 8):
        hline(im, 240, 271, gy, SLATE_DARK)
    # Calm stone courtyard stage (broad perspective slabs, no noisy grid)
    for y in range(112, 192):
        slab_h = 8 if y < 144 else 12
        row = (y - 112) // slab_h
        is_seam_y = ((y - 112) % slab_h == 0)
        for x in range(512):
            sx = (x + (row & 1) * 16) % 32
            if is_seam_y:
                c = SLATE_DARK
            elif sx == 0:
                c = SLATE_DARK
            elif (y - 112) % slab_h == 1:
                c = STONE_WARM
            else:
                c = SLATE_MID if y < 152 else SLATE_LIGHT
            px(im, x, y, c)


def generate_battle_backdrops() -> None:
    # 1. Meadow (512x192)
    im = canvas(512, 192)
    _dither_sky(im, 0, 96, [WATER_MID, WATER_LIGHT, WATER_FOAM, CREAM_SUN])
    _pixel_clouds(im, 16, seed=1)
    _ridge_range(im, 106, [(145, 36, 110), (265, 50, 95), (385, 42, 105)],
                 SLATE_LIGHT, SLATE_MID, snow_cap=True, seed=3)
    _tree_horizon(im, 108, seed=5, ramp=[LEAF_SHADOW, LEAF_DARK, LEAF_MID, LEAF_LIGHT], conifer=False)
    _ground_stage(im, 108, [INK_DEEP, LEAF_SHADOW, LEAF_DARK, LEAF_MID, LEAF_LIGHT], road_y=148, seed=11)
    save_png(im, "backdrops/bg_meadow.png")

    # 2. Forest (512x192)
    im = canvas(512, 192)
    _dither_sky(im, 0, 88, [LEAF_SHADOW, LEAF_DARK, WATER_MID, WATER_FOAM])
    _ridge_range(im, 102, [(120, 56, 110), (340, 52, 120)], LEAF_DARK, LEAF_SHADOW, seed=9)
    _tree_horizon(im, 110, seed=12, ramp=BIOME_RAMPS["forest"][1:5], conifer=True)
    _ground_stage(im, 108, BIOME_RAMPS["forest"][1:6], road_y=152, seed=14)
    save_png(im, "backdrops/bg_forest.png")

    # 3. Amber (512x192)
    im = canvas(512, 192)
    _dither_sky(im, 0, 96, [INDIGO_ROYAL, CORAL_LIGHT, GOLD_SUN, SUN_WHITE])
    _pixel_clouds(im, 18, seed=21, lit=SUN_WHITE, mid=GOLD_SUN, sh=CORAL_LIGHT)
    _ridge_range(im, 104, [(160, 50, 110), (370, 44, 120)], CRIMSON_MID, CRIMSON_DARK, seed=15)
    _tree_horizon(im, 110, seed=23, ramp=BIOME_RAMPS["amber"][2:6], conifer=False)
    _ground_stage(im, 108, BIOME_RAMPS["amber"][1:6], road_y=148, seed=27)
    save_png(im, "backdrops/bg_amber.png")

    # 4. Fen (512x192)
    im = canvas(512, 192)
    _dither_sky(im, 0, 94, [SLATE_DARK, SLATE_MID, WATER_LIGHT, STONE_WARM])
    _pixel_clouds(im, 22, seed=31, lit=STONE_WARM, mid=SLATE_LIGHT, sh=SLATE_MID)
    _ridge_range(im, 104, [(180, 72, 120), (390, 70, 110)], BIOME_RAMPS["fen"][3], BIOME_RAMPS["fen"][1], seed=33)
    _tree_horizon(im, 112, seed=35, ramp=BIOME_RAMPS["fen"][1:5], conifer=False)
    _ground_stage(im, 110, BIOME_RAMPS["fen"][1:6], water_y=144, seed=39)
    save_png(im, "backdrops/bg_fen.png")

    # 5. Heath (512x192)
    im = canvas(512, 192)
    _dither_sky(im, 0, 96, [NAVY_UI, SLATE_MID, LAVENDER, CREAM_SUN])
    _pixel_clouds(im, 20, seed=41, lit=CREAM_SUN, mid=STONE_WARM, sh=SLATE_LIGHT)
    _ridge_range(im, 106, [(110, 48, 105), (275, 42, 115), (425, 52, 95)],
                 BIOME_RAMPS["heath"][3], BIOME_RAMPS["heath"][1], seed=43)
    _tree_horizon(im, 110, seed=45, ramp=[BIOME_RAMPS["heath"][1], BIOME_RAMPS["heath"][2], BIOME_RAMPS["heath"][5], BIOME_RAMPS["heath"][6]], conifer=True)
    _ground_stage(im, 108, [INK_DEEP, BIOME_RAMPS["heath"][1], BIOME_RAMPS["heath"][2], BIOME_RAMPS["heath"][5], BIOME_RAMPS["heath"][6]], road_y=150, seed=47)
    save_png(im, "backdrops/bg_heath.png")

    # 6. Frost (512x192)
    im = canvas(512, 192)
    _dither_sky(im, 0, 96, [INDIGO_ROYAL, WATER_MID, WATER_FOAM, BIOME_RAMPS["frost"][6]])
    _pixel_clouds(im, 18, seed=51, lit=BIOME_RAMPS["frost"][6], mid=BIOME_RAMPS["frost"][5], sh=BIOME_RAMPS["frost"][4])
    _ridge_range(im, 106, [(130, 34, 110), (260, 44, 95), (395, 32, 115)],
                 BIOME_RAMPS["frost"][4], BIOME_RAMPS["frost"][2], snow_cap=True, seed=53)
    _tree_horizon(im, 110, seed=55, ramp=[LEAF_SHADOW, LEAF_DARK, BIOME_RAMPS["frost"][4], BIOME_RAMPS["frost"][6]], conifer=True)
    _ground_stage(im, 108, BIOME_RAMPS["frost"][2:7], seed=59)
    save_png(im, "backdrops/bg_frost.png")

    # 7. Dunes (512x192)
    im = canvas(512, 192)
    _dither_sky(im, 0, 96, [WATER_MID, OCHRE_LIGHT, GOLD_SUN, SUN_WHITE])
    _pixel_clouds(im, 16, seed=61, lit=SUN_WHITE, mid=CREAM_SUN, sh=OCHRE_LIGHT)
    # Flat-topped mesas & pyramids on desert horizon
    _ridge_range(im, 106, [(115, 58, 90), (290, 62, 100), (430, 56, 85)],
                 OCHRE_LIGHT, BIOME_RAMPS["dunes"][2], seed=63)
    _ground_stage(im, 104, BIOME_RAMPS["dunes"][1:7], seed=67)
    save_png(im, "backdrops/bg_dunes.png")

    # 8. Castle (512x192)
    im = canvas(512, 192)
    _draw_battle_castle(im)
    save_png(im, "backdrops/bg_castle.png")


def generate_scenic_plates() -> None:
    # 1. Title Vista (512x288) — Valley Hearth golden alpine panorama
    tb = canvas(512, 288)
    _dither_sky(tb, 0, 168, [NAVY_UI, INDIGO_ROYAL, SLATE_MID, CORAL_LIGHT, GOLD_SUN, SUN_WHITE])
    for i in range(18):
        px(tb, int(h2(i, 1, 7) * 500) + 6, int(h2(i, 2, 7) * 56) + 4, SUN_WHITE if i % 3 else CREAM_SUN)
    _pixel_clouds(tb, 42, seed=88, lit=SUN_WHITE, mid=GOLD_SUN, sh=CORAL_LIGHT)

    # Grand alpine valley silhouette (left tall peak, flanking ridges)
    _ridge_range(tb, 174, [(112, 54, 130), (275, 92, 105), (425, 68, 125)],
                 STONE_WARM, SLATE_MID, snow_cap=True, seed=12)
    _ridge_range(tb, 180, [(55, 112, 95), (200, 118, 95), (355, 108, 110), (470, 114, 90)],
                 SLATE_MID, SLATE_DARK, seed=29)

    _tree_horizon(tb, 184, seed=41, ramp=[LEAF_SHADOW, LEAF_DARK, LEAF_MID, LEAF_LIGHT], conifer=True)
    _tree_horizon(tb, 192, seed=44, ramp=[LEAF_SHADOW, LEAF_DARK, LEAF_MID, LEAF_SUN], conifer=False)

    _ground_stage(tb, 186, [INK_DEEP, LEAF_SHADOW, LEAF_DARK, LEAF_MID, LEAF_LIGHT], road_y=232, seed=91)
    # Winding valley river
    river_cx = {}
    for y in range(192, 288):
        cx = int(264 + math.sin((y - 190) * 0.055) * 76 - (y - 192) * 0.55)
        river_cx[y] = cx
        hw = int(5 + (y - 192) * 0.24)
        for x in range(max(0, cx - hw), min(512, cx + hw)):
            edge = abs(x - cx) >= hw - 2
            c = OCHRE_MID if edge else (WATER_FOAM if (x + y) % 13 == 0 else (WATER_LIGHT if x < cx else WATER_MID))
            px(tb, x, y, c)
    # Arched stone bridge centered RIGHT over the river-road crossing (y=222..238)!
    bx_mid = river_cx[230]
    rect(tb, bx_mid - 26, 219, 52, 14, STONE_WARM)
    hline(tb, bx_mid - 26, bx_mid + 25, 219, CREAM_SUN)
    hline(tb, bx_mid - 26, bx_mid + 25, 222, SLATE_DARK)
    ell(tb, bx_mid, 232, 11, 7, WATER_MID)
    hline(tb, bx_mid - 24, bx_mid + 23, 225, OCHRE_LIGHT)

    # Cozy Elmswick cottages nestled by the treeline
    _draw_mini_cottage(tb, 62, 210, SLATE_MID, SLATE_LIGHT)
    _draw_mini_cottage(tb, 100, 218, CRIMSON_MID, CORAL_LIGHT)
    _draw_mini_cottage(tb, 386, 212, SLATE_MID, WATER_FOAM)
    _draw_mini_cottage(tb, 426, 220, LEAF_DARK, LEAF_MID)
    save_png(tb, "title_bg.png")

    # 2. Dawn Intro Plate (512x288)
    dw = canvas(512, 288)
    _dither_sky(dw, 0, 178, [INDIGO_ROYAL, LAVENDER, CORAL_LIGHT, GOLD_SUN, SUN_WHITE])
    shaded_ell(dw, 152, 150, 28, 28, [CORAL_LIGHT, GOLD_SUN, SUN_WHITE])
    _ridge_range(dw, 180, [(85, 112, 110), (260, 126, 120), (430, 108, 120)],
                 SLATE_MID, NAVY_UI, seed=7)
    _tree_horizon(dw, 188, seed=13, ramp=[LEAF_SHADOW, LEAF_DARK, LEAF_MID, GOLD_SUN], conifer=True)
    _ground_stage(dw, 184, [INK_DEEP, LEAF_SHADOW, LEAF_DARK, LEAF_MID, LEAF_LIGHT], road_y=236, seed=33)
    save_png(dw, "ui/dawn.png")

    # 3. Game Over Plate (512x288) — cracked memorial obelisk, planted knight sword & crest shield
    go = canvas(512, 288)
    _dither_sky(go, 0, 178, [INK_DEEP, NAVY_UI, SLATE_DARK, CRIMSON_DARK, SLATE_MID])
    for i in range(28):
        px(go, int(h2(i, 3, 22) * 500) + 6, int(h2(i, 4, 22) * 95) + 6, STONE_WARM)
    _ridge_range(go, 184, [(95, 124, 110), (390, 118, 130)], SLATE_MID, INK_DEEP, seed=55)
    _ground_stage(go, 182, [INK_DEEP, SLATE_DARK, LEAF_SHADOW, LEAF_DARK, SLATE_MID], seed=71)
    # Cracked memorial stone monument
    rect(go, 240, 138, 34, 68, SLATE_MID)
    rect(go, 236, 198, 42, 10, SLATE_DARK)
    vline(go, 240, 138, 197, STONE_WARM)
    hline(go, 240, 273, 138, CREAM_SUN)
    # Tapered cap & carved rune
    hline(go, 243, 270, 136, STONE_WARM)
    hline(go, 247, 266, 134, CREAM_SUN)
    rect(go, 253, 150, 8, 16, SLATE_DARK)
    # Planted knight broadsword & tilted shield leaning beside monument
    rect(go, 224, 148, 4, 58, CREAM_SUN)
    vline(go, 226, 148, 205, SLATE_LIGHT)
    hline(go, 216, 235, 160, GOLD_SHADE)
    hline(go, 217, 234, 159, GOLD_SUN)
    rect(go, 224, 145, 4, 4, GOLD_SUN)
    shaded_ell(go, 284, 192, 10, 13, [NAVY_UI, INDIGO_ROYAL, SLATE_LIGHT])
    hline(go, 277, 291, 188, GOLD_SUN)
    # Small glowing watchfire ember beside the stone
    ell(go, 210, 204, 6, 3, TIMBER_MID)
    ell(go, 210, 201, 4, 4, CORAL_LIGHT)
    px(go, 210, 200, GOLD_SUN)
    save_png(go, "ui/gameover_plate.png")


def generate_all_scenery() -> None:
    generate_battle_backdrops()
    generate_scenic_plates()


if __name__ == "__main__":
    generate_all_scenery()
    print("Generated 8 512x192 battle backdrops and 3 512x288 scenic plates")
