"""
Name:        recompile.py
Description: runs the shared core's recompile.py (tools/core) for this game: the translation scaffold,
             the program translated into C an instruction at a time
Version:     0.1.0
Created:     25-10-2026
Modified:    28-10-2026 (uses the recorded attract/gameplay traces by default)
Author:      Sebastian Toyos; adapted for Spider-Man by the Spider-Man recompilation project

The script is the core's (arcade-decomp, which tools/core holds); tools/game_profile.py tells it about
this game. Its usage is the core's. If no --trace is given, the recorded executed-address traces in
tests/traces/ are used: they add the code reached only through computed jumps (the coin/start path).
The traces contain addresses, not ROM bytes.
"""
import runpy
import sys
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
ROOT = TOOLS.parent
sys.path[:0] = [str(TOOLS / "core"), str(TOOLS)]

arguments = sys.argv[1:]
if "--trace" not in arguments:
    traces = [ROOT / "tests" / "traces" / "attract.txt", ROOT / "tests" / "traces" / "coin_start.txt"]
    calls = [ROOT / "tests" / "traces" / "attract_calls.txt", ROOT / "tests" / "traces" / "coin_start_calls.txt"]
    if all(path.exists() for path in traces + calls):
        sys.argv += ["--trace", *map(str, traces), "--calls", *map(str, calls)]

runpy.run_path(str(TOOLS / "core" / "recompile.py"), run_name="__main__")
