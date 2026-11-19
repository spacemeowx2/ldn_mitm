#!/usr/bin/env bash
#
# Refreshes the pinned Atmosphere reference sources used by the differential
# test (TestSmClientParity). Pinning matters: if upstream changes the sm mitm
# wire format, this test is supposed to fail *because* the reference moved.
#
# Usage:
#   ./sync_atmosphere_ref.sh ~/project/Atmosphere
#
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
dest="$here/harness/atmosphere_ref"

if [ $# -lt 1 ]; then
    echo "usage: $0 /path/to/Atmosphere" >&2
    exit 1
fi

atmosphere="$1"
src="$atmosphere/libraries/libstratosphere/source"

if [ ! -f "$src/sm/sm_ams.os.horizon.c" ]; then
    echo "'$atmosphere' does not look like an Atmosphere checkout" >&2
    exit 1
fi

commit="$(git -C "$atmosphere" rev-parse HEAD)"

mkdir -p "$dest/sm"
cp "$src/service_guard.h" "$dest/"
cp "$src/sm/sm_ams.os.horizon.c" "$dest/sm/"
cp "$src/sm/sm_ams.os.horizon.h" "$dest/sm/"

cat > "$dest/README.md" <<EOF
# Atmosphere reference sources (test oracle only)

Vendored verbatim from Atmosphere for a byte-for-byte differential test of our
libnx-only sm mitm client (src/sm_mitm.c).

- upstream: https://github.com/Atmosphere-NX/Atmosphere
- commit:   $commit
- files:
    libraries/libstratosphere/source/service_guard.h
    libraries/libstratosphere/source/sm/sm_ams.os.horizon.c
    libraries/libstratosphere/source/sm/sm_ams.os.horizon.h

They are pure libnx C, so they build on the host alongside our implementation.
The test (TestSmClientParity) runs both against the mock sm and compares the
bytes they put on the wire.

Licence: GPLv2 (same as Atmosphere); see the headers of the files.
EOF

echo "updated $dest -> $commit"
