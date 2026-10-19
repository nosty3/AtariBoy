# Runtime diagnostics

During a game, **F** toggles FPS/OUT and **Ctrl+D** toggles detailed diagnostics.
Both are temporary and off by default. Disabled diagnostics omit timing
measurements and overlay rendering; protocol counters still operate.

| Counter | Meaning |
|---|---|
| FPS | Completed Game Boy frames per second |
| OUT | New Game Boy image generations accepted for presentation per second |
| AT / RP | Atari packets per second / repeated images |
| AU | Missing PCM batches while audio playback is active |
| AR / RX | Progress of Atari frame numbers / published COM packets |
| LS | Excess Atari frame-number progress over received packets |
| OV | COM packets superseded before EXE observed them |
| E | Ambiguous observations, excluded from reliable loss estimates |
| EX / MX | Mean / maximum active EXE iteration time |
| GB | EXE work per completed Game Boy frame, including application work |
| IG / MW | Maximum observation gap / acknowledgement wait |
| TV | Selected nominal Atari rate, 50 or 60 Hz |

Video and audio publication are independent. A repeated picture does not imply
a missing audio batch, and 60 emulated frames per second does not imply 60
presentations on PAL hardware. Normal asynchronous publication does not wait
for an acknowledgement in the emulation loop.

Compare the same scene or saved state, display mapping, PAL speed, sound and
clock settings. Allow two seconds after mode changes. Record firmware version
and relevant counters. These software counters cannot identify physical bus
edges or prove the cause of a hardware fault. Heavy scenes can still miss audio
or presentation deadlines when emulation exceeds the frame budget.
