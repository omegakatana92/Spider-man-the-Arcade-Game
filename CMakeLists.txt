# Name:        CMakeLists.txt
# Description: build of the Spider-Man native recompilation, its board runtime and its development tools
# Version:     0.1.0
# Created:     28-10-2026
# Author:      Spider-Man recompilation project
cmake_minimum_required(VERSION 3.20)
project(spidman C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Every executable lands in build/bin, whatever the generator or configuration
set(SPIDERMAN_BIN_DIR ${CMAKE_SOURCE_DIR}/build/bin CACHE PATH "Where the executables go")
foreach(config IN ITEMS "" _DEBUG _RELEASE _RELWITHDEBINFO _MINSIZEREL)
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY${config} ${SPIDERMAN_BIN_DIR})
endforeach()

if(MSVC)
	add_compile_options(/W3 /utf-8)
	add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

# System 32 board model: part of the game (sound.c is the Z80's side of the board, video.c draws the
# frames, script.c drives the controls in test runs)
add_library(s32runtime STATIC
	runtime/src/machine.c
	runtime/src/io.c
	runtime/src/script.c
	runtime/src/sound.c
	runtime/src/sound_chips.c
	runtime/src/rf5c68.c
	runtime/src/ym3438.c
	runtime/src/video.c
	runtime/src/memory_census.c
)
target_include_directories(s32runtime PUBLIC runtime/include)
if(UNIX AND NOT APPLE)
	target_link_libraries(s32runtime PUBLIC m)
endif()

# The game's data: no executable carries the ROM or anything from it. This first build reads the
# player's ROM set at run time; the asset pack follows once the extractor and the pack layout are made.
add_library(spidman_data STATIC
	runtime/src/game_data.c
	runtime/src/rom_set.c
	runtime/src/inflate.c
)
target_include_directories(spidman_data PUBLIC runtime/include)

# The translation scaffold: the V60 runtime plus the C tools/recompile.py writes from the user's ROM
# into build/generated, instruction by instruction. It is the original program translated to native C,
# with the V60 registers and flags still explicit; it is not an interpreted CPU.
file(GLOB SPIDERMAN_GENERATED CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/build/generated/*.c)
if(SPIDERMAN_GENERATED)
	add_library(spidman_program STATIC runtime/src/v60.c runtime/src/tasks.c ${SPIDERMAN_GENERATED})
	target_include_directories(spidman_program PUBLIC runtime/include build/generated)
	target_link_libraries(spidman_program PUBLIC s32runtime)
	if(MSVC)
		target_compile_options(spidman_program PRIVATE /bigobj)
	endif()

	add_executable(spidman_headless ports/headless/main.c)
	target_compile_definitions(spidman_headless PRIVATE SPIDERMAN_SCAFFOLD)
	target_link_libraries(spidman_headless PRIVATE spidman_program spidman_data)
else()
	message(STATUS "No generated program yet: run tools/recompile.py to build the scaffold")
endif()

# Development tools built on MAME's V60 code; never linked into the game
option(SPIDERMAN_REFERENCE_TOOLS "Build the MAME-based reference tools" ON)
if(SPIDERMAN_REFERENCE_TOOLS)
	find_package(Python3 COMPONENTS Interpreter REQUIRED)

	add_executable(oracle
		tools/reference/oracle.cpp
		tools/reference/mame/v60d.cpp
	)
	target_include_directories(oracle PRIVATE tools/reference/stub tools/reference/mame)

	add_executable(oracle_z80
		tools/reference/oracle_z80.cpp
		tools/reference/mame/z80/z80dasm.cpp
	)
	target_include_directories(oracle_z80 PRIVATE tools/reference/stub tools/reference/mame/z80)

	add_library(s32video_reference STATIC tools/reference/video/s32video.cpp)
	target_include_directories(s32video_reference PUBLIC tools/reference/video PRIVATE runtime/include)
	if(MSVC)
		target_compile_options(s32video_reference PRIVATE /wd4244 /wd4267 /wd4100 /wd4146)
	endif()

	set(SPIDERMAN_Z80_STEPS ${CMAKE_BINARY_DIR}/generated/cpu/z80/z80.hxx)
	add_custom_command(OUTPUT ${SPIDERMAN_Z80_STEPS}
		COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/generated/cpu/z80
		COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/tools/reference/mame/z80/z80make.py z80
			${CMAKE_SOURCE_DIR}/tools/reference/mame/z80/z80.lst ${SPIDERMAN_Z80_STEPS}
		DEPENDS tools/reference/mame/z80/z80make.py tools/reference/mame/z80/z80.lst
		VERBATIM)
	add_library(s32sound_reference STATIC
		tools/reference/sound/s32sound.cpp
		tools/reference/ymfm/ymfm_opn.cpp
		tools/reference/ymfm/ymfm_adpcm.cpp
		tools/reference/ymfm/ymfm_ssg.cpp
		${SPIDERMAN_Z80_STEPS}
	)
	target_include_directories(s32sound_reference PUBLIC tools/reference/sound
		PRIVATE tools/reference/ymfm runtime/include ${CMAKE_BINARY_DIR}/generated)
	if(MSVC)
		target_compile_options(s32sound_reference PRIVATE /bigobj /wd4244 /wd4267 /wd4100 /wd4146 /wd4127)
	endif()

	add_executable(s32ref
		tools/reference/s32ref.cpp
		tools/reference/mame/v60.cpp
		tools/reference/mame/v60d.cpp
	)
	target_include_directories(s32ref PRIVATE tools/reference/stub tools/reference/mame)
	target_link_libraries(s32ref PRIVATE s32runtime spidman_data s32video_reference s32sound_reference)
	if(MSVC)
		target_compile_options(s32ref PRIVATE /wd4646 /wd4244)
	endif()
endif()

# The game in a window (ports/sdl2), with SDL2 from the system or the development package. When the
# reference tools are built, the sound board uses the reference Z80/chips (a documented fallback until
# the native Z80 translation is complete); otherwise the game builds silent.
if(TARGET spidman_program)
	find_package(SDL2 CONFIG QUIET)
	if(SDL2_FOUND)
		add_executable(spidman WIN32 ports/sdl2/main.c ports/sdl2/settings.c)
		set(SPIDERMAN_SDL2_LIBS spidman_program spidman_data)
		if(TARGET s32sound_reference)
			target_sources(spidman PRIVATE runtime/src/sound_reference.cpp)
			list(APPEND SPIDERMAN_SDL2_LIBS s32sound_reference)
			target_compile_definitions(spidman PRIVATE SPIDERMAN_SOUND)
			message(STATUS "Sound: reference Z80/chips fallback (not native)")
		endif()
		if(TARGET SDL2::SDL2main)
			list(APPEND SPIDERMAN_SDL2_LIBS SDL2::SDL2main)
		endif()
		list(APPEND SPIDERMAN_SDL2_LIBS SDL2::SDL2)
		target_link_libraries(spidman PRIVATE ${SPIDERMAN_SDL2_LIBS})
		if(WIN32)
			add_custom_command(TARGET spidman POST_BUILD
				COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL2::SDL2> $<TARGET_FILE_DIR:spidman>)
			# The C++ sound fallback makes the executable depend on the MinGW runtime; keep the three
			# DLLs beside it so the release runs on a machine without MSYS2.
			get_filename_component(SPIDERMAN_MINGW_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
			foreach(runtime libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll)
				if(EXISTS "${SPIDERMAN_MINGW_BIN}/${runtime}")
					add_custom_command(TARGET spidman POST_BUILD
						COMMAND ${CMAKE_COMMAND} -E copy_if_different
							"${SPIDERMAN_MINGW_BIN}/${runtime}" "$<TARGET_FILE_DIR:spidman>")
				endif()
			endforeach()
		endif()
	else()
		message(STATUS "No SDL2: the window port (spidman) is not built")
	endif()
endif()

# The decompiled game, once src/ exists (tools/decompile.py writes it)
set(SPIDERMAN_DECOMPILED_DIR ${CMAKE_SOURCE_DIR}/src CACHE PATH "The game's C (tools/decompile.py)")
file(GLOB_RECURSE SPIDERMAN_DECOMPILED CONFIGURE_DEPENDS ${SPIDERMAN_DECOMPILED_DIR}/*.c)
if(SPIDERMAN_DECOMPILED)
	add_library(spidman_game STATIC runtime/src/game.c runtime/src/sound_cpu.c ${SPIDERMAN_DECOMPILED})
	target_include_directories(spidman_game PUBLIC runtime/include ${SPIDERMAN_DECOMPILED_DIR} ${SPIDERMAN_DECOMPILED_DIR}/sound)
	target_link_libraries(spidman_game PUBLIC s32runtime)
	if(MSVC)
		target_compile_options(spidman_game PRIVATE /bigobj)
	else()
		target_compile_options(spidman_game PRIVATE -fno-strict-aliasing)
	endif()
	add_executable(spidman_game_headless ports/headless/main.c)
	target_link_libraries(spidman_game_headless PRIVATE spidman_game spidman_data)
endif()
