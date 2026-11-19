#include "mock_kernel.h"

#include <switch/sf/hipc.h>

#include <cstdarg>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

namespace mock {

thread_local u8 t_tls[TlsMessageBufferSize] = {};

#ifdef _WIN32
void *AllocLow(size_t size, size_t alignment) {
    static uintptr_t s_next = 0x0000000100000000ull;
    (void)alignment;
    const size_t rounded = (size + 0xFFF) & ~static_cast<size_t>(0xFFF);
    for (int attempt = 0; attempt < 4; ++attempt) {
        void *pointer = VirtualAlloc(reinterpret_cast<void *>(s_next), rounded, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (pointer != nullptr) {
            s_next += rounded + 0x10000;
            return pointer;
        }
        s_next <<= 1;
    }
    return nullptr;
}

void FreeLow(void *pointer, size_t size) {
    (void)size;
    if (pointer != nullptr) {
        VirtualFree(pointer, 0, MEM_RELEASE);
    }
}
#else
void *AllocLow(size_t size, size_t alignment) {
    static uintptr_t s_next = 0x0000000100000000ull;
    (void)alignment;
    const size_t rounded = (size + 0xFFF) & ~static_cast<size_t>(0xFFF);
    for (int attempt = 0; attempt < 4; ++attempt) {
        void *wanted = reinterpret_cast<void *>(s_next);
        void *pointer = mmap(wanted, rounded, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
        if (pointer != MAP_FAILED) {
            s_next += rounded + 0x10000;
            return pointer;
        }
        s_next <<= 1;
    }
    return nullptr;
}

void FreeLow(void *pointer, size_t size) {
    if (pointer != nullptr) {
        munmap(pointer, (size + 0xFFF) & ~static_cast<size_t>(0xFFF));
    }
}
#endif

Kernel &Kernel::Get() {
    static Kernel s_kernel;
    return s_kernel;
}

void Kernel::Log(const char *fmt, ...) {
    if (!m_verbose) {
        return;
    }
    std::va_list args;
    va_start(args, fmt);
    std::fputs("[mock] ", stderr);
    std::vfprintf(stderr, fmt, args);
    std::fputc('\n', stderr);
    va_end(args);
}

void Kernel::Fail(const char *what) {
    ++m_failures;
    std::fprintf(stderr, "[mock][FAIL] %s\n", what);
}

void Kernel::Reset() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto *pair : m_pairs) {
            delete pair;
        }
        for (auto *port : m_port_objects) {
            delete port;
        }
        m_pairs.clear();
        m_port_objects.clear();
        m_pairs_by_client.clear();
        m_pairs_by_server.clear();
        m_ports.clear();
        m_named_ports.clear();
        m_mitm_ports.clear();
        m_events.clear();
        m_default_handler = nullptr;
        m_next_session = SessionBase;
        m_next_port = PortBase;
        m_next_event = 0x9000;
        m_failures = 0;
    }
    std::memset(t_tls, 0, sizeof(t_tls));
}

void Kernel::RegisterNamedPort(const char *name, ConnectCallback cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_named_ports[name] = std::move(cb);
}

Handle Kernel::CreatePair(const std::string &name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto *pair = new SessionPair();
    pair->name = name;
    pair->client_handle = m_next_session++;
    pair->server_handle = m_next_session++;
    m_pairs.push_back(pair);
    m_pairs_by_client[pair->client_handle] = pair;
    m_pairs_by_server[pair->server_handle] = pair;
    Log("create pair '%s' client=%#x server=%#x", name.c_str(), pair->client_handle, pair->server_handle);
    return pair->client_handle;
}

Handle Kernel::CreatePort(const char *name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto *port = new PortObject();
    port->name = name;
    const Handle handle = m_next_port++;
    m_port_objects.push_back(port);
    m_ports[handle] = port;
    Log("create port '%s' handle=%#x", name, handle);
    return handle;
}

void Kernel::EnqueueOnPort(Handle port_handle, Handle client_handle) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_ports.find(port_handle);
        if (it == m_ports.end()) {
            Fail("EnqueueOnPort: bad port handle");
            return;
        }
        it->second->pending_clients.push_back(client_handle);
        if (auto *pair = m_pairs_by_client[client_handle]; pair != nullptr) {
            pair->pending_accept = true;
        }
    }
    m_cv.notify_all();
}

void Kernel::SetMitmPort(const char *service_name, Handle port_handle) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_mitm_ports[service_name] = port_handle;
    Log("mitm installed for '%s' -> port %#x", service_name, port_handle);
}

void Kernel::ClearMitmPort(const char *service_name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_mitm_ports.erase(service_name);
}

SessionPair *Kernel::FindPairByHandleLocked(Handle h, bool want_server) {
    auto &map = want_server ? m_pairs_by_server : m_pairs_by_client;
    auto it = map.find(h);
    return it == map.end() ? nullptr : it->second;
}

PortObject *Kernel::FindPortLocked(Handle h) {
    auto it = m_ports.find(h);
    return it == m_ports.end() ? nullptr : it->second;
}

SessionPair *Kernel::GetPairByClient(Handle h) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return FindPairByHandleLocked(h, false);
}

SessionPair *Kernel::GetPairByServer(Handle h) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return FindPairByHandleLocked(h, true);
}

void Kernel::SetHandler(Handle server_handle, std::function<Result(Handle)> handler) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto *pair = FindPairByHandleLocked(server_handle, true);
        if (pair == nullptr) {
            Fail("SetHandler: bad server handle");
            return;
        }
        pair->handler = std::move(handler);
        pair->pending_accept = false;
        Log("handler installed on '%s' server=%#x", pair->name.c_str(), server_handle);
    }
    m_cv.notify_all();
}

void Kernel::SetDefaultHandler(std::function<Result(Handle)> handler) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_default_handler = std::move(handler);
}

size_t Kernel::LiveSessionCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_pairs_by_client.size();
}

size_t Kernel::LiveHandleCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_pairs_by_client.size() + m_pairs_by_server.size() + m_ports.size();
}

Handle Kernel::ConnectToNamedPort(const char *name) {
    ConnectCallback callback;
    Handle mitm_port = InvalidHandle;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto mitm = m_mitm_ports.find(name);
        if (mitm != m_mitm_ports.end()) {
            mitm_port = mitm->second;
        } else {
            auto it = m_named_ports.find(name);
            if (it == m_named_ports.end()) {
                Fail("ConnectToNamedPort: unknown port");
                return InvalidHandle;
            }
            callback = it->second;
        }
    }

    /* Do not call these under the lock: they take it themselves. */
    if (mitm_port != InvalidHandle) {
        const Handle client = CreatePair(name);
        EnqueueOnPort(mitm_port, client);
        return client;
    }
    return callback(name);
}

/* Exact number of bytes a request occupies in the message buffer. */
size_t Kernel::LastRequestSize() {
    const HipcParsedRequest request = hipcParseRequest(t_tls);
    size_t size = sizeof(HipcHeader);
    if (request.meta.send_pid || request.meta.num_copy_handles || request.meta.num_move_handles) {
        size += sizeof(HipcSpecialHeader);
    }
    if (request.meta.send_pid) {
        size += sizeof(u64);
    }
    size += request.meta.num_copy_handles * sizeof(Handle);
    size += request.meta.num_move_handles * sizeof(Handle);
    size += request.meta.num_send_statics * sizeof(HipcStaticDescriptor);
    size += request.meta.num_send_buffers * sizeof(HipcBufferDescriptor);
    size += request.meta.num_recv_buffers * sizeof(HipcBufferDescriptor);
    size += request.meta.num_exch_buffers * sizeof(HipcBufferDescriptor);
    size += request.meta.num_data_words * sizeof(u32);
    return size;
}

bool Kernel::FindLastRequest(u32 type, u8 *out, size_t capacity, size_t *out_size) {
    std::lock_guard<std::mutex> lock(m_mutex);
    const size_t count = m_captured_count < MaxCapturedRequests ? m_captured_count : MaxCapturedRequests;
    for (size_t i = 0; i < count; ++i) {
        const size_t index = (m_captured_count - 1 - i) % MaxCapturedRequests;
        const auto &slot = m_captured[index];
        if (slot.type == type && slot.size <= capacity) {
            std::memcpy(out, slot.bytes, slot.size);
            *out_size = slot.size;
            return true;
        }
    }
    return false;
}

const u8 *Kernel::LastRequest(size_t *out_size) {
    std::lock_guard<std::mutex> lock(m_mutex);
    *out_size = m_last_request_size;
    return m_last_request;
}
Result Kernel::SendSyncRequest(Handle session_handle) {
    SessionPair *pair = nullptr;
    std::function<Result(Handle)> inline_handler;
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        pair = FindPairByHandleLocked(session_handle, false);
        if (pair == nullptr) {
            Fail("SendSyncRequest: not a client session handle");
            return 1;
        }
        /* On hardware the request simply queues until the server accepts the
         * session, so block here too instead of erroring out. */
        m_cv.wait(lock, [pair] { return !pair->pending_accept || pair->closed; });
        if (pair->closed) {
            return 1;
        }
        inline_handler = pair->handler ? pair->handler : m_default_handler;
    }

    /* Capture what the client is about to send (for the parity test). */
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_last_request_size = LastRequestSize();
        std::memcpy(m_last_request, t_tls, m_last_request_size);

        auto &slot = m_captured[m_captured_count % MaxCapturedRequests];
        slot.type = hipcParseRequest(t_tls).meta.type;
        slot.size = m_last_request_size;
        std::memcpy(slot.bytes, t_tls, m_last_request_size);
        ++m_captured_count;
    }
    /* Convenience mode: the "server" runs on this thread and works on this
     * thread's TLS, exactly like a synchronous in-process call. */
    if (inline_handler) {
        return inline_handler(pair->server_handle);
    }

    /* Threaded mode: queue the message and wait for a server thread. */
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::memcpy(pair->message, t_tls, sizeof(pair->message));
        pair->request_pending = true;
        pair->reply_ready = false;
    }
    m_cv.notify_all();

    std::unique_lock<std::mutex> lock(m_mutex);
    m_cv.wait(lock, [pair] { return pair->reply_ready || pair->closed; });
    if (pair->closed) {
        return 1;
    }
    std::memcpy(t_tls, pair->message, sizeof(pair->message));
    pair->reply_ready = false;
    return 0;
}

Result Kernel::AcceptSession(Handle *out, Handle port_handle) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto *port = FindPortLocked(port_handle);
    if (port == nullptr) {
        Fail("AcceptSession: bad port handle");
        return 1;
    }
    if (port->pending_clients.empty()) {
        Fail("AcceptSession: nothing pending");
        return 1;
    }
    const Handle client = port->pending_clients.front();
    port->pending_clients.erase(port->pending_clients.begin());
    auto *pair = FindPairByHandleLocked(client, false);
    if (pair == nullptr) {
        Fail("AcceptSession: stale client handle");
        return 1;
    }
    pair->pending_accept = false;
    *out = pair->server_handle;
    m_cv.notify_all();
    return 0;
}

Result Kernel::CreateSession(Handle *server, Handle *client) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto *pair = new SessionPair();
    pair->name = "created";
    pair->client_handle = m_next_session++;
    pair->server_handle = m_next_session++;
    m_pairs.push_back(pair);
    m_pairs_by_client[pair->client_handle] = pair;
    m_pairs_by_server[pair->server_handle] = pair;
    *server = pair->server_handle;
    *client = pair->client_handle;
    Log("svcCreateSession server=%#x client=%#x", *server, *client);
    return 0;
}

Result Kernel::CloseHandle(Handle h) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_events.erase(h) != 0) {
        return 0;
    }
    if (auto it = m_pairs_by_client.find(h); it != m_pairs_by_client.end()) {
        it->second->closed = true;
        m_pairs_by_client.erase(it);
        m_cv.notify_all();
        return 0;
    }
    if (auto it = m_pairs_by_server.find(h); it != m_pairs_by_server.end()) {
        it->second->closed = true;
        m_pairs_by_server.erase(it);
        m_cv.notify_all();
        return 0;
    }
    if (auto it = m_ports.find(h); it != m_ports.end()) {
        m_ports.erase(it);
        return 0;
    }
    Fail("CloseHandle: unknown handle");
    return 1;
}

/* svcReplyAndReceive: either post a reply (handle_count == 0) or block until
 * one of `handles` has something to deliver. */
Result Kernel::ReplyAndReceive(s32 *index, const Handle *handles, s32 handle_count, Handle reply_target) {
    if (handle_count == 0 && reply_target != InvalidHandle) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto *pair = FindPairByHandleLocked(reply_target, true);
        if (pair == nullptr) {
            Fail("ReplyAndReceive: reply on a bad session handle");
            return 1;
        }
        std::memcpy(pair->message, t_tls, sizeof(pair->message));
        pair->reply_ready = true;
        pair->request_pending = false;
        m_cv.notify_all();
        return 0;
    }

    std::unique_lock<std::mutex> lock(m_mutex);
    auto ready = [&]() -> bool {
        for (s32 i = 0; i < handle_count; ++i) {
            const Handle h = handles[i];
            if (auto *port = FindPortLocked(h); port != nullptr) {
                if (!port->pending_clients.empty()) {
                    return true;
                }
                continue;
            }
            if (auto *pair = FindPairByHandleLocked(h, true); pair != nullptr) {
                if (pair->request_pending) {
                    return true;
                }
                continue;
            }
            /* The handle is gone (e.g. the port was closed to stop the loop):
             * wake up so the caller can notice. */
            return true;
        }
        return false;
    };

    m_cv.wait(lock, ready);

    for (s32 i = 0; i < handle_count; ++i) {
        const Handle h = handles[i];
        if (auto *port = FindPortLocked(h); port != nullptr && !port->pending_clients.empty()) {
            *index = i;
            return 0;
        }
        if (auto *pair = FindPairByHandleLocked(h, true); pair != nullptr && pair->request_pending) {
            /* Hand the request to this thread: it now owns the message. */
            std::memcpy(t_tls, pair->message, sizeof(t_tls));
            pair->request_pending = false;
            *index = i;
            return 0;
        }
    }

    *index = -1;
    return 1;
}

u8 *Kernel::Tls() {
    return t_tls;
}

Handle Kernel::CreateEventPair() {
    std::lock_guard<std::mutex> lock(m_mutex);
    const Handle write = m_next_event++;
    const Handle read = m_next_event++;
    m_events[write] = 0;
    m_events[read] = 0;
    Log("create event write=%#x read=%#x", write, read);
    return write;
}

bool Kernel::IsEventHandle(Handle h) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_events.find(h) != m_events.end();
}

int Kernel::SignalCount(Handle h) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_events.find(h);
    return it == m_events.end() ? -1 : it->second;
}

} // namespace mock

/* ------------------------------------------------------------------------- */
/* Kernel entry points.                                                       */
/* ------------------------------------------------------------------------- */

extern "C" {

void *armGetTls(void) {
    return mock::t_tls;
}

Result svcSendSyncRequest(Handle session) {
    return mock::K().SendSyncRequest(session);
}

Result svcCloseHandle(Handle handle) {
    return mock::K().CloseHandle(handle);
}

Result svcConnectToNamedPort(Handle *session, const char *name) {
    const Handle h = mock::K().ConnectToNamedPort(name);
    if (h == mock::InvalidHandle) {
        return 1;
    }
    *session = h;
    return 0;
}

Result svcCreateSession(Handle *server_handle, Handle *client_handle, u32 unk0, u64 unk1) {
    (void)unk0;
    (void)unk1;
    return mock::K().CreateSession(server_handle, client_handle);
}

Result svcAcceptSession(Handle *session_handle, Handle port_handle) {
    return mock::K().AcceptSession(session_handle, port_handle);
}

Result svcReplyAndReceive(s32 *index, const Handle *handles, s32 handleCount, Handle replyTarget, u64 timeout) {
    (void)timeout;
    return mock::K().ReplyAndReceive(index, handles, handleCount, replyTarget);
}

Result svcBreak(u32 breakReason, uintptr_t address, uintptr_t size) {
    (void)breakReason;
    (void)address;
    (void)size;
    std::fprintf(stderr, "[mock] svcBreak(0x%x)\n", breakReason);
    mock::K().Fail("svcBreak");
    return 0;
}

Result svcGetThreadId(u64 *threadID, Handle handle) {
    (void)handle;
    *threadID = 1;
    return 0;
}

Result svcGetInfo(u64 *out, u32 id0, Handle handle, u64 id1) {
    (void)id0;
    (void)handle;
    (void)id1;
    *out = 0;
    return 0;
}

Result svcSetHeapSize(void **out_addr, u64 size) {
    (void)size;
    *out_addr = nullptr;
    return 0;
}

void svcSleepThread(s64 ns) {
    (void)ns;
}

u64 svcGetSystemTick(void) {
    return 0;
}

Result svcCreateEvent(Handle *server_handle, Handle *client_handle) {
    const Handle write = mock::K().CreateEventPair();
    *server_handle = write;
    *client_handle = write + 1;
    return 0;
}

Result svcSignalEvent(Handle handle) {
    (void)handle;
    return 0;
}

Result svcClearEvent(Handle handle) {
    (void)handle;
    return 0;
}

} // extern "C"
