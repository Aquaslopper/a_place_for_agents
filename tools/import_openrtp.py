#!/usr/bin/env python3
"""Adapt EasyRPG OpenRTP sheets to the game's sprite/texture layout.

Original sheets and provenance are kept under assets/source/openrtp. Everything
here is cropping, colorkeying or composing their pixels; no network is needed.
"""
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent / 'assets'
SRC = ROOT / 'source' / 'openrtp'

def load(path, key=None):
    im = Image.open(SRC / path).convert('RGBA')
    if key:
        im.putdata([(r,g,b,0 if (r,g,b)==key else a) for r,g,b,a in im.getdata()])
    return im

def save(im, path):
    im.save(ROOT / path)

def crop(im, x, y, w, h):
    return im.crop((x, y, x+w, y+h))

def canvas(w, h):
    return Image.new('RGBA', (w,h))

def centered(img, size):
    out = canvas(*size)
    out.alpha_composite(img, ((size[0]-img.width)//2, size[1]-img.height))
    return out

def main():
    world = load('ChipSet/World.png')
    ext = load('ChipSet/Exterior.png')
    objects = load('ChipSet/Exterior.png', (255,103,139))
    # The central 16x16 subtiles of RPG Maker autotiles are repeatable fills.
    for name, x, y in [
        ('grass_0',16,144), ('grass_1',16,144), ('grass_2',16,144),
        ('dirt',112,32), ('sand',160,32), ('snow',160,96),
        ('rock_hill',224,96),
        ('water_0',16,80), ('water_1',32,80),
    ]:
        save(crop(world,x,y,16,16), 'tiles/'+name+'.png')
    save(crop(ext,160,160,16,16), 'tiles/cobble.png')
    # Sparse vegetation from the same chipset, cut out on transparent pink.
    flowers = crop(objects,288,176,16,16)
    for i in range(3):
        save(flowers, f'tiles/flower_{i}.png')
        save(canvas(16,16), f'tiles/tuft_{i}.png')
    # Actual OpenRTP trees and props, with existing bottom-centre anchors.
    tree = crop(objects,352,128,32,32)
    pine = crop(objects,320,128,16,32)
    for i in range(3):
        save(centered(tree,(34,46)), f'sprites/tree_{i}.png')
        save(centered(pine,(30,48)), f'sprites/pine_{i}.png')
        save(centered(crop(objects,464,0,16,16),(26,20)), f'sprites/rock_{i}.png')
    save(crop(objects,336,128,16,32), 'sprites/dead_tree.png')
    save(crop(objects,304,144,16,16), 'sprites/bush.png')
    save(crop(objects,320,224,16,16), 'sprites/sign.png')
    save(crop(objects,288,192,16,16), 'sprites/fence.png')

    # Assemble houses from unscaled OpenRTP wall/roof, window and door pieces.
    # The saved canvases retain the renderer's existing bottom-centre anchor.
    for name, roof_y in [('house_0',192),('house_1',192),('house_2',192),('shop',208),('inn',192)]:
        out = canvas(66,58)
        wall = crop(ext,240,0,48,48)
        out.alpha_composite(wall,(9,10))
        # repeating 16px roof texture, with contrasting edge trim
        for x in (1,17,33,49):
            out.alpha_composite(crop(ext,224 if name not in ('shop',) else 256,roof_y,16,16),(x,0))
            out.alpha_composite(crop(ext,224 if name not in ('shop',) else 256,roof_y,16,8),(x,16))
        # Window and door are original OpenRTP objects.
        window = crop(objects,432,32,16,16)
        out.alpha_composite(window,(14,30)); out.alpha_composite(window,(36,30))
        out.alpha_composite(crop(objects,384,64,16,32),(25,26))
        if name in ('shop','inn'):
            out.alpha_composite(crop(objects,320,224,16,16),(48,39))
        save(out,'sprites/'+name+'.png')

    # RM2k charset is 24x32, 3 frames, directions up/left/down/right.
    # The game expects idle/step/step and down/up/side, so re-order the frames.
    a1 = load('CharSet/Actor1.png', (0,147,146))
    for name, sheet, col, charrow in [('knight',a1,2,0), ('mage',a1,1,1), ('cleric',a1,2,1)]:
        out = canvas(72,96)
        for dstrow, srcrow in enumerate((2,0,1)):
            for dstcol, srccol in enumerate((1,0,2)):
                frame = crop(sheet,col*72+srccol*24,charrow*128+srcrow*32,24,32)
                out.alpha_composite(frame,(dstcol*24,dstrow*32))
        save(out,'sprites/hero_'+name+'.png')
    # First monster in this sheet is MerchantShade's CC0 slime.  The other
    # characters in the source sheet have their own credits; see CREDITS.md.
    monster = load('CharSet/Monster1.png', (0,147,146))
    slime = crop(monster,24,64,24,32)
    slime = slime.crop(slime.getbbox())
    slime = slime.resize((slime.width*2, slime.height*2), Image.Resampling.NEAREST)
    save(centered(slime,(46,38)), 'sprites/slime.png')
    # CC0 face set: 48x48 cells adapted to the existing portrait size.
    faces = load('FaceSet/Actor1.png')
    for name, col, row in [('knight',2,0),('mage',1,1),('cleric',3,1)]:
        save(crop(faces,col*48,row*48,48,48).resize((32,32),Image.Resampling.NEAREST), 'sprites/face_'+name+'.png')

if __name__ == '__main__':
    main()
