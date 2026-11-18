/*
 * mock_kernel.h -- Host-side simulation of the parts of the Horizon kernel
 * that ldn_mitm touches (IPC sessions, ports, TLS message buffer, handles).
 *
 * Goal: run *real* libnx client code (switch/sf/cmif.h, hipc.h, service.h,
 * tipc.h) against our own server implementation on a normal desktop, so the
 * protocol layer can be validated without hardware.
 *
 * Faithful bits:
 *   - one TLS message buffer, message starts at armGetTls() + 0
 *   - svcSendSyncRequest delivers the request to the *server side* of the
 *     session, which processes it in place and writes the reply into the same
 *     buffer (exactly what the kernel does with the server thread's TLS)
 *   - sessions are (client handle, server handle) pairs; a port queues
 *     accepted-pending client sessions until svcAcceptSession()
 *   - mitm routing: connecting to a named port that has a registered mitm
 *     yields a session served by the mitm instead of the real service
 *
 * Simplifications (documented, not hidden):
 *   - handles are process-global instead of per-process, so "move handle"
 *     transfer is a no-op (the value stays valid)
 *   - single-threaded / synchronous: the server is invoked from inside the
 *     client's svcSendSyncRequest call instead of a real server thread
 */
#pragma once

#include <switch/types.h>
#include <switch/result.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace mock {

constexpr size_t TlsMessageBufferSize = 0x1000;
constexpr Handle InvalidHandle = 0;

/* Handle namespaces keep debugging sane. */
constexpr Handle SessionBase = 0x1000;
constexpr Handle PortBase = 0x4000;

struct SessionPair {
    std::string name;
    Handle client_handle = InvalidHandle;
    Handle server_handle = InvalidHandle;
    /* Set once a server takes ownership of the server side. */
    std::function<Result(Handle server_handle)> handler;
    bool pending_accept = false;
    /* Fake client identity, reported through AcknowledgeMitmSession. */
    u64 client_pid = 0;
    u64 client_program_id = 0;
};

struct PortObject {
    std::string name;
    std::vector<Handle> pending_clients;
};

class Kernel {
public:
    static Kernel &Get();

    void Reset();

    /* --- test-facing helpers -------------------------------------------- */
    /* Registers a named port. `on_connect` must produce the server side of a
     * new session (usually by calling AcceptIntoPort / CreatePair). */
    using ConnectCallback = std::function<Handle(const char *name)>;
    void RegisterNamedPort(const char *name, ConnectCallback cb);

    /* Creates an unattached session pair; returns the client handle. */
    Handle CreatePair(const std::string &name);

    /* Creates a port object and returns its handle. */
    Handle CreatePort(const char *name);

    /* Queues an existing pair's client side onto a port (client "connected"). */
    void EnqueueOnPort(Handle port_handle, Handle client_handle);

    /* Routes every connection to `service_name` through `port_handle` (this is
     * what sm does once a mitm is installed for that name). */
    void SetMitmPort(const char *service_name, Handle port_handle);
    void ClearMitmPort(const char *service_name);

    SessionPair *GetPairByClient(Handle h);
    SessionPair *GetPairByServer(Handle h);

    /* Installs the handler for a server-side session handle. */
    void SetHandler(Handle server_handle, std::function<Result(Handle)> handler);

    /* Handler used for sessions created through svcCreateSession (i.e. the
     * server side of an out object). */
    void SetDefaultHandler(std::function<Result(Handle)> handler);

    /* Number of currently live sessions (for leak checks). */
    size_t LiveSessionCount() const;
    size_t LiveHandleCount() const;

    /* --- kernel entry points -------------------------------------------- */
    Handle ConnectToNamedPort(const char *name);
    Result SendSyncRequest(Handle session_handle);
    Result AcceptSession(Handle *out, Handle port_handle);
    Result CreateSession(Handle *server, Handle *client);
    Result CloseHandle(Handle h);

    u8 *Tls() { return m_tls; }

    /* --- diagnostics ----------------------------------------------------- */
    void SetVerbose(bool v) { m_verbose = v; }
    void Log(const char *fmt, ...);
    int Failures() const { return m_failures; }
    void Fail(const char *what);

private:
    Kernel() = default;

    std::unordered_map<Handle, SessionPair *> m_pairs_by_client;
    std::unordered_map<Handle, SessionPair *> m_pairs_by_server;
    std::unordered_map<Handle, PortObject *> m_ports;
    std::unordered_map<std::string, ConnectCallback> m_named_ports;
    std::unordered_map<std::string, Handle> m_mitm_ports;
    std::vector<SessionPair *> m_pairs;
    std::vector<PortObject *> m_port_objects;

    Handle m_next_session = SessionBase;
    Handle m_next_port = PortBase;

    alignas(0x100) u8 m_tls[TlsMessageBufferSize] = {};
    std::function<Result(Handle)> m_default_handler;
    bool m_verbose = false;
    int m_failures = 0;
};

/* Convenience accessor. */
inline Kernel &K() { return Kernel::Get(); }

} // namespace mock

/* ------------------------------------------------------------------------- */
/* Kernel syscall surface used by libnx client code and by the server code.   */
/* Signatures must match nx/include/switch/kernel/svc.h exactly.              */
/* ------------------------------------------------------------------------- */
#ifdef __cplusplus
extern "C" {
#endif

void *armGetTls(void); /* provided by libnx_host/switch/arm/tls.h shim */

Result svcSendSyncRequest(Handle session);
Result svcCloseHandle(Handle handle);
Result svcConnectToNamedPort(Handle *session, const char *name);
Result svcCreateSession(Handle *server_handle, Handle *client_handle, u32 unk0, u64 unk1);
Result svcAcceptSession(Handle *session_handle, Handle port_handle);
Result svcReplyAndReceive(s32 *index, const Handle *handles, s32 handleCount, Handle replyTarget, u64 timeout);
Result svcBreak(u32 breakReason, uintptr_t address, uintptr_t size);
Result svcGetThreadId(u64 *threadID, Handle handle);
Result svcGetInfo(u64 *out, u32 id0, Handle handle, u64 id1);
Result svcSetHeapSize(void **out_addr, u64 size);
void svcSleepThread(s64 ns);
u64 svcGetSystemTick(void);

#ifdef __cplusplus
}
#endif
