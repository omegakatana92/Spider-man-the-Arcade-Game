"""
Name:        game_profile.py
Description: what the shared decompiler core (tools/core) needs to know of Spider-Man: The Videogame:
             where the game is, its ROM set, and its settings
Version:     0.1.0
Created:     28-10-2026
Author:      Spider-Man recompilation project

The core (arcade-decomp: the decompiler, the V60 and Z80 frontends, the analysis, the checks) is the same
for every game; this module gives it Spider-Man's: the repository's root, the ROM set's module
(spidman_rom: the regions as the board has them), the names the generated files carry, and the
decompiler's settings for the V60 program, which it puts in the core's modules when it is imported.

Spider-Man: The Videogame (Sega, 1991) runs on Sega System 32, 4-player PCB (sega_system32_4p):
NEC V60 main CPU, Z80 + YM3438 + RF5C68 sound, 315-5296 I/O and an 8255 PPI for players 3-4.
The set is not protected. Settings marked TODO are still being established from the disassembly.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "core"))

import spidman_rom as rom  # noqa: E402
from decompiler import constants
from decompiler import dataflow
from decompiler import memory
from decompiler import pointers
from decompiler import signatures

ROOT = Path(__file__).resolve().parents[1]
GAME = "Spider-Man: The Videogame (arcade, Sega System 32)"
ROM_SET_TITLE = "Spider-Man: The Videogame (World, set spidman)"
CPU = "V60"
BINARY = "spidman"                           # its executables: spidman_game_headless, spidman
GUARD = "SPIDERMAN"                          # the generated headers' include guards

# The decompiler's settings. These are conservative first values taken from the board layout and the
# program's memory map; they are refined as the disassembly names the game's structures and variables.
pointers.STRUCTS = {}
pointers.WORK_RAM = (0x200000, 0x210000)
memory.WORK_RAM = (0x200000, 0x210000)
memory.CANONICAL = rom.canonical_address
memory.NOTES = {
    "variables": ["/* the game's variables in work RAM */"],
    "scoped": ["/* work RAM as one family of functions uses it (their scratch variables) */"],
    "tables": ["/* data in the program ROM the code reads */"],
    "hardware": ["/* the board's registers (read and written through the bus) */"],
}
signatures.CONTEXT = ()
signatures.INITIAL = {}
signatures.SCRATCH_CONTEXT = {}
signatures.CONSTANT = {}
dataflow.HANDLER_SLOTS = (0x1C, 0x9C)    # TODO: confirm the game's object handler slots from the disassembly

# The frame wait polls bit 0 of this work-RAM byte (set by the VBLANK start handler): the scaffold's
# frame loop (tools/core/recompile.py) waits on the byte at this address.
FRAME_WAIT_FLAG = 0x208010

# The sound program's board (z80/emit.py): its banked window and its sound chip; the runtime counts a
# block's cycles alone (z80_block(cycles))
SOUND_BLOCK_FETCHES = False
SOUND_HARDWARE = [(0xA000, 0xC000, "SAMPLE_BANK"), (0xC000, 0xD000, "PCM"), (0xD000, 0xE000, "PCM_RAM")]
SOUND_NOTES = {
    "variables": ["/* the sound program's variables, in the RAM it shares with the V60 (E000-FFFF) */"],
    "scoped": ["/* shared RAM as one family of functions uses it */"],
    "tables": ["/* data in the program ROM the code reads */"],
    "hardware": ["/* the sample bank's window and the RF5C68 (read and written through the bus) */"],
}
