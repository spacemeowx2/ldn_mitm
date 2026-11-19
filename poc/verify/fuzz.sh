#!/usr/bin/env bash
#
# Robustness fuzzing for the libnx-only IPC server, under ASan+UBSan.
#
# Linux only: the sanitizers and the low-address mapping are much easier to get
# right there than under MinGW.
#
# Usage:
#   ./fuzz.sh                 # 30k iterations
#   ./fuzz.sh 200000 12345    # iterations, seed
#
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
poc_root="$(dirname "$here")"
include_dir="$here/harness/libnx_include"

iterations="${1:-30000}"
seed="${2:-1}"

if [ ! -f "$include_dir/switch.h" ]; then
    echo "run ./build.sh once first (it mirrors the libnx headers)" >&2
    exit 1
fi

cxx="${CXX:-g++}"
flags=(-std=gnu++17 -O1 -g -fno-omit-frame-pointer
       # libnx parses the HIPC pid as a u64 at a 4-byte-aligned offset (fine on
       # AArch64, flagged by UBSan on x86), so alignment checks are off.
       -fsanitize=address,undefined -fno-sanitize=alignment -fno-sanitize-recover=undefined
       -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers
       -I "$include_dir" -I "$here/harness/hostshim" -I "$poc_root/src" -I "$here/harness")

"$cxx" "${flags[@]}" \
    "$here/harness/mock_kernel.cpp" \
    "$here/harness/mock_sm.cpp" \
    "$here/harness/libnx_host_runtime.cpp" \
    "$here/harness/fake_lan_discovery.cpp" \
    "$poc_root/src/ipc_server.cpp" \
    "$poc_root/src/sm_mitm.c" \
    "$poc_root/src/debug.cpp" \
    "$poc_root/src/ldn_types.cpp" \
    "$poc_root/src/ldn_services.cpp" \
    "$here/fuzz/fuzz.cpp" \
    -o "$here/fuzz_run"

echo "built $here/fuzz_run"
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" "$here/fuzz_run" "$iterations" "$seed"
