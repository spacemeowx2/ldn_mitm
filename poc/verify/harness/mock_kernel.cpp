#include "mock_kernel.h"

#include <cstdarg>

namespace mock {

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

} // namespace mock

/* ------------------------------------------------------------------------- */

namespace mock {


void Kernel::Reset() {
    for (auto *p : m_pairs) {
        delete p;
    }
    for (auto *p : m_port_objects) {
        delete p;
    }
    m_pairs.clear();
    m_port_objects.clear();
    m_pairs_by_client.clear();
    m_pairs_by_server.clear();
    m_ports.clear();
    m_named_ports.clear();
    m_next_session = SessionBase;
    m_next_port = PortBase;
    m_failures = 0;
    std::memset(m_tls, 0, sizeof(m_tls));
}

void Kernel::RegisterNamedPort(const char *name, ConnectCallback cb) {
    m_named_ports[name] = std::move(cb);
}

Handle Kernel::CreatePair(const std::string &name) {
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
    auto *port = new PortObject();
    port->name = name;
    const Handle h = m_next_port++;
    m_port_objects.push_back(port);
    m_ports[h] = port;
    Log("create port '%s' handle=%#x", name, h);
    return h;
}

void Kernel::EnqueueOnPort(Handle port_handle, Handle client_handle) {
    auto it = m_ports.find(port_handle);
    if (it == m_ports.end()) {
        Fail("EnqueueOnPort: bad port handle");
        return;
    }
    it->second->pending_clients.push_back(client_handle);
    if (auto *pair = GetPairByClient(client_handle)) {
        pair->pending_accept = true;
    }
}

SessionPair *Kernel::GetPairByClient(Handle h) {
    auto it = m_pairs_by_client.find(h);
    return it == m_pairs_by_client.end() ? nullptr : it->second;
}

SessionPair *Kernel::GetPairByServer(Handle h) {
    auto it = m_pairs_by_server.find(h);
    return it == m_pairs_by_server.end() ? nullptr : it->second;
}

void Kernel::SetHandler(Handle server_handle, std::function<Result(Handle)> handler) {
    auto *pair = GetPairByServer(server_handle);
    if (pair == nullptr) {
        Fail("SetHandler: bad server handle");
        return;
    }
    pair->handler = std::move(handler);
    pair->pending_accept = false;
    Log("handler installed on '%s' server=%#x", pair->name.c_str(), server_handle);
}

void Kernel::SetDefaultHandler(std::function<Result(Handle)> handler) {
    m_default_handler = std::move(handler);
}

size_t Kernel::LiveSessionCount() const {
    return m_pairs_by_client.size();
}

size_t Kernel::LiveHandleCount() const {
    return m_pairs_by_client.size() + m_pairs_by_server.size() + m_ports.size();
}

Handle Kernel::ConnectToNamedPort(const char *name) {
    auto it = m_named_ports.find(name);
    if (it == m_named_ports.end()) {
        Fail("ConnectToNamedPort: unknown port");
        return InvalidHandle;
    }
    return it->second(name);
}

Result Kernel::SendSyncRequest(Handle session_handle) {
    auto *pair = GetPairByClient(session_handle);
    if (pair == nullptr) {
        Fail("SendSyncRequest: not a client session handle");
        return 1;
    }
    if (pair->pending_accept) {
        Fail("SendSyncRequest: session not accepted by server yet");
        return 1;
    }
    const auto &handler = pair->handler ? pair->handler : m_default_handler;
    if (!handler) {
        Fail("SendSyncRequest: no server handler installed");
        return 1;
    }
    return handler(pair->server_handle);
}

Result Kernel::AcceptSession(Handle *out, Handle port_handle) {
    auto it = m_ports.find(port_handle);
    if (it == m_ports.end()) {
        Fail("AcceptSession: bad port handle");
        return 1;
    }
    auto &pending = it->second->pending_clients;
    if (pending.empty()) {
        Fail("AcceptSession: nothing pending");
        return 1;
    }
    const Handle client = pending.front();
    pending.erase(pending.begin());
    auto *pair = GetPairByClient(client);
    if (pair == nullptr) {
        Fail("AcceptSession: stale client handle");
        return 1;
    }
    *out = pair->server_handle;
    return 0;
}

Result Kernel::CreateSession(Handle *server, Handle *client) {
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
    if (auto it = m_pairs_by_client.find(h); it != m_pairs_by_client.end()) {
        m_pairs_by_client.erase(it);
        return 0;
    }
    if (auto it = m_pairs_by_server.find(h); it != m_pairs_by_server.end()) {
        m_pairs_by_server.erase(it);
        return 0;
    }
    if (auto it = m_ports.find(h); it != m_ports.end()) {
        m_ports.erase(it);
        return 0;
    }
    Fail("CloseHandle: unknown handle");
    return 1;
}

} // namespace mock

/* ------------------------------------------------------------------------- */
/* Kernel entry points.                                                       */
/* ------------------------------------------------------------------------- */

extern "C" {

void *armGetTls(void) {
    return mock::K().Tls();
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

/* The mock server is synchronous; the reply is already in place. */
Result svcReplyAndReceive(s32 *index, const Handle *handles, s32 handleCount, Handle replyTarget, u64 timeout) {
    (void)index;
    (void)handles;
    (void)handleCount;
    (void)replyTarget;
    (void)timeout;
    return 0;
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

} // extern "C"
