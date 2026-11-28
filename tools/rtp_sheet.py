"""Shared 16px chipset cropping, colour keys and pixel-preserving composition."""
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent / 'assets'
SRC = ROOT / 'source' / 'openrtp'

KEY_CYAN = (0, 147, 146)      # RPG Maker 2000 charset key
KEY_PINK = (255, 103, 139)    # OpenRTP chipset key
KEY_PINK2 = (254, 103, 138)   # object charsets one-off pink

EXT = 'ChipSet/Exterior.png'
WLD = 'ChipSet/World.png'
INT = 'ChipSet/Interior.png'
DUN = 'ChipSet/Dungeon.png'


# ------------------------------------------------------------------ utilities
def load(sheet):
    return Image.open(SRC / sheet).convert('RGBA')


def keyed(img, key):
    """Remove an exact colour key."""
    p = img.load()
    for y in range(img.height):
        for x in range(img.width):
            if p[x, y][:3] == key:
                p[x, y] = (0, 0, 0, 0)
    return img


def tile(sheet, tx, ty, key=None, scale=1):
    out = load(sheet).crop((tx * 16, ty * 16, tx * 16 + 16, ty * 16 + 16))
    if key:
        keyed(out, key)
    if scale != 1:
        out = out.resize((16 * scale, 16 * scale), Image.Resampling.NEAREST)
    return out


def rect(sheet, x, y, w, h, key=None):
    out = load(sheet).crop((x, y, x + w, y + h))
    if key:
        keyed(out, key)
    return out


def tcell(sheet, tx, ty, tw=1, th=1, key=None):
    return rect(sheet, tx * 16, ty * 16, tw * 16, th * 16, key)


def save(img, *parts):
    path = ROOT.joinpath(*parts)
    path.parent.mkdir(parents=True, exist_ok=True)
    img.save(path)


def trim(img, pad=0):
    bb = img.getbbox()
    if not bb:
        return img
    x0, y0, x1, y1 = bb
    return img.crop((max(0, x0 - pad), max(0, y0 - pad),
                     min(img.width, x1 + pad), min(img.height, y1 + pad)))


def canvas(w, h):
    return Image.new('RGBA', (w, h))


def centered(img, size, upscale=True):
    """Integer-scale a sprite to fill ``size`` and bottom-centre it."""
    cw, ch = size
    sc = min(cw / img.width, ch / img.height)
    if not upscale:
        sc = min(1.0, sc)
    if sc > 1.0:
        sc = max(1.0, float(int(sc)))
    nw, nh = max(1, int(img.width * sc)), max(1, int(img.height * sc))
    out = canvas(cw, ch)
    out.alpha_composite(img.resize((nw, nh), Image.Resampling.NEAREST),
                        ((cw - nw) // 2, ch - nh))
    return out


def centered_mid(img, size, upscale=True):
    """Scale and center an icon cleanly in ``size``."""
    cw, ch = size
    sc = min(cw / max(1, img.width), ch / max(1, img.height))
    if not upscale:
        sc = min(1.0, sc)
    nw, nh = max(1, int(round(img.width * sc))), max(1, int(round(img.height * sc)))
    out = canvas(cw, ch)
    out.alpha_composite(img.resize((nw, nh), Image.Resampling.NEAREST),
                        ((cw - nw) // 2, (ch - nh) // 2))
    return out


def tint(img, rgb):
    """Multiply a texture by a colour while keeping its shading."""
    if not rgb:
        return img
    out = img.copy()
    p = out.load()
    for y in range(out.height):
        for x in range(out.width):
            r, g, b, a = p[x, y]
            if a:
                p[x, y] = (r * rgb[0] // 255, g * rgb[1] // 255, b * rgb[2] // 255, a)
    return out




# ------------------------------------------------ SNES-retro chipset sheets
# The retro chipsets share the layout of their modern counterparts, but carry
# the 16-bit-era pixel content (dithered texture, chunkier objects) in the same
# cells. Crop coordinates verified cell-by-cell before use (ART_BIBLE 10.3).
REXT = 'ChipSet/retro_Exterior.png'
RWLD = 'ChipSet/retro_World.png'
RDUN = 'ChipSet/retro_Dungeon.png'

# Retro sheets mix several keys: pink (255,103,139), the 1-LSB object pink
# (254,103,138) and retro_World's magenta (224,103,191); cyan (0,147,146)
# also occurs. Key all of them with tolerance 12 (handoff note).
RETRO_KEYS = (KEY_PINK, KEY_PINK2, (224, 103, 191), KEY_CYAN)


def keyed_any(img, keys=RETRO_KEYS, tol=12):
    """Remove any of several colour keys within tolerance."""
    p = img.load()
    for y in range(img.height):
        for x in range(img.width):
            r, g, b, a = p[x, y]
            if a and any(abs(r - k[0]) + abs(g - k[1]) + abs(b - k[2]) <= tol for k in keys):
                p[x, y] = (0, 0, 0, 0)
    return img


def rtile(sheet, tx, ty, scale=1):
    out = keyed_any(load(sheet).crop((tx * 16, ty * 16, tx * 16 + 16, ty * 16 + 16)))
    if scale != 1:
        out = out.resize((16 * scale, 16 * scale), Image.Resampling.NEAREST)
    return out


def rcell(sheet, tx, ty, tw=1, th=1):
    return keyed_any(load(sheet).crop((tx * 16, ty * 16, (tx + tw) * 16, (ty + th) * 16)))


def dither_texture(im, dark, light, amount=0.5, phase=0):
    """Two-tone ordered dither between `dark`/`light` over opaque pixels.

    Gives flat modern fills the banded 16-bit texture SNES surfaces use
    (dithered sand, wave-lit water, sparkle snow). `phase` shifts the Bayer
    threshold so the water animation frames read as moving light."""
    from snes import BAYER4
    px = im.load()
    d = tuple(bytes.fromhex(dark)) if isinstance(dark, str) else dark
    l = tuple(bytes.fromhex(light)) if isinstance(light, str) else light
    for y in range(im.height):
        for x in range(im.width):
            r, g, b, a = px[x, y]
            if not a:
                continue
            thr = BAYER4[(y + phase) % 4][(x + phase * 2) % 4] / 16.0
            px[x, y] = (l if thr < amount else d) + (a,)
    return im
