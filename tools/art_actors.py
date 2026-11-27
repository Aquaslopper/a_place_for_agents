"""art_actors.py — 16x24 walking heroes and 32x32 portraits, SNES style.

3 columns (idle, step A, step B) x 3 rows (down, up, side) in a 48x96 sheet,
which is exactly what the renderer slices.
"""
from PIL import Image, ImageDraw
import random
from snes import H, mixc, ramp, outline

SKIN   = ramp(H("#e8b88a"), shadow_hue=(-4,-2,14), light_hue=(6,4,0))
SKIN_D = ramp(H("#c99a70"))
HAIR_B = ramp(H("#6b4a2a"))
HAIR_W = ramp(H("#e8e2d0"), shadow_hue=(-6,-8,16))
HAIR_R = ramp(H("#a8542c"))
STEEL  = ramp(H("#b9c2d4"))
STEEL_D= ramp(H("#8b94a8"))
GOLD   = ramp(H("#e8c860"))
CLOAK_R= ramp(H("#b8434a"))
ROBE_B = ramp(H("#3f4f9e"))
ROBE_B2= ramp(H("#5566c0"))
ROBE_W = ramp(H("#e6e2d2"), shadow_hue=(-8,-10,18))
ROBE_G = ramp(H("#c9b98e"))
BOOT   = ramp(H("#6b4a30"))
OUTL   = (28, 20, 40)
BOWL   = [(1,9),(2,8),(6,8),(7,9),(8,9),(9,8),(13,8),(14,9)]
BODY   = [(1,6),(2,5),(3,6),(4,4),(9,4),(10,6),(11,6),(12,5),(13,6),(14,6)]
ARM    = [(1,4),(2,4),(3,3),(4,3),(5,4),(6,4),(7,3),(8,3),(9,3),(10,4),(11,4),(12,3),(13,3),(14,4)]
LEGS   = [(1,8),(2,8),(3,7),(4,7),(5,8),(6,7),(7,6),(8,6),(9,6),(10,6),(11,7),(12,6),(13,8),(14,7)]

EMPTY = {(y, x) for y, x in [(-1,0),(24,0),(24,1),(24,2),(24,3),(24,4),(24,5),(24,6),(24,7),(24,8),(24,9),
                            (24,10),(24,11),(24,12),(24,13),(24,14),(24,15)]}
BOWL_PX   = [(y,x) for y, x in BOWL]
BODY_PX   = [(y,x) for y, x in BODY]
ARM_PX    = [(y,x) for y, x in ARM]
LEGS_PX   = [(y,x) for y, x in LEGS]

def blank(w=16, h=24):
    return Image.new("RGBA", (w, h), (0,0,0,0))

def put(img, x, y, c):
    if 0 <= x < img.width and 0 <= y < img.height:
        img.putpixel((x, y), c + (255,) if len(c) == 3 else c)

def clump(img, cx, cy, rx, ry, col):
    d = ImageDraw.Draw(img)
    d.ellipse([cx-rx, cy-ry, cx+rx, cy+ry], fill=col + (255,))

# ------------------------------------------------------------------ helpers
def rect(img, x0, y0, x1, y1, c):
    ImageDraw.Draw(img).rectangle([x0, y0, x1, y1], fill=c + (255,))

def put(img, x, y, c):
    if 0 <= x < img.width and 0 <= y < img.height:
        img.putpixel((x, y), c + (255,) if len(c) == 3 else c)

# ------------------------------------------------------------------ heads
def head_down(img, hair, style):
    """Face at the top of a 16x24 frame: 8x8 head, class hat on top."""
    rect(img, 5, 6, 10, 11, SKIN[2])
    for y in range(6, 12):
        put(img, 5, y, SKIN[1]); put(img, 10, y, SKIN[1])
    put(img, 6, 9, (38, 30, 50)); put(img, 9, 9, (38, 30, 50))     # eyes
    put(img, 7, 11, SKIN_D[1])
    if style == "knight":
        rect(img, 5, 5, 10, 5, hair[2])
        for x in range(4, 12):
            put(img, x, 4, hair[1])
        for x in range(5, 11):
            put(img, x, 3, hair[2])
        put(img, 4, 5, hair[0]); put(img, 11, 5, hair[0])
        put(img, 5, 6, hair[1]); put(img, 10, 6, hair[1])           # sideburns
    elif style == "mage":
        rect(img, 3, 5, 12, 6, ROBE_B[1])                           # brim
        for x in range(4, 12):
            put(img, x, 4, ROBE_B[2])
        for i, x in enumerate(range(5, 11)):
            put(img, x, 3 - i // 2, ROBE_B2[1])
            put(img, x, 2 - i // 2, ROBE_B[1])
        put(img, 8, 1, GOLD[2])
        for x in range(4, 12):
            if x % 2: put(img, x, 6, ROBE_B[0])
    else:
        rect(img, 4, 4, 11, 6, ROBE_W[2])                           # cowl
        for x in range(4, 12):
            put(img, x, 3, ROBE_W[3])
        put(img, 3, 6, ROBE_W[1]); put(img, 12, 6, ROBE_W[1])
        put(img, 7, 3, ROBE_G[1]); put(img, 8, 3, ROBE_G[1])        # gold band

def head_up(img, hair, style):
    if style == "knight":
        rect(img, 5, 4, 10, 10, hair[1])
        rect(img, 5, 4, 10, 6, hair[2])
        put(img, 5, 10, hair[0]); put(img, 10, 10, hair[0])
        put(img, 7, 10, hair[0]); put(img, 8, 10, hair[1])
        # shoulders/collar so the back view is not a floating head
        rect(img, 4, 11, 11, 12, STEEL[2])
        for x in range(4, 12): put(img, x, 12, STEEL[1])
    elif style == "mage":
        rect(img, 5, 5, 10, 10, ROBE_B[1])
        rect(img, 4, 6, 11, 6, ROBE_B[2])
        for i, x in enumerate(range(6, 10)):
            put(img, x, 4 - i // 2, ROBE_B2[1])
        rect(img, 4, 11, 11, 12, ROBE_B[2])
        for x in range(4, 12): put(img, x, 12, ROBE_B[1])
    else:
        rect(img, 5, 5, 10, 10, ROBE_W[2])
        rect(img, 5, 4, 10, 4, ROBE_W[3])
        put(img, 7, 7, ROBE_G[1]); put(img, 8, 7, ROBE_G[1])
        rect(img, 4, 11, 11, 12, ROBE_W[2])
        for x in range(4, 12): put(img, x, 12, ROBE_W[1])

def head_side(img, hair, style):
    """Profile facing right."""
    rect(img, 6, 6, 11, 11, SKIN[2])
    for y in range(6, 12):
        put(img, 6, y, SKIN[1])
    put(img, 11, 9, SKIN[1])
    put(img, 10, 9, (38, 30, 50))
    if style == "knight":
        rect(img, 5, 4, 10, 8, hair[1])
        rect(img, 6, 3, 10, 5, hair[2])
        put(img, 5, 8, hair[0]); put(img, 5, 9, hair[1])
        put(img, 6, 6, hair[1])
    elif style == "mage":
        rect(img, 4, 5, 11, 6, ROBE_B[1])
        for i, x in enumerate(range(5, 11)):
            put(img, x, 4 - i // 2, ROBE_B2[1])
        put(img, 11, 6, ROBE_B[0])
    else:
        rect(img, 4, 4, 11, 6, ROBE_W[2])
        rect(img, 5, 3, 11, 4, ROBE_W[3])
        put(img, 4, 6, ROBE_W[1])

# ------------------------------------------------------------------ bodies
def arm(img, x, y, top, low, hand):
    put(img, x, y, top); put(img, x, y + 1, top); put(img, x, y + 2, low); put(img, x, y + 3, hand)

def legs(img, liftL, liftR, boot_hi, boot_lo, legc):
    # y 20..21 = legs, 22..23 = boots
    put(img, 5, 20, legc); put(img, 6, 20, legc)
    put(img, 9, 20, legc); put(img, 10, 20, legc)
    put(img, 5, 21, legc); put(img, 6, 21, legc)
    put(img, 9, 21, legc); put(img, 10, 21, legc)
    put(img, 5 - liftL, 22, boot_hi); put(img, 6 - liftL, 22, boot_hi)
    put(img, 5 - liftL, 23, boot_lo); put(img, 6 - liftL, 23, boot_lo)
    put(img, 9 + liftR, 22, boot_hi); put(img, 10 + liftR, 22, boot_hi)
    put(img, 9 + liftR, 23, boot_lo); put(img, 10 + liftR, 23, boot_lo)

def body_knight(img, frame):
    lL, lR = [(0, 0), (1, 0), (0, 1)][frame]
    # cloak edge behind
    rect(img, 2, 13, 2, 19, CLOAK_R[1]); rect(img, 13, 13, 13, 19, CLOAK_R[0])
    # cuirass 8 wide
    rect(img, 4, 12, 11, 19, STEEL[2])
    for x in range(4, 12):
        put(img, x, 19, STEEL[1])
    put(img, 5, 13, STEEL[3]); put(img, 6, 13, STEEL[3]); put(img, 5, 14, STEEL[3])
    put(img, 10, 13, STEEL[1]); put(img, 10, 14, STEEL[1]); put(img, 10, 15, STEEL[1])
    # chest ridge (tabard)
    for y in range(13, 19):
        put(img, 7, y, CLOAK_R[2]); put(img, 8, y, CLOAK_R[1])
    # belt
    for x in range(4, 12):
        put(img, x, 18, BOOT[0])
    put(img, 7, 18, GOLD[2]); put(img, 8, 18, GOLD[1])
    # pauldrons
    rect(img, 3, 12, 4, 13, STEEL[3]); rect(img, 11, 12, 12, 13, STEEL[0])
    # arms swing opposite to the legs
    arm(img, 3, 14 - (1 if lL else 0), STEEL[2], STEEL[1], SKIN[2])
    arm(img, 12, 14 - (1 if lR else 0), STEEL[1], STEEL[0], SKIN[1])
    legs(img, lL, lR, BOOT[2], BOOT[1], STEEL_D[1])

def body_mage(img, frame):
    lL, lR = [(0, 0), (1, 0), (0, 1)][frame]
    # robe: widens toward the hem
    for y in range(12, 23):
        x0 = 4 - (1 if y > 16 else 0)
        x1 = 11 + (1 if y > 16 else 0)
        for x in range(x0, x1 + 1):
            put(img, x, y, ROBE_B[2] if x < 8 else ROBE_B[1])
    for x in range(3, 13):
        put(img, x, 22, ROBE_B[0])
        if x % 2: put(img, x, 21, ROBE_B2[1])
    for y in range(12, 22):
        put(img, 7, y, ROBE_B2[1]); put(img, 8, y, ROBE_B[1])
    for x in range(4, 12):
        put(img, x, 18, GOLD[1])
    put(img, 7, 18, GOLD[2]); put(img, 8, 18, GOLD[2])
    # sleeves
    put(img, 3, 13 + (1 if lL else 0), ROBE_B2[1]); put(img, 3, 14, ROBE_B2[0]); put(img, 3, 15, SKIN[2])
    put(img, 12, 13 + (1 if lR else 0), ROBE_B[1]); put(img, 12, 14, ROBE_B[0]); put(img, 12, 15, SKIN[1])
    # staff
    for y in range(6, 23):
        put(img, 13, y, H("#8a6a45"))
    put(img, 13, 5, GOLD[2]); put(img, 12, 6, GOLD[1]); put(img, 14, 6, GOLD[1]); put(img, 13, 4, H("#fff2b0"))
    # feet peeking out
    put(img, 5 - lL, 23, BOOT[1]); put(img, 6 - lL, 23, BOOT[1])
    put(img, 9 + lR, 23, BOOT[0]); put(img, 10 + lR, 23, BOOT[0])

def body_cleric(img, frame):
    lL, lR = [(0, 0), (1, 0), (0, 1)][frame]
    for y in range(12, 23):
        x0 = 4 - (1 if y > 16 else 0)
        x1 = 11 + (1 if y > 16 else 0)
        for x in range(x0, x1 + 1):
            put(img, x, y, ROBE_W[2] if x < 8 else ROBE_W[1])
    for x in range(3, 13):
        put(img, x, 22, ROBE_W[0])
    # gold stole down the middle
    for y in range(12, 22):
        put(img, 7, y, ROBE_G[2]); put(img, 8, y, ROBE_G[1])
    put(img, 7, 13, GOLD[2]); put(img, 8, 13, GOLD[2])
    put(img, 7, 14, GOLD[1]); put(img, 8, 14, GOLD[1])
    put(img, 7, 12, GOLD[1]); put(img, 8, 15, GOLD[1])
    # sleeves
    put(img, 3, 13 + (1 if lL else 0), ROBE_W[2]); put(img, 3, 14, ROBE_W[1]); put(img, 3, 15, SKIN[2])
    put(img, 12, 13 + (1 if lR else 0), ROBE_W[1]); put(img, 12, 14, ROBE_W[0]); put(img, 12, 15, SKIN[1])
    # staff with a gold ring
    for y in range(7, 23):
        put(img, 13, y, ROBE_G[0])
    put(img, 13, 5, GOLD[2]); put(img, 12, 6, GOLD[1]); put(img, 14, 6, GOLD[1])
    put(img, 13, 6, (255, 250, 210))
    put(img, 5 - lL, 23, BOOT[1]); put(img, 6 - lL, 23, BOOT[1])
    put(img, 9 + lR, 23, BOOT[0]); put(img, 10 + lR, 23, BOOT[0])

BODIES = {"knight": body_knight, "mage": body_mage, "cleric": body_cleric}

def frame_px(style, facing, frame):
    img = blank()
    BODIES[style](img, frame)
    if facing == 1:
        head_up(img, HAIR_B, style)
    elif facing == 2:
        head_side(img, HAIR_B, style)
        # slim the silhouette for the profile view
        for y in range(12, 24):
            if img.getpixel((3, y))[3]:
                img.putpixel((3, y), (0, 0, 0, 0))
    else:
        head_down(img, HAIR_B, style)
    return outline(img, OUTL)

def hero_sheet(style):
    sheet = Image.new("RGBA", (48, 96), (0,0,0,0))
    for row, facing in enumerate((0, 1, 2)):
        for col in range(3):
            sheet.alpha_composite(frame_px(style, facing, col), (col * 16, row * 24))
    return sheet

# ------------------------------------------------------------------ portraits
FACES = {
 "knight": dict(hair=HAIR_B, skin=SKIN, cloth=STEEL, cloth2=STEEL_D, accent=CLOAK_R, eye=(60, 90, 150)),
 "mage":   dict(hair=HAIR_R, skin=SKIN, cloth=ROBE_B, cloth2=ROBE_B2, accent=GOLD, eye=(90, 60, 140)),
 "cleric": dict(hair=HAIR_W, skin=SKIN, cloth=ROBE_W, cloth2=ROBE_G, accent=GOLD, eye=(70, 110, 90)),
}

def portrait(style):
    """32x32 bust portrait for menus and the HUD. Hair/hood first, face second."""
    img = Image.new("RGBA", (32, 32), (0,0,0,0))
    d = ImageDraw.Draw(img)
    p = FACES[style]
    # ---- shoulders / garment
    d.ellipse([1, 22, 30, 40], fill=p["cloth"][1] + (255,))
    d.ellipse([4, 24, 27, 40], fill=p["cloth"][2] + (255,))
    d.line([(6, 28), (25, 28)], fill=p["cloth"][0] + (255,))
    d.rectangle([13, 18, 18, 25], fill=p["skin"][1] + (255,))       # neck
    # ---- hair / hood silhouette (behind the face)
    if style == "knight":
        d.ellipse([6, 3, 25, 18], fill=p["hair"][1] + (255,))
        d.ellipse([7, 4, 24, 15], fill=p["hair"][2] + (255,))
        d.rectangle([6, 9, 7, 18], fill=p["hair"][1] + (255,))
        d.rectangle([24, 9, 25, 18], fill=p["hair"][0] + (255,))
    elif style == "mage":
        d.ellipse([4, 1, 27, 23], fill=p["cloth"][1] + (255,))
        d.ellipse([7, 4, 24, 22], fill=p["cloth"][2] + (255,))
        d.polygon([(4, 7), (16, -5), (28, 7)], fill=p["cloth"][1] + (255,))
        d.polygon([(4, 7), (16, -3), (16, 7)], fill=p["cloth"][2] + (255,))
    else:
        d.ellipse([4, 1, 27, 22], fill=p["cloth"][1] + (255,))
        d.ellipse([6, 3, 25, 21], fill=p["cloth"][3] + (255,))
        d.polygon([(4, 8), (16, -4), (28, 8)], fill=p["cloth"][2] + (255,))
    # ---- face
    d.ellipse([8, 6, 23, 24], fill=p["skin"][2] + (255,))
    d.ellipse([9, 7, 22, 22], fill=p["skin"][3] + (255,))
    d.rectangle([7, 12, 8, 16], fill=p["skin"][1] + (255,))          # ears
    d.rectangle([23, 12, 24, 16], fill=p["skin"][1] + (255,))
    if style == "knight":      # fringe over the forehead, above the eyes
        d.ellipse([8, 5, 23, 13], fill=p["hair"][1] + (255,))
        for x in range(9, 23, 3):
            d.line([(x, 8), (x, 11)], fill=p["hair"][2] + (255,))
    # eyes
    for ex in (11, 18):
        d.rectangle([ex, 13, ex+2, 15], fill=(252, 250, 255, 255))
        d.rectangle([ex+1, 13, ex+2, 14], fill=p["eye"] + (255,))
        d.point((ex+1, 14), fill=(26, 20, 34, 255))
    d.line([(10, 12), (13, 11)], fill=p["hair"][0] + (255,))         # brows
    d.line([(18, 11), (21, 12)], fill=p["hair"][0] + (255,))
    # nose + mouth
    d.point((15, 17), fill=p["skin"][1]); d.point((16, 17), fill=p["skin"][1])
    d.line([(14, 19), (17, 19)], fill=mixc(p["skin"][1], (130, 60, 60), 0.45) + (255,))
    d.point((15, 20), fill=p["skin"][2])
    # ---- class details on top
    if style == "mage":
        d.rectangle([4, 7, 27, 9], fill=p["accent"][1] + (255,))     # hat band
        for x in range(5, 27, 3):
            d.point((x, 8), fill=p["accent"][2])
        d.point((16, 0), fill=p["accent"][2])
    elif style == "cleric":
        d.line([(4, 8), (16, 0)], fill=p["accent"][1] + (255,))      # cowl trim
        d.line([(28, 8), (16, 0)], fill=p["accent"][1] + (255,))
        d.rectangle([13, 26, 18, 29], fill=p["accent"][1] + (255,))  # clasp
        d.point((15, 27), fill=(255, 244, 190))
        d.point((16, 27), fill=(255, 244, 190))
    else:
        d.rectangle([12, 25, 19, 28], fill=p["accent"][1] + (255,))  # tabard
        d.rectangle([12, 25, 13, 28], fill=p["accent"][2] + (255,))
    img = outline(img, OUTL)
    return img
