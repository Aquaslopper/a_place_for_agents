"""art_extra.py — generate ONLY the new content assets (dungeon & village 2.0).

Rebuild the cave mouth, dungeon stair tiles, blacksmith exterior and expanded
item icons without regenerating the rest of the art. The forge shares the
OpenRTP material/door composition used by tools/village_art.py.

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
    """Walking-scale stone entrance: broken strata, deep arch and worn steps.

    Use the village/landscape mineral palette, not smooth shaded circles.
    The bottom-centre threshold matches the shared three-cell collision anchor.
    """
    img = canvas(48, 48)
    d = ImageDraw.Draw(img)
    edge = '#354253'; dark = '#49545d'; mid = '#616a70'
    light = '#81898a'; crown = '#b8b6a4'
    profile = [(1, 45), (2, 31), (5, 26), (5, 18), (10, 14), (13, 6),
               (21, 3), (27, 5), (33, 2), (40, 10), (41, 18), (46, 24), (47, 45)]
    d.polygon(profile, fill=mid, outline=edge)
    # Broad angular planes and horizontal seams break up the mound.
    for points, col in [([(5, 27), (10, 15), (14, 7), (22, 4), (19, 17), (12, 25)], light),
                        ([(22, 5), (27, 7), (33, 3), (39, 11), (30, 13), (24, 19), (19, 17)], '#9ca29b'),
                        ([(33, 14), (40, 12), (41, 22), (46, 26), (38, 30), (29, 24)], dark),
                        ([(4, 32), (12, 27), (16, 31), (12, 42), (2, 44)], light),
                        ([(35, 31), (43, 28), (46, 38), (43, 44), (35, 44)], light)]:
        d.polygon(points, fill=col)
    d.line([(13, 7), (20, 5), (25, 7)], fill=crown)
    d.line([(29, 7), (33, 5), (38, 11)], fill=crown)
    d.line([(6, 26), (12, 23), (18, 23)], fill=dark)
    d.line([(27, 14), (32, 12), (38, 14)], fill=dark)
    d.line([(8, 34), (6, 39), (10, 42)], fill=dark)
    d.line([(39, 33), (42, 36), (40, 42)], fill=dark)
    # Recessed stone arch, lit lip and almost-black interior.
    arch = [(14, 44), (14, 28), (17, 20), (22, 16), (28, 17), (33, 22), (35, 30), (35, 44)]
    d.polygon(arch, fill=dark)
    d.line([(13, 40), (13, 28), (16, 20), (22, 15), (28, 16)], fill=crown)
    d.polygon([(17, 43), (17, 29), (20, 23), (24, 21), (28, 23), (31, 29), (32, 43)], fill='#1d2734')
    d.polygon([(20, 39), (20, 29), (23, 25), (26, 25), (29, 30), (29, 39)], fill='#111922')
    # Broken, receding threshold stones lead visibly into the opening.
    for x0, x1, y, col in [(19, 29, 39, dark), (17, 31, 42, mid), (14, 34, 45, light)]:
        d.line((x0, y, x1, y), fill=col, width=2)
        d.line((x0, y, x1, y), fill='#9ca29b')
    d.line((25, 43, 25, 46), fill=dark)
    # Sparse lichen and warm lamp: colour accents, not a hard grass rectangle.
    for x, y in [(6, 29), (8, 28), (11, 16), (36, 17), (39, 43), (5, 43)]:
        d.line((x, y, x+2, y), fill='#759475')
    d.line((39, 23, 39, 44), fill='#5b453d', width=2)
    d.rectangle((36, 24, 42, 31), fill=edge)
    d.rectangle((37, 25, 41, 29), fill='#c19657')
    d.rectangle((38, 26, 40, 28), fill='#ebc88c')
    d.line((36, 23, 42, 23), fill=crown)
    return img


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
    """Textured masonry and slate from the same material bank as Elmswick.

    Keep the centre door readable and put the fire in a recessed forge bay,
    not across the entrance. No baked smoke blobs: the chimney is architecture.
    """
    from village_art import cottage, TIMBER
    img = cottage('slate', 76, 64, kind=10, gable=False, stone=True)
    d = ImageDraw.Draw(img)
    # Broad coursed stone stack with a soot-dark throat (retro stone palette).
    d.rectangle((53, 4, 62, 26), fill='#707a7e', outline=TIMBER)
    for y in (9, 14, 19):
        d.line((54, y, 61, y), fill='#4a5258')
        d.line((57, y-3, 57, y-1), fill='#5c666c')
    d.rectangle((51, 2, 64, 6), fill='#9ca49c', outline=TIMBER)
    d.line((54, 3, 61, 3), fill='#3a4248')
    # Replace the left window with a recessed brick forge bay + banked coals.
    d.rectangle((7, 40, 26, 58), fill=TIMBER)
    d.rectangle((8, 41, 25, 57), fill='#3a4248')
    d.rectangle((10, 44, 23, 56), fill='#7c4a48')
    d.rectangle((11, 49, 22, 55), fill='#b87264')
    d.line((12, 54, 21, 54), fill='#e0a488', width=2)
    d.line((15, 52, 16, 50), fill='#e8d8b8', width=2)
    # Iron anvil silhouetted against the coals; stone workbench sill.
    d.polygon([(10, 47), (23, 47), (20, 50), (18, 50), (18, 53), (13, 53),
               (13, 51), (15, 50), (12, 50)], fill='#3a4454')
    d.line((10, 46, 23, 46), fill='#8696a8')
    d.rectangle((6, 57, 27, 59), fill='#5c666c')
    d.line((7, 57, 26, 57), fill='#9ca49c')
    # A larger hanging anvil trade sign on the right (covers the cottage window).
    d.line((52, 36, 72, 36), fill=TIMBER, width=2)
    d.line((68, 36, 68, 40), fill='#8a6c44')
    d.rectangle((53, 40, 72, 56), fill=TIMBER)
    d.rectangle((54, 41, 71, 55), fill='#2e2418')
    d.polygon([(57, 45), (68, 45), (66, 48), (64, 48), (64, 52),
               (62, 52), (62, 48), (59, 48)], fill='#e8c860')
    d.line((57, 44, 68, 44), fill='#f4e0a0')
    return img


def forge_hearth():
    """48px masonry furnace with a hood, banked coals and an iron grate."""
    from village_art import pattern, STONE
    from landscape_art import ramp as material
    from rtp_sheet import tile, EXT
    img = canvas(48, 48)
    d = ImageDraw.Draw(img)
    stone = material(tile(EXT, 13, 1), STONE)
    pattern(img, [(5, 46), (5, 24), (12, 16), (15, 2), (34, 2), (37, 16), (43, 24), (43, 46)], stone)
    d.line([(5, 46), (5, 24), (12, 16), (15, 2), (34, 2), (37, 16), (43, 24), (43, 46)], fill='#354253')
    d.rectangle((11, 24, 37, 42), fill='#354253')
    d.polygon([(13, 41), (13, 30), (17, 25), (31, 25), (35, 30), (35, 41)], fill='#583e41')
    for x, y in [(15, 37), (20, 34), (25, 36), (30, 33)]:
        d.polygon([(x-2, 40), (x, y-5), (x+1, y-2), (x+3, y-4), (x+4, 40)], fill='#b9694f')
        d.line((x, 39, x+1, y), fill='#efa974', width=2)
    for x in range(14, 36, 4): d.line((x, 38, x, 42), fill='#465873')
    d.line((12, 41, 36, 41), fill='#819aa7')
    d.rectangle((3, 44, 45, 47), fill='#49545d')
    d.line((4, 44, 44, 44), fill='#b8b6a4')
    d.line((10, 20, 38, 20), fill='#49545d', width=2)
    d.line((11, 19, 37, 19), fill='#9ca29b')
    return img


def forge_anvil():
    """32px iron anvil on a banded oak stump, readable from the field view."""
    img = canvas(32, 32)
    d = ImageDraw.Draw(img)
    d.ellipse((7, 16, 26, 30), fill='#5b453d')
    d.rectangle((7, 20, 26, 28), fill='#836043')
    for x in (10, 15, 22): d.line((x, 21, x-1, 28), fill='#62493b')
    d.line((8, 25, 25, 25), fill='#354253', width=2)
    d.ellipse((7, 15, 26, 22), fill='#a0794c', outline='#5b453d')
    d.polygon([(2, 5), (28, 5), (29, 9), (23, 11), (21, 15), (25, 18),
               (10, 18), (14, 14), (12, 10), (7, 10)], fill='#354253')
    d.polygon([(3, 5), (28, 5), (27, 7), (6, 7)], fill='#9ca29b')
    d.line((8, 8, 26, 8), fill='#68869c')
    d.line((15, 11, 17, 15), fill='#526e87', width=2)
    d.line((11, 17, 24, 17), fill='#819aa7')
    return img


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
    save(forge_hearth(), "sprites", "forge_hearth.png")
    save(forge_anvil(), "sprites", "forge_anvil.png")
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
