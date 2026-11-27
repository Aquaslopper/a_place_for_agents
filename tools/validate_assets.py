#!/usr/bin/env python3
"""Automated Art Bible & Asset Manifest validator for A Place for Agents (A7).

Checks:
1. Strict 16px grid-multiple dimensions and exact category size contracts.
2. Character sheet cell integrity (all 8x4 = 32 cells of 16x24 have visible pixels).
3. Zero duplicate basenames or stems across shipped assets (excluding assets/source/).
4. 100% parity between assets/ on disk, ART_MANIFEST.md, and C++ runtime loaders.
5. Non-empty license and source notes for every asset + presence of CREDITS.md,
   docs/ART_BIBLE.md, assets/fonts/OFL.txt, and assets/audio/SCORE.txt.
6. Bounded pixel-art color budgets, binary {0, 255} alpha (zero blurry soft-alpha),
   and absence of legacy unreferenced files or rounded-UI/non-integer C++ regressions.
"""
from __future__ import annotations

import glob
import os
import re
import sys
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets"
MANIFEST = ROOT / "ART_MANIFEST.md"


def parse_manifest() -> dict[str, dict[str, str]]:
    if not MANIFEST.is_file():
        raise AssertionError("Missing ART_MANIFEST.md at repository root.")
    entries: dict[str, dict[str, str]] = {}
    for line in MANIFEST.read_text(encoding="utf-8").splitlines():
        if not line.startswith("| `assets/"):
            continue
        parts = [p.strip() for p in line.strip("|").split("|")]
        if len(parts) != 6:
            raise AssertionError(f"Malformed manifest row: {line}")
        path = parts[0].strip("`")
        entries[path] = {
            "category": parts[1],
            "dims": parts[2].strip("`"),
            "colors": parts[3],
            "source": parts[4].strip("`"),
            "license": parts[5],
        }
    return entries


def extract_cpp_pngs() -> set[str]:
    core_text = (ROOT / "src" / "core.cpp").read_text(encoding="utf-8")
    refs = set(re.findall(r'"([a-z0-9_/]+\.png)"', core_text))
    # Ignore the resolveAssetsDir() probe string if matched as assets/...
    refs = {r for r in refs if not r.startswith("assets/")}
    for scene in ("meadow", "forest", "amber", "fen", "heath", "frost", "dunes"):
        refs.add(f"backdrops/bg_{scene}.png")
    for i in range(3):
        refs.add(f"tiles/tuft_{i}.png")
        refs.add(f"sprites/tree_{i}.png")
        refs.add(f"sprites/pine_{i}.png")
        refs.add(f"sprites/rock_{i}.png")
    for i in range(2):
        refs.add(f"tiles/flower_{i}.png")
    for i in range(5):
        refs.add(f"sprites/house_{i}.png")
    enemies = [
        "slime", "imp", "bat", "hornet", "wisp", "wolf", "skeleton", "ghost",
        "orc", "serpent", "golem", "night_rider", "slime_king",
        "bandit", "goblin", "reaper", "faerie", "drake", "naga", "shade",
        "dread_knight", "archfiend", "tiger", "porcupine",
    ]
    for e in enemies:
        refs.add(f"sprites/{e}.png")
        refs.add(f"sprites/{e}_battle.png")
    return {f"assets/{r}" for r in refs}


def extract_cpp_audio_and_fonts() -> set[str]:
    core_text = (ROOT / "src" / "core.cpp").read_text(encoding="utf-8")
    music_text = (ROOT / "src" / "music.cpp").read_text(encoding="utf-8")
    refs = set()
    for rel in re.findall(r'"(audio/[a-z0-9_]+\.ogg)"', core_text):
        refs.add(f"assets/{rel}")
    m = re.search(r'files\[\]\s*=\s*\{([^}]+)\}', music_text)
    if not m:
        raise AssertionError("Could not parse music cues from src/music.cpp")
    for cue in re.findall(r'"([a-z0-9_]+)"', m.group(1)):
        refs.add(f"assets/audio/{cue}.ogg")
    for rel in re.findall(r'"(fonts/[^"]+\.ttf)"', core_text):
        refs.add(f"assets/{rel}")
    return refs


def validate() -> None:
    errors: list[str] = []

    # Required provenance & spec documents
    for req in ("docs/ART_BIBLE.md", "CREDITS.md", "ART_MANIFEST.md",
                "assets/fonts/OFL.txt", "assets/audio/SCORE.txt"):
        if not (ROOT / req).is_file():
            errors.append(f"Missing required document: {req}")

    manifest = parse_manifest()

    # Enumerate runtime disk files (excluding offline assets/source/)
    disk_files: list[str] = []
    for p in sorted(ASSETS.rglob("*")):
        if not p.is_file():
            continue
        rel = p.relative_to(ROOT).as_posix()
        if rel.startswith("assets/source/"):
            continue
        disk_files.append(rel)
    disk_set = set(disk_files)
    png_files = [f for f in disk_files if f.endswith(".png")]

    # Check zero duplicate basenames & stems across runtime PNGs
    seen_basenames: dict[str, str] = {}
    seen_stems: dict[str, str] = {}
    for f in png_files:
        base = os.path.basename(f)
        stem = os.path.splitext(base)[0]
        if base in seen_basenames:
            errors.append(f"Duplicate PNG basename '{base}': {seen_basenames[base]} vs {f}")
        seen_basenames[base] = f
        if stem in seen_stems:
            errors.append(f"Duplicate PNG stem '{stem}': {seen_stems[stem]} vs {f}")
        seen_stems[stem] = f

    # Forbidden legacy files
    for legacy in ("assets/title_atlas.png", "assets/ui/night.png",
                   "assets/sprites/face_knight_big.png", "assets/icons/candle.png",
                   "assets/backdrops/castle.png"):
        if (ROOT / legacy).exists():
            errors.append(f"Legacy unreferenced asset still exists: {legacy}")

    # Disk <-> Manifest parity & non-empty license/source
    manifest_set = set(manifest.keys())
    for extra in sorted(disk_set - manifest_set):
        errors.append(f"File on disk missing from ART_MANIFEST.md: {extra}")
    for missing in sorted(manifest_set - disk_set):
        errors.append(f"Entry in ART_MANIFEST.md missing on disk: {missing}")

    for path, meta in manifest.items():
        if not meta["source"] or not meta["license"]:
            errors.append(f"Missing source or license note in ART_MANIFEST.md for {path}")

    # Disk <-> C++ loader parity
    cpp_pngs = extract_cpp_pngs()
    png_set = set(png_files)
    for stray in sorted(png_set - cpp_pngs):
        errors.append(f"Unreferenced PNG not loaded in src/core.cpp: {stray}")
    for missing in sorted(cpp_pngs - png_set):
        errors.append(f"PNG loaded in src/core.cpp missing on disk: {missing}")

    cpp_af = extract_cpp_audio_and_fonts()
    af_disk = {f for f in disk_files if f.endswith((".ogg", ".ttf"))}
    for stray in sorted(af_disk - cpp_af):
        errors.append(f"Unreferenced audio/font asset not loaded in C++: {stray}")
    for missing in sorted(cpp_af - af_disk):
        errors.append(f"Audio/font loaded in C++ missing on disk: {missing}")

    # Image grid, binary alpha, palette bounds, and sheet integrity checks
    for rel in png_files:
        im = Image.open(ROOT / rel).convert("RGBA")
        w, h = im.size
        if w % 16 != 0 or h % 16 != 0:
            errors.append(f"Non-16px grid size {w}x{h} in {rel}")

        if rel in manifest and manifest[rel]["dims"] != f"{w}x{h}":
            errors.append(f"Manifest dimension mismatch for {rel}: "
                          f"{manifest[rel]['dims']} vs {w}x{h}")

        # Category size contracts
        if rel.startswith("assets/icons/") and (w, h) != (16, 16):
            errors.append(f"Icon must be 16x16: {rel} is {w}x{h}")
        elif rel.startswith("assets/sprites/face_") and (w, h) != (32, 32):
            errors.append(f"Portrait must be 32x32: {rel} is {w}x{h}")
        elif rel.startswith(("assets/sprites/hero_", "assets/sprites/npc_")):
            if (w, h) != (128, 96):
                errors.append(f"Character sheet must be 128x96 (16x24, 8x4): {rel} is {w}x{h}")
            else:
                for r in range(4):
                    for c in range(8):
                        cell = im.crop((c * 16, r * 24, (c + 1) * 16, (r + 1) * 24))
                        if cell.getbbox() is None:
                            errors.append(f"Empty character cell ({c},{r}) in {rel}")
        elif rel.endswith("_battle.png") and (w, h) not in ((32, 32), (48, 48), (64, 64)):
            errors.append(f"Battle monster must be 32x32/48x48/64x64: {rel} is {w}x{h}")
        elif rel.startswith("assets/backdrops/") and (w, h) != (512, 192):
            errors.append(f"Backdrop must be 512x192: {rel} is {w}x{h}")
        elif rel in ("assets/title_bg.png", "assets/ui/dawn.png", "assets/ui/gameover_plate.png") and (w, h) != (512, 288):
            errors.append(f"Full-screen plate must be 512x288: {rel} is {w}x{h}")
        elif rel == "assets/ui/window_frame.png" and (w, h) != (32, 32):
            errors.append(f"UI frame must be 32x32: {rel} is {w}x{h}")

        # Binary alpha & color budget check
        pixels = list(im.get_flattened_data() if hasattr(im, "get_flattened_data") else im.getdata())
        alphas = {px[3] for px in pixels}
        bad_alphas = [a for a in alphas if a not in (0, 255)]
        if bad_alphas:
            errors.append(f"Soft/semi-transparent alpha pixels in {rel}: {sorted(bad_alphas)[:5]}")
        opaque_cols = {px[:3] for px in pixels if px[3] > 0}
        if not opaque_cols:
            errors.append(f"Fully transparent image: {rel}")

        max_cols = 20
        if rel == "assets/tiles/landscape.png":
            max_cols = 64
        elif rel == "assets/title_bg.png":
            max_cols = 32
        elif rel.startswith(("assets/icons/", "assets/fx/")):
            max_cols = 12
        if len(opaque_cols) > max_cols:
            errors.append(f"Color budget exceeded in {rel}: {len(opaque_cols)} > {max_cols}")

    # C++ UI & scaling discipline checks
    forbidden_cpp = (
        ("DrawRectangleRounded", "rounded vector UI call"),
        ("drawCharSheet", "legacy 24x32 character sheet drawer"),
        ("FACE_KNIGHT_BIG", "removed portrait enum"),
        ("titleAtlas", "removed titleAtlas field"),
        ("windowBody", "removed windowBody field"),
        ("font32", "removed 32px font field"),
        ("ui/night.png", "removed ui/night backdrop"),
    )
    for cpp in sorted((ROOT / "src").glob("*.[ch]pp")):
        txt = cpp.read_text(encoding="utf-8")
        for pat, desc in forbidden_cpp:
            if pat in txt:
                errors.append(f"Forbidden C++ pattern '{pat}' ({desc}) in {cpp.name}")

    if errors:
        print(f"VALIDATION FAILED ({len(errors)} error(s)):", file=sys.stderr)
        for e in errors:
            print(f"  - {e}", file=sys.stderr)
        sys.exit(1)

    print(
        f"VALIDATION PASSED: {len(png_files)} PNGs, "
        f"{len(af_disk)} audio/font files, {len(manifest)} manifest entries — "
        "100% grid-aligned, unique basenames, binary alpha, palette-bounded, and C++-synced."
    )


if __name__ == "__main__":
    validate()
