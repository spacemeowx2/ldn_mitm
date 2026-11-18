/*
 * sm_mitm.h -- libnx-only port of the sm side of Atmosphere's mitm support.
 *
 * This is the piece ldn_mitm needs from libstratosphere's `ams::sm`: registering
 * a mitm service for a name, acknowledging an incoming client session, and
 * unregistering. Atmosphere's own implementation (libstratosphere
 * source/sm/sm_ams.os.horizon.c) is already plain libnx -- svcConnectToNamedPort
 * + tipcDispatch -- so this file is a trimmed port of it, not a reimplementation
 * of a service.
 *
 * Protocol (Atmosphere extensions to sm):
 *   65000 InstallMitm(name)        -> out: 2 move handles (mitm port, query)
 *   65001 UninstallMitm(name)
 *   65003 AcknowledgeSession(name) -> in/out: MitmProcessInfo, out: move handle
 *                                     (the real service session)
 *   65006 DeclareFutureMitm(name)
 *   65007 ClearFutureMitm(name)
 *
 * Differences from the Atmosphere original:
 *   - Atmosphere opens a fresh sm session per install (DoWithPerThreadSession)
 *     to stay thread safe; this PoC keeps one session, which is fine while the
 *     caller is single threaded. Swap in per-call sessions if that changes.
 */
#pragma once

#include <switch.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    u64 process_id;
    u64 program_id;
    u64 keys_held;
    u64 flags;
} LdnMitmProcessInfo;

/* Opens the two sm sessions used below. */
Result ldn_sm_mitm_initialize(void);
void   ldn_sm_mitm_finalize(void);

/* Installs a mitm for `name`; `out_port` receives the port to accept clients on
 * and `out_query` the handle used by sm to ask ShouldMitm(). */
Result ldn_sm_mitm_install(Handle *out_port, Handle *out_query, const char *name);
Result ldn_sm_mitm_uninstall(const char *name);

/* Acknowledges the client session just accepted from the mitm port: returns the
 * session to the real service plus the client's identity. */
Result ldn_sm_mitm_acknowledge(Handle *out_forward, LdnMitmProcessInfo *out_info, const char *name);

#ifdef __cplusplus
}
#endif
