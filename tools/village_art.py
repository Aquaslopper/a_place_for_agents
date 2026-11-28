"""Walking-scale architecture in the SNES / Lufia II dialect.

The little World-map settlement icons are NOT building facades. These are
composed from the vendored **retro** Exterior chipset
(``ChipSet/retro_Exterior.png``, the SNES-retro counterpart of Exterior.png)
so the walking-scale towns read like a 16-bit console JRPG rather than a
modern RPG-Maker tileset:

  * steep pitched roofs of *dithered/checker shingles* (the retro sheet's
    own shingle cells), palette-ramped to muted dusty clay / slate / ochre /
    moss / wine families — never a smooth gradient;
  * half-timber walls: cream speckled plaster panels inside a dark timber
    frame (posts, beams and corner braces) on a coursed stone foundation;
  * tall two-pane pale-blue windows with a brown frame and a single
    upper-left highlight pixel — no shutters, no reflected-sky detail;
  * a recessed timber door (OpenRTP Object1 crop) and, for trades, a hanging
    gold-pictogram sign (no baked INN/PUB lettering);
  * selective 1px outline, light from the upper-left, integer pixels only.

Reference studied (composition only, never shipped): Lufia II Elcid /
Sundletan town maps via SNESMaps, and the PureZC "Lufia 2 Houses" tile rip.
All pixels here come from the vendored OpenRTP sheets; see CREDITS.md.
"""
from PIL import Image, ImageDraw
from rtp_sheet import EXT, KEY_PINK, KEY_PINK2, tile, rect, canvas, save, trim
from landscape_art import ramp

RETRO = 'ChipSet/retro_Exterior.png'

# Muted, hue-shifted SNES roof families (<= 8 colours each). Dark -> light.
ROOFS = {
    'clay':  '5c3a3e 7c4a48 9c5c52 b87264 cf8a72 e0a488',
    'slate': '3a4454 4a5668 5c6a7e 708094 8696a8 9caab8',
    'ochre': '5c4a38 7a6244 987c52 b49662 cca870 e0c084',
    'moss':  '3e4e42 50624e 64785c 7a8e70 90a484 a6b898',
    'wine':  '54405a 6e5270 8a6488 a4789c bc90b0 d0a8c0',
}
TIMBER = '#4a3526'; WOOD_LIGHT = '#7a5c3a'; IVORY = '#e8dcc0'
STONE = '4a5258 5c666c 707a7e 86908e 9ca49c b0b4a8'
FRAME = '#5c4326'; GLASS = '#9fc4d4'; GLASS_HI = '#c8e0e8'; GLASS_SH = '#6a94a8'


def pattern(im, points, texture):
    mask = Image.new('L', im.size); ImageDraw.Draw(mask).polygon(points, fill=255)
    layer = canvas(*im.size)
    for y in range(0, im.height, texture.height):
        for x in range(0, im.width, texture.width): layer.paste(texture, (x, y))
    im.paste(layer, (0, 0), mask)


def _shingle(style):
    # The retro sheet's own checker-shingle cell, ramped to a muted family.
    return ramp(tile(RETRO, 15, 12), ROOFS[style])


def _plaster():
    return tile(RETRO, 16, 1).crop((3, 0, 14, 16))  # cream speckle, no posts


def _stone():
    return ramp(tile(RETRO, 7, 10), STONE)


def window(im, x, y, chapel=False):
    d = ImageDraw.Draw(im)
    if chapel:
        # Arched stained-glass light, drawn in-dialect (muted blue/gold panes).
        d.rectangle((x, y + 4, x + 10, y + 14), fill=FRAME)
        d.arc((x, y, x + 10, y + 10), 180, 360, fill=FRAME, width=2)
        d.rectangle((x + 1, y + 5, x + 9, y + 13), fill=GLASS_SH)
        d.rectangle((x + 2, y + 6, x + 8, y + 12), fill=GLASS)
        d.arc((x + 1, y + 1, x + 9, y + 9), 180, 360, fill=GLASS, width=2)
        d.line((x + 5, y + 2, x + 5, y + 13), fill=FRAME)
        d.line((x + 1, y + 8, x + 9, y + 8), fill=FRAME)
        d.point((x + 3, y + 6), fill='#e8c860'); d.point((x + 7, y + 9), fill='#c86a5a')
        d.point((x + 3, y + 3), fill=GLASS_HI)
        d.line((x, y + 14, x + 10, y + 14), fill=TIMBER)
        return
    # Tall two-pane window: brown frame, pale glass, mullion, upper-left light.
    d.rectangle((x, y, x + 9, y + 13), fill=FRAME)
    d.rectangle((x + 1, y + 1, x + 8, y + 12), fill=GLASS_SH)
    d.rectangle((x + 2, y + 2, x + 7, y + 11), fill=GLASS)
    d.line((x + 4, y + 1, x + 4, y + 12), fill=FRAME)
    d.line((x + 1, y + 7, x + 8, y + 7), fill=FRAME)
    d.point((x + 2, y + 2), fill=GLASS_HI); d.point((x + 5, y + 2), fill=GLASS_HI)
    d.line((x, y + 13, x + 9, y + 13), fill=TIMBER)  # sill shadow


def hanging_sign(im, x, y, kind):
    """Lufia-style trade sign: bracket + dark board + gold pictogram."""
    d = ImageDraw.Draw(im)
    d.line((x - 2, y, x + 9, y), fill=TIMBER, width=2)
    d.line((x + 7, y, x + 7, y + 3), fill='#8a6c44')
    d.rectangle((x, y + 3, x + 10, y + 13), fill=TIMBER)
    d.rectangle((x + 1, y + 4, x + 9, y + 12), fill='#2e2418')
    g = '#e8c860'
    if kind == 6:  # inn: a bed
        d.rectangle((x + 2, y + 8, x + 8, y + 10), fill=g)
        d.rectangle((x + 2, y + 6, x + 3, y + 10), fill=g)
        d.rectangle((x + 4, y + 7, x + 6, y + 8), fill='#f4e0a0')
    elif kind == 10:  # smithy: an anvil
        d.polygon([(x + 2, y + 7), (x + 8, y + 7), (x + 6, y + 9),
                   (x + 5, y + 9), (x + 5, y + 11), (x + 4, y + 11),
                   (x + 4, y + 9), (x + 3, y + 9)], fill=g)
    else:  # shop: a coin
        d.ellipse((x + 3, y + 6, x + 7, y + 10), fill=g)
        d.point((x + 4, y + 7), fill='#f4e0a0')
        d.line((x + 5, y + 5, x + 5, y + 6), fill=g)


def cottage(style, w, h, kind=0, gable=True, stone=False):
    im = canvas(w, h); d = ImageDraw.Draw(im); mid = w // 2; eave = h // 2
    roof = _shingle(style); plaster = _plaster(); foundation = _stone()
    cols = ROOFS[style].split()
    # ---- wall body: plaster (or stone) + timber frame + foundation ----
    pattern(im, [(3, eave), (w - 4, eave), (w - 4, h - 1), (3, h - 1)],
            foundation if stone else plaster)
    pattern(im, [(3, h - 4), (w - 4, h - 4), (w - 4, h - 1), (3, h - 1)], foundation)
    d.line((3, h - 4, w - 4, h - 4), fill=TIMBER)
    # timber frame: top beam, mid rail, posts and corner braces
    d.rectangle((3, eave, w - 4, eave + 2), fill=TIMBER)
    d.line((3, eave, w - 4, eave), fill=WOOD_LIGHT)
    midy = eave + (h - 4 - eave) // 2
    d.rectangle((3, midy, w - 4, midy + 1), fill=TIMBER)
    for x in (3, w - 5, mid - 10, mid + 9):
        d.rectangle((x, eave, x + 1, h - 4), fill=TIMBER)
        d.line((x, eave, x, h - 4), fill=WOOD_LIGHT)
    if not stone:
        d.line((5, eave + 3, 12, eave + 10), fill=TIMBER, width=2)
        d.line((w - 6, eave + 3, w - 13, eave + 10), fill=TIMBER, width=2)
    # ---- roof: steep trapezoid of dithered shingles ----
    inset = 6
    pitch = [(1, eave), (inset, 3), (w - inset - 1, 3), (w - 2, eave)]
    pattern(im, pitch, roof)
    d.line(pitch + [pitch[0]], fill=TIMBER)
    d.line((inset, 3, w - inset - 1, 3), fill='#e8d8b8', width=2)   # ridge cap
    d.line((inset, 5, w - inset - 1, 5), fill='#' + cols[2])
    d.line((1, eave, w - 2, eave), fill=TIMBER, width=2)            # eave shadow
    d.line((2, eave - 1, w - 3, eave - 1), fill='#' + cols[0])
    # shaded far hip on the right (light comes from the upper-left)
    d.polygon([(w - inset - 1, 3), (w - 2, eave), (w - inset - 6, eave),
               (w - inset - 3, 5)], fill='#' + cols[1])
    # ---- chimney: a simple coursed stone stack ----
    cx = w - 21
    d.rectangle((cx, 4, cx + 5, 12), fill='#707a7e', outline=TIMBER)
    d.rectangle((cx - 1, 2, cx + 6, 4), fill='#9ca49c', outline=TIMBER)
    d.line((cx + 1, 3, cx + 4, 3), fill='#3a4248')
    # ---- recessed timber door ----
    door = trim(rect('CharSet/Object1.png', 24, 64, 24, 32, KEY_PINK2))
    door = door.resize((14, 22), Image.Resampling.NEAREST)
    d.rectangle((mid - 8, h - 25, mid + 8, h - 2), fill=TIMBER)
    im.alpha_composite(door, (mid - 7, h - 24))
    d.line((mid - 9, h - 1, mid + 9, h - 1), fill='#9ca49c')
    # ---- windows ----
    for x in (10, w - 23): window(im, x, h - 22, kind == 7)
    # ---- dormer gable for the larger houses ----
    if gable and kind in (6, 8):
        gx = mid; gy = eave - 12
        d.polygon([(gx - 9, eave), (gx, gy), (gx + 9, eave)], fill=IVORY, outline=TIMBER)
        d.line([(gx - 10, eave), (gx, gy - 2), (gx + 10, eave)], fill=TIMBER, width=2)
        d.rectangle((gx - 3, gy + 4, gx + 3, eave - 2), fill=GLASS_SH, outline=FRAME)
        d.rectangle((gx - 2, gy + 5, gx + 2, eave - 3), fill=GLASS)
        d.point((gx - 1, gy + 6), fill=GLASS_HI)
    # ---- hanging trade sign beside the door ----
    if kind in (5, 6, 10):
        hanging_sign(im, w - 16, eave + 2, kind)
    return im


def tower(w, h, steeple=False):
    """Round stone tower with a conical slate roof (Lufia keep turret).

    ``steeple=True`` makes a church bell tower: an arched belfry opening near
    the top and no ground-level door.
    """
    im = canvas(w, h); d = ImageDraw.Draw(im)
    wall = _stone()
    pattern(im, [(4, 22), (w - 4, 22), (w - 4, h - 2), (4, h - 2)], wall)
    # cylindrical shading: lit left edge, dark right edge
    d.rectangle((4, 22, 6, h - 2), fill='#9ca49c')
    d.rectangle((w - 7, 22, w - 4, h - 2), fill='#4a5258')
    d.line((4, h - 2, w - 4, h - 2), fill=TIMBER)
    for y in (26, h - 16):
        d.line((4, y, w - 4, y), fill='#5c666c')
        d.line((4, y - 1, w - 4, y - 1), fill='#b0b4a8')
    # conical roof
    peak = [(1, 23), (w // 2, 1), (w - 2, 23)]
    pattern(im, peak, _shingle('slate'))
    d.line(peak + [peak[0]], fill=TIMBER)
    d.line((w // 2, 2, 3, 22), fill='#9caab8')
    d.line((1, 23, w - 2, 23), fill=TIMBER, width=2)
    if steeple:
        # arched belfry opening with a hint of bronze bell
        bx = w // 2
        d.rectangle((bx - 4, 30, bx + 4, 40), fill='#2e2418')
        d.arc((bx - 4, 26, bx + 4, 34), 180, 360, fill='#2e2418', width=3)
        d.rectangle((bx - 2, 33, bx + 2, 38), fill='#8a6c44')
        d.line((bx - 3, 40, bx + 3, 40), fill='#9ca49c')
        return im
    window(im, w // 2 - 5, 30)
    # arched doorway
    d.rectangle((w // 2 - 4, h - 13, w // 2 + 4, h - 2), fill='#3a2a1c')
    d.arc((w // 2 - 4, h - 17, w // 2 + 4, h - 9), 180, 360, fill='#3a2a1c', width=3)
    d.line((w // 2 - 4, h - 2, w // 2 + 4, h - 2), fill='#9ca49c')
    return im


def build_buildings():
    for i, style in enumerate(('clay', 'slate', 'ochre', 'moss', 'wine')):
        save(cottage(style, 62 + (i % 2) * 4, 54 + (i % 3) * 3,
                     gable=i != 2, stone=i == 4), 'sprites', f'house_{i}.png')
    save(cottage('moss', 70, 61, 5, False), 'sprites', 'shop.png')
    save(cottage('clay', 74, 66, 6), 'sprites', 'inn.png')
    # Chapel: stone nave + a slate-roofed bell steeple with an arched belfry.
    chapel = canvas(76, 78)
    chapel.alpha_composite(cottage('slate', 70, 58, 7, True, True), (6, 20))
    chapel.alpha_composite(tower(23, 59, steeple=True), (0, 1))
    save(chapel, 'sprites', 'temple.png')
    save(cottage('wine', 78, 68, 8, True, True), 'sprites', 'manor.png')
    save(tower(33, 56), 'sprites', 'tower.png')
    # Castle: crenellated stone keep flanked by two conical turrets + gate.
    keep = canvas(98, 84); d = ImageDraw.Draw(keep)
    masonry = _stone()
    pattern(keep, [(12, 42), (84, 42), (84, 81), (12, 81)], masonry)
    d.line((12, 42, 84, 42), fill='#b0b4a8'); d.line((12, 81, 84, 81), fill=TIMBER)
    for x in range(14, 85, 8):
        d.rectangle((x, 37, x + 4, 43), fill='#86908e', outline='#4a5258')
    gate = trim(rect(EXT, 28 * 16, 8 * 16, 32, 32, KEY_PINK))
    keep.alpha_composite(gate, (33, 51))
    keep.alpha_composite(tower(29, 72), (0, 10))
    keep.alpha_composite(tower(29, 72), (69, 10))
    upper = tower(34, 67); keep.alpha_composite(upper.crop((0, 0, 34, 44)), (32, 1))
    d.line((16, 49, 81, 49), fill='#b0b4a8')
    save(keep, 'sprites', 'castle.png')
    save(gate, 'sprites', 'gate.png')
