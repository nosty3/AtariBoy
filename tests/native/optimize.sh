#!/usr/bin/env bash
set -euo pipefail
r="$(cd "$(dirname "$0")/../.." && pwd)"
b="$r/verification/optimize-native"
mkdir -p "$b"
for f in src/storage.c src/disk_optimize.c storage/flash_fs.c storage/fatfs_disk.c storage/fatfs/ff.c storage/fatfs/ffunicode.c storage/fatfs/diskio.c; do
 gcc -O2 -g -I"$r/tests/native" -I"$r" -I"$r/src" -I"$r/storage" -I"$r/storage/fatfs" -c "$r/$f" -o "$b/$(basename "$f").o"
done
g++ -O2 -g -I"$r" -I"$r/src" "$r/tests/native/optimize.cpp" "$b/"*.o -o "$b/test"
"$b/test" "$r/verification"

"$b/test" "$r/verification" legacy
