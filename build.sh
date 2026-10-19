#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
: "${PICO_SDK_PATH:?Set PICO_SDK_PATH to Pico SDK 2.1.1}"
python3 "$project_dir/build_atari.py" "${MADS:-mads}"
cmake -S "$project_dir" -B "${BUILD_DIR:-$project_dir/out/atariboy}" -G Ninja -DPICO_SDK_PATH="$PICO_SDK_PATH" -DCMAKE_BUILD_TYPE=Release "$@"
cmake --build "${BUILD_DIR:-$project_dir/out/atariboy}" --parallel 4
