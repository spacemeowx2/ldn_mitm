# Atmosphere reference sources (test oracle only)

Vendored verbatim from Atmosphere for a byte-for-byte differential test of our
libnx-only sm mitm client (src/sm_mitm.c).

- upstream: https://github.com/Atmosphere-NX/Atmosphere
- commit:   3e8915011e1de89f595462d929c1cc3294470896
- files:
    libraries/libstratosphere/source/service_guard.h
    libraries/libstratosphere/source/sm/sm_ams.os.horizon.c
    libraries/libstratosphere/source/sm/sm_ams.os.horizon.h

They are pure libnx C, so they build on the host alongside our implementation.
The test (TestSmClientParity) runs both against the mock sm and compares the
bytes they put on the wire.

Licence: GPLv2 (same as Atmosphere); see the headers of the files.
