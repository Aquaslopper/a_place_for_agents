"""Paint battlefield horizons at field pixel scale. Foreground silhouettes
are deliberately kept away from the ATB actors and their health bars."""
from math import sin
from random import Random
from PIL import Image, ImageDraw
from rtp_sheet import ROOT, save
from snes import BAYER4

# sky, haze, far ridges, ridge shadow, near hills, distant floor, near floor
PALETTES = {
    'meadow': '789fba ebdcb1 a2b3a0 8ba298 719783 a6af78 7e9a61',
    'forest': '668ca6 cedcc1 8aa398 748f8b 52796a 82976e 60845d',
    'amber':  'ab94ab f4dcac c5ada0 ac9392 997b71 c6ae72 a0945e',
    'fen':    '819393 c3cfb6 9eada3 8e9f97 718c80 889b81 617f70',
    'heath':  '8399af e2daca a8afad 85929a 768c87 a5aa8e 889480',
    'frost':  '7796b8 e0e9e8 acbfcf 859db3 6c879b c7dbe0 e5efee',
    'dunes':  'a0bcca f4d8aa d6af8e bd9682 b28a70 e5c594 d2b07c',
    'castle': '515774 a49aac 818195 6e748b 5c617b 9a9ca1 838998',
}


def rgb(s): return tuple(bytes.fromhex(s))
def mix(a,b,t): return tuple(round(x+(y-x)*t) for x,y in zip(a,b))


def gradient(d, W, y0, y1, c0, c1, steps=7, gamma=1.0):
    """SNES-style sky/ground gradient: quantised bands joined by ordered
    (Bayer 4x4) dither rows — never a smooth per-scanline blend."""
    for y in range(y0, y1):
        t = min(1.0, ((y - y0) / max(1, y1 - y0))) ** gamma
        f = t * (steps - 1)
        i = min(steps - 2, int(f)); frac = f - i
        ca = mix(c0, c1, i / (steps - 1)); cb = mix(c0, c1, (i + 1) / (steps - 1))
        b4 = BAYER4[y % 4]
        for x in range(W):
            d.point((x, y), fill=ca if b4[x % 4] / 16.0 >= frac else cb)


def build_backdrops():
    W,H,bank=512,192,77
    for idx,(name,pal) in enumerate(PALETTES.items()):
        rng=Random(5821+idx)
        sky,haze,far,shade,near,ground,front=map(rgb,pal.split())
        im=Image.new('RGB',(W,H)); d=ImageDraw.Draw(im)
        gradient(d, W, 0, bank, sky, haze, steps=7, gamma=0.85)
        sun=(337,28) if name!='dunes' else (155,26)
        d.ellipse((sun[0]-8,sun[1]-8,sun[0]+8,sun[1]+8),fill=mix(haze,(255,246,203),.75))
        for _ in range(11):
            x=rng.randrange(W); y=rng.randrange(9,49); length=rng.randrange(12,48)
            d.line((x,y,x+length,y),fill=mix(haze,sky,.26));d.line((x+4,y-1,x+length-3,y-1),fill=mix(haze,sky,.34))
        # Mountain facets behind the treeline; ochre mesas get level caps,
        # alpine peaks get windward snow, other regions have a lower horizon.
        for layer in range(2):
            x=-70
            while x<W+30:
                width=rng.randrange(52,112); tip=x+rng.randrange(23,width-12)
                high = name in ('heath','frost','castle')
                top=rng.randrange(13 if high else 33,43 if high else 58)+layer*10
                color=far if not layer else mix(near,far,.36)
                dark=shade if not layer else near
                flat=rng.randrange(12,28) if name=='dunes' else 1
                d.polygon([(x,bank),(tip-10,top+8),(tip,top),(tip+flat,top),(x+width+18,bank)],fill=color)
                d.polygon([(tip+flat,top),(x+width+18,bank),(tip+13,bank),(tip+4,top+15),(tip+7,top+12)],fill=dark)
                if name=='frost':
                    d.polygon([(tip-13,top+11),(tip,top),(tip+2,top+1),(tip+12,top+11),(tip+3,top+7),(tip-2,top+11),(tip-3,top+7)],fill=mix(haze,(248,253,255),.55))
                x+=width-11
        def prop(sprite,x,y,scale=1.,mist=0.):
            o=Image.open(ROOT/'sprites'/(sprite+'.png')).convert('RGBA'); a=o.getchannel('A')
            if mist:
                o=Image.blend(o,Image.new('RGBA',o.size,haze+(255,)),mist);o.putalpha(a)
            o=o.resize((max(1,round(o.width*scale)),max(1,round(o.height*scale))),Image.Resampling.NEAREST)
            im.paste(o,(round(x-o.width/2),round(y-o.height)),o)
        tree={'meadow':'tree_1','forest':'pine_0','amber':'amber_oak','fen':'willow',
              'heath':'pine_2','frost':'tree_snow','dunes':'yucca','castle':'pine_1'}[name]
        for x in range(-2,W+5,5 if name in ('forest','frost') else 9):
            y=bank-3+round(3*sin(x*.023));
            if name=='dunes' and rng.random()<.8:continue
            prop(tree,x,y,rng.uniform(.23,.51),.50)
        if name=='castle':prop('castle',249,bank,0.85,.14)
        elif name=='fen':prop('tower',112,bank-3,.55,.45)
        elif name=='meadow':prop('house_0',351,bank-2,.35,.48)
        gradient(d, W, bank, H, ground, front, steps=6, gamma=1.28)
        for y in range(bank,H):
            q=(y-bank)/(H-bank)
            for _ in range(20):
                x=rng.randrange(W);length=rng.randrange(1,4+int(q*5))
                d.line((x,y,x+length,y),fill=mix(mix(ground,front,q),near,rng.uniform(.03,.16)))
        # A bank on the far side; different horizontal detail for each place.
        for y in range(bank-4,bank+1):d.line((0,y,W,y),fill=mix(near,ground,(y-bank+5)/6))
        if name in ('fen','meadow','frost'):
            water=mix(sky,near,.24)
            shore=[(x,bank+5+round(2*sin(x*.029)+2*sin(x*.014))) for x in range(-5,W+6,5)]
            d.polygon([(0,bank),(W,bank)]+list(reversed(shore)),fill=water)
            for _ in range(47):
                x=rng.randrange(W);y=bank+rng.randrange(1,4);d.line((x,y,x+rng.randrange(2,14),y),fill=mix(water,haze,.5))
        elif name=='dunes':
            for base in (bank+6,bank+13,bank+19):
                line=[(x,base+round(3*sin(x*.025+base))) for x in range(-4,W+8,4)]
                d.line(line,fill=mix(ground,near,.24));d.line([(x,y-1) for x,y in line],fill=mix(ground,haze,.55))
        elif name=='castle':
            for y in (86,105,131,166):
                d.line((0,y,W,y),fill=mix(front,near,.46))
                for x in range(-32+(y%19)*4,W+20,52+(y-86)//2):d.line((x,y,x+(x-256)//24,y+7),fill=mix(front,near,.38))
        pairs={'meadow':('bush','tree_0'),'forest':('pine_0','tree_2'),'amber':('copper_oak','amber_oak'),
               'fen':('reeds','dead_tree'),'heath':('boulder','pine_2'),'frost':('crystal','tree_snow'),
               'dunes':('rock_1','yucca'),'castle':('pillar','torch')}
        for side in (0,1):
            for j in range(5):
                x=(rng.randrange(-6,45) if not side else W-rng.randrange(-6,45)); y=bank-2+j*3
                prop(pairs[name][j%2],x,y,.55+j*.075,.20)
        # `castle` is a palette entry only: its on-field backdrop is the
        # hand-kept backdrops/castle_field.png, so don't emit a stray
        # backdrops/castle.png that the manifest and validator would reject.
        if name=='castle': continue
        save(im,'backdrops',name+'.png')
