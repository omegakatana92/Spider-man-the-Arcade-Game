"""
Name:        spidman_rom.py
Description: MAME 'spidman' ROM set (Spider-Man: The Videogame, World) and memory region builders
Version:     0.1.0
Created:     28-10-2026
Author:      Spider-Man recompilation project

Files are identified by CRC32 + SHA1, not by name. Region layouts follow ROM_START( spidman )
in MAME src/mame/sega/segas32.cpp (BSD-3-Clause).

The board is Sega System 32, 4-player version (sega_system32_4p): NEC V60 main CPU,
Z80 sound CPU, YM3438 + RF5C68 sound, Sega 315-5296 I/O, 8255 PPI for players 3-4.
The set is not protected (init_spidman only calls segas32_common_init).
"""
import hashlib
import zipfile
import zlib
from pathlib import Path

SET_NAME = "spidman"    # MAME's name for the set

# MAME name -> (crc32, sha1, size)
ROM_FILES = {
    "epr-14307.ic13": ("d900219c", "d59654db1fc0ec4d5f8cda9000ab4bd3bb36cdfc", 0x020000),
    "epr-14306.ic7": ("64379dc6", "7efc7175351186c54f141161a395e63b1cc7e7a5", 0x020000),
    "epr-14281.ic14": ("8a746c42", "fa3729ec3aa4b3c59322408146ce2cfbf5a11b98", 0x020000),
    "epr-14280.ic6": ("3c8148f7", "072b7982bb95e7a9ab77844b59020146c262488d", 0x020000),

    "epr-14285.ic35": ("25aefad6", "10153f4e773a0f55378f869eb1d85156e85f893f", 0x040000),
    "mpr-14284.ic31": ("760542d4", "dcac73869c02fefd328bd6bdbcbdb3b68b0647da", 0x080000),
    "mpr-14283.ic26": ("c863a91c", "afdc76bbb9b207cfcb47d437248a757d03212f4e", 0x080000),
    "mpr-14282.ic22": ("ea20979e", "9b70ef055da8c7c56da54b7edef2379678e7c50f", 0x080000),

    "mpr-14291-s.ic38": ("490f95a1", "f220788670b76164ac414ed9b16a422f719be267", 0x100000),
    "mpr-14290-s.ic34": ("a144162d", "d43f12dd9f690cdfcebb6c7b515ff7dc7dcaa377", 0x100000),
    "mpr-14289-s.ic29": ("38570582", "a9d810a02a1f5a6849c79d65fbebff21a4b82b59", 0x100000),
    "mpr-14288-s.ic25": ("3188b636", "bc0adeeca5040caa563ee1e0eded9c323ca23446", 0x100000),

    "mpr-14299-h.ic36": ("ce59231b", "bcb1f11b74935694d0617ec8df66db2cc57b6219", 0x100000),
    "mpr-14298-h.ic32": ("2745c84c", "5a0528c921cba7a1047d3a2ece79925103d719a1", 0x100000),
    "mpr-14297-h.ic27": ("29cb9450", "7dc38d23a2f0cee2f4edde05c1a6f0dc83f331db", 0x100000),
    "mpr-14296-h.ic23": ("9d8cbd31", "55a9f9ec9029157da033e69664b58e694a28db47", 0x100000),
    "mpr-14295-h.ic37": ("29591f50", "1ac4ceaf74892e30f210ad77024eb441c5e4a959", 0x100000),
    "mpr-14294-h.ic33": ("fa86b794", "7b6907e5734fbf2fba7bcc213a8551fec5e9f3d5", 0x100000),
    "mpr-14293-s.ic28": ("52899269", "ff809ff88701109e0ca79e785a61402d97335cec", 0x100000),
    "mpr-14292-s.ic24": ("41f71443", "351d40d6159cb5b792519bce5d16490965800cfb", 0x100000),
}

# Windows of the V60 program image with unique content; the rest are mirrors (ROM_LOAD_x4 / _x2).
PROGRAM_AREAS = [
    (0x000000, 0x020000),  # epr-14307.ic13
    (0x080000, 0x0A0000),  # epr-14306.ic7
    (0x100000, 0x180000),  # epr-14281.ic14 + epr-14280.ic6, interleaved
]

# the sprite chips: eight byte lanes of 8-byte groups from each base (ROM_LOAD64_BYTE, big-endian region)
SPRITE_CHIPS = [
    (0x000000, ["mpr-14299-h.ic36", "mpr-14298-h.ic32", "mpr-14297-h.ic27", "mpr-14296-h.ic23",
                "mpr-14295-h.ic37", "mpr-14294-h.ic33", "mpr-14293-s.ic28", "mpr-14292-s.ic24"]),
]


def read_files(source):
    """Return {original_name: bytes} from a folder or a .zip."""
    source = Path(source)
    contents = {}
    if source.is_file() and source.suffix.lower() == ".zip":
        with zipfile.ZipFile(source) as archive:
            for entry in archive.infolist():
                if not entry.is_dir():
                    contents[Path(entry.filename).name] = archive.read(entry)
        return contents
    for path in sorted(source.iterdir()):
        if path.is_file():
            contents[path.name] = path.read_bytes()
    return contents


def identify_set(source):
    """Return ({mame_name: bytes}, [report lines]), matching every file by its hashes."""
    by_crc = {crc: name for name, (crc, _, _) in ROM_FILES.items()}
    found = {}
    report = []
    for original_name, data in read_files(source).items():
        crc = format(zlib.crc32(data) & 0xFFFFFFFF, "08x")
        mame_name = by_crc.get(crc)
        if mame_name and ROM_FILES[mame_name][1] == hashlib.sha1(data).hexdigest():
            found[mame_name] = data
            report.append(f"OK      {original_name:20} = {mame_name}")
        else:
            report.append(f"--      {original_name:20} not part of the set (crc {crc})")
    for mame_name in sorted(set(ROM_FILES) - set(found)):
        report.append(f"MISSING {mame_name}")
    return found, report


def load_set(source):
    """Like identify_set, but stops when a file is missing."""
    found, _ = identify_set(source)
    missing = sorted(set(ROM_FILES) - set(found))
    if missing:
        raise SystemExit("The 'spidman' ROM set is incomplete. Missing: " + ", ".join(missing))
    return found


def interleave16(even, odd):
    """ROM_LOAD16_BYTE: even bytes from one chip, odd bytes from the other."""
    result = bytearray(len(even) * 2)
    result[0::2] = even
    result[1::2] = odd
    return result


def build_program(rom):
    """mainpcb:maincpu region (2 MB, little endian) as the V60 sees it at 0x000000-0x1FFFFF."""
    image = bytearray(0x200000)
    image[0x000000:0x080000] = rom["epr-14307.ic13"] * 4
    image[0x080000:0x100000] = rom["epr-14306.ic7"] * 4
    interleaved = interleave16(rom["epr-14281.ic14"], rom["epr-14280.ic6"])
    image[0x100000:0x180000] = interleaved
    image[0x180000:0x200000] = interleaved
    return bytes(image)


def build_tiles(rom):
    """mainpcb:tiles region (4 MB): 4 byte lanes of 32-bit words (ROM_LOAD32_BYTE)."""
    lanes = [rom["mpr-14291-s.ic38"], rom["mpr-14289-s.ic29"],
             rom["mpr-14290-s.ic34"], rom["mpr-14288-s.ic25"]]
    image = bytearray(0x400000)
    for lane, data in enumerate(lanes):
        image[lane::4] = data
    return bytes(image)


def build_sprites(rom):
    """mainpcb:sprites region (8 MB) in file byte order: every 64-bit group is eight lanes."""
    image = bytearray(b"\xff" * 0x800000)
    for base, chips in SPRITE_CHIPS:
        for lane, name in enumerate(chips):
            data = rom[name]
            image[base + lane:base + len(data) * 8:8] = data
    return bytes(image)


def build_sound(rom):
    """mainpcb:soundcpu region (4 MB): Z80 program (x4) and three 1 MB banks of PCM samples."""
    image = bytearray(0x400000)
    image[0x000000:0x100000] = rom["epr-14285.ic35"] * 4
    image[0x100000:0x200000] = rom["mpr-14284.ic31"] * 2
    image[0x200000:0x300000] = rom["mpr-14283.ic26"] * 2
    image[0x300000:0x400000] = rom["mpr-14282.ic22"] * 2
    return bytes(image)


def chips_from_regions(regions):
    """The set's chips back from the regions ({region name: bytes}): {mame_name: bytes}."""
    program, tiles, sprites, sound = (regions[name] for name in ("program", "tiles", "sprites", "sound"))
    chips = {
        "epr-14307.ic13": program[0x000000:0x020000],
        "epr-14306.ic7": program[0x080000:0x0A0000],
        "epr-14281.ic14": program[0x100000:0x180000:2],
        "epr-14280.ic6": program[0x100001:0x180000:2],
        "mpr-14291-s.ic38": tiles[0::4],
        "mpr-14289-s.ic29": tiles[1::4],
        "mpr-14290-s.ic34": tiles[2::4],
        "mpr-14288-s.ic25": tiles[3::4],
        "epr-14285.ic35": sound[0x000000:0x040000],
        "mpr-14284.ic31": sound[0x100000:0x180000],
        "mpr-14283.ic26": sound[0x200000:0x280000],
        "mpr-14282.ic22": sound[0x300000:0x380000],
    }
    for base, names in SPRITE_CHIPS:
        for lane, name in enumerate(names):
            size = ROM_FILES[name][2]
            end = base + size * 8
            chip = bytearray(size)
            chip[0::1] = sprites[base + lane:end:8]
            chips[name] = chip
    return {name: bytes(chip) for name, chip in chips.items()}


def canonical_address(address):
    """Map a V60 address to its copy inside PROGRAM_AREAS, or None if it is not ROM.

    The bus is 24 bits wide; 0xF00000-0xFFFFFF mirrors the first MB of the region (MAME's map).
    """
    address &= 0xFFFFFF
    if address >= 0xF00000:
        address -= 0xF00000
    elif address >= 0x200000:
        return None
    if address < 0x080000:
        return address & 0x01FFFF
    if address < 0x100000:
        return 0x080000 | (address & 0x01FFFF)
    return 0x100000 | (address & 0x07FFFF)


class ProgramMemory:
    """V60 view of the program ROM for the disassembler: mirrors folded, anything else reads 0xFF."""

    def __init__(self, image):
        self.image = image

    def r8(self, address):
        offset = canonical_address(address)
        return self.image[offset] if offset is not None else 0xFF

    def r16(self, address):
        return self.r8(address) | (self.r8(address + 1) << 8)

    def r32(self, address):
        return self.r16(address) | (self.r16(address + 2) << 16)
