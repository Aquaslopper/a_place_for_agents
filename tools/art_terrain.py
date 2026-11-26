"""art_terrain.py — 16x16 SNES-style terrain tiles.

Hand-placed pixel patterns on 4-colour hue-shifted ramps, with ordered
dithering between ramp steps. Tiles are designed to be seamless when repeated
(patterns wrap at the 16px edge).
"""
from PIL import Image, ImageDraw
import random
from snes import H, mixc, ramp, dither_region, speckle, BAYER4

T = 16

# ---- palettes (darkest -> lightest) --------------------------------------
GRASS   = ramp(H("#4e9a52"))     # meadow
GRASS_DRY = ramp(H("#8d9a4e"), shadow_hue=(-6,-10,20), light_hue=(14,10,-10))
FOREST_FLOOR = ramp(H("#4a8a4a"))
SAND    = ramp(H("#d8bd7a"), shadow_hue=(-4,-2,16), light_hue=(8,6,-6))
DIRT    = ramp(H("#8a6a45"))
SNOW    = ramp(H("#dfe3f0"), shadow_hue=(-2,-4,24), light_hue=(6,6,2))
ICE     = ramp(H("#a8cfe4"), shadow_hue=(-8,-6,20), light_hue=(16,14,8))
WATER   = ramp(H("#2a5a9c"), shadow_hue=(-4,-8,18), light_hue=(18,20,14))
DEEP    = ramp(H("#1b3a6e"), shadow_hue=(-2,-6,14), light_hue=(10,12,10))
ROCK    = ramp(H("#8a8794"), shadow_hue=(-6,-8,16), light_hue=(10,8,-4))
COBBLE  = ramp(H("#9a9689"), shadow_hue=(-4,-6,14), light_hue=(10,8,-6))
OUTLINE = (30, 22, 40)

def _base(col, dither_from=None, amount=0.35):
    img = Image.new("RGBA", (T, T), col + (255,))
    return img

def _wrap_line(d, p0, p1, col):
    """draw a line and its wrapped copies so the tile stays seamless"""
    for ox in (-T, 0, T):
        for oy in (-T, 0, T):
            d.line([(p0[0] + ox, p0[1] + oy), (p1[0] + ox, p1[1] + oy)], fill=col + (255,))


# --------------------------------------------------------------- grass
def grass(variant):
    """Meadow: a calm mid-green field with a few soft blades.

    Earlier tiles were speckled so densely that the field read as confetti at
    1x. SNES fields are deliberately quiet so sprites pop against them.
    """
    img = Image.new("RGBA", (T, T), GRASS[1] + (255,))
    rnd = random.Random(11 + variant)
    # large, low-contrast clumps of the darker tone
    for _ in range(2):
        x0 = rnd.randrange(0, T); y0 = rnd.randrange(0, T)
        w = rnd.randrange(4, 7); h = rnd.randrange(3, 5)
        for yy in range(y0, y0 + h):
            for xx in range(x0, x0 + w):
                if BAYER4[yy & 3][xx & 3] < 6:
                    img.putpixel((xx % T, yy % T), GRASS[0] + (255,))
    # a few blade pairs: dark stem, one light tip
    for _ in range(rnd.randrange(4, 6)):
        x = rnd.randrange(T); y = rnd.randrange(3, T)
        img.putpixel((x, y), GRASS[0] + (255,))
        img.putpixel((x, y - 1), GRASS[2] + (255,))
        if rnd.random() < 0.5:
            img.putpixel((x, y - 2), GRASS[2] + (255,))
    return img

def tuft(variant):
    img = grass(variant).copy()
    d = ImageDraw.Draw(img)
    rnd = random.Random(21 + variant)
    # a proper little shrub tuft: dark base, light tips
    for _ in range(3):
        cx = rnd.randrange(2, 14); cy = rnd.randrange(4, 14)
        d.line([(cx - 1, cy), (cx - 2, cy - 2)], fill=GRASS[0] + (255,))
        d.line([(cx, cy), (cx, cy - 3)], fill=GRASS[1] + (255,))
        d.line([(cx + 1, cy), (cx + 2, cy - 2)], fill=GRASS[2] + (255,))
        d.point((cx, cy - 4), fill=GRASS[3] + (255,))
        d.point((cx - 2, cy - 2), fill=GRASS[3] + (255,))
    return img

def flower(variant):
    img = grass(variant).copy()
    d = ImageDraw.Draw(img)
    rnd = random.Random(31 + variant)
    FLOWERS = [(H("#e86a7a"), H("#ffd7dd")), (H("#ffd95e"), H("#fff6c2")),
               (H("#c98ae8"), H("#f0dcff")), (H("#ffffff"), H("#cfe0ff"))]
    for _ in range(3):
        cx = rnd.randrange(2, 14); cy = rnd.randrange(2, 14)
        petal, hi = FLOWERS[rnd.randrange(len(FLOWERS))]
        d.point((cx, cy - 1), fill=petal + (255,))
        d.point((cx - 1, cy), fill=petal + (255,))
        d.point((cx + 1, cy), fill=petal + (255,))
        d.point((cx, cy + 1), fill=petal + (255,))
        d.point((cx, cy), fill=hi + (255,))
        d.point((cx, cy + 2), fill=GRASS[1] + (255,))
    return img

# --------------------------------------------------------------- ground
def dirt(path=False):
    img = Image.new("RGBA", (T, T), DIRT[2] + (255,))
    d = ImageDraw.Draw(img)
    rnd = random.Random(41)
    for _ in range(5):
        x0 = rnd.randrange(0, T); y0 = rnd.randrange(0, T)
        for xx in range(x0, x0 + rnd.randrange(3, 7)):
            for yy in range(y0, y0 + 2):
                if BAYER4[yy & 3][xx & 3] < 7:
                    img.putpixel((xx % T, yy % T), DIRT[1] + (255,))
    speckle(img, (0, 0, T, T), DIRT[3] + (0,) and [DIRT[3], DIRT[0]], 0.05, 42)
    for _ in range(4):     # pebbles
        x = rnd.randrange(T); y = rnd.randrange(T)
        d.point((x, y), fill=DIRT[3] + (255,))
        d.point((x + 1, y), fill=DIRT[0] + (255,))
    return img

def cobble():
    """Village plaza paving: warm light stone, thin mortar, gentle relief."""
    img = Image.new("RGBA", (T, T), mixc(COBBLE[0], DIRT[1], 0.45) + (255,))
    rnd = random.Random(51)
    rows = [(0, 7), (8, 15)]
    for ri, (y0, y1) in enumerate(rows):
        off = 0 if ri % 2 == 0 else 5
        x = -off
        while x < T:
            w = rnd.choice([7, 8, 9])
            for yy in range(y0 + 1, y1):
                for xx in range(x + 1, x + w):
                    c = COBBLE[2] if (yy - y0) < (y1 - y0) * 0.6 else COBBLE[1]
                    img.putpixel((xx % T, yy % T), c + (255,))
            for xx in range(x + 1, x + w):          # lit top of each sett
                img.putpixel((xx % T, y0 + 1), COBBLE[3] + (255,))
            img.putpixel((x + 2, y1 - 1), COBBLE[0] + (255,))
            x += w
    return img

def sand(path=False):
    img = Image.new("RGBA", (T, T), SAND[2] + (255,))
    d = ImageDraw.Draw(img)
    rnd = random.Random(61)
    for _ in range(3):      # wind ripples
        y = rnd.randrange(T)
        for x in range(T):
            yy = y + (1 if BAYER4[x & 3][y & 3] < 8 else 0)
            img.putpixel((x, yy % T), SAND[3] + (255,))
            img.putpixel((x, (yy + 1) % T), SAND[1] + (255,))
    speckle(img, (0, 0, T, T), [SAND[1], SAND[3]], 0.05, 62)
    return img

def snow(deep=False):
    img = Image.new("RGBA", (T, T), SNOW[2] + (255,))
    rnd = random.Random(71)
    for _ in range(4):      # soft drifts via dithering
        x0 = rnd.randrange(T); y0 = rnd.randrange(T)
        for yy in range(y0, y0 + rnd.randrange(3, 6)):
            for xx in range(x0, x0 + rnd.randrange(4, 9)):
                if BAYER4[yy & 3][xx & 3] < 10:
                    img.putpixel((xx % T, yy % T), SNOW[1] + (255,))
    if not deep:
        speckle(img, (0, 0, T, T), [SNOW[3]], 0.05, 72)
    return img

def ice():
    img = Image.new("RGBA", (T, T), ICE[2] + (255,))
    d = ImageDraw.Draw(img)
    for y in range(T):
        if y % 5 == 0:
            for x in range(T):
                if BAYER4[y & 3][x & 3] < 6:
                    img.putpixel((x, y), ICE[1] + (255,))
    # cracks and a couple of bright glints
    _wrap_line(d, (2, 3), (7, 8), ICE[3]); _wrap_line(d, (7, 8), (13, 6), ICE[3])
    _wrap_line(d, (4, 12), (10, 10), ICE[1]); _wrap_line(d, (10, 10), (15, 13), ICE[1])
    d.point((5, 4), fill=(255, 255, 255, 255)); d.point((11, 12), fill=(255, 255, 255, 255))
    return img

def water(frame):
    """Deep blue with 2-frame animated ripple dashes."""
    img = Image.new("RGBA", (T, T), WATER[2] + (255,))
    for y in range(T):
        if y >= 11:   col = WATER[1]
        elif y >= 5:  col = WATER[2]
        else:         col = mixc(WATER[2], WATER[3], 0.30)
        for x in range(T):
            img.putpixel((x, y), col + (255,))
    d = ImageDraw.Draw(img)
    off = frame * 2
    for row, y in enumerate((4, 9, 14)):
        x = (off + row * 5) % 16
        for k in range(2):
            xx = (x + k * 7) % 16
            d.line([(xx, y), ((xx + 2) % 16, y)], fill=WATER[3] + (255,))
            if row == 1:
                d.point(((xx + 1) % 16, y + 1), fill=WATER[0] + (255,))
    if frame == 1:
        d.point((4, 6), fill=mixc(WATER[3], (255,255,255), 0.5) + (255,))
    return img

def rock_hill():
    """Impassable cliff: three chunky boulders, hard outlines, lit tops."""
    img = Image.new("RGBA", (T, T), ROCK[0] + (255,))
    d = ImageDraw.Draw(img)
    R = ROCK
    d.polygon([(0, 16), (0, 11), (2, 7), (6, 6), (9, 9), (9, 16)], fill=R[2] + (255,))
    d.polygon([(8, 16), (8, 6), (11, 3), (15, 4), (16, 8), (16, 16)], fill=R[1] + (255,))
    d.polygon([(4, 16), (4, 10), (7, 8), (9, 11), (9, 16)], fill=mixc(R[1], R[2], 0.5) + (255,))
    # lit facets
    d.line([(2, 8), (6, 7)], fill=R[3] + (255,))
    d.line([(11, 4), (15, 5)], fill=R[3] + (255,))
    d.line([(5, 11), (7, 9)], fill=R[3] + (255,))
    # deep cracks
    d.line([(9, 9), (8, 16)], fill=(24, 18, 34, 255))
    d.line([(9, 11), (4, 13)], fill=(24, 18, 34, 255))
    d.line([(0, 11), (2, 8)], fill=(24, 18, 34, 255))
    speckle(img, (0, 0, T, T), [R[0], R[3]], 0.04, 81)
    return img

def forest_floor():
    """Darker, leaf-littered grass used under tree canopies."""
    img = Image.new("RGBA", (T, T), FOREST_FLOOR[1] + (255,))
    rnd = random.Random(91)
    for _ in range(5):
        x0 = rnd.randrange(T); y0 = rnd.randrange(T)
        for yy in range(y0, y0 + rnd.randrange(2, 5)):
            for xx in range(x0, x0 + rnd.randrange(3, 6)):
                if BAYER4[yy & 3][xx & 3] < 10:
                    img.putpixel((xx % T, yy % T), FOREST_FLOOR[0] + (255,))
    for _ in range(3):      # scattered leaves
        x = rnd.randrange(T); y = rnd.randrange(T)
        img.putpixel((x, y), H("#6b5a3a") + (255,))
        img.putpixel(((x + 1) % T, y), FOREST_FLOOR[2] + (255,))
    return img

ALL = {
    "grass":   grass, "tuft": tuft, "flower": flower, "dirt": dirt,
    "cobble": cobble, "sand": sand, "snow": snow, "ice": ice,
    "water": water, "rock_hill": rock_hill, "forest_floor": forest_floor,
}
