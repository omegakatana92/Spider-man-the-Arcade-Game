# `src/` — the game's C

`tools/decompile.py` writes the high-level C here when it completes. It currently
stops on an unidentified computed jump, so this directory is empty in the functional
build: the game runs the instruction-level translation in `build/generated/`, written
by `tools/recompile.py` (not committed; generated from the player's ROM).

See `BUILD_STATUS.md` and `docs/ARCHITECTURE.md`.
