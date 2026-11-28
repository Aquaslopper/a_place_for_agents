# Visual audit: "does it look like Lufia II?" — no. Here is why.

**Status: diagnosis only. No runtime asset was changed by this pass.**

Reproduce everything below with:

```
python3 tools/audit_palette.py
```

It is read-only against `assets/`; it measures what already ships and writes
`docs/visual_audit/audit.json`, `tile_contrast.png` and `scene_before_after.png`.

---

## 1. The short answer

The game is not failing to look like *Lufia II* because it isn't a JRPG, or
because it isn't 16-bit-shaped. It is failing because of **palettes and shading
steps**, and the recent "retro pass" was built on a false premise.

Three passes in a row (PR #36, PR #37 and the second wave on #37) took the
position that the modern OpenRTP sheets were the problem and the `retro_*`
sheets were the answer. They recoloured nearly everything and reported success.
The art did not get noticeably more 16-bit, because **the pixels were never the
problem — the colour quantiser was.**

## 2. What is actually already right

Credit where it is due; this part of the repo is genuinely close to the target.

| Console-16-bit property | This game | Lufia II (Elcid frame) |
|---|---|---|
| Logical tile | 16×16 px (SNES, exactly) | 16×16 px |
| Canvas | 512×288, integer-scaled, crisp pixels | 256×224, 4:3 |
| Walking-scale buildings | steep checker-shingle roofs, half-timber cream plaster, stone foundations, two-pane pale windows, trade-sign pictograms instead of baked lettering | identical vocabulary |
| Character scale | 24×32 px walking sprites with 48×48 portraits | ~24×32 px, 40×40-ish portraits |
| Sprite hugeness vs tile | buildings are 60–100 px wide (4–6 tiles) | buildings are 4–6 tiles wide |

`tools/village_art.py` implements the §10.2 house dialect properly. The
`plates_lots.png` contact sheet reads like a SNES town. So the *composition*
is already Lufia-shaped.

## 3. The measured gap

Both frames measured on the same terms (colours per 16×16 tile, and local
contrast = mean |ΔL| between neighbouring pixels):

| | colours / screen | colours per 16px tile (mean / max) | local contrast |
|---|---|---|---|
| Lufia II — Elcid (reference) | **70** | **8.0 / 23** | **26.7** |
| this game — Elmswick (`village.png`) | **2228** | **25.8 / 172** | **15.0** |
| this game — `dialog.png` | 2304 | 28.6 / — | 14.8 |
| this game — `title.png` | 26702 | 111.4 / — | 6.0 |

The SNES gave each 16×16 tile a 15-colour palette and each screen 256. A 16-bit
JRPG village sits at 70 colours and 8 per tile. Elmswick uses **32× too many
colours per screen and 3× too many per tile** — while having **half the local
contrast** of the target.

That combination is the whole tell: *a smooth, low-contrast, high-colour image
is a modern render.* A 16-bit screen is the opposite: **few colours, far apart,
dithered.** This game is many colours, close together, softly blended. It reads
as an RPG-Maker game that has been downscaled, because that is what it is.

## 4. The mechanism: the ramps collapse the source

Every ground tile is built by `landscape_art.py::ramp()`, which palette-swaps a
source cell onto a short hard-coded family. Look at what that does to the
*luminance spread* of a single tile (max − min luma, out of 255):

| tile | colours | luminance spread |
|---|---|---|
| `grass_0` | 3 | **10** |
| `dirt` | 3 | **19** |
| `water_0` | 4 | **21** |
| `grass_2` (forest) | 4 | **24** |
| `cobble` | 4 | 77 |
| `flagstone` | 4 | 74 |
| source `retro_Exterior` grass cell | 3 | **109** |
| source `retro_World` dirt cell | 4 | **171** |
| source `retro_Exterior` forest cell | 4 | **150** |

The source cells carry 109–171 of contrast. The shipped tiles carry **10–24**.
The ramp did not re-source the texture; it **flattened** it. `ramp()` uses
`round(i * (len(colors)-1) / (len(keys)-1))`, so a three-colour family turns a
109-step source into `70a264 / 77a766 / 7eab68` — three greens **ten luma steps
apart**. Every meadow, path and forest floor in the game is that three-colour
speckle, which is why the field looks like felt.

See `docs/visual_audit/tile_contrast.png` — source, shipped, and a 5-step ramp
of the same pattern side by side.

## 5. The three-point claim that turned out to be false

PR #37 and its second wave rested on three measurable claims. None hold:

**(a) "The `retro_*` chipsets carry the 16-bit texture."**
They are not the SNES-era sheets; they are modern sheets with a different
palette. Comparing the vendored pairs tile by tile, only **1–2%** of cells
share an identical pixel silhouette (1% Exterior, 1% World, 2% Dungeon). They
are different artwork with comparable, still-modern shading. The 16-bit content
was thrown away by the ramp regardless of which sheet was read.

**(b) "The pass changed the visible world."**
`retro_pass_before_after.png` reports a mean per-channel delta of 28/255 — but
**43% of its pixels are bit-identical** between the BEFORE and AFTER rows, and
look at the rows: `ground: cobble`, `ground: dunes`, `sea phase 0`, `cave
floor` are the *same picture*. What changed is `bed`, `shelf`, `counter`,
`brick` — i.e. `mute()`, whose whole job is to harmonise colour, so identical
silhouettes are expected. The pass demonstrably tilted interiors warmer and
more muted. It did not add 16-bit texture to the field.

**(c) "The buildings are in the Lufia II dialect."**
The dialect table is followed, but the roofs are dithered **checkerboard**, not
rows of overlapping shingles running down the slope (compare the Elcid roofs),
the `house_*`/`inn`/`shop` sprites carry **38–40 distinct luminance steps** in
their red family, and their silhouette edge is only **~33% dark contour**. An
SNES sprite is 1px-contoured all the way around and uses ~5 values per material.

## 6. What else is off, in order of how much it hurts

1. **No dark contour on sprites.** Edge pixels below luma 62: buildings 33%,
   flora 31%, props 45%. Lufia II silhouettes are essentially 100% outlined.
   Without it, trees and houses sink into the ground and the scene loses the
   cut-out, layered look.
2. **Characters have no outline and no readable value structure.** The player
   sprite is a red blob with no separation from the grass; the NPCs are soft
   pastels.
3. **Portraits are modern-rendered.** `face_*.png` carry 70–160 colours each
   (median across all runtime assets: 11). They read as smooth modern pixel
   portraits, not 40×40 SNES faces.
4. **The battle background is a smooth gradient landscape** (665 colours, local
   contrast 6.6). Lufia II battlegrounds are painted in flat dithered bands.
5. **The title screen is the least 16-bit thing in the game** — 26,702 colours.
6. **Dirt and meadow are nearly the same hue family.** `dirt` (spread 19) and
   `beach` sit 20 steps from the grass in luminance, so paths do not read as
   paths from a distance. Lufia reads because road and grass differ hard.
7. **Rendering-time washes flatten further.** During dusk the whole field gets
   `Fade({255,150,70}, 0.09)` on top (`render.cpp`), which pushes contrast
   *down* on an image that already needs more. Verified: shipped `grass_0`
   `(119,167,102)` → screenshot `(131,166,99)` is exactly that overlay.

## 7. The fix, in order, with acceptance criteria

`docs/visual_audit/scene_before_after.png` is the cheap demonstration: the
right-hand plate is built from the **same shipped sprites and tiles**, only
(a) re-ramped onto 5-step ramps that preserve the source's spread and (b) given
a 1px dark contour. That alone already reads as a different decade.

1. **Rewrite `ramp()` to keep the spread.** Target 4–5 values, spread
   ≥ 90/255, hue-shifted dark→light (curve `#2f4a28 → #a2c169`, not
   three near-identical greens). The 5-step ramps in `tools/audit_palette.py`
   (`RAMP_16BIT`) are a starting point, not final art.
2. **Give every sprite a 1px contour** (`outline()` in the same tool), and cut
   each material to ≤5 values. Buildings first, then flora, then props.
3. **Cap the palette per screen.** Add a build-time check — the audit already
   computes it: fail if a town screen exceeds ~120 colours or ~12 per tile.
   That single guard would have caught all three of these passes.
4. **Re-shade the faces** onto 8–10 values, 1px outline, 3-value skin ramp.
5. **Flatten the battle backdrops** into dithered bands (the 16-bit way) and
   cut the title screen to a palette.
6. **Remove the render-time colour washes** from the field, or make them
   contrast-preserving. Never fade toward a warm mid-tone on an already-flat
   image.

Do these and the existing architecture — houses, layout, walking scale, UI
chrome, the whole Lufia-shaped skeleton that is already here — starts to look
like the game it is imitating. Do them *before* adding any more new content:
the skeleton is fine, the paint is wrong, and three passes have now tuned the
wrong variable.

## 8. Side finding (unrelated bug)

`README.md` line 4 tells agents to start at
`docs/NEXT_AGENT_PROMPT.md`, and line 21 links to it. **That file does not
exist in the repository.** Agents following the README hit a 404 on the
repository's own onboarding document.

---

## 7. This pass: the field is painted, not palette-swapped

The diagnosis above was right about *what* was wrong and wrong about *what to
do about it*. Sections 1-6 proposed re-sharing the source sheets and re-capping
the palette; four passes tried that. The actual disease was the pipeline itself:
`landscape_art.py::ramp()` takes a 109-step source cell and maps it onto three
hard-coded colours, so **nothing in the source sheet can survive**. No amount of
re-sourcing fixes a function that throws the texture away.

`tools/field_art.py` replaces that path for the field. It paints the 21 terrain
materials directly:

* **five values per material** with a hue shift (cool shadow, warm light), where
  the old pipeline used three;
* **structured texture** instead of speckle — grass clumps and blades, sand
  scallops, stone beds with faceted tops and dark mortar, wave glints, snow
  drifts, ice cracks, leaf litter, cobble courses;
* **1px hard contours at every material seam**: dark under the lip, lit along
  the top, mid on the sides, plus Bayer dithering in the transition band;
* **relief** rebuilt (`relief.png`) so a bank casts a three-band shadow onto the
  water below it and an escarpment gets a lit rim and a dark lip;
* water seams deliberately do **not** get the lit rim, or a pond's depth
  boundaries read as stripes across it.

### Measured, same scene, same pipeline

`tools/field_preview.py` composes a 512×288 field from the shipped assets using
the renderer's own corner-masked stamping, so the two rows differ only in the
atlas. Run `python3 tools/field_preview.py` (and with `FIELD_ASSETS` pointing at
an older checkout for the "before").

| | colours/screen | colours per 16px tile | local contrast |
|---|---|---|---|
| Lufia II — Elcid (reference) | 70 | 8.0 | **26.7** |
| field, before | 116 | 6.0 | **7.0** |
| field, after | 116 | 7.6 | **20.0** |

Local contrast — the thing that made the meadow look like felt — is up **2.9×**,
and colours per tile are now inside the reference's band instead of being a
three-value speckle. Colours per screen did not move (the palette was never the
problem in the field).

Preview: `docs/visual_audit/field_before_after.png`.

### What is *not* fixed

* **Grass repeats visibly at 1:1.** Three texture variants per material is not
  enough to hide a 16px cell; the next pass should cut 5-6 variants per surface
  and have `sampleGround` choose by hash, not by `variant % 3`.
* **No in-game capture was possible in this environment.** There is no GL/X11
  runtime here, so both rows are offline composites of the shipped assets, not
  screenshots. The renderer path is unchanged; the atlas format is identical.
* **Props were left alone.** A first attempt repainted the trees and made the
  silhouettes worse, so the existing canopy sprites were restored. The trees in
  the "before" row are the trees in the "after" row.
* **UI chrome** was flattened in `core.cpp` (no drop shadow, no gradient, black /
  light / black rules around a flat plate) and the missing `getVirtualMouse()`
  was implemented, but neither can be seen here for the same reason.
