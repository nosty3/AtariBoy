#!/usr/bin/env bash
set -euo pipefail
r="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"

export ATARIBOY_TEST_OUTPUT="$r/verification"
b="$r/verification/native"
mkdir -p "$b"
for f in src/audio.c vendor/minigb_apu/minigb_apu.c src/engine.c src/saves.c src/sha256.c src/disk_optimize.c src/storage.c storage/flash_fs.c storage/fatfs_disk.c storage/msc_disk.c storage/fatfs/ff.c storage/fatfs/ffunicode.c storage/fatfs/diskio.c; do
 gcc -O2 -g -DMINIGB_APU_AUDIO_FORMAT_S16SYS -DAUDIO_SAMPLE_RATE=18635 -I"$r/tests/native" -I"$r" -I"$r/src" -I"$r/storage" -I"$r/storage/fatfs" -c "$r/$f" -o "$b/$(basename "$f").o"
done
g++ -O2 -g -I"$r" -I"$r/src" "$r/tests/native/test62.cpp" "$r/src/settings.cpp" "$r/src/options_ui.cpp" "$r/src/browser.cpp" "$r/src/app.cpp" "$r/src/state_ui.cpp" "$r/src/controls.cpp" "$r/src/video.cpp" "$r/src/osd.cpp" "$b/"*.o -Wl,--wrap=f_open,--wrap=f_read,--wrap=f_opendir,--wrap=sha256_update -o "$b/test"
"$b/test"
