# Atari Boy

**Play Game Boy games on your Atari XL/XE with an A8PicoCart.**

Atari Boy is an emulator of the original monochrome Nintendo Game Boy for Atari XL/XE with an A8PicoCart.

The firmware also includes **A8PicoCart**, so you can choose between the Game Boy emulator and the cartridge’s Atari software loader at power-on.

## Features

- Original Game Boy (DMG) emulation using Peanut-GB.
- Game browser with folders and long filenames.
- Four display modes: line skipping, a movable cropped view, full 144-line output and an expanded 192-line view.
- Adjustable colors and brightness.
- POKEY sound (15 kHz, 6-bits samples), with automatic detection of mono or dual-POKEY stereo hardware.
- Configurable joystick and **Joy2B+** button mappings.
- 10 save-state slots per game, with thumbnails, descriptions and a built-in state manager.
- Battery-backed game RAM saving.
- Global and per-game settings.
- Optional loading of the latest state and startup of the last played game.
- Standard **250 MHz** and optional **300 MHz Turbo** modes.
- USB file transfer — no separate application needed.
- Built-in help, FPS display and diagnostic counters.

## Requirements

- An **Atari XL/XE** computer.
- An **A8PicoCart-compatible cartridge** using the RP2040 board with **16 MiB flash**.
- A computer and USB data cable for installing firmware and copying games.
- Game Boy ROM files in **`.gb`** format.
- A joystick (Joy2B+ joystick is recommended).

PAL/SECAM and NTSC display timings are supported. Stereo requires a second POKEY; standard Atari hardware uses mono sound.

This firmware targets the A8PicoCart hardware. A standard Raspberry Pi Pico board is not a drop-in replacement.

## Quick start

### 1. Install the firmware

1. Turn off the Atari and remove the cartridge.
2. Download **[atariboy-v090b-dual.uf2](Build/atariboy-v090b-dual.uf2)**.
3. Hold the RP2040 board’s **BOOTSEL** button while connecting it to your computer by USB.
4. Release the button when the **RPI-RP2** drive appears.
5. Copy the UF2 file to that drive.
6. Wait for the drive to disappear as the board restarts.

The UF2 contains Atari Boy, the boot selector and A8PicoCart. Normal firmware updates preserve the cartridge’s game files, saves and settings.

### 2. Copy your games

1. Connect the cartridge to your computer by USB **without holding BOOTSEL**.
2. Open the cartridge’s USB storage drive.
3. Copy your `.gb` files onto it. You can organize games into folders.
4. Safely eject the drive and disconnect USB.

Use ROM files you are entitled to use. **No Game Boy games are included in this repository.**

### 3. Start playing

1. With the Atari switched off and USB disconnected, insert the cartridge.
2. Turn on the Atari.
3. Press and release **Space** or the joystick’s **Fire** button to dismiss the title screen.
4. Choose a game with Up/Down and press **Return** or **Fire**.

To start the included **A8PicoCart loader** instead, hold **A** while turning on the Atari.

### 4. Options

1. While playing, try the HELP and OPTION keys.

## Controls

The default joystick mapping uses directions for movement and **Fire for Game Boy A**. **Space acts as Game Boy B**. You can change button assignments in the **Ctrl** options page (press Option key for options screen).

| Key | Action during gameplay |
|---|---|
| **HELP** | Open the built-in help screen |
| **OPTION** | Open Options |
| **START / SELECT** | Game Boy Start / Select |
| **Esc** | Open the game browser |
| **P** | Pause or resume |
| **R** | Reset the current game |
| **M** | Toggle sound |
| **S / L** | Save / load the selected state slot |
| **Z / X** | Select the previous / next state slot |
| **G** | Open the save-state manager |
| **Left / Right** | Switch display mode |
| **Up / Down** | Move the cropped view |
| **C / V** | Change the color hue |
| **N** | Switch Fast / Quality audio |
| **T** | Toggle Standard / Turbo clock |
| **F** | Toggle FPS display |
| **Ctrl+D** | Toggle detailed diagnostics |

In the browser, **Left** opens the parent folder and **Esc** resumes the loaded game.

## Options

Press the Atari’s **OPTION** key to open the **Graph, Sound, Emu, State, Ctrl, Info and Disk** pages.

Use Up/Down to select a field and Left/Right to change it. Select the tab row to switch pages.

- **S** — save settings for the current game.
- **L** — load settings for the current game.
- **G** — load global settings.
- **OPTION** — save global settings and return.
- **Esc** — cancel changes made since entering Options or the last successful per-game save.

The **Info** page explains these commands. Changes made through gameplay shortcuts are not automatically saved.

## Saving your progress

**Save states** capture the running game and can be saved or loaded with S/L. Press G to manage slots, previews and descriptions.

**Battery saves** preserve the RAM used by games with built-in saving. After saving inside a game, **press Esc and wait for the browser before switching off the Atari**. Battery data is also checked before changing games.

Loading an older save state restores its cartridge RAM too, which can roll back the game’s built-in save progress.

## Compatibility and limitations

Atari Boy emulates the original Game Boy, not Game Boy Color or Game Boy Advance. Game compatibility and speed depend on the title; full-speed operation is not guaranteed in every scene.

Cartridge RAM is limited to **32 KiB**. Games requiring more are rejected.

## Building and development

See **[BUILDING.md](BUILDING.md)** for dependencies and compilation instructions.

Further documentation:

- [Technical context](TECHNICAL-CONTEXT.md)
- [Memory layout](MEMORY-LAYOUT.md)
- [Save-state format](STATE-FORMAT.md)
- [Runtime diagnostics](DIAGNOSTICS.md)
- [Host regression tests](tests/README.md)

## Credits and licenses

Atari Boy builds on **Peanut-GB**, **minigb APU**, **A8PicoCart**, the **Raspberry Pi Pico SDK**, **TinyUSB** and **FatFs**. Atari-side code is assembled with **MADS**.

See [LICENSE](LICENSE), the [component notices](licenses/) and the [font attribution](assets/FONT-NOTICE.txt) for licensing and credits.
