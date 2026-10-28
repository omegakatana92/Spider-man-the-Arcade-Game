# Architecture

## Layers

```text
spidman.exe / spidman_headless
        |
        +-- ports/sdl2 or ports/headless        window/input/audio, or deterministic hashes
        |
        +-- build/generated/program_*.c         the V60 program, translated to C
        |       |
        |       +-- runtime/src/v60.c           multi-instruction helpers, interrupts, tasks
        |
        +-- runtime/src/machine.c, io.c, video.c, sound*.c
                |
                +-- runtime/src/rom_set.c        the player's ROM set -> board memories
```

## The translated program

`tools/recompile.py` splits the V60 program into functions and writes one C function
per original function. Each original instruction becomes one or a few C statements.
The V60 registers are local variables of the C function; the flags are local `z`,
`s`, `ov`, `cy` variables. Calls and returns use the V60's own stack, with
`v60_call`, `v60_jump`, `v60_returned_to` and `v60_unknown_jump` handling computed
targets and the game's unusual return paths.

This is a **static translation**, not an interpreter: there is no instruction fetch,
decode loop, or CPU state machine at run time. The host compiler turns the generated
C into native x86-64. The generated program is large (about 1.2 MB of C) but builds
to a small executable.

The frame wait is at `0x0603EF` (a ROM mirror of `0x03EF`): the game polls bit 0 of
work RAM `0x208010`, which the VBLANK handler sets. The translation calls
`v60_frame_wait()` there, which advances the board to its next event (VBLANK start,
VBLANK end, sprite list) and lets the CPU take the interrupt. At `0x0603F6` it calls
`v60_frame_end()`, which reports the frame to the port.

## The board

`runtime/src/machine.c` implements the V60's 24-bit bus:

- program ROM and its `0xF00000` mirror,
- 64 KB work RAM and its mirrors,
- video RAM, sprite RAM, sprite control,
- palette RAM (including the alternate format and blending),
- mixer registers,
- shared RAM,
- interrupt controller and timers,
- random-number generator,
- 315-5296 I/O and the 8255 PPI (in `io.c`).

`runtime/src/video.c` draws the four tilemaps, text/bitmap layers, backdrop, both
sprite buffers and the mixer into a 320/416 x 224 picture. It matches MAME's
`segas32_v.cpp` pixel for pixel in the tested frames.

`runtime/src/sound.c` and `sound_chips.c` are the Z80's side of the board: memory,
bank, shared RAM, ports, and the RF5C68/YM3438 chips. The windowed game currently
uses `runtime/src/sound_reference.cpp` around `tools/reference/sound/s32sound.cpp`:
the original Z80 program on MAME's Z80 core with the reference chips. This is a
documented emulated fallback; the native Z80 translation is incomplete. The main V60
program is native.

## The reference

`tools/reference/s32ref.cpp` runs the **original** V60 program on MAME's V60 core
against the same board model. It can:

- run in frame timing (same events as the game) or cycle timing (MAME's pace),
- write a hash of the board state after every frame,
- draw every frame with MAME's System 32 video and hash it,
- run the sound board on MAME's Z80, RF5C68 and ymfm's YM3438 and hash the samples,
- write executed-address traces, call traces and transfer traces.

The traces are fed back into `tools/recompile.py --trace` to add code reached only
through computed jumps. This is how the coin/start path was completed without
guessing.

## Data flow at start-up

1. `game_rom_set_load()` reads the player's `.zip` or folder, checks each file by
   CRC32, and builds the four regions (`program`, `tiles`, `sprites`, `sound`).
2. `s32_init()` clears the board, sets power-on values, loads the EEPROM file if
   present, and resets the interrupt controller.
3. `program_reset()` clears the V60 state; `program_run()` starts the translated
   program at its reset vector.
4. Each frame: VBLANK start -> the port draws the picture and reads controls; the
   program runs; the frame wait -> the board advances; `program_frame_done` lets the
   port present the frame, pace it, save the EEPROM periodically, and stop on quit.

## Adding a new path

If the game reports `No translated code starts at XXXXXX`, the path was reached
through a computed jump that the static analysis did not know:

1. Run `s32ref` with `--trace build/executed.txt --calls build/called.txt` on a
   control script that reaches the path.
2. Run `python tools/recompile.py --trace build/executed.txt --calls build/called.txt`.
3. Rebuild. The new addresses become functions and the call sites get targets.

Do not add a dummy function or suppress the message: the translated program must
reach real code, and the frame comparison is the check.
