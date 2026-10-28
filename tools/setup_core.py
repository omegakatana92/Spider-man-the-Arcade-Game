#!/usr/bin/env python3
"""Applies the Spider-Man changes to the arcade-decomp submodule (tools/core).

The changes are in tools/core-spidman.patch: frame waits on a flag bit, ROM-mirror frame
loops, SCHCU/SKPCU halfwords, Z80 IM 2 and (C) port I/O. Run this once after
`git submodule update --init`, before `tools/recompile.py` or `tools/decompile_sound.py`.
"""
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CORE = ROOT / "tools" / "core"
PATCH = ROOT / "tools" / "core-spidman.patch"


def git(*arguments):
    return subprocess.run(["git", "-C", str(CORE), *arguments], text=True, capture_output=True)


def main():
    if not CORE.exists():
        print("tools/core is missing: run 'git submodule update --init' first", file=sys.stderr)
        return 1
    if not (CORE / ".git").exists():
        print("tools/core is vendored (a source archive); no patch needed")
        return 0
    if not PATCH.exists():
        print(f"{PATCH} is missing", file=sys.stderr)
        return 1
    reverse = git("apply", "--check", "--reverse", str(PATCH))
    if reverse.returncode == 0:
        print("the Spider-Man changes are already applied")
        return 0
    applied = git("apply", "--3way", str(PATCH))
    if applied.returncode != 0:
        print(applied.stderr or applied.stdout, file=sys.stderr)
        print("could not apply the patch; the core may be a different revision", file=sys.stderr)
        return 1
    print("applied tools/core-spidman.patch")
    return 0


if __name__ == "__main__":
    sys.exit(main())
