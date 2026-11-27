"""snes.py — tiny pixel-art toolkit used by gen_assets.py.

Everything here exists to get closer to the SNES look:
  * hue-shifted shading ramps (shadows go cool/purple, highlights go warm)
  * a hard 1px outline around every sprite
  * ordered (Bayer 4x4) dithering between two ramp steps
  * strictly small palettes: we clamp each sprite to a handful of colours
"""
from PIL import Image, ImageDraw
import random

# ---------------------------------------------------------------- colour utils
def H(s):
    s = s.lstrip("#")
    return (int(s[0:2], 16), int(s[2:4], 16), int(s[4:6], 16))

def mixc(a, b, t):
    return tuple(int(round(x + (y - x) * t)) for x, y in zip(a[:3], b[:3]))

def shade(c, t):
    """t<0 darken, t>0 lighten"""
    if t < 0:
        return mixc(c, (0, 0, 0), -t)
    return mixc(c, (255, 255, 255), t)

def ramp(base, cols=4, shadow_hue=(-10, -16, 26), light_hue=(10, 8, -14)):
    """Return [darkest .. lightest] of a hue-shifted ramp built from `base`.

    SNES palettes almost never ramp with pure black/white: shadows drift toward
    violet/blue and highlights drift toward warm yellow. `shadow_hue` and
    `light_hue` are added to (r,g,b) proportionally to how far we walk from the
    base colour.
    """
    out = []
    for i in range(cols):
        t = i / (cols - 1)              # 0 = darkest, 1 = lightest
        d = (t - 0.5) * 2.0             # -1 .. +1
        if d < 0:
            k = -d
            c = mixc(base, (0, 0, 0), 0.42 * k)
            c = (c[0] + shadow_hue[0] * k, c[1] + shadow_hue[1] * k, c[2] + shadow_hue[2] * k)
        else:
            c = mixc(base, (255, 255, 255), 0.34 * d)
            c = (c[0] + light_hue[0] * d, c[1] + light_hue[1] * d, c[2] + light_hue[2] * d)
        out.append(tuple(int(max(0, min(255, v))) for v in c))
    return out

# ---------------------------------------------------------------- dithering
BAYER4 = [
    [ 0,  8,  2, 10],
    [12,  4, 14,  6],
    [ 3, 11,  1,  9],
    [15,  7, 13,  5],
]

def dither_region(img, box, c1, c2, amount, seed=0):
    """Fill `box` mixing c1/c2 with a 4x4 ordered pattern (amount=0..1 -> c1 share)."""
    px = img.load()
    x0, y0, x1, y1 = box
    thr = amount * 16.0
    for y in range(max(0, y0), min(img.height, y1)):
        for x in range(max(0, x0), min(img.width, x1)):
            c = c1 if BAYER4[y & 3][x & 3] < thr else c2
            px[x, y] = c + (255,)

def speckle(img, box, colors, density, seed=1, only_alpha=True):
    rnd = random.Random(seed)
    px = img.load()
    x0, y0, x1, y1 = box
    for y in range(max(0, y0), min(img.height, y1)):
        for x in range(max(0, x0), min(img.width, x1)):
            if only_alpha and px[x, y][3] == 0:
                continue
            if rnd.random() < density:
                px[x, y] = rnd.choice(colors) + (255,)

# ---------------------------------------------------------------- outline
def outline(img, color=(26, 18, 38), diagonals=False, keep_inside=False):
    """Add a 1px hard outline around every opaque cluster (SNES sprite staple)."""
    w, h = img.size
    src = img.load()
    dst = img.copy()
    dp = dst.load()
    nb = [(-1, 0), (1, 0), (0, -1), (0, 1)]
    if diagonals:
        nb += [(-1, -1), (1, -1), (-1, 1), (1, 1)]
    for y in range(h):
        for x in range(w):
            if src[x, y][3] != 0:
                continue
            for dx, dy in nb:
                nx, ny = x + dx, y + dy
                if 0 <= nx < w and 0 <= ny < h and src[nx, ny][3] != 0:
                    dp[x, y] = color + (255,)
                    break
    return dst

def inner_shade(img, box, color, amount=0.35, from_top=True):
    """Darken the bottom edge / lighten the top edge inside a silhouette."""
    px = img.load()
    x0, y0, x1, y1 = box
    for x in range(x0, x1):
        col = []
        for y in range(y0, y1):
            if px[x, y][3]:
                col.append(y)
        if not col:
            continue
        y = col[-1] if from_top else col[0]
        px[x, y] = mixc(px[x, y][:3], color, amount) + (255,)

def vgrad(img, box, top, bottom, dither=True):
    x0, y0, x1, y1 = box
    hgt = max(1, y1 - y0)
    for y in range(y0, y1):
        t = (y - y0) / hgt
        c = mixc(top, bottom, t)
        for x in range(x0, x1):
            if img.getpixel((x, y))[3] == 0:
                continue
            img.putpixel((x, y), c + (255,))

def ellipse_soft(img, cx, cy, rx, ry, color, edge=None):
    d = ImageDraw.Draw(img)
    d.ellipse([cx - rx, cy - ry, cx + rx, cy + ry], fill=color + (255,))
    if edge:
        d.ellipse([cx - rx, cy - ry, cx + rx, cy + ry], outline=edge + (255,))

def shadow_under(img, cx, cy, rx, ry, alpha=70):
    """Soft contact shadow baked into the sprite's alpha (2 tones + dither)."""
    layer = Image.new("RGBA", img.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    d.ellipse([cx - rx, cy - ry, cx + rx, cy + ry], fill=(18, 12, 28, alpha))
    d.ellipse([cx - rx * 0.65, cy - ry * 0.65, cx + rx * 0.65, cy + ry * 0.65], fill=(18, 12, 28, alpha + 45))
    img.alpha_composite(layer)
