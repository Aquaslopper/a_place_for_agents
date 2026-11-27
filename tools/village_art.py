#!/usr/bin/env python3
"""Generate cohesive 16-bit Valley Hearth buildings, landmarks, and structures.

Every building and landmark matches its C++ tile footprint on exact 16px grid
multiples, uses strictly the 32-color Core Palette, consistent upper-left
sunlight, and 1px selective dark outlines.
"""
from __future__ import annotations

from snes import (
    INK_DEEP, SLATE_DARK, SLATE_MID, SLATE_LIGHT, STONE_WARM, CREAM_SUN,
    UMBER_DARK, TIMBER_MID, OCHRE_MID, OCHRE_LIGHT, GOLD_SHADE, GOLD_SUN, SUN_WHITE,
    LEAF_SHADOW, LEAF_DARK, LEAF_MID, LEAF_LIGHT, LEAF_SUN,
    WATER_DEEP, WATER_MID, WATER_LIGHT, WATER_FOAM,
    CRIMSON_DARK, CRIMSON_MID, CORAL_LIGHT,
    NAVY_UI, INDIGO_ROYAL, LAVENDER,
    canvas, px, rect, hline, vline, line, ell, shaded_ell,
    sel_outline_in_place, h2, save_png,
)


def _shingle_roof(im, x0: int, y0: int, x1: int, y1: int, peak_half: int,
                  ramp: tuple, ridge_col=None, thatch: bool = False) -> None:
    """Draw a clean pitched roof with upper-left highlight and shingle courses."""
    dk, mid, lit, hi = ramp
    cx = (x0 + x1) // 2
    height = max(1, y1 - y0)
    for y in range(y0, y1 + 1):
        t = (y - y0) / height
        half = int(round(peak_half + t * ((x1 - x0) * 0.5 - peak_half)))
        lx = max(x0, cx - half)
        rx = min(x1, cx + half)
        course = (y - y0) % 4
        for x in range(lx, rx + 1):
            left_side = x <= cx + 1
            if y == y0 or y == y0 + 1:
                c = hi if left_side else lit
            elif y == y1:
                c = INK_DEEP
            elif y == y1 - 1:
                c = dk
            elif course == 0:
                c = dk if not thatch else mid
            elif course == 1:
                c = hi if left_side else lit
            else:
                shingle_alt = ((x // 4 + (y // 4)) & 1) == 0
                if left_side:
                    c = lit if shingle_alt else mid
                else:
                    c = mid if shingle_alt else dk
            px(im, x, y, c)
    # Upper-left gable ridge highlight
    if ridge_col:
        hline(im, max(x0, cx - peak_half), min(x1, cx + peak_half), y0, ridge_col)


def _wall_facade(im, x0: int, y0: int, x1: int, y1: int,
                 wall_lit=CREAM_SUN, wall_mid=STONE_WARM, wall_sh=SLATE_MID,
                 stone_base: bool = True, half_timber: bool = True) -> None:
    """Draw cottage wall with upper-left lit stucco, stone plinth, and timber framing."""
    w = x1 - x0 + 1
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            if stone_base and y >= y1 - 5:
                brick = ((x // 5 + (y // 3)) & 1) == 0
                c = SLATE_LIGHT if (brick and x < x0 + w // 2) else (SLATE_MID if brick else SLATE_DARK)
                if y == y1 - 5:
                    c = SLATE_LIGHT
                elif y == y1:
                    c = INK_DEEP
            else:
                # Upper-left sunlit plaster / warm shadow on right edge
                if x <= x0 + 3 or y <= y0 + 2:
                    c = wall_lit
                elif x >= x1 - 3:
                    c = wall_sh
                else:
                    c = wall_mid if h2(x, y, 17) > 0.2 else wall_lit
            px(im, x, y, c)
    # Under-eave contact shadow
    hline(im, x0, x1, y0, UMBER_DARK)
    hline(im, x0, x1, y0 + 1, wall_sh)
    if half_timber:
        # Vertical corner posts + horizontal mid-beam
        for px_x in (x0, x0 + 1, x1 - 1, x1):
            vline(im, px_x, y0, y1 - 1, TIMBER_MID if px_x < x1 - 1 else UMBER_DARK)
        mid_y = (y0 + y1) // 2 - 2
        hline(im, x0, x1, mid_y, OCHRE_MID)
        hline(im, x0, x1, mid_y + 1, UMBER_DARK)


def _door(im, cx: int, bottom_y: int, w: int = 10, h: int = 15, arched: bool = True) -> None:
    x0 = cx - w // 2
    y0 = bottom_y - h + 1
    # Stone/timber frame
    rect(im, x0 - 1, y0 - 1, w + 2, h + 1, UMBER_DARK)
    for y in range(y0, bottom_y + 1):
        for x in range(x0, x0 + w):
            if arched and y == y0 and (x == x0 or x == x0 + w - 1):
                px(im, x, y, SLATE_LIGHT)
                continue
            plank = ((x - x0) % 3) == 0
            c = UMBER_DARK if plank else (OCHRE_MID if x < cx else TIMBER_MID)
            if y == y0:
                c = OCHRE_LIGHT
            px(im, x, y, c)
    # Iron strap & brass doorknob
    hline(im, x0 + 1, x0 + w - 2, y0 + 5, SLATE_DARK)
    px(im, x0 + w - 3, y0 + 8, GOLD_SUN)
    # Stone step below threshold
    hline(im, x0 - 1, x0 + w, bottom_y + 1, STONE_WARM)


def _window(im, x: int, y: int, w: int = 8, h: int = 8, glow: bool = True, flowerbox: bool = False) -> None:
    # Frame
    rect(im, x - 1, y - 1, w + 2, h + 2, UMBER_DARK)
    rect(im, x, y, w, h, GOLD_SUN if glow else WATER_MID)
    if glow:
        rect(im, x + 1, y + 1, w - 2, h - 3, SUN_WHITE)
        hline(im, x, x + w - 1, y + h - 1, GOLD_SHADE)
    else:
        hline(im, x, x + w - 1, y, WATER_FOAM)
        hline(im, x, x + w - 1, y + h - 1, WATER_DEEP)
    # Cross mullions
    vline(im, x + w // 2 - 1, y, y + h - 1, TIMBER_MID)
    hline(im, x, x + w - 1, y + h // 2 - 1, TIMBER_MID)
    # Sill
    hline(im, x - 1, x + w, y + h, OCHRE_LIGHT)
    hline(im, x - 1, x + w, y + h + 1, UMBER_DARK)
    if flowerbox:
        rect(im, x - 1, y + h, w + 2, 3, TIMBER_MID)
        for bx in range(x, x + w, 2):
            px(im, bx, y + h - 1, LEAF_LIGHT)
            px(im, bx + 1, y + h - 1, CRIMSON_MID if (bx & 2) else GOLD_SUN)


def _chimney(im, x: int, y0: int, y1: int, smoke: bool = True) -> None:
    rect(im, x, y0, 7, y1 - y0 + 1, SLATE_MID)
    vline(im, x, y0, y1, STONE_WARM)
    vline(im, x + 6, y0, y1, SLATE_DARK)
    hline(im, x - 1, x + 7, y0, CREAM_SUN)
    hline(im, x - 1, x + 7, y0 + 1, SLATE_DARK)
    if smoke:
        for sx, sy, c in ((x + 3, y0 - 2, STONE_WARM), (x + 4, y0 - 4, SLATE_LIGHT),
                          (x + 5, y0 - 6, STONE_WARM), (x + 6, y0 - 8, CREAM_SUN)):
            px(im, sx, sy, c)
            px(im, sx + 1, sy, c)


def make_houses() -> None:
    # Roof ramps matching Valley Hearth aesthetic
    roof_styles = [
        ((SLATE_DARK, SLATE_MID, SLATE_LIGHT, WATER_FOAM), False, CREAM_SUN, STONE_WARM, True),   # 0: Blue-slate cottage
        ((CRIMSON_DARK, CRIMSON_MID, CORAL_LIGHT, GOLD_SUN), False, CREAM_SUN, STONE_WARM, False),# 1: Terracotta cottage
        ((LEAF_SHADOW, LEAF_DARK, LEAF_MID, LEAF_LIGHT), False, STONE_WARM, SLATE_MID, True),     # 2: Moss-shingle cottage
        ((TIMBER_MID, OCHRE_MID, OCHRE_LIGHT, GOLD_SUN), True, CREAM_SUN, OCHRE_LIGHT, False),    # 3: Thatch cottage
        ((SLATE_DARK, SLATE_MID, SLATE_LIGHT, CREAM_SUN), False, STONE_WARM, SLATE_LIGHT, True),  # 4: Alpine steep slate
    ]
    for idx, (ramp, thatch, wl, wm, box) in enumerate(roof_styles):
        im = canvas(64, 64)
        # Body: x=7..56 (50px wide centered in 64px tile box), floor baseline y=62
        _chimney(im, 42 if idx % 2 == 0 else 15, 7, 25, smoke=(idx in (0, 2, 3)))
        _wall_facade(im, 8, 32, 55, 61, wall_lit=wl, wall_mid=wm, wall_sh=SLATE_MID,
                     stone_base=True, half_timber=True)
        peak = 12 if idx != 4 else 6
        roof_y0 = 12 if idx != 4 else 8
        _shingle_roof(im, 5, roof_y0, 58, 33, peak_half=peak, ramp=ramp,
                      ridge_col=ramp[3], thatch=thatch)
        # Front centered or offset door
        dcx = 32 if idx in (0, 2, 4) else 24
        _door(im, dcx, 61, w=10, h=15, arched=True)
        if dcx == 32:
            _window(im, 13, 41, 8, 8, glow=True, flowerbox=box)
            _window(im, 43, 41, 8, 8, glow=True, flowerbox=box)
        else:
            _window(im, 37, 40, 12, 9, glow=True, flowerbox=box)
            _window(im, 11, 41, 6, 8, glow=True, flowerbox=False)
        # Attic dormer / round gable window on variants 0 and 4
        if idx in (0, 4):
            _window(im, 29, 20, 6, 6, glow=True, flowerbox=False)
        sel_outline_in_place(im, INK_DEEP, SLATE_DARK)
        save_png(im, f"sprites/house_{idx}.png")


def make_shop() -> None:
    # 80x64 (b.w = 5 tiles = 80px)
    im = canvas(80, 64)
    _chimney(im, 58, 7, 24, smoke=True)
    _wall_facade(im, 8, 30, 71, 61, wall_lit=CREAM_SUN, wall_mid=STONE_WARM, wall_sh=SLATE_MID)
    _shingle_roof(im, 5, 11, 74, 31, peak_half=20,
                  ramp=(CRIMSON_DARK, CRIMSON_MID, CORAL_LIGHT, GOLD_SUN), ridge_col=GOLD_SUN)
    # Striped shop awning along porch
    for y in range(34, 41):
        for x in range(11, 46):
            stripe = ((x - 11) // 4) & 1
            if y == 40:
                c = INK_DEEP
            elif y == 34:
                c = SUN_WHITE if stripe else CORAL_LIGHT
            else:
                c = CREAM_SUN if stripe else CRIMSON_MID
            px(im, x, y, c)
    # Display goods window under awning + door on right
    _window(im, 15, 44, 24, 10, glow=True, flowerbox=False)
    _door(im, 56, 61, w=11, h=16, arched=True)
    # Hanging gold coin emblem sign
    hline(im, 47, 50, 35, SLATE_DARK)
    ell(im, 49, 40, 3.0, 3.0, GOLD_SUN)
    px(im, 48, 39, SUN_WHITE)
    sel_outline_in_place(im, INK_DEEP, SLATE_DARK)
    save_png(im, "sprites/shop.png")


def make_inn() -> None:
    # 80x64 (b.w = 5 tiles) — 2-story tavern/inn with glowing dormer & stag sign
    im = canvas(80, 64)
    _chimney(im, 15, 6, 22, smoke=True)
    _wall_facade(im, 7, 29, 72, 61, wall_lit=CREAM_SUN, wall_mid=OCHRE_LIGHT, wall_sh=TIMBER_MID)
    _shingle_roof(im, 4, 10, 75, 30, peak_half=24,
                  ramp=(SLATE_DARK, SLATE_MID, SLATE_LIGHT, WATER_FOAM), ridge_col=WATER_FOAM)
    # Dormer gable sticking out of roof
    rect(im, 32, 13, 16, 14, CREAM_SUN)
    _shingle_roof(im, 30, 7, 49, 15, peak_half=2,
                  ramp=(SLATE_DARK, SLATE_MID, SLATE_LIGHT, CREAM_SUN), ridge_col=CREAM_SUN)
    _window(im, 36, 17, 8, 7, glow=True)
    # Ground floor welcoming double door + warm tavern windows
    _door(im, 40, 61, w=12, h=16, arched=True)
    _window(im, 14, 41, 10, 9, glow=True, flowerbox=True)
    _window(im, 56, 41, 10, 9, glow=True, flowerbox=True)
    # Lantern sconces flanking door
    for lx in (31, 48):
        px(im, lx, 47, SLATE_DARK)
        px(im, lx, 48, GOLD_SUN)
        px(im, lx, 49, SUN_WHITE)
    # Hanging signboard on left
    hline(im, 25, 29, 34, UMBER_DARK)
    rect(im, 25, 35, 6, 6, GOLD_SUN)
    rect(im, 26, 36, 4, 4, TIMBER_MID)
    sel_outline_in_place(im, INK_DEEP, SLATE_DARK)
    save_png(im, "sprites/inn.png")


def make_smithy() -> None:
    # 80x64 (b.w = 5 tiles) — stone forge + timber smithy porch
    im = canvas(80, 64)
    # Tall stone furnace chimney on left
    _chimney(im, 12, 4, 34, smoke=True)
    _wall_facade(im, 8, 28, 71, 61, wall_lit=STONE_WARM, wall_mid=SLATE_MID, wall_sh=SLATE_DARK)
    _shingle_roof(im, 6, 12, 73, 30, peak_half=18,
                  ramp=(SLATE_DARK, SLATE_MID, SLATE_LIGHT, STONE_WARM), ridge_col=STONE_WARM)
    # Glowing forge furnace opening on left wing
    rect(im, 14, 45, 13, 15, SLATE_DARK)
    rect(im, 16, 47, 9, 12, INK_DEEP)
    rect(im, 17, 52, 7, 6, CRIMSON_MID)
    rect(im, 18, 54, 5, 4, GOLD_SUN)
    hline(im, 19, 21, 56, SUN_WHITE)
    # Door & anvil porch on right
    _door(im, 42, 61, w=11, h=15, arched=True)
    _window(im, 57, 42, 9, 8, glow=True)
    # Hanging anvil sign
    hline(im, 31, 35, 36, SLATE_DARK)
    rect(im, 30, 38, 6, 3, SLATE_LIGHT)
    rect(im, 31, 41, 4, 2, SLATE_DARK)
    sel_outline_in_place(im, INK_DEEP, SLATE_DARK)
    save_png(im, "sprites/smithy.png")


def make_manor() -> None:
    # 80x80 (b.w = 5 tiles) — Elder's Hall / grand timbered stone manor
    im = canvas(80, 80)
    _chimney(im, 15, 10, 30, smoke=True)
    _chimney(im, 58, 10, 30, smoke=True)
    _wall_facade(im, 7, 36, 72, 77, wall_lit=CREAM_SUN, wall_mid=STONE_WARM, wall_sh=SLATE_MID)
    _shingle_roof(im, 4, 14, 75, 37, peak_half=22,
                  ramp=(SLATE_DARK, SLATE_MID, SLATE_LIGHT, WATER_FOAM), ridge_col=GOLD_SUN)
    # Twin stepped gables
    for gx in (18, 50):
        _shingle_roof(im, gx - 2, 16, gx + 14, 28, peak_half=2,
                      ramp=(SLATE_DARK, SLATE_MID, SLATE_LIGHT, CREAM_SUN), ridge_col=CREAM_SUN)
        _window(im, gx + 3, 22, 6, 6, glow=True)
    # Upper gallery windows
    _window(im, 14, 41, 10, 9, glow=True)
    _window(im, 35, 41, 10, 9, glow=True)
    _window(im, 56, 41, 10, 9, glow=True)
    # Grand columned portico + oak doors
    rect(im, 27, 56, 26, 21, SLATE_MID)
    rect(im, 29, 58, 22, 19, STONE_WARM)
    _door(im, 40, 77, w=12, h=17, arched=True)
    # Flanking lanterns
    for lx in (28, 51):
        px(im, lx, 62, GOLD_SUN)
        px(im, lx, 63, SUN_WHITE)
    sel_outline_in_place(im, INK_DEEP, SLATE_DARK)
    save_png(im, "sprites/manor.png")


def make_temple() -> None:
    # 80x80 (b.w = 5 tiles) — Meadow Chapel with slate bell spire & rose window
    im = canvas(80, 80)
    # Main stone hall
    _wall_facade(im, 10, 38, 69, 77, wall_lit=CREAM_SUN, wall_mid=STONE_WARM, wall_sh=SLATE_MID, half_timber=False)
    _shingle_roof(im, 7, 22, 72, 39, peak_half=16,
                  ramp=(SLATE_DARK, SLATE_MID, SLATE_LIGHT, WATER_FOAM), ridge_col=GOLD_SUN)
    # Central bell tower spire
    rect(im, 30, 18, 20, 28, STONE_WARM)
    vline(im, 30, 18, 45, CREAM_SUN)
    vline(im, 49, 18, 45, SLATE_DARK)
    _shingle_roof(im, 27, 4, 52, 20, peak_half=1,
                  ramp=(SLATE_DARK, SLATE_MID, SLATE_LIGHT, WATER_FOAM), ridge_col=GOLD_SUN)
    # Gold spire finial
    vline(im, 40, 1, 5, GOLD_SUN)
    hline(im, 38, 42, 3, GOLD_SUN)
    px(im, 40, 1, SUN_WHITE)
    # Stained-glass rose window
    ell(im, 40, 33, 5.0, 5.0, SLATE_DARK)
    ell(im, 40, 33, 4.0, 4.0, INDIGO_ROYAL)
    px(im, 40, 33, GOLD_SUN)
    px(im, 39, 33, LAVENDER)
    px(im, 41, 33, WATER_FOAM)
    px(im, 40, 32, CORAL_LIGHT)
    px(im, 40, 34, WATER_LIGHT)
    # Arched lancet side windows
    _window(im, 17, 48, 7, 14, glow=True)
    _window(im, 56, 48, 7, 14, glow=True)
    _door(im, 40, 77, w=12, h=18, arched=True)
    sel_outline_in_place(im, INK_DEEP, SLATE_DARK)
    save_png(im, "sprites/temple.png")


def make_castle() -> None:
    # 96x80 — Northern Castle of Elm keep: distinct grand silhouette at 1x!
    im = canvas(96, 80)
    # Curtain walls
    _wall_facade(im, 12, 32, 83, 77, wall_lit=STONE_WARM, wall_mid=SLATE_LIGHT, wall_sh=SLATE_MID,
                 stone_base=True, half_timber=False)
    # Central keep tower rising above curtain wall
    _wall_facade(im, 31, 14, 64, 48, wall_lit=CREAM_SUN, wall_mid=STONE_WARM, wall_sh=SLATE_MID,
                 stone_base=False, half_timber=False)
    _shingle_roof(im, 28, 3, 67, 16, peak_half=6,
                  ramp=(SLATE_DARK, SLATE_MID, INDIGO_ROYAL, LAVENDER), ridge_col=GOLD_SUN)
    # Corner flanking turrets
    for tx, tall in ((6, True), (72, True)):
        _wall_facade(im, tx, 20, tx + 17, 77, wall_lit=CREAM_SUN if tx < 40 else STONE_WARM,
                     wall_mid=STONE_WARM, wall_sh=SLATE_DARK, stone_base=True, half_timber=False)
        _shingle_roof(im, tx - 2, 8, tx + 19, 22, peak_half=1,
                      ramp=(SLATE_DARK, SLATE_MID, INDIGO_ROYAL, WATER_FOAM), ridge_col=GOLD_SUN)
        # Pennant flag on spire
        fx = tx + 9
        vline(im, fx, 2, 8, GOLD_SUN)
        rect(im, fx + 1, 3, 6, 3, CRIMSON_MID)
        hline(im, fx + 1, fx + 5, 3, GOLD_SUN)
    # Crenellations along curtain wall
    for cx in range(24, 71, 6):
        rect(im, cx, 28, 4, 4, CREAM_SUN if cx < 48 else STONE_WARM)
        rect(im, cx + 1, 30, 3, 2, SLATE_MID)
    # Portcullis archway
    rect(im, 37, 50, 22, 27, SLATE_DARK)
    for y in range(52, 78):
        for x in range(39, 57):
            dist = ((x - 47.5) / 8.5) ** 2 + ((y - 62.0) / 11.0) ** 2
            if y >= 62 or dist <= 1.0:
                grid = (x % 3 == 0) or (y % 4 == 0)
                px(im, x, y, SLATE_MID if grid else INK_DEEP)
    # Royal banners flanking gate
    for bx in (30, 61):
        rect(im, bx, 46, 5, 14, CRIMSON_MID)
        vline(im, bx + 2, 48, 56, GOLD_SUN)
        hline(im, bx, bx + 4, 46, GOLD_SUN)
    sel_outline_in_place(im, INK_DEEP, SLATE_DARK)
    save_png(im, "sprites/castle.png")


def make_tower() -> None:
    # 48x64 — Old Watchtower ruin keep
    im = canvas(48, 64)
    _wall_facade(im, 10, 14, 37, 61, wall_lit=CREAM_SUN, wall_mid=STONE_WARM, wall_sh=SLATE_DARK,
                 stone_base=True, half_timber=False)
    # Crumbled battlements at crown
    for bx, bh in ((9, 5), (17, 6), (25, 4), (33, 6)):
        rect(im, bx, 10 + (6 - bh), 6, bh, CREAM_SUN if bx < 24 else STONE_WARM)
        hline(im, bx, bx + 5, 10 + (6 - bh), CREAM_SUN)
    # Ivy creeping down left stone shoulder
    for ix, iy in ((11, 20), (12, 21), (11, 22), (12, 24), (13, 25), (12, 27), (14, 28), (11, 32), (12, 33)):
        rect(im, ix, iy, 2, 2, LEAF_MID if iy < 26 else LEAF_DARK)
        px(im, ix, iy, LEAF_LIGHT)
    # Arrow slits
    rect(im, 22, 23, 3, 7, INK_DEEP)
    rect(im, 22, 36, 3, 7, INK_DEEP)
    # Heavy stone archway door
    _door(im, 24, 61, w=10, h=14, arched=True)
    sel_outline_in_place(im, INK_DEEP, SLATE_DARK)
    save_png(im, "sprites/tower.png")


def make_gate_and_landmarks() -> None:
    # 1. Village timber-and-stone gate (48x48)
    gt = canvas(48, 48)
    _wall_facade(gt, 4, 24, 13, 45, wall_lit=CREAM_SUN, wall_mid=STONE_WARM, wall_sh=SLATE_MID, half_timber=False)
    _wall_facade(gt, 34, 24, 43, 45, wall_lit=STONE_WARM, wall_mid=SLATE_MID, wall_sh=SLATE_DARK, half_timber=False)
    _shingle_roof(gt, 2, 10, 45, 24, peak_half=16,
                  ramp=(SLATE_DARK, SLATE_MID, SLATE_LIGHT, WATER_FOAM), ridge_col=GOLD_SUN)
    rect(gt, 12, 24, 24, 4, TIMBER_MID)
    hline(gt, 12, 35, 24, OCHRE_LIGHT)
    for lx in (11, 36):
        px(gt, lx, 28, GOLD_SUN)
        px(gt, lx, 29, SUN_WHITE)
    sel_outline_in_place(gt, INK_DEEP, SLATE_DARK)
    save_png(gt, "sprites/gate.png")

    # 2. Expedition pavilion tent (48x48)
    pv = canvas(48, 48)
    # Canopy pitched tent (striped crimson and cream)
    for y in range(12, 44):
        if y < 26:
            t = (y - 12) / 14.0
            hw = int(round(3 + t * 18))
        else:
            hw = 21
        for x in range(24 - hw, 24 + hw + 1):
            stripe = ((x - 4) // 5) & 1
            if y < 26:
                c = (CREAM_SUN if x < 24 else STONE_WARM) if stripe else (CORAL_LIGHT if x < 24 else CRIMSON_MID)
            else:
                c = (STONE_WARM if x > 28 else CREAM_SUN) if stripe else (CRIMSON_DARK if x > 28 else CRIMSON_MID)
            px(pv, x, y, c)
    # Dark interior doorway flap
    for y in range(28, 45):
        hw = (y - 27) // 3 + 2
        for x in range(24 - hw, 24 + hw + 1):
            px(pv, x, y, INK_DEEP if abs(x - 24) < hw - 1 else UMBER_DARK)
    # Center ridge pole & pennant
    vline(pv, 24, 3, 12, GOLD_SUN)
    rect(pv, 25, 4, 7, 4, CRIMSON_MID)
    hline(pv, 25, 31, 4, GOLD_SUN)
    sel_outline_in_place(pv, INK_DEEP, UMBER_DARK)
    save_png(pv, "sprites/pavilion.png")

    # 3. Standing-stone ruin arch (32x32)
    ra = canvas(32, 32)
    rect(ra, 4, 10, 6, 20, STONE_WARM)
    vline(ra, 4, 10, 29, CREAM_SUN)
    vline(ra, 9, 10, 29, SLATE_DARK)
    rect(ra, 22, 10, 6, 20, SLATE_MID)
    vline(ra, 22, 10, 29, STONE_WARM)
    vline(ra, 27, 10, 29, SLATE_DARK)
    rect(ra, 2, 5, 28, 6, STONE_WARM)
    hline(ra, 2, 29, 5, CREAM_SUN)
    hline(ra, 2, 29, 10, SLATE_DARK)
    # Moss patches
    rect(ra, 4, 6, 6, 2, LEAF_LIGHT)
    rect(ra, 20, 7, 5, 3, LEAF_MID)
    rect(ra, 5, 24, 3, 4, LEAF_MID)
    sel_outline_in_place(ra, INK_DEEP, SLATE_DARK)
    save_png(ra, "sprites/ruin_arch.png")

    # 4. Carved rune monument obelisk (32x32)
    mn = canvas(32, 32)
    rect(mn, 8, 25, 16, 5, SLATE_MID)
    hline(mn, 8, 23, 25, CREAM_SUN)
    for y in range(6, 26):
        hw = 3 + (y - 6) // 6
        for x in range(16 - hw, 16 + hw + 1):
            c = CREAM_SUN if x < 14 else (STONE_WARM if x < 18 else SLATE_MID)
            px(mn, x, y, c)
    # Glowing arcane rune carved down center
    for ry in (11, 12, 14, 15, 16, 18, 19):
        px(mn, 16, ry, LAVENDER if ry % 2 else WATER_FOAM)
    px(mn, 15, 15, GOLD_SUN)
    px(mn, 17, 15, GOLD_SUN)
    rect(mn, 11, 22, 3, 3, LEAF_MID)
    sel_outline_in_place(mn, INK_DEEP, SLATE_DARK)
    save_png(mn, "sprites/monument.png")

    # 5. Guardian hero statue (32x32)
    st = canvas(32, 32)
    rect(st, 8, 24, 16, 6, SLATE_MID)
    hline(st, 8, 23, 24, CREAM_SUN)
    rect(st, 10, 21, 12, 4, STONE_WARM)
    # Stone torso, cloak, helm & raised sword silhouette
    rect(st, 12, 11, 8, 10, STONE_WARM)
    vline(st, 12, 11, 20, CREAM_SUN)
    vline(st, 19, 11, 20, SLATE_DARK)
    ell(st, 16, 8, 3.2, 3.0, CREAM_SUN)
    vline(st, 22, 4, 16, SLATE_LIGHT)
    hline(st, 20, 24, 14, GOLD_SHADE)
    sel_outline_in_place(st, INK_DEEP, SLATE_DARK)
    save_png(st, "sprites/statue.png")

    # 6. Village cobblestone well (32x32)
    wl = canvas(32, 32)
    # Cobble ring base
    ell(wl, 16, 24, 11, 5.5, SLATE_MID)
    ell(wl, 16, 22, 10, 4.5, STONE_WARM)
    ell(wl, 16, 22, 7.5, 3.0, WATER_DEEP)
    px(wl, 14, 22, WATER_LIGHT)
    px(wl, 15, 22, WATER_FOAM)
    # Timber roof posts
    rect(wl, 7, 11, 2, 12, TIMBER_MID)
    rect(wl, 23, 11, 2, 12, UMBER_DARK)
    # Rope & hanging bucket
    vline(wl, 16, 12, 20, OCHRE_LIGHT)
    rect(wl, 14, 17, 4, 3, TIMBER_MID)
    # Small slate shingle roof
    _shingle_roof(wl, 4, 3, 27, 12, peak_half=2,
                  ramp=(SLATE_DARK, SLATE_MID, SLATE_LIGHT, WATER_FOAM), ridge_col=CREAM_SUN)
    sel_outline_in_place(wl, INK_DEEP, SLATE_DARK)
    save_png(wl, "sprites/well.png")


def generate_village_structures() -> None:
    make_houses()
    make_shop()
    make_inn()
    make_smithy()
    make_manor()
    make_temple()
    make_castle()
    make_tower()
    make_gate_and_landmarks()


if __name__ == "__main__":
    generate_village_structures()
    print("Generated all village buildings & landmarks")
