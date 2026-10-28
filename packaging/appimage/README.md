# AppImage packaging

`build_appimage.sh` in the repository root builds the Linux x86_64 binary and writes
`dist/SpiderMan-SteamDeck-x86_64.AppImage`. It uses:

- `spiderman.desktop` — desktop entry
- `spiderman.png` — 256x256 icon (`tools/make_icon.py`)

The AppDir bundles the executable and `libSDL2-2.0.so.0` when the host provides it.
The AppImage is built and verified on Linux (WSL2 Ubuntu 22.04 in this project); a
Windows executable is never repackaged. The game reads the player's own
`spidman.zip`; no ROM is included.
