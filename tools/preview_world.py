#!/usr/bin/env python3
"""preview_world.py — draw the generated Lufia II world with the shipped art.

This is an *offline* preview, not a screenshot: it compiles the headless
`tools/dump_world.cpp`, asks it for one continent, then composites the real
runtime atlas (`assets/tiles/ow_atlas.png`), the miniature site icons and the
hero sheet exactly the way `src/world.cpp::worldDrawOverworld` does. It exists so
the world can be judged on a machine with no GPU, and so a change to the
geography can be looked at before it ships.

    python3 tools/preview_world.py
    python3 tools/preview_world.py --seed 4444 --region 0 0

Writes:
    docs/screenshots/overworld.png     512x288, the opening view at Elmswick
    docs/screenshots/overworld_castle.png  512x288, the north road and pass
    docs/screenshots/world_map.png     960x960, the whole homeland continent
"""
from __future__ import annotations

import argparse
import os
import pathlib
import subprocess
import tempfile

from PIL import Image, ImageDraw

ROOT = pathlib.Path(__file__).resolve().parents[1]
A = ROOT / "assets"
OUT = ROOT / "docs" / "screenshots"
T = 16
VW, VH = 512, 288
REGION = 320
HALF = REGION // 2

# OwTileKind
DEEP_SEA, SEA, GRASS, FOREST_LEAF, FOREST_PINE, SAND, SNOW, FOREST_SNOW, SWAMP, \
    MOUNTAIN, MOUNTAIN_SNOW, BRIDGE_H, BRIDGE_V, PASS_H, PASS_V = range(15)

WATER = {DEEP_SEA, SEA}
MOUNTAIN_KINDS = {MOUNTAIN, MOUNTAIN_SNOW}
FOREST_KINDS = {FOREST_LEAF, FOREST_PINE, FOREST_SNOW}

TERRAIN_COLOURS = {
    DEEP_SEA: (18, 52, 98), SEA: (40, 98, 150), GRASS: (86, 146, 62),
    FOREST_LEAF: (44, 96, 40), FOREST_PINE: (38, 84, 38), SAND: (226, 200, 138),
    SNOW: (232, 240, 246), FOREST_SNOW: (176, 198, 200), SWAMP: (66, 102, 76),
    MOUNTAIN: (124, 100, 70), MOUNTAIN_SNOW: (176, 182, 190),
    BRIDGE_H: (200, 156, 100), BRIDGE_V: (200, 156, 100),
    PASS_H: (186, 162, 114), PASS_V: (186, 162, 114),
}

SITE_ICONS = {
    1: "ow_hamlet.png", 2: "ow_town.png", 3: "ow_town.png", 4: "ow_castle_grey.png",
    5: "ow_cave_grass.png", 6: "ow_tower.png", 7: "ow_temple.png", 8: "ow_ruins.png",
    9: "ow_camp.png",
}


def load(path: pathlib.Path) -> Image.Image:
    return Image.open(path).convert("RGBA")


class World:
    def __init__(self, seed: int, rx: int, ry: int):
        self.seed = seed
        self.rx = rx
        self.ry = ry
        self.base_x = rx * REGION
        self.base_y = ry * REGION
        self.kinds: list[list[int]] = []
        self.roads: list[list[int]] = []
        self.sites: list[dict] = []
        self.name = ""
        self.title = ""

    def kind(self, lx: int, ly: int) -> int:
        if lx < -HALF or lx >= HALF or ly < -HALF or ly >= HALF:
            return DEEP_SEA
        return self.kinds[ly + HALF][lx + HALF]

    def road(self, lx: int, ly: int) -> int:
        if lx < -HALF or lx >= HALF or ly < -HALF or ly >= HALF:
            return 0
        return self.roads[ly + HALF][lx + HALF]


def dump_world(seed: int, rx: int, ry: int) -> World:
    with tempfile.TemporaryDirectory(prefix="apfa-preview-") as temp:
        binary = pathlib.Path(temp) / "dump_world"
        subprocess.run([
            os.environ.get("CXX", "c++"), "-std=c++17", "-O2",
            f"-I{ROOT / 'src'}",
            str(ROOT / "src" / "overworld_gen.cpp"),
            str(ROOT / "tools" / "dump_world.cpp"),
            "-o", str(binary),
        ], check=True, cwd=ROOT)
        text = subprocess.run([str(binary), str(seed), str(rx), str(ry)],
                              check=True, capture_output=True, text=True).stdout

    w = World(seed, rx, ry)
    lines = text.splitlines()
    for line in lines:
        if line.startswith("seed "):
            continue
        if line.startswith("region "):
            continue
        if line.startswith("name "):
            w.name = line[5:]
            continue
        if line.startswith("title "):
            w.title = line[6:]
            continue
        if line.startswith("site "):
            parts = line.split()
            w.sites.append({
                "x": int(parts[1]), "y": int(parts[2]), "type": int(parts[3]),
                "climate": int(parts[4]), "tide": int(parts[5]),
                "name": " ".join(parts[6:]).replace("_", " "),
            })
            continue
        if len(line) == REGION * 2:
            kinds = [ord(line[i * 2]) - ord("a") for i in range(REGION)]
            roads = [1 if line[i * 2 + 1] == "1" else 0 for i in range(REGION)]
            w.kinds.append(kinds)
            w.roads.append(roads)
    return w


def site_tile(w: World, site_type: int, climate: int) -> Image.Image:
    name = SITE_ICONS.get(site_type, "ow_ruins.png")
    if site_type == 5 and climate == 1:
        name = "ow_cave_sand.png"
    if site_type == 5 and climate == 2:
        name = "ow_cave_snow.png"
    if site_type == 1 and climate == 2:
        name = "ow_town_snow.png"
    if site_type == 3 and climate == 2:
        name = "ow_town_snow.png"
    return load(A / "sprites" / name)


def stamp(dst: Image.Image, src: Image.Image, x: int, y: int) -> None:
    if src.getbbox():
        dst.alpha_composite(src, (x, y))


def draw_ground_layer(w: World, img: Image.Image, lx0: int, ly0: int, cols: int, rows: int,
                      frame: int = 0) -> None:
    atlas = load(A / "tiles" / "ow_atlas.png")

    def cell(col: int, row: int) -> Image.Image:
        return atlas.crop((col * T, row * T, col * T + T, row * T + T))

    for ry in range(rows):
        for rx in range(cols):
            lx, ly = lx0 + rx, ly0 + ry
            k = w.kind(lx, ly)
            dx, dy = rx * T, ry * T

            if k == DEEP_SEA:
                stamp(img, cell(4 + ((frame + lx + ly) & 3), 0), dx, dy)
                continue
            stamp(img, cell((frame + ((lx + ly) & 1)) & 3, 0), dx, dy)
            if k == SEA:
                continue

            kN, kE, kS, kW = w.kind(lx, ly - 1), w.kind(lx + 1, ly), w.kind(lx, ly + 1), w.kind(lx - 1, ly)
            land = lambda q: q not in WATER
            lmask = (1 if land(kN) else 0) | (2 if land(kE) else 0) | (4 if land(kS) else 0) | (8 if land(kW) else 0)
            is_sand = k == SAND
            is_snow = k in (SNOW, FOREST_SNOW, MOUNTAIN_SNOW)

            if is_sand and lmask != 15:
                stamp(img, cell(lmask, 2), dx, dy)
            elif is_snow and lmask != 15:
                stamp(img, cell(lmask, 3), dx, dy)
            else:
                stamp(img, cell(lmask, 1), dx, dy)
                if is_sand:
                    sm = (1 if kN == SAND or kN in WATER else 0) | (2 if kE == SAND or kE in WATER else 0) | \
                         (4 if kS == SAND or kS in WATER else 0) | (8 if kW == SAND or kW in WATER else 0)
                    stamp(img, cell(sm, 4), dx, dy)
                elif is_snow:
                    snowish = lambda q: q in (SNOW, FOREST_SNOW, MOUNTAIN_SNOW) or q in WATER
                    sm = (1 if snowish(kN) else 0) | (2 if snowish(kE) else 0) | \
                         (4 if snowish(kS) else 0) | (8 if snowish(kW) else 0)
                    stamp(img, cell(sm, 5), dx, dy)

            if k in FOREST_KINDS:
                fm = (1 if kN in FOREST_KINDS else 0) | (2 if kE in FOREST_KINDS else 0) | \
                     (4 if kS in FOREST_KINDS else 0) | (8 if kW in FOREST_KINDS else 0)
                row = 6 if k == FOREST_LEAF else (7 if k == FOREST_PINE else 8)
                stamp(img, cell(fm, row), dx, dy)
            elif k == SWAMP:
                sm = (1 if kN == SWAMP else 0) | (2 if kE == SWAMP else 0) | \
                     (4 if kS == SWAMP else 0) | (8 if kW == SWAMP else 0)
                stamp(img, cell(sm, 13), dx, dy)
            elif k in MOUNTAIN_KINDS:
                mm = (1 if kN in MOUNTAIN_KINDS else 0) | (2 if kE in MOUNTAIN_KINDS else 0) | \
                     (4 if kS in MOUNTAIN_KINDS else 0) | (8 if kW in MOUNTAIN_KINDS else 0)
                stamp(img, cell(mm, 10 if k == MOUNTAIN_SNOW else 9), dx, dy)
            elif k == SAND and not w.road(lx, ly) and lmask == 15:
                h = (lx * 73856093) ^ (ly * 19349663) ^ w.seed
                if h % 19 == 0:
                    stamp(img, cell(13, 0), dx, dy)
                elif h % 29 == 0:
                    stamp(img, cell(14, 0), dx, dy)

            if k == BRIDGE_H:
                stamp(img, cell(8, 0), dx, dy)
            elif k == BRIDGE_V:
                stamp(img, cell(9, 0), dx, dy)
            elif k == PASS_V:
                stamp(img, cell(10, 0), dx, dy)
            elif k == PASS_H:
                stamp(img, cell(11, 0), dx, dy)
            elif w.road(lx, ly):
                def road_conn(nx: int, ny: int) -> bool:
                    nk = w.kind(nx, ny)
                    return bool(w.road(nx, ny)) or nk in (BRIDGE_H, BRIDGE_V, PASS_H, PASS_V)
                rm = (1 if road_conn(lx, ly - 1) else 0) | (2 if road_conn(lx + 1, ly) else 0) | \
                     (4 if road_conn(lx, ly + 1) else 0) | (8 if road_conn(lx - 1, ly) else 0)
                stamp(img, cell(rm, 11), dx, dy)


def draw_sites(w: World, img: Image.Image, lx0: int, ly0: int, cols: int, rows: int,
               small: float = 1.0) -> None:
    for s in w.sites:
        sx, sy = s["x"] - w.base_x, s["y"] - w.base_y
        if sx < lx0 - 3 or sx > lx0 + cols + 2 or sy < ly0 - 3 or sy > ly0 + rows + 2:
            continue
        icon = site_tile(w, s["type"], s["climate"])
        if small != 1.0:
            icon = icon.resize((max(1, int(icon.width * small)), max(1, int(icon.height * small))),
                               Image.NEAREST)
        cx = (sx - lx0 + 0.5) * T
        foot = (sy - ly0 + 0.92) * T
        # soft ground shadow under the icon
        shadow = Image.new("RGBA", (icon.width, 6), (0, 0, 0, 0))
        shadow.paste((0, 0, 0, 56), (0, 2, icon.width, 5))
        img.alpha_composite(shadow, (int(cx - icon.width / 2), int(foot - 3)))
        stamp(img, icon, int(cx - icon.width / 2), int(foot - icon.height))


def draw_scene(w: World, cx: int, cy: int, out: pathlib.Path, hero: bool = True,
               hero_dy: int = 3) -> None:
    cols, rows = VW // T + 2, VH // T + 2
    lx0, ly0 = cx - cols // 2, cy - rows // 2
    img = Image.new("RGBA", (VW, VH), (0, 0, 0, 255))
    draw_ground_layer(w, img, lx0, ly0, cols, rows)
    draw_sites(w, img, lx0, ly0, cols, rows)

    if hero:
        sheet = load(A / "sprites" / "hero_knight.png")
        frame = sheet.crop((0, 0, 24, 32))
        hero_img = frame.resize((19, 25), Image.NEAREST)
        hx = (cx - lx0 + 0.5) * T
        hy = (cy + hero_dy - ly0 + 0.35) * T
        stamp(img, hero_img, int(hx - 19 / 2), int(hy - 25))

    img.convert("RGB").save(out)
    print("wrote", out.relative_to(ROOT), f"({VW}x{VH})")


def draw_world_map(w: World, out: pathlib.Path, scale: int = 3) -> None:
    size = REGION * scale
    img = Image.new("RGB", (size, size), (18, 52, 98))
    d = ImageDraw.Draw(img)
    for ly in range(-HALF, HALF):
        for lx in range(-HALF, HALF):
            k = w.kind(lx, ly)
            c = TERRAIN_COLOURS[k]
            if w.road(lx, ly):
                c = (226, 192, 120)
            x, y = (lx + HALF) * scale, (ly + HALF) * scale
            d.rectangle([x, y, x + scale - 1, y + scale - 1], fill=c)

    # Site icons and labels
    for s in w.sites:
        sx, sy = s["x"] - w.base_x, s["y"] - w.base_y
        icon = site_tile(w, s["type"], s["climate"])
        icon = icon.resize((max(2, icon.width // 2), max(2, icon.height // 2)), Image.NEAREST)
        px = int((sx + HALF + 0.5) * scale - icon.width / 2)
        py = int((sy + HALF + 0.92) * scale - icon.height)
        img.paste(icon, (px, py), icon)
        if s["type"] in (1, 2, 3, 4):
            label = s["name"]
            tw = d.textlength(label)
            d.rectangle([px - 1, py - 9, px + tw + 1, py - 1], fill=(16, 20, 28))
            d.text((px, py - 8), label, fill=(255, 238, 190))

    # Compass rose and caption
    d.rectangle([8, 8, 232, 28], fill=(16, 20, 28))
    d.text((13, 12), f"{w.name.upper()}  seed {w.seed}", fill=(255, 232, 150))
    cx = size // 2
    d.polygon([(size - 26, 20), (size - 18, 44), (size - 26, 38), (size - 34, 44)], fill=(230, 226, 210))
    d.text((size - 32, 48), "N", fill=(230, 226, 210))
    img.save(out)
    print("wrote", out.relative_to(ROOT), f"({size}x{size})")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--seed", type=int, default=1337)
    ap.add_argument("--region", type=int, nargs=2, default=[0, 0])
    args = ap.parse_args()

    OUT.mkdir(parents=True, exist_ok=True)
    w = dump_world(args.seed, args.region[0], args.region[1])
    if len(w.kinds) != REGION:
        raise SystemExit(f"dump_world returned {len(w.kinds)} rows")

    # opening view: Elmswick's vale
    elm = next((s for s in w.sites if s["name"] == "Elmswick"),
               {"x": w.base_x, "y": w.base_y})
    draw_scene(w, elm["x"] - w.base_x, elm["y"] - w.base_y - 1, OUT / "overworld.png")

    # the north road: a pass and the Castle of Elm
    castle = next((s for s in w.sites if s["name"] == "Castle of Elm"), None)
    if castle:
        draw_scene(w, castle["x"] - w.base_x, castle["y"] - w.base_y + 3,
                   OUT / "overworld_castle.png")

    # the south coast, where the boat is hired and the Tidewell isles wait
    draw_scene(w, 6, 66, OUT / "overworld_coast.png", hero_dy=0)

    draw_world_map(w, OUT / "world_map.png")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
