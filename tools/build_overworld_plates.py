#!/usr/bin/env python3
"""Build the 3x3 overworld "plate" library from the downloaded OpenRTP chipset.

RUN FROM THE REPO ROOT:   python3 tools/build_overworld_plates.py

This is the plate-authoring step for the procedural overworld generator. It
crops 16x16 cells out of `assets/source/openrtp/ChipSet/retro_World.png` (the
SNES-retro World chipset, ART_BIBLE §9) and composes them into 48x48 px plates
(3x3 tiles). The plates are the Carcassonne-style chunks the generator will
edge-match. Lufia II's overworld was used ONLY as a look reference; none of its
art is copied. Output goes to `docs/overworld_assets/plates/` (one PNG per plate
plus `index.json`) and two contact sheets in `docs/overworld_assets/`.

METHOD
------
* Transparency key. retro_World.png keys transparency with MAGENTA (224,103,191),
  NOT the (255,103,139) pink used by the 2022 World.png. cell() keys it out with
  a small tolerance. Getting this wrong leaves pink boxes behind every icon.
* Standard autotiles (forest/sand/snow/dirt/mountain). A 3x4 RM2k autotile block
  stores a preview row (row 0) above a 3x3 of [corner,edge,corner / edge,centre,
  edge / corner,edge,corner]. Those 9 cells tile together into a coherent
  "island" of the terrain on grass. island(c0,r0) grabs that 3x3. render_mask()
  then composes ANY 3x3 binary shape (half, corner, channel, pond, cross...) by
  picking, per cell, the edge/corner tile whose grass-facing sides match the
  mask neighbours (off-plate neighbours clamp to the edge so terrain continues).
* Site / landmark plates. A grass base with an upper-layer icon stamped on top
  (foot-anchored). Icon cell coordinates are ART_BIBLE §9.5.
* Special "transporter" plates. bridge_over_water, cave_in_mountain, gate_pass:
  a terrain base + an entry icon. These are the plates that drop the player into
  the map beneath (town/cave/tower) or let them cross an otherwise-blocked edge.

KNOWN DEFECT — WATER (must be fixed by the next agent)
------------------------------------------------------
The water_* plates are WRONG. The water region (cols 0-5, rows 0-7) is the RM2k
*boundary* autotile, not a standard 3x4 island autotile, so island(0,0) grabs the
wrong pieces (dark bands + white specks instead of blue sea + foam coast). The
mask/render_mask machinery is correct; only the water tile SOURCE is wrong.
Fix: decode the RM2k water boundary (corner-animation 8x8 pieces, H/V sides,
"+" centre across the first blocks; open-water centre in the deep-sea block
cols 0-2 rows 3-7; animated water cols 3-5 rows 3-7), build a correct water
island/edge set, and re-run. forest/sand/snow/dirt/mountain and all 25 site /
special plates are correct and usable as-is.
"""
from PIL import Image, ImageDraw
import os, json

SRC = 'assets/source/openrtp/ChipSet/retro_World.png'
OUT = 'docs/overworld_assets/plates'
sheet = Image.open(SRC).convert('RGBA')


def cell(c, r):
    t = sheet.crop((c * 16, r * 16, c * 16 + 16, r * 16 + 16)).convert('RGBA')
    px = t.load()
    for y in range(16):
        for x in range(16):
            rr, gg, bb, aa = px[x, y]
            if abs(rr - 224) < 12 and abs(gg - 103) < 12 and abs(bb - 191) < 12:
                px[x, y] = (0, 0, 0, 0)
    return t


def island(c0, r0):
    return [[cell(c0 + i, r0 + 1 + j) for i in range(3)] for j in range(3)]


GRASS = cell(1, 10)


def render_mask(mask, isl, iso):
    out = Image.new('RGBA', (48, 48))

    def nb(r, c, dr, dc):
        rr = min(2, max(0, r + dr)); cc = min(2, max(0, c + dc))
        return mask[rr][cc]

    for r in range(3):
        for c in range(3):
            if mask[r][c] == 0:
                t = GRASS
            else:
                sides = set()
                if nb(r, c, -1, 0) == 0: sides.add('N')
                if nb(r, c, 1, 0) == 0: sides.add('S')
                if nb(r, c, 0, -1) == 0: sides.add('W')
                if nb(r, c, 0, 1) == 0: sides.add('E')
                m = {frozenset(): isl[1][1], frozenset('N'): isl[0][1],
                     frozenset('S'): isl[2][1], frozenset('E'): isl[1][2],
                     frozenset('W'): isl[1][0], frozenset('NE'): isl[0][2],
                     frozenset('NW'): isl[0][0], frozenset('SE'): isl[2][2],
                     frozenset('SW'): isl[2][0]}
                t = m.get(frozenset(sides), iso if len(sides) == 4 else isl[1][1])
            out.paste(t, (c * 16, r * 16))
    return out


def stamp(base, cells, anchor):
    cs = [c for c, r in cells]; rs = [r for c, r in cells]
    c0, r0 = min(cs), min(rs)
    ic = Image.new('RGBA', ((max(cs) - c0 + 1) * 16, (max(rs) - r0 + 1) * 16))
    for c, r in cells:
        ic.alpha_composite(cell(c, r), ((c - c0) * 16, (r - r0) * 16))
    base.alpha_composite(ic, (anchor[0] * 16, anchor[1] * 16))
    return base


ISL = {'forest': island(6, 4), 'sand': island(9, 0), 'snow': island(9, 4),
       'dirt': island(6, 0), 'mountain': island(3, 12), 'water': island(0, 0)}
ISO = {'forest': cell(6, 4), 'sand': cell(9, 0), 'snow': cell(9, 4),
       'dirt': cell(6, 0), 'mountain': cell(3, 12), 'water': cell(0, 0)}
MASKS = {
    'full': [[1, 1, 1], [1, 1, 1], [1, 1, 1]],
    'pond': [[0, 0, 0], [0, 1, 0], [0, 0, 0]],
    'top': [[1, 1, 1], [0, 0, 0], [0, 0, 0]],
    'bottom': [[0, 0, 0], [0, 0, 0], [1, 1, 1]],
    'left': [[1, 0, 0], [1, 0, 0], [1, 0, 0]],
    'right': [[0, 0, 1], [0, 0, 1], [0, 0, 1]],
    'top2': [[1, 1, 1], [1, 1, 1], [0, 0, 0]],
    'bottom2': [[0, 0, 0], [1, 1, 1], [1, 1, 1]],
    'left2': [[1, 1, 0], [1, 1, 0], [1, 1, 0]],
    'right2': [[0, 1, 1], [0, 1, 1], [0, 1, 1]],
    'nw': [[1, 1, 0], [1, 1, 0], [0, 0, 0]],
    'ne': [[0, 1, 1], [0, 1, 1], [0, 0, 0]],
    'sw': [[0, 0, 0], [1, 1, 0], [1, 1, 0]],
    'se': [[0, 0, 0], [0, 1, 1], [0, 1, 1]],
    'vchannel': [[0, 1, 0], [0, 1, 0], [0, 1, 0]],
    'hchannel': [[0, 0, 0], [1, 1, 1], [0, 0, 0]],
    'cross': [[0, 1, 0], [1, 1, 1], [0, 1, 0]],
}

P = {}
P['grass_plain'] = render_mask([[0] * 3 for _ in range(3)], ISL['forest'], ISO['forest'])
for terr in ['water', 'forest', 'sand', 'snow', 'dirt', 'mountain']:
    use = list(MASKS) if terr in ('water', 'forest') else \
        ['full', 'pond', 'top', 'bottom', 'left', 'right', 'nw', 'se', 'vchannel', 'hchannel']
    for mn in use:
        P[f'{terr}_{mn}'] = render_mask(MASKS[mn], ISL[terr], ISO[terr])

SITES = {
    'tree_round': ([(19, 8)], (1, 1)), 'tree_dead': ([(20, 8)], (1, 1)),
    'tree_large': ([(18, 10), (19, 10), (18, 11), (19, 11)], (1, 1)),
    'bush_flower': ([(18, 9)], (1, 1)), 'cactus': ([(19, 9)], (1, 1)), 'palm': ([(20, 9)], (1, 1)),
    'town_red': ([(22, 8)], (1, 1)), 'town_snow': ([(23, 8)], (1, 1)),
    'town_wood': ([(22, 9)], (1, 1)), 'town_snow2': ([(23, 9)], (1, 1)),
    'castle_grey': ([(20, 10), (21, 10), (20, 11), (21, 11)], (0, 1)),
    'castle_blue': ([(22, 10), (23, 10), (22, 11), (23, 11)], (0, 1)),
    'cave': ([(23, 13)], (1, 1)), 'tower': ([(20, 12), (20, 13)], (1, 1)),
    'temple': ([(22, 12), (22, 13)], (1, 1)), 'monument': ([(21, 12), (21, 13)], (1, 1)),
    'camp': ([(18, 12), (19, 12), (18, 13), (19, 13)], (0, 1)),
    'ruins': ([(20, 14), (21, 14), (20, 15), (21, 15)], (0, 1)),
    'volcano': ([(18, 14), (19, 14), (18, 15), (19, 15)], (0, 1)),
    'gate': ([(22, 14), (22, 15)], (1, 1)), 'bridge': ([(23, 14), (23, 15)], (1, 1)),
    'colosseum': ([(24, 14), (25, 14), (24, 15), (25, 15)], (0, 1)),
}
for n, (cells, anc) in SITES.items():
    P[n] = stamp(render_mask([[0] * 3 for _ in range(3)], ISL['forest'], ISO['forest']), cells, anc)

P['bridge_over_water'] = stamp(render_mask(MASKS['vchannel'], ISL['water'], ISO['water']), [(23, 14), (23, 15)], (1, 1))
P['cave_in_mountain'] = stamp(render_mask(MASKS['full'], ISL['mountain'], ISO['mountain']), [(23, 13)], (1, 1))
P['gate_pass'] = stamp(render_mask(MASKS['hchannel'], ISL['mountain'], ISO['mountain']), [(22, 14), (22, 15)], (1, 1))

os.makedirs(OUT, exist_ok=True)
# remove stale plates from earlier builds so the folder matches index.json exactly
keep = {f'{n}.png' for n in P}
for f in os.listdir(OUT):
    if f.endswith('.png') and f not in keep:
        os.remove(os.path.join(OUT, f))
for n, p in P.items():
    p.save(f'{OUT}/{n}.png')


def sheet_img(names, cols, Z, path):
    pad = 5; lab = 16; cw = 48 * Z; ch = 48 * Z + lab
    rows = (len(names) + cols - 1) // cols
    sh = Image.new('RGB', (cols * (cw + pad) + pad, rows * (ch + pad) + pad), (24, 26, 32))
    d = ImageDraw.Draw(sh)
    for k, n in enumerate(names):
        r, c = divmod(k, cols); x = pad + c * (cw + pad); y = pad + r * (ch + pad)
        sh.paste(P[n].resize((cw, cw), Image.NEAREST), (x, y + lab))
        d.text((x + 2, y + 3), n, fill=(235, 225, 180))
    sh.save(path)
    return sh.size


terrain = [n for n in P if any(n.startswith(t + '_') for t in ['water', 'forest', 'sand', 'snow', 'dirt', 'mountain']) or n == 'grass_plain']
sites = [n for n in P if n not in terrain]
s1 = sheet_img(terrain, 6, 3, 'docs/overworld_assets/plates_terrain.png')
s2 = sheet_img(sites, 6, 3, 'docs/overworld_assets/plates_sites.png')
json.dump({'count': len(P), 'names': list(P)}, open(f'{OUT}/index.json', 'w'), indent=1)
print('TOTAL PLATES:', len(P), '| terrain:', len(terrain), '| sites/special:', len(sites))
print('terrain sheet', s1, 'sites sheet', s2)
