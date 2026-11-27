#!/usr/bin/env python3
"""import_openrtp.py — adapt the vendored OpenRTP sheets to this game's layout.

Every pixel the game draws — terrain, buildings, trees, props, heroes, villagers,
monsters, item icons, the window skin, battle effects and title art — comes from
the OpenRTP package vendored under ``assets/source/openrtp/`` (see CREDITS.md for
authors and licences).  This script only *adapts* those pixels:

  * it crops chipset tiles (16x16 cells),
  * cuts sprites out of their magenta / cyan colour key,
  * re-orders RPG Maker 2000 walking frames (idle/step/step x down/left/right/up)
    into the row order this renderer draws (down / up / side),
  * composes cottages, a castle and battle effects out of the original pieces.

    python3 tools/import_openrtp.py

Two colour keys are in play: ``(0,147,146)`` on the charset sheets and
``(255,103,139)`` on the chipsets.  Tile coordinates are (column, row) of the
16x16 grid and were checked against the sheets with ``tools/..`` scratch zooms.
"""
from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent.parent / 'assets'
SRC = ROOT / 'source' / 'openrtp'

KEY_CYAN = (0, 147, 146)      # RPG Maker 2000 charset key
KEY_PINK = (255, 103, 139)
KEY_PINK2 = (254, 103, 138)   # the object charsets use a one-off pink    # OpenRTP chipset key

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


# =================================================================== 1. tiles
def build_tiles():
    """Ground textures, used as seamless 16x16 fills by the world renderer."""
    # --- meadow grass: four shades so the plains never look flat
    save(tile(EXT, 5, 8), 'tiles', 'grass_0.png')
    save(tile(EXT, 7, 8), 'tiles', 'grass_1.png')
    save(tile(WLD, 2, 9), 'tiles', 'grass_2.png')
    save(tile(WLD, 4, 10), 'tiles', 'grass_3.png')
    save(tile(EXT, 7, 9), 'tiles', 'grass_dark.png')

    # --- paths, fields, sand
    save(tile(WLD, 22, 3), 'tiles', 'dirt.png')        # packed earth road
    save(tile(EXT, 6, 8), 'tiles', 'soil.png')           # tilled field
    save(tile(EXT, 3, 12), 'tiles', 'sand.png')          # loose yellow sand
    save(tile(EXT, 4, 14), 'tiles', 'beach.png')         # pale shoreline sand
    save(tile(EXT, 10, 4), 'tiles', 'arcane.png')        # crystal ground

    # --- stone
    save(tile(EXT, 9, 4), 'tiles', 'cobble.png')         # village plaza (setts)
    save(tile(EXT, 9, 4), 'tiles', 'flagstone.png')
    save(tile(EXT, 10, 6), 'tiles', 'rock_hill.png')     # lichen stone
    save(tile(EXT, 10, 6), 'tiles', 'rock_hill2.png')
    save(tile(EXT, 12, 11), 'tiles', 'brick.png')        # pale brick
    save(tile(EXT, 12, 11), 'tiles', 'brick_dark.png')
    save(tile(WLD, 12, 8), 'tiles', 'dungeon_floor.png')

    # --- cold
    save(tile(EXT, 6, 0), 'tiles', 'snow.png')
    save(tile(INT, 11, 4), 'tiles', 'snow_drift.png')
    save(tile(WLD, 7, 6), 'tiles', 'tundra.png')
    save(tile(DUN, 8, 2), 'tiles', 'ice.png')
    save(tile(DUN, 10, 2), 'tiles', 'ice_crack.png')

    # --- water
    save(tile(EXT, 1, 2), 'tiles', 'water_0.png')
    save(tile(EXT, 2, 2), 'tiles', 'water_1.png')
    save(tile(DUN, 1, 3), 'tiles', 'deep_water.png')
    save(tile(EXT, 13, 8), 'tiles', 'swamp.png')

    # --- interior flooring (shop / inn / temple interiors)
    save(tile(INT, 18, 4), 'tiles', 'plank_floor.png')
    save(tile(INT, 14, 4), 'tiles', 'tile_floor.png')
    save(tile(INT, 15, 4), 'tiles', 'stone_floor.png')
    save(tile(INT, 16, 4), 'tiles', 'carpet.png')

    # --- ground overlays: flowers, grass tufts, mushrooms, pebbles
    save(trim(tcell(EXT, 18, 11, 1, 1, KEY_PINK)), 'tiles', 'flower_0.png')   # scattered blossoms
    save(trim(tcell(EXT, 18, 9, 1, 1, KEY_PINK)), 'tiles', 'flower_1.png')    # flowering shrub
    save(trim(tcell(EXT, 19, 9, 1, 1, KEY_PINK)), 'tiles', 'tuft_0.png')      # flowered shrub
    save(trim(tcell(EXT, 19, 8, 1, 1, KEY_PINK)), 'tiles', 'tuft_1.png')      # toadstools
    save(trim(tcell(EXT, 21, 9, 1, 1, KEY_PINK)), 'tiles', 'tuft_2.png')      # leafy bush
    save(trim(tcell(EXT, 18, 8, 1, 1, KEY_PINK)), 'tiles', 'pebble.png')      # pebble/stone


# ================================================================= 2. objects
def build_objects():
    """Trees, rocks, bushes, fences, street furniture and landmarks."""
    k = KEY_PINK

    # ---- trees -----------------------------------------------------------
    broad = trim(tcell(EXT, 22, 8, 2, 2, k))            # broad-leaf shade tree
    save(centered(broad, (40, 42)), 'sprites', 'tree_0.png')
    save(centered(broad.transpose(Image.Transpose.FLIP_LEFT_RIGHT), (40, 42)),
         'sprites', 'tree_1.png')
    broad2 = trim(tcell(WLD, 18, 10, 2, 2, k))          # deep-forest crown
    save(centered(broad2, (46, 48)), 'sprites', 'tree_2.png')
    conifer = trim(tcell(EXT, 20, 8, 1, 2, k))          # conifer, one tile wide
    save(centered(conifer, (28, 46)), 'sprites', 'pine_0.png')
    save(centered(conifer, (24, 40)), 'sprites', 'pine_1.png')
    save(centered(conifer.transpose(Image.Transpose.FLIP_LEFT_RIGHT), (28, 46)),
         'sprites', 'pine_2.png')
    dead = trim(tcell(EXT, 21, 8, 1, 2, k))             # bare / dead tree
    save(centered(dead, (24, 38)), 'sprites', 'dead_tree.png')
    # snow-capped tree from the world sheet
    snowy = trim(tcell(WLD, 19, 8, 1, 2, k))
    save(centered(snowy, (26, 40)), 'sprites', 'tree_snow.png')

    # ---- rocks -----------------------------------------------------------
    lump = trim(tcell(DUN, 21, 9, 1, 1, k))             # mossy grey boulder
    save(centered(lump, (20, 18)), 'sprites', 'rock_0.png')
    save(centered(lump.transpose(Image.Transpose.FLIP_LEFT_RIGHT), (20, 18)),
         'sprites', 'rock_1.png')
    save(centered(trim(tcell(DUN, 22, 10, 1, 1, k)), (22, 18)), 'sprites', 'rock_2.png')
    # cave / mountain props from the dungeon sheet
    save(centered(trim(tcell(DUN, 22, 10, 1, 1, k)), (28, 26)), 'sprites', 'boulder.png')
    # blue crystal cluster: a 1x2 object, scales up into a proper landmark
    save(centered(trim(rect(DUN, 19 * 16, 9 * 16, 16, 32, k)), (22, 30)), 'sprites', 'crystal.png')

    # ---- plants ----------------------------------------------------------
    save(trim(rect(WLD, 25 * 16, 12 * 16, 16, 16, k)), 'sprites', 'stump.png')
    save(centered(trim(tcell(EXT, 19, 9, 1, 1, k)), (18, 18)), 'sprites', 'bush.png')
    save(centered(trim(tcell(EXT, 20, 9, 1, 1, k)), (16, 16)), 'sprites', 'bush_plain.png')
    save(tcell(EXT, 19, 8, 1, 1, k), 'sprites', 'mushroom.png')
    save(trim(tcell(EXT, 21, 11, 1, 1, k)), 'sprites', 'potted_plant.png')
    save(trim(tcell(EXT, 22, 11, 1, 1, k)), 'sprites', 'urn.png')

    # ---- street furniture ------------------------------------------------
    # the two-tile stone well from the object bank (24,0)
    save(trim(tcell(EXT, 24, 0, 2, 2, k)), 'sprites', 'well.png')
    save(centered(trim(tcell(EXT, 20, 10, 1, 1, k)), (20, 18)), 'sprites', 'sign.png')
    save(trim(tcell(EXT, 28, 10, 1, 1, k)), 'sprites', 'bench.png')
    save(trim(tcell(EXT, 18, 10, 1, 1, k)), 'sprites', 'torch.png')
    save(trim(tcell(EXT, 21, 12, 1, 1, k)), 'sprites', 'campfire.png')
    save(trim(tcell(EXT, 22, 12, 1, 1, k)), 'sprites', 'cauldron.png')
    save(trim(tcell(EXT, 23, 11, 1, 1, k)), 'sprites', 'basin.png')
    save(trim(tcell(EXT, 26, 10, 1, 1, k)), 'sprites', 'mailbox.png')

    # ---- fences ----------------------------------------------------------
    save(trim(rect(EXT, 19 * 16, 12 * 16, 16, 16, k)), 'sprites', 'fence.png')
    save(trim(rect(EXT, 18 * 16, 12 * 16, 16, 16, k)), 'sprites', 'fence_post.png')
    save(trim(rect(EXT, 21 * 16, 14 * 16, 16, 16, k)), 'sprites', 'fence_gate.png')
    save(trim(rect(EXT, 22 * 16, 10 * 16, 16, 16, k)), 'sprites', 'ladder.png')

    # ---- landmarks -------------------------------------------------------
    save(centered(trim(tcell(EXT, 26, 8, 1, 2, k)), (24, 40)), 'sprites', 'statue.png')
    save(centered(trim(tcell(EXT, 27, 8, 1, 2, k)), (22, 40)), 'sprites', 'pillar.png')
    save(trim(tcell(EXT, 24, 13, 3, 3, k)), 'sprites', 'monument.png')
    save(trim(tcell(EXT, 27, 13, 3, 3, k)), 'sprites', 'pavilion.png')

    # ---- interior dressing (shops, inns, houses) -------------------------
    save(trim(tcell(WLD, 25, 10, 1, 1, k)), 'sprites', 'barrel.png')
    save(trim(tcell(EXT, 24, 3, 1, 1, k)), 'sprites', 'table.png')
    save(trim(tcell(EXT, 24, 7, 1, 1, k)), 'sprites', 'counter.png')
    save(trim(tcell(INT, 26, 6, 1, 1, k)), 'sprites', 'lantern.png')
    save(trim(tcell(INT, 24, 6, 1, 1, k)), 'sprites', 'candle.png')
    save(trim(tcell(INT, 27, 6, 1, 1, k)), 'sprites', 'plate.png')
    save(trim(tcell(INT, 25, 6, 1, 1, k)), 'sprites', 'keg.png')
    save(trim(rect(WLD, 26 * 16, 12 * 16, 16, 16, k)), 'sprites', 'bed.png')
    save(trim(tcell(EXT, 26, 4, 1, 1, k)), 'sprites', 'stool.png')
    save(trim(tcell(EXT, 27, 2, 1, 1, k)), 'sprites', 'noticeboard.png')
    # shop shelving built from the interior bookshelf tiles
    # a long shop table reads as a counter in the interior scenes
    save(trim(tcell(EXT, 24, 4, 2, 1, k)), 'sprites', 'longtable.png')

    # ---- chests: closed and open, from the object charset -----------------
    obj = 'CharSet/Object1.png'
    closed = trim(keyed(rect(obj, 2 * 72, 128 + 64, 24, 32), KEY_PINK2))
    save(centered(closed, (18, 18)), 'sprites', 'chest_closed.png')
    opened = trim(keyed(rect(obj, 2 * 72, 128 + 96, 24, 32), KEY_PINK2))
    save(centered(opened, (18, 22)), 'sprites', 'chest_open.png')


# ================================================================ 3. houses
WALLS = {
    # seamless wall fills taken from the building autotile block
    'plaster':  (16, 2),      # cream plaster
    'plaster2': (16, 1),
    'ashlar':   (15, 10),     # pale ashlar blocks
    'stone':    (15, 10),
    'brick':    (13, 2),      # purple brick
    'brick2':   (13, 1),
    'brickred': (12, 10),
    'timber':   (18, 6),      # vertical boards
    'timber2':  (18, 5),
    'log':      (18, 4),
}

ROOFS = {
    # (tile x, tile y, tint) — one chipset shingle band, recoloured per roof
    'thatch':  (13, 5, None),
    'slate':   (13, 4, (150, 162, 196)),
    'plank':   (14, 4, (188, 226, 168)),
    'brick':   (13, 5, (238, 132, 120)),
    'dark':    (14, 3, (126, 136, 172)),
}


def _fill(sheet, tx, ty, w, h):
    out = canvas(w, h)
    t = tile(sheet, tx, ty)
    for y in range(0, h, 16):
        for x in range(0, w, 16):
            out.alpha_composite(t, (x, y))
    return out


def _roof(w, h, style):
    """A shingle band with a lit ridge and a dark eave."""
    tx, ty, col = ROOFS[style]
    out = tint(_fill(EXT, tx, ty, w, h), col)
    d = ImageDraw.Draw(out)
    d.rectangle((0, 0, w - 1, 1), fill=(252, 234, 202, 80))
    d.rectangle((0, 1, w - 1, 2), fill=(110, 66, 34, 90))
    d.rectangle((0, h - 4, w - 1, h - 3), fill=(64, 38, 20, 255))
    d.rectangle((0, h - 2, w - 1, h - 1), fill=(34, 20, 12, 255))
    return out


def _window_small():
    return trim(tcell(EXT, 25, 2, 1, 1, KEY_PINK)).resize((14, 14), Image.Resampling.NEAREST)


def _window_arch():
    """The tall cream arch window object (3x3 tiles)."""
    return tcell(EXT, 15, 0, 3, 3, KEY_PINK)


def _door():
    """A plank door built from the chipset's own timber, framed in oak."""
    planks = tile(EXT, 18, 6)
    out = canvas(18, 26)
    out.alpha_composite(planks.crop((1, 2, 15, 16)).resize((14, 24), Image.Resampling.NEAREST),
                        (2, 2))
    d = ImageDraw.Draw(out)
    d.rectangle((0, 0, 17, 1), fill=(112, 70, 36, 255))
    d.rectangle((0, 0, 1, 25), fill=(112, 70, 36, 255))
    d.rectangle((16, 0, 17, 25), fill=(112, 70, 36, 255))
    d.rectangle((2, 24, 15, 25), fill=(48, 30, 16, 255))
    d.rectangle((12, 13, 13, 14), fill=(252, 214, 92, 255))
    return out


def build_house(roof, wall, w=64, h=58, sign=False, chimney=False, awning=False,
                banner=False, storeys=1, arch_window=False):
    """Compose a cottage from OpenRTP wall, roof, window and door pieces.

    The sprite is bottom-centred on its house tile: the bottom edge of the image
    sits on the low edge of the anchor tile and the building rises from there.
    """
    out = canvas(w, h)
    roof_h = 20 if storeys == 1 else 24
    out.alpha_composite(_roof(w, roof_h, roof), (0, 0))
    out.alpha_composite(_fill(EXT, *WALLS[wall], w - 6, h - roof_h), (3, roof_h))

    d = ImageDraw.Draw(out)
    d.rectangle((2, roof_h, 3, h - 1), fill=(60, 38, 22, 255))
    d.rectangle((w - 4, roof_h, w - 3, h - 1), fill=(60, 38, 22, 255))
    d.rectangle((2, h - 3, w - 3, h - 1), fill=(44, 28, 16, 255))

    door = _door()
    dx = (w - door.width) // 2
    win = _window_small()
    for wx in (7, w - 7 - win.width):
        out.alpha_composite(win, (wx, roof_h + 8))
        if storeys > 1:
            out.alpha_composite(win, (wx, roof_h - 13))

    if arch_window:
        aw = _window_arch().resize((32, 32), Image.Resampling.NEAREST)
        out.alpha_composite(aw, ((w - 32) // 2, roof_h + 1))
        out.alpha_composite(door.resize((14, 20), Image.Resampling.NEAREST),
                            ((w - 14) // 2, h - 22))
    else:
        out.alpha_composite(door, (dx, h - door.height - 2))

    if sign:
        board = trim(tcell(EXT, 20, 10, 1, 1, KEY_PINK)).resize((12, 12), Image.Resampling.NEAREST)
        out.alpha_composite(board, (dx + door.width + 1, h - 22))
    if chimney:
        chim = tile(EXT, 28, 8, key=KEY_PINK).crop((2, 0, 14, 16))
        out.alpha_composite(chim, (w - 20, 0))
    if awning:
        for i, x in enumerate(range(3, w - 3, 16)):
            c = 16 if i % 2 == 0 else 17
            out.alpha_composite(tile(EXT, c, 12).crop((0, 6, 16, 16)), (x, roof_h - 2))
    if banner:
        cloth = tile(EXT, 18, 1).crop((3, 4, 13, 16))
        out.alpha_composite(cloth, (dx + 4, h - 42))
    return out


def build_buildings():
    save(build_house('thatch', 'plaster'), 'sprites', 'house_0.png')
    save(build_house('slate', 'timber', chimney=True, storeys=2), 'sprites', 'house_1.png')
    save(build_house('brick', 'brick', w=66, h=60, chimney=True), 'sprites', 'house_2.png')
    save(build_house('plank', 'timber2', w=66, h=60, storeys=2), 'sprites', 'house_3.png')
    save(build_house('dark', 'ashlar', w=68, h=62, storeys=2, chimney=True),
         'sprites', 'house_4.png')

    save(build_house('slate', 'plaster', w=72, h=62, sign=True, awning=True),
         'sprites', 'shop.png')
    save(build_house('thatch', 'brick2', w=74, h=68, sign=True, banner=True,
                     chimney=True, storeys=2), 'sprites', 'inn.png')
    save(build_house('dark', 'ashlar', w=76, h=72, sign=True, storeys=2,
                     arch_window=True), 'sprites', 'temple.png')
    save(build_house('brick', 'plaster', w=78, h=70, storeys=2, chimney=True,
                     arch_window=True), 'sprites', 'manor.png')
    save(_castle(), 'sprites', 'castle.png')
    save(_tower(), 'sprites', 'tower.png')
    save(_gate(), 'sprites', 'gate.png')


def _castle():
    """The world chipset's own castle blocks: blue-roofed keep over a curtain wall."""
    out = canvas(96, 86)
    wall = trim(tcell(WLD, 20, 10, 2, 2, KEY_PINK)).resize((56, 56), Image.Resampling.NEAREST)
    out.alpha_composite(wall, (2, 28))
    out.alpha_composite(wall.transpose(Image.Transpose.FLIP_LEFT_RIGHT), (38, 28))
    keep = trim(tcell(WLD, 22, 10, 2, 2, KEY_PINK)).resize((56, 56), Image.Resampling.NEAREST)
    out.alpha_composite(keep, (20, 10))
    return out


def _tower():
    """Two stacked watchtower blocks make a spire landmark."""
    low = trim(tcell(DUN, 20, 12, 1, 1, KEY_PINK)).resize((26, 26), Image.Resampling.NEAREST)
    top = trim(tcell(DUN, 22, 12, 1, 1, KEY_PINK)).resize((26, 26), Image.Resampling.NEAREST)
    out = canvas(28, 54)
    out.alpha_composite(low, (1, 28))
    out.alpha_composite(top, (1, 2))
    return out


def _gate():
    """A stone archway (village gate) from the world sheet."""
    return trim(tcell(WLD, 23, 12, 1, 2, KEY_PINK)).resize((28, 44), Image.Resampling.NEAREST)


# ================================================================= 4. heroes
CELL_W, CELL_H = 24, 32
CHAR_W, CHAR_H = 72, 128


def charset_char(sheet, col, row, key=KEY_CYAN):
    im = keyed(load(sheet), key)
    return im.crop((col * CHAR_W, row * CHAR_H, col * CHAR_W + CHAR_W, row * CHAR_H + CHAR_H))


def walk_sheet(sheet, col, row, key=KEY_CYAN):
    """Re-order RM2k frames into idle/walkA/walkB columns and down/up/side rows.

    Source row order is down, left, right, up; the game draws down, up, side and
    mirrors the side row when walking left.
    """
    block = charset_char(sheet, col, row, key)
    out = canvas(CELL_W * 3, CELL_H * 3)
    for dst_row, src_row in ((0, 0), (1, 3), (2, 1)):
        for dst_col, src_col in enumerate((1, 0, 2)):
            out.alpha_composite(
                block.crop((src_col * CELL_W, src_row * CELL_H,
                            src_col * CELL_W + CELL_W, src_row * CELL_H + CELL_H)),
                (dst_col * CELL_W, dst_row * CELL_H))
    return out


def front_cell(sheet, col, row, key=KEY_CYAN):
    """The front-facing (down) middle animation frame of a charset character."""
    block = charset_char(sheet, col, row, key)
    return trim(block.crop((CELL_W, 0, CELL_W * 2, CELL_H)))


def portrait(sheet, col, row, size=48):
    im = load(sheet)
    return im.crop((col * 48, row * 48, col * 48 + 48, row * 48 + 48)).resize(
        (size, size), Image.Resampling.NEAREST)


def build_heroes():
    A = 'CharSet/Actor1.png'
    # Aldric the knight (red armour), Iris the mage (blue robe), Pip the cleric.
    save(walk_sheet(A, 2, 0), 'sprites', 'hero_knight.png')
    save(walk_sheet(A, 1, 1), 'sprites', 'hero_mage.png')
    save(walk_sheet(A, 0, 0), 'sprites', 'hero_cleric.png')
    # The rest of the actor sheet doubles as the hero-select gallery.
    roster = [(0, 1), (1, 0), (2, 1), (3, 0), (3, 1), (0, 0)]
    for i, (col, row) in enumerate(roster):
        save(walk_sheet(A, col, row), 'sprites', f'hero_alt{i}.png')

    F = 'FaceSet/Actor1.png'
    save(portrait(F, 3, 0), 'sprites', 'face_knight.png')     # helmeted knight
    save(portrait(F, 1, 0), 'sprites', 'face_mage.png')       # blue-haired mage
    save(portrait(F, 0, 0), 'sprites', 'face_cleric.png')     # brown-haired cleric
    save(portrait(F, 2, 0, 64), 'sprites', 'face_knight_big.png')

    P = 'FaceSet/People1.png'
    save(portrait(P, 0, 0), 'sprites', 'face_villager0.png')
    save(portrait(P, 1, 0), 'sprites', 'face_villager1.png')
    save(portrait(P, 2, 0), 'sprites', 'face_villager2.png')
    save(portrait(P, 3, 0), 'sprites', 'face_villager3.png')
    save(portrait(P, 2, 1), 'sprites', 'face_elder.png')      # grey-bearded elder
    save(portrait(P, 0, 1), 'sprites', 'face_mayor.png')
    save(portrait(P, 1, 1), 'sprites', 'face_woman.png')
    save(portrait(P, 3, 1), 'sprites', 'face_child.png')
    P2 = 'FaceSet/People2.png'
    save(portrait(P2, 0, 0), 'sprites', 'face_smith.png')
    save(portrait(P2, 3, 2), 'sprites', 'face_bard.png')
    A2 = 'FaceSet/Actor2.png'
    save(portrait(A2, 1, 1), 'sprites', 'face_shopkeeper.png')
    save(portrait(A2, 2, 2), 'sprites', 'face_innkeeper.png')
    save(portrait(A2, 3, 0), 'sprites', 'face_guard.png')
    save(portrait(A2, 0, 1), 'sprites', 'face_ranger.png')


def build_villagers():
    """NPC walking sprites so villages are actually inhabited."""
    npcs = [
        ('npc_villager0', 'CharSet/People1.png', 0, 0),
        ('npc_villager1', 'CharSet/People1.png', 1, 0),
        ('npc_villager2', 'CharSet/People1.png', 2, 0),
        ('npc_villager3', 'CharSet/People1.png', 3, 0),
        ('npc_elder',     'CharSet/People1.png', 2, 1),
        ('npc_mayor',     'CharSet/People1.png', 0, 1),
        ('npc_woman',     'CharSet/People1.png', 1, 1),
        ('npc_child',     'CharSet/People1.png', 3, 1),
        ('npc_guard',     'CharSet/People3.png', 3, 1),
        ('npc_soldier',   'CharSet/People3.png', 2, 1),
        ('npc_knight',    'CharSet/People3.png', 0, 1),
        ('npc_merchant',  'CharSet/People2.png', 0, 0),
        ('npc_smith',     'CharSet/People2.png', 3, 0),
        ('npc_priest',    'CharSet/People4.png', 0, 0),
        ('npc_farmer',    'CharSet/People5.png', 0, 0),
    ]
    for name, sheet, col, row in npcs:
        save(walk_sheet(sheet, col, row), 'sprites', name + '.png')
    # animals that wander the meadow
    for name, col, row in (('npc_dog', 0, 1), ('npc_chicken', 2, 1),
                           ('npc_sheep', 3, 1), ('npc_cat', 1, 0)):
        save(walk_sheet('CharSet/Animal.png', col, row), 'sprites', name + '.png')


# =============================================================== 5. monsters
# name: (sheet, char col, char row, field sprite size)
MONSTERS = {
    'slime':       ('CharSet/Monster1.png', 0, 0, (38, 30)),
    'imp':         ('CharSet/Monster1.png', 1, 0, (34, 40)),
    'skeleton':    ('CharSet/Monster1.png', 0, 1, (32, 42)),
    'ghost':       ('CharSet/Monster1.png', 3, 0, (34, 40)),
    'orc':         ('CharSet/Monster1.png', 1, 1, (36, 42)),
    'wolf':        ('CharSet/Monster2.png', 1, 0, (40, 34)),
    'bat':         ('CharSet/Monster2.png', 0, 1, (42, 28)),
    'wisp':        ('CharSet/Monster2.png', 2, 0, (34, 40)),
    'golem':       ('CharSet/Monster2.png', 0, 1, (40, 44)),
    'slime_king':  ('CharSet/Monster2.png', 0, 0, (44, 40)),
    'serpent':     ('CharSet/Monster3.png', 1, 0, (44, 42)),
    'night_rider': ('CharSet/Monster3.png', 3, 0, (40, 46)),
    'hornet':      ('Monster/Hornet.png',   0, 0, (38, 34)),
}


def monster_sprite(entry):
    sheet, col, row, _ = entry
    if sheet.startswith('Monster/'):
        im = load(sheet)
        p = im.load()
        for y in range(im.height):
            for x in range(im.width):
                r, g, b, a = p[x, y]
                if r > 220 and g < 80 and b > 220:
                    p[x, y] = (0, 0, 0, 0)
        return trim(im)
    return front_cell(sheet, col, row, KEY_CYAN)


def build_monsters():
    for name, entry in MONSTERS.items():
        w, h = entry[3]
        sprite = monster_sprite(entry)
        save(centered(sprite, (w, h)), 'sprites', name + '.png')
        save(centered(sprite, (w * 3, h * 3)), 'sprites', name + '_battle.png')

    M = 'FaceSet/Monster.png'
    save(portrait(M, 0, 0), 'sprites', 'face_slime.png')
    save(portrait(M, 4, 1), 'sprites', 'face_ghost.png')
    save(portrait(M, 4, 0), 'sprites', 'face_skeleton.png')
    save(portrait(M, 1, 0), 'sprites', 'face_demon.png')
    save(portrait(M, 5, 1), 'sprites', 'face_wolf.png')


# ================================================================== 6. items
def build_items():
    """Item and equipment icons lifted from the chipset prop banks."""
    k = KEY_PINK
    save(trim(tcell(INT, 27, 7, 1, 1, k)), 'icons', 'potion.png')       # bottle
    save(trim(tcell(INT, 28, 7, 1, 1, k)), 'icons', 'hi_potion.png')    # flask + cup
    save(trim(tcell(INT, 26, 6, 1, 1, k)), 'icons', 'ether.png')        # lantern
    save(trim(tcell(INT, 29, 6, 1, 1, k)), 'icons', 'elixir.png')       # red flask
    save(trim(tcell(INT, 24, 4, 1, 1, k)), 'icons', 'book.png')         # tome
    save(trim(tcell(INT, 24, 6, 1, 1, k)), 'icons', 'candle.png')
    save(trim(tcell(INT, 27, 6, 1, 1, k)), 'icons', 'food.png')         # roast
    save(trim(tcell(INT, 21, 11, 1, 1, k)), 'icons', 'feather.png')     # feather
    save(trim(tcell(DUN, 25, 3, 1, 1, k)), 'icons', 'gem.png')          # gem
    save(trim(tcell(INT, 30, 3, 2, 2, k)), 'icons', 'shield.png')       # wall shield
    save(trim(tcell(INT, 28, 5, 1, 1, k)), 'icons', 'ring.png')         # red gem potion

    # currency: the system sheet's own coin glyph
    save(load('System/System.png').crop((128, 39, 144, 49)), 'icons', 'coin.png')

    # weapons: blade swings from the battle-weapon sheet
    wp = load('BattleWeapon/Weapon.png')
    for i, (x, y) in enumerate(((0, 64), (64, 64), (128, 64))):
        save(centered(trim(wp.crop((x, y, x + 64, y + 64))), (16, 16), upscale=False),
             'icons', f'sword_{i}.png')
    # armour: the interior bank's shield / garment props
    save(trim(tcell(INT, 29, 2, 1, 1, k)), 'icons', 'armor_0.png')
    save(trim(tcell(INT, 27, 3, 1, 1, k)), 'icons', 'armor_1.png')
    save(trim(tcell(INT, 28, 3, 1, 1, k)), 'icons', 'armor_2.png')
    save(trim(tcell(INT, 24, 9, 1, 1, k)), 'icons', 'armor_3.png')      # armour stand


# =================================================================== 7. UI
def build_window():
    """Window skin cut from the OpenRTP system graphic.

    The RM2k system graphic holds its window frame at (32,0)-(96,48); the game
    stretches that frame's middle so any panel looks like an RM2k window.
    """
    blue = load('System/System.png')
    save(blue.crop((64, 0, 96, 32)), 'ui', 'window_frame.png')     # window skin body
    save(blue.crop((32, 0, 64, 32)), 'ui', 'window_selector.png')  # orange selector box
    save(blue.crop((96, 0, 160, 8)), 'ui', 'gauge.png')
    save(blue.crop((32, 40, 144, 56)), 'ui', 'digits.png')

    royal = load('System/Royal.png')
    save(royal.crop((64, 0, 96, 32)), 'ui', 'window_frame_royal.png')
    save(royal.crop((32, 0, 64, 32)), 'ui', 'window_selector_royal.png')


# ================================================================= 8. title
def build_title():
    """Title art: OpenRTP's painted sky, its atlas map and the game-over plate."""
    save(load('Title/Title1.png').convert('RGB').resize((512, 288), Image.Resampling.BICUBIC),
         'title_bg.png')
    save(load('Title/Title2.png').convert('RGB').resize((512, 288), Image.Resampling.BICUBIC),
         'title_atlas.png')
    cloud = keyed(load('Picture/Cloud.png'), KEY_PINK)
    save(trim(cloud), 'sprites', 'cloud.png')
    save(load('GameOver/Game Over.png').convert('RGB').resize((512, 288),
                                                             Image.Resampling.BICUBIC),
         'ui', 'gameover_plate.png')
    save(load('Panorama/Dawn2.png').convert('RGB').resize((512, 288), Image.Resampling.BICUBIC),
         'ui', 'dawn.png')
    save(load('Panorama/Night Sky1.png').convert('RGB').resize((512, 288),
                                                              Image.Resampling.BICUBIC),
         'ui', 'night.png')


# =============================================================== 9. effects
def build_effects():
    """Battle effect masks from the OpenRTP Battle animation sheets."""
    def frames(sheet, fw, fh, cols, rows):
        im = load(sheet)
        out = []
        for r in range(rows):
            for c in range(cols):
                cell = trim(im.crop((c * fw, r * fh, c * fw + fw, r * fh + fh)))
                if cell.width > 2 and cell.height > 2:
                    out.append(cell)
        return out

    sword = frames('Battle/Sword1.png', 96, 96, 5, 3)
    if sword:
        save(centered(sword[2], (30, 30)), 'fx', 'slash.png')
        save(centered(sword[1], (20, 20)), 'fx', 'spark.png')
        save(centered(sword[0], (24, 24)), 'fx', 'sparkle.png')
    blow = frames('Battle/Blow.png', 96, 96, 5, 3)
    if blow:
        save(centered(blow[0], (22, 22)), 'fx', 'impact.png')
        save(centered(blow[len(blow) // 2], (26, 22)), 'fx', 'poof.png')
        save(centered(blow[-1], (20, 20)), 'fx', 'fireball.png')
    arrow = frames('Battle/Arrow.png', 96, 48, 5, 2)
    if arrow:
        save(centered(arrow[0], (20, 20), upscale=False), 'fx', 'arrow.png')


# =====================================================================
def main():
    build_tiles()
    build_objects()
    build_buildings()
    build_heroes()
    build_villagers()
    build_monsters()
    build_items()
    build_window()
    build_title()
    build_effects()
    print('OpenRTP art imported into assets/')


if __name__ == '__main__':
    main()
