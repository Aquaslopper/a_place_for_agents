"""art_misc.py — enemies (battle sprites), item icons, battle FX, title art."""
from PIL import Image, ImageDraw
import random, math
from snes import H, mixc, ramp, outline, speckle, BAYER4

OUTL = (26, 20, 40)

def canvas(w, h):
    return Image.new("RGBA", (w, h), (0, 0, 0, 0))

def put(img, x, y, c):
    if 0 <= x < img.width and 0 <= y < img.height:
        img.putpixel((x, y), c + (255,) if len(c) == 3 else c)

def ell(d, box, c):
    d.ellipse(box, fill=c + (255,))

# ================================================================== enemies
def slime():
    """46x38 — classic blob with a lit dome, eyes and a drip."""
    w, h = 46, 38
    img = canvas(w, h)
    d = ImageDraw.Draw(img)
    G = ramp(H("#4fb0c8"), shadow_hue=(-8,-4,18), light_hue=(14,10,4))
    ell(d, [2, 10, 43, 36], G[1])
    ell(d, [4, 14, 41, 35], G[2])
    ell(d, [8, 12, 26, 26], G[3])                    # highlight dome
    ell(d, [10, 14, 20, 22], mixc(G[3], (255,255,255), 0.55))
    # drips + wet bottom edge
    for (x0, y0, x1, y1) in [(6, 30, 12, 37), (30, 32, 38, 37)]:
        ell(d, [x0, y0, x1, y1], G[1])
    ell(d, [6, 28, 40, 37], G[0])
    ell(d, [10, 22, 36, 34], G[1])
    # eyes
    for ex in (16, 28):
        ell(d, [ex, 19, ex + 6, 26], (255, 255, 255))
        ell(d, [ex + 2, 21, ex + 5, 25], (28, 24, 44))
        put(img, ex + 3, 22, (255, 255, 255))
    d.line([(20, 29), (26, 29)], fill=(40, 30, 60, 255))
    return outline(img, OUTL)

def wolf():
    """62x36 — side-on dire wolf: low head, raised shoulders, bushy tail."""
    w, h = 62, 36
    img = canvas(w, h)
    d = ImageDraw.Draw(img)
    F = ramp(H("#8b8fa6"), shadow_hue=(-6,-8,16), light_hue=(12,10,2))
    D = ramp(H("#5d6278"))
    # torso: level back, deep chest
    d.polygon([(16, 14), (34, 12), (48, 15), (50, 24), (30, 26), (16, 24)], fill=F[2] + (255,))
    d.polygon([(16, 17), (34, 15), (48, 18), (48, 24), (30, 26), (16, 24)], fill=F[1] + (255,))
    # shoulder hump + neck
    d.polygon([(14, 20), (18, 11), (26, 10), (30, 16), (24, 22)], fill=F[2] + (255,))
    d.polygon([(16, 18), (20, 12), (26, 13), (26, 21)], fill=F[3] + (255,))
    # head: wedge pointing left, low
    d.polygon([(2, 22), (8, 15), (18, 15), (20, 22), (14, 26), (4, 26)], fill=F[2] + (255,))
    d.polygon([(2, 22), (6, 18), (12, 18), (12, 24), (4, 25)], fill=F[3] + (255,))
    d.polygon([(0, 23), (5, 21), (6, 25)], fill=D[0] + (255,))          # muzzle
    put(img, 1, 22, (30, 24, 40)); put(img, 3, 22, (250, 250, 255))
    put(img, 5, 21, (220, 70, 60))                                    # eye
    # ears
    d.polygon([(10, 15), (12, 8), (16, 14)], fill=D[1] + (255,))
    d.polygon([(16, 14), (19, 8), (21, 15)], fill=D[0] + (255,))
    # legs with paws
    for lx in (17, 25, 37, 45):
        d.rectangle([lx, 23, lx + 3, 31], fill=F[1] + (255,))
        d.rectangle([lx - 1, 30, lx + 4, 33], fill=D[1] + (255,))
    # tail
    d.polygon([(48, 16), (60, 10), (58, 16), (62, 22), (50, 24)], fill=D[1] + (255,))
    for x in range(18, 48, 3):
        put(img, x, 13 + (x % 2), F[3])
    return outline(img, OUTL)

def bat():
    """46x28 — wide wings, small body."""
    w, h = 46, 28
    img = canvas(w, h)
    d = ImageDraw.Draw(img)
    P = ramp(H("#7a5a9e"), shadow_hue=(-6,-10,18), light_hue=(12,6,2))
    # wings
    d.polygon([(0, 6), (18, 10), (18, 22), (8, 18), (0, 14)], fill=P[1] + (255,))
    d.polygon([(45, 6), (28, 10), (28, 22), (37, 18), (45, 14)], fill=P[0] + (255,))
    d.polygon([(3, 8), (17, 12), (17, 19), (10, 16), (4, 13)], fill=P[2] + (255,))
    # body
    ell(d, [18, 8, 28, 22], P[2])
    ell(d, [20, 10, 26, 18], P[3])
    # ears
    d.polygon([(19, 9), (21, 2), (23, 9)], fill=P[1] + (255,))
    d.polygon([(24, 9), (26, 2), (28, 9)], fill=P[1] + (255,))
    put(img, 21, 13, (255, 240, 120)); put(img, 25, 13, (255, 240, 120))
    put(img, 22, 18, (255, 255, 255)); put(img, 24, 18, (255, 255, 255))
    return outline(img, OUTL)

def wisp():
    """40x40 — orb with trailing flame."""
    w, h = 40, 40
    img = canvas(w, h)
    d = ImageDraw.Draw(img)
    G = ramp(H("#6ad0e8"), shadow_hue=(-10,-4,20), light_hue=(16,12,6))
    for i, r in enumerate((16, 13, 10, 6)):
        c = [G[0], G[1], G[2], G[3]][i]
        ell(d, [20 - r, 18 - r, 20 + r, 18 + r], c)
    ell(d, [15, 12, 25, 22], (255, 255, 255))
    ell(d, [17, 14, 23, 20], (220, 250, 255))
    # trailing wisps
    for (x0, y0, x1, y1) in [(6, 26, 18, 36), (24, 28, 34, 38), (10, 30, 18, 39)]:
        ell(d, [x0, y0, x1, y1], G[1])
    put(img, 17, 16, (30, 40, 70)); put(img, 23, 16, (30, 40, 70))
    return outline(img, OUTL)

def golem():
    """56x64 — hulking stone giant: broad shoulders, tiny head, huge fists."""
    w, h = 56, 64
    img = canvas(w, h)
    d = ImageDraw.Draw(img)
    S = ramp(H("#9a8f7e"), shadow_hue=(-6,-6,14), light_hue=(10,8,-2))
    MOSS = H("#5f8a4a")
    # torso (tapered)
    d.polygon([(8, 22), (47, 22), (43, 54), (30, 60), (17, 60), (12, 54)], fill=S[2] + (255,))
    d.polygon([(10, 24), (28, 24), (26, 57), (18, 57), (12, 50)], fill=S[3] + (255,))
    d.polygon([(30, 26), (44, 26), (40, 50), (31, 55)], fill=S[1] + (255,))
    # pecs / abdomen plates
    d.line([(14, 32), (41, 32)], fill=S[0] + (255,))
    d.line([(16, 42), (39, 42)], fill=S[0] + (255,))
    d.line([(28, 24), (28, 58)], fill=S[0] + (255,))
    # shoulders + arms
    ell(d, [0, 18, 18, 36], S[2])
    ell(d, [38, 18, 56, 36], S[1])
    d.rectangle([3, 30, 13, 48], fill=S[1] + (255,))
    d.rectangle([43, 30, 53, 48], fill=S[0] + (255,))
    ell(d, [0, 44, 16, 58], S[1])          # left fist
    ell(d, [40, 44, 56, 58], S[0])         # right fist
    # head: small, set into the shoulders
    d.rectangle([22, 10, 34, 23], fill=S[2] + (255,))
    d.rectangle([22, 10, 27, 16], fill=S[3] + (255,))
    d.rectangle([23, 24, 33, 26], fill=S[0] + (255,))
    for ex in (24, 30):
        d.rectangle([ex, 15, ex + 3, 18], fill=(255, 186, 84, 255))
        d.point((ex + 1, 16), fill=(255, 250, 200, 255))
    d.line([(26, 21), (31, 21)], fill=S[0] + (255,))
    # cracks + moss
    for (x0, y0, x1, y1) in [(18, 30, 24, 40), (34, 34, 30, 46), (14, 46, 22, 52), (38, 26, 44, 34)]:
        d.line([(x0, y0), (x1, y1)], fill=(46, 38, 34, 255))
    rnd = random.Random(5)
    for _ in range(16):
        x = rnd.randrange(8, 48); y = rnd.randrange(24, 58)
        if img.getpixel((x, y))[3]:
            put(img, x, y, MOSS)
    return outline(img, OUTL)

ENEMIES = {"slime": (slime, (46, 38)), "wolf": (wolf, (62, 36)), "bat": (bat, (46, 28)),
           "wisp": (wisp, (40, 40)), "golem": (golem, (56, 64))}

# ================================================================== icons
def icon_flask(fill, cap, label=None):
    img = canvas(16, 16)
    d = ImageDraw.Draw(img)
    d.rectangle([6, 1, 9, 4], fill=cap + (255,))
    d.rectangle([5, 0, 10, 1], fill=mixc(cap, (255, 255, 255), 0.4) + (255,))
    d.polygon([(6, 4), (9, 4), (13, 13), (2, 13)], fill=(226, 232, 240, 255))
    d.polygon([(6, 6), (9, 6), (12, 13), (3, 13)], fill=fill + (255,))
    ell(d, [4, 8, 8, 12], mixc(fill, (255, 255, 255), 0.35))
    if label:
        put(img, 8, 10, label)
    return outline(img, OUTL)

def icon_potion():  return icon_flask(H("#d2423c"), H("#a8763f"))
def icon_hipotion():return icon_flask(H("#e8722c"), H("#c8a040"))
def icon_ether():   return icon_flask(H("#4a6ae0"), H("#7a5aa0"))
def icon_elixir():
    img = icon_flask(H("#e8c840"), H("#e8e0cc"))
    for (x, y) in [(7, 9), (9, 11), (6, 12)]:
        put(img, x, y, (255, 255, 220))
    return img

def icon_sword(tier):
    """16x16 blade, tier 0..2 (bronze/iron/mythril)."""
    img = canvas(16, 16)
    d = ImageDraw.Draw(img)
    steel = [H("#c98a4a"), H("#c2c8d4"), H("#8ae0d0")][tier]
    hi = mixc(steel, (255, 255, 255), 0.45)
    lo = mixc(steel, (0, 0, 0), 0.35)
    for i in range(9):
        put(img, 4 + i, 12 - i, steel)
        put(img, 5 + i, 12 - i, hi)
        put(img, 4 + i, 13 - i, lo)
    d.line([(3, 9), (7, 13)], fill=H("#e8c860") + (255,))     # guard
    d.line([(2, 12), (5, 15)], fill=H("#7a5330") + (255,))     # grip
    put(img, 1, 14, H("#e8c860"))
    return outline(img, OUTL)

def icon_armor(tier):
    """16x16 breastplate."""
    img = canvas(16, 16)
    d = ImageDraw.Draw(img)
    base = [H("#8a5a3a"), H("#8b94a8"), H("#8ae0d0")][tier]
    d.polygon([(3, 2), (12, 2), (14, 6), (12, 14), (3, 14), (1, 6)], fill=base + (255,))
    d.polygon([(4, 3), (7, 3), (7, 13), (3, 13), (2, 6)], fill=mixc(base, (255, 255, 255), 0.3) + (255,))
    d.line([(8, 3), (8, 13)], fill=mixc(base, (0, 0, 0), 0.4) + (255,))
    d.line([(2, 6), (13, 6)], fill=mixc(base, (0, 0, 0), 0.3) + (255,))
    return outline(img, OUTL)

def icon_coin():
    img = canvas(16, 16)
    d = ImageDraw.Draw(img)
    G = ramp(H("#e8b830"))
    ell(d, [2, 2, 13, 13], G[1])
    ell(d, [3, 3, 12, 12], G[2])
    ell(d, [4, 3, 9, 8], G[3])
    d.line([(6, 5), (6, 10)], fill=G[0] + (255,))
    d.line([(7, 5), (10, 5)], fill=G[0] + (255,))
    d.line([(7, 10), (10, 10)], fill=G[0] + (255,))
    return outline(img, OUTL)

# ================================================================== FX
def fx_slash():
    img = canvas(28, 28)
    d = ImageDraw.Draw(img)
    for i, (a, b) in enumerate([((2, 22), (24, 4)), ((4, 24), (26, 6))]):
        d.line([a, b], fill=(255, 255, 255, 255) if i == 0 else (180, 210, 255, 255))
    for i in range(6):
        put(img, 6 + i * 3, 20 - i * 3, (255, 255, 255))
    return img

def fx_impact():
    img = canvas(18, 18)
    d = ImageDraw.Draw(img)
    for r, c in ((8, (255, 230, 150)), (5, (255, 180, 90)), (2, (255, 255, 255))):
        d.ellipse([9 - r, 9 - r, 9 + r, 9 + r], fill=c + (255,))
    for a in range(0, 360, 45):
        x = 9 + int(math.cos(math.radians(a)) * 8); y = 9 + int(math.sin(math.radians(a)) * 8)
        put(img, x, y, (255, 220, 160))
    return img

def fx_fireball():
    img = canvas(18, 18)
    d = ImageDraw.Draw(img)
    ell(d, [2, 2, 15, 15], (220, 70, 30))
    ell(d, [4, 4, 13, 13], (255, 150, 40))
    ell(d, [6, 6, 11, 11], (255, 240, 170))
    put(img, 8, 4, (255, 255, 220))
    return img

def fx_spark():
    img = canvas(14, 14)
    d = ImageDraw.Draw(img)
    d.line([(7, 0), (7, 13)], fill=(255, 255, 210, 255))
    d.line([(0, 7), (13, 7)], fill=(255, 255, 210, 255))
    d.line([(2, 2), (12, 12)], fill=(220, 240, 255, 255))
    d.line([(12, 2), (2, 12)], fill=(220, 240, 255, 255))
    return img

def fx_sparkle():
    img = canvas(16, 16)
    for (x, y) in [(8, 3), (8, 13), (3, 8), (13, 8)]:
        put(img, x, y, (255, 255, 255))
    for (x, y) in [(8, 8), (7, 7), (9, 9), (7, 9), (9, 7)]:
        put(img, x, y, (255, 250, 200))
    put(img, 8, 1, (200, 240, 255)); put(img, 8, 15, (200, 240, 255))
    return img

def fx_poof():
    img = canvas(26, 18)
    d = ImageDraw.Draw(img)
    for (cx, cy, r, c) in [(7, 10, 6, (230, 230, 240)), (13, 8, 7, (250, 250, 255)),
                           (19, 11, 5, (210, 210, 225))]:
        d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=c + (255,))
    for x in range(0, 26, 3):
        if BAYER4[0][x & 3] < 8:
            put(img, x, 16, (200, 200, 215))
    return img

# ================================================================== title
def title_bg():
    """512x288 painted vista: layered hills at dusk, matching the SNES box look."""
    w, h = 512, 288
    img = Image.new("RGBA", (w, h), (0, 0, 0, 255))
    px = img.load()
    sky_top = H("#1c1038"); sky_mid = H("#4a2a5e"); sky_low = H("#b4586a"); sky_hor = H("#e89a5a")
    for y in range(h):
        t = y / float(h)
        if t < 0.45:    c = mixc(sky_top, sky_mid, t / 0.45)
        elif t < 0.72:  c = mixc(sky_mid, sky_low, (t - 0.45) / 0.27)
        else:           c = mixc(sky_low, sky_hor, (t - 0.72) / 0.28)
        for x in range(w):
            # ordered dither on the sky gradient = classic SNES banding
            n = BAYER4[y & 3][x & 3] / 16.0
            cc = mixc(c, mixc(sky_top, (255, 240, 200), 0.12), n * 0.18)
            px[x, y] = cc + (255,)
    d = ImageDraw.Draw(img)
    # stars
    rnd = random.Random(3)
    for _ in range(90):
        x = rnd.randrange(w); y = rnd.randrange(0, int(h * 0.42))
        b = rnd.randrange(140, 255)
        d.point((x, y), fill=(b, b, min(255, b + 30), 255))
    # moon
    moon = Image.new("RGBA", (80, 80), (0, 0, 0, 0))
    md = ImageDraw.Draw(moon)
    md.ellipse([8, 8, 72, 72], fill=(255, 246, 214, 255))
    md.ellipse([-14, -4, 44, 56], fill=(0, 0, 0, 0))
    # carve the crescent by masking
    mask = Image.new("L", (80, 80), 0)
    ImageDraw.Draw(mask).ellipse([8, 8, 72, 72], fill=255)
    ImageDraw.Draw(mask).ellipse([-16, -6, 42, 54], fill=0)
    moon.putalpha(Image.composite(moon.getchannel("A"), Image.new("L", (80, 80), 0), mask))
    img.alpha_composite(moon, (376, 18))
    # mountain layers
    def ridge(base_y, amp, col, seed, step=8):
        r = random.Random(seed)
        pts = []
        for x in range(0, w + step, step):
            yy = base_y + int(math.sin(x * 0.011 + seed) * amp + math.sin(x * 0.031 + seed * 2) * amp * 0.4)
            pts.append((x, yy))
        pts += [(w, h), (0, h)]
        d.polygon(pts, fill=col + (255,))
    ridge(150, 26, H("#3a2a52"), 1)
    ridge(180, 20, H("#2e2346"), 2)
    ridge(212, 14, H("#241c3a"), 3)
    # forest silhouette
    r = random.Random(9)
    for x in range(0, w, 5):
        hh = 10 + r.randrange(0, 16)
        d.polygon([(x, 246 - hh), (x + 4, 246), (x - 4, 246)], fill=(18, 14, 30, 255))
    d.rectangle([0, 244, w, h], fill=(24, 34, 30, 255))
    # foreground ground with a lit path
    d.rectangle([0, 262, w, h], fill=(30, 48, 36, 255))
    for x in range(w):
        if BAYER4[262 & 3][x & 3] < 8:
            d.point((x, 261), fill=(46, 70, 50, 255))
    d.polygon([(238, 288), (252, 288), (262, 262), (250, 262)], fill=(70, 60, 60, 255))
    d.polygon([(244, 288), (252, 288), (258, 262), (252, 262)], fill=(92, 78, 72, 255))
    # grass tufts
    for _ in range(120):
        x = r.randrange(w); y = r.randrange(250, h)
        d.line([(x, y), (x, y - r.randrange(1, 4))], fill=(52, 80, 52, 255))
    return img
