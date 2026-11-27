"""art_extra.py — generate ONLY the new content assets (dungeon & village 2.0).

Unlike build_art.py this never rewrites existing art: it adds the cave mouth,
dungeon stair tiles, the blacksmith's forge exterior and the new item icons
that arrived with the dungeon / equipment expansion.

    python3 tools/art_extra.py
"""
import os
import random
import sys

from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from snes import H, mixc, ramp, outline, BAYER4  # noqa: E402
import art_objects as AO  # noqa: E402

OUTL = (26, 20, 40)
ROOT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "assets")


def canvas(w, h):
    return Image.new("RGBA", (w, h), (0, 0, 0, 0))


def put(img, x, y, c):
    if 0 <= x < img.width and 0 <= y < img.height:
        img.putpixel((x, y), c + (255,) if len(c) == 3 else c)


def save(img, sub, name):
    path = os.path.join(ROOT, sub, name)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    img.save(path)
    print("wrote", os.path.relpath(path, os.path.dirname(ROOT)))


# ================================================================== cave mouth
def cave_mouth():
    """48x40 overworld cavern entrance: rocky mound, dark arch, lantern post."""
    w, h = 48, 40
    img = canvas(w, h)
    d = ImageDraw.Draw(img)
    ROCK = ramp(H("#8e8b96"), shadow_hue=(-12, -18, 24), light_hue=(10, 8, -12))
    ROCK_D = ramp(H("#6f6c7a"))
    DARK = (14, 10, 20)

    # --- grass skirt along the bottom so it beds into the turf
    for x in range(2, w - 2):
        put(img, x, h - 2, H("#3f7a38"))
        put(img, x, h - 1, H("#2f5f2c"))

    # --- mound silhouette: stacked rock lumps
    d.ellipse([1, 8, 30, 39], fill=ROCK[1] + (255,))
    d.ellipse([16, 3, 47, 39], fill=ROCK[1] + (255,))
    d.ellipse([6, 14, 44, 40], fill=ROCK[1] + (255,))
    # lit crowns (light from upper-left)
    d.ellipse([5, 9, 26, 26], fill=ROCK[2] + (255,))
    d.ellipse([8, 11, 22, 21], fill=ROCK[3] + (255,))
    d.ellipse([26, 6, 45, 24], fill=ROCK[2] + (255,))
    d.ellipse([30, 8, 42, 18], fill=ROCK[3] + (255,))

    # --- dark arch opening, centred low
    ax0, ax1 = 15, 34
    ay1 = 38
    d.polygon([(ax0, ay1), (ax0, 24), (ax0 + 2, 18), ((ax0 + ax1) // 2, 15),
               (ax1 - 2, 18), (ax1, 24), (ax1, ay1)], fill=DARK + (255,))
    # arch rim highlight
    for i, x in enumerate(range(ax0 - 1, (ax0 + ax1) // 2 + 1)):
        t = i / max(1, (ax1 - ax0) // 2)
        y = int(24 - 8 * (1 - (2 * t - 1) ** 2))
        put(img, x, y, ROCK[3])
        put(img, x, y + 1, ROCK[2])
    for x in range((ax0 + ax1) // 2, ax1 + 1):
        t = (x - (ax0 + ax1) // 2) / max(1, (ax1 - ax0) // 2)
        y = int(16 + 8 * t * t)
        put(img, x, y, ROCK[2])
    # inner glow: faint warm lantern light on the cave floor
    for y in range(31, 38):
        for x in range(ax0 + 2, ax1 - 1):
            dist = abs(x - (ax0 + ax1) // 2) / ((ax1 - ax0) * 0.5)
            if dist < 0.75 and img.getpixel((x, y))[3]:
                put(img, x, y, mixc(DARK, (210, 150, 70), 0.28 * (1 - dist)))

    # --- boulders flanking the mouth
    for (cx, cy, r) in ((7, 33, 5), (41, 34, 4), (11, 27, 3)):
        d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=ROCK_D[2] + (255,))
        d.ellipse([cx - r + 1, cy - r, cx + r - 1, cy - 1], fill=ROCK_D[3] + (255,))

    # --- lantern post beside the entrance (call to adventure)
    px_, py_ = 40, 20
    for y in range(py_, 38):
        put(img, px_, y, H("#6b4a2a"))
        put(img, px_ + 1, y, H("#543a22"))
    d.rectangle([px_ - 3, py_ - 6, px_ + 4, py_ - 1], fill=H("#4a4458") + (255,))
    d.rectangle([px_ - 2, py_ - 5, px_ + 3, py_ - 2], fill=H("#f0c060") + (255,))
    put(img, px_ - 1, py_ - 4, H("#ffe8a0"))
    d.line([(px_ - 4, py_ - 7), (px_ + 5, py_ - 7)], fill=H("#3a3448") + (255,))

    return outline(img, OUTL)


# ================================================================== stairs
def stair_down():
    """16x16 floor tile: dark well with stone steps spiralling down."""
    img = canvas(16, 16)
    d = ImageDraw.Draw(img)
    STONE = ramp(H("#8e8b96"))
    d.rectangle([1, 1, 14, 14], fill=STONE[1] + (255,))
    d.rectangle([3, 3, 12, 12], fill=(12, 10, 18, 255))
    # descending steps (darker as they go down)
    steps = [(3, 4, 12, 6, STONE[2]), (4, 6, 11, 8, STONE[1]),
             (5, 8, 10, 10, mixc(STONE[1], (0, 0, 0), 0.35)),
             (6, 10, 9, 12, mixc(STONE[1], (0, 0, 0), 0.6))]
    for (x0, y0, x1, y1, c) in steps:
        d.rectangle([x0, y0, x1, y1], fill=c + (255,))
        d.line([(x0, y0), (x1, y0)], fill=mixc(c, (255, 255, 255), 0.35) + (255,))
    # iron handrail
    d.line([(2, 3), (2, 13)], fill=H("#55505e") + (255,))
    d.line([(13, 3), (13, 13)], fill=H("#45404e") + (255,))
    return outline(img, OUTL, keep_inside=True)


def stair_up():
    """16x16 floor tile: stone steps climbing back to daylight."""
    img = canvas(16, 16)
    d = ImageDraw.Draw(img)
    STONE = ramp(H("#9b98a4"))
    d.rectangle([1, 1, 14, 14], fill=STONE[1] + (255,))
    # bright sky slot at the top
    d.rectangle([3, 1, 12, 4], fill=H("#bfe0f0") + (255,))
    d.rectangle([3, 1, 12, 2], fill=H("#e8f4ff") + (255,))
    # steps coming toward the viewer
    steps = [(4, 4, 11, 7, STONE[3]), (3, 7, 12, 10, STONE[2]), (2, 10, 13, 14, STONE[1])]
    for (x0, y0, x1, y1, c) in steps:
        d.rectangle([x0, y0, x1, y1], fill=c + (255,))
        d.line([(x0, y0), (x1, y0)], fill=mixc(c, (255, 255, 255), 0.4) + (255,))
    d.line([(2, 4), (2, 14)], fill=H("#55505e") + (255,))
    d.line([(13, 4), (13, 14)], fill=H("#45404e") + (255,))
    return outline(img, OUTL, keep_inside=True)


# ================================================================== smithy
def smithy():
    """70x61 forge exterior: dark slate roof, stone walls, chimney smoke, anvil sign."""
    w, h = 70, 61
    img = canvas(w, h)
    d = ImageDraw.Draw(img)
    SLATE = ramp(H("#4a4f66"), shadow_hue=(-8, -12, 18), light_hue=(8, 8, -10))
    STONE_W = ramp(H("#b9b0a0"), shadow_hue=(-8, -12, 20), light_hue=(8, 6, -8))
    WOOD = AO.WOOD
    WOOD_D = AO.WOOD_D

    # body
    AO._house_common(img, w, h, STONE_W, SLATE, SLATE, WOOD_D, windows=False)

    # chimney with smoke puffs
    d.rectangle([46, 6, 56, 24], fill=STONE_W[2] + (255,))
    d.rectangle([44, 4, 58, 8], fill=STONE_W[3] + (255,))
    for (sx, sy, r) in ((52, 2, 4), (57, -1 + 3, 3), (49, 0, 2)):
        d.ellipse([sx - r, sy - r, sx + r, sy + r], fill=(210, 210, 220, 160))

    # forge glow in the doorway
    dx0, dx1 = w // 2 - 6, w // 2 + 6
    base_y = h - 1
    d.rectangle([dx0, base_y - 18, dx1, base_y], fill=(30, 22, 24, 255))
    d.rectangle([dx0 + 2, base_y - 10, dx1 - 2, base_y - 1], fill=H("#e8862c") + (255,))
    d.rectangle([dx0 + 3, base_y - 8, dx1 - 3, base_y - 3], fill=H("#f8c040") + (255,))
    put(img, w // 2, base_y - 6, H("#ffe8a0"))

    # wide anvil window to the left of the door
    d.rectangle([9, 32, 27, 46], fill=H("#3a3448") + (255,))
    d.rectangle([10, 33, 26, 45], fill=H("#e0a040") + (255,))
    # anvil silhouette inside the window
    d.rectangle([13, 39, 23, 41], fill=(30, 24, 34, 255))
    d.rectangle([15, 41, 21, 44], fill=(30, 24, 34, 255))
    d.rectangle([12, 38, 16, 39], fill=(30, 24, 34, 255))
    d.line([(18, 33), (18, 45)], fill=WOOD_D[1] + (255,))

    # hanging anvil sign on the right
    d.rectangle([40, 26, 58, 36], fill=WOOD[2] + (255,))
    d.rectangle([40, 26, 58, 28], fill=WOOD[3] + (255,))
    d.line([(49, 26), (49, 23)], fill=H("#c8b070") + (255,))
    # tiny anvil pictogram
    d.rectangle([44, 31, 54, 33], fill=(40, 34, 46, 255))
    d.rectangle([47, 33, 51, 35], fill=(40, 34, 46, 255))
    d.rectangle([43, 30, 47, 31], fill=(40, 34, 46, 255))

    # barrels & sparks by the door
    for bx in (6, 62):
        d.rectangle([bx, 50, bx + 6, 59], fill=AO.WOOD[2] + (255,))
        d.line([(bx, 53), (bx + 6, 53)], fill=(70, 50, 34, 255))
        d.line([(bx, 57), (bx + 6, 57)], fill=(70, 50, 34, 255))

    return outline(img, OUTL)


# ================================================================== icons
def icon_staff():
    """16x16 oak staff with a knotted head."""
    img = canvas(16, 16)
    d = ImageDraw.Draw(img)
    W = ramp(H("#a8763f"))
    for y in range(3, 15):
        put(img, 7, y, W[2])
        put(img, 8, y, W[1])
    d.ellipse([5, 1, 10, 6], fill=H("#6f9c58") + (255,))
    d.ellipse([6, 2, 9, 4], fill=H("#9cc878") + (255,))
    put(img, 4, 5, W[2])
    put(img, 11, 5, W[2])
    put(img, 5, 14, W[0])
    return outline(img, OUTL)


def icon_axe():
    """16x16 battle axe."""
    img = canvas(16, 16)
    d = ImageDraw.Draw(img)
    STEEL = ramp(H("#a8b0c0"))
    for i in range(10):
        put(img, 5 + i, 14 - i, H("#7a5330"))
        put(img, 6 + i, 14 - i, H("#94653c"))
    # blade wedge
    d.polygon([(9, 2), (14, 4), (13, 9), (8, 7)], fill=STEEL[2] + (255,))
    d.polygon([(10, 3), (13, 4), (12, 8), (9, 6)], fill=STEEL[3] + (255,))
    d.line([(9, 2), (8, 7)], fill=STEEL[1] + (255,))
    return outline(img, OUTL)


def icon_spear():
    """16x16 halberd / spear."""
    img = canvas(16, 16)
    d = ImageDraw.Draw(img)
    STEEL = ramp(H("#c2c8d4"))
    for i in range(12):
        put(img, 2 + i, 14 - i, H("#7a5330"))
        put(img, 3 + i, 14 - i, H("#94653c"))
    d.polygon([(11, 1), (15, 3), (12, 6), (10, 4)], fill=STEEL[2] + (255,))
    d.polygon([(11, 2), (14, 3), (12, 5)], fill=STEEL[3] + (255,))
    # side axe blade
    d.polygon([(9, 5), (12, 7), (9, 9), (7, 7)], fill=STEEL[1] + (255,))
    return outline(img, OUTL)


def icon_bow():
    """16x16 hunting bow with string."""
    img = canvas(16, 16)
    d = ImageDraw.Draw(img)
    W = ramp(H("#a8763f"))
    # curved limb
    for i in range(14):
        t = i / 13.0
        y = 1 + i
        x = 4 + int(5 * (1 - (2 * t - 1) ** 2))
        put(img, x, y, W[2])
        put(img, x + 1, y, W[1])
    d.line([(5, 1), (5, 14)], fill=(235, 235, 220, 255))          # string
    d.line([(9, 7), (3, 7)], fill=(210, 170, 90, 255))            # nocked arrow
    d.point((2, 7), fill=(230, 230, 235, 255))
    return outline(img, OUTL)


def icon_mace():
    """16x16 flanged mace."""
    img = canvas(16, 16)
    d = ImageDraw.Draw(img)
    STEEL = ramp(H("#9aa2b4"))
    for y in range(6, 15):
        put(img, 7, y, H("#7a5330"))
        put(img, 8, y, H("#5d3f28"))
    d.ellipse([4, 1, 11, 8], fill=STEEL[2] + (255,))
    d.ellipse([5, 2, 9, 5], fill=STEEL[3] + (255,))
    for (x, y) in ((4, 4), (11, 4), (7, 1), (7, 8)):
        put(img, x, y, STEEL[1])
    return outline(img, OUTL)


def icon_boots():
    """16x16 swift boots."""
    img = canvas(16, 16)
    d = ImageDraw.Draw(img)
    LEA = ramp(H("#8a5a3a"))
    d.polygon([(4, 3), (9, 3), (9, 11), (14, 12), (14, 14), (3, 14), (3, 6)],
              fill=LEA[2] + (255,))
    d.polygon([(4, 4), (7, 4), (7, 12), (4, 12)], fill=LEA[3] + (255,))
    d.rectangle([3, 13, 14, 14], fill=H("#4a3220") + (255,))
    # wing flourish
    d.polygon([(9, 4), (13, 2), (11, 6)], fill=(240, 240, 250, 255))
    return outline(img, OUTL)


def icon_robe():
    """16x16 silk robe."""
    img = canvas(16, 16)
    d = ImageDraw.Draw(img)
    ROBE = ramp(H("#5a6ad0"), shadow_hue=(-10, -14, 22))
    d.polygon([(5, 2), (10, 2), (13, 6), (11, 14), (4, 14), (2, 6)], fill=ROBE[2] + (255,))
    d.polygon([(5, 3), (8, 3), (7, 13), (5, 13), (3, 6)], fill=ROBE[3] + (255,))
    d.line([(4, 8), (12, 8)], fill=H("#e8c860") + (255,))
    put(img, 8, 8, H("#e8c860"))
    return outline(img, OUTL)


def icon_remedy():
    """16x16 remedy herb bundle."""
    img = canvas(16, 16)
    d = ImageDraw.Draw(img)
    LEAF = ramp(H("#4a9a48"))
    d.line([(4, 13), (11, 4)], fill=H("#7a5330") + (255,))
    for (lx, ly) in ((6, 9), (8, 7), (5, 11), (9, 5)):
        d.ellipse([lx - 2, ly - 2, lx + 2, ly + 1], fill=LEAF[2] + (255,))
        put(img, lx, ly - 1, LEAF[3])
    d.ellipse([10, 9, 14, 13], fill=H("#d2423c") + (255,))
    put(img, 11, 10, H("#f08070"))
    return outline(img, OUTL)


def icon_bomb():
    """16x16 smoke bomb (cloth-wrapped charge with fuse)."""
    img = canvas(16, 16)
    d = ImageDraw.Draw(img)
    BALL = ramp(H("#4a4458"))
    d.ellipse([3, 5, 13, 15], fill=BALL[2] + (255,))
    d.ellipse([5, 6, 10, 10], fill=BALL[3] + (255,))
    d.line([(8, 5), (10, 2)], fill=(200, 170, 110, 255))
    put(img, 11, 1, H("#f0c040"))
    put(img, 12, 0, H("#ffe890"))
    d.line([(4, 10), (12, 10)], fill=(30, 26, 38, 255))
    return outline(img, OUTL)


def icon_star_metal():
    """16x16 Star Metal ore — quest treasure."""
    img = canvas(16, 16)
    d = ImageDraw.Draw(img)
    ORE = ramp(H("#6ab8d8"), shadow_hue=(-14, -10, 20), light_hue=(12, 10, -6))
    d.polygon([(3, 9), (6, 3), (12, 4), (14, 10), (9, 14), (4, 13)], fill=ORE[2] + (255,))
    d.polygon([(6, 4), (11, 5), (9, 9), (5, 9)], fill=ORE[3] + (255,))
    d.polygon([(9, 9), (13, 10), (9, 13)], fill=ORE[1] + (255,))
    # sparkle
    put(img, 12, 2, (255, 255, 255))
    put(img, 12, 1, (255, 255, 255))
    put(img, 11, 2, (255, 255, 255))
    put(img, 13, 2, (255, 255, 255))
    put(img, 12, 3, (255, 255, 255))
    return outline(img, OUTL)


def icon_shield_kite():
    """16x16 kite shield with a boss."""
    img = canvas(16, 16)
    d = ImageDraw.Draw(img)
    S = ramp(H("#3f5f9e"))
    d.polygon([(3, 2), (12, 2), (13, 8), (8, 14), (2, 8)], fill=S[2] + (255,))
    d.polygon([(4, 3), (8, 3), (8, 13), (4, 9)], fill=S[3] + (255,))
    d.line([(8, 3), (8, 13)], fill=(230, 210, 140, 255))
    d.ellipse([6, 6, 10, 10], fill=H("#e8c860") + (255,))
    return outline(img, OUTL)


def main():
    save(cave_mouth(), "sprites", "cave_mouth.png")
    save(stair_down(), "sprites", "stair_down.png")
    save(stair_up(), "sprites", "stair_up.png")
    save(smithy(), "sprites", "smithy.png")
    save(icon_staff(), "icons", "staff.png")
    save(icon_axe(), "icons", "axe.png")
    save(icon_spear(), "icons", "spear.png")
    save(icon_bow(), "icons", "bow.png")
    save(icon_mace(), "icons", "mace.png")
    save(icon_boots(), "icons", "boots.png")
    save(icon_robe(), "icons", "robe.png")
    save(icon_remedy(), "icons", "remedy.png")
    save(icon_bomb(), "icons", "bomb.png")
    save(icon_star_metal(), "icons", "star_metal.png")
    save(icon_shield_kite(), "icons", "shield_kite.png")


if __name__ == "__main__":
    main()
