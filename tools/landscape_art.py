"""Field terrain, transitions and vegetation. All source cells are 16 px.

Atlas convention: one row per material/variation, sixteen marching-squares
masks (NW=1, NE=2, SW=4, SE=8). No commercial game pixels are used here.
"""
from PIL import Image, ImageDraw, ImageChops
from math import sin, pi
import rtp_sheet as r


def ramp(im, colors, foliage=False):
    """Palette swap, not a tint: preserve shading and the silhouette/trunk."""
    im = im.convert('RGBA')
    def keep(p):
        r, g, b, a = p
        return a and (not foliage or g > r * 1.07 and g > b)
    keys = sorted({p for p in im.getdata() if keep(p)},
                  key=lambda p: (p[0]*3 + p[1]*6 + p[2], p))
    colors = [tuple(bytes.fromhex(c)) + (255,) for c in colors.split()]
    lut = {p: colors[round(i*(len(colors)-1)/max(1, len(keys)-1))] for i, p in enumerate(keys)}
    out = im.copy(); out.putdata([lut.get(p, p) for p in im.getdata()]); return out


def make_landscape():
    tile, save = r.tile, r.save
    def fill(sheet, x, y, colors):
        return ramp(tile(sheet, x, y), colors)
    # Interior cells of the RM2k terrain blocks, not isolated preview/edge tiles.
    grass = tile(r.EXT, 1, 10); earth = tile(r.WLD, 7, 2); sand = tile(r.EXT, 4, 14)
    meadow = ramp(grass, '70a264 77a766 7eab68')
    forest = ramp(tile(r.EXT, 4, 10), '477657 4d7c59 54825c 5a865e 609063')
    amber = ramp(grass, '8b9055 959858 9c9e5c')
    heath = ramp(tile(r.WLD, 4, 10), '6e816a 7e8e75 8b987c 95a17f a8ae89')
    mire = ramp(tile(r.EXT, 4, 10), '4c7570 537c72 598075 628775')
    dune = ramp(earth, 'd5b27e dab884 dfbe88')
    ripples = ramp(sand, 'c49b68 cda572 d4ac77 dbb47e')
    leaves = ramp(earth, 'b89956 bda35c c9ae62')
    mesa = ramp(earth, 'b78d68 c09973 caa47b')
    beach = ramp(earth, 'd1b886 dfc89b e9d5ac f0deb5')
    dirt = ramp(earth, 'ba9470 c49e78 cca881')
    rock = fill(r.DUN, 16, 2, '666f70 747f79 889081 9aa088 a8aa92')
    snow = fill(r.DUN, 7, 2, 'c8dce5 dce9ec e6efee')
    frostpath = ramp(earth, 'a3b8bf b2c6ca bbcfd2')
    snowcrag = ramp(grass, 'b1c6d1 bbd1d9 c5dbe1')
    ice = fill(r.DUN, 10, 2, '91bfce 9ac6d4 a4cfd9 adcfdc')
    cobble = fill(r.EXT, 10, 6, '686e6b 7b827c 8d9285 9b9d8d b1ad98 bfbba2')
    court = fill(r.EXT, 7, 10, '576277 667485 7e8b98 949eaa a6acb1')
    # Furrows run continuously across plots. Plant tops are separate from soil.
    soil = ramp(earth, '75553e 866047 926c4e a67c56'); d = ImageDraw.Draw(soil)
    for y in (2, 10):
        d.line((0,y,15,y),fill='#664e40'); d.line((0,y+2,15,y+2),fill='#b18c61')
    water = [fill(r.EXT, i, 5, '377e91 3c8497 438a9a 4d909e 5698a5') for i in range(3)]
    deep = [fill(r.EXT, i, 5, '325e7d 356580 396c85 3e728b 447a90') for i in range(3)]
    # Also export single swatches for the art pipeline; the field uses one atlas.
    for name, im in {'grass_0':meadow,'grass_1':ImageChops.offset(meadow,7,3),
                     'grass_2':forest,'grass_3':heath,'grass_dark':forest,'dirt':dirt,'sand':dune,
                     'beach':beach,'swamp':mire,'tundra':heath,'rock_hill':rock,
                     'rock_hill2':ImageChops.offset(rock,3,7),'soil':soil,'snow':snow,
                     'snow_drift':ImageChops.offset(snow,6,5),'ice':ice,
                     'ice_crack':ImageChops.offset(ice,4,7),'cobble':cobble,'flagstone':court,
                     'water_0':water[0],'water_1':water[1],'deep_water':deep[0]}.items():
        save(im, 'tiles', name+'.png')

    # Row order shared with Surface in render.cpp. Every surface has 3 phases:
    # water uses real animation frames; land uses registered texture offsets.
    surfaces = [deep, water, beach, meadow, forest, amber, heath, mire, dune, snow,
                ice, rock, dirt, soil, cobble, court, leaves, ripples, mesa, frostpath, snowcrag]
    def inside(mask, x, y):
        u, v = (x+0.5)/16, (y+0.5)/16
        # Straight shores retain a hand-pixelled 1px irregularity. All four
        # edges agree with adjacent cells, including at multi-material joins.
        u += 0.048*sin(v*2*pi); v += 0.048*sin((x+0.5)*2*pi/16)
        weights = [(1-u)*(1-v), u*(1-v), (1-u)*v, u*v]
        return sum(w for i,w in enumerate(weights) if mask & 1<<i) >= 0.5
    masks = [Image.new('L',(16,16)) for _ in range(16)]
    for k, mask in enumerate(masks):
        mask.putdata([255*inside(k,x,y) for y in range(16) for x in range(16)])
    atlas = r.canvas(256, len(surfaces)*3*16)
    for row, material in enumerate(surfaces):
        phases = material if isinstance(material,list) else [material,ImageChops.offset(material,7,5),
                                                              ImageChops.offset(material,2,9)]
        for phase, im in enumerate(phases):
            for k, mask in enumerate(masks):
                patch = im.copy(); patch.putalpha(mask); atlas.paste(patch,(16*k,16*(row*3+phase)))
    save(atlas, 'tiles', 'landscape.png')
    # Boundaries get their own relief pass. Use a vertical offset of the same
    # contour for south-facing cut banks and escarpments (not a tiled wall).
    relief = r.canvas(256, 80)
    crag = fill(r.WLD, 22, 6, '4e5158 5b5961 726a67 897d71 9d8f7c b2a18b')
    sandstone = fill(r.WLD,22,6,'815f55 926a58 a67a5e b48b6a c49b76 d6ad82')
    blue_stone = fill(r.WLD,22,6,'506479 637a8a 8297a0 9cabb1 b4c6cd cad6db')
    # Extend masks by clamping to border corner weights outside this cell.
    def at(k,x,y):
        return inside(k,max(-0.5,min(x,15.5)),max(-0.5,min(y,15.5)))
    for k in range(1,15):
        for y in range(16):
            for x in range(16):
                land = at(k,x,y)
                if land and not all(at(k,x+dx,y+dy) for dx,dy in ((-1,0),(1,0),(0,-1),(0,1))):
                    relief.putpixel((k*16+x,y),(107,107,75,255))
                    relief.putpixel((k*16+x,y+16),(205,224,230,255))
                elif not land:
                    for row,colors in enumerate([[(124,99,74),(100,84,70),(79,81,74),(86,156,161)],
                                                  [(162,187,201),(136,164,183),(110,145,159),(141,193,204)]]):
                        for dep,color in enumerate(colors,1):
                            if at(k,x,y-dep):
                                relief.putpixel((k*16+x,row*16+y),color+(255,));break
                        else:
                            if any(at(k,x+dx,y+dy) for dx,dy in ((-1,0),(1,0),(0,1))):
                                relief.putpixel((k*16+x,row*16+y),(141,196,190,255))
                if land:
                    for dep in range(1,8):
                        if not at(k,x,y+dep):
                            c = crag.getpixel((x,y)) if dep<7 else (166,168,145,255)
                            if dep==1: c=(72,77,77,255)
                            relief.putpixel((k*16+x,32+y),c)
                            for row,stone,lip in [(3,sandstone,(223,191,143,255)),(4,blue_stone,(223,236,237,255))]:
                                pixel=stone.getpixel((x,y)) if dep<7 else lip
                                relief.putpixel((k*16+x,row*16+y),pixel)
                            break
    save(relief,'tiles','relief.png')

    base = r.trim(r.tcell(r.EXT,22,8,2,2,r.KEY_PINK))
    def oak(colors,old=False):
        tree = ramp(base,colors,True)
        if not old: return tree
        # One old spreading crown; keep the complete trunk at its foot.
        out = r.canvas(43,39); out.alpha_composite(tree,(5,7))
        crown=tree.crop((0,0,tree.width,tree.height-6))
        out.alpha_composite(crown,(0,6));out.alpha_composite(crown,(11,5));out.alpha_composite(crown,(5,0))
        return r.trim(out)
    for name,im in [('tree_0',oak('355540 426c42 598344 75a04b 94b85c',True)),
                    ('tree_1',oak('395942 487447 658948 86a950 a9c363')),
                    ('tree_2',oak('2d5145 376644 49794a 648e53 83a25c',True)),
                    ('amber_oak',oak('713f3e 95563d b8793e d99b47 eec467',True)),
                    ('copper_oak',oak('71413f 974d40 bb6348 d88350 e9a866'))]:
        save(im,'sprites',name+'.png')
    import random
    rng=random.Random(32)
    willow=r.canvas(30,37); willow.alpha_composite(ramp(base,'335e53 416f5d 59856b 779776 90a77a',True).resize((28,34),Image.Resampling.NEAREST),(1,3))
    dr=ImageDraw.Draw(willow)
    for x in (2,4,6,10,18,22,25,27):
        y=16+rng.randrange(4); end=y+rng.randrange(7,15)
        dr.line([(x,y),(x-1,y+5),(x-1,end)],fill='#486d58');dr.line((x,y+2,x,end-3),fill='#81a478')
    save(willow,'sprites','willow.png')
    conifer = r.trim(r.tcell(r.EXT,20,8,1,2,r.KEY_PINK))
    for i,pal in enumerate(('315443 416d4f 538756 72a25e 93b672',
                             '2d514b 3a6b5b 4a7c65 5d8b71 77a384',
                             '354d47 446251 577a5b 739466 93aa79')):
        save(ramp(conifer,pal,True),'sprites',f'pine_{i}.png')
    save(ramp(conifer,'496376 658294 97b4bf c4dbe0 edf4ec',True),'sprites','tree_snow.png')
    save(r.trim(r.tcell(r.DUN,22,10,2,2,r.KEY_PINK)),'sprites','boulder.png')
    save(r.trim(r.tcell(r.DUN,19,9,1,1,r.KEY_PINK)),'sprites','crystal.png')
    # Region-specific low plants, all at walking scale; never indoor furniture.
    reeds = r.canvas(17,19); d=ImageDraw.Draw(reeds)
    for x,y,h,lean in [(4,16,9,-2),(9,17,15,1),(12,15,10,2),(6,17,13,-1),(10,16,7,-3)]:
        d.line((x,y,x+lean,y-h),fill='#365e52');d.line((x+1,y-2,x+lean+1,y-h+1),fill='#99aa68')
        if h>=13: d.rectangle((x+lean-1,y-h-2,x+lean,y-h+1),fill='#785438')
    save(reeds,'sprites','reeds.png')
    scrub=r.canvas(19,15);d=ImageDraw.Draw(scrub)
    for x,y in [(1,8),(4,2),(7,5),(10,0),(14,3),(17,6),(18,10)]:
        d.polygon([(8,13),(x,y),(12,12)],fill='#61765a',outline='#51624d');d.line((x,y,10,11),fill='#a7b87b')
    save(scrub,'sprites','yucca.png')
    lily=r.canvas(16,16);d=ImageDraw.Draw(lily)
    for x,y in [(2,8),(10,4)]:
        d.ellipse((x,y,x+5,y+3),fill='#376758');d.line((x+1,y,x+4,y),fill='#87a866')
    d.rectangle((11,2,12,4),fill='#f0c5ab');d.point((13,3),fill='#fff0c8');save(lily,'tiles','lilies.png')
    crop = r.canvas(16,16);d=ImageDraw.Draw(crop)
    for x,y in [(3,7),(11,14)]:
        d.ellipse((x-1,y-3,x+2,y),fill='#dcc5a2');d.polygon([(x,y-2),(x-3,y-5),(x,y-5)],fill='#568445')
        d.line((x,y-3,x+2,y-7),fill='#8dad5b');d.line((x+1,y-3,x+4,y-5),fill='#4c7440')
    save(crop,'tiles','turnips.png')
    deck=r.canvas(16,16);d=ImageDraw.Draw(deck)
    for y in range(0,16,4):
        d.rectangle((0,y,15,y+3),fill='#a17b52');d.line((0,y,15,y),fill='#c29b6a')
        d.line((0,y+3,15,y+3),fill='#71563f');d.line((y//2+1,y+2,y//2+5,y+2),fill='#8e6c4e')
        d.point((1,y+1),fill='#695445');d.point((14,y+1),fill='#695445')
    save(deck,'tiles','bridge.png')
