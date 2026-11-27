#!/usr/bin/env python3
"""import_openrtp.py — adapt the vendored OpenRTP sheets to this game's layout.

This script adapts the OpenRTP package vendored under
``assets/source/openrtp/`` (see CREDITS.md for authors and licences). A few
small overlays and fallback objects are generated in-repository instead:

  * it crops chipset tiles (16x16 cells),
  * cuts sprites out of their magenta / cyan colour key,
  * re-orders RPG Maker 2000 walking frames (up / right / down / left)
    into the row order this renderer draws (down / up / right, mirrored for left),
  * composes cottages, a castle and battle effects out of the original pieces.

    python3 tools/import_openrtp.py

Two colour keys are in play: ``(0,147,146)`` on the charset sheets and
``(255,103,139)`` on the chipsets.
"""
from PIL import Image, ImageDraw
import art_objects
import art_misc
from rtp_sheet import (EXT, WLD, INT, DUN, KEY_CYAN, KEY_PINK, KEY_PINK2, load, keyed,
                       tile, rect, tcell, save, trim, canvas, centered, centered_mid, tint)
from landscape_art import make_landscape, ramp
from village_art import build_buildings
from battle_scenery import build_backdrops

# =================================================================== 1. tiles
def build_tiles():
    """Ground textures, used as seamless 16x16 fills by the world renderer."""
    make_landscape()
    # Keep masonry on walls, boards underfoot and woven fabric on the rug.
    save(tile(INT, 15, 0), 'tiles', 'brick.png')
    save(tile(INT, 16, 5), 'tiles', 'brick_dark.png')
    save(tile(DUN, 13, 6), 'tiles', 'dungeon_floor.png')
    save(tile(INT, 13, 3), 'tiles', 'plank_floor.png')
    save(ramp(tile(EXT,7,10),'99988c a8a69a b6b4a6 c5c0aa'), 'tiles', 'tile_floor.png')
    save(ramp(tile(EXT,12,11),'8b8d89 a1a49c b0b0a2 c1beac d0cab3'), 'tiles', 'stone_floor.png')
    save(tile(INT, 16, 13), 'tiles', 'carpet.png')

    # Small, transparent ground cover, not full shrubs or indoor furniture.
    for v in range(3):
        img = canvas(16, 16)
        d = ImageDraw.Draw(img)
        for x, y in [(3 + v, 12), (8, 10 + v), (12 - v, 13)]:
            d.line((x, y, x - 1, y - 3), fill=(49, 103, 43, 255))
            d.line((x, y, x + 1, y - 4), fill=(135, 181, 71, 255))
        save(img, 'tiles', f'tuft_{v}.png')
    save(trim(tcell(EXT, 18, 11, 1, 1, KEY_PINK)), 'tiles', 'flower_0.png')
    # Reuse scattered blossoms, not the large flowering bush, as a ground overlay.
    save(trim(tcell(EXT, 18, 11, 1, 1, KEY_PINK)).transpose(Image.Transpose.FLIP_LEFT_RIGHT),
         'tiles', 'flower_1.png')
    pebble = canvas(16, 16)
    d = ImageDraw.Draw(pebble)
    d.ellipse((4, 10, 7, 12), fill=(100, 107, 96, 255))
    d.ellipse((10, 12, 12, 13), fill=(155, 157, 139, 255))
    save(pebble, 'tiles', 'pebble.png')

# ================================================================= 2. objects
def build_objects():
    """Trees, rocks, bushes, fences, street furniture and landmarks."""
    k = KEY_PINK

    save(trim(tcell(EXT, 21, 8, 1, 2, k)), 'sprites', 'dead_tree.png')

    # ---- rocks -----------------------------------------------------------
    lump = trim(tcell(DUN, 20, 9, 1, 1, k))             # complete rounded stone
    save(centered(lump, (20, 18)), 'sprites', 'rock_0.png')
    save(centered(lump.transpose(Image.Transpose.FLIP_LEFT_RIGHT), (20, 18)),
         'sprites', 'rock_1.png')
    save(centered(trim(tcell(WLD, 27, 1, 1, 1, k)), (22, 18)), 'sprites', 'rock_2.png')
    # ---- plants ----------------------------------------------------------
    save(trim(tcell(EXT, 19, 8, 1, 1, k)), 'sprites', 'stump.png')             # real tree stump
    save(centered(trim(tcell(EXT, 18, 9, 1, 1, k)), (18, 18)), 'sprites', 'bush.png')
    save(centered(trim(tcell(EXT, 19, 9, 1, 1, k)), (16, 16)), 'sprites', 'bush_plain.png')
    # The dungeon crop was a pile of stones, not a mushroom.
    save(centered(art_objects.mushroom(), (16, 16)), 'sprites', 'mushroom.png')
    save(trim(tcell(EXT, 21, 11, 1, 1, k)), 'sprites', 'potted_plant.png')
    save(trim(tcell(EXT, 22, 11, 1, 1, k)), 'sprites', 'urn.png')

    # ---- street furniture ------------------------------------------------
    save(trim(tcell(EXT, 24, 0, 2, 2, k)), 'sprites', 'well.png')
    save(centered(trim(tcell(EXT, 20, 14, 1, 1, k)), (20, 18)), 'sprites', 'sign.png')
    save(trim(tcell(EXT, 27, 10, 2, 1, k)), 'sprites', 'bench.png')
    save(trim(tcell(EXT, 18, 10, 1, 1, k)), 'sprites', 'torch.png')
    save(trim(tcell(EXT, 21, 12, 1, 1, k)), 'sprites', 'campfire.png')
    save(trim(tcell(INT, 23, 10, 1, 1, k)), 'sprites', 'cauldron.png')        # real iron cauldron
    save(trim(tcell(INT, 25, 8, 1, 1, k)), 'sprites', 'basin.png')            # real wooden bucket
    bed = canvas(28, 12)
    d = ImageDraw.Draw(bed)
    d.rectangle((1,6,26,11),fill='#986644',outline='#614c36'); d.line((2,7,25,7),fill='#d19d64')
    bush = trim(tcell(EXT,18,9,1,1,k)).resize((13,8),Image.Resampling.NEAREST)
    bed.alpha_composite(bush,(1,0)); bed.alpha_composite(bush,(14,0))
    save(bed, 'sprites', 'flowerbed.png')
    save(trim(tcell(EXT, 20, 11, 1, 1, k)), 'sprites', 'mailbox.png')

    # ---- fences ----------------------------------------------------------
    save(trim(rect(EXT, 19 * 16, 12 * 16, 16, 16, k)), 'sprites', 'fence.png')
    save(trim(rect(EXT, 18 * 16, 12 * 16, 16, 16, k)), 'sprites', 'fence_post.png')
    save(trim(tcell(EXT, 18, 12, 3, 1, k)), 'sprites', 'fence_gate.png')
    save(trim(tcell(EXT,22,10,1,1,k)), 'sprites', 'ladder.png')

    # ---- landmarks -------------------------------------------------------
    save(centered(trim(tcell(EXT, 26, 8, 1, 2, k)), (24, 40)), 'sprites', 'statue.png')
    save(centered(trim(tcell(EXT, 27, 8, 1, 2, k)), (22, 40)), 'sprites', 'pillar.png')
    save(trim(tcell(EXT, 24, 13, 3, 3, k)), 'sprites', 'monument.png')
    save(trim(tcell(EXT, 27, 13, 3, 3, k)), 'sprites', 'pavilion.png')  # canvas tent, for camps
    arch=canvas(36,35); pier=trim(tcell(EXT,27,8,1,2,k)); arch.alpha_composite(pier,(0,4))
    arch.alpha_composite(pier.crop((0,6,pier.width,pier.height)),(25,10))
    lintel=ramp(tile(EXT,15,11),'48525b 646d71 818683 a3a395 bfb9a3')
    arch.alpha_composite(lintel.crop((0,0,16,8)),(1,2));arch.alpha_composite(lintel.crop((0,0,6,5)),(17,2))
    save(arch,'sprites','ruin_arch.png')

    # ---- interior dressing (shops, inns, houses) -------------------------
    save(trim(tcell(EXT, 27, 5, 1, 1, k)), 'sprites', 'barrel.png')
    crate = tile(INT,13,3); d=ImageDraw.Draw(crate)
    d.rectangle((0,0,15,15),outline='#513c30',width=2); d.line((2,2,13,13),fill='#ab8058',width=2)
    save(crate,'sprites','crate.png')
    save(trim(tcell(INT, 26, 7, 1, 1, k)), 'sprites', 'table.png')            # round wooden table
    save(trim(tcell(INT, 26, 10, 1, 1, k)), 'sprites', 'counter.png')         # real shop counter
    save(trim(tcell(INT, 26, 6, 1, 1, k)), 'sprites', 'lantern.png')
    save(trim(tcell(INT, 24, 6, 1, 1, k)), 'sprites', 'candle.png')
    save(trim(tcell(INT, 27, 6, 1, 1, k)), 'sprites', 'plate.png')
    save(trim(tcell(INT, 25, 6, 1, 1, k)), 'sprites', 'keg.png')
    save(trim(tcell(INT, 24, 10, 1, 2, k)), 'sprites', 'bed.png')             # real cosy bed
    save(trim(tcell(INT, 26, 8, 1, 1, k)), 'sprites', 'stool.png')
    save(trim(tcell(EXT, 20, 10, 1, 1, k)), 'sprites', 'noticeboard.png')
    save(trim(tcell(INT, 18, 1, 3, 2, k)), 'sprites', 'shelf.png')            # real bookcase / shop shelf
    save(trim(tcell(EXT, 24, 7, 3, 1, k)), 'sprites', 'longtable.png')        # real long wooden table

    # ---- chests: closed and open, from the object charset -----------------
    obj = 'CharSet/Object1.png'
    closed = trim(keyed(rect(obj, 2 * 72, 128 + 64, 24, 32), KEY_PINK2))
    save(centered(closed, (18, 18)), 'sprites', 'chest_closed.png')
    opened = trim(keyed(rect(obj, 2 * 72, 128 + 96, 24, 32), KEY_PINK2))
    save(centered(opened, (18, 22)), 'sprites', 'chest_open.png')


# ================================================================= 4. heroes
CELL_W, CELL_H = 24, 32
CHAR_W, CHAR_H = 72, 128


def charset_char(sheet, col, row, key=KEY_CYAN):
    im = keyed(load(sheet), key)
    return im.crop((col * CHAR_W, row * CHAR_H, col * CHAR_W + CHAR_W, row * CHAR_H + CHAR_H))


def walk_sheet(sheet, col, row, key=KEY_CYAN):
    """Re-order RM2k frames into idle/walkA/walkB columns and down/up/right rows.

    RM2k charset row order is 0=UP, 1=RIGHT, 2=DOWN, 3=LEFT.
    The game draws row 0=DOWN, row 1=UP, row 2=RIGHT (horizontally flipped for LEFT).
    """
    block = charset_char(sheet, col, row, key)
    out = canvas(CELL_W * 3, CELL_H * 3)
    for dst_row, src_row in ((0, 2), (1, 0), (2, 1)):
        for dst_col, src_col in enumerate((1, 0, 2)):
            out.alpha_composite(
                block.crop((src_col * CELL_W, src_row * CELL_H,
                            src_col * CELL_W + CELL_W, src_row * CELL_H + CELL_H)),
                (dst_col * CELL_W, dst_row * CELL_H))
    return out


def charset_cell(sheet, col, row, dir_row=2, key=KEY_CYAN):
    """Extract the middle animation frame of a charset character facing ``dir_row``.
    RM2k rows: 0=up, 1=right, 2=down (front), 3=left.
    """
    block = charset_char(sheet, col, row, key)
    return trim(block.crop((CELL_W, dir_row * CELL_H, CELL_W * 2, (dir_row + 1) * CELL_H)))


def portrait(sheet, col, row, size=48):
    im = load(sheet)
    return im.crop((col * 48, row * 48, col * 48 + 48, row * 48 + 48)).resize(
        (size, size), Image.Resampling.NEAREST)


def build_heroes():
    A = 'CharSet/Actor1.png'
    # Aldric the knight (red armour), Iris the mage (witch hat), Pip the cleric.
    save(walk_sheet(A, 2, 0), 'sprites', 'hero_knight.png')
    save(walk_sheet(A, 1, 1), 'sprites', 'hero_mage.png')
    save(walk_sheet(A, 0, 0), 'sprites', 'hero_cleric.png')
    roster = [(0, 1), (1, 0), (2, 1), (3, 0), (3, 1), (0, 0)]
    for i, (col, row) in enumerate(roster):
        save(walk_sheet(A, col, row), 'sprites', f'hero_alt{i}.png')

    F = 'FaceSet/Actor1.png'
    save(portrait(F, 2, 0), 'sprites', 'face_knight.png')     # red-helmeted knight Aldric
    save(portrait(F, 1, 1), 'sprites', 'face_mage.png')       # witch-hat mage Iris
    save(portrait(F, 0, 0), 'sprites', 'face_cleric.png')     # brown-haired cleric Pip
    save(portrait(F, 2, 0, 64), 'sprites', 'face_knight_big.png')

    P = 'FaceSet/People1.png'
    save(portrait(P, 0, 0), 'sprites', 'face_villager0.png')
    save(portrait(P, 1, 0), 'sprites', 'face_villager1.png')
    save(portrait(P, 2, 0), 'sprites', 'face_villager2.png')
    save(portrait(P, 3, 0), 'sprites', 'face_villager3.png')
    save(portrait(P, 2, 1), 'sprites', 'face_elder.png')      # grey-bearded elder
    save(portrait(P, 0, 1), 'sprites', 'face_mayor.png')
    save(portrait(P, 1, 1), 'sprites', 'face_woman.png')
    save(portrait(P, 1, 0), 'sprites', 'face_child.png')
    P2 = 'FaceSet/People2.png'
    save(portrait(P2, 0, 0), 'sprites', 'face_smith.png')
    save(portrait(P2, 3, 2), 'sprites', 'face_bard.png')
    A2 = 'FaceSet/Actor2.png'
    save(portrait(P, 3, 0), 'sprites', 'face_shopkeeper.png')
    save(portrait(P, 0, 1), 'sprites', 'face_innkeeper.png')
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
        ('npc_child',     'CharSet/People1.png', 1, 0),
        ('npc_guard',     'CharSet/People3.png', 3, 1),
        ('npc_soldier',   'CharSet/People3.png', 2, 1),
        ('npc_knight',    'CharSet/People3.png', 0, 1),
        ('npc_merchant',  'CharSet/People1.png', 3, 0),
        ('npc_smith',     'CharSet/People2.png', 3, 0),
        ('npc_priest',    'CharSet/People4.png', 0, 0),
        ('npc_farmer',    'CharSet/People5.png', 0, 1),   # straw-hat farmer
    ]
    for name, sheet, col, row in npcs:
        frames=walk_sheet(sheet,col,row)
        if name=='npc_child':
            small=canvas(72,96)
            for y in range(3):
                for x in range(3):
                    cell=frames.crop((24*x,32*y,24*x+24,32*y+32))
                    small.alpha_composite(cell.resize((19,25),Image.Resampling.NEAREST),(x*24+2,y*32+7))
            frames=small
        save(frames,'sprites',name+'.png')
    # animals: (0,0)=dog, (1,0)=cat, (2,0)=chicken, (3,0)=sheep
    for name, col, row in (('npc_dog', 0, 0), ('npc_chicken', 2, 0),
                           ('npc_sheep', 3, 0), ('npc_cat', 1, 0)):
        save(walk_sheet('CharSet/Animal.png', col, row), 'sprites', name + '.png')


# =============================================================== 5. monsters
# name: (sheet, char col, char row, field sprite size, optional tint)
MONSTERS = {
    'slime':       ('CharSet/Monster1.png', 0, 0, (36, 28), None),
    'imp':         ('CharSet/Monster1.png', 1, 0, (34, 40), None),
    'hornet':      ('Monster/Hornet.png',   0, 0, (38, 34), None),
    'skeleton':    ('CharSet/Monster1.png', 0, 1, (32, 42), None),
    'ghost':       ('CharSet/Monster1.png', 3, 0, (34, 40), None),
    'orc':         ('CharSet/Monster1.png', 3, 1, (38, 42), None),             # horned warrior orc
    'golem':       ('CharSet/Monster2.png', 0, 1, (40, 44), None),             # stone golem
    'night_rider': ('CharSet/Monster3.png', 3, 0, (42, 46), None),
}


def monster_sprite(entry, battle=False):
    sheet, col, row, _, rgb = entry
    if sheet.startswith('Monster/'):
        im = load(sheet)
        p = im.load()
        for y in range(im.height):
            for x in range(im.width):
                r, g, b, a = p[x, y]
                if r > 220 and g < 80 and b > 220:
                    p[x, y] = (0, 0, 0, 0)
        out = trim(im)
    else:
        # Field faces forward; the party stands to the right in battle.
        dir_row = 1 if battle else 2
        out = charset_cell(sheet, col, row, dir_row=dir_row, key=KEY_CYAN)
    return tint(out, rgb) if rgb else out


def build_slime_king():
    """Royal crowned Slime King built from the OpenRTP slime + golden pixel crown."""
    raw = tint(charset_cell('CharSet/Monster1.png', 0, 0, dir_row=2, key=KEY_CYAN),
               (145, 255, 195))
    # Scale the royal slime body 2x first so the golden crown sits cleanly in proportion
    base = raw.resize((raw.width * 2, raw.height * 2), Image.Resampling.NEAREST)
    bw, bh = base.size
    crown_h = 9
    king = canvas(max(bw + 4, 36), bh + crown_h)
    ox = (king.width - bw) // 2
    king.alpha_composite(base, (ox, crown_h))
    d = ImageDraw.Draw(king)
    cx = king.width // 2
    gold = (252, 214, 72, 255)
    dark = (96, 52, 14, 255)
    hi   = (255, 246, 176, 255)
    ruby = (224, 48, 64, 255)
    # Well-proportioned royal crown perched on top of the dome
    d.rectangle((cx - 7, 5, cx + 6, 10), fill=dark)
    for px in (cx - 6, cx, cx + 5):
        d.rectangle((px - 1, 2, px + 1, 6), fill=dark)
    d.rectangle((cx - 6, 6, cx + 5, 9), fill=gold)
    for px in (cx - 6, cx, cx + 5):
        d.rectangle((px, 3, px, 6), fill=gold)
        d.point((px, 3), fill=hi)
    d.rectangle((cx - 6, 7, cx + 5, 7), fill=hi)
    d.rectangle((cx - 1, 7, cx, 8), fill=ruby)
    # Fierce royal eyes on the slime body
    ey = crown_h + bh // 2 - 2
    d.rectangle((cx - 7, ey, cx - 5, ey + 2), fill=(24, 24, 36, 255))
    d.rectangle((cx + 4, ey, cx + 6, ey + 2), fill=(24, 24, 36, 255))
    d.point((cx - 6, ey), fill=(255, 240, 120, 255))
    d.point((cx + 5, ey), fill=(255, 240, 120, 255))

    save(centered(king, (48, 44)), 'sprites', 'slime_king.png')
    save(centered(king, (120, 110)), 'sprites', 'slime_king_battle.png')


def build_monsters():
    for name, entry in MONSTERS.items():
        w, h = entry[3]
        field_sp = monster_sprite(entry, battle=False)
        battle_sp = monster_sprite(entry, battle=True)
        save(centered(field_sp, (w, h)), 'sprites', name + '.png')
        save(centered(battle_sp, (w * 2, h * 2)), 'sprites', name + '_battle.png')
    # No standalone wolf, bat, wisp or snake on these character sheets:
    # the old selections were a tiger, armoured fiend, hooded man and scorpion.
    serpent=canvas(36,31); d=ImageDraw.Draw(serpent)
    d.ellipse((6,19,33,29),fill='#456457',outline='#344946');d.ellipse((10,20,26,26),fill='#87a469',outline='#587958')
    d.line([(26,22),(23,15),(17,12),(16,7)],fill='#344946',width=7)
    d.line([(25,22),(22,16),(16,12),(15,7)],fill='#82965e',width=5)
    d.polygon([(6,5),(14,3),(19,7),(16,11),(7,10),(4,8)],fill='#9bac70',outline='#405b4e')
    d.point((9,6),fill='#f3d380');d.line((5,9,1,11),fill='#aa6564');d.line((28,25,35,19),fill='#55775c',width=2)
    for name, im, dims in [('wolf',art_misc.wolf(),(40,32)),('wisp',art_misc.wisp(),(34,40)),
                           ('bat',art_misc.bat(),(40,34)),('serpent',serpent,(44,42))]:
        body=trim(im); w,h=dims
        field=centered_mid(body,(max(1,w*2//3),max(1,h*2//3)))
        f=canvas(w,h); f.alpha_composite(field,((w-field.width)//2,h-field.height)); save(f,'sprites',name+'.png')
        save(centered(body,(2*w,2*h)), 'sprites',name+'_battle.png')
    build_slime_king()

    # FaceSet/Monster.png is 4x4 portraits (cols 0..3, rows 0..3)
    M = 'FaceSet/Monster.png'
    save(portrait(M, 0, 0), 'sprites', 'face_slime.png')
    save(portrait(M, 3, 0), 'sprites', 'face_ghost.png')
    save(portrait(M, 0, 1), 'sprites', 'face_skeleton.png')
    save(portrait(M, 1, 0), 'sprites', 'face_demon.png')
    dog = centered_mid(charset_cell('CharSet/Animal.png',0,0), (40,40))
    panel=Image.new('RGBA',(48,48),'#334956'); panel.alpha_composite(dog,(4,4)); save(panel,'sprites','face_dog.png')


# ================================================================== 6. items
def build_coin_icon():
    """Crisp 16x16 golden coin icon matching the OpenRTP pixel palette."""
    im = canvas(16, 16)
    d = ImageDraw.Draw(im)
    d.ellipse((2, 2, 13, 13), fill=(244, 192, 52, 255), outline=(120, 68, 16, 255))
    d.ellipse((4, 4, 11, 11), fill=(255, 226, 102, 255), outline=(196, 134, 28, 255))
    d.rectangle((7, 5, 8, 10), fill=(196, 134, 28, 255))
    d.point((5, 5), fill=(255, 250, 210, 255))
    d.point((6, 4), fill=(255, 250, 210, 255))
    return im


def build_items():
    """Item and equipment icons lifted from the chipset prop banks."""
    k = KEY_PINK
    # Shop shelf goods are not standalone inventory flasks.
    save(art_misc.icon_potion(), 'icons', 'potion.png')
    save(art_misc.icon_hipotion(), 'icons', 'hi_potion.png')
    save(art_misc.icon_ether(), 'icons', 'ether.png')
    save(art_misc.icon_elixir(), 'icons', 'elixir.png')
    save(centered_mid(trim(tcell(INT, 25, 4, 1, 1, k)), (16, 16)), 'icons', 'book.png')
    save(centered_mid(trim(tcell(INT, 24, 6, 1, 1, k)), (16, 16)), 'icons', 'candle.png')
    save(centered_mid(trim(tcell(INT, 28, 6, 1, 1, k)), (16, 16)), 'icons', 'food.png')        # herbs / antidote
    save(centered_mid(trim(tcell(INT, 22, 11, 1, 1, k)), (16, 16)), 'icons', 'feather.png')    # phoenix feather
    save(centered_mid(trim(tcell(DUN, 28, 3, 1, 1, k)), (16, 16)), 'icons', 'gem.png')         # blue gem / key
    save(centered_mid(trim(tcell(INT, 22, 8, 1, 1, k)), (16, 16)), 'icons', 'shield.png')      # real heater shield
    save(centered_mid(trim(tcell(INT, 21, 10, 1, 1, k)), (16, 16)), 'icons', 'ring.png')       # pendant / ring / locket
    save(build_coin_icon(), 'icons', 'coin.png')
    key = canvas(16,16); d=ImageDraw.Draw(key)
    d.ellipse((1,1,8,8),outline='#674d43',width=3);d.ellipse((2,2,7,7),outline='#c39155',width=2)
    d.line((7,7,14,14),fill='#705042',width=3);d.line((7,6,14,13),fill='#cea264',width=2)
    d.line((10,10,12,8),fill='#ad7949',width=2);d.line((13,12,15,10),fill='#ad7949',width=2)
    save(key,'icons','key.png')
    crown=canvas(16,16);d=ImageDraw.Draw(crown)
    d.polygon([(1,4),(5,7),(8,2),(11,7),(14,4),(13,12),(3,12)],fill='#e3ad48',outline='#885641')
    d.line((3,10,13,10),fill='#ffe690');d.rectangle((7,8,8,9),fill='#aa4b53');save(crown,'icons','crown.png')

    # weapons: crisp sword blades tinted by metal tier
    sword_base = trim(tcell(INT, 20, 8, 1, 1, k))
    save(centered_mid(tint(sword_base, (240, 195, 130)), (16, 16)), 'icons', 'sword_0.png')    # bronze
    save(centered_mid(sword_base, (16, 16)), 'icons', 'sword_1.png')                           # iron
    save(centered_mid(tint(sword_base, (165, 225, 255)), (16, 16)), 'icons', 'sword_2.png')    # mythril/dragon

    # armour: real interior armour sprites
    save(centered_mid(trim(tcell(INT, 22, 9, 1, 1, k)), (16, 16)), 'icons', 'armor_0.png')     # leather vest
    save(ramp(art_misc.icon_armor(1),'363c49 505e6c 77868a a5b3af d8d8c5'), 'icons', 'armor_1.png')     # chain/helm
    save(centered_mid(trim(tcell(INT, 20, 9, 1, 1, k)), (16, 16)), 'icons', 'armor_2.png')     # mythril mail
    save(ramp(art_misc.icon_armor(2),'47394e 594760 80667d a08088 d1a18d'), 'icons', 'armor_3.png')     # dragon scale stand


# =================================================================== 7. UI
def build_window():
    blue = load('System/System.png')
    save(blue.crop((64, 0, 96, 32)), 'ui', 'window_frame.png')
    save(blue.crop((32, 0, 64, 32)), 'ui', 'window_selector.png')
    save(blue.crop((96, 0, 160, 8)), 'ui', 'gauge.png')
    save(blue.crop((32, 40, 144, 56)), 'ui', 'digits.png')

    royal = load('System/Royal.png')
    save(royal.crop((64, 0, 96, 32)), 'ui', 'window_frame_royal.png')
    save(royal.crop((32, 0, 64, 32)), 'ui', 'window_selector_royal.png')


# ================================================================= 8. title
def build_title():
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
    def frames(sheet, fw, fh, cols, rows):
        im = keyed(load(sheet), (24, 117, 24))  # Battle sheets use green, NOT chipset pink
        out = []
        for r in range(rows):
            for c in range(cols):
                cell = im.crop((c * fw, r * fh, c * fw + fw, r * fh + fh))
                if cell.getbbox():
                    out.append(trim(cell))
        return out

    sword = frames('Battle/Sword1.png', 96, 96, 5, 3)
    if sword:
        save(centered(sword[2], (30, 30)), 'fx', 'slash.png')
        save(art_misc.fx_spark(), 'fx', 'spark.png')
        save(art_misc.fx_sparkle(), 'fx', 'sparkle.png')
    blow = frames('Battle/Blow.png', 96, 96, 5, 3)
    if blow:
        save(centered(blow[0], (22, 22)), 'fx', 'impact.png')
        save(centered(blow[len(blow) // 2], (26, 22)), 'fx', 'poof.png')
        save(art_misc.fx_fireball(), 'fx', 'fireball.png')
    arrow = frames('Battle/Arrow.png', 96, 48, 5, 2)
    if arrow:
        save(centered(max(arrow, key=lambda im: im.width), (20, 20), upscale=False), 'fx', 'arrow.png')


# =====================================================================
def main():
    build_tiles()
    build_objects()
    build_buildings()
    build_backdrops()
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
