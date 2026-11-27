"""Walking-scale architecture assembled from the Exterior masonry, roof and
window banks. The little World-map settlement icons are NOT building facades.
"""
from PIL import Image, ImageDraw
from rtp_sheet import EXT, KEY_PINK, KEY_PINK2, tile, tcell, rect, canvas, save, trim
from landscape_art import ramp

ROOFS = {
    'clay': '583e41 794648 9b554a b9694f d88a61 efa974',
    'slate': '354253 465873 526e87 68869c 819aa7 a6b8b4',
    'ochre': '62493b 836043 a0794c c19657 d9ae69 ebc88c',
    'moss': '364f4c 496657 587b64 759475 91aa83 aaba92',
    'wine': '544052 745163 95596c af7080 c88c92 d9a6a6',
}
TIMBER = '#5b453d'; WOOD_LIGHT = '#a57e52'; IVORY = '#e4cea2'
STONE = '49545d 616a70 81898a 9ca29b b8b6a4 d0c8b5'


def pattern(im, points, texture):
    mask=Image.new('L',im.size); ImageDraw.Draw(mask).polygon(points,fill=255)
    layer=canvas(*im.size)
    for y in range(0,im.height,texture.height):
        for x in range(0,im.width,texture.width): layer.paste(texture,(x,y))
    im.paste(layer,(0,0),mask)


def window(im,x,y,chapel=False):
    d=ImageDraw.Draw(im)
    if chapel:
        im.alpha_composite(tile(EXT,24,2,KEY_PINK),(x-2,y-2));return
    # Recess, shutters, lit mullion, sill & two tiny reflected sky pixels.
    d.rectangle((x-3,y+1,x+12,y+11),fill=TIMBER)
    for dx in (x-2,x+10):
        d.rectangle((dx,y+2,dx+1,y+9),fill=WOOD_LIGHT)
    d.rectangle((x,y,x+9,y+10),fill='#695445');d.rectangle((x+1,y+1,x+8,y+9),fill='#445965')
    d.rectangle((x+2,y+2,x+7,y+7),fill='#83a7ab'); d.line((x+1,y+6,x+8,y+6),fill='#605c54')
    d.line((x+5,y+1,x+5,y+9),fill=TIMBER);d.line((x-1,y+11,x+10,y+11),fill='#c29e70')


def cottage(style, w, h, kind=0, gable=True, stone=False):
    im=canvas(w,h); d=ImageDraw.Draw(im); mid=w//2; eave=h//2
    brick=ramp(tile(EXT,15,10),STONE)
    plaster=ramp(tile(EXT,16,1),'c0a982 d4bc95 dec8a0 e5cfaa')
    roof=ramp(tile(EXT,14,13),ROOFS[style])
    # Receding end wall/foundation at right; centred door stays on its grid anchor.
    pattern(im,[(4,eave),(w-5,eave),(w-5,h-4),(w-10,h-1),(4,h-1)],brick)
    pattern(im,[(5,eave),(w-10,eave),(w-10,h-6),(5,h-6)],brick if stone else plaster)
    d.polygon([(w-10,eave),(w-4,eave-3),(w-4,h-5),(w-10,h-2)],fill='#9b876c')
    d.line((4,h-2,w-10,h-2),fill=TIMBER);d.line((5,h-5,w-10,h-5),fill=WOOD_LIGHT)
    for x in (5,w-11,mid-12,mid+11):
        d.rectangle((x,eave,x+1,h-5),fill=TIMBER);d.line((x+2,eave+2,x+2,h-6),fill=WOOD_LIGHT)
    if not stone:
        d.line((6,eave+3,14,eave+10),fill=WOOD_LIGHT,width=2)
        d.line((w-12,eave+3,w-20,eave+10),fill=WOOD_LIGHT,width=2)
    # Full pitch, shaded far hip and raised ceramic ridge.
    pitch=[(0,eave),(7,6),(w-11,6),(w-1,eave-3),(w-7,eave+2),(2,eave+2)]
    pattern(im,pitch,roof);d.line(pitch+[pitch[0]],fill=TIMBER)
    d.polygon([(w-11,7),(w-1,eave-3),(w-8,eave+1)],fill='#655456')
    d.line((8,6,w-12,6),fill='#e0c097',width=2);d.line((2,eave,w-8,eave),fill='#d1a881')
    d.line((2,eave+2,w-8,eave+2),fill='#523e39',width=2)
    # Chimney has a lip, an opening and a little flashing at the foot.
    cx=w-21;d.rectangle((cx,3,cx+6,13),fill='#8c7165',outline=TIMBER)
    d.rectangle((cx-1,1,cx+7,4),fill='#bbb1a1',outline=TIMBER);d.line((cx+1,2,cx+5,2),fill='#625a55')
    d.line((cx+1,8,cx+5,8),fill='#b5a595')
    if gable:
        gx=mid if kind==7 else w//3; gy=eave-11
        d.polygon([(gx-11,eave+1),(gx,gy-2),(gx+11,eave+1)],fill=IVORY,outline=TIMBER)
        d.line([(gx-13,eave+1),(gx,gy-4),(gx+13,eave+1)],fill=TIMBER,width=3)
        d.line([(gx-13,eave),(gx,gy-5),(gx+12,eave-1)],fill='#dabe96')
        d.line((gx,gy,gx,eave-1),fill=WOOD_LIGHT,width=2);d.line((gx-7,eave-2,gx+7,eave-2),fill=TIMBER)
    door=trim(rect('CharSet/Object1.png',24,64,24,32,KEY_PINK2))
    door=door.resize((15,23),Image.Resampling.NEAREST);im.alpha_composite(door,(mid-7,h-25))
    d.line((mid-9,h-1,mid+9,h-1),fill='#c8b493');d.point((mid+4,h-13),fill='#e5bf6d')
    for x in (10,w-23): window(im,x,h-22,kind==7)
    if kind==5:
        # A striped linen canopy, scalloped valance and a brass shop plaque.
        for i,x in enumerate(range(5,w-8,5)):
            edge=min(x+4,w-9); col=('#bc6a55','#ecd3a4')[i%2]
            d.polygon([(x,eave+4),(edge,eave+4),(edge-1,eave+10),(x-1,eave+10)],fill=col)
            d.rectangle((x-1,eave+10,edge-1,eave+12),fill=('#9a564a','#c9aa87')[i%2])
        d.line((5,eave+3,w-8,eave+3),fill=TIMBER)
    if kind==6:
        x=w-13; y=eave-2;d.line((x-2,y,x+8,y),fill=TIMBER,width=2)
        d.line((x+2,y,x+2,y+4),fill='#5f554c');d.rectangle((x-3,y+4,x+7,y+14),fill='#515467',outline=TIMBER)
        d.ellipse((x-1,y+5,x+5,y+11),fill='#e6c37c');d.ellipse((x+1,y+4,x+6,y+9),fill='#515467')
    return im


def tower(w,h):
    im=canvas(w,h); d=ImageDraw.Draw(im)
    wall=ramp(tile(EXT,13,1),STONE)
    pattern(im,[(5,21),(w-5,21),(w-5,h-4),(w-10,h-1),(5,h-1)],wall)
    d.rectangle((w-10,22,w-5,h-4),fill='#697176');d.line((6,h-2,w-10,h-2),fill='#424e59')
    for y in (23,h-18):
        d.rectangle((4,y,w-4,y+2),fill='#58616b');d.line((5,y,w-5,y),fill='#c4beaa')
    peak=[(0,22),(w//2,1),(w-1,22),(w-6,25),(4,25)]
    pattern(im,peak,ramp(tile(EXT,16,14),ROOFS['slate']));d.line(peak+[peak[0]],fill='#3d4656');d.line((w//2,2,4,22),fill='#9caeb8')
    window(im,w//2-5,28); d.rectangle((w//2-4,h-14,w//2+4,h-2),fill='#424551')
    d.arc((w//2-4,h-18,w//2+4,h-10),180,360,fill='#424551',width=3)
    return im


def build_buildings():
    for i,style in enumerate(('clay','slate','ochre','moss','wine')):
        save(cottage(style,62+(i%2)*4,54+(i%3)*3,gable=i!=2,stone=i==4),'sprites',f'house_{i}.png')
    save(cottage('moss',70,61,5,False),'sprites','shop.png')
    save(cottage('clay',74,66,6),'sprites','inn.png')
    chapel=canvas(76,78); chapel.alpha_composite(cottage('slate',70,58,7,True,True),(6,20))
    chapel.alpha_composite(tower(23,59),(0,1)); save(chapel,'sprites','temple.png')
    save(cottage('wine',78,68,8,True,True),'sprites','manor.png')
    save(tower(33,56),'sprites','tower.png')
    keep=canvas(98,84);d=ImageDraw.Draw(keep)
    masonry=ramp(tile(EXT,13,1),STONE)
    pattern(keep,[(12,42),(84,42),(84,81),(12,81)],masonry)
    for x in range(14,85,8):d.rectangle((x,37,x+4,43),fill='#9da5a3',outline='#626c76')
    gate=trim(tcell(EXT,28,8,2,2,KEY_PINK));keep.alpha_composite(gate,(33,51))
    keep.alpha_composite(tower(29,72),(0,10));keep.alpha_composite(tower(29,72),(69,10))
    upper=tower(34,67);keep.alpha_composite(upper.crop((0,0,34,44)),(32,1))
    d.line((16,49,81,49),fill='#d7ceba');save(keep,'sprites','castle.png')
    save(gate,'sprites','gate.png')
