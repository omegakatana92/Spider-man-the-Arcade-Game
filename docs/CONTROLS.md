# Controls

The original cabinet is a four-player Sega System 32 panel: each player has an 8-way
joystick and two buttons, plus Start and Coin. The port maps that panel to the
keyboard and to up to four SDL2 game controllers.

## Keyboard defaults

| Action | Player 1 | Player 2 | Player 3 | Player 4 |
|---|---|---|---|---|
| Up | Up arrow | W | I | Keypad 8 |
| Down | Down arrow | S | K | Keypad 5 |
| Left | Left arrow | A | J | Keypad 4 |
| Right | Right arrow | D | L | Keypad 6 |
| Button 1 | Z or Space | Q | U | Keypad 7 |
| Button 2 | X | E | O | Keypad 9 |
| Start | 1 | 2 | 3 | 4 |
| Coin | 5 | 6 | 7 | 8 |

Common:

| Action | Key |
|---|---|
| Service | 9 |
| Test mode | F2 |
| Full screen | F11 or Alt+Enter |
| Smooth / sharp pixels | F3 |
| Quit | Escape |

In test mode the game's own operator menu is used: the joystick moves the cursor,
button 1 selects, service/test advance as the original does.

## Game controllers

Each controller is one player, in the order SDL reports them:

- Left stick or D-pad: move
- **A**: button 1
- **B**: button 2
- **Start**: start
- **Back**: coin

The Steam Deck's built-in controller is player 1. Additional USB/Bluetooth
controllers become players 2-4. No external remapping tool is needed; SDL2's
controller database supplies the mappings.

## Steam Deck

- **Gaming Mode:** add the AppImage as a non-Steam game. The built-in controls work
  as player 1. Use the Steam Input layout "Gamepad" (the default) — the port reads
  the controller directly through SDL2.
- **Desktop Mode:** run the AppImage; the same mappings apply.
- Player 1's keyboard keys also work with an external keyboard.

## Settings

`spidman.ini` is written beside the executable on Windows and in
`~/.local/share/SpiderMan/` on Linux. It holds the window scale, full-screen and
smooth-pixel flags, and the volume. It is re-read while the game runs when saved.

`spidman.eeprom` holds the board's 93C46 settings and bookkeeping (credits, high
scores, operator settings), exactly where the original board keeps them. Delete it to
return to factory defaults.

## Test scripts

The headless runner and `s32ref` take a text script; one event per line:

```text
FRAME ACTION [ARGUMENTS]
press INPUT [FRAMES]      # pressed for FRAMES (default 4), then released
hold INPUT                # held until release
release INPUT
dip SWITCH on|off         # switch 1-4
poke ADDRESS VALUE        # byte through the bus, hex
FRAME end
```

Inputs: `coin1 coin2 start1 start2 start3 start4 button1 button2 button3 button4
service test up1 down1 left1 right1 ... up4 down4 left4 right4`.

Example (`tests/controls/coin_start.txt`):

```text
120 press coin1
180 press start1
240 hold right1
300 press button1
900 end
```
