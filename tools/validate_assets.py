#!/usr/bin/env python3
"""validate_assets.py — Workstream A7: automatic asset validation.

    python3 tools/validate_assets.py

Stdlib only (no Pillow needed). Fails (exit 1) if any check fails.

What this enforces — see docs/ART_BIBLE.md for the contract behind each rule:

  1. No duplicate basenames among *runtime* assets (assets/** minus the
     vendored source tree). Directory-qualified loading makes duplicates a
     silent-wrong-file hazard. (Vendored upstream sheets under
     assets/source/openrtp/ keep their upstream folder layout and are
     exempt — CharSet/Actor1.png vs FaceSet/Actor1.png is upstream RTP
     structure, not a runtime reference.)
  2. Category dimension contracts: 16x16 tiles/icons, 512x192 backdrops,
     native OpenRTP character sheets (72x96 = 3 frames x 3 rows of 24x32),
     48x48 faces, battle sprites at exactly 2x their field sprite, and an
     explicit allow-list for atlases/fx/ui plates. Anything else must be
     recorded in ART_MANIFEST.md with its exact size (rule 3).
  3. ART_MANIFEST.md parity: manifest table == files on disk, exactly,
     in both directions, and every entry carries a license note.
  4. Every non-pattern asset literal referenced from src/*.cpp exists.
  5. One UI finish: no DrawRectangleRounded* anywhere in src/*.cpp
     (use drawWindow / drawWindowBlue / drawPixelPanel).
  6. Biome palette discipline: every colour ramp literal in
     tools/landscape_art.py has at most 8 colours (ART_BIBLE A2).
  7. OpenRTP guardrails (the PR #14 lesson — a validator must catch an
     asset-replacement, not bless it):
       * tools/import_openrtp.py exists and imports from the vendored tree
       * assets/source/openrtp/ is intact (>= 60 files, key sheets at
         native size)
       * CREDITS.md credits OpenRTP per source sheet and does NOT claim
         the runtime art is "100% original repository-authored"
       * the three hero sheets are three distinct files (A5 readability
         starts with not shipping the same silhouette three times)
"""
import hashlib
import pathlib
import re
import struct
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
ASSETS = ROOT / "assets"
SRC = ROOT / "src"

failures = []
warnings = []


def fail(msg):
    failures.append(msg)


def png_size(path):
    data = path.read_bytes()[:24]
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        return None
    return struct.unpack(">II", data[16:24])


def runtime_files():
    out = []
    for p in sorted(ASSETS.rglob("*")):
        if p.is_file() and "source" not in p.relative_to(ASSETS).parts:
            out.append(p)
    return out


def all_asset_files():
    return [p for p in sorted(ASSETS.rglob("*")) if p.is_file()]


# ------------------------------------------------------------------ rules
def check_duplicates(files):
    seen = {}
    for p in files:
        rel = p.relative_to(ROOT).as_posix()
        if p.name in seen:
            fail(f"duplicate runtime basename {p.name!r}: {seen[p.name]} and {rel}")
        seen[p.name] = rel


TILE_EXCEPTIONS = {  # trimmed overlays / atlases — see ART_BIBLE A4
    "flower_0.png": (14, 13), "flower_1.png": (14, 13),
    "landscape.png": (256, 1008), "relief.png": (256, 80),
    # Grid-aligned procedural town/interior materials — ART_BIBLE §10.
    "town_facade_atlas.png": (96, 96),
    "town_masonry_atlas.png": (64, 48),
    "town_battlement_atlas.png": (96, 80),
    "town_shingles_red_atlas.png": (32, 32),
    "town_shingles_blue_atlas.png": (32, 32),
    "interior_wall_face_atlas.png": (64, 192),
}
FX_SIZES = {
    "arrow.png": (20, 20), "fireball.png": (18, 18), "impact.png": (22, 22),
    "poof.png": (26, 22), "slash.png": (30, 30), "spark.png": (14, 14),
    "sparkle.png": (16, 16),
}
UI_SIZES = {
    "window_frame.png": (32, 32), "window_frame_royal.png": (32, 32),
    "window_selector.png": (32, 32), "window_selector_royal.png": (32, 32),
    "digits.png": (112, 16), "gauge.png": (64, 8),
    "dawn.png": (512, 288), "night.png": (512, 288),
    "gameover_plate.png": (512, 288),
}
BOSS_BATTLE_SCALE = {"slime_king_battle.png": 2.5}  # boss exception, ART_BIBLE A5


def check_dimensions(files):
    sizes = {}
    for p in files:
        if p.suffix != ".png":
            continue
        rel = p.relative_to(ASSETS)
        wh = png_size(p)
        if wh is None:
            fail(f"{rel}: not a valid PNG")
            continue
        sizes[rel.as_posix()] = wh
        w, h = wh
        top = rel.parts[0] if len(rel.parts) > 1 else ""
        if top == "icons" and wh != (16, 16):
            fail(f"{rel}: icon must be 16x16, got {w}x{h}")
        elif top == "tiles":
            want = TILE_EXCEPTIONS.get(p.name, (16, 16))
            if wh != want:
                fail(f"{rel}: tile must be {want[0]}x{want[1]}, got {w}x{h}")
        elif top == "backdrops" and wh != (512, 192):
            fail(f"{rel}: backdrop must be 512x192, got {w}x{h}")
        elif top == "fx" and FX_SIZES.get(p.name) != wh:
            fail(f"{rel}: fx size {w}x{h} not in contract {FX_SIZES.get(p.name)}")
        elif top == "ui" and UI_SIZES.get(p.name) != wh:
            fail(f"{rel}: ui size {w}x{h} not in contract {UI_SIZES.get(p.name)}")
        elif len(rel.parts) == 1 and wh != (512, 288):
            fail(f"{rel}: root plate must be 512x288, got {w}x{h}")
        elif top == "sprites":
            name = p.name
            if name.startswith("interior_door_") and wh != (24, 96):
                fail(f"{rel}: door pose strip must be 24x96 (3 stacked 24x32 poses), got {w}x{h}")
            elif name.startswith(("hero_", "npc_")) and wh != (72, 96):
                fail(f"{rel}: character sheet must be 72x96 (3x 24x32 frames, "
                     f"3 rows), got {w}x{h}")
            elif name.startswith("face_") and name != "face_knight_big.png" and wh != (48, 48):
                fail(f"{rel}: face must be 48x48, got {w}x{h}")
            elif name == "face_knight_big.png" and wh != (64, 64):
                fail(f"{rel}: face_knight_big must be 64x64, got {w}x{h}")
    # battle sprites: exactly 2x the field sprite (boss exception above)
    by_name = {pathlib.Path(k).name: v for k, v in sizes.items()}
    for name, (w, h) in by_name.items():
        if name.endswith("_battle.png"):
            base = name.replace("_battle.png", ".png")
            if base not in by_name:
                fail(f"sprites/{name}: no field sprite {base}")
            else:
                bw, bh = by_name[base]
                scale = BOSS_BATTLE_SCALE.get(name, 2.0)
                if (w, h) != (int(bw * scale), int(bh * scale)):
                    fail(f"sprites/{name}: battle sprite must be {scale}x "
                         f"{base} ({bw}x{bh}), got {w}x{h}")
    return sizes


def check_manifest(files):
    manifest = ROOT / "ART_MANIFEST.md"
    if not manifest.exists():
        fail("ART_MANIFEST.md missing — run tools/generate_manifest.py")
        return
    text = manifest.read_text()
    entries = {}
    for m in re.finditer(r"^\| `(assets/[^`]+)` \| (\d+) B \| ([^|]+) \| ([^|]+) \|", text, re.M):
        entries[m.group(1)] = (int(m.group(2)), m.group(3).strip(), m.group(4).strip())
    on_disk = {p.relative_to(ROOT).as_posix() for p in files}
    by_rel = {p.relative_to(ROOT).as_posix(): p for p in files}
    for rel, (nbytes, dims, _note) in entries.items():
        p = by_rel.get(rel)
        if p is None:
            continue
        if p.stat().st_size != nbytes:
            fail(f"manifest byte size stale for {rel}: manifest {nbytes} B, disk {p.stat().st_size} B "
                 f"— run tools/generate_manifest.py")
        if p.suffix == ".png":
            wh = png_size(p)
            if wh and not dims.startswith(f"{wh[0]}×{wh[1]} "):
                fail(f"manifest dimensions stale for {rel}: manifest {dims!r}, disk {wh[0]}x{wh[1]}")
    for rel in sorted(on_disk - set(entries)):
        fail(f"manifest missing entry for {rel} — run tools/generate_manifest.py")
    for rel in sorted(set(entries) - on_disk):
        fail(f"manifest lists non-existent file {rel}")
    for rel, (_nbytes, _dims, license_note) in entries.items():
        if not re.search(r"CC0|CC BY|OFL|SIL|zlib|public domain", license_note):
            fail(f"manifest entry {rel} has no recognisable license note: {license_note!r}")


def check_code_refs():
    pat = re.compile(r'"((?:assets/)?(?:tiles|sprites|icons|fx|ui|backdrops|audio|fonts)/[^"]+|title_bg\.png|title_atlas\.png)"')
    missing = []
    for cpp in SRC.glob("*.cpp"):
        for m in pat.finditer(cpp.read_text(errors="ignore")):
            ref = m.group(1)
            if "%s" in ref:
                continue  # name-templated (enemy tables); manifest parity covers these
            if "%d" in ref:
                for i in range(3):  # every numbered family ships at least 0..2
                    if not (ASSETS / (ref % i)).exists():
                        missing.append(f"{cpp.name}: {ref % i}")
                continue
            path = ASSETS / ref if not ref.startswith("assets/") else ROOT / ref
            if ref in ("title_bg.png", "title_atlas.png"):
                path = ASSETS / ref
            if not path.exists():
                missing.append(f"{cpp.name}: {ref}")
    for m in sorted(set(missing)):
        fail(f"C++ references missing asset: {m}")
    # dynamic backdrop scenes in core.cpp
    core = (SRC / "core.cpp").read_text(errors="ignore")
    scenes = re.search(r'scenes\[BI_COUNT\] = \{([^}]+)\}', core)
    if scenes:
        for name in re.findall(r'"([^"]+)"', scenes.group(1)):
            if not (ASSETS / "backdrops" / f"{name}.png").exists():
                fail(f"C++ backdrop scene missing: backdrops/{name}.png")


def check_ui_finish():
    for cpp in SRC.glob("*.cpp"):
        if "DrawRectangleRounded" in cpp.read_text(errors="ignore"):
            fail(f"{cpp.name}: DrawRectangleRounded* found — use drawPixelPanel/drawWindow (ART_BIBLE A6)")


def check_ramps():
    text = (ROOT / "tools" / "landscape_art.py").read_text()
    for m in re.finditer(r"'([0-9a-f]{6}(?: [0-9a-f]{6})+)'", text):
        n = len(m.group(1).split())
        if n > 8:
            fail(f"landscape_art.py ramp has {n} colours (>8): {m.group(1)}")


def check_openrtp_guardrails(sizes):
    imp = ROOT / "tools" / "import_openrtp.py"
    if not imp.exists():
        fail("tools/import_openrtp.py missing — the OpenRTP import pipeline must never be deleted")
    elif "openrtp" not in imp.read_text().lower():
        fail("tools/import_openrtp.py no longer references OpenRTP")
    source = ASSETS / "source" / "openrtp"
    n = sum(1 for p in source.rglob("*") if p.is_file())
    if n < 60:
        fail(f"vendored OpenRTP source tree incomplete: {n} files (<60)")
    for sheet, want in [("CharSet/Actor1.png", (288, 256)),
                        ("ChipSet/Exterior.png", (480, 256)),
                        ("FaceSet/Actor1.png", (192, 192))]:
        p = source / sheet
        if not p.exists() or png_size(p) != want:
            fail(f"vendored OpenRTP sheet {sheet} missing or wrong size (want {want})")
    credits = (ROOT / "CREDITS.md").read_text()
    if "OpenRTP" not in credits:
        fail("CREDITS.md no longer credits OpenRTP")
    if "100% original repository-authored" in credits:
        fail("CREDITS.md claims '100% original repository-authored' art — "
             "the runtime art is OpenRTP-derived; never reintroduce this claim")
    # A5: the three heroes must not be the same silhouette shipped thrice
    hero_hashes = set()
    for name in ("hero_knight.png", "hero_mage.png", "hero_cleric.png"):
        p = ASSETS / "sprites" / name
        if p.exists():
            hero_hashes.add(hashlib.sha256(p.read_bytes()).hexdigest())
    if len(hero_hashes) != 3:
        fail("hero_knight/mage/cleric sheets are not three distinct files")


def main():
    files = all_asset_files()
    runtime = runtime_files()
    check_duplicates(runtime)
    sizes = check_dimensions(runtime)
    check_manifest(files)
    check_code_refs()
    check_ui_finish()
    check_ramps()
    check_openrtp_guardrails(sizes)
    print(f"validate_assets: {len(runtime)} runtime files, {len(files)} total asset files")
    for w in warnings:
        print(f"  WARN  {w}")
    if failures:
        print(f"validate_assets: FAILED ({len(failures)} problem(s))")
        for f in failures:
            print(f"  FAIL  {f}")
        return 1
    print("validate_assets: OK — dimensions, manifest parity, code refs, UI finish, "
          "palette ramps and OpenRTP guardrails all pass")
    return 0


if __name__ == "__main__":
    sys.exit(main())
