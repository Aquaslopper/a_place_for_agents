#!/usr/bin/env python3
"""Measure how far the shipped art sits from a 16-bit console screen, and write
the evidence into docs/visual_audit/.

This tool is READ-ONLY with respect to assets/: it never writes into assets/ or
docs/screenshots/. It only measures the files that already ship and writes
preview PNGs plus a JSON report under docs/visual_audit/.

Run from any directory:
    python3 tools/audit_palette.py

What is measured (see docs/VISUAL_AUDIT.md for the interpretation):

  * colours per 16x16 logical tile   - the SNES has a 15-colour-per-tile palette,
                                       so a floor-building screen keeps this low.
  * colours per screen               - hardware allowed 256 per screen; a SNES
                                       JRPG village screen typically uses < 100.
  * local contrast                   - mean |dL| between neighbouring pixels. A
                                       16-bit screen is LOW on colours but HIGH
                                       on contrast: hard, far-apart values.
  * luminance spread of a ground tile - how many steps the tile's shading uses,
                                        and how far apart they are.

Source sheets are read from assets/source/ (vendored, credited); nothing here
crops a commercial rip.
"""
from __future__ import annotations

import json
import statistics
from pathlib import Path

from PIL import Image, ImageFilter

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets"
OUT = ROOT / "docs" / "visual_audit"
TILE = 16

# A hand-picked Lufia II reference frame, if the human has dropped one in
# image-search/ (that folder is gitignored; the tool degrades gracefully).
REFERENCE_CANDIDATES = [
    ROOT / "image-search" / "lufia-ii-rise-of-the-sinistrals-snes-vil-1.png",
]


# ------------------------------------------------------------------ measuring
def tile_budgets(img: Image.Image) -> list[int]:
    w, h = img.size
    out = []
    for ty in range(0, h - TILE + 1, TILE):
        for tx in range(0, w - TILE + 1, TILE):
            out.append(len(set(img.crop((tx, ty, tx + TILE, ty + TILE)).getdata())))
    return out


def local_contrast(img: Image.Image) -> float:
    """Mean absolute luminance delta between a pixel and its right neighbour."""
    g = img.convert("L")
    w, h = g.size
    d = list(g.getdata())
    diffs = []
    for y in range(0, h - 1, 2):
        row = y * w
        for x in range(0, w - 2, 2):
            diffs.append(abs(d[row + x] - d[row + x + 1]))
    return statistics.mean(diffs) if diffs else 0.0


def luma(p) -> float:
    return 0.299 * p[0] + 0.587 * p[1] + 0.114 * p[2]


def luminance_spread(img: Image.Image) -> tuple[int, int]:
    px = [p for p in img.convert("RGBA").getdata() if p[3]]
    if not px:
        return 0, 0
    lums = sorted({luma(p) for p in px})
    return round(lums[-1] - lums[0]), len({p[:3] for p in px})


def measure_screen(label: str, img: Image.Image) -> dict:
    img = img.convert("RGB")
    budgets = tile_budgets(img)
    return {
        "label": label,
        "size": list(img.size),
        "colours": len(set(img.getdata())),
        "colours_per_tile_mean": round(statistics.mean(budgets), 1),
        "colours_per_tile_max": max(budgets),
        "local_contrast": round(local_contrast(img), 2),
    }


def measure_asset(label: str, path: Path) -> dict:
    im = Image.open(path).convert("RGBA")
    spread, colours = luminance_spread(im)
    return {
        "label": label,
        "file": str(path.relative_to(ROOT)),
        "colours": colours,
        "luminance_spread": spread,
    }


# ------------------------------------------------------------------ 16-bit fixes
# A 16-bit ground tile: 5 steps, hue-shifted, and SPANNING the source's range
# instead of collapsing it. Compare with the ramp() calls in landscape_art.py,
# which give grass three colours spanning 10/255.
RAMP_16BIT = {
    "grass":  "2f4a28 446232 5c7d3f 7d9e50 a2c169",
    "cobble": "3b4145 5a6166 7b8183 9aa09c bcbfb4",
    "dirt":   "6a4a33 8a6344 a97d55 c79b6d e2ba8a",
    "forest": "22381f 324b2b 456138 5b7a45 739253",
}


def ramp_to(im: Image.Image, colours: str) -> Image.Image:
    """Palette-remap a tile onto the given ramp, preserving its pattern."""
    im = im.convert("RGBA")
    cols = [tuple(bytes.fromhex(c)) + (255,) for c in colours.split()]
    keys = sorted({p for p in im.getdata() if p[3]}, key=lambda p: (luma(p), p))
    lut = {p: cols[round(i * (len(cols) - 1) / max(1, len(keys) - 1))]
           for i, p in enumerate(keys)}
    out = im.copy()
    out.putdata([lut.get(p, p) for p in im.getdata()])
    return out


def outline(im: Image.Image, colour=(38, 30, 24, 255)) -> Image.Image:
    """Grow a one-pixel dark contour around a sprite, the way console art is
    separated from the ground it stands on."""
    im = im.convert("RGBA")
    mask = im.split()[3].filter(ImageFilter.MaxFilter(3))       # dilate silhouette
    dark = Image.new("RGBA", im.size, colour)
    dark.putalpha(mask)
    dark.alpha_composite(im)
    return dark


def posterise(im: Image.Image, colours: int = 14) -> Image.Image:
    """Cut a smooth-shaded sprite down to a console-sized palette."""
    im = im.convert("RGBA")
    alpha = im.split()[3]
    q = im.convert("RGB").quantize(colors=colours, method=Image.Quantize.MEDIANCUT)
    out = q.convert("RGBA")
    out.putalpha(alpha)
    return out


# ------------------------------------------------------------------ previews
def label(draw, xy, text, fill=(228, 230, 236)):
    draw.text(xy, text, fill=fill)


def build_tile_audit() -> Image.Image:
    from PIL import ImageDraw
    import sys
    sys.path.insert(0, str(ROOT / "tools"))
    import rtp_sheet as r                                    # noqa: E402

    pairs = [
        ("meadow grass", r.tile(r.REXT, 1, 10), "grass_0", "grass"),
        ("cobble road", r.tile(r.REXT, 10, 6), "cobble", "cobble"),
        ("dirt path", r.tile(r.RWLD, 7, 2), "dirt", "dirt"),
        ("forest floor", r.tile(r.REXT, 4, 10), "grass_2", "forest"),
    ]
    z, pad, head = 8, 12, 46
    cell = TILE * z
    w = pad * 4 + cell * 3
    h = head + len(pairs) * (cell + pad + 16)
    img = Image.new("RGB", (w, h), (22, 24, 30))
    d = ImageDraw.Draw(img)
    label(d, (pad, 8), "the 16-bit source tile", (150, 190, 255))
    label(d, (pad * 2 + cell, 8), "what the game ships today", (240, 150, 140))
    label(d, (pad * 3 + cell * 2, 8), "a 5-step ramp over the same pattern", (150, 230, 160))
    y = head
    for name, src, asset, rampkey in pairs:
        shipped = Image.open(ASSETS / "tiles" / f"{asset}.png")
        fixed = ramp_to(shipped, RAMP_16BIT[rampkey])
        s_spread, s_c = luminance_spread(src)
        p_spread, p_c = luminance_spread(shipped)
        f_spread, f_c = luminance_spread(fixed)
        label(d, (pad, y), name, (232, 234, 240))
        img.paste(src.convert("RGB").resize((cell, cell), Image.Resampling.NEAREST), (pad, y + 14))
        img.paste(shipped.convert("RGB").resize((cell, cell), Image.Resampling.NEAREST), (pad * 2 + cell, y + 14))
        img.paste(fixed.convert("RGB").resize((cell, cell), Image.Resampling.NEAREST), (pad * 3 + cell * 2, y + 14))
        yy = y + 14 + cell + 2
        label(d, (pad, yy), f"source: {s_c} col, spread {s_spread}", (150, 190, 255))
        label(d, (pad * 2 + cell, yy), f"shipped: {p_c} col, spread {p_spread}", (240, 150, 140))
        label(d, (pad * 3 + cell * 2, yy), f"5-step: {f_c} col, spread {f_spread}", (150, 230, 160))
        y = yy + 16 + pad
    return img


def build_scene(corrected: bool) -> Image.Image:
    """A small village plate reusing only registered runtime art."""
    T = TILE
    cols, rows = 22, 12
    scene = Image.new("RGBA", (cols * T, rows * T))
    tiles = {n: Image.open(ASSETS / "tiles" / f"{n}.png").convert("RGBA")
             for n in ("grass_0", "grass_1", "dirt", "cobble")}
    sprites = {n: Image.open(ASSETS / "sprites" / f"{n}.png").convert("RGBA")
               for n in ("house_0", "house_2", "shop", "tree_0", "tree_1", "well", "fence")}
    if corrected:
        tiles = {n: ramp_to(im, RAMP_16BIT[k])
                 for (n, k) in zip(("grass_0", "grass_1", "dirt", "cobble"),
                                   ("grass", "grass", "dirt", "cobble"))
                 for im in [tiles[n]]}
        sprites = {n: outline(posterise(im, 14)) for n, im in sprites.items()}

    ground = Image.new("RGBA", (cols * T, rows * T))
    for y in range(rows):
        for x in range(cols):
            ground.alpha_composite(tiles["grass_0" if (x + y) % 7 else "grass_1"], (x * T, y * T))
    scene.alpha_composite(ground)
    # a dirt lane and a cobbled square
    for x in range(cols):
        for y in (7, 8):
            scene.alpha_composite(tiles["dirt"], (x * T, y * T))
    for x in range(8, 15):
        for y in range(3, 8):
            scene.alpha_composite(tiles["cobble"], (x * T, y * T))

    def put(name, tx, ty):
        spr = sprites[name]
        scene.alpha_composite(spr, (tx * T + (T * 2 - spr.width) // 2 if spr.width < T * 4 else tx * T,
                                    ty * T))

    put("house_0", 1, 1)
    put("house_2", 15, 1)
    put("shop", 17, 4)
    for tx, ty in ((0, 4), (5, 0), (19, 0), (20, 7), (6, 9), (3, 9)):
        put("tree_0" if (tx + ty) % 2 else "tree_1", tx, ty)
    put("well", 11, 5)
    for tx in range(4, 8):
        scene.alpha_composite(sprites["fence"], (tx * T, 10 * T))
    return scene.convert("RGB")


def build_scene_audit() -> Image.Image:
    from PIL import ImageDraw
    a, b = build_scene(False), build_scene(True)
    gap = 10
    w = a.width * 2 + gap
    canvas = Image.new("RGB", (w, a.height + 24), (18, 20, 26))
    canvas.paste(a, (0, 24))
    canvas.paste(b, (a.width + gap, 24))
    d = ImageDraw.Draw(canvas)
    label(d, (4, 6), "shipped today", (240, 150, 140))
    label(d, (a.width + gap + 4, 6), "same assets: 5-step ramps + 1px contour", (150, 230, 160))
    return canvas.resize((canvas.width * 2, canvas.height * 2), Image.Resampling.NEAREST)


# ------------------------------------------------------------------ main
def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    report: dict = {"screens": [], "ground_tiles": [], "reference": None}

    report["ground_tiles"] = [
        measure_asset(name, ASSETS / "tiles" / f"{name}.png")
        for name in ("grass_0", "cobble", "dirt", "grass_2", "water_0", "flagstone")
    ]

    for shot in sorted((ROOT / "docs" / "screenshots").glob("*.png")):
        im = Image.open(shot)
        # screenshots are integer upscales of the 512x288 canvas
        if im.width > 512:
            im = im.resize((512, 288), Image.Resampling.NEAREST)
        report["screens"].append(measure_screen(shot.name, im))

    ref = next((p for p in REFERENCE_CANDIDATES if p.is_file()), None)
    if ref:
        report["reference"] = measure_screen(
            "Lufia II reference (Elcid, SNESMaps) — study only, not shipped",
            Image.open(ref))

    build_tile_audit().save(OUT / "tile_contrast.png")
    build_scene_audit().save(OUT / "scene_before_after.png")

    (OUT / "audit.json").write_text(json.dumps(report, indent=2) + "\n")

    print("screens (512x288 canvas):")
    for s in report["screens"]:
        print(f"  {s['label']:22s} colours={s['colours']:5d}  per-tile mean={s['colours_per_tile_mean']:5.1f}"
              f"  contrast={s['local_contrast']:5.2f}")
    if report["reference"]:
        r = report["reference"]
        print(f"  {r['label'][:22]:22s} colours={r['colours']:5d}  per-tile mean={r['colours_per_tile_mean']:5.1f}"
              f"  contrast={r['local_contrast']:5.2f}   <- the target")
    print("\nground tiles (shipped):")
    for t in report["ground_tiles"]:
        print(f"  {t['file']:28s} colours={t['colours']:2d}  luminance spread={t['luminance_spread']:3d}/255")
    print(f"\nwrote {OUT.relative_to(ROOT)}/tile_contrast.png, scene_before_after.png, audit.json")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
