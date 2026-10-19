# Dual-boot startup contract

Atari Boy occupies `0x10000000` (752 KiB), the selector `0x100BC000` (16 KiB),
and A8PicoCart `0x100C0000` (256 KiB). The shared filesystem starts at
`0x10100000`. The DUL1/version 1 manifest at `0x100BFF00` stores both original
application stack pointers and reset entries.

`merge.py` keeps Atari Boy's boot2 and patches only its reset-vector word to
`selector_entry`. Application launch uses the original reset entry from the
manifest, not the patched vector. Each touched 4 KiB sector is emitted as all
16 UF2 pages to satisfy RP2040-E14; unused whole sectors and FAT are omitted.

On a cold start the selector initializes its own SDK. Core 1 remains in boot
ROM. No PHI2 for 100 ms selects Atari Boy, which provides USB storage. With an
Atari attached, a small ROM copies a stub to page $06. It disables IRQ/NMI/DMA,
sets SKCTL=$03 and samples the held **A** key for three frames using SKSTAT
$D20F bit 2 and KBCODE $D209 masked to six bits. A latched key code alone does
not select A8PicoCart. Command $A1FE=$47 selects Atari Boy; $A1FE=$A8 selects
A8PicoCart. Selection does not use page $D5.

Handover requires a full RP2040 reset. `launch` checks the manifest, releases
the data bus, disables interrupts and records a single-use token in watchdog
scratch registers 0..3: `0x53454C31`, choice, complemented choice, complemented
magic. Registers 4..7 belong to boot ROM/SDK. `watchdog_reboot(0,0,0)` resets
the hardware. Before CRT startup, `selector_entry` consumes and validates the
token, reset reason and manifest, then enters the selected original CRT.
Without a valid token it starts the selector SDK. Do not jump into a second
SDK from a running first SDK.

While Pico resets, Atari waits in RAM for all six target vector bytes at
$BFFA..$BFFF. It calls INIT through a RAM trampoline, restores DMA/NMI/status
and calls RUN. Regenerate the selector after changing either Atari boot ROM.
Atari RESET alone does not change the selected application; power-cycle to
select again.

The embedded A8PicoCart application comes from `ascrnet/A8PicoCart`, revision
`092397bd6d6056e4c1e9652e380e8619fdaf2193`. Integration uses its own slot linker,
a relative 100 ms PHI2 detection window, Pico SDK 2.1.1 compatibility and disabled
USB/UART stdio. Its cartridge handling and shared filesystem format are retained.
A root `A8PICOCART.ROM` file can override its built-in Atari menu.
