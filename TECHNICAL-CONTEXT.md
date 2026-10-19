# Atari Boy technical context

This is the maintainer's guide to **v0.90b dual**. It describes the current
implementation and the contracts that must survive future changes. Source code
is authoritative where an implementation detail differs from this guide.
Historical experiments, release-by-release results and private test data are
not part of the repository's technical specification.

## 1. Scope and documentation

Atari Boy emulates the original monochrome Game Boy (DMG) on an RP2040
A8PicoCart board. Atari XL/XE provides the display, POKEY audio and controls.
The combined firmware also includes a separately built A8PicoCart application,
selected by holding **A** while powering on the Atari. Without PHI2, Atari Boy
starts its USB mass-storage mode. Atari RESET alone does not select another
application or reset the RP2040 application state.

The emulator uses Peanut-GB revision
`8e656982f08663785794b84823d3e27f856fdb7f`, with local APU cycle hooks, and a
modified minigb APU. It does not emulate GBC or GBA. ROM-only/ROM+RAM and
MBC1/2/3/5 cartridge families are supported within the core's capabilities;
rumble cartridge types and GBC-only files are rejected. Compatibility with
every Game Boy title is not guaranteed.

| Document | Responsibility |
|---|---|
| [BUILDING.md](BUILDING.md) | Dependencies, build commands and source directory map |
| [MEMORY-LAYOUT.md](MEMORY-LAYOUT.md) | SRAM/XIP placement, stack/heap and flash-write rules |
| [STATE-FORMAT.md](STATE-FORMAT.md) | Exact saved-state header, payload and APU schema |
| [DIAGNOSTICS.md](DIAGNOSTICS.md) | Runtime counters and interpretation |
| [dual/ARCHITEKTURA.md](dual/ARCHITEKTURA.md) | Selector, reset token and handover contract |
| [tests/README.md](tests/README.md) | Retained self-contained host regression suites |

Build the combined release with `bash dual/build.sh -DPICO_NO_PICOTOOL=1`
after setting `PICO_SDK_PATH` and `MADS` as described in BUILDING.md.
The tested toolchain is Pico SDK 2.1.1, Arm GCC/G++ 13.2.1 and MADS 2.1.7.
Python/Pillow generates assets and Atari code. No Game Boy ROM is needed.
The current release is `Build/atariboy-v090b-dual.uf2`; generated intermediates
are ignored by Git. Build scripts live in `tools/` and `dual/`.

## 2. Hardware and clocking

The target is the purple RP2040 clone with **16 MiB flash and GPIO 0–29
exposed**, as used by A8PicoCart. A standard Pico does not expose all required
pins. The table describes the software interface, not electrical certification
of a board or a substitute for its hardware documentation.

| RP2040 GPIO | Atari signal | Direction at RP2040 |
|---|---|---|
| 0–12 | A0–A12 | Input |
| 13–20 | D0–D7 | Bidirectional |
| 21 | /CCTL | Input; unused by Atari Boy's protocol |
| 22 | PHI2 | Input |
| 23 | R/W | Input; high means Atari reads |
| 24 / 25 | /S4 / /S5 | Window select inputs |
| 26 / 27 | RD4 / RD5 | Cartridge-presence outputs |
| 28–29 | Unused | — |
| RUN | RP2040 reset | Not the Atari RESET signal |

A slash denotes active-low. `RD4=0, RD5=1` selects the 8 KiB cartridge window
at `$A000–$BFFF`. Data is `(pins >> 13) & 255`; the window address is
`pins & 8191`; the data-pin mask is `0x001FE000`. A prepared GPIO word is
`RD5 | (uint32_t(byte) << 13)`.

Atari-mode clocks are **250 MHz at 1.10 V**, or **300 MHz at 1.15 V** in Turbo.
The board uses `PICO_FLASH_SPI_CLKDIV=6`, giving approximately 41.67/50 MHz
flash clocks. Both CPU rates exceed the nominal RP2040 specification; software
models cannot qualify overclock stability on individual boards. The selector
and A8PicoCart use 250 MHz. USB mode uses the SDK's normal CPU clock.

### Safe clock transitions

`Settings.turbo` is the requested setting; `clock_turbo` is actual runtime
state and is not serialized. Startup begins at 250 MHz. Saved settings or
changes made in Options take effect when returning to the game loop, where
the Atari-side handshake is available. **T** toggles the request during play.

EXE sets request/busy; Atari code in RAM disables DMA/NMI, silences POKEY and
writes `$A160=1`. COM completes that write, releases D0–D7 and parks in SRAM.
EXE raises voltage and waits 1 ms before increasing the clock; when reducing
speed, it lowers the clock first, then voltage, and waits 1 ms. COM resumes
after EXE signals completion. Atari makes no cartridge access for 512 WSYNC
lines before checking busy and resynchronizing the audio loop.

Do not change the PLL while COM is serving the bus: both cores temporarily
pass through a slower SDK clock. Do not trigger this from title/Help/Options
loops. The guard interval is not recovery from unstable overclocking. A saved
Turbo request can be removed by deleting the relevant CFG through USB.

## 3. Core ownership and bus invariants

**Core 1 = COM; core 0 = EXE.** COM exclusively owns bus GPIO. EXE initializes
SRAM before enabling the cartridge, then runs emulation, UI, rendering and
storage. Mutable Game Boy state belongs only to EXE. Atari-mode interrupts
are disabled after initialization; FIFO is used for startup synchronization.

- ROM/display reads require `/S5=0` and `R/W=1`. The address may change while
  selection remains active. Prepare ordinary read data before PHI2 high;
  release data outputs when selection is lost or Atari begins writing.
- Register writes require the correct selection, address, R/W and PHI2.
  Capture the last valid high-phase sample. If qualification is lost before
  observing PHI2 low, retain that sample and drain the remaining high phase,
  then commit once. Do not count a re-selection as a second write.
- Keep COM and all its dependencies/data in SRAM. Normal bus service must not
  allocate, log, wait for EXE or fetch Game Boy ROM bytes from XIP.
- `read_route[1024]` classifies addresses in eight-byte groups and selects
  video, audio or register bases without a range-check cascade per read.
  Keep this path short; source-level functional equivalence alone does not
  establish equivalent physical timing.
- `$A100–$A1FF` is Atari Boy's control window. `$D500–$D5FF` has no protocol
  aliases. This restriction concerns Atari Boy, not the separate A8PicoCart.

Two GPIO-encoded `cart_words[8192]` buffers consume 64 KiB SRAM. Video and PCM
use disjoint regions with **independent tickets, acknowledgements and front
pointers**. EXE writes only a free region, then publishes it with release
semantics. At the `$A137` input commit, COM switches each pending region and
acknowledges it independently. A video ACK never releases PCM, or vice versa.
Never reset tickets asynchronously or overwrite a pending region.

Input snapshots use an even/odd sequence and atomic loads/stores with barriers.
Do not add locks or atomic read-modify-write operations to the COM hot path.
Crucially, buffer ACK must not depend on validation of input packet order or
completeness: such coupling can halt the entire application.

## 4. Boot and loader protocol

The boot ROM starts at `$B000`; `$BFFA–$BFFF` holds cartridge vectors. It
turns off DMA/NMI, writes `$11` to `$A1B0`, downloads a 256-byte loader to
`$0600–$06FF`, verifies CRC-16/CCITT-FALSE (initial `$FFFF`, polynomial `$1021`)
and jumps to it. `$0700/$0701` are temporary CRC workspace.

The RAM loader distinguishes a 312-line PAL/SECAM frame from 262-line NTSC
using VCOUNT, then writes 50 or 60 to `$A1B2`. EXE copies the selected packed
XEX from XIP to a single SRAM buffer before publishing the matching reply.
Only then does the loader request the program stream.

| Register | Contract |
|---|---|
| `$A1B0`, write `$11` / `$22` | Restart loader / program stream |
| `$A1B0`, read | Next byte; `$FF` after EOF, without advancing |
| `$A1B1`, read bit 0 | Byte available; status does not consume data |
| `$A1B2`, write/read | PAL/SECAM=50 or NTSC=60; reply means SRAM copy is ready |

A stream byte may be driven in low PHI2, but is consumed only after a qualified
access reaches high PHI2 and ends. Repeated GPIO sampling is not repeated reads.
COM expands packed program data one prepared byte at a time; ordinary
video/PCM reads do not run the decompressor. No stream data comes from XIP.

The standard segmented XEX supports repeated `$FFFF` markers, inclusive segment
ends, INITAD after each segment and RUNAD at EOF. INITAD is cleared before a
segment; writing RUNAD alone does not end the stream. INIT may alter A/X/Y and
zero page but must preserve the active loader page and stack, and must not
restart its stream without preserving state. RUNAD is required. This is a
loader for the cooperating application, not a guarantee for arbitrary XEXs.
Loader CRC failure shows background `$28`; malformed/truncated XEX shows `$46`.

`tools/pack_atari.py` uses literal tags 0–127, a 45-byte stereo sample template
with incremented addresses at tag 128, and new template addresses at tag 129.
The build checks pack/unpack equality. PAL and NTSC share assembly sources.

## 5. Display memory and Atari execution

CPU and ANTIC see a **full, side-effect-free framebuffer in the cartridge
window**. COM does not infer rows from read order or move a 40-byte window.
Each display-list row has its own LMS, and no 40-byte row crosses a 4 KiB
boundary. Restore the display list every frame, not only on mode changes.

| Atari address | Use |
|---|---|
| `$A000` | Mode: 0 browser, 1 $0D game, 2 Graph, 3 text UI, 4 $0E/144, 5 $0E/192, 6 title, 7 state manager |
| `$A001–$A004` | COLBK, COLPF0, COLPF1, COLPF2; published with video |
| `$A005` | Two-POKEY playback flag |
| `$A100–$A1FF` | Live communication registers |
| `$A200–$AFE7` | Source rows 0–88, 40 bytes each |
| `$B200–$BA97` | Source rows 89–143, 40 bytes each |
| `$BAA8–$BACF`, `$BAD0–$BAF7` | Independent bottom OSD rows in $0E/192 |
| `$BB00`, `$BD00`, `$BE40` | Left PCM planes, 312 bytes each |
| `$B000–$B1FF` | Boot code initially; right PCM after startup |
| `$BFFA–$BFFF` | Cartridge vectors initially; reused after startup |

Right PCM totals 936 bytes across `$B000–$B1FF`, `$A008–$A0FF`,
`$BF78–$BFFF`, `$AFE8–$AFFF` and `$BA98–$BAA7`. Do not overwrite boot/vectors
before startup finishes. These regions belong to the audio mailbox.

The clean GB image is 160×144, packed 2 bpp (5760 bytes). The engine masks
pixel indices to two bits and inverts shade order: GB 0 is white, Atari 0 black.
`src/video.cpp` supports four mappings:

| Setting | Display mapping |
|---|---|
| 0 | ANTIC $0D, 108 rows by skipping every fourth source row |
| 1 | ANTIC $0D, a movable 108-row crop; initial top=18 |
| 2 | ANTIC $0E, all 144 rows, one TV line per row |
| 3 (default) | ANTIC $0E, all 144 rows, every third row repeated by LMS to give 192 lines |

`config/config.json` controls $0D height. The generator accepts 64–120 in
steps of four, but increasing the released 108-row height also requires
checking/repositioning audio-loop input tasks outside visible DMA. It is not
an independently validated UI option. Full modes keep fixed geometry.

Bottom OSD uses 20 characters for two seconds, rendered after conversion from
a precomputed outlined font. In $0E/192, source rows 140 and 143 have independent
copies so the bottom message occupies eight physical lines without duplication.
The top FPS/debug overlay still follows normal display scaling. The clean GB
framebuffer never contains OSD, and OSD is not saved in game states.

Atari RAM holds the main code/display lists around `$2000–$2FFF`, mono audio
at `$3000`, Options/Graph and state UI around `$4000–$46FF`, clock handover on
page `$47`, Graph font at `$4800`, menu charset at `$4C00–$4FFF`, and unrolled
stereo code from `$5000` to below `$A000`. Audio reserves ZP `$F0–$F4`.
Generated MADS listings are authoritative for exact segment ends.

The browser is 40×24 ANTIC $02 text. Graph uses mixed $02/$05 lines and DLI
through VDSLST `$0200/$0201`; color bars use PF0, with a black border. The title
combines 64 hires lines and 16 text rows; its 320×64 two-color logo is packed
from `assets/title-logo.png`. Menu glyphs are rasterized from the supplied
Early GameBoy font while retaining Atari graphics; preserve its attribution.

## 6. Input packets and UI boundaries

Game/browser loops use WSYNC with OS IRQ/NMI disabled. Options/Help/title use
separate raster loops; DLI is enabled where required. Do not reintroduce OS
VBI processing that can overwrite palette, DMA or display-list state.

The 24-byte packet is staged at `$A120–$A137`:

| Address | Value |
|---|---|
| `$A120/$A121`, `$A122–$A123` | Joystick 0 directions/fire, POT0–1 |
| `$A124/$A125`, `$A126–$A127` | Joystick 1 directions/fire, POT2–3 |
| `$A128/$A129` | Last six-bit key code / key-event counter |
| `$A12A` | Held key; `$FF` means none |
| `$A12B–$A12E` | HELP, START, SELECT, OPTION; 1 means pressed |
| `$A12F–$A131` | Raw PORTA, SKSTAT, KBCODE |
| `$A132–$A135` | POT4–7 |
| `$A136` | Reserved, zero |
| `$A137` | Frame number modulo 256; writing commits input and buffer switches |

Joystick direction bits 0..3 are up/down/left/right, active-low; fire is also
active-low. SKSTAT is read at `$D20F`, KBCODE at `$D209`. `$D20B` is POTGO on
write. SKSTAT bit 2 is low for a held key; raw KBCODE bits 7/6 are Ctrl/Shift.
Held key and latched last key must not be treated as the same thing.

Commands trigger on key changes, not every frame. Modifier changes alone do
not retrigger ordinary shortcuts. OPTION must first be observed as exactly 0
before a 1 press is accepted; `$FF` neither presses nor arms it. START/SELECT
also require exactly 1. A released key must not insert the stale KBCODE into
the state-description editor.

Paddle conversion can need 228 TV lines. PAL reads the preceding measurement
before starting the next one. NTSC PCM starts POTGO at sample 6 and reads POT0–7
at samples 240–247. The first two packets use neutral 228. Joy2B+ is recognized
only after a channel has produced a value below 228; afterwards 228 means
pressed. A held-at-start button may need release first. There is no automatic
analog-paddle/Joy2B+ distinction.

Other registers: `$A13C` reports height, `$A13D` reports detected mono/stereo,
`$A13E` reports nominal 50/60 Hz, and `$A13F` is `$FF` for a new audible PCM batch
of the current epoch, otherwise zero. `$A140–$A15F` holds eight little-endian
u32 debug values: Atari frames, video repeats, latest/max EXE work time, budget
overruns, reserved zero, GB error and completed GB frames. `$A160/$A161` are
clock request/acknowledgement and busy. See DIAGNOSTICS.md for overlay counters.

Title dismissal requires a fresh press and release of Space/Fire0 and a stereo
report; only then does optional last-game startup run. Esc resumes a loaded
game from the browser; Left goes to the parent directory. The browser retains
15 entries, directories first, case-insensitive ordering, FAT names up to 255
characters and paths up to 1023 bytes. Save/options icons are cached basename
hints, not content-validity checks; never hash whole ROMs to draw these icons.

Game shortcuts include P pause, R reset, M sound, F FPS, Ctrl+D diagnostics,
T clock, N audio preset, C/V hue, S/L state save/load, Z/X slot, G state manager,
HELP help and OPTION options. Keyboard arrows change mapping or move the crop;
joysticks do not move the view. Crop movement follows Atari frame-number deltas,
not GB execution count, and must not catch up time spent in menus.

Reset reinitializes CPU/APU while preserving the current ROM map, cartridge
RAM and RTC; it does not reload the game or auto-load a state. Menus pause GB
execution while retaining game state and the user's manual-pause setting.
State management supports ten slots, thumbnails, 30-character descriptions,
copy/paste and deletion. File mutations are deferred until the operation
screen has been published, not performed inside input/rendering code.

## 7. Emulation cadence and audio

The core executes a complete GB frame, including CPU/PPU/timers/APU work even
when the frame will not be presented. ROM files are 32 KiB–8 MiB, powers of two,
with checked size, header checksum and cartridge codes before `gb_init`.
The active `gb_s` and 32 KiB cartridge RAM buffer are static; larger RAM requests
are rejected. MBC2 uses 512 bytes. New RAM is filled with `$FF`. Validate a
candidate and allocate its ROM-sector map before replacing a paused game.

Full-speed cadence uses the exact GB period `70224/4194304` seconds. Each EXE
step executes at most one GB frame and services inputs/video/PCM between
steps. It neither groups two frames nor waits for video ACK. After sustained
lateness, the schedule restarts rather than running an unbounded catch-up burst.
PAL's optional 50-fps mode executes one GB frame per new Atari packet, slowing
both gameplay and pitch. NTSC always uses the GB clock, not a strict 1:1 lock
to Atari frames, whose frequency differs slightly.

The APU hook receives instruction cycles, including HALT, rather than generating
a whole frame from final register values. It produces 312 signed 16-bit samples
per 70224 GB cycles, with `AUDIO_SAMPLE_RATE=18635`. Mono mixes left/right;
stereo keeps them separate. Each side has a 1024-sample FIFO. Cadence consumes
312 output samples per PAL/SECAM frame or 262 per NTSC frame, at one sample per
TV line. Audio memory planes remain 312 bytes even in NTSC.

Fast uses nearest-sample resampling; Quality (default) adds Q16 interpolation.
Both retain the wave-channel volume fix. Effects masks are 4/6; averaging is
not enabled in these presets. Volume and final quantization occur after
resampling; signed PCM must not be prematurely reduced in the FIFO. Quality
has no interpolation effect in PAL's one-frame-per-packet mode. There is no
anti-alias filter. Disabled channels skip waveform/mix work while retaining
length/envelope/sweep/NR52 behavior; their waveform phase freezes.

The final DAC has 61 nominal sums of four POKEY volume-only channels. Encoded
zero means silence at level 30. For level L, q=L/4, r=L%4:
`pair=q+(r==3)`, `c2=q+(r==1||r==2)`, `c3=L-2*pair-c2`.
The four volumes are pair, pair, c2, c3, each with bit `$10` set. This does not
guarantee ideal analog linearity or six effective bits.

PCM publication is independent of video. FIFO underflow preserves partial
source data; overflow discards the oldest excess. Pause/UI/start/load/reset
and sound-mode transitions clear transport data and advance its epoch. COM
acknowledges stale pending tickets but does not play their old-epoch audio.
No fresh PCM means silence, not replay of the preceding batch. Transport
buffers, epochs and output settings do not belong to a saved GB state.

Stereo detection runs on Atari before normal input: configure the second
POKEY, wait two WSYNCs and AND 256 RANDOM reads from the first. Stock Atari
mirrors `$D21x` onto `$D20x`; forced stereo would overwrite the same chip.
`detected_stereo` is 255 until reported, then 0/1. Hardware detection always
wins over legacy configuration, and there is no user output selector.

The stereo player uses unrolled absolute accesses: six loads and eight AUDC
stores per sample. Keep input work outside visible DMA. On audible-to-silent
transitions, silence writes must occur **after the first WSYNC of the new
frame**. Moving them before it overruns the boundary and accumulates raster
drift. PAL must retain 312 lines and NTSC 262 through sound/silence/menu
transitions. Instruction-start timestamps are not bus-write timestamps when
WSYNC/DMA stalls occur. Measure actual writes when checking audio timing.

## 8. Filesystem, states and settings

USB and Atari own the filesystem in separate operating modes. Do not mount
FAT locally while the USB host can write it. Normal USB has no COM core.
After using host USB, eject and power-cycle into Atari mode.

The shared flash translation format begins at flash offset 1 MiB, has signature
`RHE!FS30`, and exposes 30716 logical 512-byte sectors. Fifteen 4 KiB physical
sectors hold the 16-bit mapping. Each entry is `physical_4K_sector*8 + page`;
zero means logical zeros. Fresh Atari Boy FAT16 uses a 30000-sector superfloppy
with 2048-byte clusters, while USB retains the upstream reported geometry.
An existing filesystem is not automatically reformatted; initialization is
allowed only when the metadata region is erased.

ROM access follows a precomputed physical-sector map: two SRAM bytes per
512 ROM bytes. Each callback reads directly from
`0x10100000 + (map[address >> 9] << 9) + (address & 511)`.
Do not put `f_read`/`f_lseek` in the per-byte GB callback. Flash writes must
preserve live physical addresses; any future relocating collector must also
rebuild active ROM maps.

USB borrows the cartridge buffer for the full map. Atari writes use a 4 KiB
map-page cache with sync on replacement/end. USB idle flushing runs in the
main loop, not an alarm interrupt; synchronize/eject also flushes. Retain
bounds/error handling. FAT free space and free physical flash mappings differ;
deleting files does not by itself discard all obsolete mappings.

### Saved states and battery RAM

States are `/.saves/<ROM basename>/SS_NNNN.sav`, slots 01–10. Sequence numbers
increase across the directory and have at least four digits. SHA-256 of the
entire ROM distinguishes equal basenames; foreign states are not loaded or
deleted as this game's slots. Renaming a ROM requires moving its save directory.

ATBOYST2 has an 80-byte header, explicit little-endian state fields, a mandatory
492-byte APU block, 896-byte thumbnail and 30-byte description. Total size is
24200 bytes plus cartridge RAM. See STATE-FORMAT.md for exact offsets and CRCs.
Never dump `gb_s`: it contains pointers, padding, bitfields and ABI-dependent
types. Callback pointers and immutable configuration come from the current ROM.

Saving streams a temporary file, closes/syncs it, validates it and renames it
before removing older matching slot files. Loading validates the entire file
through an immutable physical map before changing emulated state. There is no
second full state buffer. CRC detects corruption, not authenticity. Unknown
core/layout changes need an explicit format/version decision.

Battery files `battery-<SHA256>.sav` in the same directory store cartridge RAM
and RTC separately, using ATBOYBA1. Supported battery cartridge types are
3, 6, 9, 15, 16, 19 and 27. Dirty content is saved on **Esc to the browser** and
checked again before switching games. It is not continuously flushed during
play. Wait for the browser before powering off. Loading a state also restores
RAM/RTC, so a later battery save persists that restored content.

### Configuration records

Global settings: `/.options/global.cfg`, magic ATBOYOP1, payload length u16,
TLV records `[id:u8, length:u8, value]`, and CRC32 over header/payload.
Current output is 95 bytes with 27 one-byte values. Unknown fields are ignored;
invalid lengths/CRC reject the record. Missing values use defaults, or inherited
global values for game settings.

| Field IDs | Meaning |
|---|---|
| 1; 2–5 | Hue; four brightness values |
| 6–10 | Sound, volume, PAL full speed, load latest state, boot last game |
| 11–16 | Legacy control migration only; do not reuse |
| 17–20 | Channel mask, picture mode, auto-color minutes, advance save slot |
| 21–25 / 26–30 | Port 1 / 2 type, three fire mappings, Up mapping |
| 31–32 | Space mapping, audio quality |
| 33 | Ignored legacy output selection; not written |
| 34 | Requested Turbo setting |

Defaults: hue 0, brightness 0/4/8/14, sound on, volume 15, all four channels on,
full speed, picture 3, Quality, standard clock. Auto-load, last-game boot,
auto-color timer and automatic slot advance are off. Port 1 defaults to Joy,
port 2 disabled, fire mappings A/B/Start, Up unassigned, Space=B.

Game settings live at `/.options/<basename>/<SHA256>.cfg` inside an ATBOYGO1
wrapper with ROM hash and the TLV record. Boot-last is global only. Options
opens on runtime values: G loads global settings, L loads game settings,
S saves game settings and stays, OPTION saves global settings and exits.
Esc restores the entry snapshot, or the snapshot from the last successful S;
it does not undo a file already saved. Runtime shortcuts do not automatically
write settings. Mono/stereo is always re-derived from hardware detection.

`/.atariboy.last` stores the last successfully started path as ATBOYLG1, a u16
length, absolute path and CRC32. It is updated independently of the boot-last
preference. Autostart runs only after title dismissal; a missing/invalid ROM
returns to the browser. Game startup applies game/global settings, loads battery
RAM and optionally the latest state. Esc/resume and R do not auto-load states.

Settings and battery replacements use temporary/backup files and validation.
These safeguards do not make the underlying flash/FAT metadata power-loss
transactional. Preserve previous valid data when replacement fails.

## 9. Disk maintenance

**Optimize disk** is offline FAT16 reclamation, not a format or a full fsck.
It accepts one or two FAT copies, including the original A8PicoCart layout.
Before any write it validates geometry, FAT copies where present, file/directory
chains, cross-links, references to free clusters and physical-map uniqueness.
Directory depth above 32 is rejected. Allocated orphan clusters are retained.
FAT[1]'s top two bits are clean-shutdown/error flags, not cluster-address bits;
validate its reserved low 14 bits and preserve the flags.

Temporary masks/buffers need approximately 9 KiB; insufficient SRAM can refuse
the operation while a large game is loaded. Reclamation updates mappings only
for free FAT clusters. It does not relocate live sectors, change FAT contents
or invalidate the active ROM map. Reported KiB is recovered physical mappings,
not increased FAT free space. Do not erase data sectors before committing the
map or add redundant erases before the allocator reuses a block.

**Erase disk** explicitly deletes all files, resets runtime game/settings state
and recreates the shared flash map/FAT16 volume labeled ATARIBOY. The first
1 MiB of firmware is never touched. Clearing a game releases dynamic maps;
the static emulator and cartridge-RAM storage remain allocated.

Both operations require NO→YES and a separate Return/Fire confirmation. Publish
the busy screen and wait for ACK plus an Atari frame before storage work.
COM remains active in SRAM. Neither operation runs concurrently with host USB.
An error must be reported, not treated as successful formatting/reclamation.

## 10. Validation boundaries and remaining limitations

Retained host suites exercise production storage/UI/state code with synthetic
ROM data and hardware stubs. They cover both FAT layouts and reject invalid
disks without mutation. They are not physical RP2040/Atari tests. A clean build
also checks Atari packing and firmware slot limits. Keep all private games,
flash images, generated listings and local test logs out of version control.

For future bus, clock or audio changes, also verify physical timing on Atari.
Relevant emulator-level checks include COM operation with XIP unavailable,
actual linked flash erase/program paths, real allocator failure behavior,
stack depth, loader INIT/RUN, and audio writes across sound/silence/UI
transitions. Historical external harnesses are not prerequisites supplied by
this repository; do not present model results as hardware qualification.

The source notes report hardware confirmation of PAL operation, NTSC mono and
the NTSC Joy2B+ measurement change. NTSC stereo was checked in Altirra but had
not received hardware confirmation. v0.90b's one-FAT optimization support had
software validation; the reported target disk still awaited hardware testing.
Keep these evidence limits unless new test results establish otherwise.

Sporadic long-running freezes and reports of USB slowdown/disconnection did
not have an established root cause in the supplied notes. Offline optimization
reclaims mappings but is not a confirmed repair for those incidents. Do not
attribute unexplained faults to U1MB, electrical edges, EXE load or a specific
bus path without new evidence. Heavy scenes can miss presentation/audio
deadlines; full speed is not guaranteed for every game or board.

## 11. Component provenance

- [Peanut-GB](https://github.com/deltabeard/Peanut-GB): pinned revision above;
  preserve its MIT notice and local APU integration changes.
- [Robin Edwards' A8PicoCart](https://github.com/robinhedwards/A8PicoCart):
  storage/integration reference `fb0b36137a969a71b9258841c46676701125699d`.
- [ascrnet/A8PicoCart](https://github.com/ascrnet/A8PicoCart): embedded dual-boot
  application `092397bd6d6056e4c1e9652e380e8619fdaf2193`.
- [Pico SDK](https://github.com/raspberrypi/pico-sdk): 2.1.1; its TinyUSB submodule
  at `86ad6e56c1700e85f1c5678607a762cfe3aa2f47` was used in the original environment.
- [MADS](https://github.com/tebe6502/Mad-Assembler) assembles Atari code.
- [Joy2B+](https://github.com/ascrnet/Joy2Bplus) provides the controller reference.

Retain third-party copyright and component terms in `licenses/` and sources;
do not assume all upstream code shares one license. Font attribution is in
`assets/FONT-NOTICE.txt`. Hardware timing references are the
[RP2040 datasheet](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf)
and [Altirra Hardware Reference Manual](https://www.virtualdub.org/downloads/Altirra%20Hardware%20Reference%20Manual.pdf).
