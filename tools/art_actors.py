#!/usr/bin/env python3
"""Generate all 128x96 (8 cols x 4 rows of 16x24) hero/NPC character sheets
and 32x32 pixel-art dialogue/HUD portraits strictly using the Core Palette.

Sheet layout (16x24 cell):
  Rows: 0=Down, 1=Up, 2=Left (UL lit), 3=Right (UL lit)
  Cols: 0=IdleA, 1=IdleB, 2=Walk1, 3=Walk2, 4=Walk3, 5=Walk4,
        6=Emote (greet/wave/alert), 7=Action (ready/weapon/celebrate)
"""
from __future__ import annotations

from dataclasses import dataclass
from snes import (
    INK_DEEP, SLATE_DARK, SLATE_MID, SLATE_LIGHT, STONE_WARM, CREAM_SUN,
    UMBER_DARK, TIMBER_MID, OCHRE_MID, OCHRE_LIGHT, GOLD_SHADE, GOLD_SUN, SUN_WHITE,
    LEAF_SHADOW, LEAF_DARK, LEAF_MID, LEAF_LIGHT, LEAF_SUN,
    WATER_DEEP, WATER_MID, WATER_LIGHT, WATER_FOAM,
    CRIMSON_DARK, CRIMSON_MID, CORAL_LIGHT,
    NAVY_UI, INDIGO_ROYAL, LAVENDER,
    SKIN_LIGHT, SKIN_WARM, SKIN_DEEP, ROSE_CHEEK,
    canvas, px, rect, hline, vline, ell, sel_outline_in_place, save_png,
)


@dataclass
class CharStyle:
    skin: tuple[int, int, int]
    skin_sh: tuple[int, int, int]
    hair: tuple[int, int, int]
    hair_hi: tuple[int, int, int]
    tunic: tuple[int, int, int]
    tunic_sh: tuple[int, int, int]
    accent: tuple[int, int, int]
    pants: tuple[int, int, int]
    boots: tuple[int, int, int]
    hat: str = "none"       # none, helm, wizard, hood, straw, cap, crownlet, hood_priest, headband, bandana
    weapon: str = "none"    # none, sword_shield, staff, mace, hammer, bow, pack, apron
    beard: bool = False
    skirt: bool = False
    child: bool = False
    cloak: tuple[int, int, int] | None = None


def _draw_humanoid_frame(im, ox: int, oy: int, st: CharStyle, row: int, col: int) -> None:
    """Paint one 16x24 cell inside temporary local 16x24 sprite then outline & blit."""
    cell = canvas(16, 24)
    # Frame kinematics
    bob = 0
    if col == 1:
        bob = 1  # idle breathe settle
    elif col in (3, 5):
        bob = -1  # passing stride crest
    elif col == 6:
        bob = -2  # emote hop

    child_dy = 3 if st.child else 0
    hy = 4 + bob + child_dy  # top of head (inside 16x24)
    if hy < 1:
        hy = 1

    # Walk stride offsets (legs & arms)
    l_leg_dy, r_leg_dy = 0, 0
    l_leg_dx, r_leg_dx = 0, 0
    arm_l_dy, arm_r_dy = 0, 0
    if col == 2:
        l_leg_dy, r_leg_dy = -1, 1
        l_leg_dx, r_leg_dx = -1, 1
        arm_l_dy, arm_r_dy = 1, -1
    elif col == 4:
        l_leg_dy, r_leg_dy = 1, -1
        l_leg_dx, r_leg_dx = 1, -1
        arm_l_dy, arm_r_dy = -1, 1
    elif col == 6:
        arm_r_dy = -3  # raised waving / greeting hand!
    elif col == 7:
        arm_l_dy = -1
        arm_r_dy = -2  # ready stance

    # ---- Direction handling (0=Down, 1=Up, 2=Left, 3=Right)
    is_side = row in (2, 3)
    face_left = (row == 2)
    face_up = (row == 1)

    torso_y = hy + (6 if st.child else 7)
    torso_h = 6 if st.child else 7
    hip_y = torso_y + torso_h

    # 1. Back cloak (when facing up or side)
    if st.cloak and face_up:
        rect(cell, 4, torso_y, 8, torso_h + 3, st.cloak)
        vline(cell, 4, torso_y, torso_y + torso_h + 2, st.tunic)
        vline(cell, 11, torso_y, torso_y + torso_h + 2, st.tunic_sh)

    # 2. Legs / Boots (foot baseline near y=22)
    if not is_side:
        # Front/back view: two distinct boots
        ly = min(22, hip_y + 2 + max(0, l_leg_dy))
        ry = min(22, hip_y + 2 + max(0, r_leg_dy))
        rect(cell, 5, hip_y, 2, max(1, ly - hip_y), st.pants)
        rect(cell, 9, hip_y, 2, max(1, ry - hip_y), st.pants)
        rect(cell, 5, ly - 1, 2, 2, st.boots)
        rect(cell, 9, ry - 1, 2, 2, st.boots)
        if st.skirt:
            rect(cell, 4, hip_y - 1, 8, 3, st.tunic_sh)
            hline(cell, 4, 11, hip_y - 1, st.accent)
    else:
        # Side view (Left/Right): scissoring stride
        step = -2 if col == 2 else (2 if col == 4 else 0)
        bx_f = 7 + step
        bx_b = 7 - step
        rect(cell, bx_b, hip_y, 2, 3, st.tunic_sh)
        rect(cell, bx_b, hip_y + 2, 2, 2, st.boots)
        rect(cell, bx_f, hip_y, 2, 3, st.pants)
        rect(cell, bx_f, hip_y + 2, 2, 2, st.boots)
        if st.skirt:
            rect(cell, 5, hip_y - 1, 6, 3, st.tunic_sh)

    # 3. Torso
    if not is_side:
        rect(cell, 4, torso_y, 8, torso_h, st.tunic)
        vline(cell, 4, torso_y, torso_y + torso_h - 1, st.tunic)
        vline(cell, 11, torso_y, torso_y + torso_h - 1, st.tunic_sh)
        hline(cell, 4, 11, torso_y + torso_h - 2, st.accent)
        if not face_up:
            # Collar / tabard detail
            rect(cell, 7, torso_y, 2, 3, st.accent)
        if st.weapon == "apron" and not face_up:
            rect(cell, 5, torso_y + 2, 6, torso_h - 1, CREAM_SUN)
            hline(cell, 5, 10, torso_y + 2, STONE_WARM)
        if st.cloak and not face_up:
            vline(cell, 4, torso_y, torso_y + torso_h, st.cloak)
            vline(cell, 11, torso_y, torso_y + torso_h, st.tunic_sh)
    else:
        rect(cell, 5, torso_y, 6, torso_h, st.tunic)
        # Upper-left lit regardless of facing!
        vline(cell, 5, torso_y, torso_y + torso_h - 1, st.tunic if face_left else st.accent)
        vline(cell, 10, torso_y, torso_y + torso_h - 1, st.tunic_sh)
        hline(cell, 5, 10, torso_y + torso_h - 2, st.accent)
        if st.cloak:
            cx_b = 10 if face_left else 5
            vline(cell, cx_b, torso_y + 1, torso_y + torso_h + 1, st.cloak)

    # 4. Arms & Held Equipment
    if not is_side:
        rect(cell, 3, torso_y + 1 + arm_l_dy, 1, 4, st.tunic)
        px(cell, 3, torso_y + 5 + arm_l_dy, st.skin)
        if col == 6:
            # Raised greeting arm!
            rect(cell, 12, torso_y - 3, 1, 4, st.tunic)
            px(cell, 12, torso_y - 4, st.skin)
        else:
            rect(cell, 12, torso_y + 1 + arm_r_dy, 1, 4, st.tunic_sh)
            px(cell, 12, torso_y + 5 + arm_r_dy, st.skin_sh)
        # Hero gear silhouettes
        if st.weapon == "sword_shield" and not face_up:
            rect(cell, 2, torso_y + 1, 3, 5, INDIGO_ROYAL)
            hline(cell, 2, 4, torso_y + 1, GOLD_SUN)
            px(cell, 3, torso_y + 3, GOLD_SUN)
            # Sword on right hip / raised on col 7
            if col == 7:
                vline(cell, 13, torso_y - 5, torso_y + 1, CREAM_SUN)
                hline(cell, 12, 14, torso_y + 1, GOLD_SUN)
            else:
                vline(cell, 13, torso_y + 2, torso_y + 6, SLATE_LIGHT)
        elif st.weapon == "staff":
            sx = 13 if col != 7 else 14
            sy = torso_y - (4 if col == 7 else 1)
            vline(cell, sx, sy + 2, torso_y + 7, TIMBER_MID)
            px(cell, sx, sy, LAVENDER)
            px(cell, sx, sy + 1, WATER_FOAM)
        elif st.weapon == "mace":
            sx = 13
            sy = torso_y - (3 if col == 7 else 0)
            vline(cell, sx, sy + 2, torso_y + 6, TIMBER_MID)
            rect(cell, sx - 1, sy, 3, 2, GOLD_SUN)
        elif st.weapon == "hammer":
            vline(cell, 13, torso_y, torso_y + 6, TIMBER_MID)
            rect(cell, 12, torso_y - 1, 3, 2, SLATE_LIGHT)
        elif st.weapon == "bow" and not face_up:
            vline(cell, 2, torso_y - 1, torso_y + 6, OCHRE_MID)
            px(cell, 3, torso_y - 1, CREAM_SUN)
            px(cell, 3, torso_y + 6, CREAM_SUN)
    else:
        ax = 7 + (1 if col == 2 else (-1 if col == 4 else 0))
        if col == 6:
            fx = 4 if face_left else 11
            rect(cell, fx, torso_y - 2, 2, 3, st.tunic)
            px(cell, fx, torso_y - 3, st.skin)
        else:
            rect(cell, ax, torso_y + 1, 2, 4, st.tunic)
            px(cell, ax + (0 if face_left else 1), torso_y + 5, st.skin)
        front_x = 3 if face_left else 12
        if st.weapon == "sword_shield":
            if col == 7:
                vline(cell, front_x, torso_y - 5, torso_y + 2, CREAM_SUN)
                hline(cell, front_x - 1, front_x + 1, torso_y + 2, GOLD_SUN)
            else:
                rect(cell, 6 if face_left else 7, torso_y + 1, 3, 5, INDIGO_ROYAL)
                px(cell, 7 if face_left else 8, torso_y + 3, GOLD_SUN)
        elif st.weapon == "staff":
            sx = front_x
            sy = torso_y - (4 if col == 7 else 1)
            vline(cell, sx, sy + 2, torso_y + 7, TIMBER_MID)
            px(cell, sx, sy, LAVENDER)
            px(cell, sx, sy + 1, WATER_FOAM)
        elif st.weapon == "mace":
            sx = front_x
            sy = torso_y - (3 if col == 7 else 0)
            vline(cell, sx, sy + 2, torso_y + 6, TIMBER_MID)
            rect(cell, sx - 1, sy, 3, 2, GOLD_SUN)

    # 5. Head & Face (width ~6px, height ~6px)
    if not is_side:
        rect(cell, 5, hy, 6, 6, st.skin)
        vline(cell, 10, hy + 1, hy + 5, st.skin_sh)
        if face_up:
            rect(cell, 5, hy, 6, 6, st.hair)
            hline(cell, 5, 9, hy, st.hair_hi)
        else:
            # Hair crown & sideburns
            rect(cell, 5, hy, 6, 2, st.hair)
            hline(cell, 5, 8, hy, st.hair_hi)
            px(cell, 5, hy + 2, st.hair)
            px(cell, 10, hy + 2, st.hair)
            # Crisp 2px pixel eyes
            px(cell, 6, hy + 3, INK_DEEP)
            px(cell, 9, hy + 3, INK_DEEP)
            if col == 1:  # gentle blink / calm eye tick on Idle B
                px(cell, 6, hy + 3, st.skin_sh)
                px(cell, 9, hy + 3, st.skin_sh)
            if st.beard:
                rect(cell, 6, hy + 4, 4, 3, st.hair)
                px(cell, 7, hy + 4, st.hair_hi)
    else:
        rect(cell, 5, hy, 6, 6, st.skin)
        # Upper-left highlight vs back hair
        if face_left:
            rect(cell, 7, hy, 4, 5, st.hair)
            hline(cell, 5, 9, hy, st.hair_hi)
            px(cell, 6, hy + 3, INK_DEEP if col != 1 else st.skin_sh)
            if st.beard:
                rect(cell, 5, hy + 4, 3, 3, st.hair)
        else:
            rect(cell, 5, hy, 4, 5, st.hair)
            hline(cell, 5, 8, hy, st.hair_hi)
            px(cell, 9, hy + 3, INK_DEEP if col != 1 else st.skin_sh)
            if st.beard:
                rect(cell, 8, hy + 4, 3, 3, st.hair)

    # 6. Hat / Helmet Silhouette (key A5 readability!)
    if st.hat == "helm":
        rect(cell, 4, hy - 1, 8, 4, SLATE_LIGHT)
        hline(cell, 4, 9, hy - 1, CREAM_SUN)
        # Plume / crest
        rect(cell, 7, hy - 3, 2, 2, CRIMSON_MID)
        px(cell, 7, hy - 3, CORAL_LIGHT)
        if is_side:
            px(cell, 6 if face_left else 9, hy + 2, INK_DEEP)
    elif st.hat == "wizard":
        # Wide-brimmed pointed mage hat
        hline(cell, 3, 12, hy + 1, INDIGO_ROYAL)
        hline(cell, 4, 11, hy, GOLD_SUN)
        rect(cell, 5, hy - 2, 6, 2, INDIGO_ROYAL)
        rect(cell, 6, hy - 4, 4, 2, INDIGO_ROYAL)
        px(cell, 7 if not face_left else 8, hy - 5, LAVENDER)
    elif st.hat == "hood":
        rect(cell, 4, hy - 1, 8, 3, st.tunic)
        hline(cell, 4, 9, hy - 1, st.accent)
        if face_up or is_side:
            rect(cell, 5 if face_left else 4, hy + 1, 7, 4, st.tunic)
    elif st.hat == "straw":
        hline(cell, 3, 12, hy + 1, OCHRE_LIGHT)
        hline(cell, 4, 11, hy, GOLD_SUN)
        rect(cell, 5, hy - 1, 6, 2, OCHRE_MID)
    elif st.hat == "cap":
        rect(cell, 4, hy - 1, 8, 2, st.accent)
        hline(cell, 4, 9, hy - 1, GOLD_SUN)
    elif st.hat == "crownlet":
        hline(cell, 5, 10, hy - 1, GOLD_SUN)
        px(cell, 5, hy - 2, SUN_WHITE)
        px(cell, 8, hy - 2, GOLD_SUN)
        px(cell, 10, hy - 2, GOLD_SUN)
    elif st.hat == "hood_priest":
        rect(cell, 4, hy - 1, 8, 3, CREAM_SUN)
        hline(cell, 5, 10, hy - 1, GOLD_SUN)
    elif st.hat == "bandana":
        hline(cell, 4, 11, hy, st.accent)
        px(cell, 11 if not is_side or face_left else 4, hy + 1, st.accent)

    # Emote sparkle/alert dot above head on col 6
    if col == 6:
        px(cell, 12, max(1, hy - 3), GOLD_SUN)
        px(cell, 12, max(0, hy - 4), SUN_WHITE)

    sel_outline_in_place(cell, INK_DEEP, SLATE_DARK)
    im.paste(cell, (ox, oy))


def _draw_animal_frame(im, ox: int, oy: int, kind: str, row: int, col: int) -> None:
    """Paint crisp 16x24 quad/bird animal frames for all 4 dirs x 8 cols."""
    cell = canvas(16, 24)
    bob = -1 if col in (2, 4, 6) else (1 if col == 1 else 0)
    by = 14 + bob
    is_side = row in (2, 3)
    left = (row == 2)
    up = (row == 1)
    step = -1 if col == 2 else (1 if col == 4 else 0)

    if kind == "dog":
        body, hi, sh = OCHRE_MID, OCHRE_LIGHT, TIMBER_MID
        if not is_side:
            rect(cell, 5, by, 6, 5, body)
            rect(cell, 5, by - 4, 6, 4, hi)
            px(cell, 4, by - 4, sh)
            px(cell, 11, by - 4, sh)
            if not up:
                px(cell, 6, by - 2, INK_DEEP)
                px(cell, 9, by - 2, INK_DEEP)
                px(cell, 7, by - 1, INK_DEEP)
                hline(cell, 6, 9, by, CRIMSON_MID)  # collar
            rect(cell, 5, by + 4 + max(0, step), 2, 3, sh)
            rect(cell, 9, by + 4 + max(0, -step), 2, 3, sh)
            if col == 6:
                px(cell, 11, by - 1, GOLD_SUN)
        else:
            rect(cell, 4, by + 1, 8, 4, body)
            hline(cell, 4, 11, by + 1, hi)
            hx = 3 if left else 9
            rect(cell, hx, by - 3, 4, 4, hi)
            px(cell, hx + (0 if left else 3), by - 1, INK_DEEP)  # nose
            px(cell, hx + (1 if left else 2), by - 2, INK_DEEP)  # eye
            px(cell, hx + (2 if left else 1), by - 4, sh)        # ear
            tx = 12 if left else 3
            px(cell, tx, by - (1 if col in (1, 6) else 0), body)
            rect(cell, 4 + step, by + 5, 2, 2, sh)
            rect(cell, 10 - step, by + 5, 2, 2, sh)
    elif kind == "cat":
        body, hi, sh = CREAM_SUN, SUN_WHITE, STONE_WARM
        if not is_side:
            rect(cell, 6, by + 1, 4, 4, body)
            rect(cell, 5, by - 3, 6, 4, hi)
            px(cell, 5, by - 4, sh)
            px(cell, 10, by - 4, sh)
            if not up:
                px(cell, 6, by - 1, LEAF_LIGHT)
                px(cell, 9, by - 1, LEAF_LIGHT)
            rect(cell, 6, by + 5, 1, 2, sh)
            rect(cell, 9, by + 5, 1, 2, sh)
        else:
            rect(cell, 5, by + 2, 6, 3, body)
            hx = 4 if left else 8
            rect(cell, hx, by - 2, 4, 4, hi)
            px(cell, hx + (2 if left else 1), by - 3, sh)
            px(cell, hx + (1 if left else 2), by - 1, LEAF_LIGHT)
            tx = 11 if left else 4
            vline(cell, tx, by - 2, by + 2, sh)
            rect(cell, 5 + step, by + 5, 1, 2, sh)
            rect(cell, 9 - step, by + 5, 1, 2, sh)
    elif kind == "chicken":
        body, hi, sh = CREAM_SUN, SUN_WHITE, STONE_WARM
        by += 2
        if not is_side:
            ell(cell, 8, by + 2, 3.2, 2.5, body)
            rect(cell, 6, by - 2, 4, 4, hi)
            px(cell, 7, by - 3, CRIMSON_MID)  # comb
            if not up:
                px(cell, 7, by, GOLD_SUN)  # beak
                px(cell, 6, by - 1, INK_DEEP)
                px(cell, 9, by - 1, INK_DEEP)
            px(cell, 6, by + 5 + max(0, step), GOLD_SHADE)
            px(cell, 9, by + 5 + max(0, -step), GOLD_SHADE)
        else:
            ell(cell, 8, by + 2, 3.5, 2.5, body)
            hx = 5 if left else 9
            rect(cell, hx, by - 2, 3, 4, hi)
            px(cell, hx + 1, by - 3, CRIMSON_MID)
            px(cell, hx + (-1 if left else 3), by, GOLD_SUN)
            px(cell, hx + (0 if left else 2), by - 1, INK_DEEP)
            px(cell, 7 + step, by + 5, GOLD_SHADE)
            px(cell, 9 - step, by + 5, GOLD_SHADE)
    elif kind == "sheep":
        wool, hi, sh = CREAM_SUN, SUN_WHITE, STONE_WARM
        if not is_side:
            ell(cell, 8, by + 2, 4.8, 3.8, wool)
            ell(cell, 7, by + 1, 3.5, 2.5, hi)
            if not up:
                rect(cell, 6, by, 4, 4, SLATE_DARK)
                px(cell, 6, by + 1, CREAM_SUN)
                px(cell, 9, by + 1, CREAM_SUN)
            rect(cell, 5, by + 5 + max(0, step), 2, 3, SLATE_DARK)
            rect(cell, 9, by + 5 + max(0, -step), 2, 3, SLATE_DARK)
        else:
            ell(cell, 8, by + 2, 5.0, 3.6, wool)
            hx = 3 if left else 10
            rect(cell, hx, by, 3, 4, SLATE_DARK)
            px(cell, hx + (1 if left else 1), by + 1, CREAM_SUN)
            rect(cell, 5 + step, by + 5, 2, 3, SLATE_DARK)
            rect(cell, 10 - step, by + 5, 2, 3, SLATE_DARK)
    elif kind == "cow":
        if not is_side:
            rect(cell, 4, by - 1, 8, 7, CREAM_SUN)
            rect(cell, 4, by + 1, 4, 3, TIMBER_MID)
            rect(cell, 5, by - 4, 6, 4, STONE_WARM if up else CREAM_SUN)
            px(cell, 4, by - 5, CREAM_SUN)
            px(cell, 11, by - 5, CREAM_SUN)
            if not up:
                rect(cell, 6, by - 2, 4, 2, ROSE_CHEEK)
                px(cell, 6, by - 3, INK_DEEP)
                px(cell, 9, by - 3, INK_DEEP)
            rect(cell, 4, by + 6 + max(0, step), 2, 2, UMBER_DARK)
            rect(cell, 10, by + 6 + max(0, -step), 2, 2, UMBER_DARK)
        else:
            rect(cell, 3, by, 10, 6, CREAM_SUN)
            rect(cell, 6, by + 1, 4, 3, TIMBER_MID)
            hx = 2 if left else 10
            rect(cell, hx, by - 3, 4, 4, STONE_WARM)
            px(cell, hx + (1 if left else 2), by - 4, CREAM_SUN)
            px(cell, hx + (0 if left else 3), by - 1, ROSE_CHEEK)
            px(cell, hx + (1 if left else 2), by - 2, INK_DEEP)
            rect(cell, 4 + step, by + 6, 2, 2, UMBER_DARK)
            rect(cell, 10 - step, by + 6, 2, 2, UMBER_DARK)
    else:  # horse
        body, mane = OCHRE_MID, UMBER_DARK
        by -= 1
        if not is_side:
            rect(cell, 5, by - 1, 6, 7, body)
            rect(cell, 6, by - 6, 4, 5, OCHRE_LIGHT)
            rect(cell, 7, by - 7, 2, 3, mane)
            if not up:
                px(cell, 6, by - 4, INK_DEEP)
                px(cell, 9, by - 4, INK_DEEP)
            rect(cell, 5, by + 6 + max(0, step), 2, 3, mane)
            rect(cell, 9, by + 6 + max(0, -step), 2, 3, mane)
        else:
            rect(cell, 3, by, 10, 5, body)
            hline(cell, 3, 12, by, OCHRE_LIGHT)
            hx = 2 if left else 10
            rect(cell, hx, by - 5, 4, 6, OCHRE_LIGHT)
            vline(cell, hx + (3 if left else 0), by - 5, by, mane)
            px(cell, hx + (1 if left else 2), by - 3, INK_DEEP)
            tx = 13 if left else 2
            vline(cell, tx, by + 1, by + 5, mane)
            rect(cell, 4 + step, by + 5, 2, 4, mane)
            rect(cell, 10 - step, by + 5, 2, 4, mane)

    if col == 6:
        px(cell, 12, max(1, by - 6), GOLD_SUN)
    sel_outline_in_place(cell, INK_DEEP, UMBER_DARK)
    im.paste(cell, (ox, oy))


def _build_sheet(path: str, st: CharStyle | None = None, animal: str | None = None) -> None:
    im = canvas(128, 96)  # 8 cols x 4 rows of 16x24
    for row in range(4):
        for col in range(8):
            if animal:
                _draw_animal_frame(im, col * 16, row * 24, animal, row, col)
            else:
                _draw_humanoid_frame(im, col * 16, row * 24, st, row, col)
    save_png(im, path)


# ---------------------------------------------------------------------------
# 32x32 Pixel-Art Portraits (Core Palette, Crisp Bust Silhouettes)
# ---------------------------------------------------------------------------
def _draw_portrait(path: str, st: CharStyle | None = None, monster: str | None = None) -> None:
    im = canvas(32, 32)
    if monster is None and st is not None:
        # Bust shoulders & collar
        ell(im, 16, 29, 12, 7, st.tunic)
        ell(im, 14, 27, 9, 5, st.tunic)
        for x in range(18, 28):
            for y in range(24, 31):
                if im.getpixel((x, y))[3] > 0:
                    px(im, x, y, st.tunic_sh)
        # Neck + collar accent
        rect(im, 13, 21, 6, 4, st.skin_sh)
        hline(im, 11, 20, 24, st.accent)
        hline(im, 12, 19, 25, st.accent)
        # Head (12x13 rounded face)
        ell(im, 16, 14, 7.2, 7.8, st.skin)
        for y in range(10, 22):
            for x in range(18, 24):
                if im.getpixel((x, y))[3] > 0:
                    px(im, x, y, st.skin_sh)
        # Eyes + brows + cheek + mouth
        px(im, 12, 13, st.hair)
        px(im, 13, 13, st.hair)
        px(im, 18, 13, st.hair)
        px(im, 19, 13, st.hair)
        rect(im, 12, 14, 2, 2, INK_DEEP)
        rect(im, 18, 14, 2, 2, INK_DEEP)
        px(im, 12, 14, CREAM_SUN)  # catchlight!
        px(im, 18, 14, CREAM_SUN)
        px(im, 11, 16, ROSE_CHEEK)
        px(im, 20, 16, ROSE_CHEEK)
        hline(im, 14, 17, 18, st.skin_sh)
        if st.beard:
            ell(im, 16, 20, 6.2, 4.0, st.hair)
            hline(im, 13, 18, 18, st.hair_hi)
            hline(im, 14, 17, 19, ROSE_CHEEK)
        # Hair crown
        ell(im, 16, 8, 7.6, 3.8, st.hair)
        ell(im, 14, 7, 5.0, 2.2, st.hair_hi)
        rect(im, 9, 9, 2, 6, st.hair)
        rect(im, 21, 9, 2, 6, st.hair)
        # Signature headgear
        if st.hat == "helm":
            ell(im, 16, 8, 8.2, 4.2, SLATE_LIGHT)
            hline(im, 10, 21, 9, CREAM_SUN)
            rect(im, 14, 2, 4, 4, CRIMSON_MID)
            px(im, 15, 2, CORAL_LIGHT)
        elif st.hat == "wizard":
            hline(im, 4, 27, 10, INDIGO_ROYAL)
            hline(im, 7, 24, 9, GOLD_SUN)
            rect(im, 9, 6, 14, 3, INDIGO_ROYAL)
            rect(im, 11, 3, 10, 3, INDIGO_ROYAL)
            rect(im, 14, 1, 5, 2, LAVENDER)
        elif st.hat == "hood":
            ell(im, 16, 10, 9.0, 5.5, st.tunic)
            hline(im, 10, 21, 7, st.accent)
            ell(im, 16, 15, 5.5, 5.5, st.skin)
            rect(im, 12, 14, 2, 2, INK_DEEP)
            rect(im, 18, 14, 2, 2, INK_DEEP)
            px(im, 12, 14, CREAM_SUN)
            px(im, 18, 14, CREAM_SUN)
            hline(im, 14, 17, 18, ROSE_CHEEK)
        elif st.hat == "straw":
            hline(im, 4, 27, 9, OCHRE_LIGHT)
            hline(im, 6, 25, 8, GOLD_SUN)
            rect(im, 9, 5, 14, 3, OCHRE_MID)
        elif st.hat == "crownlet":
            hline(im, 9, 22, 8, GOLD_SUN)
            for cx in (10, 16, 21):
                rect(im, cx, 5, 2, 3, GOLD_SUN)
                px(im, cx, 5, SUN_WHITE)
        elif st.hat == "cap":
            ell(im, 16, 8, 8.0, 3.2, st.accent)
            hline(im, 9, 22, 10, GOLD_SUN)
        elif st.hat == "hood_priest":
            ell(im, 16, 9, 8.5, 4.5, CREAM_SUN)
            hline(im, 10, 21, 7, GOLD_SUN)
    else:
        if monster == "slime":
            ell(im, 16, 20, 10, 8, LEAF_MID)
            ell(im, 14, 17, 7, 5, LEAF_LIGHT)
            px(im, 11, 14, LEAF_SUN)
            # Crown on Slime King portrait
            rect(im, 10, 7, 12, 5, GOLD_SUN)
            rect(im, 10, 4, 2, 3, GOLD_SUN)
            rect(im, 15, 3, 2, 4, SUN_WHITE)
            rect(im, 20, 4, 2, 3, GOLD_SUN)
            px(im, 16, 9, CRIMSON_MID)
            rect(im, 11, 19, 3, 3, INK_DEEP)
            rect(im, 18, 19, 3, 3, INK_DEEP)
            px(im, 11, 19, SUN_WHITE)
            px(im, 18, 19, SUN_WHITE)
        elif monster == "ghost":
            ell(im, 16, 15, 9, 10, WATER_FOAM)
            ell(im, 14, 12, 6, 6, SUN_WHITE)
            rect(im, 11, 13, 3, 3, NAVY_UI)
            rect(im, 18, 13, 3, 3, NAVY_UI)
            px(im, 12, 14, GOLD_SUN)
            px(im, 19, 14, GOLD_SUN)
        elif monster == "skeleton":
            ell(im, 16, 14, 8, 8, CREAM_SUN)
            rect(im, 12, 20, 8, 5, STONE_WARM)
            rect(im, 11, 13, 3, 3, INK_DEEP)
            rect(im, 18, 13, 3, 3, INK_DEEP)
            px(im, 15, 17, INK_DEEP)
            for tx in (13, 15, 17, 19):
                px(im, tx, 22, INK_DEEP)
        elif monster == "demon":
            ell(im, 16, 16, 8.5, 8.5, CRIMSON_MID)
            rect(im, 8, 5, 3, 6, STONE_WARM)
            rect(im, 21, 5, 3, 6, STONE_WARM)
            px(im, 12, 14, GOLD_SUN)
            px(im, 19, 14, GOLD_SUN)
            hline(im, 13, 18, 20, SUN_WHITE)
        else:  # dog
            ell(im, 16, 18, 8.5, 7.5, OCHRE_LIGHT)
            rect(im, 7, 11, 3, 8, TIMBER_MID)
            rect(im, 22, 11, 3, 8, TIMBER_MID)
            rect(im, 11, 15, 2, 2, INK_DEEP)
            rect(im, 19, 15, 2, 2, INK_DEEP)
            px(im, 11, 15, CREAM_SUN)
            px(im, 19, 15, CREAM_SUN)
            ell(im, 16, 20, 3, 2, INK_DEEP)
            px(im, 16, 22, ROSE_CHEEK)

    sel_outline_in_place(im, INK_DEEP, SLATE_DARK)
    save_png(im, path)


def generate_actors() -> None:
    heroes = {
        "knight": CharStyle(SKIN_LIGHT, SKIN_WARM, OCHRE_MID, GOLD_SUN, INDIGO_ROYAL, SLATE_DARK,
                            GOLD_SUN, SLATE_MID, UMBER_DARK, hat="helm", weapon="sword_shield", cloak=CRIMSON_MID),
        "mage":   CharStyle(SKIN_LIGHT, SKIN_WARM, LAVENDER, CREAM_SUN, INDIGO_ROYAL, NAVY_UI,
                            GOLD_SUN, SLATE_DARK, TIMBER_MID, hat="wizard", weapon="staff", cloak=CRIMSON_MID),
        "cleric": CharStyle(SKIN_WARM, SKIN_DEEP, TIMBER_MID, OCHRE_LIGHT, LEAF_MID, LEAF_DARK,
                            GOLD_SUN, CREAM_SUN, TIMBER_MID, hat="hood", weapon="mace", cloak=CREAM_SUN),
    }
    for name, st in heroes.items():
        _build_sheet(f"sprites/hero_{name}.png", st=st)
        _draw_portrait(f"sprites/face_{name}.png", st=st)

    npc_specs: dict[str, tuple[CharStyle, str | None]] = {
        "villager0": (CharStyle(SKIN_LIGHT, SKIN_WARM, TIMBER_MID, OCHRE_LIGHT, LEAF_MID, LEAF_DARK, OCHRE_LIGHT, TIMBER_MID, UMBER_DARK), "villager0"),
        "villager1": (CharStyle(SKIN_WARM, SKIN_DEEP, INK_DEEP, SLATE_MID, CRIMSON_MID, CRIMSON_DARK, CREAM_SUN, SLATE_DARK, UMBER_DARK, hat="cap"), "villager1"),
        "villager2": (CharStyle(SKIN_LIGHT, SKIN_WARM, GOLD_SHADE, GOLD_SUN, WATER_MID, WATER_DEEP, CREAM_SUN, SLATE_MID, TIMBER_MID, skirt=True), "villager2"),
        "villager3": (CharStyle(SKIN_DEEP, UMBER_DARK, INK_DEEP, SLATE_DARK, OCHRE_MID, TIMBER_MID, GOLD_SUN, SLATE_DARK, UMBER_DARK), "villager3"),
        "villager4": (CharStyle(SKIN_LIGHT, SKIN_WARM, CORAL_LIGHT, GOLD_SUN, LEAF_DARK, LEAF_SHADOW, CREAM_SUN, TIMBER_MID, UMBER_DARK, skirt=True), "villager4"),
        "villager5": (CharStyle(SKIN_WARM, SKIN_DEEP, TIMBER_MID, OCHRE_MID, INDIGO_ROYAL, NAVY_UI, WATER_FOAM, SLATE_DARK, TIMBER_MID, weapon="apron"), "villager5"),
        "villager6": (CharStyle(SKIN_LIGHT, SKIN_WARM, STONE_WARM, CREAM_SUN, CRIMSON_MID, CRIMSON_DARK, GOLD_SUN, SLATE_MID, TIMBER_MID, skirt=True, weapon="apron"), "villager6"),
        "villager7": (CharStyle(SKIN_LIGHT, SKIN_WARM, OCHRE_MID, OCHRE_LIGHT, TIMBER_MID, UMBER_DARK, CREAM_SUN, SLATE_DARK, UMBER_DARK, hat="hood_priest"), "villager7"),
        "villager8": (CharStyle(SKIN_WARM, SKIN_DEEP, TIMBER_MID, OCHRE_LIGHT, LEAF_LIGHT, LEAF_MID, GOLD_SUN, TIMBER_MID, UMBER_DARK, child=True), "villager8"),
        "villager9": (CharStyle(SKIN_LIGHT, SKIN_WARM, CREAM_SUN, SUN_WHITE, SLATE_MID, SLATE_DARK, GOLD_SHADE, TIMBER_MID, UMBER_DARK, beard=True), "villager9"),
        "elder":     (CharStyle(SKIN_LIGHT, SKIN_WARM, CREAM_SUN, SUN_WHITE, INDIGO_ROYAL, NAVY_UI, GOLD_SUN, SLATE_DARK, UMBER_DARK, beard=True, weapon="staff"), "elder"),
        "mayor":     (CharStyle(SKIN_WARM, SKIN_DEEP, STONE_WARM, CREAM_SUN, CRIMSON_MID, CRIMSON_DARK, GOLD_SUN, SLATE_DARK, UMBER_DARK, hat="crownlet", beard=True), "mayor"),
        "woman":     (CharStyle(SKIN_LIGHT, SKIN_WARM, CORAL_LIGHT, GOLD_SUN, WATER_MID, WATER_DEEP, CREAM_SUN, SLATE_DARK, TIMBER_MID, skirt=True), "woman"),
        "child":     (CharStyle(SKIN_LIGHT, SKIN_WARM, OCHRE_LIGHT, GOLD_SUN, CRIMSON_MID, CRIMSON_DARK, CREAM_SUN, SLATE_MID, TIMBER_MID, child=True, hat="cap"), "child"),
        "guard":     (CharStyle(SKIN_LIGHT, SKIN_WARM, TIMBER_MID, OCHRE_MID, SLATE_MID, SLATE_DARK, GOLD_SUN, SLATE_DARK, UMBER_DARK, hat="helm", weapon="sword_shield"), "guard"),
        "soldier":   (CharStyle(SKIN_WARM, SKIN_DEEP, INK_DEEP, SLATE_MID, SLATE_MID, SLATE_DARK, CRIMSON_MID, SLATE_DARK, UMBER_DARK, hat="helm", weapon="sword_shield"), None),
        "knight":    (CharStyle(SKIN_LIGHT, SKIN_WARM, GOLD_SHADE, GOLD_SUN, SLATE_LIGHT, SLATE_MID, GOLD_SUN, SLATE_DARK, UMBER_DARK, hat="helm", weapon="sword_shield", cloak=INDIGO_ROYAL), None),
        "merchant":  (CharStyle(SKIN_WARM, SKIN_DEEP, TIMBER_MID, OCHRE_LIGHT, OCHRE_MID, TIMBER_MID, GOLD_SUN, SLATE_DARK, UMBER_DARK, hat="cap", weapon="apron"), "shopkeeper"),
        "smith":     (CharStyle(SKIN_WARM, SKIN_DEEP, TIMBER_MID, OCHRE_MID, TIMBER_MID, UMBER_DARK, CORAL_LIGHT, SLATE_DARK, UMBER_DARK, hat="bandana", weapon="hammer", beard=True), "smith"),
        "priest":    (CharStyle(SKIN_LIGHT, SKIN_WARM, CREAM_SUN, SUN_WHITE, CREAM_SUN, STONE_WARM, GOLD_SUN, SLATE_MID, TIMBER_MID, hat="hood_priest", weapon="mace"), None),
        "farmer":    (CharStyle(SKIN_WARM, SKIN_DEEP, TIMBER_MID, OCHRE_LIGHT, LEAF_MID, LEAF_DARK, OCHRE_LIGHT, TIMBER_MID, UMBER_DARK, hat="straw", weapon="apron"), None),
        "ranger":    (CharStyle(SKIN_DEEP, UMBER_DARK, INK_DEEP, SLATE_MID, LEAF_DARK, LEAF_SHADOW, GOLD_SHADE, TIMBER_MID, UMBER_DARK, hat="hood", weapon="bow", cloak=LEAF_MID), "ranger"),
    }
    for npc_name, (st, face_key) in npc_specs.items():
        _build_sheet(f"sprites/npc_{npc_name}.png", st=st)
        if face_key:
            _draw_portrait(f"sprites/face_{face_key}.png", st=st)

    # Extra dedicated portraits (innkeeper, bard, and monsters/dog)
    _draw_portrait("sprites/face_innkeeper.png", st=npc_specs["mayor"][0])
    _draw_portrait("sprites/face_bard.png", st=CharStyle(
        SKIN_LIGHT, SKIN_WARM, OCHRE_LIGHT, GOLD_SUN, CRIMSON_MID, CRIMSON_DARK,
        GOLD_SUN, SLATE_MID, TIMBER_MID, hat="cap",
    ))
    for animal in ("dog", "cat", "chicken", "sheep", "cow", "horse"):
        _build_sheet(f"sprites/npc_{animal}.png", animal=animal)

    for mon in ("slime", "ghost", "skeleton", "demon", "dog"):
        _draw_portrait(f"sprites/face_{mon}.png", monster=mon)


if __name__ == "__main__":
    generate_actors()
    print("Generated 31 128x96 character sheets and 28 32x32 pixel portraits")
