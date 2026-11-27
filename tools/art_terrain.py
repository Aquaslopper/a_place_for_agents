#!/usr/bin/env python3
"""Generate 16x16 floor/overlay tiles, dungeon stairs, 48x48 cavern mouth,
and the canonical 32x32 navy-and-gold 9-slice UI window frame.
"""
from __future__ import annotations

from snes import (
    INK_DEEP, SLATE_DARK, SLATE_MID, SLATE_LIGHT, STONE_WARM, CREAM_SUN,
    UMBER_DARK, TIMBER_MID, OCHRE_MID, OCHRE_LIGHT, GOLD_SHADE, GOLD_SUN, SUN_WHITE,
    LEAF_SHADOW, LEAF_DARK, LEAF_MID, LEAF_LIGHT, LEAF_SUN,
    WATER_DEEP, WATER_MID, WATER_LIGHT, WATER_FOAM,
    CRIMSON_DARK, CRIMSON_MID, CORAL_LIGHT,
    NAVY_UI, INDIGO_ROYAL, LAVENDER, ROSE_CHEEK,
    BIOME_RAMPS, canvas, px, rect, hline, vline, line, ell, shaded_ell,
    sel_outline_in_place, stamp_palette_sprite, h2, save_png,
)


def make_bridge() -> None:
    im = canvas(16, 16)
    # 4 cedar planks with upper-left bevel and dark seam joints
    for py in range(16):
        plank_y = py % 4
        if plank_y == 0:
            hline(im, 0, 15, py, UMBER_DARK)
        elif plank_y == 1:
            hline(im, 0, 15, py, OCHRE_LIGHT)
        else:
            for x in range(16):
                c = OCHRE_MID if h2(x, py, 3) > 0.25 else TIMBER_MID
                px(im, x, py, c)
    # Iron nail heads on plank edges
    for y in (2, 6, 10, 14):
        px(im, 2, y, SLATE_DARK)
        px(im, 13, y, SLATE_DARK)
    # Side timber rails left/right
    vline(im, 0, 0, 15, UMBER_DARK)
    vline(im, 1, 0, 15, TIMBER_MID)
    vline(im, 14, 0, 15, TIMBER_MID)
    vline(im, 15, 0, 15, INK_DEEP)
    save_png(im, "tiles/bridge.png")


def make_turnips() -> None:
    im = canvas(16, 16)
    # Two lush garden turnip tops sprouting from tilled rows
    for cx, cy in ((5, 11), (11, 8)):
        # Root shoulder peeking above earth
        rect(im, cx - 2, cy, 4, 2, CREAM_SUN)
        px(im, cx - 2, cy + 1, STONE_WARM)
        px(im, cx + 1, cy + 1, LAVENDER)
        # Leaf rosette
        px(im, cx - 2, cy - 2, LEAF_LIGHT)
        px(im, cx - 1, cy - 3, LEAF_SUN)
        px(im, cx, cy - 2, LEAF_MID)
        px(im, cx + 1, cy - 3, LEAF_LIGHT)
        px(im, cx + 2, cy - 2, LEAF_DARK)
        px(im, cx, cy - 1, LEAF_DARK)
        px(im, cx - 1, cy - 1, LEAF_MID)
    sel_outline_in_place(im, LEAF_SHADOW, LEAF_DARK)
    save_png(im, "tiles/turnips.png")


def make_lilies() -> None:
    im = canvas(16, 16)
    # Emerald water-lily pads with V-notch and pale lotus blossom
    pad = [
        "..llll..",
        ".lLLLld.",
        "lLLLLldd",
        "lLLm..dd",
        "lLLmmddd",
        ".dmmddd.",
        "..dddd..",
    ]
    cmap = {"l": LEAF_LIGHT, "L": LEAF_MID, "m": LEAF_DARK, "d": LEAF_SHADOW}
    stamp_palette_sprite(im, pad, cmap, 2, 6)
    stamp_palette_sprite(im, pad[:5], cmap, 8, 2)
    # Cream & rose lotus blossom
    px(im, 10, 4, CREAM_SUN)
    px(im, 11, 4, ROSE_CHEEK)
    px(im, 10, 3, SUN_WHITE)
    px(im, 9, 4, CREAM_SUN)
    save_png(im, "tiles/lilies.png")


def make_tufts() -> None:
    patterns = [
        [
            "................",
            "................",
            "......h.........",
            "......h..s......",
            "....h.m..m......",
            "....m.m.dm..s...",
            "....mdm.dd..m...",
            ".....d...d..d...",
        ],
        [
            "................",
            "................",
            ".......s........",
            "....h..m..h.....",
            "....m.dm..m.....",
            "....ddmd..md....",
            ".....dd...d.....",
            "................",
        ],
        [
            "................",
            "................",
            ".........s......",
            ".....h...m..h...",
            ".....m..dm..m...",
            "...s.md.dd.md...",
            "...m.dd....d....",
            "................",
        ],
    ]
    cmap = {"s": LEAF_SUN, "h": LEAF_LIGHT, "m": LEAF_MID, "d": LEAF_DARK}
    for i, pat in enumerate(patterns):
        im = canvas(16, 16)
        stamp_palette_sprite(im, pat, cmap, 0, 8)
        save_png(im, f"tiles/tuft_{i}.png")


def make_flowers() -> None:
    f0 = [
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "....w.......y...",
        "...wYw.....yRy..",
        "....w..h....y...",
        "....m..m..h.m...",
        "....d.md..md....",
        ".....ddd..dd....",
        "................",
        "................",
        "................",
        "................",
    ]
    f1 = [
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "......r.........",
        ".....rYr....w...",
        "......r....wBw..",
        "...h..m..h..w...",
        "...m.dm..m..m...",
        "....ddd..ddd....",
        "................",
        "................",
        "................",
        "................",
    ]
    cmap = {
        "w": CREAM_SUN, "Y": GOLD_SUN, "y": GOLD_SUN, "R": CORAL_LIGHT,
        "r": CRIMSON_MID, "B": LAVENDER, "h": LEAF_LIGHT, "m": LEAF_MID, "d": LEAF_DARK,
    }
    for idx, pat in enumerate((f0, f1)):
        im = canvas(16, 16)
        stamp_palette_sprite(im, pat, cmap, 0, 0)
        save_png(im, f"tiles/flower_{idx}.png")


def make_pebble() -> None:
    im = canvas(16, 16)
    # Small river/desert pebble cluster, upper-left lit
    ell(im, 5.5, 11.5, 2.5, 1.8, SLATE_MID)
    px(im, 4, 10, STONE_WARM)
    px(im, 5, 10, CREAM_SUN)
    px(im, 6, 12, SLATE_DARK)
    ell(im, 11.0, 12.2, 2.0, 1.4, STONE_WARM)
    px(im, 10, 11, CREAM_SUN)
    px(im, 12, 13, SLATE_DARK)
    px(im, 8, 9, SLATE_LIGHT)
    px(im, 8, 10, SLATE_DARK)
    save_png(im, "tiles/pebble.png")


def make_brick_and_floors() -> None:
    # 1. Warm interior brick wall (16x16)
    br = canvas(16, 16, TIMBER_MID)
    for y in range(16):
        row = y // 4
        sy = y % 4
        for x in range(16):
            sx = (x + (row & 1) * 4) % 8
            if sy == 0 or sx == 0:
                px(br, x, y, UMBER_DARK)
            elif sy == 1 or sx == 1:
                px(br, x, y, OCHRE_MID if (row + x // 8) % 2 else CORAL_LIGHT)
            else:
                px(br, x, y, TIMBER_MID if h2(x, y, 7) > 0.25 else CRIMSON_DARK)
    save_png(br, "tiles/brick.png")

    # 2. Cool slate dungeon brick wall (16x16)
    bd = canvas(16, 16, SLATE_DARK)
    dung = BIOME_RAMPS["dungeon"]
    for y in range(16):
        row = y // 4
        sy = y % 4
        for x in range(16):
            sx = (x + (row & 1) * 4) % 8
            if sy == 0 or sx == 0:
                px(bd, x, y, INK_DEEP)
            elif sy == 1 or sx == 1:
                px(bd, x, y, dung[3])
            else:
                px(bd, x, y, dung[2] if h2(x, y, 13) > 0.22 else dung[1])
    save_png(bd, "tiles/brick_dark.png")

    # 3. Warm oak floorboards (16x16)
    pl = canvas(16, 16, TIMBER_MID)
    for y in range(16):
        plank = y // 4
        py = y % 4
        seam_x = (plank * 6 + 3) % 16
        for x in range(16):
            if py == 0 or x == seam_x:
                px(pl, x, y, UMBER_DARK)
            elif py == 1:
                px(pl, x, y, OCHRE_MID)
            else:
                px(pl, x, y, TIMBER_MID if h2(x, y, 21) > 0.18 else OCHRE_MID)
    save_png(pl, "tiles/plank_floor.png")

    # 4. Hall checkered cream-and-slate tile (16x16)
    tf = canvas(16, 16)
    for y in range(16):
        for x in range(16):
            tx, ty = x // 8, y // 8
            light = ((tx + ty) & 1) == 0
            lx, ly = x % 8, y % 8
            if lx == 0 or ly == 0:
                px(tf, x, y, SLATE_DARK)
            elif lx == 1 or ly == 1:
                px(tf, x, y, CREAM_SUN if light else SLATE_LIGHT)
            else:
                px(tf, x, y, STONE_WARM if light else SLATE_MID)
    save_png(tf, "tiles/tile_floor.png")

    # 5. Quiet dungeon stone flagstones (16x16)
    sf = canvas(16, 16)
    for y in range(16):
        for x in range(16):
            bx = (x + ((y // 8) & 1) * 8) % 8
            by = y % 8
            if bx == 0 or by == 0:
                px(sf, x, y, INK_DEEP)
            elif bx == 1 and by == 1:
                px(sf, x, y, dung[3])
            else:
                px(sf, x, y, dung[2] if h2(x, y, 44) > 0.2 else dung[1])
    save_png(sf, "tiles/stone_floor.png")

    # 6. Royal crimson-and-gold woven carpet (16x16)
    cp = canvas(16, 16, CRIMSON_MID)
    for y in range(16):
        for x in range(16):
            if x in (0, 15):
                px(cp, x, y, INK_DEEP)
            elif x in (1, 14):
                px(cp, x, y, GOLD_SUN if (y & 1) == 0 else GOLD_SHADE)
            elif x in (2, 13):
                px(cp, x, y, CRIMSON_DARK)
            elif (x in (5, 10) and y in (3, 11)) or (x in (7, 8) and y in (7, 8)):
                px(cp, x, y, GOLD_SHADE)
            elif (x + y) % 4 == 0:
                px(cp, x, y, CORAL_LIGHT)
    save_png(cp, "tiles/carpet.png")


def make_stairs_and_cave() -> None:
    # Stair down (16x16)
    sd = canvas(16, 16, INK_DEEP)
    rect(sd, 0, 0, 16, 16, SLATE_DARK)
    rect(sd, 1, 1, 14, 14, SLATE_MID)
    for step in range(5):
        y = 3 + step * 2
        hline(sd, 2, 13, y, STONE_WARM if step < 2 else (SLATE_LIGHT if step < 4 else SLATE_DARK))
        hline(sd, 2, 13, y + 1, SLATE_DARK if step < 3 else INK_DEEP)
    # Stone stair cheek walls
    vline(sd, 1, 1, 14, SLATE_LIGHT)
    vline(sd, 14, 1, 14, INK_DEEP)
    hline(sd, 1, 14, 1, CREAM_SUN)
    rect(sd, 3, 12, 10, 3, INK_DEEP)
    save_png(sd, "tiles/../sprites/stair_down.png")

    # Stair up (16x16)
    su = canvas(16, 16, SLATE_DARK)
    rect(su, 1, 1, 14, 14, SLATE_MID)
    for step in range(5):
        y = 12 - step * 2
        col = CREAM_SUN if step >= 3 else (STONE_WARM if step >= 1 else SLATE_LIGHT)
        hline(su, 2, 13, y, col)
        hline(su, 2, 13, y + 1, SLATE_DARK)
    hline(su, 2, 13, 2, SUN_WHITE)
    hline(su, 2, 13, 3, GOLD_SUN)
    vline(su, 1, 1, 14, CREAM_SUN)
    vline(su, 14, 1, 14, INK_DEEP)
    save_png(su, "sprites/stair_up.png")

    # Cave mouth (48x48) — grassy mounded stone arch with timber bracing & deep cavern shadow
    cm = canvas(48, 48)
    # Outer grassy stone ridge mound
    shaded_ell(cm, 24, 29, 22, 17, [INK_DEEP, SLATE_DARK, SLATE_MID, SLATE_LIGHT, STONE_WARM], seed=9)
    # Turf cap draping over upper arch
    for y in range(10, 26):
        for x in range(4, 44):
            dx = (x - 24) / 20.0
            dy = (y - 17) / 8.0
            if dx * dx + dy * dy <= 1.0 and y < 22 + int(h2(x, y, 5) * 4):
                c = LEAF_SUN if y < 13 else (LEAF_LIGHT if y < 17 else (LEAF_MID if y < 21 else LEAF_DARK))
                px(cm, x, y, c)
    # Stone archway blocks
    for y in range(20, 47):
        for x in range(9, 39):
            dist = ((x - 24) / 14.0) ** 2 + ((y - 38) / 17.0) ** 2
            if dist <= 1.0:
                if dist > 0.62:
                    # Arch stones
                    c = STONE_WARM if (x < 24 and y < 34) else (SLATE_LIGHT if x < 28 else SLATE_MID)
                    if ((x + y) % 6 == 0):
                        c = SLATE_DARK
                    px(cm, x, y, c)
                else:
                    # Deep dark cavern mouth
                    px(cm, x, y, INK_DEEP if dist < 0.45 else SLATE_DARK)
    # Timber pit props & crossbeam
    rect(cm, 14, 27, 2, 19, TIMBER_MID)
    vline(cm, 14, 27, 45, OCHRE_MID)
    rect(cm, 32, 27, 2, 19, UMBER_DARK)
    vline(cm, 32, 27, 45, TIMBER_MID)
    hline(cm, 14, 33, 27, OCHRE_LIGHT)
    hline(cm, 14, 33, 28, TIMBER_MID)
    # Glowing lantern hanging on left timber post
    px(cm, 16, 31, SLATE_DARK)
    px(cm, 16, 32, GOLD_SUN)
    px(cm, 16, 33, SUN_WHITE)
    sel_outline_in_place(cm, INK_DEEP, LEAF_SHADOW)
    save_png(cm, "sprites/cave_mouth.png")


def make_window_frame() -> None:
    """Canonical 32x32 navy-and-gold 9-slice UI frame (8px corners, 16px center)."""
    im = canvas(32, 32, NAVY_UI)
    # Subtle stepped upper navy sheen inside frame (strictly CORE_PALETTE)
    for y in range(4, 9):
        for x in range(4, 28):
            if y < 6 or (y == 6 and ((x ^ y) & 1) == 0):
                px(im, x, y, INDIGO_ROYAL)
    # Outer dark border
    hline(im, 0, 31, 0, INK_DEEP)
    hline(im, 0, 31, 31, INK_DEEP)
    vline(im, 0, 0, 31, INK_DEEP)
    vline(im, 31, 0, 31, INK_DEEP)
    # Main sunlit gold bevel (top/left bright, bottom/right antique shade)
    hline(im, 1, 30, 1, GOLD_SUN)
    vline(im, 1, 1, 30, GOLD_SUN)
    hline(im, 1, 30, 30, GOLD_SHADE)
    vline(im, 30, 1, 30, GOLD_SHADE)
    # Second bevel line (warm gold -> shaded gold)
    hline(im, 2, 29, 2, GOLD_SHADE)
    vline(im, 2, 2, 29, GOLD_SHADE)
    hline(im, 2, 29, 29, UMBER_DARK)
    vline(im, 29, 2, 29, UMBER_DARK)
    # Inner dark recess rim
    hline(im, 3, 28, 3, INK_DEEP)
    vline(im, 3, 3, 28, INK_DEEP)
    hline(im, 3, 28, 28, INK_DEEP)
    vline(im, 28, 3, 28, INK_DEEP)
    # Ornate corner rivets (fitting cleanly inside 8x8 corners)
    for cx, cy in ((2, 2), (28, 2), (2, 28), (28, 28)):
        px(im, cx, cy, SUN_WHITE)
        px(im, cx + 1, cy, GOLD_SUN)
        px(im, cx, cy + 1, GOLD_SUN)
        px(im, cx + 1, cy + 1, GOLD_SHADE)
    px(im, 1, 1, SUN_WHITE)
    save_png(im, "ui/window_frame.png")


def generate_terrain_and_ui() -> None:
    make_bridge()
    make_turnips()
    make_lilies()
    make_tufts()
    make_flowers()
    make_pebble()
    make_brick_and_floors()
    make_stairs_and_cave()
    make_window_frame()


if __name__ == "__main__":
    generate_terrain_and_ui()
    print("Generated tiles/* and ui/window_frame.png")
