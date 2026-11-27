#!/usr/bin/env python3
"""Generate all 24 monsters (native 1x field + dedicated native 1x battle sprites),
all 31 16x16 item/equipment icons, and all 7 crisp pixel-art battle FX.
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
    SKIN_LIGHT, SKIN_WARM, SKIN_DEEP, ROSE_CHEEK,
    canvas, px, rect, hline, vline, line, ell, shaded_ell,
    sel_outline_in_place, stamp_palette_sprite, h2, save_png,
)


# ---------------------------------------------------------------------------
# 1. Monster Builders (Native 1x Pixel Density — All Facing Right Toward Party)
# ---------------------------------------------------------------------------
def _draw_slime(size: int, boss: bool = False):
    im = canvas(size, size)
    cx, cy = size // 2, int(size * 0.65)
    rx, ry = int(size * 0.36), int(size * 0.24)
    ramp = [LEAF_SHADOW, LEAF_DARK, LEAF_MID, LEAF_LIGHT, LEAF_SUN]
    if boss:
        ramp = [WATER_DEEP, LEAF_DARK, LEAF_MID, LEAF_LIGHT, LEAF_SUN]
    shaded_ell(im, cx, cy, rx, ry, ramp, seed=4)
    # Wet upper-left sheen blob
    ell(im, cx - rx * 0.35, cy - ry * 0.35, max(2, rx * 0.25), max(1, ry * 0.2), SUN_WHITE)
    # Expressive eyes looking right
    eye_y = cy - 1
    eye_s = 2 if size <= 32 else 3
    rect(im, cx + 1 - eye_s, eye_y, eye_s, eye_s + 1, INK_DEEP)
    rect(im, cx + int(rx * 0.45), eye_y, eye_s, eye_s + 1, INK_DEEP)
    px(im, cx + 1 - eye_s, eye_y, SUN_WHITE)
    px(im, cx + int(rx * 0.45), eye_y, SUN_WHITE)
    if boss:
        # Royal jewelled crown natively at 1x resolution!
        cw = int(size * 0.36)
        ch = int(size * 0.17)
        x0 = cx - cw // 2
        y0 = cy - ry - ch + 2
        rect(im, x0, y0 + ch // 2, cw, ch // 2 + 1, GOLD_SUN)
        hline(im, x0, x0 + cw - 1, y0 + ch, GOLD_SHADE)
        for px_c in (x0, cx - 2, x0 + cw - 3):
            rect(im, px_c, y0, 3, ch // 2 + 1, GOLD_SUN)
            px(im, px_c, y0, SUN_WHITE)
        px(im, cx, y0 + ch - 2, CRIMSON_MID)
        px(im, x0 + 2, y0 + ch - 2, WATER_FOAM)
        px(im, x0 + cw - 3, y0 + ch - 2, WATER_FOAM)
    sel_outline_in_place(im, INK_DEEP, LEAF_SHADOW)
    return im


def _draw_bat(size: int):
    im = canvas(size, size)
    cx, cy = size // 2, size // 2 + 2
    s = size / 32.0
    # Swept leather wings (asymmetric flap)
    for side in (-1, 1):
        for dx in range(1, int(13 * s)):
            wy = cy - int(math.sin(dx / (13.0 * s) * 2.4) * 6 * s)
            wh = max(2, int((6 - dx * 0.35 / s) * s))
            for dy in range(wh):
                col = INDIGO_ROYAL if (dy == 0 and side == -1) else (SLATE_MID if dy < 2 else SLATE_DARK)
                px(im, cx + side * dx, wy + dy, col)
    shaded_ell(im, cx, cy, 4.5 * s, 5.2 * s, [SLATE_DARK, SLATE_MID, INDIGO_ROYAL, LAVENDER])
    # Pointed ears & glowing amber eyes
    rect(im, int(cx - 3 * s), int(cy - 8 * s), max(2, int(2 * s)), max(3, int(3 * s)), SLATE_MID)
    rect(im, int(cx + 2 * s), int(cy - 8 * s), max(2, int(2 * s)), max(3, int(3 * s)), SLATE_DARK)
    px(im, int(cx + 1 * s), int(cy - 1 * s), GOLD_SUN)
    px(im, int(cx + 3 * s), int(cy - 1 * s), GOLD_SUN)
    px(im, int(cx + 2 * s), int(cy + 2 * s), CREAM_SUN)
    sel_outline_in_place(im, INK_DEEP, SLATE_DARK)
    return im


def _draw_imp(size: int):
    im = canvas(size, size)
    cx, by = size // 2, size - 3
    s = size / 32.0
    # Legs, barbed tail, wings
    line(im, cx - 2, by - 7, cx - int(10 * s), by - int(11 * s), CRIMSON_DARK)
    px(im, cx - int(10 * s), by - int(12 * s), CORAL_LIGHT)
    rect(im, cx - int(4 * s), by - int(6 * s), max(2, int(2 * s)), int(6 * s), CRIMSON_DARK)
    rect(im, cx + int(2 * s), by - int(6 * s), max(2, int(2 * s)), int(6 * s), CRIMSON_MID)
    # Torso & head
    shaded_ell(im, cx, by - int(10 * s), 5 * s, 5 * s, [CRIMSON_DARK, CRIMSON_MID, CORAL_LIGHT])
    shaded_ell(im, cx + int(1 * s), by - int(17 * s), 5.5 * s, 5.0 * s, [CRIMSON_DARK, CRIMSON_MID, CORAL_LIGHT])
    # Horns
    line(im, cx - 2, by - int(21 * s), cx - int(5 * s), by - int(26 * s), STONE_WARM)
    line(im, cx + 3, by - int(21 * s), cx + int(5 * s), by - int(26 * s), CREAM_SUN)
    # Eyes + pitchfork
    px(im, cx + int(2 * s), by - int(17 * s), GOLD_SUN)
    px(im, cx + int(4 * s), by - int(17 * s), GOLD_SUN)
    vline(im, cx + int(8 * s), by - int(23 * s), by - 2, TIMBER_MID)
    hline(im, cx + int(6 * s), cx + int(10 * s), by - int(23 * s), SLATE_LIGHT)
    sel_outline_in_place(im, INK_DEEP, CRIMSON_DARK)
    return im


def _draw_hornet(size: int):
    im = canvas(size, size)
    cx, cy = size // 2, size // 2 + 2
    s = size / 32.0
    # Translucent buzzing wings
    ell(im, cx - int(3 * s), cy - int(7 * s), 6 * s, 3 * s, WATER_FOAM)
    ell(im, cx - int(6 * s), cy - int(5 * s), 5 * s, 2.5 * s, WATER_LIGHT)
    # Striped abdomen
    shaded_ell(im, cx - int(5 * s), cy + int(2 * s), 6 * s, 4 * s, [UMBER_DARK, GOLD_SHADE, GOLD_SUN])
    for bx in range(cx - int(9 * s), cx, max(2, int(3 * s))):
        vline(im, bx, cy - 1, cy + int(5 * s), INK_DEEP)
    line(im, cx - int(11 * s), cy + 3, cx - int(14 * s), cy + 4, CREAM_SUN)  # stinger
    # Thorax & head facing right
    shaded_ell(im, cx + int(3 * s), cy, 4 * s, 4 * s, [UMBER_DARK, TIMBER_MID, OCHRE_MID])
    shaded_ell(im, cx + int(7 * s), cy - 1, 3.2 * s, 3.2 * s, [TIMBER_MID, GOLD_SHADE, GOLD_SUN])
    px(im, cx + int(8 * s), cy - 1, CRIMSON_MID)
    sel_outline_in_place(im, INK_DEEP, UMBER_DARK)
    return im


def _draw_wisp(size: int):
    im = canvas(size, size)
    cx, cy = size // 2, size // 2 + 2
    s = size / 32.0
    ell(im, cx, cy - int(2 * s), 8 * s, 9 * s, INDIGO_ROYAL)
    ell(im, cx - int(1 * s), cy - int(4 * s), 6 * s, 7 * s, WATER_MID)
    ell(im, cx, cy, 5 * s, 5 * s, WATER_FOAM)
    ell(im, cx, cy, 3 * s, 3 * s, SUN_WHITE)
    # Orbiting sparks & eerie eyes
    px(im, cx + int(2 * s), cy, NAVY_UI)
    px(im, cx + int(4 * s), cy, NAVY_UI)
    for ox, oy in ((-8, -8), (8, -6), (-7, 6), (9, 5)):
        px(im, int(cx + ox * s), int(cy + oy * s), WATER_FOAM)
    sel_outline_in_place(im, NAVY_UI, WATER_DEEP)
    return im


def _draw_faerie(size: int):
    im = canvas(size, size)
    cx, cy = size // 2, size // 2 + 1
    s = size / 32.0
    # Butterfly wings
    ell(im, cx - int(6 * s), cy - int(4 * s), 5 * s, 4 * s, WATER_FOAM)
    ell(im, cx - int(5 * s), cy + int(2 * s), 4 * s, 3 * s, LAVENDER)
    # Slender woodland tunic & head
    rect(im, cx - int(2 * s), cy - int(2 * s), max(3, int(4 * s)), int(8 * s), LEAF_LIGHT)
    ell(im, cx + int(1 * s), cy - int(6 * s), 3.2 * s, 3.2 * s, SKIN_LIGHT)
    ell(im, cx, cy - int(8 * s), 3.0 * s, 2.0 * s, GOLD_SUN)
    px(im, cx + int(2 * s), cy - int(6 * s), INK_DEEP)
    # Wand sparkle
    line(im, cx + int(3 * s), cy, cx + int(9 * s), cy - int(4 * s), GOLD_SUN)
    px(im, cx + int(10 * s), cy - int(4 * s), SUN_WHITE)
    sel_outline_in_place(im, INK_DEEP, LEAF_SHADOW)
    return im


def _draw_porcupine(size: int):
    im = canvas(size, size)
    cx, by = size // 2, size - 4
    s = size / 32.0
    # Spiky quill fan along back
    for ang in range(-155, -25, 16):
        rad = math.radians(ang)
        ex = int(cx + math.cos(rad) * 12 * s)
        ey = int(by - int(6 * s) + math.sin(rad) * 11 * s)
        line(im, cx - int(2 * s), by - int(6 * s), ex, ey, TIMBER_MID if ang < -90 else OCHRE_MID)
        px(im, ex, ey, CREAM_SUN)
    shaded_ell(im, cx, by - int(5 * s), 8 * s, 5 * s, [UMBER_DARK, TIMBER_MID, OCHRE_MID, OCHRE_LIGHT])
    # Snout facing right
    rect(im, cx + int(6 * s), by - int(6 * s), int(4 * s), int(4 * s), OCHRE_LIGHT)
    px(im, cx + int(9 * s), by - int(5 * s), INK_DEEP)
    px(im, cx + int(7 * s), by - int(6 * s), INK_DEEP)
    rect(im, cx - int(5 * s), by - 2, max(2, int(2 * s)), 2, UMBER_DARK)
    rect(im, cx + int(3 * s), by - 2, max(2, int(2 * s)), 2, UMBER_DARK)
    sel_outline_in_place(im, INK_DEEP, UMBER_DARK)
    return im


def _draw_wolf_or_tiger(size: int, tiger: bool = False):
    im = canvas(size, size)
    cx, by = size // 2, size - 4
    s = size / 48.0
    ramp = [UMBER_DARK, OCHRE_MID, OCHRE_LIGHT, GOLD_SUN] if tiger else [SLATE_DARK, SLATE_MID, SLATE_LIGHT, STONE_WARM]
    # Legs
    for lx in (-10, -4, 4, 10):
        rect(im, int(cx + lx * s), by - int(9 * s), max(2, int(3 * s)), int(9 * s), ramp[0] if lx < 0 else ramp[1])
    # Tail
    line(im, int(cx - 12 * s), by - int(13 * s), int(cx - 20 * s), by - int(18 * s), ramp[1])
    # Body & shoulder mane
    shaded_ell(im, cx - int(1 * s), by - int(12 * s), 12 * s, 6.5 * s, ramp)
    shaded_ell(im, cx + int(7 * s), by - int(14 * s), 7 * s, 6.5 * s, ramp)
    if tiger:
        for sx in range(int(cx - 10 * s), int(cx + 8 * s), max(3, int(4 * s))):
            vline(im, sx, by - int(17 * s), by - int(11 * s), INK_DEEP)
    # Head & jaws facing right
    shaded_ell(im, cx + int(13 * s), by - int(19 * s), 6 * s, 5 * s, ramp)
    rect(im, int(cx + 15 * s), by - int(18 * s), int(6 * s), int(4 * s), ramp[2])
    px(im, int(cx + 11 * s), by - int(24 * s), ramp[2])
    px(im, int(cx + 13 * s), by - int(24 * s), ramp[1])
    px(im, int(cx + 16 * s), by - int(20 * s), GOLD_SUN if tiger else CRIMSON_MID)
    px(im, int(cx + 20 * s), by - int(17 * s), INK_DEEP)
    sel_outline_in_place(im, INK_DEEP, ramp[0])
    return im


def _draw_skeleton_or_reaper(size: int, reaper: bool = False):
    im = canvas(size, size)
    cx, by = size // 2, size - 3
    s = size / 48.0
    if reaper:
        # Floating layered spectral cloak + broad curved death scythe
        for y in range(int(by - 31 * s), by - 2):
            t = (y - (by - 31 * s)) / max(1.0, 29 * s)
            hw = int((6 + t * 11) * s)
            for x in range(cx - hw, cx + hw + 1):
                fold = ((x - cx) + (y >> 2)) % 5 == 0
                c = INDIGO_ROYAL if (x < cx - 2 or fold) else (SLATE_DARK if y > by - 10 else NAVY_UI)
                px(im, x, y, c)
        shaded_ell(im, cx + int(2 * s), by - int(32 * s), 7 * s, 6.5 * s, [NAVY_UI, INDIGO_ROYAL, SLATE_MID])
        ell(im, cx + int(3 * s), by - int(32 * s), 4.5 * s, 4.5 * s, INK_DEEP)
        rect(im, cx + int(3 * s), by - int(33 * s), max(2, int(2 * s)), max(2, int(2 * s)), LAVENDER)
        rect(im, cx + int(6 * s), by - int(33 * s), max(2, int(2 * s)), max(2, int(2 * s)), SUN_WHITE)
        # Heavy curved scythe blade & brass pommel
        line(im, cx + int(10 * s), by - 2, cx + int(14 * s), by - int(38 * s), TIMBER_MID)
        line(im, cx + int(11 * s), by - 2, cx + int(15 * s), by - int(38 * s), OCHRE_MID)
        for dx in range(int(-12 * s), int(2 * s)):
            dy = int((dx / max(1.0, 12 * s)) ** 2 * 6 * s)
            sx = cx + int(14 * s) + dx
            sy = by - int(39 * s) + dy
            rect(im, sx, sy, 2, max(2, int(4 * s)), CREAM_SUN if dy < 3 else SLATE_LIGHT)
    else:
        # Muscular bone frame, brass buckler, curved rusted cutlass
        for lx in (-5, 3):
            rect(im, int(cx + lx * s), by - int(12 * s), max(3, int(3.5 * s)), int(12 * s), STONE_WARM)
        rect(im, int(cx - 7 * s), by - int(25 * s), int(14 * s), int(13 * s), CREAM_SUN)
        for ry in range(by - int(23 * s), by - int(14 * s), max(2, int(3 * s))):
            hline(im, int(cx - 5 * s), int(cx + 5 * s), ry, SLATE_DARK)
        # Shoulder bones & horned iron helm crest
        ell(im, cx - int(8 * s), by - int(24 * s), 3.5 * s, 3 * s, STONE_WARM)
        ell(im, cx + int(8 * s), by - int(24 * s), 3.5 * s, 3 * s, CREAM_SUN)
        shaded_ell(im, cx + int(1 * s), by - int(32 * s), 6.5 * s, 6 * s, [SLATE_MID, STONE_WARM, CREAM_SUN, SUN_WHITE])
        rect(im, cx + int(2 * s), by - int(33 * s), max(2, int(2 * s)), max(2, int(2.5 * s)), INK_DEEP)
        rect(im, cx + int(5 * s), by - int(33 * s), max(2, int(2 * s)), max(2, int(2.5 * s)), INK_DEEP)
        # Broad notched steel blade raised right & round shield left
        shaded_ell(im, cx - int(11 * s), by - int(18 * s), 5 * s, 6.5 * s, [UMBER_DARK, SLATE_MID, STONE_WARM])
        for t in range(int(14 * s)):
            rect(im, cx + int(8 * s) + t, by - int(20 * s) - t, max(2, int(3 * s)), max(2, int(3 * s)),
                 CREAM_SUN if t > 4 else SLATE_LIGHT)
    sel_outline_in_place(im, INK_DEEP, SLATE_DARK)
    return im


def _draw_ghost_or_shade(size: int, shade: bool = False):
    im = canvas(size, size)
    cx, by = size // 2, size - 4
    s = size / 48.0
    ramp = [INK_DEEP, NAVY_UI, INDIGO_ROYAL, LAVENDER] if shade else [WATER_MID, WATER_LIGHT, WATER_FOAM, SUN_WHITE]
    shaded_ell(im, cx, by - int(18 * s), 11 * s, 13 * s, ramp)
    # Wavy tattered spectral tail
    for dx in range(-9, 10, 3):
        rect(im, int(cx + dx * s), by - int(8 * s), max(2, int(3 * s)), int((4 + (abs(dx) % 4)) * s), ramp[1])
    eye = CRIMSON_MID if shade else NAVY_UI
    rect(im, int(cx + 1 * s), by - int(21 * s), max(2, int(3 * s)), max(2, int(3 * s)), eye)
    rect(im, int(cx + 6 * s), by - int(21 * s), max(2, int(3 * s)), max(2, int(3 * s)), eye)
    px(im, int(cx + 2 * s), by - int(20 * s), GOLD_SUN if shade else SUN_WHITE)
    px(im, int(cx + 7 * s), by - int(20 * s), GOLD_SUN if shade else SUN_WHITE)
    sel_outline_in_place(im, INK_DEEP, ramp[0])
    return im


def _draw_goblin_orc_bandit(size: int, kind: str):
    im = canvas(size, size)
    cx, by = size // 2, size - 3
    s = size / 48.0
    if kind == "goblin":
        skin = [LEAF_SHADOW, LEAF_DARK, LEAF_MID, LEAF_LIGHT]
        tunic = TIMBER_MID
        scale = 0.90
    elif kind == "orc":
        skin = [LEAF_SHADOW, LEAF_DARK, LEAF_MID, LEAF_LIGHT]
        tunic = SLATE_DARK
        scale = 1.15
    else:  # bandit
        skin = [SKIN_DEEP, SKIN_WARM, SKIN_LIGHT]
        tunic = CRIMSON_DARK
        scale = 1.04

    ts = s * scale
    for lx in (-5, 2):
        rect(im, int(cx + lx * ts), by - int(10 * ts), max(3, int(4 * ts)), int(10 * ts), UMBER_DARK)
        hline(im, int(cx + lx * ts), int(cx + (lx + 3) * ts), by - 2, TIMBER_MID)
    # Stocky armored torso & brass belt
    rect(im, int(cx - 9 * ts), by - int(24 * ts), int(18 * ts), int(14 * ts), tunic)
    rect(im, int(cx - 9 * ts), by - int(24 * ts), int(8 * ts), int(14 * ts),
         OCHRE_MID if kind == "goblin" else (SLATE_MID if kind == "orc" else CRIMSON_MID))
    hline(im, int(cx - 9 * ts), int(cx + 8 * ts), by - int(14 * ts), GOLD_SHADE)
    shaded_ell(im, cx + int(1 * ts), by - int(30 * ts), 7 * ts, 6 * ts, skin)
    if kind == "bandit":
        rect(im, int(cx - 6 * ts), by - int(35 * ts), int(14 * ts), int(4 * ts), CRIMSON_MID)
        rect(im, int(cx - 2 * ts), by - int(29 * ts), int(9 * ts), int(3 * ts), INK_DEEP)
    elif kind == "goblin":
        for dy in range(3):
            line(im, int(cx - 5 * ts), by - int((31 - dy) * ts), int(cx - 12 * ts), by - int(35 * ts), LEAF_MID)
            line(im, int(cx + 6 * ts), by - int((31 - dy) * ts), int(cx + 13 * ts), by - int(34 * ts), LEAF_LIGHT)
    else:
        rect(im, int(cx + 3 * ts), by - int(27 * ts), max(2, int(2 * ts)), max(2, int(3 * ts)), CREAM_SUN)
        rect(im, int(cx + 7 * ts), by - int(27 * ts), max(2, int(2 * ts)), max(2, int(3 * ts)), CREAM_SUN)
        shaded_ell(im, cx - int(8 * ts), by - int(22 * ts), 5 * ts, 4 * ts, [SLATE_DARK, SLATE_MID, SLATE_LIGHT])
    rect(im, int(cx + 2 * ts), by - int(31 * ts), max(2, int(2 * ts)), max(2, int(2 * ts)),
         GOLD_SUN if kind != "bandit" else CREAM_SUN)
    rect(im, int(cx + 6 * ts), by - int(31 * ts), max(2, int(2 * ts)), max(2, int(2 * ts)),
         GOLD_SUN if kind != "bandit" else CREAM_SUN)
    # Chunky curved cleaver / broadsword held right
    for t in range(int(12 * ts)):
        rect(im, int(cx + 8 * ts) + t, by - int(18 * ts) - t, max(2, int(3.5 * ts)), max(2, int(3.5 * ts)),
             CREAM_SUN if t > 4 else SLATE_LIGHT)
    sel_outline_in_place(im, INK_DEEP, SLATE_DARK)
    return im


def _draw_serpent_or_naga(size: int, naga: bool = False):
    im = canvas(size, size)
    cx, by = size // 2, size - 4
    s = size / 48.0
    ramp = [WATER_DEEP, WATER_MID, WATER_LIGHT, WATER_FOAM] if naga else [LEAF_SHADOW, LEAF_DARK, LEAF_MID, LEAF_SUN]
    # Coiled S-curve tail
    shaded_ell(im, cx - int(3 * s), by - int(6 * s), 13 * s, 5.5 * s, ramp)
    shaded_ell(im, cx - int(5 * s), by - int(12 * s), 9 * s, 4.5 * s, ramp)
    if naga:
        # Upright scaled torso + trident facing right
        rect(im, int(cx - 2 * s), by - int(26 * s), int(9 * s), int(14 * s), ramp[1])
        shaded_ell(im, cx + int(3 * s), by - int(31 * s), 5.5 * s, 5 * s, ramp)
        vline(im, int(cx + 12 * s), by - int(36 * s), by - 5, GOLD_SHADE)
        hline(im, int(cx + 9 * s), int(cx + 15 * s), by - int(35 * s), WATER_FOAM)
    else:
        # Raised hooded cobra neck & fangs facing right
        for dy in range(int(12 * s), int(27 * s)):
            px_x = cx + int(math.sin(dy * 0.25) * 3 * s)
            rect(im, px_x - int(3 * s), by - dy, int(6 * s), 2, ramp[2])
        shaded_ell(im, cx + int(4 * s), by - int(28 * s), 7 * s, 5.5 * s, ramp)
        px(im, cx + int(7 * s), by - int(29 * s), GOLD_SUN)
        line(im, cx + int(10 * s), by - int(27 * s), cx + int(14 * s), by - int(26 * s), CRIMSON_MID)
    sel_outline_in_place(im, INK_DEEP, ramp[0])
    return im


def _draw_golem(size: int):
    im = canvas(size, size)
    cx, by = size // 2, size - 3
    s = size / 48.0
    ramp = [SLATE_DARK, SLATE_MID, SLATE_LIGHT, STONE_WARM]
    rect(im, int(cx - 10 * s), by - int(11 * s), int(7 * s), int(10 * s), SLATE_MID)
    rect(im, int(cx + 3 * s), by - int(11 * s), int(7 * s), int(10 * s), SLATE_DARK)
    shaded_ell(im, cx, by - int(22 * s), 14 * s, 12 * s, ramp)
    # Broad heavy fists
    shaded_ell(im, cx - int(15 * s), by - int(15 * s), 5 * s, 7 * s, ramp)
    shaded_ell(im, cx + int(15 * s), by - int(15 * s), 5 * s, 7 * s, ramp)
    # Glowing amber rune core & visor
    ell(im, cx + int(1 * s), by - int(21 * s), 3.5 * s, 3.5 * s, GOLD_SUN)
    px(im, cx + int(1 * s), by - int(21 * s), SUN_WHITE)
    hline(im, int(cx - 4 * s), int(cx + 5 * s), by - int(30 * s), GOLD_SUN)
    sel_outline_in_place(im, INK_DEEP, SLATE_DARK)
    return im


def _draw_knight_or_rider(size: int, rider: bool = False):
    im = canvas(size, size)
    cx, by = size // 2, size - 3
    s = size / 48.0
    if rider:
        # Spectral steed + horned dread lance knight
        for lx in (-12, -5, 5, 12):
            rect(im, int(cx + lx * s), by - int(11 * s), max(2, int(3 * s)), int(11 * s), NAVY_UI)
        shaded_ell(im, cx, by - int(15 * s), 14 * s, 6.5 * s, [INK_DEEP, NAVY_UI, INDIGO_ROYAL, SLATE_LIGHT])
        shaded_ell(im, cx + int(13 * s), by - int(22 * s), 5 * s, 5 * s, [NAVY_UI, INDIGO_ROYAL, SLATE_LIGHT])
        px(im, cx + int(15 * s), by - int(23 * s), CRIMSON_MID)
        # Rider torso & helm
        rect(im, int(cx - 4 * s), by - int(32 * s), int(9 * s), int(13 * s), SLATE_DARK)
        shaded_ell(im, cx + int(1 * s), by - int(36 * s), 5 * s, 5 * s, [SLATE_DARK, SLATE_MID, SLATE_LIGHT])
        line(im, cx - int(6 * s), by - int(23 * s), cx + int(20 * s), by - int(26 * s), CREAM_SUN)
    else:  # dread_knight
        # Flowing crimson cape behind heavy dread armor
        for y in range(by - int(27 * s), by - int(5 * s)):
            rect(im, int(cx - 13 * s), y, int(10 * s), 2, CRIMSON_DARK if y > by - int(14 * s) else CRIMSON_MID)
        for lx in (-7, 2):
            rect(im, int(cx + lx * s), by - int(12 * s), int(5 * s), int(12 * s), SLATE_DARK)
            hline(im, int(cx + lx * s), int(cx + (lx + 4) * s), by - int(10 * s), SLATE_LIGHT)
        rect(im, int(cx - 10 * s), by - int(27 * s), int(20 * s), int(15 * s), SLATE_MID)
        rect(im, int(cx - 10 * s), by - int(27 * s), int(9 * s), int(15 * s), SLATE_LIGHT)
        shaded_ell(im, cx - int(10 * s), by - int(26 * s), 6 * s, 5 * s, [SLATE_MID, SLATE_LIGHT, CREAM_SUN])
        shaded_ell(im, cx + int(10 * s), by - int(26 * s), 6 * s, 5 * s, [SLATE_DARK, SLATE_MID, SLATE_LIGHT])
        shaded_ell(im, cx, by - int(35 * s), 6.5 * s, 6 * s, [SLATE_DARK, SLATE_MID, SLATE_LIGHT, CREAM_SUN])
        line(im, cx - int(5 * s), by - int(38 * s), cx - int(10 * s), by - int(45 * s), GOLD_SUN)
        line(im, cx + int(5 * s), by - int(38 * s), cx + int(10 * s), by - int(45 * s), GOLD_SUN)
        rect(im, cx + int(1 * s), by - int(35 * s), max(2, int(2 * s)), max(2, int(2 * s)), CRIMSON_MID)
        rect(im, cx + int(4 * s), by - int(35 * s), max(2, int(2 * s)), max(2, int(2 * s)), CRIMSON_MID)
        # Towering dark runeblade
        for t in range(int(18 * s)):
            rect(im, cx + int(9 * s) + int(t * 0.6), by - int(16 * s) - t, max(2, int(3.5 * s)), max(2, int(3.5 * s)),
                 SUN_WHITE if t > 10 else CREAM_SUN)
    sel_outline_in_place(im, INK_DEEP, SLATE_DARK)
    return im


def _draw_drake_or_archfiend(size: int, fiend: bool = False):
    im = canvas(size, size)
    cx, by = size // 2, size - 4
    s = size / 48.0
    ramp = [INK_DEEP, CRIMSON_DARK, CRIMSON_MID, CORAL_LIGHT] if fiend else [CRIMSON_DARK, CRIMSON_MID, CORAL_LIGHT, GOLD_SUN]
    # Sweeping bat/dragon wings
    for side in (-1, 1):
        for dx in range(4, int(19 * s)):
            top_y = by - int((35 - abs(dx - 10) * 0.6) * s)
            bot_y = by - int((20 - dx * 0.3) * s)
            vline(im, int(cx + side * dx * 0.8), top_y, bot_y, INDIGO_ROYAL if fiend else CRIMSON_DARK)
            px(im, int(cx + side * dx * 0.8), top_y, LAVENDER if fiend else CORAL_LIGHT)
    shaded_ell(im, cx, by - int(16 * s), 10 * s, 11 * s, ramp)
    shaded_ell(im, cx + int(5 * s), by - int(29 * s), 6.5 * s, 5.5 * s, ramp)
    # Horns
    line(im, cx + int(1 * s), by - int(33 * s), cx - int(4 * s), by - int(41 * s), STONE_WARM)
    line(im, cx + int(7 * s), by - int(33 * s), cx + int(9 * s), by - int(41 * s), CREAM_SUN)
    px(im, cx + int(7 * s), by - int(30 * s), GOLD_SUN)
    if not fiend:
        # Tail curling left
        line(im, cx - int(8 * s), by - int(10 * s), cx - int(18 * s), by - int(14 * s), CRIMSON_MID)
        px(im, cx - int(19 * s), by - int(15 * s), GOLD_SUN)
    sel_outline_in_place(im, INK_DEEP, CRIMSON_DARK)
    return im


def generate_monsters() -> None:
    small = {"slime", "imp", "bat", "hornet", "wisp", "faerie", "porcupine"}
    all_mon = [
        "slime", "imp", "bat", "hornet", "wisp", "wolf", "skeleton", "ghost",
        "orc", "serpent", "golem", "night_rider", "slime_king",
        "bandit", "goblin", "reaper", "faerie", "drake", "naga", "shade",
        "dread_knight", "archfiend", "tiger", "porcupine",
    ]

    def build(name: str, sz: int):
        if name == "slime":
            return _draw_slime(sz, boss=False)
        if name == "slime_king":
            return _draw_slime(sz, boss=True)
        if name == "bat":
            return _draw_bat(sz)
        if name == "imp":
            return _draw_imp(sz)
        if name == "hornet":
            return _draw_hornet(sz)
        if name == "wisp":
            return _draw_wisp(sz)
        if name == "faerie":
            return _draw_faerie(sz)
        if name == "porcupine":
            return _draw_porcupine(sz)
        if name == "wolf":
            return _draw_wolf_or_tiger(sz, tiger=False)
        if name == "tiger":
            return _draw_wolf_or_tiger(sz, tiger=True)
        if name == "skeleton":
            return _draw_skeleton_or_reaper(sz, reaper=False)
        if name == "reaper":
            return _draw_skeleton_or_reaper(sz, reaper=True)
        if name == "ghost":
            return _draw_ghost_or_shade(sz, shade=False)
        if name == "shade":
            return _draw_ghost_or_shade(sz, shade=True)
        if name in ("goblin", "orc", "bandit"):
            return _draw_goblin_orc_bandit(sz, name)
        if name == "serpent":
            return _draw_serpent_or_naga(sz, naga=False)
        if name == "naga":
            return _draw_serpent_or_naga(sz, naga=True)
        if name == "golem":
            return _draw_golem(sz)
        if name == "night_rider":
            return _draw_knight_or_rider(sz, rider=True)
        if name == "dread_knight":
            return _draw_knight_or_rider(sz, rider=False)
        if name == "drake":
            return _draw_drake_or_archfiend(sz, fiend=False)
        return _draw_drake_or_archfiend(sz, fiend=True)

    for m in all_mon:
        field_sz = 48 if m == "slime_king" else 32
        battle_sz = 64 if m == "slime_king" else (32 if m in small else 48)
        save_png(build(m, field_sz), f"sprites/{m}.png")
        save_png(build(m, battle_sz), f"sprites/{m}_battle.png")


# ---------------------------------------------------------------------------
# 2. All 31 Icons (Strictly 16x16, Core Palette, Crisp 1px Selective Outline)
# ---------------------------------------------------------------------------
ICON_MAPS: dict[str, tuple[list[str], dict[str, tuple[int, int, int]]]] = {
    "potion": ([
        "......cccc......",
        "......tttt......",
        ".......cc.......",
        ".....ccWWcc.....",
        "....cRRRRRRc....",
        "...cRRRRRRRRc...",
        "...cRRRRRRRRc...",
        "...cRRRRddddc...",
        "....cdddddddc...",
        ".....ccccccc....",
    ], {"c": CREAM_SUN, "t": TIMBER_MID, "W": SUN_WHITE, "R": CRIMSON_MID, "d": CRIMSON_DARK}),
    "hi_potion": ([
        "......GGGG......",
        "......tttt......",
        ".......cc.......",
        ".....ccWWcc.....",
        "....cLLLLLLc....",
        "...cLLLLLLLLc...",
        "...cLLLLLLLLc...",
        "...cLLLLddddc...",
        "....cdddddddc...",
        ".....ccccccc....",
    ], {"G": GOLD_SUN, "t": TIMBER_MID, "c": CREAM_SUN, "W": SUN_WHITE, "L": LEAF_LIGHT, "d": LEAF_DARK}),
    "ether": ([
        "......cccc......",
        "......tttt......",
        ".......cc.......",
        ".....ccWWcc.....",
        "....cBBBBBBc....",
        "...cBBBBBBBBc...",
        "...cBBBBBBBBc...",
        "...cBBBBddddc...",
        "....cdddddddc...",
        ".....ccccccc....",
    ], {"c": CREAM_SUN, "t": TIMBER_MID, "W": WATER_FOAM, "B": WATER_LIGHT, "d": INDIGO_ROYAL}),
    "elixir": ([
        "......GGGG......",
        "......tttt......",
        "......GGGG......",
        "....ccWWWWcc....",
        "...cGGGGGGGGc...",
        "...cGGSSSSGGc...",
        "...cGGGGGGGGc...",
        "...cGGddddGGc...",
        "....cdddddddc...",
        ".....ccccccc....",
    ], {"G": GOLD_SUN, "t": TIMBER_MID, "c": CREAM_SUN, "W": SUN_WHITE, "S": CORAL_LIGHT, "d": GOLD_SHADE}),
    "feather": ([
        "..........WW....",
        ".........WGGW...",
        "........WGRRW...",
        ".......WGGRRW...",
        "......WGRRWW....",
        ".....WGRRW......",
        "....WGRRW.......",
        "...WGGRW........",
        "..ttWW..........",
        ".tt.............",
    ], {"W": SUN_WHITE, "G": GOLD_SUN, "R": CRIMSON_MID, "t": GOLD_SHADE}),
    "food": ([
        "......ll........",
        ".....lLLl.......",
        "....RRRRRR......",
        "...RRRRRRRR.....",
        "...RRRRRRdd.....",
        "...RRRRdddd.....",
        "....RRdddd......",
        ".....dddd.......",
    ], {"l": LEAF_LIGHT, "L": LEAF_MID, "R": CRIMSON_MID, "d": CRIMSON_DARK}),
    "book": ([
        "...BBBBBBBB.....",
        "..BCCCCCCCCB....",
        "..BWWWWWWWWB....",
        "..BCCCCCCCCB....",
        "..BGGGGGGGGB....",
        "..BGGGGdddGB....",
        "..BGGGGGGGGb....",
        "..BGGGGGGGGb....",
        "...bbbbbbbb.....",
    ], {"B": INDIGO_ROYAL, "b": NAVY_UI, "C": CREAM_SUN, "W": SUN_WHITE, "G": GOLD_SUN, "d": GOLD_SHADE}),
    "icon_candle": ([
        ".......W........",
        "......GGG.......",
        "......ccc.......",
        "......cWc.......",
        "......cWc.......",
        "......ccc.......",
        "......ccc.......",
        "....GGGGGGG.....",
        ".....ddddd......",
    ], {"W": SUN_WHITE, "G": GOLD_SUN, "c": CREAM_SUN, "d": GOLD_SHADE}),
    "gem": ([
        ".....WWWW.......",
        "...WWBBBBWW.....",
        "..WBBBBBBBBW....",
        "..WBBWWWWBBW....",
        "..WBBBBBBBBW....",
        "...WBBBBBBW.....",
        "....WBBBBW......",
        ".....WBBW.......",
        "......WW........",
    ], {"W": WATER_FOAM, "B": LAVENDER}),
    "coin": ([
        "....GGGGGG......",
        "..GGWWWWGGGG....",
        ".GGWWGGGGGGdG...",
        ".GGWGGGddGGdG...",
        ".GGWGGGddGGdG...",
        ".GGWGGGGGGddG...",
        "..GGddddddGG....",
        "....GGGGGG......",
    ], {"G": GOLD_SUN, "W": SUN_WHITE, "d": GOLD_SHADE}),
    "sword_0": ([
        "..........WW....",
        ".........WSS....",
        "........WSS.....",
        ".......WSS......",
        "......WSS.......",
        "..GG.WSS........",
        "...GGSS.........",
        "...tGG..........",
        "..t..G..........",
    ], {"W": CREAM_SUN, "S": SLATE_LIGHT, "G": OCHRE_MID, "t": TIMBER_MID}),
    "sword_1": ([
        "..........WW....",
        ".........WSS....",
        "........WSS.....",
        ".......WSS......",
        "......WSS.......",
        "..GG.WSS........",
        "...GGGG.........",
        "..tGG...........",
        ".G..............",
    ], {"W": SUN_WHITE, "S": SLATE_LIGHT, "G": GOLD_SUN, "t": INDIGO_ROYAL}),
    "sword_2": ([
        ".........WWW....",
        "........WWBB....",
        ".......WWBB.....",
        "......WWBB......",
        ".....WWBB.......",
        "..GGWWBB........",
        "...GGGG.........",
        "..tGG...........",
        ".G..............",
    ], {"W": SUN_WHITE, "B": WATER_FOAM, "G": GOLD_SUN, "t": CRIMSON_MID}),
    "armor_0": ([
        "..tt....tt......",
        ".tttttttttt.....",
        ".tOOOOOOOOt.....",
        "..tOOOOOOt......",
        "..tOOddOOt......",
        "..tOOOOOOt......",
        "..tddddddt......",
        "..tttttttt......",
    ], {"t": TIMBER_MID, "O": OCHRE_MID, "d": UMBER_DARK}),
    "armor_1": ([
        "..SS....SS......",
        ".SWWSSSSWWS.....",
        ".SWSSSSSSWS.....",
        "..SSSSSSSS......",
        "..SSSddSSS......",
        "..SSSSSSSS......",
        "..SddddddS......",
        "..SSSSSSSS......",
    ], {"S": SLATE_MID, "W": CREAM_SUN, "d": SLATE_DARK}),
    "armor_2": ([
        "..SS....SS......",
        ".SWWGGGGWWS.....",
        ".SWSSSSSSWS.....",
        "..SSGGGGSS......",
        "..SSSSSSSS......",
        "..SddSSddS......",
        "..SddddddS......",
        "..SSSSSSSS......",
    ], {"S": SLATE_LIGHT, "W": SUN_WHITE, "G": GOLD_SUN, "d": SLATE_MID}),
    "armor_3": ([
        "..GG....GG......",
        ".GWWGGGGWWG.....",
        ".GRRRRRRRRG.....",
        "..RGGGGGGGR.....",
        "..RRRRRRRRR.....",
        "..RddGGddRR.....",
        "..RddddddRR.....",
        "..GGGGGGGG......",
    ], {"G": GOLD_SUN, "W": SUN_WHITE, "R": CRIMSON_MID, "d": CRIMSON_DARK}),
    "shield": ([
        "..GGGGGGGG......",
        ".GWWWWWWWWG.....",
        ".GWBBBBBBWG.....",
        ".GWBGGGGGBG.....",
        ".GWBGGGGGBG.....",
        "..WBBBBBBW......",
        "...WBBBBW.......",
        "....WBBW........",
        ".....WW.........",
    ], {"G": GOLD_SUN, "W": CREAM_SUN, "B": INDIGO_ROYAL}),
    "ring": ([
        ".....BB.........",
        "....BWWB........",
        "....GGGG........",
        "...GG..GG.......",
        "...G....G.......",
        "...GG..GG.......",
        "....GGGG........",
        ".....dd.........",
    ], {"B": WATER_FOAM, "W": SUN_WHITE, "G": GOLD_SUN, "d": GOLD_SHADE}),
    "key": ([
        "...GGGG.........",
        "..GWWWWG........",
        "..GW..WG........",
        "...GGGGGGGGGG...",
        ".........G..G...",
        ".........G..G...",
    ], {"G": GOLD_SUN, "W": SUN_WHITE}),
    "crown": ([
        ".W....W....W....",
        ".G.W..G..W.G....",
        ".G.G.GGG.G.G....",
        ".GGGGGGGGGGG....",
        ".GGRGGBGGRGG....",
        ".ddddddddddd....",
    ], {"W": SUN_WHITE, "G": GOLD_SUN, "R": CRIMSON_MID, "B": WATER_FOAM, "d": GOLD_SHADE}),
    "staff": ([
        ".........WW.....",
        "........WBBW....",
        "........WBBW....",
        ".......ttWW.....",
        "......tt........",
        ".....tt.........",
        "....tt..........",
        "...tt...........",
        "..tt............",
    ], {"W": SUN_WHITE, "B": LAVENDER, "t": TIMBER_MID}),
    "axe": ([
        ".......WWW......",
        "......WSSSW.....",
        "......WSSSW.....",
        ".......tWW......",
        "......tt........",
        ".....tt.........",
        "....tt..........",
        "...tt...........",
    ], {"W": CREAM_SUN, "S": SLATE_LIGHT, "t": TIMBER_MID}),
    "spear": ([
        "..........WW....",
        ".........WSS....",
        "........WSS.....",
        ".......tt.......",
        "......tt........",
        ".....tt.........",
        "....tt..........",
        "...tt...........",
    ], {"W": SUN_WHITE, "S": SLATE_LIGHT, "t": TIMBER_MID}),
    "bow": ([
        "....tt..........",
        "...t..C.........",
        "..t....C........",
        "..t....C........",
        "..t....C........",
        "...t..C.........",
        "....tt..........",
    ], {"t": OCHRE_LIGHT, "C": CREAM_SUN}),
    "mace": ([
        "........GGG.....",
        ".......GWWWG....",
        ".......GGGG.....",
        "......ttGG......",
        ".....tt.........",
        "....tt..........",
        "...tt...........",
    ], {"G": GOLD_SUN, "W": SUN_WHITE, "t": TIMBER_MID}),
    "boots": ([
        "...tt......tt...",
        "...tO......tO...",
        "...tO......tO...",
        "..ttOO....ttOO..",
        ".ddddd...ddddd..",
    ], {"t": TIMBER_MID, "O": OCHRE_LIGHT, "d": UMBER_DARK}),
    "robe": ([
        "....GGGG........",
        "..BBBBBBBB......",
        ".BBWWWWWWBB.....",
        "..BBBBBBBB......",
        "..BBBddBBB......",
        "..BBBBBBBB......",
        ".BBBBddBBBB.....",
        ".dddddddddd.....",
    ], {"G": GOLD_SUN, "B": INDIGO_ROYAL, "W": LAVENDER, "d": NAVY_UI}),
    "remedy": ([
        ".....ll.........",
        "....lLLl........",
        "...llllll.......",
        "...GGGGGG.......",
        "..GWWBBBBG......",
        "..GBBBBBBG......",
        "...GGGGGG.......",
    ], {"l": LEAF_SUN, "L": LEAF_MID, "G": CREAM_SUN, "W": SUN_WHITE, "B": WATER_LIGHT}),
    "bomb": ([
        "........WG......",
        ".......t........",
        ".....SSSS.......",
        "...SSWWWWSS.....",
        "..SSSWWWSSSS....",
        "..SSSSSSSSSS....",
        "...SSSSSSSS.....",
        "....dddddd......",
    ], {"W": SUN_WHITE, "G": GOLD_SUN, "t": OCHRE_MID, "S": SLATE_DARK, "d": INK_DEEP}),
    "star_metal": ([
        "......WW........",
        ".....WBBW.......",
        "..WWWBBBBWWW....",
        "...WBBBBBBW.....",
        "....WBBBBW......",
        "...WBW..WBW.....",
        "..WW......WW....",
    ], {"W": SUN_WHITE, "B": WATER_FOAM}),
}


def generate_icons() -> None:
    for name, (grid, cmap) in ICON_MAPS.items():
        im = canvas(16, 16)
        ox = max(0, (16 - len(grid[0])) // 2)
        oy = max(0, (16 - len(grid)) // 2)
        stamp_palette_sprite(im, grid, cmap, ox, oy)
        sel_outline_in_place(im, INK_DEEP, SLATE_DARK)
        save_png(im, f"icons/{name}.png")


# ---------------------------------------------------------------------------
# 3. Battle FX (Pure Crisp Pixel Art — Zero Blurry Alpha Gradients)
# ---------------------------------------------------------------------------
def generate_fx() -> None:
    # 1. Slash (32x32)
    sl = canvas(32, 32)
    for t in range(28):
        x = 3 + t
        y = 27 - int((t / 27.0) ** 1.2 * 22)
        px(sl, x, y, SUN_WHITE)
        px(sl, x - 1, y + 1, WATER_FOAM)
        if 4 < t < 24:
            px(sl, x, y + 1, CREAM_SUN)
            px(sl, x - 2, y + 2, WATER_LIGHT)
    sel_outline_in_place(sl, NAVY_UI, WATER_DEEP)
    save_png(sl, "fx/slash.png")

    # 2. Impact (32x32)
    imp = canvas(32, 32)
    ell(imp, 16, 16, 5, 5, GOLD_SUN)
    ell(imp, 16, 16, 3, 3, SUN_WHITE)
    for ang in range(0, 360, 45):
        rad = math.radians(ang)
        line(imp, 16, 16, int(16 + math.cos(rad) * 12), int(16 + math.sin(rad) * 12), GOLD_SUN)
        px(imp, int(16 + math.cos(rad) * 13), int(16 + math.sin(rad) * 13), SUN_WHITE)
    sel_outline_in_place(imp, CRIMSON_DARK, UMBER_DARK)
    save_png(imp, "fx/impact.png")

    # 3. Poof (32x32)
    pf = canvas(32, 32)
    for cx, cy, rx, ry in ((11, 18, 6, 5), (20, 17, 6, 5), (16, 12, 7, 6)):
        shaded_ell(pf, cx, cy, rx, ry, [SLATE_MID, SLATE_LIGHT, CREAM_SUN, SUN_WHITE])
    sel_outline_in_place(pf, SLATE_DARK, SLATE_MID)
    save_png(pf, "fx/poof.png")

    # 4. Spark (16x16)
    sp = canvas(16, 16)
    hline(sp, 2, 13, 8, GOLD_SUN)
    vline(sp, 8, 2, 13, GOLD_SUN)
    rect(sp, 7, 7, 3, 3, SUN_WHITE)
    for dx, dy in ((-3, -3), (3, -3), (-3, 3), (3, 3)):
        px(sp, 8 + dx, 8 + dy, CORAL_LIGHT)
    save_png(sp, "fx/spark.png")

    # 5. Sparkle (16x16)
    sk = canvas(16, 16)
    hline(sk, 3, 12, 8, WATER_FOAM)
    vline(sk, 8, 3, 12, WATER_FOAM)
    rect(sk, 7, 7, 3, 3, SUN_WHITE)
    for dx, dy in ((-4, -4), (4, -4), (-4, 4), (4, 4)):
        px(sk, 8 + dx, 8 + dy, LAVENDER)
    save_png(sk, "fx/sparkle.png")

    # 6. Fireball (16x16)
    fb = canvas(16, 16)
    shaded_ell(fb, 9, 8, 5, 4.5, [CRIMSON_MID, CORAL_LIGHT, GOLD_SUN, SUN_WHITE])
    for tx, ty in ((2, 7), (3, 9), (4, 6), (4, 10)):
        px(fb, tx, ty, CRIMSON_MID)
    sel_outline_in_place(fb, CRIMSON_DARK, UMBER_DARK)
    save_png(fb, "fx/fireball.png")

    # 7. Arrow (16x16)
    ar = canvas(16, 16)
    hline(ar, 2, 12, 8, TIMBER_MID)
    rect(ar, 11, 7, 3, 3, SLATE_LIGHT)
    px(ar, 14, 8, CREAM_SUN)
    px(ar, 2, 7, CRIMSON_MID)
    px(ar, 2, 9, CRIMSON_MID)
    sel_outline_in_place(ar, INK_DEEP, UMBER_DARK)
    save_png(ar, "fx/arrow.png")


if __name__ == "__main__":
    generate_monsters()
    generate_icons()
    generate_fx()
    print("Generated 48 monster sprites, 31 icons, and 7 FX")
