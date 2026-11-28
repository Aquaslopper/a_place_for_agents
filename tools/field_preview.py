#!/usr/bin/env python3
"""field_preview.py — compose a field scene from the shipped assets at 1:1
game scale, using the same corner-masked stamping the renderer does.

This is an *offline composite*, not a screenshot: it exists so a change to the
terrain atlas can be judged (and measured) without a GPU. It reads only
assets/tiles and assets/sprites, applies the 8x8-quadrant marching-squares
rule from src/render.cpp::drawGround, layers relief, deco and Y-sorted props,
and writes docs/visual_audit/field_after_1x.png (512x288, the game's canvas).

    python3 tools/field_preview.py
"""
from __future__ import annotations

import os

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
A = os.environ.get("FIELD_ASSETS") or os.path.join(ROOT, "assets")
OUT = os.path.join(ROOT, "docs", "visual_audit")
T = 16
VW, VH = 512, 288

# surface row order, shared with src/render.cpp
DEEP, WATER, BEACH, MEADOW, FOREST, AMBER, HEATH, MIRE, DUNE, SNOW, ICE, ROCK, \
    DIRT, SOIL, COBBLE, COURT, LEAVES, RIPPLES, MESA, FROST, SNOWCRAG = range(21)


def load(path):
    return Image.open(os.path.join(A, path)).convert("RGBA")


LAND = load("tiles/landscape.png")
RELIEF = load("tiles/relief.png")


def inside(mask, x, y):
    """Quadrant coverage for the 16 corner masks (NW, NE, SW, SE)."""
    quad = (mask & 1, mask & 2, mask & 4, mask & 8)
    return bool(quad[(0 if y < 8 else 2) + (0 if x < 8 else 1)])


def stamp(dst, atlas, mask, row, sx, sy):
    src = atlas.crop((mask * T, row * T, mask * T + T, row * T + T))
    if src.getbbox():
        dst.alpha_composite(src, (sx, sy))


def scene():
    W, H = VW // T + 3, VH // T + 3
    surf = [[MEADOW] * (W + 1) for _ in range(H + 1)]
    deco = {}

    def to_tile(x, y):
        return int(x // T) + 1, int(y // T) + 1

    # a pond, a rock outcrop, a dirt lane, a stand of forest
    ponds = [(16.4, 5.2, 3.3), (15.2, 6.6, 2.1)]
    crags = [(25.6, 3.4, 3.0), (7.8, 10.4, 2.2)]
    for ty in range(H):
        for tx in range(W):
            for cx, cy, rad in ponds:
                d = ((tx - cx) ** 2) * 0.55 + (ty - cy) ** 2
                if d < rad * rad:
                    surf[ty][tx] = WATER if d > (rad - 1.1) ** 2 else DEEP
                elif d < (rad + 0.55) ** 2:
                    surf[ty][tx] = BEACH
            for cx, cy, rad in crags:
                d = ((tx - cx) ** 2) * 0.8 + (ty - cy) ** 2
                if d < rad * rad:
                    surf[ty][tx] = ROCK
            # lane: a lazy diagonal from the south-west to the north-east
            lane = abs((ty - 2) - (tx * 0.42)) % 7 < 1.1 and 3 < tx < 28
            if lane and surf[ty][tx] in (MEADOW, FOREST, HEATH):
                surf[ty][tx] = DIRT
            if 20 < tx < 27 and ty < 4 and surf[ty][tx] == MEADOW:
                surf[ty][tx] = FOREST

    img = Image.new("RGBA", (VW, VH), (0, 0, 0, 255))
    # ---------------- ground: corner-masked, exactly like drawGround()
    for ty in range(H):
        for tx in range(W):
            corners = (surf[ty][tx], surf[ty][tx + 1], surf[ty + 1][tx], surf[ty + 1][tx + 1])
            sx, sy = tx * T - T + T // 2, ty * T - T + T // 2
            if len(set(corners)) == 1:
                stamp(img, LAND, 15, corners[0] * 3, sx, sy)
                continue
            masks = [0] * 21
            for i, s in enumerate(corners):
                if i == 0:
                    masks[s] |= 1 | 2 | 4 | 8
                elif i == 1:
                    masks[s] |= 2 | 8
                elif i == 2:
                    masks[s] |= 4 | 8
                else:
                    masks[s] |= 8
            left = 15
            for s in range(21):
                if masks[s]:
                    stamp(img, LAND, left, s * 3, sx, sy)
                    left &= ~masks[s]
            dry = 15 ^ (masks[WATER] | masks[DEEP])
            if dry and dry != 15:
                stamp(img, RELIEF, dry, 0, sx, sy)
            ledge = masks[ROCK] | masks[MESA] | masks[SNOWCRAG]
            if ledge:
                stamp(img, RELIEF, ledge, 2, sx, sy)

    # ---------------- deco, then Y-sorted props
    tuft = [load(f"tiles/tuft_{i}.png") for i in range(3)]
    flower = [load(f"tiles/flower_{i}.png") for i in range(3)]
    pebble = load("tiles/pebble.png")
    random_bits = 0x2545F491
    for ty in range(2, H - 1):
        for tx in range(2, W - 1):
            random_bits = (random_bits * 1103515245 + 12345) & 0x7FFFFFFF
            if surf[ty][tx] not in (MEADOW, FOREST, HEATH):
                continue
            h = (random_bits >> 9) % 100
            sx, sy = (tx - 1) * T, (ty - 1) * T
            if h < 13:
                spr = tuft[h % 3]
                img.alpha_composite(spr, (sx + (T - spr.width) // 2, sy + T - spr.height))
            elif h < 20:
                spr = flower[h % 3]
                img.alpha_composite(spr, (sx + (T - spr.width) // 2, sy + T - spr.height))
            elif h < 23:
                img.alpha_composite(pebble, (sx, sy))

    props = []
    for tx, ty, name in [
        (3, 3, "tree_0"), (4, 4, "tree_1"), (2, 5, "bush"), (5, 2, "tree_2"),
        (21, 2, "pine_0"), (22, 3, "pine_1"), (23, 2, "pine_2"), (24, 4, "tree_1"),
        (11, 9, "rock_0"), (12, 10, "bush"), (9, 6, "bush_plain"),
        (19, 8, "tree_0"), (20, 9, "bush"), (7, 7, "stump"),
        (26, 7, "tree_2"), (27, 9, "bush"), (13, 4, "rock_1"), (14, 5, "bush"),
        (17, 11, "tree_1"), (10, 12, "rock_2"), (6, 11, "bush_plain"),
        (23, 11, "bush"), (25, 12, "tree_0"), (2, 9, "pine_0"),
    ]:
        props.append((ty, tx, name))
    props.sort()
    for ty, tx, name in props:
        spr = load(f"sprites/{name}.png")
        fx = (tx - 1) * T + (T - spr.width) // 2 + (T // 2 if spr.width < T else 0)
        fy = (ty - 1) * T + T - spr.height
        img.alpha_composite(spr, (max(0, fx), max(0, fy)))
    return img


def main():
    os.makedirs(OUT, exist_ok=True)
    im = scene()
    tag = os.environ.get("FIELD_TAG", "after")
    im.convert("RGB").save(os.path.join(OUT, f"field_{tag}_1x.png"))
    im.resize((VW * 2, VH * 2), Image.NEAREST).convert("RGB").save(
        os.path.join(OUT, f"field_{tag}_2x.png"))
    print(f"wrote docs/visual_audit/field_{tag}_1x.png (512x288) and _2x.png")


if __name__ == "__main__":
    main()
