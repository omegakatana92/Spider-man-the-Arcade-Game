#!/usr/bin/env bash
# Name:        build_appimage.sh
# Description: builds Spider-Man: The Videogame for Linux x86_64 and packages a Steam Deck friendly
#              AppImage. Run this on Linux (SteamOS, Ubuntu, WSL) — not on Windows.
# Version:     0.1.0
# Created:     28-10-2026
# Author:      Spider-Man recompilation project
#
# Usage:
#   ./build_appimage.sh              build, package and verify
#   ./build_appimage.sh --no-verify  build and package without launching the AppImage
#
# Requirements: cmake, a C/C++ compiler, libsdl2-dev, python3, curl or wget, file.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD="$ROOT/build/linux"
APPDIR="$ROOT/build/AppDir"
DIST="$ROOT/dist"
APP="SpiderMan-SteamDeck-x86_64.AppImage"
VERIFY=1

for argument in "$@"; do
    case "$argument" in
        --no-verify) VERIFY=0 ;;
        *) echo "unknown option: $argument" >&2; exit 2 ;;
    esac
done

say() { printf '\n=== %s ===\n' "$1"; }
fail() { printf '\nERROR: %s\n' "$1" >&2; exit 1; }

say "Checking the toolchain"
for tool in cmake gcc g++ python3; do
    command -v "$tool" >/dev/null 2>&1 || fail "$tool is not installed"
done
if command -v pkg-config >/dev/null 2>&1; then
    pkg-config --exists sdl2 || fail "SDL2 development files are missing (install libsdl2-dev)"
else
    [ -e /usr/include/SDL2/SDL.h ] || fail "SDL2 headers not found (install libsdl2-dev)"
fi
echo "  cmake:  $(cmake --version | head -1)"
echo "  gcc:    $(gcc --version | head -1)"
echo "  SDL2:   $(pkg-config --modversion sdl2 2>/dev/null || echo present)"

say "Finding the ROM set"
ROM="${SPIDERMAN_ROM:-}"
if [ -z "$ROM" ] && [ -f "$ROOT/rom/spidman.zip" ]; then ROM="$ROOT/rom/spidman.zip"; fi
if [ -z "$ROM" ] && [ -d "$ROOT/rom" ]; then ROM="$ROOT/rom"; fi
[ -n "$ROM" ] && [ -e "$ROM" ] || fail "no spidman.zip (put it in rom/ or set SPIDERMAN_ROM)"
echo "  $ROM"
python3 "$ROOT/tools/verify_rom.py" "$ROM" || fail "the ROM set did not verify"

say "Translating the V60 program"
python3 "$ROOT/tools/recompile.py" "$ROM" || fail "the V60 translation failed"

say "Configuring"
cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release

say "Building"
cmake --build "$BUILD" --parallel

BIN="$ROOT/build/bin"
[ -x "$BIN/spidman" ] || fail "spidman was not built"
[ -x "$BIN/spidman_headless" ] || fail "spidman_headless was not built"

say "Preparing the AppDir"
rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin" "$APPDIR/usr/lib" "$APPDIR/usr/share/applications"
mkdir -p "$APPDIR/usr/share/icons/hicolor/256x256/apps"
cp "$BIN/spidman" "$APPDIR/usr/bin/spidman"
cp "$ROOT/packaging/appimage/spiderman.desktop" "$APPDIR/spiderman.desktop"
cp "$ROOT/packaging/appimage/spiderman.desktop" "$APPDIR/usr/share/applications/spiderman.desktop"
cp "$ROOT/packaging/appimage/spiderman.png" "$APPDIR/spiderman.png"
cp "$ROOT/packaging/appimage/spiderman.png" "$APPDIR/usr/share/icons/hicolor/256x256/apps/spiderman.png"

# Bundle SDL2 if it is a shared library, so the AppImage works on SteamOS images with a different
# SDL2 build. The rest of the libraries (libc, libm, libstdc++) come from the host, as AppImage
# guidance recommends.
SDL_LIB="$(ldd "$BIN/spidman" | awk '/libSDL2-2\.0\.so/{print $3; exit}')"
if [ -n "$SDL_LIB" ] && [ -f "$SDL_LIB" ]; then
    cp "$SDL_LIB" "$APPDIR/usr/lib/"
    echo "  bundled $(basename "$SDL_LIB")"
fi

cat > "$APPDIR/AppRun" <<'EOF'
#!/bin/sh
HERE="$(dirname "$(readlink -f "$0")")"
export LD_LIBRARY_PATH="$HERE/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec "$HERE/usr/bin/spidman" "$@"
EOF
chmod +x "$APPDIR/AppRun"

# The game itself is ROM-free; say how to supply the player's own set.
cat > "$APPDIR/README-ROM.txt" <<'EOF'
Spider-Man: The Videogame — Steam Deck / Linux x86_64

This AppImage contains no ROM and no game data. Put your own MAME spidman.zip beside
the AppImage, or in ~/.local/share/SpiderMan/, or set SPIDERMAN_ROM to it, then run
the AppImage again.

Controls: keyboard player 1 = arrows + Z/X, 1 start, 5 coin; the Steam Deck's
built-in controller is player 1 (A/B, Start, Back). F11 full screen, F3 pixels,
Escape quit. See docs/CONTROLS.md in the source tree.
EOF

say "Fetching appimagetool"
TOOLS="$ROOT/build/tools"
mkdir -p "$TOOLS"
APPIMAGETOOL="$TOOLS/appimagetool-x86_64.AppImage"
if [ ! -x "$APPIMAGETOOL" ]; then
    URL="https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage"
    if command -v curl >/dev/null 2>&1; then
        curl -L --fail --retry 3 -o "$APPIMAGETOOL" "$URL"
    elif command -v wget >/dev/null 2>&1; then
        wget -O "$APPIMAGETOOL" "$URL"
    else
        fail "curl or wget is needed to download appimagetool"
    fi
    chmod +x "$APPIMAGETOOL"
fi

say "Packaging"
mkdir -p "$DIST"
rm -f "$DIST/$APP"
ARCH=x86_64 "$APPIMAGETOOL" --appimage-extract-and-run "$APPDIR" "$DIST/$APP" 2>/dev/null \
    || ARCH=x86_64 "$APPIMAGETOOL" "$APPDIR" "$DIST/$APP"
chmod +x "$DIST/$APP"
[ -s "$DIST/$APP" ] || fail "the AppImage was not produced"

say "Verifying"
# The AppImage must at least unpack and contain the executable and the desktop entry.
EXTRACT="$ROOT/build/appimage-check"
rm -rf "$EXTRACT"
mkdir -p "$EXTRACT"
( cd "$EXTRACT" && "$DIST/$APP" --appimage-extract >/dev/null )
[ -x "$EXTRACT/squashfs-root/usr/bin/spidman" ] || fail "the AppImage has no executable"
[ -f "$EXTRACT/squashfs-root/spiderman.desktop" ] || fail "the AppImage has no desktop entry"
echo "  unpacked, executable and desktop entry present"
if [ "$VERIFY" = 1 ] && command -v timeout >/dev/null 2>&1; then
    # A short real launch with SDL's dummy drivers: proves the binary starts, loads its libraries and
    # reaches the game loop (status 124 = still running). With no ROM it must exit 1 with its message.
    ROM="${SPIDERMAN_ROM:-}"
    if [ -z "$ROM" ] && [ -f "$ROOT/rom/spidman.zip" ]; then ROM="$ROOT/rom/spidman.zip"; fi
    set +e
    if [ -n "$ROM" ] && [ -f "$ROM" ]; then
        SPIDERMAN_ROM="$ROM" SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
            timeout 8 "$EXTRACT/squashfs-root/AppRun" >/dev/null 2>&1
        STATUS=$?
        if [ "$STATUS" -ne 0 ] && [ "$STATUS" -ne 124 ]; then
            fail "the AppImage exited with $STATUS"
        fi
        echo "  launched with the ROM set (status $STATUS; 124 = still running)"
    else
        SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
            timeout 8 "$EXTRACT/squashfs-root/AppRun" >/dev/null 2>&1
        STATUS=$?
        if [ "$STATUS" -ne 1 ]; then
            echo "  note: no ROM set here; launch returned $STATUS (expected 1 for the missing-ROM message)"
        else
            echo "  launched; reported the missing ROM as expected"
        fi
    fi
    set -e
fi

say "Done"
ls -l "$DIST/$APP"
echo "Copy it anywhere; put spidman.zip beside it or in ~/.local/share/SpiderMan/."
