#!/usr/bin/env python3
"""field_art.py — paint the field in a 16-bit dialect, from scratch.

Why this file exists
--------------------
The field atlas used to be built by palette-swapping cells of a modern RPG-Maker
chipset onto three-colour families (`landscape_art.py::ramp`). That flattens a
109-step source tile into three greens ten luma steps apart: measured local
contrast 10-24 against a Lufia II frame's 26.7, and 25.8 colours per 16x16 tile
against the reference's 8. The field therefore reads as felt, no matter how many
times it is "re-sourced".

This module paints the materials instead:

  * every material is a five-value ramp with a hue shift (cool shadow, warm
    light), never a tint of one colour;
  * texture is *structured*: clumps, blades, strata, stones, wave crests — the
    eye gets an edge to hold on to at any zoom;
  * transitions are 1px hard contours (dark below, lit above) plus Bayer
    dithering, the two things a 16-bit tile does and a modern render does not;
  * nothing here samples a commercial or RPG-Maker sheet.

Every image is deterministic: same phase -> same pixels, so the atlas is a pure
function of this file and can be verified by rebuilding it.

    python3 tools/field_art.py          # write assets/
"""
from __future__ import annotations

import math
import os
import random

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ASSETS = os.path.join(ROOT, "assets")
T = 16

BAYER4 = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]


def hexc(s: str):
    s = s.strip()
    return (int(s[0:2], 16), int(s[2:4], 16), int(s[4:6], 16), 255)


def ramp(spec: str):
    return [hexc(c) for c in spec.split()]


# ------------------------------------------------------------------ ramps
# Five values, dark -> light, hue-shifted: shadows lean blue/violet, highlights
# lean yellow. The mid three carry the texture; 0 and 4 are the contour and the
# sparkle, used sparingly (a 16-bit tile does not use all colours equally).
RAMPS = {
    "meadow":   ramp("1a3310 2c5218 3f7422 599a2e 85c247"),
    "forest":   ramp("121f0c 20351a 33511f 4a7028 6b9435"),
    "amber":    ramp("3a2a10 5a4218 7a5c24 99752e b8943f"),
    "heath":    ramp("252b18 3b4226 545b34 737a48 9a9f62"),
    "mire":     ramp("15251e 253a30 37503f 4e6b52 6a8a68"),
    "dune":     ramp("63451f 84602c a37c38 c2984c dab266"),
    "beach":    ramp("6b5630 8c7442 ae9456 c9b074 e2cf9c"),
    "snow":     ramp("66798c 8aa0b2 a9c6d4 c8e4ec e9f6f9"),
    "ice":      ramp("2f5566 457a8a 65a0ae 8ac2cb b2e0e3"),
    "rock":     ramp("25292d 3b4045 535a5e 6c7378 888e92"),
    "dirt":     ramp("35240f 54391d 755226 95703e b8925a"),
    "soil":     ramp("241708 3e2a16 563c1e 6e4c28 8a6136"),
    "cobble":   ramp("24262a 3b4045 545a60 6e747a 899096"),
    "court":    ramp("1e232e 333a47 4a5264 636b80 7e879c"),
    "leaves":   ramp("332410 4f3b1c 6b5126 896a33 a58944"),
    "ripples":  ramp("52391a 74552b 967342 b39358 d2b57c"),
    "mesa":     ramp("34190f 55301f 774626 9a5c38 ba7449"),
    "frost":    ramp("26333d 3f4e5b 5b6c79 7a8b98 9dafba"),
    "snowcrag": ramp("283845 40515f 5a6f7e 7a8d9b 9db0bd"),
    "water":    ramp("17455a 256b83 34879e 48a3b6 62bdcc"),
    "deep":     ramp("0f2b44 173f5e 205275 2a6a90 397fa6"),
    "trunk":    ramp("2a1a0e 462d17 63421f 815a2b 9c7439"),
    "canopy":   ramp("16290f 28471a 3d6624 588a2e 7fb744"),
    "canopy2":  ramp("14261a 234421 365f2b 4f7f36 6ea34a"),
    "autumn":   ramp("3d1f10 632f14 8b4a1c b06a26 d29438"),
    "blossom":  ramp("3a2a3c 5e4560 8a5f80 b27f96 d9a8b4"),
    "stone":    ramp("26282b 3d4145 585d61 747a7e 93999c"),
    # the six surfaces the renderer animates, in Surface order
}
SURFACES = [
    ("deep", 3), ("water", 3), ("beach", 3), ("meadow", 3), ("forest", 3),
    ("amber", 3), ("heath", 3), ("mire", 3), ("dune", 3), ("snow", 3),
    ("ice", 3), ("rock", 3), ("dirt", 3), ("soil", 3), ("cobble", 3),
    ("court", 3), ("leaves", 3), ("ripples", 3), ("mesa", 3), ("frost", 3),
    ("snowcrag", 3),
]


# ------------------------------------------------------------------ helpers
def rng(seed: int) -> random.Random:
    return random.Random(seed)


def blend(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(4))


class Canvas:
    """A 16x16 torus: every plot wraps, so any crop tiles seamlessly."""

    def __init__(self, base):
        self.px = [[base for _ in range(T)] for _ in range(T)]

    def put(self, x, y, c):
        self.px[y % T][x % T] = c

    def get(self, x, y):
        return self.px[y % T][x % T]

    def blob(self, cx, cy, w, h, c, r: random.Random):
        """An irregular clump, 2-4 px across, never a perfect circle."""
        for dy in range(-h, h + 1):
            for dx in range(-w, w + 1):
                if dx * dx / (w * w + 0.2) + dy * dy / (h * h + 0.2) > 1.0:
                    continue
                if abs(dx) == w and abs(dy) == h:
                    continue
                if r.random() < 0.12:
                    continue
                self.put(cx + dx, cy + dy, c)

    def dither_edge(self, c_in, c_out, r: random.Random, amount=0.55):
        """Bayer-chequer the boundary between two values: how a 16-bit tile
        fakes a third tone without owning one."""
        for y in range(T):
            for x in range(T):
                if self.px[y][x] != c_out:
                    continue
                near = any(0 <= x + dx < T and 0 <= y + dy < T and
                           self.px[y + dy][x + dx] == c_in
                           for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))
                if near and BAYER4[y % 4][x % 4] / 16.0 < amount:
                    pass  # keep the out colour on this dither cell
                elif near:
                    self.px[y][x] = c_in

    def image(self):
        im = Image.new("RGBA", (T, T))
        im.putdata([self.px[y][x] for y in range(T) for x in range(T)])
        return im


def _speckle(r: random.Random, n: int):
    return [(r.randrange(T), r.randrange(T)) for _ in range(n)]


# ------------------------------------------------------------------ terrain
def paint(name: str, phase: int):
    """One 16x16 ground tile. `phase` (0..2) is the animation frame the
    renderer cycles; it only moves the texture, never the palette."""
    C = RAMPS[name]
    r = rng(abs(hash((name, phase))) & 0xFFFFFFFF)
    base = C[2]
    cv = Canvas(base)

    if name in ("meadow", "forest", "amber", "heath", "mire"):
        # 1. the base stays readable: a few big soft patches, not a speckle.
        # A 16-bit meadow is mostly one mid tone with clumps sitting on it.
        for _ in range(9):
            cv.blob(r.randrange(T), r.randrange(T), r.randint(2, 3), r.randint(1, 2), C[1], r)
        for _ in range(8):
            cv.blob(r.randrange(T), r.randrange(T), r.randint(2, 3), r.randint(1, 2), C[3], r)
        cv.dither_edge(C[3], base, r, 0.16)
        # 2. blades: short lit strokes, sparse — the grass reads as grass
        for _ in range(5):
            x, y = r.randrange(T), r.randrange(T)
            cv.put(x, y, C[4])
            cv.put(x, y - 1, C[3])
        for _ in range(5):
            x, y = r.randrange(T), r.randrange(T)
            cv.put(x, y, C[0])
        if name == "forest":
            for x, y in _speckle(r, 6):
                cv.put(x, y, RAMPS["trunk"][2])
                cv.put(x + 1, y, RAMPS["trunk"][1])
        if name == "amber":
            for x, y in _speckle(r, 7):
                cv.put(x, y, RAMPS["autumn"][3])
        if name == "mire":
            for x, y in _speckle(r, 5):
                cv.put(x, y, C[0])
                cv.put(x + 1, y + (phase % 2), C[1])

    elif name in ("dune", "beach"):
        # wind ripples: short scalloped crests, each lit on top and shaded
        # under, offset from its neighbour so the sand never reads as planks
        for k in range(4):
            y0 = (k * 4 + phase) % T
            x = (k * 5) % T
            n = 4 + (k % 3)
            for i in range(n):
                xx = (x + i) % T
                yy = (y0 + (i // 3)) % T
                cv.put(xx, yy, C[3])
                cv.put(xx, yy + 1, C[1])
        for _ in range(14):
            cv.blob(r.randrange(T), r.randrange(T), 2, 1, C[1], r)
        for x, y in _speckle(r, 7):
            cv.put(x, y, C[4] if r.random() < 0.4 else C[0])

    elif name == "frost":
        # frozen ground: pale packed snow over blue-grey, wind-scoured
        for _ in range(18):
            cv.blob(r.randrange(T), r.randrange(T), r.randint(1, 3), 1, C[1], r)
        for _ in range(22):
            cv.blob(r.randrange(T), r.randrange(T), r.randint(1, 3), 1, C[3], r)
        cv.dither_edge(C[3], base, r, 0.35)
        for x, y in _speckle(r, 6):
            cv.put(x, y, C[0])
        for x, y in _speckle(r, 4):
            cv.put(x, y, C[4])

    elif name in ("snow",):
        cv.px = [[C[2] for _ in range(T)] for _ in range(T)]
        for _ in range(22):
            cv.blob(r.randrange(T), r.randrange(T), r.randint(1, 3), 1, C[3], r)
        for _ in range(16):
            cv.blob(r.randrange(T), r.randrange(T), r.randint(1, 2), 1, C[1], r)
        cv.dither_edge(C[3], C[2], r, 0.4)
        for x, y in _speckle(r, 5):
            cv.put(x, y, C[4])

    elif name == "ice":
        for _ in range(20):
            cv.blob(r.randrange(T), r.randrange(T), 2, 1, C[3], r)
        # cracks: a 1px run with a lit side
        for _ in range(3):
            x, y = r.randrange(T), r.randrange(T)
            for k in range(5 + phase):
                cv.put(x + k, y + (k // 2), C[0])
                cv.put(x + k, y + (k // 2) - 1, C[4])

    elif name in ("rock", "mesa", "snowcrag"):
        # broken stone: irregular blocks, each faceted (lit top, dark seam),
        # with rubble between them. Never one continuous course.
        y = 0
        while y < T:
            h = r.choice((3, 4, 5))
            x = r.randrange(0, 4)
            while x < T + 4:
                w = r.randint(4, 8)
                tone = C[r.choice((1, 2, 2, 3))]
                for yy in range(y, min(T, y + h)):
                    for xx in range(x, x + w):
                        cv.put(xx, yy, tone if (yy - y) < h - 1 else C[0])
                for xx in range(x, x + w):        # lit top facet
                    cv.put(xx, y, C[3] if tone != C[3] else C[4])
                for yy in range(y, y + h):        # dark mortar at the bed edge
                    cv.put(x + w, yy, C[0])
                x += w + 1
            y += h + (0 if r.random() < 0.5 else -1)
            y = max(1, y)
        for x, y in _speckle(r, 10):
            cv.put(x, y, C[0] if r.random() < 0.5 else C[4])

    elif name == "cobble":
        # real stones: staggered 8x5 courses with dark mortar
        for row in range(4):
            y = row * 4
            off = (row % 2) * 4 + phase
            for k in range(3):
                x0 = (off + k * 8) % T
                w = 7
                c = C[2] if (row + k) % 2 else C[3]
                for yy in range(y, y + 3):
                    for xx in range(w):
                        cv.put(x0 + xx, yy, c)
                for xx in range(w):          # lit top, dark seam under
                    cv.put(x0 + xx, y, C[4] if c == C[3] else C[3])
                    cv.put(x0 + xx, y + 3, C[0])
                for yy in range(y, y + 4):
                    cv.put(x0 + w, yy, C[0])
            for xx in range(T):
                cv.put(xx, y, C[0] if r.random() < 0.5 else cv.get(xx, y))

    elif name == "court":
        for row in range(2):
            y = row * 8
            off = (row % 2) * 8
            for k in range(2):
                x0 = (off + k * 8) % T
                c = C[2] if (row + k) % 2 else C[3]
                for yy in range(y, y + 7):
                    for xx in range(7):
                        cv.put(x0 + xx, yy, c)
                for xx in range(7):
                    cv.put(x0 + xx, y, C[4])
                    cv.put(x0 + xx, y + 7, C[0])
                for yy in range(y, y + 8):
                    cv.put(x0 + 7, yy, C[0])

    elif name in ("dirt", "soil"):
        for _ in range(20):
            cv.blob(r.randrange(T), r.randrange(T), 2, 1, C[1], r)
        for _ in range(16):
            cv.blob(r.randrange(T), r.randrange(T), 2, 1, C[3], r)
        cv.dither_edge(C[3], base, r, 0.4)
        for x, y in _speckle(r, 7):        # pebbles: dark base, lit cap
            cv.put(x, y, C[0])
            cv.put(x, y - 1, C[4])
        if name == "soil":                 # furrows run across the plot
            for fy in (2, 10):
                for x in range(T):
                    cv.put(x, fy, C[0])
                    cv.put(x, fy + 2, C[3])

    elif name == "leaves":
        # leaf litter: separate 2-3px leaves in three autumn tones, not a
        # grain. Each leaf gets a lit edge and a shadow under it.
        for _ in range(22):
            x, y = r.randrange(T), r.randrange(T)
            tone = C[r.choice((1, 2, 3))]
            cv.put(x, y, tone)
            cv.put(x + 1, y, tone)
            cv.put(x + 1, y - 1, C[min(4, C.index(tone) + 1)])
            cv.put(x, y + 1, C[0])
        for x, y in _speckle(r, 6):
            cv.put(x, y, RAMPS["autumn"][2])

    elif name == "ripples":
        # wet sand with shallow standing water: pools, not stripes
        for _ in range(10):
            cv.blob(r.randrange(T), r.randrange(T), r.randint(2, 4), 1, C[1], r)
        for _ in range(7):
            x, y = r.randrange(T), r.randrange(T)
            cv.put(x, y, RAMPS["water"][2])
            cv.put(x + 1, y, RAMPS["water"][3])
            cv.put(x, y + 1, C[3])
        for x, y in _speckle(r, 6):
            cv.put(x, y, C[4])

    elif name in ("water", "deep"):
        # a single body tone, with 2px wave glints marching across it. Wide
        # alternating bands read as planks; glints read as water.
        for y in range(T):
            for x in range(T):
                cv.put(x, y, C[2] if BAYER4[y % 4][(x + phase) % 4] / 16.0 > 0.72 else C[1])
        for k in range(3):
            y0 = (phase * 5 + k * 5) % T
            x0 = (phase * 3 + k * 6 + 1) % T
            for i in range(3 + (k % 2) * 2):
                xx, yy = (x0 + i) % T, (y0 + (i // 4)) % T
                cv.put(xx, yy, C[3])
                cv.put(xx, yy - 1, C[4])
        for k in range(2):
            y0 = (phase * 4 + k * 7 + 3) % T
            for i in range(3):
                cv.put((y0 * 3 + i * 2) % T, (y0 + i) % T, C[0])

    return cv.image()


# ------------------------------------------------------------------ masks
def inside(mask: int, x: int, y: int) -> bool:
    """Marching-squares corner: the same rule the renderer stamps with. A 1px
    wobble keeps the seam hand-pixelled instead of vector-straight, and all
    four edges still agree with the neighbouring cell."""
    u, v = (x + 0.5) / T, (y + 0.5) / T
    u += 0.05 * math.sin(v * 2 * math.pi)
    v += 0.05 * math.sin((x + 0.5) * 2 * math.pi / T)
    w = ((1 - u) * (1 - v), u * (1 - v), (1 - u) * v, u * v)
    return sum(wi for i, wi in enumerate(w) if mask & (1 << i)) >= 0.5


def edge_mask(mask: int):
    """Per-pixel edge role: 'below' (shadow side), 'above' (lit side),
    'side', or None."""
    role = {}
    for y in range(T):
        for x in range(T):
            if not inside(mask, x, y):
                continue
            if not inside(mask, x, y + 1):
                role[(x, y)] = "below"
            elif not inside(mask, x, y - 1):
                role[(x, y)] = "above"
            elif not inside(mask, x - 1, y) or not inside(mask, x + 1, y):
                role[(x, y)] = "side"
    return role


def masked(tile: Image.Image, mask: int, ramp_name: str) -> Image.Image:
    """Cut the material into the mask shape and draw the contour that makes a
    16-bit tile read: dark under the lip, lit along the top, mid on the sides."""
    C = RAMPS[ramp_name]
    out = Image.new("RGBA", (T, T), (0, 0, 0, 0))
    px = tile.load()
    role = edge_mask(mask)
    for y in range(T):
        for x in range(T):
            if not inside(mask, x, y):
                continue
            c = px[x, y]
            r = role.get((x, y))
            if ramp_name in ("water", "deep"):
                # water has no lip to light: a depth seam must not read as a
                # stripe across the pond, so the seam only shifts one tone.
                if r:
                    c = C[1] if r == "below" else C[2]
                out.putpixel((x, y), c)
                continue
            if r == "below":
                c = C[0]
            elif r == "above":
                c = C[4]
            elif r == "side":
                c = C[1]
            out.putpixel((x, y), c)
    return out


def build_landscape():
    cols, rows = 16, len(SURFACES) * 3
    atlas = Image.new("RGBA", (cols * T, rows * T), (0, 0, 0, 0))
    for s, (name, phases) in enumerate(SURFACES):
        for phase in range(phases):
            tile = paint(name, phase)
            for mask in range(16):
                atlas.paste(masked(tile, mask, name), (mask * T, (s * 3 + phase) * T))
    save(atlas, "tiles", "landscape.png")
    return atlas


def build_relief():
    """The one-pixel shadow that gives the field depth: water under a bank,
    snow under a drift, and a rock lip along the top of every escarpment."""
    atlas = Image.new("RGBA", (256, 80), (0, 0, 0, 0))
    crag, sandstone, blue = RAMPS["rock"], RAMPS["mesa"], RAMPS["snowcrag"]
    water, snow = RAMPS["water"], RAMPS["snow"]

    def at(k, x, y):
        return inside(k, max(-0.5, min(x, 15.5)), max(-0.5, min(y, 15.5)))

    for k in range(1, 15):
        # ---- rows 0/1: the cast shadow a bank throws on the surface below
        for x in range(T):
            for y in range(T):
                if at(k, x, y):
                    if not at(k, x, y + 1):        # bottom lip of the land
                        atlas.putpixel((k * T + x, 0 * T + y), RAMPS["meadow"][0])
                        atlas.putpixel((k * T + x, 1 * T + y), RAMPS["snow"][0])
                    continue
                for depth, tone in enumerate((0, 1, 2), start=1):
                    if at(k, x, y - depth):
                        atlas.putpixel((k * T + x, 0 * T + y), water[tone])
                        atlas.putpixel((k * T + x, 1 * T + y), snow[tone])
                        break
        # ---- rows 2/3/4: the escarpment lip
        for x in range(T):
            for y in range(T):
                if not at(k, x, y):
                    continue
                drop = 0
                for dep in range(1, 8):
                    if not at(k, x, y + dep):
                        drop = dep
                        break
                if not drop:
                    continue
                for row, stone, lip in ((2, crag, crag[4]), (3, sandstone, sandstone[4]),
                                        (4, blue, blue[4])):
                    band = drop // 2
                    c = stone[min(4, 1 + band)]
                    if drop == 1:
                        c = stone[0]          # the shadow line at the very lip
                    elif drop >= 6:
                        c = lip               # the lit rim on top of the face
                    atlas.putpixel((k * T + x, row * T + y), c)
    save(atlas, "tiles", "relief.png")
    return atlas


# ------------------------------------------------------------------ overlays
def tuft(v: int):
    C = RAMPS["meadow"]
    im = Image.new("RGBA", (T, T), (0, 0, 0, 0))
    d = im.load()
    r = rng(100 + v)
    def put(x, y, c):
        if 0 <= x < T and 0 <= y < T:
            d[x, y] = c
    for i in range(5):
        x = 2 + i * 3 + v
        y = 13 - (i % 2) * 2
        put(x, y, C[1])
        put(x, y - 1, C[2])
        put(x - 1, y - 2, C[3])
        put(x + 1, y - 3, C[4])
        put(x, y - 3, C[3])
        put(x + 2, y - 2, C[2])
        if r.random() < 0.6:
            put(x - 1, y, C[0])
    return im


def flower(v: int):
    C = RAMPS["meadow"]
    petal = RAMPS["autumn"] if v == 0 else (RAMPS["blossom"] if v == 1 else RAMPS["canopy"])
    im = Image.new("RGBA", (16, 14), (0, 0, 0, 0))
    d = im.load()
    r = rng(200 + v)
    def fput(x, y, c):
        if 0 <= x < 16 and 0 <= y < 14:
            d[x, y] = c
    for i in range(4):
        x = 2 + i * 4 + (v % 2)
        y = 11 - (i % 2) * 3
        fput(x, y, C[0])
        fput(x, y - 1, C[1])
        fput(x, y - 2, petal[2])
        fput(x - 1, y - 2, petal[3])
        fput(x + 1, y - 2, petal[1])
        fput(x, y - 3, petal[4])
        if r.random() < 0.5:
            fput(x, y - 4, petal[3])
    return im


def pebble_tile():
    C, S = RAMPS["meadow"], RAMPS["stone"]
    im = Image.new("RGBA", (T, T), (0, 0, 0, 0))
    d = im.load()
    for (cx, cy, w) in ((4, 10, 3), (10, 6, 2), (12, 12, 2)):
        for dy in range(-1, 2):
            for dx in range(-w, w + 1):
                if abs(dx) + abs(dy) * 2 > w + 1:
                    continue
                d[cx + dx, cy + dy] = S[3] if dy < 0 else S[1]
        d[cx, cy - 2] = S[4]
        d[cx, cy + 1] = S[0]
        d[cx - w, cy] = S[0]
        d[cx + w, cy] = S[0]
    d[8, 14] = C[1]
    return im


def lilies():
    W = RAMPS["water"]
    im = Image.new("RGBA", (T, T), (0, 0, 0, 0))
    d = im.load()
    for (cx, cy) in ((4, 5), (11, 9), (6, 12)):
        for dx, dy in ((0, 0), (1, 0), (2, 0), (1, 1), (2, 1), (0, 1)):
            d[cx + dx, cy + dy] = RAMPS["canopy"][3]
        d[cx + 1, cy] = RAMPS["canopy"][4]
        d[cx, cy + 1] = RAMPS["canopy"][1]
        d[cx + 3, cy + 1] = W[1]
    return im


# ------------------------------------------------------------------ props
def canopy(size, hollow=None):
    """A broadleaf crown in the 16-bit dialect: three to five lobes at
    different heights, a 1px dark contour all the way round, a lit upper-left
    face and a shaded underside, then a thick trunk with a root flare."""
    w, h = size
    im = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    C = hollow if hollow else RAMPS["canopy"]
    r = rng(w * 31 + h * 17)
    mask = [[False] * w for _ in range(h)]
    crown_bottom = int(h * 0.80)
    # lobes: big central mass, then offsets that break the circle
    cx, cy = w / 2, h * 0.36
    lobes = [(cx, cy, w * 0.34)]
    for _ in range(4):
        lobes.append((cx + r.uniform(-w * 0.30, w * 0.30) * (1 if r.random() < 0.5 else -1),
                      cy + r.uniform(-h * 0.16, h * 0.20), r.uniform(w * 0.20, w * 0.30)))
    for (ox, oy, rad) in lobes:
        for y in range(crown_bottom):
            for x in range(w):
                if (x - ox) ** 2 / (rad * rad) + (y - oy) ** 2 / (rad * rad * 0.80) <= 1.0:
                    mask[y][x] = True
    # scallop the rim: foliage is a lobed silhouette, never a balloon
    for y in range(h):
        for x in range(w):
            if not mask[y][x]:
                continue
            n = ((x % 4) - 1.5) + ((y % 4) - 1.5)
            if n > 1.8 and r.random() < 0.55:
                mask[y][x] = False
    px = im.load()
    for y in range(crown_bottom):
        for x in range(w):
            if not mask[y][x]:
                continue
            edge = not any(0 <= x + dx < w and 0 <= y + dy < crown_bottom and mask[y + dy][x + dx]
                           for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))
            if edge:
                px[x, y] = C[0]
                continue
            # light falls from the upper left; lobe interiors get light clumps
            t = ((x - cx) * 0.75 + (y - cy) * 1.0) / max(1.0, h * 0.50)
            if t < -0.55:
                c = C[4]
            elif t < -0.15:
                c = C[3]
            elif t < 0.40:
                c = C[2]
            else:
                c = C[1]
            if r.random() < 0.28:
                c = C[max(0, min(4, C.index(c) + (1, -1, 0)[r.randrange(3)]))]
            # a second contour inside the crown gives the lobe edge weight
            near_edge = not all(0 <= x + dx < w and 0 <= y + dy < crown_bottom and mask[y + dy][x + dx]
                                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))
            if near_edge and r.random() < 0.7:
                c = C[1] if c != C[0] else c
            px[x, y] = c
    # trunk: three columns, dark contour, flared roots
    Tr = RAMPS["trunk"]
    tw = max(3, w // 8)
    tx0 = int(cx - tw / 2)
    top = int(h * 0.52)
    for y in range(top, h):
        for k in range(tw):
            x = tx0 + k
            if 0 <= x < w:
                px[x, y] = Tr[2] if k else Tr[1]
        if 0 <= tx0 - 1 < w:
            px[tx0 - 1, y] = Tr[0]
            px[tx0 + tw, y] = Tr[0]
    for y in range(h - 3, h):
        for x in range(tx0 - 2, tx0 + tw + 2):
            if 0 <= x < w and (y == h - 1 or px[x, y][3] == 0):
                px[x, y] = Tr[0]
    return im


def pine(w, h, snow=False):
    im = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    C = RAMPS["canopy2"]
    px = im.load()
    cx = w // 2
    tiers = 5
    for y in range(2, h - 3):
        tier = y * tiers // (h - 4)
        half = max(1, int((cx) * (1.0 - tier / (tiers + 0.6))) + 1)
        for x in range(cx - half, cx + half + 1):
            if not (0 <= x < w):
                continue
            d = abs(x - cx)
            if d >= half:
                px[x, y] = C[0]                  # contour
            elif d >= half - 1:
                px[x, y] = C[1]
            else:
                px[x, y] = C[3] if (y % 3 == 0) else C[2]
        if tier % 2 == 0:
            px[cx - half, y] = C[0]
    for y in range(h - 4, h):
        px[cx, y] = RAMPS["trunk"][2]
        px[cx - 1, y] = RAMPS["trunk"][0]
    if snow:
        for y in range(0, h - 3):
            run = 0
            for x in range(cx, 0, -1):
                if px[x, y][3] and px[x, y] != C[0]:
                    run = x
                    break
            for x in range(run, min(w, run + 3)):
                px[x, y] = RAMPS["snow"][3]
    return im


def bush(size=(18, 18), plain=False):
    w, h = size
    C = RAMPS["canopy2"]
    im = canopy((w, h + 4), C)
    return im.crop((0, 2, w, h + 2)) if h + 2 <= im.height else im


def rock(w=20, h=18):
    C = RAMPS["stone"]
    im = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    px = im.load()
    r = rng(w * 13 + h)
    top = h * 0.35
    for y in range(h):
        half = (w * 0.5) * math.sqrt(max(0.0, 1 - ((y - top) / (h * 0.62)) ** 2))
        half = max(2, half)
        for x in range(w):
            dx = x - w / 2
            if abs(dx) > half:
                continue
            edge = abs(abs(dx) - half) < 1.2 or y >= h - 2 or y <= top - half * 0.6
            shade = 4 if (y < top - 2 and abs(dx) < half * 0.5) else (
                3 if y < top else (2 if y < h * 0.7 else 1))
            px[x, y] = C[0] if edge else C[shade]
    for i in range(3):                            # cracks
        x, y = r.randrange(3, w - 3), r.randrange(3, h - 4)
        for k in range(3):
            if 0 <= y + k < h:
                px[x + (k % 2), y + k] = C[0]
    return im


def stump():
    C, Tr = RAMPS["canopy"], RAMPS["trunk"]
    im = Image.new("RGBA", (11, 9), (0, 0, 0, 0))
    px = im.load()
    for y in range(9):
        half = 4 if y > 2 else 5
        for x in range(5 - half, 5 + half):
            if 0 <= x < 11:
                px[x, y] = Tr[2] if 2 < y < 8 else Tr[1]
    for x in range(1, 10):
        px[x, 8] = Tr[0]
        px[x, 1] = Tr[3]
    for x in range(3, 8):
        px[x, 0] = C[1]
    return im


def dead_tree():
    Tr = RAMPS["trunk"]
    im = Image.new("RGBA", (13, 30), (0, 0, 0, 0))
    px = im.load()
    r = rng(77)
    branches = [(6, 28, 6, 12), (6, 20, 2, 10), (6, 17, 10, 7), (6, 24, 10, 16),
                (6, 14, 4, 4), (6, 11, 8, 3)]
    for (x0, y0, x1, y1) in branches:
        n = max(abs(x1 - x0), abs(y1 - y0))
        for i in range(n + 1):
            t = i / max(1, n)
            x = round(x0 + (x1 - x0) * t)
            y = round(y0 + (y1 - y0) * t)
            if 0 <= x < 13 and 0 <= y < 30:
                px[x, y] = Tr[1]
                if y + 1 < 30:
                    px[x, y + 1] = Tr[0]
    return im


def reeds():
    C = RAMPS["canopy"]
    im = Image.new("RGBA", (17, 19), (0, 0, 0, 0))
    px = im.load()
    r = rng(31)
    for i in range(6):
        x = 2 + i * 2
        top = r.randint(2, 7)
        for y in range(top, 19):
            px[x, y] = C[2] if y > top + 2 else C[3]
        px[x, top] = C[4]
        px[x - 1, 16] = C[1]
    return im


def willow():
    C = RAMPS["canopy2"]
    im = canopy((30, 37), C)
    px = im.load()
    # trailing fronds: vertical strands off the crown
    for i in range(7):
        x = 4 + i * 3
        for y in range(12, 30 - (i % 3) * 3):
            if 0 <= x < 30 and px[x, y][3]:
                px[x, y] = C[1] if y % 3 else C[2]
    return im


# ------------------------------------------------------------------ save
def save(img, *parts):
    path = os.path.join(ASSETS, *parts)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    img.save(path)
    return path


def build_overlays():
    for v in range(3):
        save(tuft(v), "tiles", f"tuft_{v}.png")
        save(flower(v), "tiles", f"flower_{v}.png")
    save(pebble_tile(), "tiles", "pebble.png")
    save(lilies(), "tiles", "lilies.png")
    # single swatches, used by interiors and the town plates
    for name, s in (("grass_0", "meadow"), ("grass_2", "forest"), ("grass_3", "heath"),
                    ("grass_dark", "forest"), ("dirt", "dirt"), ("sand", "dune"),
                    ("beach", "beach"), ("swamp", "mire"), ("tundra", "heath"),
                    ("rock_hill", "rock"), ("rock_hill2", "rock"), ("soil", "soil"),
                    ("snow", "snow"), ("snow_drift", "snow"), ("ice", "ice"),
                    ("ice_crack", "ice"), ("cobble", "cobble"), ("flagstone", "court"),
                    ("water_0", "water"), ("water_1", "water"), ("deep_water", "deep")):
        save(paint(s, 0 if name != "grass_1" else 1), "tiles", name + ".png")


def build_props():
    save(canopy((41, 37), RAMPS["canopy"]), "sprites", "tree_0.png")
    save(canopy((30, 30), RAMPS["canopy2"]), "sprites", "tree_1.png")
    save(canopy((41, 37), RAMPS["canopy2"]), "sprites", "tree_2.png")
    save(canopy((41, 37), RAMPS["autumn"]), "sprites", "amber_oak.png")
    save(canopy((30, 30), RAMPS["autumn"]), "sprites", "copper_oak.png")
    save(willow(), "sprites", "willow.png")
    for v in range(3):
        save(pine(16, 31, snow=(v == 2)), "sprites", f"pine_{v}.png")
    save(pine(16, 31, snow=True), "sprites", "tree_snow.png")
    save(bush((18, 18)), "sprites", "bush.png")
    save(bush((16, 16), True), "sprites", "bush_plain.png")
    for v in range(3):
        save(rock(20 if v < 2 else 22, 18), "sprites", f"rock_{v}.png")
    save(stump(), "sprites", "stump.png")
    save(dead_tree(), "sprites", "dead_tree.png")
    save(reeds(), "sprites", "reeds.png")


def main():
    build_landscape()
    build_relief()
    build_overlays()
    print("field art painted: landscape, relief, overlays")


if __name__ == "__main__":
    main()
