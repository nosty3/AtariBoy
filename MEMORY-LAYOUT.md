# SRAM and flash layout

The authoritative section placement is in `memmap_hybrid.ld`.
It is based on Pico SDK 2.1.1's copy-to-RAM linker script and preserves the SDK
startup copy symbols, while placing selected code and constants in `.cold_xip`.

## Flash slots

| Image | XIP address | Maximum size |
|---|---|---|
| Atari Boy | `0x10000000` | 752 KiB |
| Boot selector | `0x100BC000` | 16 KiB, including manifest at `0x100BFF00` |
| A8PicoCart | `0x100C0000` | 256 KiB |
| Shared filesystem | `0x10100000` | Remainder of the 16 MiB flash |

The applications run separately and each owns the full SRAM when active.
Firmware updates must never emit filesystem blocks.

## Execution and ownership

Core 1 (COM) owns the Atari bus; core 0 (EXE) runs the emulator, UI and storage.
COM code and all data needed for bus reads stay in SRAM. COM must not allocate,
log, read XIP or wait for EXE during normal bus service.

Cold UI, settings, state serialization, FatFs and USB routines execute from XIP.
The linker also places selected engine/state codecs, OSD/font and disk
optimization routines there. Hot GB execution, APU synthesis and video
conversion remain in SRAM. Writable data stays in SRAM regardless of code
placement. Consult the linker list before moving functions between regions.

Two `cart_words[8192]` GPIO-encoded images consume 64 KiB. Video and PCM have
independent publication tickets and acknowledgements. An acknowledgement for
one region does not release the other. The source Game Boy framebuffer is
160 x 144 at 2 bpp (5760 bytes). The presentation workspace has 80 additional
bytes for independent bottom OSD rows. The 192-line display repeats LMS
addresses rather than allocating a second expanded framebuffer.

`gb_s` and the 32 KiB cartridge RAM buffer are static. ROM bytes remain in flash;
sector maps need two SRAM bytes per 512 ROM bytes. ROM changes may temporarily
require both the old and candidate maps. Cartridge RAM above 32 KiB is rejected.
Stereo reserves two 1024-sample int16 FIFOs. USB borrows 61440 bytes from the
cartridge image storage; COM is not started in USB mode.

Each core has a separate 4 KiB stack in scratch SRAM. Heap ends at
`0x20040000`. Allocation failure must remain recoverable:
`PICO_MALLOC_PANIC=0`, `PICO_USE_OPTIMISTIC_SBRK=0`. Determine current free space
from the built ELF and runtime allocations, not historical size figures.
Avoid large automatic arrays in game-start and save/load call chains.

## Flash writes

FAT/state code may execute from XIP between hardware operations. Erase/program
wrappers and their dependencies execute from SRAM with interrupts disabled;
the SDK restores XIP through its RAM copy of boot2 before returning. Program
source buffers must also be in SRAM. COM continues to serve the published
image while EXE performs storage operations; do not use multicore lockout.

Publish the operation screen and wait for its acknowledgement and a complete
Atari frame before writes. Do not execute GB code or read its ROM during a
flash hardware operation. Restore the prior interrupt state afterwards.

The flash-map cache and USB/FAT access require exclusive ownership. Offline
optimization validates FAT16 (one or two FAT copies) and the physical map before
removing mappings for free clusters. Live sector addresses stay unchanged.
The existing metadata format is not power-loss transactional.

When upgrading the SDK, compare its startup linker script with all custom
scripts. Validate linked section addresses, stack usage and flash-safe paths.
Software tests do not establish physical bus timing or overclock stability.
