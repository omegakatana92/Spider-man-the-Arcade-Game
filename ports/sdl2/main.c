/**
 *    Name:        main.c
 *    Description: Spider-Man: The Videogame in a window: SDL2 shows the frames, plays the sound, reads the
 *                 four players' controls and keeps the pace, around the same board and program the headless
 *                 runs check
 *    Version:     0.1.0
 *    Created:     28-10-2026
 *    Author:      Spider-Man recompilation project
 *
 *    license:BSD-3-Clause
 *    Based on the SDL2 port of segasonic-decomp (Sebastian Toyos, BSD-3-Clause); the trackball handling is
 *    replaced by the four-player joystick panel of System 32's 4-player board.
 *
 *    The executable carries nothing of the game's: it reads the player's own MAME set 'spidman.zip' (or its
 *    20 files in a folder) when it starts. What it writes goes beside it on Windows, elsewhere to the
 *    user's folder for the game: spidman.eeprom (the board's settings) and spidman.ini (the window's).
 *
 *    Controls, player 1 (the arcade panel's four players are on the keyboard and on game controllers):
 *      arrows                         move              Z, space       button 1
 *      X                              button 2          1 / 5          start 1 / coin 1
 *      P2: W A S D, Q, E, 2 / 6       P3: I J K L, U, O, 3 / 7
 *      P4: keypad 8 4 5 6, 7, 9, 4 / 8
 *      9                              service           F2             test
 *      F11 or Alt+Enter               full screen       F3             smooth or sharp pixels
 *      Escape                         quit
 *    Game controllers, one per player: the left stick or D-pad moves, A and B are the buttons, Start starts,
 *    Back inserts a coin. The Steam Deck's built-in controller is player 1.
 */
#include "s32/machine.h"
#include "s32/program.h"
#include "s32/video.h"
#include "game/data.h"
#include "game/extract.h"
#include "s32/script.h"
#ifdef SPIDERMAN_SOUND
#include "game/sound_reference.h"
#endif

#include "settings.h"

#include <SDL.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PLAYERS            4
#define OUTPUT_RATE        48000
#define QUEUE_FRAMES_MAX   6          /* audio queued beyond this many frames is dropped */
#define ROM_FILE           "spidman.zip"

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static SDL_AudioDeviceID audio;
static SDL_GameController *controllers[PLAYERS];
static uint64_t next_frame;
static double shown_scale;
static char eeprom_path[1024];
static bool quit_requested;
static s32_script *script;      /* --script: deterministic controls for a test run */

/* What the controls say, gathered between frames */
static struct
{
	bool up[PLAYERS], down[PLAYERS], left[PLAYERS], right[PLAYERS];
	bool button1[PLAYERS], button2[PLAYERS];
	bool coin[PLAYERS];
	bool start[PLAYERS];
	bool service;
	bool test;
} controls;

/* A chip's samples on their way to the output rate */
typedef struct resampler
{
	double step;
	double position;
	int16_t *samples;
	size_t count;
	size_t capacity;
} resampler;

static resampler mixed;

/* ---- files ---- */

static uint8_t *load_file(const char *path, size_t *size)
{
	FILE *input = fopen(path, "rb");
	uint8_t *data;
	long length;

	if (!input)
		return NULL;
	fseek(input, 0, SEEK_END);
	length = ftell(input);
	fseek(input, 0, SEEK_SET);
	data = length > 0 ? (uint8_t *)malloc((size_t)length) : NULL;
	if (data && fread(data, 1, (size_t)length, input) != (size_t)length)
	{
		free(data);
		data = NULL;
	}
	fclose(input);
	if (size)
		*size = data ? (size_t)length : 0;
	return data;
}

static void save_eeprom(void)
{
	FILE *output;

	if (!s32_board.eeprom.dirty)
		return;
	output = fopen(eeprom_path, "wb");
	if (!output)
		return;
	fwrite(s32_board.eeprom.data, 1, sizeof(s32_board.eeprom.data), output);
	fclose(output);
	s32_board.eeprom.dirty = false;
}

static void load_eeprom(void)
{
	size_t size;
	uint8_t *data = load_file(eeprom_path, &size);

	if (data && size == sizeof(s32_board.eeprom.data))
		memcpy(s32_board.eeprom.data, data, size);
	free(data);
}

/* ---- the picture ---- */

static void on_frame_shown(void)
{
	int width;
	const uint32_t *pixels;
	SDL_Rect source;

	s32_video_draw_frame();
	pixels = s32_video_frame(&width);
	SDL_UpdateTexture(texture, NULL, pixels, S32_VIDEO_MAX_WIDTH * (int)sizeof(uint32_t));
	source.x = 0;
	source.y = 0;
	source.w = width;
	source.h = S32_VIDEO_HEIGHT;
	SDL_RenderSetLogicalSize(renderer, width, S32_VIDEO_HEIGHT);
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, texture, &source, NULL);
	SDL_RenderPresent(renderer);
}

static void toggle_full_screen(void)
{
	settings.full_screen = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP) == 0;
	SDL_SetWindowFullscreen(window, settings.full_screen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
	settings_save();
}

static void toggle_smooth(void)
{
	settings.smooth = !settings.smooth;
	SDL_SetTextureScaleMode(texture, settings.smooth ? SDL_ScaleModeLinear : SDL_ScaleModeNearest);
	settings_save();
}

/* the settings the picture shows, as the file has them after it was read again */
static void apply_settings(void)
{
	const bool full = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0;

	SDL_SetTextureScaleMode(texture, settings.smooth ? SDL_ScaleModeLinear : SDL_ScaleModeNearest);
	if (full != settings.full_screen)
		SDL_SetWindowFullscreen(window, settings.full_screen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
	if (settings.window_scale != shown_scale)
	{
		shown_scale = settings.window_scale;
		SDL_SetWindowSize(window, (int)(320 * shown_scale), (int)(S32_VIDEO_HEIGHT * shown_scale));
	}
}

static void read_events(void)
{
	SDL_Event event;

	while (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
			case SDL_QUIT:
				quit_requested = true;
				break;
			case SDL_KEYDOWN:
				if (event.key.repeat)
					break;
				if (event.key.keysym.sym == SDLK_ESCAPE)
					quit_requested = true;
				if (event.key.keysym.sym == SDLK_F11
					|| (event.key.keysym.sym == SDLK_RETURN && (event.key.keysym.mod & KMOD_ALT)))
					toggle_full_screen();
				if (event.key.keysym.sym == SDLK_F3)
					toggle_smooth();
				break;
			case SDL_CONTROLLERDEVICEADDED:
				for (int player = 0; player < PLAYERS; player++)
					if (!controllers[player])
					{
						controllers[player] = SDL_GameControllerOpen(event.cdevice.which);
						break;
					}
				break;
			case SDL_CONTROLLERDEVICEREMOVED:
				for (int player = 0; player < PLAYERS; player++)
					if (controllers[player]
						&& SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controllers[player])) == event.cdevice.which)
					{
						SDL_GameControllerClose(controllers[player]);
						controllers[player] = NULL;
					}
				break;
			default:
				break;
		}
	}
}

/* The keyboard's four players: player 1 arrows/Z/X, player 2 W/A/S/D/Q/E, player 3 I/J/K/L/U/O,
   player 4 the numeric keypad. */
static void read_keyboard(const Uint8 *keys)
{
	static const SDL_Scancode up[PLAYERS] = { SDL_SCANCODE_UP, SDL_SCANCODE_W, SDL_SCANCODE_I, SDL_SCANCODE_KP_8 };
	static const SDL_Scancode down[PLAYERS] = { SDL_SCANCODE_DOWN, SDL_SCANCODE_S, SDL_SCANCODE_K, SDL_SCANCODE_KP_5 };
	static const SDL_Scancode left[PLAYERS] = { SDL_SCANCODE_LEFT, SDL_SCANCODE_A, SDL_SCANCODE_J, SDL_SCANCODE_KP_4 };
	static const SDL_Scancode right[PLAYERS] = { SDL_SCANCODE_RIGHT, SDL_SCANCODE_D, SDL_SCANCODE_L, SDL_SCANCODE_KP_6 };
	static const SDL_Scancode button1[PLAYERS] = { SDL_SCANCODE_Z, SDL_SCANCODE_Q, SDL_SCANCODE_U, SDL_SCANCODE_KP_7 };
	static const SDL_Scancode button2[PLAYERS] = { SDL_SCANCODE_X, SDL_SCANCODE_E, SDL_SCANCODE_O, SDL_SCANCODE_KP_9 };
	static const SDL_Scancode start[PLAYERS] = { SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3, SDL_SCANCODE_4 };
	static const SDL_Scancode coin[PLAYERS] = { SDL_SCANCODE_5, SDL_SCANCODE_6, SDL_SCANCODE_7, SDL_SCANCODE_8 };

	for (int player = 0; player < PLAYERS; player++)
	{
		controls.up[player] |= keys[up[player]] != 0;
		controls.down[player] |= keys[down[player]] != 0;
		controls.left[player] |= keys[left[player]] != 0;
		controls.right[player] |= keys[right[player]] != 0;
		controls.button1[player] |= keys[button1[player]] != 0 || (player == 0 && keys[SDL_SCANCODE_SPACE]) != 0;
		controls.button2[player] |= keys[button2[player]] != 0;
		controls.start[player] |= keys[start[player]] != 0;
		controls.coin[player] |= keys[coin[player]] != 0;
	}
	controls.service |= keys[SDL_SCANCODE_9] != 0;
	controls.test |= keys[SDL_SCANCODE_F2] != 0;
}

static void read_controllers(void)
{
	for (int player = 0; player < PLAYERS; player++)
	{
		SDL_GameController *pad = controllers[player];
		const int dead = 12000;

		if (!pad)
			continue;
		const int x = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
		const int y = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY);

		controls.left[player] |= x < -dead || SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
		controls.right[player] |= x > dead || SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
		controls.up[player] |= y < -dead || SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_UP);
		controls.down[player] |= y > dead || SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
		controls.button1[player] |= SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_A) != 0;
		controls.button2[player] |= SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_B) != 0;
		controls.start[player] |= SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_START) != 0;
		controls.coin[player] |= SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_BACK) != 0;
	}
}

/* the state of every control once a frame, where the board reads them */
static void on_controls_update(void)
{
	const Uint8 *keys = SDL_GetKeyboardState(NULL);
	s32_inputs *input = &s32_board.input;
	uint8_t port_a = 0xff, port_b = 0xff, extra1 = 0xff, extra2 = 0xff, extra3 = 0xff;

	read_events();
	if (script)
	{
		s32_script_apply(script, s32_board.frame);
		return;
	}
	read_keyboard(keys);
	read_controllers();

	for (int player = 0; player < PLAYERS; player++)
	{
		uint8_t *port = player == 0 ? &port_a : player == 1 ? &port_b : player == 2 ? &extra1 : &extra2;

		*port &= (uint8_t)~(controls.up[player] ? 0x20 : 0)
			& (uint8_t)~(controls.down[player] ? 0x10 : 0)
			& (uint8_t)~(controls.left[player] ? 0x80 : 0)
			& (uint8_t)~(controls.right[player] ? 0x40 : 0)
			& (uint8_t)~(controls.button1[player] ? 0x01 : 0)
			& (uint8_t)~(controls.button2[player] ? 0x02 : 0);
	}
	/* the board's ports are active low: a cleared bit is a pressed button */
	input->port_a = port_a;
	input->port_b = port_b;
	input->extra1 = extra1;
	input->extra2 = extra2;
	input->extra3 = (uint8_t)(0xff
		& ~(controls.start[2] ? 0x01 : 0) & ~(controls.start[3] ? 0x02 : 0));
	input->port_e = (uint8_t)(0xff
		& ~(controls.service ? 0x01 : 0) & ~(controls.test ? 0x02 : 0)
		& ~(controls.coin[0] ? 0x04 : 0) & ~(controls.coin[1] ? 0x08 : 0)
		& ~(controls.start[0] ? 0x10 : 0) & ~(controls.start[1] ? 0x20 : 0));

	memset(&controls, 0, sizeof(controls));
}

/* ---- the sound ---- */
#ifdef SPIDERMAN_SOUND

static void keep_samples(const int16_t *samples, size_t count)
{
	if (mixed.count + count > mixed.capacity)
	{
		mixed.capacity = (mixed.count + count) * 2;
		mixed.samples = (int16_t *)realloc(mixed.samples, mixed.capacity * 2 * sizeof(int16_t));
	}
	memcpy(mixed.samples + mixed.count * 2, samples, count * 2 * sizeof(int16_t));
	mixed.count += count;
}

static void sample_at(double position, double out[2])
{
	const size_t index = (size_t)position;
	const double fraction = position - (double)index;

	for (int side = 0; side < 2; side++)
	{
		const double here = mixed.samples[index * 2 + side];
		const double next = mixed.samples[(index + 1) * 2 + side];
		out[side] = here + (next - here) * fraction;
	}
}

static void drop_used(void)
{
	const size_t used = (size_t)mixed.position;

	if (!used)
		return;
	memmove(mixed.samples, mixed.samples + used * 2, (mixed.count - used) * 2 * sizeof(int16_t));
	mixed.count -= used;
	mixed.position -= (double)used;
}

/* The frame's mixed samples from the sound board (the original Z80 program on MAME's core, the
   RF5C68 and YM3438 as MAME/ymfm make them), resampled to 48 kHz at the settings' volume. */
static void play_frame(void)
{
	static int16_t out[OUTPUT_RATE / 20 * 2];
	size_t made = 0, count = 0;
	const int16_t *samples;

	game_sound_end_frame(s32_board.cycles);
	samples = game_sound_frame_mix(&count);
	if (getenv("SPIDERMAN_SOUND_TRACE") && program_frames() % 60 == 0)
	{
		int peak = 0;
		for (size_t index = 0; index < count * 2; index++)
		{
			const int value = samples[index] < 0 ? -samples[index] : samples[index];
			if (value > peak)
				peak = value;
		}
		fprintf(stderr, "sound: frame %llu, %zu samples at %u Hz, peak %d\n",
		        (unsigned long long)program_frames(), count, game_sound_rate(), peak);
	}
	keep_samples(samples, count);
	while (made < OUTPUT_RATE / 20)
	{
		double value[2];

		if (mixed.position + 1 >= (double)mixed.count)
			break;
		sample_at(mixed.position, value);
		for (int side = 0; side < 2; side++)
		{
			const double scaled = settings.volume * value[side];
			out[made * 2 + side] = (int16_t)(scaled > 32767 ? 32767 : (scaled < -32768 ? -32768 : scaled));
		}
		mixed.position += mixed.step;
		made++;
	}
	drop_used();
	if (audio && SDL_GetQueuedAudioSize(audio) < (Uint32)(OUTPUT_RATE / 60 * 4 * QUEUE_FRAMES_MAX))
		SDL_QueueAudio(audio, out, (Uint32)(made * 2 * sizeof(int16_t)));
}
#endif

/* ---- the frame ---- */

static void wait_for_frame(void)
{
	const uint64_t frequency = SDL_GetPerformanceFrequency();
	uint64_t now = SDL_GetPerformanceCounter();

	next_frame += frequency / S32_FRAME_RATE;
	if (now > next_frame + frequency / 4)
		next_frame = now;
	while ((now = SDL_GetPerformanceCounter()) < next_frame)
	{
		const uint64_t left = (next_frame - now) * 1000 / frequency;
		if (left > 2)
			SDL_Delay((Uint32)(left - 1));
	}
}

static void on_frame_done(void)
{
	if (quit_requested)
	{
		program_stop();
		return;
	}
#ifdef SPIDERMAN_SOUND
	play_frame();
#endif
	if (program_frames() % 600 == 0)
		save_eeprom();
	if (program_frames() % 30 == 0 && settings_reread())
		apply_settings();
	wait_for_frame();
}

static void on_irq_changed(bool asserted)
{
	program_irq_line(asserted);
}

static void on_log(const char *message)
{
	(void)message;
}

/* A window with what went wrong: the port has no console */
static int fail(const char *what, const char *detail)
{
	char message[1200];

	snprintf(message, sizeof(message), "%s\n%s", what, detail);
	fprintf(stderr, "%s\n", message);
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Spider-Man: The Videogame", message, NULL);
	return 1;
}

/* Where the game keeps what it writes: beside the executable on Windows; elsewhere the user's folder */
static char *saves_folder(void)
{
#ifdef _WIN32
	return SDL_GetBasePath();
#else
	return SDL_GetPrefPath("", "SpiderMan");
#endif
}

/* The player's ROM set: $SPIDERMAN_ROM, else spidman.zip beside the executable, in the folder the game
   keeps what it writes, or rom/spidman.zip in the current directory */
static const char *find_rom(const char *base, const char *saves, char *path, size_t size)
{
	const char *given = getenv("SPIDERMAN_ROM");

	if (given && given[0])
		return given;
	snprintf(path, size, "%s" ROM_FILE, base ? base : "");
	{
		FILE *probe = fopen(path, "rb");
		if (probe)
		{
			fclose(probe);
			return path;
		}
	}
	if (saves && saves[0])
	{
		snprintf(path, size, "%s" ROM_FILE, saves);
		FILE *probe = fopen(path, "rb");
		if (probe)
		{
			fclose(probe);
			return path;
		}
	}
	snprintf(path, size, "rom/%s", ROM_FILE);
	return path;
}

int main(int argc, char **argv)
{
	SDL_AudioSpec wanted;
	char settings_path[1024], rom_path[1024];
	char *base, *saves;
	const char *rom;

	(void)argc;
	(void)argv;
	for (int index = 1; index < argc; index++)
		if (strcmp(argv[index], "--script") == 0 && index + 1 < argc)
		{
			char error[256];

			script = s32_script_load(argv[++index], error, sizeof(error));
			if (!script)
				return fail("The control script cannot be read.", error);
		}
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0)
		return fail("SDL could not start.", SDL_GetError());
	base = SDL_GetBasePath();
	saves = saves_folder();
	snprintf(settings_path, sizeof(settings_path), "%sspidman.ini", saves ? saves : "");
	snprintf(eeprom_path, sizeof(eeprom_path), "%sspidman.eeprom", saves ? saves : "");
	rom = find_rom(base, saves, rom_path, sizeof(rom_path));
	if (game_rom_set_load(rom))
		return fail("The ROM set is missing or incomplete.",
			"Put your own MAME spidman.zip beside the executable (or in rom/), or set SPIDERMAN_ROM.");
	SDL_free(base);
	SDL_free(saves);
	settings_open(settings_path);
	shown_scale = settings.window_scale;
	window = SDL_CreateWindow("Spider-Man: The Videogame", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		(int)(320 * shown_scale), (int)(S32_VIDEO_HEIGHT * shown_scale),
		SDL_WINDOW_RESIZABLE | (settings.full_screen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0));
	renderer = window ? SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED) : NULL;
	if (window && !renderer)
		renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
	texture = renderer ? SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
		S32_VIDEO_MAX_WIDTH, S32_VIDEO_HEIGHT) : NULL;
	if (!texture)
		return fail("The window could not be opened.", SDL_GetError());
	SDL_RenderSetIntegerScale(renderer, SDL_FALSE);
	SDL_SetTextureScaleMode(texture, settings.smooth ? SDL_ScaleModeLinear : SDL_ScaleModeNearest);

	SDL_zero(wanted);
	wanted.freq = OUTPUT_RATE;
	wanted.format = AUDIO_S16SYS;
	wanted.channels = 2;
	wanted.samples = 1024;
	audio = SDL_OpenAudioDevice(NULL, 0, &wanted, NULL, 0);
	if (audio)
		SDL_PauseAudioDevice(audio, 0);

	/* the board, the video, the sound board and the game, as the headless runs set them up */
	s32_board.irq_changed = on_irq_changed;
	s32_board.controls_update = on_controls_update;
	s32_board.log = on_log;
	s32_board.frame_timing = true;
	s32_board.sound_rom = game_sound;
	s32_video_init(game_tiles, GAME_TILES_SIZE, game_sprites, GAME_SPRITES_SIZE);
	s32_board.frame_shown = on_frame_shown;
#ifdef SPIDERMAN_SOUND
	game_sound_attach();
	mixed.step = (double)game_sound_rate() / OUTPUT_RATE;
#endif
	s32_init(game_program);
	load_eeprom();
	program_reset();
	program_frame_done = on_frame_done;
	next_frame = SDL_GetPerformanceCounter();

	program_run();

	save_eeprom();
	if (audio)
		SDL_CloseAudioDevice(audio);
	for (int player = 0; player < PLAYERS; player++)
		if (controllers[player])
			SDL_GameControllerClose(controllers[player]);
	s32_script_free(script);
	SDL_Quit();
	return 0;
}
