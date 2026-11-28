#!/usr/bin/env python3
"""Build and run the standalone C++ world-map acceptance verifier.

The generator and verifier use only the C++ standard library; this wrapper
keeps the verifier easy to run without configuring the raylib game target.
"""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import shlex
import subprocess
import tempfile


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--seeds", type=int, default=10, help="determinism seeds (default: 10)")
    parser.add_argument("--radius", type=int, default=3, help="atlas cell radius for link checks (default: 3)")
    parser.add_argument("--outward", type=int, default=200, help="cells to generate eastward (default: 200)")
    parser.add_argument("--cxx", default=os.environ.get("CXX", "c++"),
                        help="C++17 compiler command (default: CXX or c++)")
    args = parser.parse_args()

    if min(args.seeds, args.radius, args.outward) < 1:
        parser.error("--seeds, --radius and --outward must all be positive")

    compiler = shlex.split(args.cxx)
    if not compiler:
        parser.error("--cxx must name a compiler")

    with tempfile.TemporaryDirectory(prefix="apfa-world-verify-") as temp:
        binary = Path(temp) / "verify_world"
        command = compiler + [
            "-std=c++17", "-O2", "-Wall", "-Wextra", "-Wpedantic",
            f"-I{root / 'src'}",
            str(root / "src" / "map.cpp"),
            str(root / "tools" / "verify_world.cpp"),
            "-o", str(binary),
        ]
        print("Building headless map verifier:", " ".join(shlex.quote(part) for part in command))
        subprocess.run(command, cwd=root, check=True)
        run = [str(binary), "--seeds", str(args.seeds), "--radius", str(args.radius),
               "--outward", str(args.outward)]
        return subprocess.run(run, cwd=root, check=False).returncode


if __name__ == "__main__":
    raise SystemExit(main())
