/**
 *    Name:        rom_set.c
 *    Description: the player's ROM set into the board's memory (game/data.h), from a .zip or a folder
 *    Version:     0.2.0
 *    Created:     20-10-2026
 *    Modified:    28-10-2026 (Spider-Man: MAME set 'spidman')
 *    Author:      Sebastian Toyos; adapted for Spider-Man by the Spider-Man recompilation project
 *
 *    license:BSD-3-Clause
 *
 *    Each of the set's 20 files is known by its CRC32: in a .zip by its own, whatever its name (stored or
 *    deflated: inflate.c); in a folder by MAME's name. The regions are laid out as tools/spidman_rom.py
 *    lays them, after ROM_START( spidman ) in MAME's src/mame/sega/segas32.cpp (BSD-3-Clause). The
 *    reference runs the original code, so it reads the set when it starts, as MAME does; the game reads it
 *    once, to write its assets pack (extract.c).
 */
#include "game/extract.h"

#include "game/data.h"
#include "inflate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ZIP_END             0x06054B50u    /* the end of the central directory */
#define ZIP_ENTRY           0x02014B50u    /* a central directory entry */
#define ZIP_LOCAL           0x04034B50u    /* a local header */
#define ZIP_STORED          0
#define ZIP_DEFLATED        8
#define ZIP_COMMENT_MOST    0xFFFFu

typedef struct
{
	const char *name;
	uint32_t crc, size;
} rom_file;

enum
{
	P_IC13, P_IC7, P_IC14, P_IC6,                 /* program */
	S_IC35, S_IC31, S_IC26, S_IC22,               /* sound program + PCM banks */
	T_IC38, T_IC29, T_IC34, T_IC25,               /* tiles, four byte lanes */
	G_IC36, G_IC32, G_IC27, G_IC23, G_IC37, G_IC33, G_IC28, G_IC24,    /* sprites, eight byte lanes */
	FILES
};

static const rom_file files[FILES] = {
	{ "epr-14307.ic13", 0xd900219cu, 0x020000u },
	{ "epr-14306.ic7",  0x64379dc6u, 0x020000u },
	{ "epr-14281.ic14", 0x8a746c42u, 0x020000u },
	{ "epr-14280.ic6",  0x3c8148f7u, 0x020000u },
	{ "epr-14285.ic35", 0x25aefad6u, 0x040000u },
	{ "mpr-14284.ic31", 0x760542d4u, 0x080000u },
	{ "mpr-14283.ic26", 0xc863a91cu, 0x080000u },
	{ "mpr-14282.ic22", 0xea20979eu, 0x080000u },
	{ "mpr-14291-s.ic38", 0x490f95a1u, 0x100000u },
	{ "mpr-14289-s.ic29", 0x38570582u, 0x100000u },
	{ "mpr-14290-s.ic34", 0xa144162du, 0x100000u },
	{ "mpr-14288-s.ic25", 0x3188b636u, 0x100000u },
	{ "mpr-14299-h.ic36", 0xce59231bu, 0x100000u },
	{ "mpr-14298-h.ic32", 0x2745c84cu, 0x100000u },
	{ "mpr-14297-h.ic27", 0x29cb9450u, 0x100000u },
	{ "mpr-14296-h.ic23", 0x9d8cbd31u, 0x100000u },
	{ "mpr-14295-h.ic37", 0x29591f50u, 0x100000u },
	{ "mpr-14294-h.ic33", 0xfa86b794u, 0x100000u },
	{ "mpr-14293-s.ic28", 0x52899269u, 0x100000u },
	{ "mpr-14292-s.ic24", 0x41f71443u, 0x100000u },
};

static uint32_t crc32_of(const uint8_t *data, uint32_t size)
{
	static uint32_t table[256];
	uint32_t crc = 0xFFFFFFFFu, index;
	int bit;

	if (table[1] == 0u)
		for (index = 0; index < 256u; index++)
		{
			table[index] = index;
			for (bit = 0; bit < 8; bit++)
				table[index] = (table[index] >> 1) ^ (0xEDB88320u & (0u - (table[index] & 1u)));
		}
	for (index = 0; index < size; index++)
		crc = (crc >> 8) ^ table[(crc ^ data[index]) & 0xFFu];
	return ~crc;
}

static uint32_t little16(const uint8_t *bytes)
{
	return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8;
}

static uint32_t little32(const uint8_t *bytes)
{
	return little16(bytes) | little16(bytes + 2) << 16;
}

/* A whole file into memory (the caller frees it), or NULL */
static uint8_t *read_whole(const char *path, size_t *size)
{
	FILE *input = fopen(path, "rb");
	uint8_t *data = NULL;
	long length;

	if (!input)
		return NULL;
	if (fseek(input, 0, SEEK_END) == 0 && (length = ftell(input)) > 0 && fseek(input, 0, SEEK_SET) == 0
		&& (data = malloc((size_t)length)) != NULL && fread(data, 1, (size_t)length, input) != (size_t)length)
	{
		free(data);
		data = NULL;
	}
	fclose(input);
	if (data)
		*size = (size_t)length;
	return data;
}

/* The set's files in the .zip 'zip' ('size' bytes) into 'chips', by their CRC32: 0 when it is no .zip */
static int read_zip(const uint8_t *zip, size_t size, uint8_t **chips)
{
	size_t end, entry, local;
	uint32_t entries, index;
	int file;

	if (size < 22)
		return 0;
	for (end = size - 22;; end--)
	{
		if (little32(zip + end) == ZIP_END)
			break;
		if (end == 0 || size - end > 22 + ZIP_COMMENT_MOST)
			return 0;
	}
	entries = little16(zip + end + 10);
	entry = little32(zip + end + 16);
	for (index = 0; index < entries; index++)
	{
		uint32_t method, crc, packed, unpacked;

		if (entry > size || size - entry < 46 || little32(zip + entry) != ZIP_ENTRY)
			return 0;
		method = little16(zip + entry + 10);
		crc = little32(zip + entry + 16);
		packed = little32(zip + entry + 20);
		unpacked = little32(zip + entry + 24);
		local = little32(zip + entry + 42);
		entry += 46 + little16(zip + entry + 28) + little16(zip + entry + 30) + little16(zip + entry + 32);
		for (file = 0; file < FILES; file++)
			if (!chips[file] && files[file].crc == crc && files[file].size == unpacked)
				break;
		if (file == FILES || local > size || size - local < 30 || little32(zip + local) != ZIP_LOCAL)
			continue;
		local += 30 + little16(zip + local + 26) + little16(zip + local + 28);
		if (local > size || packed > size - local || (method == ZIP_STORED && packed != unpacked)
			|| !(chips[file] = malloc(unpacked)))
			continue;    /* (what the entry says does not fit the file) */
		if ((method == ZIP_STORED ? (memcpy(chips[file], zip + local, unpacked), 1)
				: method == ZIP_DEFLATED && inflate_raw(zip + local, packed, chips[file], unpacked))
			&& crc32_of(chips[file], unpacked) == crc)
			continue;
		free(chips[file]);
		chips[file] = NULL;
	}
	return 1;
}

/* The set's files in the folder 'folder' into 'chips', by their names and their CRC32 */
static void read_folder(const char *folder, uint8_t **chips)
{
	char path[1024];
	int file;

	for (file = 0; file < FILES; file++)
	{
		size_t size = 0;

		snprintf(path, sizeof(path), "%s/%s", folder, files[file].name);
		chips[file] = read_whole(path, &size);
		if (chips[file] && (size != files[file].size || crc32_of(chips[file], files[file].size) != files[file].crc))
		{
			free(chips[file]);
			chips[file] = NULL;
		}
	}
}

/* ROM_LOAD16_BYTE: even bytes from one chip, odd bytes from the other */
static void interleave16(uint8_t *out, const uint8_t *even, const uint8_t *odd, uint32_t size)
{
	uint32_t index;

	for (index = 0; index < size; index++)
	{
		out[index * 2u] = even[index];
		out[index * 2u + 1u] = odd[index];
	}
}

const char *game_rom_set_load(const char *source)
{
	static char problem[96];
	uint8_t *chips[FILES] = { NULL };
	size_t size = 0;
	uint8_t *whole = read_whole(source, &size);
	int file, copy;

	if (whole)
	{
		const int zip = read_zip(whole, size, chips);

		free(whole);
		if (!zip)
			return "not a .zip";
	}
	else
		read_folder(source, chips);
	problem[0] = '\0';
	for (file = 0; file < FILES && !problem[0]; file++)
		if (!chips[file])
			snprintf(problem, sizeof(problem), "no %s (or not this version of it)", files[file].name);
	if (!problem[0])
	{
		/* program: two x4 chips, then the interleaved data chip pair, mirrored */
		for (copy = 0; copy < 4; copy++)
			memcpy(game_program + copy * 0x20000u, chips[P_IC13], 0x20000u);
		for (copy = 0; copy < 4; copy++)
			memcpy(game_program + 0x080000u + copy * 0x20000u, chips[P_IC7], 0x20000u);
		interleave16(game_program + 0x100000u, chips[P_IC14], chips[P_IC6], 0x20000u);
		memcpy(game_program + 0x180000u, game_program + 0x100000u, 0x80000u);

		/* tiles: four byte lanes of 32-bit words */
		for (copy = 0; copy < 0x100000; copy++)
		{
			game_tiles[copy * 4u + 0u] = chips[T_IC38][copy];
			game_tiles[copy * 4u + 1u] = chips[T_IC29][copy];
			game_tiles[copy * 4u + 2u] = chips[T_IC34][copy];
			game_tiles[copy * 4u + 3u] = chips[T_IC25][copy];
		}

		/* sprites: eight byte lanes of 64-bit groups */
		for (copy = 0; copy < 0x100000; copy++)
		{
			game_sprites[copy * 8u + 0u] = chips[G_IC36][copy];
			game_sprites[copy * 8u + 1u] = chips[G_IC32][copy];
			game_sprites[copy * 8u + 2u] = chips[G_IC27][copy];
			game_sprites[copy * 8u + 3u] = chips[G_IC23][copy];
			game_sprites[copy * 8u + 4u] = chips[G_IC37][copy];
			game_sprites[copy * 8u + 5u] = chips[G_IC33][copy];
			game_sprites[copy * 8u + 6u] = chips[G_IC28][copy];
			game_sprites[copy * 8u + 7u] = chips[G_IC24][copy];
		}

		/* sound: the Z80 program x4, then three 512 KB banks x2 */
		for (copy = 0; copy < 4; copy++)
			memcpy(game_sound + copy * 0x40000u, chips[S_IC35], 0x40000u);
		for (copy = 0; copy < 2; copy++)
		{
			memcpy(game_sound + 0x100000u + copy * 0x80000u, chips[S_IC31], 0x80000u);
			memcpy(game_sound + 0x200000u + copy * 0x80000u, chips[S_IC26], 0x80000u);
			memcpy(game_sound + 0x300000u + copy * 0x80000u, chips[S_IC22], 0x80000u);
		}
	}
	for (file = 0; file < FILES; file++)
		free(chips[file]);
	return problem[0] ? problem : NULL;
}
