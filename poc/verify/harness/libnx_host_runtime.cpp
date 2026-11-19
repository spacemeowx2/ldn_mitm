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

/* MinGW's <thread> implementation references ::nanosleep. The tests synchronise
 * with condition variables, so sleeping can be a no-op here. (Including
 * <windows.h> would collide with the arm_acle shim, hence no Sleep().) */
int nanosleep(const struct timespec *req, struct timespec *rem) {
    (void)req;
    (void)rem;
    return 0;
}


/* --- kernel/mutex.h ------------------------------------------------------ */
/* libnx implements these in C over svcWaitSynchronization; the mock only needs
 * mutual exclusion between test threads, so a recursive mutex stands in (it
 * also cannot deadlock if the same lock is taken twice). */
#include <mutex>

namespace {
std::recursive_mutex g_host_mutex;
}

void mutexLock(Mutex *m) {
    (void)m;
    g_host_mutex.lock();
}

bool mutexTryLock(Mutex *m) {
    (void)m;
    return g_host_mutex.try_lock();
}

void mutexUnlock(Mutex *m) {
    (void)m;
    g_host_mutex.unlock();
}

/* --- services/sm.h ------------------------------------------------------- */
/* The reference sm client reaches the global sm session for the commands that
 * are not per-thread (Uninstall, HasMitm, ...). Point it at the mock sm. */
static TipcService g_mock_sm_session;
static bool g_mock_sm_ready;

TipcService *smGetServiceSessionTipc(void) {
    if (!g_mock_sm_ready) {
        Handle handle = 0;
        if (R_SUCCEEDED(svcConnectToNamedPort(&handle, "sm:"))) {
            tipcCreate(&g_mock_sm_session, handle);
            if (R_SUCCEEDED(tipcDispatch(&g_mock_sm_session, 0, .in_send_pid = true))) {
                g_mock_sm_ready = true;
            }
        }
    }
    return &g_mock_sm_session;
}
} // extern "C"
