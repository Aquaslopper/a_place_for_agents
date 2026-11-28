#!/usr/bin/env python3
"""Extract verified OpenRTP town/interior art without replacing existing art.

    python3 tools/town_art.py [--check]

Crop definitions use zero-based (column, row, width, height) for chipset
cells. Only props that straddle cells use pixel rectangles. All output is
native resolution; scaling and backgrounds are only for the review sheet.
"""
import argparse
from dataclasses import dataclass

from PIL import Image, ImageDraw

from rtp_sheet import (EXT, INT, KEY_PINK, KEY_PINK2, ROOT, keyed, keyed_any,
                       load, save, REXT)
from landscape_art import ramp

OBJECTS = "CharSet/Object1.png"
SHEETS = {
    EXT: ((480, 256), KEY_PINK),
    INT: ((480, 256), KEY_PINK),
    REXT: ((480, 256), None),   # multi-key retro sheet: keyed_any
    OBJECTS: ((288, 256), KEY_PINK2),
}

# Muted families for the Interior wall faces (ART_BIBLE 10.3): one per face
# strip, top to bottom, matching the two-row-per-face atlas layout.
WALL_FAMILIES = (
    '4e4458 62566c 786a80 8e8094 a294a6',   # purple brick -> wine stone
    '9c8f74 b0a488 c2b69a d2c6aa e0d4b8',   # plaster -> cream
    '8b8d89 a1a49c b0b0a2 c1beac d0cab3',   # pale stone (stone_floor family)
    '4a5258 5c666c 707a7e 86908e 9ca49c',   # dark stone -> slate
    '77685c 8c7c6e a29282 b6a694 c8b8a6',   # rubble -> warm grey
    '4c5c74 5e6e88 72829c 8696ae 9aaabe',   # blue brick -> blue slate
)
PREVIEW = ROOT.parent / "docs/town_assets/town_interior_crops.png"


@dataclass(frozen=True)
class Crop:
    folder: str
    name: str
    sheet: str
    box: tuple[int, int, int, int]
    clear: tuple[int, int, int, int] | None = None  # crop-local, half-open

    @property
    def filename(self):
        return self.name + ".png"


def cells(name, sheet, c, r, w=1, h=1, *, folder="sprites"):
    return Crop(folder, name, sheet, (16*c, 16*r, 16*(c+w), 16*(r+h)))


def door(name, column):
    # Stack three 24×32 animation poses. The source repeats each pose across
    # the three columns of its charset slot, so only one column is needed.
    return Crop("sprites", name, OBJECTS, (column*24, 0, (column+1)*24, 96))


CROPS = (
    # Town materials and perimeter. These opaque strips keep the 16px grid.
    cells("town_facade_atlas", REXT, 12, 0, 6, 6, folder="tiles"),
    cells("town_masonry_atlas", REXT, 14, 9, 4, 3, folder="tiles"),
    cells("town_battlement_atlas", REXT, 18, 0, 6, 5, folder="tiles"),
    cells("town_round_turret", EXT, 24, 0, 2, 2),
    cells("town_broken_turret", EXT, 26, 0, 2, 2),
    cells("town_gate_double", EXT, 28, 8, 2, 2),
    cells("town_gable_red", EXT, 24, 11, 2, 2),
    cells("town_gable_blue", EXT, 26, 11, 2, 2),
    cells("town_shingles_red_atlas", REXT, 14, 12, 2, 2, folder="tiles"),
    cells("town_shingles_blue_atlas", REXT, 16, 14, 2, 2, folder="tiles"),
    cells("town_sign_tankard", EXT, 22, 13),
    cells("town_sign_weapon", EXT, 22, 14),
    cells("town_sign_inn", EXT, 23, 14),
    cells("town_sign_armor", EXT, 22, 15),
    cells("town_sign_goods", EXT, 23, 15),
    cells("town_market_flowers_orange", REXT, 22, 6),
    cells("town_market_flowers_red", REXT, 23, 6),
    cells("town_market_sacks", EXT, 28, 2),
    cells("town_banner_vert", EXT, 29, 5, 1, 2),
    cells("town_banner_short", EXT, 28, 6),
    cells("town_banner_wide", EXT, 28, 7, 2, 1),
    cells("town_wall_ivy", REXT, 25, 8, 1, 3),

    # Interior walls and doorway. Six two-row face strips; not cap autotiles.
    cells("interior_wall_face_atlas", INT, 14, 0, 4, 12, folder="tiles"),
    cells("interior_open_doorway", INT, 26, 3, 1, 2),
    cells("interior_window_arch", INT, 24, 1),
    cells("interior_window_shutters", INT, 26, 1),
    cells("interior_window_stained", INT, 24, 4),
    cells("interior_painting_small", INT, 24, 2),
    cells("interior_painting_wide", INT, 24, 3, 2, 1),
    cells("interior_curtain_panel", INT, 22, 4, 2, 3),
    cells("interior_curtain_pair", INT, 18, 10, 2, 3),
    cells("interior_stone_stairs", INT, 21, 3, 1, 3),

    # Furniture and detail. Reuse the shipped shelf, upright bed and hearth;
    # the following are different modules rather than replacements for them.
    # Adjacent bones are in the bottom-right of this otherwise tight crop.
    Crop("sprites", "interior_corner_counter", INT, (293, 211, 331, 255), (26, 30, 38, 44)),
    cells("interior_bookcase_narrow", INT, 27, 4, 1, 2),
    cells("interior_shop_bottles", INT, 23, 8, 1, 2),
    cells("interior_shop_swords", INT, 20, 8, 2, 1),
    cells("interior_shop_armor", INT, 20, 9, 1, 1),
    cells("interior_shop_herbs", INT, 25, 0),
    cells("interior_bed_side", INT, 25, 11, 2, 1),
    cells("interior_piano", INT, 28, 11, 2, 1),
    cells("interior_crystal_ball", INT, 29, 10),
    cells("interior_royal_chair", INT, 26, 14, 1, 2),
    cells("interior_throne", INT, 27, 14, 3, 2),
    Crop("sprites", "interior_ritual_circle", INT, (339, 194, 381, 237)),
    door("interior_door_wood_panel", 0),
    door("interior_door_wood_plank", 3),
    door("interior_door_iron_trim", 6),
)


def extract():
    sheets = {}
    for source, (size, key) in SHEETS.items():
        image = load(source)
        if image.size != size:
            raise ValueError(f"Unexpected source sheet format: {source}")
        if key is None:
            keyed_any(image)
        else:
            keyed(image, key)
        if image.getextrema()[3][0] != 0:
            raise ValueError(f"Transparency key not found: {source}")
        sheets[source] = image
    for crop in CROPS:
        sheet = sheets[crop.sheet]
        x0, y0, x1, y1 = crop.box
        if not (0 <= x0 < x1 <= sheet.width and 0 <= y0 < y1 <= sheet.height):
            raise ValueError(f"Crop outside source sheet: {crop.name}")
        image = sheet.crop(crop.box)
        if crop.clear is not None:
            image.paste((0, 0, 0, 0), crop.clear)
        if not image.getbbox():
            raise ValueError(f"Empty crop: {crop.name}")
        if crop.name == "interior_wall_face_atlas":
            # Six faces, two 16px rows each: mute each into its own family.
            for band, family in enumerate(WALL_FAMILIES):
                y0, y1 = band * 32, (band + 1) * 32
                face = image.crop((0, y0, image.width, y1))
                image.paste(ramp(face, family), (0, y0))
        yield crop, image


def build_town_art():
    for crop, image in extract():
        save(image, crop.folder, crop.filename)


def contact_sheet(crops):
    cols, cell_w, cell_h = 5, 218, 132
    groups = (("Town exterior (SNES-retro materials)", REXT), ("Town exterior", EXT),
              ("Interior rooms", INT), ("Door poses (open to closed)", OBJECTS))
    pad, header = 18, 30
    height = pad * 2 + header + sum(
        header + ((sum(c.sheet == source for c, _ in crops) + cols - 1) // cols) * cell_h
        for _, source in groups
    )
    preview = Image.new("RGB", (cols * cell_w + 2 * pad, height), "#151d2c")
    draw = ImageDraw.Draw(preview)
    draw.text((pad, pad), "PROCEDURAL JRPG WORLD  /  TOWN & INTERIOR CROP REVIEW", fill="#f1d483")
    y = pad + header
    for title, source in groups:
        draw.text((pad, y), title, fill="#bcd3ed")
        y += header
        group = [(c, im) for c, im in crops if c.sheet == source]
        for i, (crop, image) in enumerate(group):
            x = pad + (i % cols) * cell_w
            top = y + (i // cols) * cell_h
            draw.rectangle((x, top, x + cell_w - 7, top + cell_h - 7), fill="#2b3649")
            scale = 2 if image.width * 2 < cell_w - 16 and image.height * 2 < cell_h - 30 else 1
            if image.height > cell_h - 30:  # long atlas: show its bands side by side
                panels = [image.crop((0, a, image.width, min(a + 96, image.height)))
                          for a in range(0, image.height, 96)]
            else:
                panels = [image.resize((image.width * scale, image.height * scale), Image.Resampling.NEAREST)]
            px = x + (cell_w - 7 - sum(p.width for p in panels) - 4 * (len(panels) - 1)) // 2
            for panel in panels:
                preview.paste(panel, (px, top + 5 + (cell_h - 30 - panel.height) // 2), panel)
                px += panel.width + 4
            draw.text((x + 7, top + cell_h - 22), crop.name.removeprefix("interior_").removeprefix("town_"), fill="#e2ecf4")
        y += ((len(group) + cols - 1) // cols) * cell_h
    return preview


def matches(path, expected):
    if not path.is_file():
        return False
    with Image.open(path) as actual:
        return actual.mode == expected.mode and actual.size == expected.size and actual.tobytes() == expected.tobytes()


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true", help="compare committed crops and preview to fresh extraction")
    args = parser.parse_args()
    crops = list(extract())
    outputs = [(ROOT / crop.folder / crop.filename, image) for crop, image in crops]
    outputs.append((PREVIEW, contact_sheet(crops)))
    if args.check:
        stale = [str(path.relative_to(ROOT.parent)) for path, image in outputs if not matches(path, image)]
        if stale:
            parser.exit(1, "Stale or missing town art: " + ", ".join(stale) + "\n")
        print(f"Town art: verified {len(crops)} crops and preview")
    else:
        for path, image in outputs:
            path.parent.mkdir(parents=True, exist_ok=True)
            image.save(path)
        print(f"Town art: wrote {len(crops)} crops and {PREVIEW.relative_to(ROOT.parent)}")


if __name__ == "__main__":
    main()
