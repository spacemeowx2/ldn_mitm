#!/usr/bin/env bash
#
# Host-side verification build (Linux/macOS) -- the counterpart of build.ps1.
#
# It compiles:
#   - tests/main.cpp          : test cases, written as a *client* using real libnx
#   - src/ipc_server.cpp      : our libnx-only CMIF server (the code under test)
#   - harness/mock_kernel.cpp : simulated kernel (sessions, ports, TLS)
#
# against a mirrored copy of libnx's headers in which only switch/arm/tls.h and
# BITL() are patched (both explained in build.ps1).
#
# Usage:
#   ./build.sh                # sync headers + build + run
#   SYNC=0 ./build.sh         # skip header mirror
#   LIBNX=/path/to/libnx/nx/include ./build.sh
#
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
poc_root="$(dirname "$here")"
include_dir="$here/harness/libnx_include"
libnx="${LIBNX:-$HOME/project/libnx/nx/include}"

if [ "${SYNC:-1}" != "0" ]; then
    if [ ! -f "$libnx/switch.h" ]; then
        echo "libnx headers not found at '$libnx' (set LIBNX=... or SYNC=0)" >&2
        exit 1
    fi
    rm -rf "$include_dir"
    mkdir -p "$include_dir"
    cp -r "$libnx"/* "$include_dir"/

    # (1) aarch64 asm: tpidrro_el0 does not exist on the mock kernel.
    cat > "$include_dir/switch/arm/tls.h" <<'EOF'
#pragma once
#include "../types.h"

/* Host shim: the real libnx header reads tpidrro_el0, which does not exist on
 * the mock kernel. The implementation lives in harness/mock_kernel.cpp. */
#ifdef __cplusplus
extern "C" {
#endif

void* armGetTls(void);

#ifdef __cplusplus
}
#endif
EOF

    # (2) BITL() must be a 64-bit shift (host is LLP64, AArch64 is LP64).
    sed -i 's/#define BITL(n) (1UL<<(n))/#define BITL(n) ((uint64_t)1<<(n))/' "$include_dir/switch/types.h"

    echo "synced libnx headers -> $include_dir"
fi

cxx="${CXX:-g++}"

"$cxx" -std=gnu++17 -O1 -g -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers \
    -I "$include_dir" \
    -I "$here/harness/hostshim" \
    -I "$poc_root/src" \
    -I "$here/harness" \
    "$here/harness/mock_kernel.cpp" \
    "$poc_root/src/ipc_server.cpp" \
    "$here/tests/main.cpp" \
    -o "$here/ipc_tests"

"$here/ipc_tests"
