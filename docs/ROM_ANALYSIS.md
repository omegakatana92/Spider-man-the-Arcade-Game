# ROM Analysis — Spider-Man: The Videogame

## Identification

| | |
|---|---|
| Title | **Spider-Man: The Videogame** |
| Manufacturer | Sega (licensed from Marvel Entertainment Group) |
| Release | 1991 |
| MAME set | `spidman` — *Spider-Man: The Videogame (World)* |
| MAME driver | `src/mame/sega/segas32.cpp`, `GAME(1991, spidman, 0, sega_system32_4p, spidman, segas32_new_state, init_spidman, ROT0, "Sega", ...)` |
| Board | **Sega System 32**, 4-player PCB (`sega_system32_4p`, ROM board 834-8332-02 / 837-7429-01) |
| Protection | **None** — `init_spidman()` only calls `segas32_common_init()`; the set is marked "not protected" in MAME |
| Parent / BIOS | None. `spidman` is a parent set; it needs no BIOS, samples, CHD or other ROM set |
| Players | 4, simultaneous |
| Orientation | Horizontal (ROT0) |

The supplied archive `spidman.zip` contains exactly the 20 files of MAME's `spidman`
set. Every file was matched by CRC32 and SHA-1 (`tools/verify_rom.py`): **20/20**.

## Hardware

### Main CPU

- **NEC V60** (uPD70616), 16.10795 MHz (32.2159 MHz crystal / 2), 32-bit, little-endian,
  24-bit external address bus.
- 2 MB program ROM at `0x000000-0x1FFFFF`, with the first 1 MB mirrored at `0xF00000-0xFFFFFF`.
- V60 reset vector at `0xFFFFFFF0` (canonical `0x09FFF0`): `jmp 0x060006`.
- Interrupt vector table at `0x09F000` (SBR = `0x09F000`): 256 32-bit entries. The board's
  interrupt controller supplies vectors 0x40-0x44 (VBLANK start/end, sound, timer 0/1).

### Sound CPU and chips

- **Zilog Z80**, 8.053975 MHz (32.2159 MHz / 4).
- 1 MB sound program ROM visible as 40 KB at `0x0000-0x9FFF` plus an 8 KB bank at
  `0xA000-0xBFFF`; the remaining 3 MB are PCM sample banks selected by ports `0xA0`/`0xB0`.
- **RF5C68** (Sega 315-5476A / ASSP 5C105) PCM chip at `0xC000-0xDFFF`, 64 KB wave RAM,
  12.5 MHz (50 MHz / 4).
- **Two YM3438** FM chips at Z80 ports `0x80-0x83` and `0x90-0x93`, 8.053975 MHz.
- 8 KB shared RAM at Z80 `0xE000-0xFFFF`, seen by the V60 at `0x700000`.
- Sound interrupt controller at Z80 ports `0xC0-0xD3`; the V60 requests interrupts through
  the main interrupt controller at `0xD0000C-0xD0000F`.

### Graphics

- Sega System 32 video: four tilemap layers (NBG0-NBG3), 16x16 tiles at 4 bits per pixel,
  row scroll, row select, zoom, clip windows.
- 4 MB tile ROM (`mainpcb:tiles`): four byte lanes of 32-bit words, 32,768 tiles.
- 8 MB sprite ROM (`mainpcb:sprites`, big-endian region): eight byte lanes of 64-bit groups.
- 128 KB video RAM at `0x300000`, 128 KB sprite RAM at `0x400000`, sprite control at
  `0x500000`, 32K palette entries at `0x600000`, mixer registers at `0x610000`.
- Text layer at `0x31F000` (64 x 32 cells), backdrop and bitmap layer as in MAME's
  `segas32_v.cpp`.
- Native picture: **320 x 224** (the game selects 320-pixel mode through `0x31FF00`;
  MAME's video also supports 416 x 224 for other System 32 games).

### Input

- Sega 315-5296 I/O chip at `0xC00000` (players 1-2, service/test, coins 1-2,
  starts 1-2, EEPROM).
- **8255 PPI** at `0xC00060` on the 4-player board: port A = player 3, port B = player 4,
  port C = start 3 / start 4.
- 93C46 serial EEPROM (16-bit organization) for settings and bookkeeping.
- No trackballs are used by this game (the uPD4701 inputs are present on the board but
  unpopulated/unused).

## Memory map (V60, 24-bit bus)

| Address | Size | Device |
|---|---|---|
| `0x000000-0x1FFFFF` | 2 MB | program ROM |
| `0x200000-0x20FFFF` | 64 KB | work RAM (mirrored every 64 KB to `0x2FFFFF`) |
| `0x300000-0x31FFFF` | 128 KB | video RAM (mirrored) |
| `0x400000-0x41FFFF` | 128 KB | sprite RAM (mirrored) |
| `0x500000-0x50000F` | 16 B | sprite control (byte-wide) |
| `0x600000-0x60FFFF` | 64 KB | palette RAM |
| `0x610000-0x61007F` | 128 B | mixer registers |
| `0x700000-0x701FFF` | 8 KB | sound shared RAM (mirrored) |
| `0x800000-0x801002` | — | Sega 32 communication device (not used by this set) |
| `0xC00000-0xC0001F` | 32 B | 315-5296 I/O |
| `0xC00040-0xC0007F` | 64 B | I/O expansion; `0xC00060-0xC00067` = 8255 PPI |
| `0xD00000-0xD0000F` | 16 B | interrupt controller |
| `0xD80000-0xDFFFFF` | 512 KB | random-number generator |
| `0xF00000-0xFFFFFF` | 1 MB | mirror of program ROM `0x000000-0x0FFFFF` |

The V60 boot code writes 16 bytes to `0xE00000`; MAME's System 32 map does not decode
that address either, so the writes are unmapped on the reference and on this model.
They are logged (16 writes) and do not affect the compared state or picture.

## ROM files

All files are 16-bit or 8-bit mask ROMs. `size` is the file size in bytes.

| MAME name | Size | CRC32 | SHA-1 | Region |
|---|---:|---|---|---|
| `epr-14307.ic13` | 0x020000 | `d900219c` | `d59654db1fc0ec4d5f8cda9000ab4bd3bb36cdfc` | main program |
| `epr-14306.ic7` | 0x020000 | `64379dc6` | `7efc7175351186c54f141161a395e63b1cc7e7a5` | main program |
| `epr-14281.ic14` | 0x020000 | `8a746c42` | `fa3729ec3aa4b3c59322408146ce2cfbf5a11b98` | main program (even) |
| `epr-14280.ic6` | 0x020000 | `3c8148f7` | `072b7982bb95e7a9ab77844b59020146c262488d` | main program (odd) |
| `epr-14285.ic35` | 0x040000 | `25aefad6` | `10153f4e773a0f55378f869eb1d85156e85f893f` | Z80 program |
| `mpr-14284.ic31` | 0x080000 | `760542d4` | `dcac73869c02fefd328bd6bdbcbdb3b68b0647da` | PCM bank 0 |
| `mpr-14283.ic26` | 0x080000 | `c863a91c` | `afdc76bbb9b207cfcb47d437248a757d03212f4e` | PCM bank 1 |
| `mpr-14282.ic22` | 0x080000 | `ea20979e` | `9b70ef055da8c7c56da54b7edef2379678e7c50f` | PCM bank 2 |
| `mpr-14291-s.ic38` | 0x100000 | `490f95a1` | `f220788670b76164ac414ed9b16a422f719be267` | tiles, lane 0 |
| `mpr-14289-s.ic29` | 0x100000 | `38570582` | `a9d810a02a1f5a6849c79d65fbebff21a4b82b59` | tiles, lane 1 |
| `mpr-14290-s.ic34` | 0x100000 | `a144162d` | `d43f12dd9f690cdfcebb6c7b515ff7dc7dcaa377` | tiles, lane 2 |
| `mpr-14288-s.ic25` | 0x100000 | `3188b636` | `bc0adeeca5040caa563ee1e0eded9c323ca23446` | tiles, lane 3 |
| `mpr-14299-h.ic36` | 0x100000 | `ce59231b` | `bcb1f11b74935694d0617ec8df66db2cc57b6219` | sprites, lane 0 |
| `mpr-14298-h.ic32` | 0x100000 | `2745c84c` | `5a0528c921cba7a1047d3a2ece79925103d719a1` | sprites, lane 1 |
| `mpr-14297-h.ic27` | 0x100000 | `29cb9450` | `7dc38d23a2f0cee2f4edde05c1a6f0dc83f331db` | sprites, lane 2 |
| `mpr-14296-h.ic23` | 0x100000 | `9d8cbd31` | `55a9f9ec9029157da033e69664b58e694a28db47` | sprites, lane 3 |
| `mpr-14295-h.ic37` | 0x100000 | `29591f50` | `1ac4ceaf74892e30f210ad77024eb441c5e4a959` | sprites, lane 4 |
| `mpr-14294-h.ic33` | 0x100000 | `fa86b794` | `7b6907e5734fbf2fba7bcc213a8551fec5e9f3d5` | sprites, lane 5 |
| `mpr-14293-s.ic28` | 0x100000 | `52899269` | `ff809ff88701109e0ca79e785a61402d97335cec` | sprites, lane 6 |
| `mpr-14292-s.ic24` | 0x100000 | `41f71443` | `351d40d6159cb5b792519bce5d16490965800cfb` | sprites, lane 7 |

### Region construction

- **Program (2 MB):** `epr-14307` x4 at `0x000000`; `epr-14306` x4 at `0x080000`;
  `epr-14281`/`epr-14280` interleaved 16-bit at `0x100000`, mirrored at `0x180000`.
- **Tiles (4 MB):** 32-bit words from four byte lanes
  (`14291`, `14289`, `14290`, `14288` at byte offsets 0, 1, 2, 3).
- **Sprites (8 MB):** 64-bit groups from eight byte lanes
  (`14299`, `14298`, `14297`, `14296`, `14295`, `14294`, `14293`, `14292`).
- **Sound (4 MB):** `epr-14285` x4 at `0x000000`; each PCM bank (`mpr-14284`, `-14283`,
  `-14282`) x2 at `0x100000`, `0x200000`, `0x300000`.

The exact layouts are implemented in `tools/spidman_rom.py` (analysis) and
`runtime/src/rom_set.c` (runtime), after `ROM_START(spidman)` in MAME's
`src/mame/sega/segas32.cpp` (BSD-3-Clause).

## Compatibility assessment

| Component | Toolkit support | Status |
|---|---|---|
| NEC V60 CPU | `v60/` decoder, disassembler, lift, C translation | **Supported**; 6,273 distinct instructions decoded, 718 functions |
| Sega System 32 board | `system32/` reference and runtime | **Supported**; reused from `segasonic-decomp` (System 32) and adapted to the 4-player board |
| 315-5296 I/O | runtime | **Supported** |
| 8255 PPI (players 3-4) | added in this project | **Implemented** |
| RF5C68 + YM3438 sound | runtime chips; Z80 tooling | **Chips supported**; Z80 sound program decompilation is incomplete (see below) |
| System 32 video | runtime video library | **Supported**; frame-for-frame match with MAME |
| Protection | none | **Not needed** |

### Missing / incomplete components

1. **High-level decompilation of the V60 program** is not complete. The static translation
   (`tools/recompile.py`) covers the whole executed program and is the functional build.
   `tools/decompile.py` stops on an unidentified computed jump in the game's code because
   the project's `symbols.txt` does not yet describe that table. This is a readability
   limitation, not a functional one.
2. **Native sound program.** The Z80 driver is decoded and its tables are mapped, but
   `tools/decompile_sound.py` stops on a `push`/`jp (hl)` command dispatch whose stack
   balance the decompiler does not yet model. The game uses a documented fallback: the
   original Z80 program on MAME's Z80 core with MAME's RF5C68 and ymfm's YM3438
   (BSD-3-Clause). This sound CPU is emulated, not native; the main V60 program is
   native. The reference tool `s32ref` runs the same sound board and produces sample
   hashes, so the missing piece is isolated.
3. The V60 boot writes to `0xE00000`, which MAME also leaves unmapped. The writes are
   reproduced as unmapped writes and do not affect state or video.

## How the ROM is used

No ROM data is committed to Git. The runtime reads the player's own `spidman.zip`
(or the 20 files in `rom/`) at start-up, verifies each file by CRC32, and lays the
regions out as above. The generated native program contains only translated code
addresses and constants; it carries no copyrighted graphics, samples or program bytes.
