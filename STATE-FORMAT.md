# AtariBoy state format 2

Files: `/.saves/<ROM basename without extension>/SS_NNNN.sav`.
Slot 01–10; sequence has at least four decimal digits. All integer fields below
are unsigned little endian. The format is independent of host struct packing
and ARM/native `uint_fast*` widths. The pinned core is Peanut-GB
`8e656982f08663785794b84823d3e27f856fdb7f`, DMG only. Version 2 requires the fixed-size APU block and adds thumbnail/description metadata. Older state files are rejected. Battery save files are unchanged.

## Header (80 bytes)

| Offset | Size | Meaning |
|---|---:|---|
| 0 | 8 | ASCII `ATBOYST2` |
| 8 | 4 | Format version = 2 |
| 12 | 4 | Header size = 80 |
| 16 | 4 | Pinned core identifier = 0x8e656982 |
| 20 | 4 | Payload length = 23194 + cartridge RAM size |
| 24 | 4 | ROM size |
| 28 | 4 | Cartridge RAM size |
| 32 | 32 | SHA-256 of complete ROM file |
| 64 | 4 | CRC32 of payload AND appended metadata |
| 68 | 4 | CRC32 of header bytes 0..67 |
| 72 | 4 | Metadata length = 926 |
| 76 | 4 | Reserved, must be zero |

CRC32: reflected polynomial 0xedb88320, initial 0xffffffff, final XOR 0xffffffff.
Exact file length, version, core, sizes, ROM hash, header and payload CRC are
checked before loading. Reserved header bytes must be zero.

## Payload

First 29 fields are each encoded as a 32-bit value, in the exact following order:

```
gb_halt, gb_ime, gb_frame, lcd_blank, cart_is_mbc3O,
selected_rom_bank, cart_ram_bank, enable_cart_ram, cart_mode_select,
cpu_reg.f.reg, cpu_reg.a, cpu_reg.bc.reg, cpu_reg.de.reg, cpu_reg.hl.reg,
cpu_reg.sp.reg, cpu_reg.pc.reg,
counter.lcd_count, counter.div_count, counter.tima_count, counter.serial_count,
counter.rtc_count, counter.lcd_off_count,
display.window_clear, display.WY, display.frame_skip_count,
display.interlace_count, direct.interlace, direct.frame_skip, direct.joypad
```

Then byte arrays, without alignment padding:

| Payload offset | Size | Array |
|---:|---:|---|
| 116 | 5 | rtc_latched.bytes |
| 121 | 5 | rtc_real.bytes |
| 126 | 8192 | wram |
| 8318 | 8192 | vram |
| 16510 | 160 | oam |
| 16670 | 256 | hram_io |
| 16926 | 4 | display.bg_palette |
| 16930 | 8 | display.sp_palette |
| 16938 | N | Cartridge RAM, N from header |
| 16938 + N | 5760 | Full 160×144 packed framebuffer, Atari shade ordering |
| 22698 + N | 4 | engine_frames, little endian |

No ROM bytes, callback addresses, `direct.priv`, host pointers, padding, COM,
OSD or menu state are stored. Immutable cartridge configuration remains from
`gb_init` of the matching ROM. Error state is cleared on successful load.

## Operation contracts

EXE pauses on a frame boundary. COM continues to serve the published SRAM image.
Save: publish `SAVING...`, wait for ACK and a full Atari frame, stream to temporary
file, close/sync, verify the physical file, rename, sync, remove older matching
files in that slot. An intact foreign ROM header is never treated as our slot.
The global filename sequence includes all recognized filenames in the shared
basename directory to prevent collisions between different ROM hashes.

Load: build a small physical sector map, validate all bytes, then restore from
that same immutable flash mapping without further fallible filesystem reads.
This requires exclusive FAT ownership and no flash writes during validation or
restoration. Save/load routines are EXE-only and non-reentrant.

FAT/flash metadata are not power-loss transactional. A completed replacement is
verified before deleting the old file, but power loss during a sector erase can
still damage filesystem metadata. The format CRC is corruption detection, not
an authentication mechanism. A future core/layout change must change the format
or explicitly implement a converter.

## Metadata after the emulator payload

896 bytes: 53 x 64 thumbnail, 14 bytes per row, 2 bits per pixel, MSB first.
The three unused pixels at the right of each packed row are zero. Source pixels
come from the clean GB framebuffer using x=floor(x_out*160/53),
y=floor(y_out*9/4), equivalent to taking every third row of the 192-line view.
Then 30 ASCII/ATASCII printable description bytes padded with spaces, no terminator.
File length: **24200 + cartridge RAM size**. Header payload length excludes metadata;
payload CRC includes it. Metadata is not restored into the emulated Game Boy.
Editing a description preserves the sequence number. Copy/paste creates a new
sequence number and retains the copied thumbnail/description. Temporary clipboard
is a complete immutable file, cleared on exit; it is not a save slot.

## APU block (required)

492 bytes appended after engine_frames, before thumbnail metadata.

Four channels, each with 27 little-endian uint32 values:
```
enabled, powered, on_left, on_right, volume, volume_init,
freq, freq_counter, freq_inc, val,
len.load, len.enabled, len.counter, len.inc,
env.step, env.up, env.counter, env.inc,
sweep.rate, sweep.shift, sweep.down, sweep.freq, sweep.counter, sweep.inc,
union0, union1, union2
```
Union fields: channels 1/2 = duty, duty_counter, zero; channel 3 = sample, zero,
zero; channel 4 = lfsr_reg, lfsr_wide, lfsr_div. Signed val is encoded as its
32-bit two's-complement representation. Then three uint32 values: vol_l, vol_r,
sample phase accumulator; then the 48 bytes of FF10–FF3F.

This serializes semantic fields, not struct memory/padding or pointers.
The fixed-point APU configuration is AUDIO_SAMPLE_RATE=18635, FREQ_INC_MULT=105;
312 output samples per 70224 GB cycles. Altering these semantics requires a
new extension schema/conversion. Output PCM buffers are not saved.

