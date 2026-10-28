# Build status

Last updated: 2026-10-28 (Windows x64, MinGW-w64 GCC 16.2, MSYS2)

## Verified working

### 1. ROM verification

```text
python tools/verify_rom.py C:\Users\omega\Documents\system 32\spidman.zip
...
20/20 files of the 'spidman' set
```

Every file matched by CRC32 **and** SHA-1. MAME set `spidman` (World), System 32,
4-player PCB, no protection.

### 2. Disassembly / analysis

```text
python tools/disassemble.py C:\Users\omega\Documents\system 32\spidman.zip
instructions:        5836
functions:           258
code labels:         836
invalid paths:       0
overlapping paths:   0
```

After adding the trace of the coin/start path, the translation covers **6,273
distinct instructions** in **718 functions** (`tools/recompile.py` report:
`instructions: 16442 written (6273 distinct)`, `not translated: 0`).

### 3. Native build (Windows x64)

```text
cmake -S . -B build/cmake -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/cmake
BUILD_EXIT=0
```

Artifacts:

| File | Size | Notes |
|---|---:|---|
| `build/bin/spidman.exe` | 1,461,198 bytes | SDL2 window port, with the reference sound fallback |
| `build/bin/SDL2.dll` | 2,412,441 bytes | copied beside the executable |
| `build/bin/spidman_headless.exe` | 838,220 bytes | deterministic test runner |
| `build/bin/s32ref.exe` | 1,248,792 bytes | MAME V60 reference |
| `build/bin/oracle.exe`, `oracle_z80.exe` | 196,237 / 124,886 bytes | disassembler oracles |

The windowed executable was started and stayed running (window created, ROM loaded,
frames drawn). Automated picture comparison is done through the headless build, which
shares the same board, video and program code.

### 4. Frame-for-frame comparison with MAME

**Attract mode, 1,500 frames (25 s):**

```text
state frames 1500 1500   first diff: none
video frames 1500 1500   first diff: none
```

**Coin + start + movement + attack, 900 frames:**

```text
state frames 901 901   first diff: none
video frames 901 901   first diff: none
```

The native build and `s32ref` (the original V60 program on MAME's core) produce the
same work-RAM/video-RAM/sprite-RAM/palette/mixer hash and the same drawn picture on
every frame. Screenshots from these runs are in `docs/screenshots/` (legal screen,
title, gameplay).

### 5. Board model

- 4-player 8255 PPI at `0xC00060` implemented and mapped to players 3-4 and starts 3-4.
- 315-5296 I/O, 93C46 EEPROM, interrupt controller, timers, palette/mixer, video.
- 20 ROM files laid out as MAME's `ROM_START(spidman)`.
- 16 unmapped writes at `0xE00000`, the same 16 MAME leaves unmapped in the reference.
- No protection code (the set is unprotected).

### 6. SDL2 port

- Four-player keyboard and controller input.
- Full-screen/windowed, smooth/sharp pixels, volume.
- EEPROM and settings persistence.
- ROM loaded from `SPIDERMAN_ROM`, beside the executable, or `rom/spidman.zip`.
- Steam Deck: built-in controller is player 1; AppImage script provided.

## Not working / incomplete

### Sound

**Status: working, via a documented reference fallback (not native).**

The original Z80 program runs on MAME's Z80 core, with MAME's RF5C68 and ymfm's
YM3438, through `tools/reference/sound/s32sound.cpp` and the C wrapper
`runtime/src/sound_reference.cpp`. The SDL2 port calls it and mixes its samples at
48 kHz. This is the same sound board `s32ref` uses, so the samples are the reference
samples.

Verified in the windowed build with SDL's dummy drivers and the coin/start script:

```text
sound: frame 60,  932 samples at 55930 Hz, peak 0
sound: frame 120, 932 samples at 55930 Hz, peak 0
sound: frame 180, 932 samples at 55930 Hz, peak 383
sound: frame 240, 932 samples at 55930 Hz, peak 863
sound: frame 300, 932 samples at 55930 Hz, peak 748
sound: frame 360, 933 samples at 55930 Hz, peak 1631
sound: frame 420, 932 samples at 55930 Hz, peak 3897
```

Attract mode is silent (peak 0); gameplay is not. The reference `s32ref --sound-wav`
produces the same non-silent stream (peak 7340 over 900 frames).

**Native sound is still incomplete.** The Z80 program is decoded (`z80/`), the main
loop is at `0x0075`, the command dispatch tables at `0x101C`, `0x03F4`,
`0x0836`/`0x0936`, and the chips are modelled (`runtime/src/rf5c68.c`,
`ym3438.c`, `sound_chips.c`). `tools/decompile_sound.py` currently stops at:

```text
decompiler.stack.StackError: sound_03BF: returns with SP -2 from where it started
```

`sound_03BF` is a command dispatcher that does `push bc` (a continuation at `0x03EB`)
and then `jp (hl)` through a 32-entry table; each target returns to `0x03EB`, which
continues the dispatcher. The decompiler treats `jp (hl)` as a switch inside the
function and does not model the pushed continuation, so the stack balance fails. A
fix needs either (a) a Z80 call-table model for this `push`/`jp (hl)` pattern, or
(b) a Z80 instruction-level scaffold like the V60 one. Until then, the emulated
fallback is used and is clearly marked in the UI documentation and this file.

### High-level decompilation

`tools/decompile.py` (readable C, no CPU left) gets through analysis and most
functions but stops on a computed jump the current `symbols.txt` does not describe:

```text
ValueError: block end ComputedJump(address=Reg(index=15, bits=32))
```

The instruction-level translation (`tools/recompile.py`) is complete and is the
functional build. Naming the remaining jump tables and structures in `symbols.txt`
is the next step for readable C; it is a readability task, not a gameplay blocker.

### Linux / AppImage

`bash build_appimage.sh` was run in **WSL2 Ubuntu 22.04 x86_64** (GCC 11.4, SDL2
2.0.20). It built `dist/SpiderMan-SteamDeck-x86_64.AppImage` (1,365,184 bytes),
unpacked it, found the executable and desktop entry, and launched it with the ROM
set and SDL's dummy drivers:

```text
=== Verifying ===
  unpacked, executable and desktop entry present
  launched with the ROM set (status 124; 124 = still running)
```

A second run with `--script tests/controls/coin_start.txt` produced the same
non-silent sound trace as Windows (peak 383 at frame 180, 863 at 240, 1631 at 360).
The AppImage was **not** run on physical Steam Deck hardware (none available here);
it was tested on x86_64 Linux with the same libraries SteamOS provides. A Windows
`.exe` is never repackaged as the AppImage.

### MSVC

CMake supports MSVC, but the verified build here is MinGW-w64. MSVC was not tested.

## Release artifacts (built here)

| Artifact | Size | Verified |
|---|---:|---|
| `dist/SpiderMan-Windows-x64.zip` | 1,464,903 bytes | built, extracted to a clean folder, launched with the ROM; ROM-free |
| `dist/SpiderMan-SteamDeck-x86_64.AppImage` | 1,361,088 bytes | built in WSL2 Ubuntu 22.04, unpacked, launched with the ROM (status 124 = running), sound peak verified |
| `dist/SpiderMan-Source.zip` | 1,065,421 bytes | 257 files; no ROM, build products or private artifacts |

None of these files is committed or uploaded. Publishing needs explicit authorization.

## Reproduce

```powershell
# Windows
.\build_windows.ps1

# tests (ROM in rom/)
python tests\run_tests.py
```

Expected test tail:

```text
ROM: 20/20
attract: 1500/1500 state, 1500/1500 video identical
coin_start: 901/901 state, 901/901 video identical
PASS
```





