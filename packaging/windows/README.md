# Windows packaging

`build_windows.ps1` in the repository root builds the executable and writes
`dist/SpiderMan-Windows-x64.zip`. The package contains:

- `spidman.exe`
- `SDL2.dll`
- `README.md`, `LICENSE`, `BUILD_STATUS.md`
- `docs/CONTROLS.md`, `docs/ROM_ANALYSIS.md`
- `READ-ME-FIRST.txt` (how to supply your own `spidman.zip`)

It contains no ROM, no extracted graphics/audio and no build products other than the
executables. The script verifies the ROM, translates the program, builds, runs the
frame comparisons and then packages.
