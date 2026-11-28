#!/usr/bin/env python3
"""build_art.py — regenerate the SNES-style art in assets/ (tiles, sprites, icons, fx, title).

Generated art is size-checked; vendored OpenRTP art is then adapted by
import_openrtp.py, including its native 24x32 walking frames.

    python3 tools/build_art.py
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from PIL import Image
import art_terrain as T
import art_objects as O
import art_actors as A
import art_misc as M

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
A_DIR = os.path.join(ROOT, "assets")
written = []

EXPECT = {
    "tiles": (16, 16),
    "sprites/house": (66, 58), "sprites/shop": (66, 58), "sprites/inn": (66, 58),
    "sprites/castle": (74, 78),
    "sprites/hero": (48, 96), "sprites/face": (32, 32),
    "sprites/tree": (34, 46), "sprites/pine": (30, 48),
    "sprites/slime": (46, 38), "sprites/wolf": (62, 36), "sprites/bat": (46, 28),
    "sprites/wisp": (40, 40), "sprites/golem": (56, 64),
    # the tide quest (src/sail.cpp / src/world.cpp)
    "sprites/boat": (72, 96), "sprites/npc_ferryman": (72, 96),
    "sprites/tidewell": (40, 30), "sprites/tidewell_dry": (40, 30),
    "sprites/tide_stone": (26, 36),
    "icons": (16, 16),
    "fx/slash": (28, 28), "fx/impact": (18, 18), "fx/fireball": (18, 18),
    "fx/spark": (14, 14), "fx/sparkle": (16, 16), "fx/poof": (26, 18),
    "title_bg": (512, 288),
}

def save(img, *parts):
    path = os.path.join(A_DIR, *parts)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    key = "/".join(parts[:-1])
    base = os.path.splitext(parts[-1])[0]
    want = EXPECT.get(key) or EXPECT.get("sprites/" + base) or EXPECT.get(base)
    if want:
        assert img.size == want, f"{path}: got {img.size}, renderer expects {want}"
    img.save(path)
    written.append(os.path.relpath(path, ROOT))

def main():
    # ---------------------------------------------------------- tiles
    for v in range(3):
        save(T.grass(v), "tiles", f"grass_{v}.png")
        save(T.tuft(v), "tiles", f"tuft_{v}.png")
        save(T.flower(v), "tiles", f"flower_{v}.png")
    save(T.dirt(), "tiles", "dirt.png")
    save(T.cobble(), "tiles", "cobble.png")
    save(T.sand(), "tiles", "sand.png")
    save(T.snow(), "tiles", "snow.png")
    save(T.ice(), "tiles", "ice.png")
    save(T.rock_hill(), "tiles", "rock_hill.png")
    save(T.water(0), "tiles", "water_0.png")
    save(T.water(1), "tiles", "water_1.png")

    # ---------------------------------------------------------- objects
    for v in range(3):
        save(O.tree(v), "sprites", f"tree_{v}.png")
        save(O.pine(v), "sprites", f"pine_{v}.png")
        save(O.rock(v), "sprites", f"rock_{v}.png")
        save(O.house(v), "sprites", f"house_{v}.png")
    save(O.dead_tree(), "sprites", "dead_tree.png")
    save(O.bush(), "sprites", "bush.png")
    save(O.mushroom(), "sprites", "mushroom.png")
    save(O.well(), "sprites", "well.png")
    save(O.sign(), "sprites", "sign.png")
    save(O.fence(), "sprites", "fence.png")
    save(O.shop(), "sprites", "shop.png")
    save(O.inn(), "sprites", "inn.png")
    save(O.castle(), "sprites", "castle.png")

    # ---------------------------------------------------------- heroes
    for style, name in (("knight", "hero_knight"), ("mage", "hero_mage"), ("cleric", "hero_cleric")):
        save(A.hero_sheet(style), "sprites", f"{name}.png")
    for style, name in (("knight", "face_knight"), ("mage", "face_mage"), ("cleric", "face_cleric")):
        save(A.portrait(style), "sprites", f"{name}.png")

    # ---------------------------------------------------------- enemies
    for key, (fn, _size) in M.ENEMIES.items():
        save(fn(), "sprites", f"{key}.png")

    # ---------------------------------------------------------- icons
    save(M.icon_potion(), "icons", "potion.png")
    save(M.icon_hipotion(), "icons", "hi_potion.png")
    save(M.icon_ether(), "icons", "ether.png")
    save(M.icon_elixir(), "icons", "elixir.png")
    for i in range(3):
        save(M.icon_sword(i), "icons", f"sword_{i}.png")
        save(M.icon_armor(i), "icons", f"armor_{i}.png")
    save(M.icon_coin(), "icons", "coin.png")

    # ---------------------------------------------------------- fx + title
    save(M.fx_slash(), "fx", "slash.png")
    save(M.fx_impact(), "fx", "impact.png")
    save(M.fx_fireball(), "fx", "fireball.png")
    save(M.fx_spark(), "fx", "spark.png")
    save(M.fx_sparkle(), "fx", "sparkle.png")
    save(M.fx_poof(), "fx", "poof.png")
    save(M.title_bg(), "title_bg.png")

    # Adapt the vendored OpenRTP originals after generating the remaining art.
    import import_openrtp
    import_openrtp.main()
    # The field is painted from scratch last, so the palette-ramped chipset
    # cells (flat 3-colour families) never reach assets/. See tools/field_art.py.
    import field_art
    field_art.main()
    print(f"wrote {len(written)} files, then applied OpenRTP art, then painted the field")

if __name__ == "__main__":
    main()
