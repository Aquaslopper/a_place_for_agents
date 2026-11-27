#!/usr/bin/env python3
"""Unified 16-bit SNES pixel-art palette, biome ramps, and raster primitives.

All visual assets in `assets/` (excluding vendored audio sources) share this
module's 32-color Core Palette and <=8-color per-biome ramps, anchored to the
golden-hour / cool-slate valley aesthetic.
"""
from __future__ import annotations

import math
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets"

# ---------------------------------------------------------------------------
# 1. Master 32-Color Core Palette (RGB tuples)
# ---------------------------------------------------------------------------
INK_DEEP     = (0x14, 0x18, 0x24)  # Deep indigo-charcoal (1px selective outline / recess)
SLATE_DARK   = (0x24, 0x2D, 0x3E)  # Cool slate shadow (roof/rock/armor shadow)
SLATE_MID    = (0x3F, 0x4F, 0x66)  # Cool slate mid (shingles, castle stone, steel)
SLATE_LIGHT  = (0x67, 0x7B, 0x92)  # Lit cool slate (stone rim, steel edge)
STONE_WARM   = (0xA8, 0x9D, 0x8A)  # Warm limestone / masonry midtone
CREAM_SUN    = (0xEF, 0xE4, 0xC6)  # Sunlit cream stucco / parchment / bright highlight

UMBER_DARK   = (0x38, 0x25, 0x20)  # Deep warm umber (bark/timber crevice & shadow)
TIMBER_MID   = (0x68, 0x44, 0x30)  # Rich cedar / oak timber midtone
OCHRE_MID    = (0x9C, 0x68, 0x39)  # Warm earth / leather / lit timber
OCHRE_LIGHT  = (0xCF, 0x94, 0x4E)  # Sunlit packed earth / straw / brass
GOLD_SHADE   = (0xC9, 0x8A, 0x2C)  # Antique gold shadow (UI bevel shade, crown)
GOLD_SUN     = (0xF2, 0xC9, 0x68)  # Sunlit gold (UI trim, torch glow, coins)
SUN_WHITE    = (0xFF, 0xF3, 0xCA)  # Hot sunlit cream-gold (flame core, sparkle)

LEAF_SHADOW  = (0x1B, 0x30, 0x29)  # Deep forest shadow
LEAF_DARK    = (0x2F, 0x52, 0x38)  # Pine / shaded foliage
LEAF_MID     = (0x52, 0x7D, 0x3F)  # Sage-emerald foliage & grass midtone
LEAF_LIGHT   = (0x82, 0xA8, 0x4B)  # Sunlit olive grass / canopy highlight
LEAF_SUN     = (0xB6, 0xCC, 0x62)  # Upper-left sunlit lime foliage rim

WATER_DEEP   = (0x21, 0x3B, 0x4F)  # Deep slate-teal water
WATER_MID    = (0x3A, 0x64, 0x77)  # Mid river / lake teal
WATER_LIGHT  = (0x5C, 0x91, 0x9E)  # Sunlit shallow water / glacier cyan
WATER_FOAM   = (0xA4, 0xCE, 0xD1)  # Water shimmer / ice glint / snow highlight

CRIMSON_DARK = (0x6E, 0x22, 0x30)  # Deep wine shadow (capes, brick, roofs)
CRIMSON_MID  = (0xB5, 0x3B, 0x3B)  # Warm terracotta / banner / blossom
CORAL_LIGHT  = (0xE5, 0x7A, 0x5E)  # Warm coral highlight / blush

NAVY_UI      = (0x19, 0x23, 0x44)  # Canonical UI window navy
INDIGO_ROYAL = (0x35, 0x4B, 0x87)  # Royal indigo cloak / active UI highlight
LAVENDER     = (0x8B, 0x7E, 0xC8)  # Arcane violet / twilight highlight

SKIN_LIGHT   = (0xF2, 0xC9, 0xA5)  # Fair sunlit skin
SKIN_WARM    = (0xD2, 0x90, 0x6C)  # Warm tanned skin / shadow
SKIN_DEEP    = (0x8B, 0x54, 0x42)  # Rich deep brown skin / warm shadow
ROSE_CHEEK   = (0xD8, 0x7A, 0x70)  # Rosy accent / warm lips / terracotta

CORE_PALETTE: list[tuple[int, int, int]] = [
    INK_DEEP, SLATE_DARK, SLATE_MID, SLATE_LIGHT, STONE_WARM, CREAM_SUN,
    UMBER_DARK, TIMBER_MID, OCHRE_MID, OCHRE_LIGHT, GOLD_SHADE, GOLD_SUN, SUN_WHITE,
    LEAF_SHADOW, LEAF_DARK, LEAF_MID, LEAF_LIGHT, LEAF_SUN,
    WATER_DEEP, WATER_MID, WATER_LIGHT, WATER_FOAM,
    CRIMSON_DARK, CRIMSON_MID, CORAL_LIGHT,
    NAVY_UI, INDIGO_ROYAL, LAVENDER,
    SKIN_LIGHT, SKIN_WARM, SKIN_DEEP, ROSE_CHEEK,
]

# Semantic UI aliases
UI_OUTLINE   = INK_DEEP
UI_NAVY_DARK = (0x14, 0x18, 0x24)
UI_NAVY      = NAVY_UI
UI_NAVY_HI   = INDIGO_ROYAL
UI_GOLD_HI   = SUN_WHITE
UI_GOLD      = GOLD_SUN
UI_GOLD_SH   = GOLD_SHADE

# ---------------------------------------------------------------------------
# 2. Per-Biome Ramps (strictly <= 8 colors per biome, including INK_DEEP)
# ---------------------------------------------------------------------------
BIOME_RAMPS: dict[str, list[tuple[int, int, int]]] = {
    "meadow": [
        INK_DEEP, LEAF_SHADOW, LEAF_DARK, LEAF_MID, LEAF_LIGHT, LEAF_SUN,
        OCHRE_MID, OCHRE_LIGHT,
    ],
    "forest": [
        INK_DEEP, (0x14, 0x24, 0x23), (0x1E, 0x3A, 0x2E), LEAF_DARK,
        (0x3F, 0x70, 0x45), (0x65, 0x93, 0x4C), UMBER_DARK, TIMBER_MID,
    ],
    "amber": [
        INK_DEEP, (0x2B, 0x1E, 0x28), (0x5E, 0x2D, 0x29), (0x8F, 0x46, 0x2D),
        (0xBF, 0x6C, 0x34), (0xDE, 0x9B, 0x42), GOLD_SUN, LEAF_MID,
    ],
    "fen": [
        INK_DEEP, (0x18, 0x21, 0x28), (0x27, 0x3A, 0x37), (0x3C, 0x58, 0x48),
        (0x59, 0x75, 0x53), (0x7F, 0x94, 0x62), (0xA3, 0xB2, 0x7D), WATER_DEEP,
    ],
    "heath": [
        INK_DEEP, (0x24, 0x24, 0x38), (0x42, 0x3C, 0x58), (0x65, 0x56, 0x75),
        (0x8B, 0x71, 0x88), (0x58, 0x6D, 0x4D), (0x7B, 0x91, 0x62), STONE_WARM,
    ],
    "frost": [
        INK_DEEP, (0x1C, 0x2A, 0x3D), (0x34, 0x50, 0x6B), (0x5B, 0x83, 0x9E),
        (0x93, 0xBD, 0xD0), (0xCC, 0xE3, 0xEC), (0xEE, 0xF6, 0xF9), LEAF_DARK,
    ],
    "dunes": [
        INK_DEEP, UMBER_DARK, (0x75, 0x47, 0x2E), (0xA8, 0x6C, 0x3C),
        OCHRE_LIGHT, (0xE8, 0xC0, 0x74), (0xF5, 0xDF, 0xB0), (0x64, 0x7D, 0x4A),
    ],
    "dungeon": [
        INK_DEEP, (0x20, 0x27, 0x38), (0x34, 0x3F, 0x56), (0x4F, 0x5D, 0x75),
        (0x77, 0x85, 0x9A), STONE_WARM, GOLD_SHADE, GOLD_SUN,
    ],
}

# Master union palette (all Core + Biome colors)
MASTER_PALETTE: list[tuple[int, int, int]] = list(
    dict.fromkeys(CORE_PALETTE + [c for ramp in BIOME_RAMPS.values() for c in ramp])
)

# ---------------------------------------------------------------------------
# 3. Color & Hashing Helpers
# ---------------------------------------------------------------------------
def rgba(col: tuple[int, ...], alpha: int = 255) -> tuple[int, int, int, int]:
    if len(col) == 4:
        return (int(col[0]), int(col[1]), int(col[2]), int(col[3]))
    return (int(col[0]), int(col[1]), int(col[2]), alpha)


def mix(a: tuple[int, ...], b: tuple[int, ...], t: float) -> tuple[int, int, int]:
    t = max(0.0, min(1.0, t))
    return (
        int(round(a[0] + (b[0] - a[0]) * t)),
        int(round(a[1] + (b[1] - a[1]) * t)),
        int(round(a[2] + (b[2] - a[2]) * t)),
    )


def nearest_in(col: tuple[int, int, int], palette: list[tuple[int, int, int]]) -> tuple[int, int, int]:
    r, g, b = col[:3]
    best = palette[0]
    best_d = 1e12
    for p in palette:
        dr = r - p[0]
        dg = g - p[1]
        db = b - p[2]
        d = dr * dr * 0.30 + dg * dg * 0.59 + db * db * 0.11
        if d < best_d:
            best_d = d
            best = p
    return best


def h2(x: int, y: int, seed: int = 0) -> float:
    n = (x * 374761393 + y * 668265263 + seed * 1442695041) & 0xFFFFFFFF
    n = ((n ^ (n >> 13)) * 1274126177) & 0xFFFFFFFF
    return ((n ^ (n >> 16)) & 0xFFFFFF) / 16777216.0


# ---------------------------------------------------------------------------
# 4. Crisp Pixel-Art Canvas & Selective-Outline Primitives
# ---------------------------------------------------------------------------
def canvas(w: int, h: int, fill: tuple[int, ...] = (0, 0, 0, 0)) -> Image.Image:
    return Image.new("RGBA", (w, h), rgba(fill, 0) if len(fill) == 3 and fill == (0, 0, 0) else rgba(fill))


def px(im: Image.Image, x: int, y: int, col: tuple[int, ...]) -> None:
    if 0 <= x < im.width and 0 <= y < im.height:
        im.putpixel((x, y), rgba(col))


def getpx(im: Image.Image, x: int, y: int) -> tuple[int, int, int, int]:
    if 0 <= x < im.width and 0 <= y < im.height:
        return im.getpixel((x, y))
    return (0, 0, 0, 0)


def rect(im: Image.Image, x: int, y: int, w: int, h: int, col: tuple[int, ...]) -> None:
    c = rgba(col)
    for yy in range(max(0, y), min(im.height, y + h)):
        for xx in range(max(0, x), min(im.width, x + w)):
            im.putpixel((xx, yy), c)


def hline(im: Image.Image, x0: int, x1: int, y: int, col: tuple[int, ...]) -> None:
    if y < 0 or y >= im.height:
        return
    c = rgba(col)
    for xx in range(max(0, min(x0, x1)), min(im.width, max(x0, x1) + 1)):
        im.putpixel((xx, y), c)


def vline(im: Image.Image, x: int, y0: int, y1: int, col: tuple[int, ...]) -> None:
    if x < 0 or x >= im.width:
        return
    c = rgba(col)
    for yy in range(max(0, min(y0, y1)), min(im.height, max(y0, y1) + 1)):
        im.putpixel((x, yy), c)


def line(im: Image.Image, x0: int, y0: int, x1: int, y1: int, col: tuple[int, ...]) -> None:
    c = rgba(col)
    dx = abs(x1 - x0)
    dy = -abs(y1 - y0)
    sx = 1 if x0 < x1 else -1
    sy = 1 if y0 < y1 else -1
    err = dx + dy
    x, y = x0, y0
    while True:
        if 0 <= x < im.width and 0 <= y < im.height:
            im.putpixel((x, y), c)
        if x == x1 and y == y1:
            break
        e2 = 2 * err
        if e2 >= dy:
            err += dy
            x += sx
        if e2 <= dx:
            err += dx
            y += sy


def ell(im: Image.Image, cx: float, cy: float, rx: float, ry: float, col: tuple[int, ...]) -> None:
    if rx <= 0 or ry <= 0:
        return
    c = rgba(col)
    x0 = max(0, int(math.floor(cx - rx)))
    x1 = min(im.width - 1, int(math.ceil(cx + rx)))
    y0 = max(0, int(math.floor(cy - ry)))
    y1 = min(im.height - 1, int(math.ceil(cy + ry)))
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            nx = (x + 0.5 - cx) / rx
            ny = (y + 0.5 - cy) / ry
            if nx * nx + ny * ny <= 1.0:
                im.putpixel((x, y), c)


def shaded_ell(
    im: Image.Image,
    cx: float,
    cy: float,
    rx: float,
    ry: float,
    ramp: list[tuple[int, int, int]],
    seed: int = 0,
    dither: bool = False,
) -> None:
    """Fill an ellipse with crisp upper-left (-1, -1) directional ramp steps."""
    if rx <= 0 or ry <= 0 or not ramp:
        return
    n = len(ramp)
    x0 = max(0, int(math.floor(cx - rx)))
    x1 = min(im.width - 1, int(math.ceil(cx + rx)))
    y0 = max(0, int(math.floor(cy - ry)))
    y1 = min(im.height - 1, int(math.ceil(cy + ry)))
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            nx = (x + 0.5 - cx) / rx
            ny = (y + 0.5 - cy) / ry
            d2 = nx * nx + ny * ny
            if d2 > 1.0:
                continue
            # Upper-left light bias: (-0.65, -0.76)
            lit = 0.52 - (nx * 0.42 + ny * 0.50) + (1.0 - math.sqrt(d2)) * 0.16
            if dither and ((x ^ y) & 1):
                lit += 0.08
            elif not dither:
                lit += (h2(x, y, seed) - 0.5) * 0.09
            idx = max(0, min(n - 1, int(lit * n)))
            im.putpixel((x, y), rgba(ramp[idx]))


def selective_outline(
    im: Image.Image,
    dark_col: tuple[int, ...] = INK_DEEP,
    hi_edge_col: tuple[int, ...] | None = None,
    preserve_existing_dark: bool = True,
) -> Image.Image:
    """Apply A2 1px selective outline:
    - Bottom/right and dark-side edges receive `dark_col`.
    - Exposed top/left edges receive `hi_edge_col` (darker ramp step, never pure black)
      when provided, keeping sprites legible against both light and dark grounds.
    """
    w, h = im.size
    out = im.copy()
    src = im.load()
    dst = out.load()
    dk = rgba(dark_col)
    hi = rgba(hi_edge_col) if hi_edge_col is not None else dk
    for y in range(h):
        for x in range(w):
            if src[x, y][3] != 0:
                continue
            has_left = x > 0 and src[x - 1, y][3] > 0
            has_right = x < w - 1 and src[x + 1, y][3] > 0
            has_up = y > 0 and src[x, y - 1][3] > 0
            has_down = y < h - 1 and src[x, y + 1][3] > 0
            if not (has_left or has_right or has_up or has_down):
                continue
            # Bottom/right borders catch deep shadow outline; top/left catch selective tint
            if has_up or has_left:
                dst[x, y] = dk
            else:
                dst[x, y] = hi if not preserve_existing_dark else hi
    return out


def sel_outline_in_place(
    im: Image.Image,
    dark_col: tuple[int, ...] = INK_DEEP,
    top_left_col: tuple[int, ...] | None = None,
) -> None:
    """Modify `im` in place with a crisp 1px selective outline."""
    res = selective_outline(im, dark_col=dark_col, hi_edge_col=top_left_col)
    im.paste(res, (0, 0))


def stamp_palette_sprite(
    im: Image.Image,
    grid: list[str],
    cmap: dict[str, tuple[int, ...]],
    ox: int = 0,
    oy: int = 0,
    flip_x: bool = False,
) -> None:
    """Stamp an ASCII pixel art sprite onto `im` cleanly."""
    h = len(grid)
    w = len(grid[0]) if h else 0
    for r, row in enumerate(grid):
        for c, ch in enumerate(row):
            if ch == "." or ch == " " or ch not in cmap:
                continue
            tx = ox + ((w - 1 - c) if flip_x else c)
            ty = oy + r
            px(im, tx, ty, cmap[ch])


def save_png(im: Image.Image, rel_or_path: str | Path) -> Path:
    path = Path(rel_or_path)
    if not path.is_absolute():
        path = ASSETS / path
    path.parent.mkdir(parents=True, exist_ok=True)
    im.save(path, format="PNG", optimize=True)
    return path
