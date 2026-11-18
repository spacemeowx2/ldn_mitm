#include "sm_mitm.h"

#include <string.h>

#define LDN_SM_TRY(expr)            \
    do {                            \
        const Result _rc = (expr);  \
        if (R_FAILED(_rc)) {        \
            return _rc;             \
        }                           \
    } while (0)

#define LDN_SM_CMD_INSTALL_MITM    65000u
#define LDN_SM_CMD_UNINSTALL_MITM  65001u
#define LDN_SM_CMD_ACK_SESSION     65003u
#define LDN_SM_CMD_DETACH_CLIENT   4u

static TipcService g_sm_session;      /* install / uninstall */
static TipcService g_ack_session;     /* acknowledge */
static bool g_initialized;

static SmServiceName encode_name(const char *name) {
    return smEncodeName(name);
}

static Result open_sm_session(TipcService *out) {
    Handle handle = 0;
    Result rc = svcConnectToNamedPort(&handle, "sm:");
    if (R_FAILED(rc)) {
        return rc;
    }
    tipcCreate(out, handle);
    return tipcDispatch(out, 0, .in_send_pid = true);
}

static void close_sm_session(TipcService *session) {
    /* Hand the client slot back to sm; sm reuses these. */
    tipcDispatch(session, LDN_SM_CMD_DETACH_CLIENT, .in_send_pid = true);
    tipcClose(session);
}

Result ldn_sm_mitm_initialize(void) {
    if (g_initialized) {
        return 0;
    }
    LDN_SM_TRY(open_sm_session(&g_sm_session));
    LDN_SM_TRY(open_sm_session(&g_ack_session));
    g_initialized = true;
    return 0;
}

void ldn_sm_mitm_finalize(void) {
    if (!g_initialized) {
        return;
    }
    close_sm_session(&g_ack_session);
    close_sm_session(&g_sm_session);
    g_initialized = false;
}

Result ldn_sm_mitm_install(Handle *out_port, Handle *out_query, const char *name) {
    SmServiceName service_name = encode_name(name);
    Handle handles[2] = { 0, 0 };

    LDN_SM_TRY(tipcDispatchIn(&g_sm_session, LDN_SM_CMD_INSTALL_MITM, service_name,
        .out_handle_attrs = { SfOutHandleAttr_HipcMove, SfOutHandleAttr_HipcMove },
        .out_handles      = handles));

    *out_port  = handles[0];
    *out_query = handles[1];
    return 0;
}

Result ldn_sm_mitm_uninstall(const char *name) {
    SmServiceName service_name = encode_name(name);
    return tipcDispatchIn(&g_sm_session, LDN_SM_CMD_UNINSTALL_MITM, service_name);
}

Result ldn_sm_mitm_acknowledge(Handle *out_forward, LdnMitmProcessInfo *out_info, const char *name) {
    SmServiceName service_name = encode_name(name);
    LdnMitmProcessInfo info;
    Handle handle = 0;

    memset(&info, 0, sizeof(info));

    LDN_SM_TRY(tipcDispatchInOut(&g_ack_session, LDN_SM_CMD_ACK_SESSION, service_name, info,
        .out_handle_attrs = { SfOutHandleAttr_HipcMove },
        .out_handles      = &handle));

    *out_forward = handle;
    *out_info    = info;
    return 0;
}
