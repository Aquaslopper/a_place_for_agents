#!/usr/bin/env python3
"""link_check.py — prove the game can be linked, without a GPU.

For ten PRs `main` failed at the CI Build step while every art pass reported
success, because a dozen field functions (sampleGround, propAt, ...) and a
global (gChestsInView) were called from six translation units and defined in
none. Compiling each file with -fsyntax-only cannot see that: only the linker
can. This script runs the linker's half of the job on a machine with no GL, no
X11 and no display:

  1. compile every src/*.cpp to an object file;
  2. collect undefined symbols and the symbols the objects define;
  3. stub whatever is still undefined *and is not ours* (raylib, libc);
  4. link a throwaway binary and report any remaining undefined project symbol.

Needs raylib's headers, not its library. Point at them with --raylib-include, or
let the script fetch the pinned tag from GitHub.

    python3 tools/link_check.py --raylib-include /usr/local/include
    python3 tools/link_check.py            # fetches raylib 5.5 headers

Exit code 0 means every symbol this repository declares is also defined.
"""
from __future__ import annotations

import argparse
import os
import pathlib
import subprocess
import sys
import re
import tarfile
import tempfile
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parents[1]
SRC = ROOT / "src"
RAYLIB_TAG = "5.5"
RAYLIB_URL = f"https://codeload.github.com/raysan5/raylib/tar.gz/refs/tags/{RAYLIB_TAG}"

# Symbols the C library and libstdc++ supply at link time; anything else that is
# still undefined is either raylib (stubbed) or ours (an error).
LIBC_PREFIXES = ("__", "_Unwind", "std", "operator", "_Z", "mem", "str", "snprintf",
                 "vsnprintf", "pow", "sin", "cos", "tan", "sqrt", "floor", "round",
                 "ceil", "fmod", "exp", "log", "abs", "atan", "hypot", "malloc",
                 "calloc", "realloc", "free", "time", "getenv", "qsort", "abort")


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, **kw)


def get_raylib_headers(explicit: str | None) -> pathlib.Path | None:
    if explicit:
        return pathlib.Path(explicit)
    cache = pathlib.Path(tempfile.gettempdir()) / f"raylib-{RAYLIB_TAG}"
    if (cache / "raylib.h").exists():
        return cache
    try:
        with tempfile.TemporaryDirectory() as tmp:
            tgz = os.path.join(tmp, "r.tgz")
            urllib.request.urlretrieve(RAYLIB_URL, tgz)
            with tarfile.open(tgz) as tf:
                tf.extractall(tmp)
            (cache.parent).mkdir(parents=True, exist_ok=True)
            os.replace(os.path.join(tmp, f"raylib-{RAYLIB_TAG}", "src"), cache)
        return cache
    except Exception as exc:  # offline: still worth checking our own symbols
        print(f"link_check: cannot fetch raylib headers ({exc}); compiling without")
        return None


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--raylib-include", default=os.environ.get("RAYLIB_INCLUDE"))
    ap.add_argument("--keep", action="store_true", help="keep the object directory")
    args = ap.parse_args()

    inc = get_raylib_headers(args.raylib_include)
    objdir = pathlib.Path(tempfile.mkdtemp(prefix="linkcheck-"))
    objs, failed = [], False
    for cpp in sorted(SRC.glob("*.cpp")):
        o = objdir / (cpp.stem + ".o")
        cmd = ["g++", "-std=c++17", "-c", "-O0", f"-I{SRC}"]
        if inc:
            cmd.append(f"-I{inc}")
        r = run(cmd + [str(cpp), "-o", str(o)])
        if r.returncode != 0:
            failed = True
            print(f"COMPILE FAIL {cpp.name}")
            print("\n".join(r.stderr.splitlines()[:12]))
        else:
            objs.append(str(o))
    if failed:
        print("link_check: FAILED (does not compile)")
        return 1

    undef, defined = set(), set()
    for o in objs:
        for line in run(["nm", o]).stdout.splitlines():
            parts = line.split()
            if len(parts) == 2:          # undefined symbols have no address
                kind, sym = parts
            elif len(parts) == 3:
                kind, sym = parts[1], parts[2]
            else:
                continue
            if kind == "U":
                undef.add(sym)
            elif kind in "TDBRWV":
                defined.add(sym)

    remaining = sorted(undef - defined)
    # Anything that survived the link attempt *and* looks like ours is a bug;
    # everything else is supplied by raylib/libc, so stub it and try the link.
    # Stub exactly the functions raylib's own headers declare. Everything else
    # that is still undefined is either the C library (which the linker will
    # resolve) or a symbol this repository declares and never defines — which is
    # precisely the bug this tool exists to catch.
    api = set()
    if inc and (inc / "raylib.h").exists():
        api = set(re.findall(r"RLAPI\s+[\w\s\*]+?\b(\w+)\s*\(", (inc / "raylib.h").read_text()))
        for extra in ("rlgl.h", "raymath.h"):
            if (inc / extra).exists():
                api |= set(re.findall(r"RLAPI\s+[\w\s\*]+?\b(\w+)\s*\(", (inc / extra).read_text()))
    stubs = [s for s in remaining if s in api]
    asm = objdir / "stubs.s"
    with asm.open("w") as f:
        f.write(".text\n")
        for s in stubs:
            f.write(f".globl {s}\n.type {s},@function\n{s}:\n\tret\n")
        f.write(".section .note.GNU-stack,\"\",@progbits\n")
    run(["gcc", "-c", str(asm), "-o", str(objdir / "stubs.o")])
    link = run(["g++", "-o", str(objdir / "game")] + objs + [str(objdir / "stubs.o"), "-lm"])
    unresolved = sorted({line.split("`")[1].split("'")[0]
                         for line in link.stderr.splitlines()
                         if "undefined reference" in line and "`" in line})

    ok = link.returncode == 0 and not unresolved
    if ok:
        print(f"link_check: OK — {len(objs)} objects, "
              f"{len(stubs)} external (raylib) symbols stubbed, binary links")
    else:
        print("link_check: FAILED")
        if not unresolved:
            print("\n".join(link.stderr.splitlines()[:12]))
        else:
            print("undefined references:")
        for u in unresolved:
            print("   ", u)
    if not args.keep:
        for p in objdir.glob("*"):
            p.unlink()
        objdir.rmdir()
    else:
        print("objects kept in", objdir)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
