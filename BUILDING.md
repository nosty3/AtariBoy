# Building Atari Boy 0.90b

The release is dual-boot firmware for the 16 MiB RP2040 A8PicoCart board.
Hold **A** while powering on the Atari to select A8PicoCart; otherwise Atari Boy
starts. A USB-only connection starts Atari Boy's mass-storage mode. A standard
Raspberry Pi Pico does not expose all GPIO pins required by this cartridge.

## Requirements

- Raspberry Pi Pico SDK **2.1.1**, including its TinyUSB submodule.
- GNU Arm Embedded GCC/G++ (tested with **13.2.1**), newlib and C++ headers.
- CMake, Ninja, Bash and Python 3 with Pillow.
- Mad Assembler (MADS), tested with **2.1.7**.

Install the tools and put the compiler, CMake and Ninja on PATH. For example,
prepare the SDK with:

```sh
git clone --branch 2.1.1 https://github.com/raspberrypi/pico-sdk.git
git -C pico-sdk submodule update --init lib/tinyusb
python3 -m pip install Pillow
```

No Game Boy ROM is needed for compilation. None is included or embedded.

## Build the complete release

From the repository root:

```sh
export PICO_SDK_PATH=/absolute/path/to/pico-sdk
export MADS=/absolute/path/to/mads
bash dual/build.sh -DPICO_NO_PICOTOOL=1
```

The option disables the SDK's optional picotool download. The project's own
merger creates the final UF2 from the three ELF/BIN outputs, so picotool is not
needed for the dual build.

The script regenerates the fonts, title graphics, PAL/NTSC Atari programs and
selector ROM, builds all three RP2040 applications, and creates:

- `Build/atariboy-v090b-dual.uf2`: complete flashable release.
- `out/atariboy/`, `out/selector/`, `out/a8picocart/`: compiler outputs.
- `out/firmware/`: ELF/BIN copies and `layout.json` with sizes and UF2 SHA-256.

`Build` contains only the current release UF2. Intermediate files are generated
under `out`, `generated` and `dual/selector/generated`, and are ignored by Git.
`BUILD_DIR`, `SELECTOR_BUILD_DIR` and `A8_BUILD_DIR` can override compiler output
directories. Additional arguments are forwarded to CMake for all three builds.

For an emulator-only development build, use `bash build.sh`. With picotool
available, the SDK also produces `out/atariboy/atariboy.uf2`. The published
release is the combined image produced by `dual/build.sh`.

## Flashing and data

Enter the RP2040 BOOTSEL mode and copy the release UF2 to its boot drive.
The merged image writes only firmware below flash offset 1 MiB; the shared
filesystem starts at 1 MiB. Each touched 4 KiB sector is emitted in full to
satisfy RP2040-E14. Do not replace this packaging with naive concatenation.

Use the normal USB storage mode to copy your own legally obtained ROM files.
Eject the drive before disconnecting. Atari and host USB access require separate
power cycles and must not own the filesystem concurrently.

## Source map

| Location | Purpose |
|---|---|
| `main.cpp` | RP2040 cartridge bus and buffer publication |
| `app.cpp`, UI modules | Application state, menus and emulation scheduling |
| `engine.c`, `vendor/` | Peanut-GB core and minigb APU |
| `audio.c`, `cadence.hpp` | PCM generation, resampling and frame scheduling |
| `storage/`, `saves.c`, `settings.cpp` | Flash filesystem, saved states and settings |
| `atari/`, `build_*.py`, `pack_atari.py` | Atari loaders, display lists and audio players |
| `dual/` | Boot selector, A8PicoCart application and UF2 merger |
| `tests/native/` | Self-contained host regression tests |
| `licenses/`, `assets/FONT-NOTICE.txt` | Component attribution and supplied notices |

The optional menu source in `dual/upstream-atari-source/A8PicoCart.asm` is retained
for modifications; the firmware build uses `dual/a8picocart/rom.h`. Assemble the
menu with MADS and regenerate that byte array if changing the embedded menu.
The selector build derives its handover vectors from this array.

See `MEMORY-LAYOUT.md`, `STATE-FORMAT.md`, `DIAGNOSTYKA.md`,
`dual/ARCHITEKTURA.md` and `tests/README.md` for development details.
