#!/usr/bin/env python3
"""Generate all biome flora, boulders, village/dungeon props, furniture, and clouds.

Every sprite is an exact 16px grid multiple, adheres to strict Core Palette and
<=8-color biome ramps, uses upper-left sunlight, and carries a 1px selective outline.
"""
from __future__ import annotations

import math
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


def _broadleaf(
    im,
    trunk_x: int,
    base_y: int,
    clusters: list[tuple[float, float, float, float]],
    ramp: list[tuple[int, int, int]],
    seed: int = 0,
    droop: bool = False,
) -> None:
    """Tiered broadleaf canopy with shadowed trunk and sunlit UL leaf lobes."""
    # Flared roots & shaded trunk
    rect(im, trunk_x - 3, base_y - 12, 6, 11, TIMBER_MID)
    vline(im, trunk_x - 3, base_y - 11, base_y - 2, OCHRE_MID)
    vline(im, trunk_x + 2, base_y - 12, base_y - 1, UMBER_DARK)
    hline(im, trunk_x - 4, trunk_x + 3, base_y - 1, UMBER_DARK)
    # Under-canopy trunk cast shadow
    rect(im, trunk_x - 3, base_y - 12, 6, 3, UMBER_DARK)

    # Paint leaf clusters back-to-front (lower/right shadow lobes first, UL sunlit top last)
    for cx, cy, rx, ry in clusters:
        shaded_ell(im, cx, cy, rx, ry, ramp, seed=seed, dither=False)
        # Crisp leaf-clump highlights along UL arc
        for _ in range(int(rx * 1.4)):
            lx = int(round(cx - rx * 0.45 + (h2(int(cx), _, seed + 3) - 0.5) * rx))
            ly = int(round(cy - ry * 0.45 + (h2(_, int(cy), seed + 9) - 0.5) * ry * 0.7))
            if 0 <= lx < im.width and 0 <= ly < im.height and im.getpixel((lx, ly))[3] > 0:
                px(im, lx, ly, ramp[-1])

    if droop:
        # Weeping willow hanging fronds along lower rim
        for fx in range(4, im.width - 4, 3):
            col_top = None
            for yy in range(im.height - 6):
                if im.getpixel((fx, yy))[3] > 0:
                    col_top = yy
            if col_top and col_top < base_y - 4:
                length = 4 + (fx % 5)
                for dy in range(1, length):
                    if col_top + dy < base_y - 1:
                        px(im, fx, col_top + dy, ramp[2] if dy < length - 1 else ramp[1])

    sel_outline_in_place(im, INK_DEEP, ramp[1])


def _conifer(im, cx: int, base_y: int, width_scale: float, ramp: list[tuple[int, int, int]],
             snow: bool = False) -> None:
    """Tiered evergreen spruce/pine with upper-left highlights and dark needle tiers."""
    rect(im, cx - 2, base_y - 6, 4, 5, TIMBER_MID)
    vline(im, cx - 2, base_y - 5, base_y - 1, OCHRE_MID)
    vline(im, cx + 1, base_y - 5, base_y - 1, UMBER_DARK)

    tiers = [
        (base_y - 6, 13 * width_scale, 9),
        (base_y - 14, 11 * width_scale, 9),
        (base_y - 22, 9 * width_scale, 9),
        (base_y - 30, 6.5 * width_scale, 9),
        (base_y - 37, 4 * width_scale, 8),
    ]
    dk, mid, lit, hi = ramp[1], ramp[2], ramp[3], ramp[4]
    for tier_idx, (ty_bot, half_w, th) in enumerate(tiers):
        for dy in range(th):
            y = int(ty_bot - th + dy)
            if y < 1 or y >= im.height:
                continue
            frac = dy / max(1, th - 1)
            hw = max(1, int(round(1 + frac * half_w)))
            for x in range(cx - hw, cx + hw + 1):
                if x < 1 or x >= im.width - 1:
                    continue
                if snow and dy <= max(1, th // 2 - (1 if x > cx else 0)):
                    c = BIOME_RAMPS["frost"][6] if x <= cx else BIOME_RAMPS["frost"][5]
                elif dy == th - 1:
                    c = ramp[0]
                elif x < cx - hw // 3 or dy < 2:
                    c = hi if x <= cx else lit
                elif x > cx + hw // 3:
                    c = dk
                else:
                    c = lit if ((x + dy + tier_idx) & 1) == 0 else mid
                px(im, x, y, c)
    sel_outline_in_place(im, INK_DEEP, dk)


def make_trees() -> None:
    meadow_ramp = [LEAF_SHADOW, LEAF_DARK, LEAF_MID, LEAF_LIGHT, LEAF_SUN]
    forest_ramp = [BIOME_RAMPS["forest"][1], BIOME_RAMPS["forest"][2], BIOME_RAMPS["forest"][3],
                   BIOME_RAMPS["forest"][4], BIOME_RAMPS["forest"][5]]
    amber_ramp  = [BIOME_RAMPS["amber"][1], BIOME_RAMPS["amber"][2], BIOME_RAMPS["amber"][3],
                   BIOME_RAMPS["amber"][4], BIOME_RAMPS["amber"][5], GOLD_SUN]
    copper_ramp = [UMBER_DARK, CRIMSON_DARK, CRIMSON_MID, CORAL_LIGHT, OCHRE_LIGHT, GOLD_SUN]
    willow_ramp = [BIOME_RAMPS["fen"][1], BIOME_RAMPS["fen"][2], BIOME_RAMPS["fen"][3],
                   BIOME_RAMPS["fen"][4], BIOME_RAMPS["fen"][5]]

    # Broadleaf 0 (lush Valley Oak, 32x48)
    t0 = canvas(32, 48)
    _broadleaf(t0, 16, 47, [(19, 25, 10, 8), (12, 24, 10, 8), (16, 17, 11, 9), (14, 12, 8, 6)],
               meadow_ramp, seed=11)
    save_png(t0, "sprites/tree_0.png")

    # Broadleaf 1 (tall Elm, 32x48)
    t1 = canvas(32, 48)
    _broadleaf(t1, 16, 47, [(19, 22, 9, 9), (12, 20, 9, 9), (16, 13, 9, 8), (15, 8, 6, 5)],
               meadow_ramp, seed=23)
    save_png(t1, "sprites/tree_1.png")

    # Broadleaf 2 (forest canopy oak, 32x48)
    t2 = canvas(32, 48)
    _broadleaf(t2, 16, 47, [(18, 25, 10, 8), (12, 23, 9, 8), (16, 15, 10, 9)],
               forest_ramp, seed=37)
    save_png(t2, "sprites/tree_2.png")

    # Amber Oak (32x48)
    ao = canvas(32, 48)
    _broadleaf(ao, 16, 47, [(19, 24, 10, 8), (12, 23, 10, 8), (16, 15, 11, 9), (13, 11, 7, 5)],
               amber_ramp, seed=41)
    save_png(ao, "sprites/amber_oak.png")

    # Copper Oak (32x48)
    co = canvas(32, 48)
    _broadleaf(co, 16, 47, [(18, 25, 10, 8), (13, 22, 9, 8), (16, 14, 10, 8)],
               copper_ramp, seed=53)
    save_png(co, "sprites/copper_oak.png")

    # Weeping Willow (32x48)
    wl = canvas(32, 48)
    _broadleaf(wl, 16, 47, [(18, 23, 11, 8), (13, 22, 11, 8), (16, 15, 11, 8)],
               willow_ramp, seed=67, droop=True)
    save_png(wl, "sprites/willow.png")

    # Pine 0, 1, 2 (32x48)
    for i, sc in enumerate((0.92, 1.0, 0.84)):
        pn = canvas(32, 48)
        _conifer(pn, 16, 47, sc, forest_ramp, snow=False)
        save_png(pn, f"sprites/pine_{i}.png")

    # Snow Spruce (32x48)
    ts = canvas(32, 48)
    _conifer(ts, 16, 47, 0.98, [LEAF_SHADOW, LEAF_DARK, LEAF_MID, LEAF_LIGHT, WATER_FOAM], snow=True)
    save_png(ts, "sprites/tree_snow.png")

    # Dead Snag Tree (32x48)
    dt = canvas(32, 48)
    rect(dt, 14, 22, 4, 25, TIMBER_MID)
    vline(dt, 14, 22, 46, STONE_WARM)
    vline(dt, 17, 22, 46, UMBER_DARK)
    line(dt, 15, 28, 7, 17, OCHRE_MID)
    line(dt, 7, 17, 5, 11, STONE_WARM)
    line(dt, 17, 25, 25, 15, TIMBER_MID)
    line(dt, 25, 15, 27, 9, OCHRE_MID)
    line(dt, 15, 22, 13, 11, STONE_WARM)
    line(dt, 13, 11, 16, 5, CREAM_SUN)
    sel_outline_in_place(dt, INK_DEEP, UMBER_DARK)
    save_png(dt, "sprites/dead_tree.png")

    # Desert Yucca (32x32)
    yc = canvas(32, 32)
    rect(yc, 14, 16, 4, 14, TIMBER_MID)
    vline(yc, 14, 16, 29, OCHRE_MID)
    for ang_deg, length in ((-130, 10), (-105, 11), (-75, 11), (-50, 10), (-160, 8), (-20, 8)):
        rad = math.radians(ang_deg)
        ex = int(round(16 + math.cos(rad) * length))
        ey = int(round(16 + math.sin(rad) * length))
        line(yc, 16, 16, ex, ey, LEAF_MID if ang_deg < -90 else LEAF_DARK)
        line(yc, 16, 15, ex, ey - 1, LEAF_LIGHT if ang_deg < -80 else LEAF_MID)
    px(yc, 16, 8, CREAM_SUN)
    px(yc, 16, 9, GOLD_SUN)
    sel_outline_in_place(yc, INK_DEEP, LEAF_SHADOW)
    save_png(yc, "sprites/yucca.png")


def make_rocks_and_flora() -> None:
    # 3 small rocks (16x16)
    rock_shapes = [
        (8, 10, 5.5, 4.2, [SLATE_DARK, SLATE_MID, SLATE_LIGHT, STONE_WARM, CREAM_SUN]),
        (8, 11, 6.0, 3.8, [UMBER_DARK, TIMBER_MID, OCHRE_MID, OCHRE_LIGHT, CREAM_SUN]),
        (8, 10, 5.2, 4.5, [BIOME_RAMPS["frost"][2], BIOME_RAMPS["frost"][3], BIOME_RAMPS["frost"][4],
                           BIOME_RAMPS["frost"][5], BIOME_RAMPS["frost"][6]]),
    ]
    for i, (cx, cy, rx, ry, rmp) in enumerate(rock_shapes):
        rk = canvas(16, 16)
        shaded_ell(rk, cx, cy, rx, ry, rmp, seed=i * 19 + 3)
        sel_outline_in_place(rk, INK_DEEP, rmp[1])
        save_png(rk, f"sprites/rock_{i}.png")

    # Highland boulder (32x32)
    bd = canvas(32, 32)
    shaded_ell(bd, 16, 20, 12, 9, [SLATE_DARK, SLATE_MID, SLATE_LIGHT, STONE_WARM, CREAM_SUN], seed=5)
    shaded_ell(bd, 12, 16, 7, 5, [SLATE_MID, SLATE_LIGHT, STONE_WARM, CREAM_SUN], seed=8)
    # Moss patch on upper-left crest
    rect(bd, 9, 12, 5, 2, LEAF_LIGHT)
    rect(bd, 10, 14, 4, 2, LEAF_MID)
    sel_outline_in_place(bd, INK_DEEP, SLATE_DARK)
    save_png(bd, "sprites/boulder.png")

    # Arcane crystal cluster (16x16)
    cr = canvas(16, 16)
    for x0, y0, w, h, hi, mid in ((6, 2, 4, 12, WATER_FOAM, LAVENDER),
                                  (3, 6, 3, 8, WATER_FOAM, WATER_LIGHT),
                                  (10, 7, 3, 7, LAVENDER, INDIGO_ROYAL)):
        rect(cr, x0, y0 + 2, w, h - 2, mid)
        vline(cr, x0, y0 + 1, y0 + h - 1, hi)
        px(cr, x0 + w // 2, y0, SUN_WHITE)
    sel_outline_in_place(cr, INK_DEEP, SLATE_DARK)
    save_png(cr, "sprites/crystal.png")

    # Bushes (16x16)
    for fname, berries in (("sprites/bush.png", True), ("sprites/bush_plain.png", False)):
        bs = canvas(16, 16)
        shaded_ell(bs, 8, 10, 6, 4.5, [LEAF_SHADOW, LEAF_DARK, LEAF_MID, LEAF_LIGHT, LEAF_SUN], seed=7)
        shaded_ell(bs, 6, 8, 4, 3, [LEAF_DARK, LEAF_MID, LEAF_LIGHT, LEAF_SUN], seed=12)
        if berries:
            for bx, by in ((5, 8), (9, 7), (11, 10), (7, 11)):
                px(bs, bx, by, CRIMSON_MID)
                px(bs, bx, by - 1, CORAL_LIGHT)
        sel_outline_in_place(bs, INK_DEEP, LEAF_SHADOW)
        save_png(bs, fname)

    # Mushroom cluster (16x16)
    ms = canvas(16, 16)
    rect(ms, 6, 9, 3, 5, CREAM_SUN)
    rect(ms, 8, 10, 1, 4, STONE_WARM)
    ell(ms, 7.5, 7.5, 4.5, 2.8, CRIMSON_MID)
    hline(ms, 4, 9, 6, CORAL_LIGHT)
    px(ms, 5, 6, CREAM_SUN)
    px(ms, 9, 7, CREAM_SUN)
    rect(ms, 11, 11, 2, 3, CREAM_SUN)
    ell(ms, 12, 10, 2.5, 1.5, OCHRE_LIGHT)
    sel_outline_in_place(ms, INK_DEEP, UMBER_DARK)
    save_png(ms, "sprites/mushroom.png")

    # Potted plant (16x16)
    pp = canvas(16, 16)
    rect(pp, 5, 10, 6, 5, CRIMSON_MID)
    hline(pp, 4, 11, 10, CORAL_LIGHT)
    hline(pp, 5, 10, 14, CRIMSON_DARK)
    shaded_ell(pp, 8, 7, 4.2, 3.2, [LEAF_DARK, LEAF_MID, LEAF_LIGHT, LEAF_SUN], seed=14)
    sel_outline_in_place(pp, INK_DEEP, LEAF_SHADOW)
    save_png(pp, "sprites/potted_plant.png")

    # Urn (16x16)
    ur = canvas(16, 16)
    ell(ur, 8, 10, 4.5, 4.5, OCHRE_MID)
    ell(ur, 7, 9, 3.0, 3.0, OCHRE_LIGHT)
    rect(ur, 6, 4, 4, 2, OCHRE_LIGHT)
    hline(ur, 5, 10, 4, CREAM_SUN)
    hline(ur, 6, 9, 5, UMBER_DARK)
    sel_outline_in_place(ur, INK_DEEP, UMBER_DARK)
    save_png(ur, "sprites/urn.png")

    # Stump (16x16)
    st = canvas(16, 16)
    rect(st, 5, 8, 6, 6, TIMBER_MID)
    vline(st, 5, 8, 13, OCHRE_MID)
    vline(st, 10, 8, 13, UMBER_DARK)
    ell(st, 8, 8, 3.5, 2.0, OCHRE_LIGHT)
    px(st, 8, 8, TIMBER_MID)
    px(st, 4, 13, UMBER_DARK)
    px(st, 11, 13, UMBER_DARK)
    sel_outline_in_place(st, INK_DEEP, UMBER_DARK)
    save_png(st, "sprites/stump.png")

    # Reeds (16x16)
    rd = canvas(16, 16)
    for rx, h in ((4, 11), (7, 13), (10, 12), (12, 9)):
        vline(rd, rx, 15 - h, 14, LEAF_MID if rx < 9 else LEAF_DARK)
        px(rd, rx, 15 - h, LEAF_LIGHT)
    for cx, cy in ((7, 3), (10, 5)):
        rect(rd, cx, cy, 2, 3, TIMBER_MID)
        px(rd, cx, cy, OCHRE_LIGHT)
    sel_outline_in_place(rd, LEAF_SHADOW, LEAF_DARK)
    save_png(rd, "sprites/reeds.png")


def make_village_and_dungeon_props() -> None:
    # 1. Signpost (16x16)
    sg = canvas(16, 16)
    rect(sg, 7, 8, 2, 7, TIMBER_MID)
    rect(sg, 2, 2, 12, 7, OCHRE_LIGHT)
    hline(sg, 2, 13, 2, CREAM_SUN)
    hline(sg, 2, 13, 8, UMBER_DARK)
    hline(sg, 4, 11, 4, TIMBER_MID)
    hline(sg, 4, 9, 6, TIMBER_MID)
    sel_outline_in_place(sg, INK_DEEP, UMBER_DARK)
    save_png(sg, "sprites/sign.png")

    # 2. Torch (16x16)
    tc = canvas(16, 16)
    rect(tc, 7, 7, 2, 8, TIMBER_MID)
    rect(tc, 6, 5, 4, 3, SLATE_MID)
    rect(tc, 6, 2, 4, 3, GOLD_SUN)
    px(tc, 7, 1, CORAL_LIGHT)
    px(tc, 7, 3, SUN_WHITE)
    px(tc, 8, 3, SUN_WHITE)
    sel_outline_in_place(tc, INK_DEEP, UMBER_DARK)
    save_png(tc, "sprites/torch.png")

    # 3. Campfire (16x16)
    cf = canvas(16, 16)
    for rx, ry in ((4, 13), (12, 13), (8, 14), (6, 12), (10, 12)):
        rect(cf, rx - 1, ry - 1, 2, 2, SLATE_MID)
    line(cf, 4, 11, 12, 12, TIMBER_MID)
    line(cf, 5, 12, 11, 10, OCHRE_MID)
    ell(cf, 8, 8, 3.0, 3.5, CRIMSON_MID)
    ell(cf, 8, 8, 2.0, 2.5, GOLD_SUN)
    px(cf, 8, 7, SUN_WHITE)
    px(cf, 7, 4, GOLD_SUN)
    sel_outline_in_place(cf, INK_DEEP, UMBER_DARK)
    save_png(cf, "sprites/campfire.png")

    # 4. Iron Cauldron (16x16)
    cd = canvas(16, 16)
    ell(cd, 8, 10, 5.5, 4.5, SLATE_DARK)
    ell(cd, 7, 9, 3.5, 2.5, SLATE_MID)
    hline(cd, 4, 12, 5, SLATE_LIGHT)
    hline(cd, 5, 11, 6, LEAF_SUN)
    px(cd, 7, 5, LEAF_LIGHT)
    sel_outline_in_place(cd, INK_DEEP, SLATE_DARK)
    save_png(cd, "sprites/cauldron.png")

    # 5. Water Basin / Bucket (16x16)
    bs = canvas(16, 16)
    rect(bs, 4, 6, 8, 8, TIMBER_MID)
    hline(bs, 4, 11, 8, SLATE_LIGHT)
    hline(bs, 4, 11, 12, SLATE_DARK)
    ell(bs, 8, 6, 4.0, 1.8, WATER_LIGHT)
    px(bs, 7, 6, WATER_FOAM)
    sel_outline_in_place(bs, INK_DEEP, UMBER_DARK)
    save_png(bs, "sprites/basin.png")

    # 6. Fence H (16x16), Fence Post (16x16), Fence Gate (32x16)
    fn = canvas(16, 16)
    hline(fn, 0, 15, 7, OCHRE_LIGHT)
    hline(fn, 0, 15, 8, TIMBER_MID)
    hline(fn, 0, 15, 12, OCHRE_MID)
    hline(fn, 0, 15, 13, UMBER_DARK)
    for px_x in (2, 11):
        rect(fn, px_x, 4, 3, 11, TIMBER_MID)
        vline(fn, px_x, 4, 14, OCHRE_LIGHT)
        px(fn, px_x + 1, 3, CREAM_SUN)
    sel_outline_in_place(fn, INK_DEEP, UMBER_DARK)
    save_png(fn, "sprites/fence.png")

    fp = canvas(16, 16)
    rect(fp, 6, 4, 4, 11, TIMBER_MID)
    vline(fp, 6, 4, 14, OCHRE_LIGHT)
    vline(fp, 9, 4, 14, UMBER_DARK)
    hline(fp, 6, 9, 3, CREAM_SUN)
    sel_outline_in_place(fp, INK_DEEP, UMBER_DARK)
    save_png(fp, "sprites/fence_post.png")

    fg = canvas(32, 16)
    for px_x in (2, 27):
        rect(fg, px_x, 3, 3, 12, TIMBER_MID)
        vline(fg, px_x, 3, 14, OCHRE_LIGHT)
    for gx in range(6, 26, 4):
        rect(fg, gx, 5, 2, 9, OCHRE_MID)
        px(fg, gx, 5, CREAM_SUN)
    hline(fg, 5, 26, 7, OCHRE_LIGHT)
    hline(fg, 5, 26, 11, TIMBER_MID)
    sel_outline_in_place(fg, INK_DEEP, UMBER_DARK)
    save_png(fg, "sprites/fence_gate.png")

    # 7. Bench (32x16) & Flowerbed (32x16)
    bn = canvas(32, 16)
    rect(bn, 3, 5, 26, 3, OCHRE_LIGHT)
    hline(bn, 3, 28, 7, TIMBER_MID)
    rect(bn, 2, 9, 28, 3, OCHRE_MID)
    rect(bn, 4, 12, 3, 3, UMBER_DARK)
    rect(bn, 25, 12, 3, 3, UMBER_DARK)
    sel_outline_in_place(bn, INK_DEEP, UMBER_DARK)
    save_png(bn, "sprites/bench.png")

    fb = canvas(32, 16)
    rect(fb, 2, 7, 28, 8, TIMBER_MID)
    hline(fb, 2, 29, 7, OCHRE_LIGHT)
    rect(fb, 4, 6, 24, 5, LEAF_DARK)
    for fx in range(5, 27, 3):
        px(fb, fx, 5, LEAF_LIGHT)
        px(fb, fx + 1, 5, GOLD_SUN if (fx % 6 == 2) else CRIMSON_MID)
        px(fb, fx + 1, 4, SUN_WHITE if (fx % 6 == 2) else CORAL_LIGHT)
    sel_outline_in_place(fb, INK_DEEP, UMBER_DARK)
    save_png(fb, "sprites/flowerbed.png")

    # 8. Ladder (16x16), Barrel (16x16), Crate (16x16), Keg (16x16)
    ld = canvas(16, 16)
    rect(ld, 4, 1, 2, 14, TIMBER_MID)
    rect(ld, 10, 1, 2, 14, TIMBER_MID)
    for y in (3, 7, 11):
        hline(ld, 5, 10, y, OCHRE_LIGHT)
    sel_outline_in_place(ld, INK_DEEP, UMBER_DARK)
    save_png(ld, "sprites/ladder.png")

    br = canvas(16, 16)
    ell(br, 8, 9, 5.0, 5.5, TIMBER_MID)
    rect(br, 4, 4, 8, 2, OCHRE_LIGHT)
    hline(br, 3, 12, 6, SLATE_LIGHT)
    hline(br, 3, 12, 11, SLATE_DARK)
    vline(br, 5, 5, 13, OCHRE_MID)
    sel_outline_in_place(br, INK_DEEP, UMBER_DARK)
    save_png(br, "sprites/barrel.png")

    ct = canvas(16, 16)
    rect(ct, 3, 3, 10, 11, OCHRE_MID)
    rect(ct, 4, 4, 8, 9, OCHRE_LIGHT)
    hline(ct, 3, 12, 3, CREAM_SUN)
    line(ct, 4, 4, 11, 11, TIMBER_MID)
    line(ct, 11, 4, 4, 11, TIMBER_MID)
    sel_outline_in_place(ct, INK_DEEP, UMBER_DARK)
    save_png(ct, "sprites/crate.png")

    kg = canvas(16, 16)
    ell(kg, 8, 8, 5.5, 4.5, TIMBER_MID)
    hline(kg, 3, 12, 6, OCHRE_LIGHT)
    vline(kg, 6, 4, 12, SLATE_LIGHT)
    vline(kg, 10, 4, 12, SLATE_DARK)
    px(kg, 8, 10, GOLD_SUN)
    sel_outline_in_place(kg, INK_DEEP, UMBER_DARK)
    save_png(kg, "sprites/keg.png")

    # 9. Table (16x16), Longtable (48x16), Counter (16x16), Stool (16x16)
    tb = canvas(16, 16)
    rect(tb, 2, 4, 12, 5, OCHRE_LIGHT)
    hline(tb, 2, 13, 4, CREAM_SUN)
    hline(tb, 2, 13, 9, TIMBER_MID)
    rect(tb, 3, 10, 2, 4, UMBER_DARK)
    rect(tb, 11, 10, 2, 4, UMBER_DARK)
    sel_outline_in_place(tb, INK_DEEP, UMBER_DARK)
    save_png(tb, "sprites/table.png")

    lt = canvas(48, 16)
    rect(lt, 2, 4, 44, 6, OCHRE_LIGHT)
    hline(lt, 2, 45, 4, CREAM_SUN)
    hline(lt, 2, 45, 9, TIMBER_MID)
    for lx in (4, 23, 41):
        rect(lt, lx, 10, 3, 4, UMBER_DARK)
    sel_outline_in_place(lt, INK_DEEP, UMBER_DARK)
    save_png(lt, "sprites/longtable.png")

    cn = canvas(16, 16)
    rect(cn, 0, 2, 16, 13, TIMBER_MID)
    hline(cn, 0, 15, 2, CREAM_SUN)
    hline(cn, 0, 15, 3, OCHRE_LIGHT)
    hline(cn, 0, 15, 5, UMBER_DARK)
    hline(cn, 0, 15, 14, INK_DEEP)
    save_png(cn, "sprites/counter.png")

    sl = canvas(16, 16)
    rect(sl, 5, 6, 6, 3, OCHRE_LIGHT)
    hline(sl, 5, 10, 6, CREAM_SUN)
    rect(sl, 5, 9, 2, 5, TIMBER_MID)
    rect(sl, 9, 9, 2, 5, UMBER_DARK)
    sel_outline_in_place(sl, INK_DEEP, UMBER_DARK)
    save_png(sl, "sprites/stool.png")

    # 10. Lantern (16x16), Candle (16x16), Plate (16x16)
    ln = canvas(16, 16)
    rect(ln, 6, 4, 4, 2, SLATE_DARK)
    rect(ln, 5, 6, 6, 6, GOLD_SUN)
    rect(ln, 6, 7, 4, 4, SUN_WHITE)
    hline(ln, 5, 10, 12, SLATE_MID)
    sel_outline_in_place(ln, INK_DEEP, SLATE_DARK)
    save_png(ln, "sprites/lantern.png")

    cnd = canvas(16, 16)
    rect(cnd, 6, 11, 4, 2, GOLD_SHADE)
    rect(cnd, 7, 6, 2, 5, CREAM_SUN)
    px(cnd, 7, 4, GOLD_SUN)
    px(cnd, 8, 4, GOLD_SUN)
    px(cnd, 7, 3, SUN_WHITE)
    sel_outline_in_place(cnd, INK_DEEP, UMBER_DARK)
    save_png(cnd, "sprites/candle.png")

    pl = canvas(16, 16)
    ell(pl, 8, 9, 4.5, 3.0, CREAM_SUN)
    ell(pl, 8, 9, 2.8, 1.8, STONE_WARM)
    px(pl, 7, 8, CORAL_LIGHT)
    px(pl, 9, 9, GOLD_SUN)
    sel_outline_in_place(pl, INK_DEEP, SLATE_DARK)
    save_png(pl, "sprites/plate.png")

    # 11. Shelf (48x32), Bed (16x32), Pillar (16x32)
    sh = canvas(48, 32)
    rect(sh, 2, 2, 44, 28, TIMBER_MID)
    hline(sh, 2, 45, 2, OCHRE_LIGHT)
    for sy in (9, 18, 27):
        rect(sh, 4, sy - 6, 40, 5, UMBER_DARK)
        hline(sh, 3, 44, sy, OCHRE_LIGHT)
        for bx in range(6, 42, 4):
            col = (CRIMSON_MID, INDIGO_ROYAL, GOLD_SUN, LEAF_MID, WATER_LIGHT)[(bx + sy) % 5]
            rect(sh, bx, sy - 5, 3, 4, col)
            px(sh, bx, sy - 5, SUN_WHITE)
    sel_outline_in_place(sh, INK_DEEP, UMBER_DARK)
    save_png(sh, "sprites/shelf.png")

    bd = canvas(16, 32)
    rect(bd, 2, 3, 12, 27, TIMBER_MID)
    hline(bd, 2, 13, 3, OCHRE_LIGHT)
    rect(bd, 3, 5, 10, 6, CREAM_SUN)
    rect(bd, 3, 11, 10, 17, CRIMSON_MID)
    hline(bd, 3, 12, 11, CREAM_SUN)
    hline(bd, 3, 12, 12, CORAL_LIGHT)
    hline(bd, 3, 12, 27, CRIMSON_DARK)
    sel_outline_in_place(bd, INK_DEEP, UMBER_DARK)
    save_png(bd, "sprites/bed.png")

    pil = canvas(16, 32)
    rect(pil, 3, 3, 10, 4, CREAM_SUN)
    rect(pil, 4, 7, 8, 19, STONE_WARM)
    vline(pil, 5, 7, 25, CREAM_SUN)
    vline(pil, 7, 7, 25, SLATE_LIGHT)
    vline(pil, 10, 7, 25, SLATE_DARK)
    rect(pil, 2, 26, 12, 4, SLATE_MID)
    hline(pil, 2, 13, 26, CREAM_SUN)
    sel_outline_in_place(pil, INK_DEEP, SLATE_DARK)
    save_png(pil, "sprites/pillar.png")

    # 12. Mailbox (16x16), Noticeboard (16x16), Chest Closed (16x16), Chest Open (16x16)
    mb = canvas(16, 16)
    rect(mb, 7, 9, 2, 6, TIMBER_MID)
    rect(mb, 4, 4, 8, 6, SLATE_MID)
    hline(mb, 4, 11, 4, SLATE_LIGHT)
    px(mb, 11, 5, CRIMSON_MID)
    px(mb, 11, 6, CORAL_LIGHT)
    sel_outline_in_place(mb, INK_DEEP, SLATE_DARK)
    save_png(mb, "sprites/mailbox.png")

    nb = canvas(16, 16)
    rect(nb, 3, 9, 2, 6, TIMBER_MID)
    rect(nb, 11, 9, 2, 6, UMBER_DARK)
    rect(nb, 1, 2, 14, 9, OCHRE_MID)
    rect(nb, 2, 3, 12, 7, TIMBER_MID)
    rect(nb, 3, 4, 4, 5, CREAM_SUN)
    rect(nb, 9, 4, 4, 4, GOLD_SUN)
    sel_outline_in_place(nb, INK_DEEP, UMBER_DARK)
    save_png(nb, "sprites/noticeboard.png")

    cc = canvas(16, 16)
    rect(cc, 2, 5, 12, 9, TIMBER_MID)
    hline(cc, 2, 13, 5, GOLD_SUN)
    hline(cc, 2, 13, 9, GOLD_SHADE)
    vline(cc, 4, 5, 13, GOLD_SUN)
    vline(cc, 11, 5, 13, GOLD_SHADE)
    rect(cc, 7, 8, 2, 3, SUN_WHITE)
    sel_outline_in_place(cc, INK_DEEP, UMBER_DARK)
    save_png(cc, "sprites/chest_closed.png")

    co = canvas(16, 16)
    rect(co, 2, 3, 12, 4, OCHRE_MID)
    hline(co, 2, 13, 3, GOLD_SUN)
    rect(co, 2, 7, 12, 7, TIMBER_MID)
    rect(co, 3, 7, 10, 3, INK_DEEP)
    hline(co, 2, 13, 10, GOLD_SHADE)
    vline(co, 4, 3, 13, GOLD_SUN)
    vline(co, 11, 3, 13, GOLD_SHADE)
    sel_outline_in_place(co, INK_DEEP, UMBER_DARK)
    save_png(co, "sprites/chest_open.png")

    # 13. Forge Hearth (48x48) & Forge Anvil (32x32)
    fh = canvas(48, 48)
    rect(fh, 16, 2, 16, 20, SLATE_MID)
    vline(fh, 16, 2, 21, STONE_WARM)
    rect(fh, 6, 18, 36, 27, SLATE_DARK)
    rect(fh, 8, 20, 32, 23, SLATE_MID)
    hline(fh, 6, 41, 18, CREAM_SUN)
    # Glowing coal arch
    rect(fh, 14, 27, 20, 15, INK_DEEP)
    rect(fh, 16, 33, 16, 8, CRIMSON_MID)
    rect(fh, 18, 35, 12, 5, GOLD_SUN)
    rect(fh, 20, 37, 8, 3, SUN_WHITE)
    sel_outline_in_place(fh, INK_DEEP, SLATE_DARK)
    save_png(fh, "sprites/forge_hearth.png")

    fa = canvas(32, 32)
    rect(fa, 9, 17, 14, 12, TIMBER_MID)
    vline(fa, 9, 17, 28, OCHRE_MID)
    # Horned iron anvil on block
    rect(fa, 7, 11, 18, 4, SLATE_MID)
    hline(fa, 5, 25, 11, CREAM_SUN)
    hline(fa, 5, 9, 12, SLATE_LIGHT)
    rect(fa, 12, 15, 8, 3, SLATE_DARK)
    sel_outline_in_place(fa, INK_DEEP, SLATE_DARK)
    save_png(fa, "sprites/forge_anvil.png")

    # 14. Pixel Cloud (64x32) — clean 1x clouds
    cl = canvas(64, 32)
    for cx, cy, rx, ry in ((20, 19, 14, 7), (33, 16, 15, 9), (45, 20, 11, 6)):
        shaded_ell(cl, cx, cy, rx, ry, [SLATE_LIGHT, WATER_FOAM, CREAM_SUN, SUN_WHITE], seed=9)
    hline(cl, 12, 52, 25, SLATE_LIGHT)
    save_png(cl, "sprites/cloud.png")


def generate_objects() -> None:
    make_trees()
    make_rocks_and_flora()
    make_village_and_dungeon_props()


if __name__ == "__main__":
    generate_objects()
    print("Generated all flora, rocks, props, furniture, and cloud sprites")
