#!/usr/bin/env bash
# Portable build; requires Pico SDK 2.1.1, Arm GCC, MADS, Python/Pillow.
set -euo pipefail
r="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
: "${PICO_SDK_PATH:?Set PICO_SDK_PATH}"
export BUILD_DIR="${BUILD_DIR:-$r/out/atariboy}"
selector_build="${SELECTOR_BUILD_DIR:-$r/out/selector}"
a8_build="${A8_BUILD_DIR:-$r/out/a8picocart}"
bash "$r/build.sh" "$@"
python3 "$r/dual/build_selector.py" "${MADS:-mads}"
for part in selector a8picocart; do
 if [[ "$part" == selector ]]; then out="$selector_build"; else out="$a8_build"; fi
 cmake -S "$r/dual/$part" -B "$out" -G Ninja -DPICO_SDK_PATH="$PICO_SDK_PATH" -DCMAKE_BUILD_TYPE=Release "$@"
 cmake --build "$out" --parallel 4
done
python3 "$r/dual/merge.py" "$BUILD_DIR" "$selector_build" "$a8_build"
