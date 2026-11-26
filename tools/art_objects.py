"""art_objects.py — SNES-style overworld objects, anchored bottom-centre.

All sprites keep the exact pixel dimensions the renderer already expects, and
every one gets a hard dark outline plus a hue-shifted shading ramp.
"""
from PIL import Image, ImageDraw
import random
from snes import H, mixc, ramp, outline, speckle, BAYER4, shadow_under

# ---------------------------------------------------------------- palettes
TRUNK  = ramp(H("#7a5330"))
TRUNK_D= ramp(H("#5d3f28"))
LEAF   = ramp(H("#3f8c3a"))          # broadleaf
LEAF_D = ramp(H("#356f37"))
PINE   = ramp(H("#2f6b45"))
PINE_D = ramp(H("#24543a"))
ROCKC  = ramp(H("#8e8b96"))
BUSH   = ramp(H("#4a9448"))
WOOD   = ramp(H("#a8763f"))
WOOD_D = ramp(H("#7d5429"))
ROOF_R = ramp(H("#b6493c"))
ROOF_B = ramp(H("#3f5f9e"))
ROOF_G = ramp(H("#4a8a5c"))
WALL   = ramp(H("#d9c69a"), shadow_hue=(-6,-8,20), light_hue=(8,6,-8))
WALL_D = ramp(H("#b9a37c"))
STONE  = ramp(H("#9b98a4"))
OUTL   = (26, 20, 38)

def canvas(w, h):
    return Image.new("RGBA", (w, h), (0, 0, 0, 0))

def px(img, x, y, c, a=255):
    if 0 <= x < img.width and 0 <= y < img.height:
        img.putpixel((x, y), c + (a,))

def blob(img, cx, cy, rx, ry, cols, seed=0, jitter=1):
    """A dithered foliage blob: dark rim, mid body, lit crown."""
    rnd = random.Random(seed)
    d = ImageDraw.Draw(img)
    d.ellipse([cx - rx, cy - ry, cx + rx, cy + ry], fill=cols[1] + (255,))
    d.ellipse([cx - rx * 0.72, cy - ry * 0.78, cx + rx * 0.72, cy + ry * 0.2], fill=cols[2] + (255,))
    d.ellipse([cx - rx * 0.36, cy - ry * 0.86, cx + rx * 0.36, cy - ry * 0.1], fill=cols[3] + (255,))
    # rim shadow
    for a in range(0, 360, 7):
        import math
        x = int(cx + math.cos(math.radians(a)) * rx)
        y = int(cy + math.sin(math.radians(a)) * ry)
        if math.sin(math.radians(a)) > 0.1:
            px(img, x, y, cols[0])
    return img

# ---------------------------------------------------------------- trees
def tree(variant):
    """Broadleaf tree, 34x46, trunk at bottom-centre (renderer anchors here)."""
    w, h = 34, 46
    img = canvas(w, h)
    cx = w // 2
    # trunk
    for y in range(h - 14, h):
        half = 1 if y < h - 5 else 2
        for x in range(cx - half, cx + half + 1):
            px(img, x, y, TRUNK[1] if x <= cx else TRUNK[0])
    for y in range(h - 14, h):        # bark texture
        if y % 3 == 0:
            px(img, cx - 1, y, TRUNK_D[0])
    px(img, cx + 1, h - 1, TRUNK_D[1]); px(img, cx - 2, h - 1, TRUNK[0])
    px(img, cx - 2, h - 13, TRUNK[1]); px(img, cx + 2, h - 14, TRUNK[1])
    # canopy: stacked tiers, widest at the bottom
    base = LEAF if variant != 2 else LEAF_D
    blob(img, cx, 24, 16, 13, base, seed=100 + variant)
    blob(img, cx - 9, 18, 10, 9, base, seed=200 + variant)
    blob(img, cx + 9, 19, 10, 9, base, seed=300 + variant)
    blob(img, cx, 11, 11, 8, base, seed=400 + variant)
    # leaf detail: clumps of highlight and shadow
    rnd = random.Random(500 + variant)
    for _ in range(30):
        x = rnd.randrange(2, w - 2); y = rnd.randrange(1, 32)
        if img.getpixel((x, y))[3] == 0: continue
        px(img, x, y, base[3])
        if x + 1 < w: px(img, x + 1, y, base[3])
    for _ in range(22):
        x = rnd.randrange(2, w - 2); y = rnd.randrange(4, 34)
        if img.getpixel((x, y))[3] == 0: continue
        px(img, x, y, base[0])
    img = outline(img, OUTL)
    return img

def pine(variant):
    """Conifer, 30x48: stacked tiers, lit left edge, dark right edge."""
    w, h = 30, 48
    img = canvas(w, h)
    cx = w // 2
    base = PINE if variant != 2 else PINE_D
    for y in range(h - 8, h):
        px(img, cx, y, TRUNK[0]); px(img, cx - 1, y, TRUNK[1]); px(img, cx + 1, y, TRUNK_D[1])
    tiers = [(30, 36, 12), (23, 29, 10), (16, 22, 8), (9, 15, 6), (3, 8, 4)]
    for i, (y0, y1, half) in enumerate(tiers):
        for y in range(y0, y1):
            t = (y - y0) / float(y1 - y0)
            hw = int(half * (0.35 + 0.65 * t))
            for x in range(cx - hw, cx + hw + 1):
                if x <= cx - hw + 1:   c = base[3]
                elif x >= cx + hw - 1: c = base[0]
                else:                  c = base[2] if y < (y0 + y1) // 2 else base[1]
                px(img, x, y, c)
        for x in range(cx - int(half * 0.7), cx + int(half * 0.7)):
            if 0 <= x < w and img.getpixel((x, y1 - 1))[3]:
                px(img, x, y1 - 1, base[0])
    img = outline(img, OUTL)
    return img

def dead_tree():
    w, h = 24, 32
    img = canvas(w, h)
    cx = w // 2
    C0, C1 = H("#6b5140"), H("#4a3a2e")
    for y in range(h - 20, h):
        for x in range(cx - 1, cx + 2):
            px(img, x, y, C1 if x < cx else C0)
    px(img, cx, h - 21, C0)
    for (x0, y0, x1, y1) in [(cx, h - 14, cx - 5, h - 21), (cx, h - 17, cx + 5, h - 24),
                             (cx - 5, h - 21, cx - 7, h - 26), (cx + 5, h - 24, cx + 6, h - 28),
                             (cx, h - 21, cx + 2, h - 27)]:
        ImageDraw.Draw(img).line([(x0, y0), (x1, y1)], fill=C0 + (255,))
    img = outline(img, OUTL)
    return img

def rock(variant):
    w, h = 18, 14
    img = canvas(w, h)
    R = ROCKC
    ImageDraw.Draw(img).polygon([(1, 13), (2, 6), (6, 2), (12, 3), (16, 8), (16, 13)], fill=R[2] + (255,))
    ImageDraw.Draw(img).polygon([(3, 13), (4, 7), (8, 5), (12, 8), (13, 13)], fill=R[1] + (255,))
    ImageDraw.Draw(img).line([(6, 3), (11, 4)], fill=R[3] + (255,))
    ImageDraw.Draw(img).line([(2, 7), (5, 4)], fill=R[3] + (255,))
    ImageDraw.Draw(img).line([(9, 7), (9, 12)], fill=R[0] + (255,))
    return outline(img, OUTL)

def bush():
    w, h = 16, 12
    img = canvas(w, h)
    blob(img, 8, 7, 7, 5, BUSH, seed=7)
    blob(img, 5, 6, 4, 3, BUSH, seed=8)
    blob(img, 11, 7, 4, 3, BUSH, seed=9)
    return outline(img, OUTL)

def mushroom():
    w, h = 12, 10
    img = canvas(w, h)
    RED = ramp(H("#c8453c")); STEM = ramp(H("#e8e0cc"))
    ImageDraw.Draw(img).rectangle([4, 5, 7, 9], fill=STEM[1] + (255,))
    ImageDraw.Draw(img).ellipse([1, 1, 10, 7], fill=RED[1] + (255,))
    ImageDraw.Draw(img).ellipse([2, 1, 6, 4], fill=RED[2] + (255,))
    px(img, 4, 3, (255, 255, 255)); px(img, 8, 4, (255, 255, 255)); px(img, 6, 2, (255, 255, 255))
    return outline(img, OUTL)

def well():
    """28x30: stone ring, wooden posts, small roof."""
    w, h = 28, 30
    img = canvas(w, h)
    cx = w // 2
    # stone ring
    ImageDraw.Draw(img).ellipse([2, 18, 26, 29], fill=STONE[1] + (255,))
    ImageDraw.Draw(img).ellipse([5, 20, 23, 27], fill=(24, 20, 34, 255))
    ImageDraw.Draw(img).ellipse([7, 21, 21, 26], fill=ramp(H("#2f6fa8"))[1] + (255,))
    ImageDraw.Draw(img).ellipse([9, 22, 15, 24], fill=ramp(H("#4f93c8"))[2] + (255,))
    for x in range(3, 26, 4):        # masonry joints
        px(img, x, 25, STONE[0]); px(img, x + 2, 28, STONE[0])
    # posts + roof
    for y in range(6, 19):
        px(img, 4, y, WOOD[1]); px(img, 5, y, WOOD[0])
        px(img, 22, y, WOOD[1]); px(img, 23, y, WOOD[0])
    ImageDraw.Draw(img).polygon([(1, 7), (14, 0), (27, 7)], fill=ROOF_R[1] + (255,))
    ImageDraw.Draw(img).polygon([(1, 7), (14, 3), (14, 8), (1, 9)], fill=ROOF_R[2] + (255,))
    ImageDraw.Draw(img).line([(2, 8), (26, 8)], fill=ROOF_R[0] + (255,))
    # bucket rope
    ImageDraw.Draw(img).line([(14, 8), (14, 16)], fill=H("#c8b070") + (255,))
    ImageDraw.Draw(img).rectangle([12, 16, 16, 19], fill=WOOD_D[1] + (255,))
    return outline(img, OUTL)

def sign():
    w, h = 16, 18
    img = canvas(w, h)
    for y in range(9, 18):
        px(img, 7, y, WOOD[0]); px(img, 8, y, WOOD_D[1])
    ImageDraw.Draw(img).rectangle([1, 2, 14, 10], fill=WOOD[2] + (255,))
    ImageDraw.Draw(img).rectangle([1, 2, 14, 3], fill=WOOD[3] + (255,))
    for y in (5, 7):     # "writing"
        ImageDraw.Draw(img).line([(4, y), (11, y)], fill=WOOD_D[0] + (255,))
    px(img, 3, 9, WOOD_D[0]); px(img, 12, 9, WOOD_D[0])
    return outline(img, OUTL)

def fence():
    w, h = 16, 10
    img = canvas(w, h)
    for x in (2, 13):
        for y in range(1, 10):
            px(img, x, y, WOOD[2]); px(img, x + 1, y, WOOD[0])
        px(img, x, 1, WOOD[3])
    for y in (3, 6):
        ImageDraw.Draw(img).line([(2, y), (14, y)], fill=WOOD[2] + (255,))
        ImageDraw.Draw(img).line([(2, y + 1), (14, y + 1)], fill=WOOD_D[1] + (255,))
    return outline(img, OUTL)

# ---------------------------------------------------------------- buildings
def _house_common(img, w, h, wall, roof, roof_dark, door_col, windows=True, awning=None):
    d = ImageDraw.Draw(img)
    base_y = h - 1
    body_top = h - 31
    # ---- walls: lit left, shaded right, thin plaster texture
    d.rectangle([4, body_top, w - 5, base_y], fill=wall[2] + (255,))
    for y in range(body_top, base_y + 1):
        for x in range(4, w - 4):
            t = (x - 4) / float(w - 8)
            if t < 0.14:   px(img, x, y, wall[3])
            elif t > 0.86: px(img, x, y, wall[1])
    for y in range(body_top + 2, base_y, 5):      # faint plaster banding
        for x in range(6, w - 6, 3):
            if BAYER4[y & 3][x & 3] < 3 and img.getpixel((x, y))[3]:
                px(img, x, y, wall[1])
    # ---- timber framing: corner posts + a beam under the eave
    for y in range(body_top, base_y + 1):
        px(img, 4, y, WOOD_D[1]); px(img, w - 5, y, WOOD_D[1])
    for x in range(4, w - 4):
        px(img, x, body_top, WOOD_D[1])
        if x % 2: px(img, x, body_top + 1, WOOD_D[2] if len(WOOD_D) > 2 else WOOD_D[1])
    # ---- door with frame and step
    dx0, dx1 = w // 2 - 6, w // 2 + 6
    d.rectangle([dx0 - 1, base_y - 19, dx1 + 1, base_y], fill=WOOD_D[1] + (255,))
    d.rectangle([dx0, base_y - 18, dx1, base_y], fill=door_col[1] + (255,))
    d.rectangle([dx0, base_y - 18, dx1, base_y - 17], fill=door_col[2] + (255,))
    d.line([(w // 2, base_y - 18), (w // 2, base_y)], fill=door_col[0] + (255,))
    d.rectangle([dx0 + 1, base_y - 12, dx1 - 1, base_y - 11], fill=door_col[2] + (255,))
    d.rectangle([dx0 + 1, base_y - 6, dx1 - 1, base_y - 5], fill=door_col[2] + (255,))
    px(img, dx1 - 2, base_y - 10, H("#e8c860"))
    px(img, dx1 - 3, base_y - 10, H("#b89a40"))
    # ---- windows: deep blue glass, warm lit top, wooden cross frame
    if windows:
        for wx in (w // 2 - 22, w // 2 + 11):
            d.rectangle([wx - 1, body_top + 6, wx + 9, body_top + 19], fill=WOOD_D[1] + (255,))
            d.rectangle([wx, body_top + 7, wx + 8, body_top + 18], fill=H("#33507a") + (255,))
            d.rectangle([wx, body_top + 7, wx + 8, body_top + 8], fill=H("#7fa8d0") + (255,))
            d.line([(wx + 4, body_top + 7), (wx + 4, body_top + 18)], fill=WOOD_D[1] + (255,))
            d.line([(wx, body_top + 12), (wx + 8, body_top + 12)], fill=WOOD_D[1] + (255,))
            px(img, wx + 1, body_top + 10, H("#bcd8f2"))
    # ---- roof: trapezoid, staggered shingle rows, lit ridge, dark eave
    ry = body_top
    top_y = ry - 17
    d.polygon([(0, ry), (7, top_y), (w - 8, top_y), (w - 1, ry)], fill=roof[1] + (255,))
    d.polygon([(0, ry), (7, top_y), (w // 2 - 2, top_y), (w // 2 - 2, ry)], fill=roof[2] + (255,))
    # shingle rows
    for row in range(0, 4):
        yy = top_y + 3 + row * 4
        span_a = 0 + row * 2
        span_b = w - 1 - row * 2
        for x in range(span_a + (row % 2) * 3, span_b, 6):
            for k in range(3):
                if 0 <= yy < h and img.getpixel((x + k, yy))[3]:
                    px(img, x + k, yy, roof[0])
                    if yy + 1 < h and img.getpixel((x + k, yy + 1))[3]:
                        px(img, x + k, yy + 1, roof[3] if row < 2 else roof[2])
    # ridge highlight + eave shadow
    d.line([(8, top_y), (w - 9, top_y)], fill=roof[3] + (255,))
    d.line([(1, ry), (w - 2, ry)], fill=roof_dark[0] + (255,))
    d.line([(0, ry - 1), (w - 1, ry - 1)], fill=roof_dark[1] + (255,))
    # barge boards along the slopes
    d.line([(0, ry), (7, top_y)], fill=roof_dark[1] + (255,))
    d.line([(w - 1, ry), (w - 8, top_y)], fill=roof_dark[1] + (255,))
    # ---- chimney: brick, sits on the right slope
    chx = w - 22
    d.rectangle([chx, top_y - 9, chx + 6, top_y + 6], fill=ramp(H("#9a5b48"))[1] + (255,))
    d.rectangle([chx - 1, top_y - 10, chx + 7, top_y - 8], fill=ramp(H("#9a5b48"))[0] + (255,))
    for yy in range(top_y - 6, top_y + 5, 3):
        for xx in range(chx + 1, chx + 6, 3):
            px(img, xx + (yy % 2), yy, ramp(H("#9a5b48"))[0])
    # ---- shop awning: scalloped stripes
    if awning:
        ay = body_top + 2
        for x in range(2, w - 2, 8):
            for k in range(7):
                c = awning[0] if (x // 8) % 2 == 0 else awning[1]
                px(img, x + k, ay, c); px(img, x + k, ay + 1, c)
                px(img, x + k, ay + 2, mixc(c, (0, 0, 0), 0.25))
            px(img, x + 1, ay + 3, awning[0] if (x // 8) % 2 == 0 else awning[1])
            px(img, x + 4, ay + 3, awning[0] if (x // 8) % 2 == 0 else awning[1])
    return img

def house(variant):
    """66x58 village house, door at bottom-centre."""
    w, h = 66, 58
    img = canvas(w, h)
    roofs = [(ROOF_R, ROOF_R), (ROOF_B, ROOF_B), (ROOF_G, ROOF_G)]
    wall = WALL if variant != 1 else WALL_D
    roof, roof_d = roofs[variant % 3]
    _house_common(img, w, h, wall, roof, roof_d, WOOD_D)
    img = outline(img, OUTL)
    return img

def shop():
    """66x58 shop: red awning, hanging sign, wide window."""
    w, h = 66, 58
    img = canvas(w, h)
    _house_common(img, w, h, WALL, ROOF_B, ROOF_B, WOOD_D, windows=False,
                  awning=(H("#c8453c"), H("#e8e0cc")))
    d = ImageDraw.Draw(img)
    # shop window (wide, warm lit)
    d.rectangle([10, 36, 30, 47], fill=H("#e0b060") + (255,))
    d.rectangle([10, 36, 30, 37], fill=H("#f8e0a0") + (255,))
    d.line([(20, 36), (20, 47)], fill=WOOD_D[1] + (255,))
    d.rectangle([36, 36, 56, 47], fill=H("#e0b060") + (255,))
    d.rectangle([36, 36, 56, 37], fill=H("#f8e0a0") + (255,))
    # hanging sign
    d.rectangle([24, 24, 42, 32], fill=WOOD[2] + (255,))
    d.rectangle([24, 24, 42, 25], fill=WOOD[3] + (255,))
    d.line([(33, 24), (33, 21)], fill=H("#c8b070") + (255,))
    for x in (28, 33, 38):
        px(img, x, 28, WOOD_D[0]); px(img, x, 29, WOOD_D[0])
    img = outline(img, OUTL)
    return img

def inn():
    """66x58 inn: sign board with a bed pictogram."""
    w, h = 66, 58
    img = canvas(w, h)
    _house_common(img, w, h, ramp(H("#cdb894")), ROOF_R, ROOF_R, WOOD_D)
    d = ImageDraw.Draw(img)
    d.rectangle([20, 22, 46, 33], fill=WOOD[2] + (255,))
    d.rectangle([20, 22, 46, 23], fill=WOOD[3] + (255,))
    # little bed pictogram
    d.rectangle([24, 27, 41, 30], fill=H("#e8e0cc") + (255,))
    d.rectangle([24, 26, 30, 27], fill=H("#f8f0dc") + (255,))
    d.line([(24, 25), (24, 31)], fill=WOOD_D[0] + (255,))
    d.line([(41, 25), (41, 31)], fill=WOOD_D[0] + (255,))
    img = outline(img, OUTL)
    return img

def castle():
    w, h = 74, 78
    img = canvas(w, h)
    d = ImageDraw.Draw(img)
    d.rectangle([6, 34, 67, 77], fill=STONE[2] + (255,))
    for x in range(6, 68, 8):
        d.rectangle([x, 28, x + 5, 34], fill=STONE[2] + (255,))
    for tw, tx in ((16, 26), (16, 42)):
        d.rectangle([tw, tx, tw + 13, tx + 24], fill=STONE[1] + (255,))
        for x in range(tw, tw + 14, 5):
            d.rectangle([x, tx - 4, x + 3, tx], fill=STONE[1] + (255,))
    d.rectangle([30, 56, 43, 77], fill=(24, 20, 34, 255))
    d.ellipse([33, 58, 40, 71], fill=H("#5a4230") + (255,))
    for y in range(40, 70, 6):
        d.line([(8, y), (65, y)], fill=STONE[0] + (255,))
    for x in range(12, 66, 12):
        for y in range(40, 70, 12):
            d.line([(x, y), (x, y + 5)], fill=STONE[0] + (255,))
    d.polygon([(0, 30), (37, 6), (73, 30)], fill=ROOF_B[1] + (255,))
    d.polygon([(0, 30), (37, 6), (37, 13), (0, 33)], fill=ROOF_B[2] + (255,))
    d.line([(0, 30), (73, 30)], fill=ROOF_B[0] + (255,))
    return outline(img, OUTL)
