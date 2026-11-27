#!/usr/bin/env python3
"""Unified 16-bit SNES pixel-art pipeline for A Place for Agents.

Generates all runtime terrain tiles, elevation reliefs, village/dungeon props,
buildings, landmarks, 128x96 character sheets, 32x32 portraits, native 1x
field and battle monsters, 16x16 icons, battle FX, 512x192 battle backdrops,
512x288 scenic plates, and the canonical 32x32 navy-and-gold 9-slice window.

Usage:
    python3 tools/build_art.py
"""
from __future__ import annotations

import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))

from snes import ASSETS  # noqa: E402
from landscape_art import generate_landscape  # noqa: E402
from art_terrain import generate_terrain_and_ui  # noqa: E402
from village_art import generate_village_structures  # noqa: E402
from art_objects import generate_objects  # noqa: E402
from art_actors import generate_actors  # noqa: E402
from art_misc import generate_monsters, generate_icons, generate_fx  # noqa: E402
from battle_scenery import generate_all_scenery  # noqa: E402


def _clean_runtime_pngs() -> int:
    """Remove all non-source runtime PNGs so zero stale/legacy OpenRTP files remain."""
    removed = 0
    for p in list(ASSETS.rglob("*.png")):
        if "source" in p.relative_to(ASSETS).parts:
            continue
        p.unlink()
        removed += 1
    return removed


def main() -> None:
    removed = _clean_runtime_pngs()
    generate_landscape()
    generate_terrain_and_ui()
    generate_village_structures()
    generate_objects()
    generate_actors()
    generate_monsters()
    generate_icons()
    generate_fx()
    generate_all_scenery()

    count = sum(
        1 for p in ASSETS.rglob("*.png")
        if "source" not in p.relative_to(ASSETS).parts
    )
    print(f"build_art: cleaned {removed} old PNGs, generated {count} coherent 16-bit PNGs.")


if __name__ == "__main__":
    main()
