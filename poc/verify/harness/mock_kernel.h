/*
 * mock_kernel.h -- Host-side simulation of the parts of the Horizon kernel
 * that ldn_mitm touches (IPC sessions, ports, TLS message buffer, handles).
 *
 * Goal: run *real* libnx client code (switch/sf/cmif.h, hipc.h, service.h,
 * tipc.h) against our own server implementation on a normal desktop, so the
 * protocol layer can be validated without hardware.
 *
 * Faithful bits:
 *   - one TLS message buffer per thread, message starts at armGetTls() + 0
 *   - svcSendSyncRequest hands the request to the server side of the session:
 *       * if the session has a handler installed, it runs inline (this is the
 *         convenience mode used by the protocol tests)
 *       * otherwise the request is queued and the caller blocks until a server
 *         thread picks it up in svcReplyAndReceive() and replies, which is what
 *         the real accept/dispatch loop does
 *   - ports queue connected clients until svcAcceptSession()
 *   - mitm routing: connecting to a named port that has a registered mitm
 *     yields a session served by the mitm instead of the real service
 *
 * Simplifications (documented, not hidden):
 *   - handles are process-global instead of per-process, so handle transfer is
 *     a no-op (the value stays valid)
 */
#pragma once

#include <switch/types.h>
#include <switch/result.h>

#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <mutex>
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

    /* Convenience mode: run inline on the caller's thread. */
    std::function<Result(Handle)> handler;

    /* Threaded mode: a request waits in `message` until a server thread takes
     * it in svcReplyAndReceive() and posts the reply back. */
    bool request_pending = false;
    bool reply_ready = false;
    bool closed = false;
    u8 message[TlsMessageBufferSize] = {};

    bool pending_accept = false;
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
    using ConnectCallback = std::function<Handle(const char *name)>;
    void RegisterNamedPort(const char *name, ConnectCallback cb);

    Handle CreatePair(const std::string &name);
    Handle CreatePort(const char *name);
    void EnqueueOnPort(Handle port_handle, Handle client_handle);

    /* Routes every connection to `service_name` through `port_handle` (this is
     * what sm does once a mitm is installed for that name). */
    void SetMitmPort(const char *service_name, Handle port_handle);
    void ClearMitmPort(const char *service_name);

    SessionPair *GetPairByClient(Handle h);
    SessionPair *GetPairByServer(Handle h);

    void SetHandler(Handle server_handle, std::function<Result(Handle)> handler);
    void SetDefaultHandler(std::function<Result(Handle)> handler);

    size_t LiveSessionCount() const;
    size_t LiveHandleCount() const;

    /* --- kernel entry points -------------------------------------------- */
    Handle ConnectToNamedPort(const char *name);
    Result SendSyncRequest(Handle session_handle);
    Result AcceptSession(Handle *out, Handle port_handle);
    Result CreateSession(Handle *server, Handle *client);
    Result CloseHandle(Handle h);
    Result ReplyAndReceive(s32 *index, const Handle *handles, s32 handle_count, Handle reply_target);

    u8 *Tls();

    /* Byte image of the last request, captured before dispatch. Used by the
     * differential test against Atmosphere's own sm client. */
    const u8 *LastRequest(size_t *out_size);

    /* Looks back through recent requests for one whose HIPC type matches
     * (TIPC encodes the command id as type + 16). */
    bool FindLastRequest(u32 type, u8 *out, size_t capacity, size_t *out_size);
    size_t LastRequestSize();

    /* Kernel events (fake handles; the mock only signals them). */
    Handle CreateEventPair();
    bool IsEventHandle(Handle h) const;
    int SignalCount(Handle h) const;

    /* --- diagnostics ----------------------------------------------------- */
    void SetVerbose(bool v) { m_verbose = v; }
    void Log(const char *fmt, ...);
    int Failures() const { return m_failures; }
    void Fail(const char *what);

private:
    Kernel() = default;

    SessionPair *FindPairByHandleLocked(Handle h, bool want_server);
    PortObject *FindPortLocked(Handle h);

    mutable std::mutex m_mutex;
    std::condition_variable m_cv;

    std::unordered_map<Handle, SessionPair *> m_pairs_by_client;
    std::unordered_map<Handle, SessionPair *> m_pairs_by_server;
    std::unordered_map<Handle, PortObject *> m_ports;
    std::unordered_map<std::string, ConnectCallback> m_named_ports;
    std::unordered_map<std::string, Handle> m_mitm_ports;
    std::vector<SessionPair *> m_pairs;
    std::vector<PortObject *> m_port_objects;
    std::unordered_map<Handle, int> m_events;

    Handle m_next_event = 0x9000;
    Handle m_next_session = SessionBase;
    Handle m_next_port = PortBase;

    std::function<Result(Handle)> m_default_handler;
    struct CapturedRequest { u32 type; size_t size; u8 bytes[TlsMessageBufferSize]; };
    static constexpr size_t MaxCapturedRequests = 16;
    CapturedRequest m_captured[MaxCapturedRequests] = {};
    size_t m_captured_count = 0;
    u8 m_last_request[TlsMessageBufferSize] = {};
    size_t m_last_request_size = 0;

    bool m_verbose = false;
    int m_failures = 0;
};

/* Convenience accessor. */
inline Kernel &K() { return Kernel::Get(); }

/* ------------------------------------------------------------------------- */
/* Low-address buffers                                                       */
/* ------------------------------------------------------------------------- */
/* HIPC encodes "server pointers" (send statics / recv list) in ~40 bits, which
 * is plenty on the device (all user addresses fit) but truncates ordinary
 * x86_64 stack/heap pointers (0x7ffd..., 0x5555...). Any test buffer that is
 * passed as SfBufferAttr_HipcAutoSelect or _HipcPointer must therefore live
 * below 1 TiB, which is what these helpers provide. */
void *AllocLow(size_t size, size_t alignment = 0x1000);
void FreeLow(void *pointer, size_t size);

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

/* Events (kernel objects the mock only needs to hand out and signal). */
Result svcCreateEvent(Handle *server_handle, Handle *client_handle);
Result svcSignalEvent(Handle handle);
Result svcClearEvent(Handle handle);

#ifdef __cplusplus
}
#endif
