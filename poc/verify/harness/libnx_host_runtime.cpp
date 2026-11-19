/*
 * libnx_host_runtime.cpp -- the few libnx functions that live in libnx's .c
 * files rather than in headers, re-implemented on top of the mock kernel, plus
 * service stubs that the host has no way to reach.
 */
#include <switch.h>

#include "mock_kernel.h"

extern "C" {

/* --- kernel/event.h ------------------------------------------------------ */
Result eventCreate(Event *t, bool autoclear) {
    t->autoclear = autoclear;
    return svcCreateEvent(&t->wevent, &t->revent);
}

void eventClose(Event *t) {
    if (t->revent != 0) {
        svcCloseHandle(t->revent);
    }
    if (t->wevent != 0) {
        svcCloseHandle(t->wevent);
    }
    t->revent = 0;
    t->wevent = 0;
}

Result eventFire(Event *t) {
    return svcSignalEvent(t->wevent);
}

Result eventClear(Event *t) {
    return svcClearEvent(t->revent);
}

Result eventWait(Event *t, u64 timeout) {
    (void)t;
    (void)timeout;
    return 0;
}

/* --- services/nifm.h ----------------------------------------------------- */
/* There is no nifm service in the mock kernel; report a fixed configuration so
 * GetIpv4Address can still be exercised end to end. */
Result nifmGetCurrentIpConfigInfo(u32 *current_addr, u32 *subnet_mask, u32 *gateway, u32 *primary_dns_server, u32 *secondary_dns_server) {
    if (current_addr != nullptr)      { *current_addr = 0xC0A8010Au; }       /* 192.168.1.10, network order */
    if (subnet_mask != nullptr)       { *subnet_mask = 0xFFFFFF00u; }        /* 255.255.255.0 */
    if (gateway != nullptr)           { *gateway = 0x0A0A0A01u; }
    if (primary_dns_server != nullptr)   { *primary_dns_server = 0x08080808u; }
    if (secondary_dns_server != nullptr) { *secondary_dns_server = 0x08080404u; }
    return 0;
}

} // extern "C"
