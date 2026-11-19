#!/usr/bin/env bash
#
# Cross-compile every target source in src/ for aarch64 with the real libnx and
# the real LAN implementation. The host test build replaces lan_discovery.cpp
# with a fake, so this is what proves the real modules still compile for the
# device.
#
# Requires devkitPro's devkitA64 (see dev-env.md):
#   export DEVKITPRO=/opt/devkitpro
#   export PATH=$DEVKITPRO/devkitA64/bin:$PATH
#
# Usage:
#   ./cross_check.sh
#   LIBNX=/opt/devkitpro/libnx/include ./cross_check.sh
#
set -uo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
poc_root="$(dirname "$here")"
libnx_include="${LIBNX:-${DEVKITPRO:-/opt/devkitpro}/libnx/include}"
cxx="${CXX:-aarch64-none-elf-g++}"
cc="${CC:-aarch64-none-elf-gcc}"

if [ ! -f "$libnx_include/switch.h" ]; then
    echo "libnx headers not found at '$libnx_include'" >&2
    exit 1
fi

out="$(mktemp -d)"
trap 'rm -rf "$out"' EXIT

status=0

echo "== aarch64 cross-compile: $poc_root/src =="

for src in "$poc_root"/src/*.cpp; do
    name="$(basename "$src")"
    printf '  %-28s ' "$name"
    if "$cxx" -std=gnu++17 -O2 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers \
        -c -I "$libnx_include" -I "$poc_root/src" "$src" -o "$out/$name.o" 2> "$out/$name.log"; then
        echo "OK"
    else
        echo "FAIL"
        sed 's/^/      /' "$out/$name.log" | head -n 20
        status=1
    fi
done

for src in "$poc_root"/src/*.c; do
    [ -e "$src" ] || continue
    name="$(basename "$src")"
    printf '  %-28s ' "$name"
    if "$cc" -std=gnu11 -O2 -Wall -Wextra -D_GNU_SOURCE \
        -c -I "$libnx_include" -I "$poc_root/src" "$src" -o "$out/$name.o" 2> "$out/$name.log"; then
        echo "OK"
    else
        echo "FAIL"
        sed 's/^/      /' "$out/$name.log" | head -n 20
        status=1
    fi
done

if [ "$status" -eq 0 ]; then
    echo "all target sources compile for aarch64"
fi
exit "$status"
