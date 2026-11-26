# Credits & Licenses

## Engine
- [raylib](https://github.com/raysan5/raylib) 5.5 — zlib/libpng license.
  Fetched at build time via CMake FetchContent; not vendored.

## Font
- **Press Start 2P** by CodeMan38 — SIL Open Font License 1.1.
  Bundled at `assets/fonts/PressStart2P-Regular.ttf`; license text in
  `assets/fonts/OFL.txt` (also fetched from the google/fonts repository).

## Art, world, music, SFX
All generated in-repository by `tools/gen_assets.py` (pillow + numpy).
Licensed CC0 by the authors of this repository — do whatever you like
with them. Standing on the shoulders of the Liberated Pixel Cup and the
RPG Maker community for the *style*, but not a single third-party pixel
is included.

## Search notes (what was NOT used, and why)
During asset selection we looked at GitHub-hosted packs and rejected:
- RPG Maker MV/MZ decrypted assets (license does not allow redistribution)
- Liberated Pixel Cup starter kit (Git LFS not fetchable in this environment)
- Pipoya / AsLib map chips (free to *use in games*, not to redistribute)
- Ninja Adventure (CC0, but side-view ninja art — wrong genre fit)

so the world you see is 100% generated.
