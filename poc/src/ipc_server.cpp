#include "ipc_server.hpp"

#include "sm_mitm.h"

#include <cstring>

namespace ldn_nx {

namespace {

/* CMIF control command ids (cmifMakeControlRequest). */
constexpr u32 ControlCommand_ConvertCurrentObjectToDomain = 0;
constexpr u32 ControlCommand_CopyFromCurrentDomain        = 1;
constexpr u32 ControlCommand_CloneCurrentObject           = 2;
constexpr u32 ControlCommand_QueryPointerBufferSize       = 3;
constexpr u32 ControlCommand_CloneCurrentObjectEx         = 4;

constexpr u64 MitmProcessIdTag   = 0xFFFE000000000000ull;
constexpr u64 OldProcessIdMask   = 0x0000FFFFFFFFFFFFull;

void ParseCmif(Request *req, bool session_is_domain) {
    u8 *const base = static_cast<u8 *>(req->base);
    req->hipc = hipcParseRequest(base);
    req->type = static_cast<u8>(req->hipc.meta.type);

    u8 *const start = static_cast<u8 *>(cmifGetAlignedDataStart(req->hipc.data.data_words, base));

    /* Control messages are never wrapped in a domain header. */
    if (req->IsControl()) {
        auto *hdr = reinterpret_cast<CmifInHeader *>(start);
        req->cmd_id  = hdr->command_id;
        req->token   = hdr->token;
        req->version = hdr->version;
        req->in_data = reinterpret_cast<const u8 *>(hdr + 1);
        const size_t data_bytes = req->hipc.meta.num_data_words * sizeof(u32);
        const size_t off = static_cast<size_t>(reinterpret_cast<u8 *>(hdr) - reinterpret_cast<u8 *>(req->hipc.data.data_words));
        req->in_size = data_bytes > off + sizeof(CmifInHeader)
            ? static_cast<u32>(data_bytes - off - sizeof(CmifInHeader))
            : 0;
        return;
    }

    if (!req->IsRequest()) {
        return;
    }

    if (session_is_domain) {
        auto *dh = reinterpret_cast<CmifDomainInHeader *>(start);
        req->is_domain = true;
        req->object_id = dh->object_id;
        if (dh->type == CmifDomainRequestType_Close) {
            req->close_object = true;
            return;
        }
        if (dh->type != CmifDomainRequestType_SendMessage) {
            return;
        }
        auto *hdr = reinterpret_cast<CmifInHeader *>(dh + 1);
        req->cmd_id  = hdr->command_id;
        req->token   = hdr->token;
        req->version = hdr->version;
        req->in_data = reinterpret_cast<const u8 *>(hdr + 1);
        req->in_size = dh->data_size >= sizeof(CmifInHeader)
            ? static_cast<u32>(dh->data_size - sizeof(CmifInHeader))
            : 0;
        return;
    }

    auto *hdr = reinterpret_cast<CmifInHeader *>(start);
    req->cmd_id  = hdr->command_id;
    req->token   = hdr->token;
    req->version = hdr->version;
    req->in_data = reinterpret_cast<const u8 *>(hdr + 1);
    const size_t data_bytes = req->hipc.meta.num_data_words * sizeof(u32);
    const size_t off = static_cast<size_t>(reinterpret_cast<u8 *>(hdr) - reinterpret_cast<u8 *>(req->hipc.data.data_words));
    req->in_size = data_bytes > off + sizeof(CmifInHeader)
        ? static_cast<u32>(data_bytes - off - sizeof(CmifInHeader))
        : 0;
}

} // namespace

/* ------------------------------------------------------------------------- */
/* Request: HIPC buffers                                                     */
/* ------------------------------------------------------------------------- */
const u8 *Request::InPointer(u32 index, size_t *out_size) const {
    if (index >= this->hipc.meta.num_send_statics || this->hipc.data.send_statics == nullptr) {
        *out_size = 0;
        return nullptr;
    }
    const HipcStaticDescriptor *desc = &this->hipc.data.send_statics[index];
    *out_size = hipcGetStaticSize(desc);
    return static_cast<const u8 *>(hipcGetStaticAddress(desc));
}

const u8 *Request::InBuffer(u32 index, size_t *out_size) const {
    if (index >= this->hipc.meta.num_send_buffers || this->hipc.data.send_buffers == nullptr) {
        *out_size = 0;
        return nullptr;
    }
    const HipcBufferDescriptor *desc = &this->hipc.data.send_buffers[index];
    *out_size = hipcGetBufferSize(desc);
    return static_cast<const u8 *>(hipcGetBufferAddress(desc));
}

const u8 *Request::InAutoSelect(size_t *out_size) const {
    size_t size = 0;
    const u8 *pointer = this->InPointer(0, &size);
    if (pointer != nullptr && size != 0) {
        *out_size = size;
        return pointer;
    }
    return this->InBuffer(0, out_size);
}

u8 *Request::OutPointer(u32 index, size_t *out_capacity) const {
    if (index >= this->hipc.meta.num_recv_statics || this->hipc.data.recv_list == nullptr) {
        *out_capacity = 0;
        return nullptr;
    }
    const HipcRecvListEntry *entry = &this->hipc.data.recv_list[index];
    *out_capacity = entry->size;
    return reinterpret_cast<u8 *>(entry->address_low | (static_cast<uintptr_t>(entry->address_high) << 32));
}

u8 *Request::OutBuffer(u32 index, size_t *out_capacity) const {
    if (index >= this->hipc.meta.num_recv_buffers || this->hipc.data.recv_buffers == nullptr) {
        *out_capacity = 0;
        return nullptr;
    }
    const HipcBufferDescriptor *desc = &this->hipc.data.recv_buffers[index];
    *out_capacity = hipcGetBufferSize(desc);
    return static_cast<u8 *>(hipcGetBufferAddress(desc));
}

u8 *Request::OutAutoSelect(size_t *out_capacity) const {
    size_t capacity = 0;
    u8 *pointer = this->OutPointer(0, &capacity);
    if (pointer != nullptr && capacity != 0) {
        *out_capacity = capacity;
        return pointer;
    }
    return this->OutBuffer(0, out_capacity);
}

Handle Request::InHandle(u32 index) const {
    if (index >= this->hipc.meta.num_copy_handles || this->hipc.data.copy_handles == nullptr) {
        return 0;
    }
    return this->hipc.data.copy_handles[index];
}

/* ------------------------------------------------------------------------- */
/* Reply                                                                     */
/* ------------------------------------------------------------------------- */
void *Reply::Alloc(size_t size, size_t alignment) {
    const size_t offset = AlignUp(m_used, alignment);
    if (offset + size > sizeof(m_scratch)) {
        return nullptr;
    }
    std::memset(m_scratch + offset, 0, size);
    m_used = offset + size;
    return m_scratch + offset;
}

void Reply::AddObject(ServerObject *object) {
    if (object == nullptr || m_num_objects >= MaxOutObjects) {
        return;
    }
    if (m_req.is_domain) {
        u32 id = 0;
        if (R_SUCCEEDED(m_server.AllocateDomainObject(m_req.session, object, &id))) {
            m_object_ids[m_num_objects] = id;
        }
    } else {
        Handle client = 0;
        if (R_SUCCEEDED(m_server.CreateOutSession(object, &client))) {
            m_object_handles[m_num_objects] = client;
        }
    }
    m_num_objects++;
}

void Reply::AddMoveHandle(Handle handle) {
    if (m_num_move_handles < MaxOutHandles) {
        m_move_handles[m_num_move_handles++] = handle;
    }
}

void Reply::AddCopyHandle(Handle handle) {
    if (m_num_copy_handles < MaxOutHandles) {
        m_copy_handles[m_num_copy_handles++] = handle;
    }
}

void Reply::AddOutPointer(const void *address, size_t size) {
    if (m_num_out_pointers < MaxOutObjects) {
        m_out_pointers[m_num_out_pointers] = address;
        m_out_pointer_sizes[m_num_out_pointers] = size;
        ++m_num_out_pointers;
    }
}

Result Reply::Finish(Result rc) {
    u8 *const base = static_cast<u8 *>(m_req.base);
    const bool domain = m_req.is_domain;

    const size_t header_bytes = sizeof(CmifOutHeader) + (domain ? sizeof(CmifDomainOutHeader) : 0);
    const size_t object_bytes = domain ? m_num_objects * sizeof(u32) : 0;
    const size_t table_bytes  = m_num_out_pointers * sizeof(u16);
    const size_t raw_size     = header_bytes + m_used + object_bytes + table_bytes;
    const u32 num_data_words  = static_cast<u32>((AlignUp(raw_size, sizeof(u32)) + 0x10) / sizeof(u32));
    const size_t num_move     = m_num_move_handles + (domain ? 0 : m_num_objects);

    const HipcRequest response = hipcMakeRequestInline(base,
        .type             = CmifCommandType_Invalid, /* this is a response */
        .num_send_statics = static_cast<u32>(m_num_out_pointers),
        .num_data_words   = num_data_words,
        .num_copy_handles = static_cast<u32>(m_num_copy_handles),
        .num_move_handles = static_cast<u32>(num_move));

    u8 *data = static_cast<u8 *>(cmifGetAlignedDataStart(response.data_words, base));
    u8 *cursor = data;

    if (domain) {
        auto *domain_header = reinterpret_cast<CmifDomainOutHeader *>(cursor);
        std::memset(domain_header, 0, sizeof(*domain_header));
        domain_header->num_out_objects = static_cast<u32>(m_num_objects);
        cursor += sizeof(*domain_header);
    }

    auto *out_header = reinterpret_cast<CmifOutHeader *>(cursor);
    cursor += sizeof(*out_header);

    if (m_used > 0) {
        std::memcpy(cursor, m_scratch, m_used);
        cursor += m_used;
    }

    if (domain && m_num_objects > 0) {
        std::memcpy(cursor, m_object_ids, object_bytes);
        cursor += object_bytes;
    }

    if (m_num_out_pointers > 0) {
        auto *sizes = reinterpret_cast<u16 *>(cursor);
        for (size_t i = 0; i < m_num_out_pointers; ++i) {
            sizes[i] = static_cast<u16>(m_out_pointer_sizes[i]);
            response.send_statics[i] = hipcMakeSendStatic(m_out_pointers[i], m_out_pointer_sizes[i], static_cast<u8>(i));
        }
    }

    out_header->magic   = CmifOutHeaderMagic;
    out_header->version = 0;
    out_header->result  = rc;
    out_header->token   = m_req.token;

    size_t index = 0;
    if (!domain) {
        for (size_t i = 0; i < m_num_objects; ++i) {
            response.move_handles[index++] = m_object_handles[i];
        }
    }
    for (size_t i = 0; i < m_num_move_handles; ++i) {
        response.move_handles[index++] = m_move_handles[i];
    }
    for (size_t i = 0; i < m_num_copy_handles; ++i) {
        response.copy_handles[i] = m_copy_handles[i];
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
/* IpcServer                                                                 */
/* ------------------------------------------------------------------------- */
IpcServer::~IpcServer() {
    for (auto &session : m_sessions) {
        if (session.object != nullptr) {
            session.object->Release();
            delete session.object;
            session.object = nullptr;
        }
        for (size_t i = 0; i < session.num_domain_objects; ++i) {
            if (session.domain_objects[i] != nullptr) {
                session.domain_objects[i]->Release();
                delete session.domain_objects[i];
                session.domain_objects[i] = nullptr;
            }
        }
    }
}

void IpcServer::AttachPort(Handle port, const char *name) {
    m_port = port;
    std::strncpy(m_name, name, sizeof(m_name) - 1);
}

IpcServer::Session *IpcServer::FindSession(Handle handle) {
    for (auto &session : m_sessions) {
        if (session.handle == handle && session.handle != 0) {
            return &session;
        }
    }
    return nullptr;
}

Result IpcServer::RegisterSession(Handle server_handle, ServerObject *object, bool is_mitm, Handle forward_handle, u64 client_pid) {
    for (auto &session : m_sessions) {
        if (session.handle == 0) {
            session.handle         = server_handle;
            session.object         = object;
            session.is_mitm        = is_mitm;
            session.forward_handle = forward_handle;
            session.client_pid     = client_pid;
            session.is_domain      = false;
            session.num_domain_objects = 0;
            return 0;
        }
    }
    return 1;
}

Result IpcServer::CreateOutSession(ServerObject *object, Handle *out_client_handle) {
    Handle server_handle = 0;
    Handle client_handle = 0;
    if (R_FAILED(svcCreateSession(&server_handle, &client_handle, 0, 0))) {
        return 1;
    }
    if (R_FAILED(this->RegisterSession(server_handle, object, false, 0, 0))) {
        return 1;
    }
    *out_client_handle = client_handle;
    return 0;
}

Result IpcServer::AllocateDomainObject(Handle session_handle, ServerObject *object, u32 *out_id) {
    Session *session = this->FindSession(session_handle);
    if (session == nullptr || !session->is_domain) {
        return 1;
    }
    for (size_t i = 0; i < MaxDomainObjects; ++i) {
        if (session->domain_objects[i] == nullptr) {
            session->domain_ids[i]      = static_cast<u32>(i + 1);
            session->domain_objects[i]  = object;
            if (session->num_domain_objects < i + 1) {
                session->num_domain_objects = i + 1;
            }
            *out_id = session->domain_ids[i];
            return 0;
        }
    }
    return 1;
}

ServerObject *IpcServer::GetObject(Handle session_handle) {
    Session *session = this->FindSession(session_handle);
    return session != nullptr ? session->object : nullptr;
}

Result IpcServer::ConvertToDomain(Session *session, Reply &reply) {
    /* Move the session's current object into slot 0 of a fresh domain table. */
    auto *id = reply.Out<u32>();
    if (id == nullptr) {
        return 1;
    }
    for (size_t i = 0; i < MaxDomainObjects; ++i) {
        session->domain_objects[i] = nullptr;
        session->domain_ids[i] = 0;
    }
    session->num_domain_objects = 1;
    session->domain_ids[0]      = 1;
    session->domain_objects[0]  = session->object;
    session->object             = nullptr; /* ownership moved into the domain */
    session->is_domain          = true;
    *id = 1;
    return 0;
}

Result IpcServer::CopyFromDomain(Session *session, u32 object_id, Reply &reply) {
    if (object_id == 0 || object_id > MaxDomainObjects) {
        return 1;
    }
    ServerObject *object = session->domain_objects[object_id - 1];
    if (object == nullptr) {
        return 1;
    }
    Handle client = 0;
    if (R_FAILED(this->CreateOutSession(object, &client))) { return 1; }
    reply.AddMoveHandle(client);
    return 0;
}

Result IpcServer::HandleControl(Request &req, Session *session, Reply &reply) {
    switch (req.cmd_id) {
        case ControlCommand_ConvertCurrentObjectToDomain:
            return this->ConvertToDomain(session, reply);
        case ControlCommand_CopyFromCurrentDomain: {
            if (!req.InSizeOk(sizeof(u32))) {
                return 1;
            }
            return this->CopyFromDomain(session, *reinterpret_cast<const u32 *>(req.in_data), reply);
        }
        case ControlCommand_QueryPointerBufferSize: {
            auto *size = reply.Out<u16>();
            if (size == nullptr) {
                return 1;
            }
            *size = static_cast<u16>(TlsMessageBufferSize);
            return 0;
        }
        default:
            return 1;
    }
}

Result IpcServer::HandleDomainRequest(Request &req, Session *session, Reply &reply) {
    if (req.close_object) {
        const u32 id = req.object_id;
        if (id != 0 && id <= MaxDomainObjects) {
            ServerObject *object = session->domain_objects[id - 1];
            if (object != nullptr) {
                object->Release();
                delete object;
                session->domain_objects[id - 1] = nullptr;
            }
        }
        return 0;
    }
    if (req.object_id == 0 || req.object_id > MaxDomainObjects) {
        return 1;
    }
    ServerObject *object = session->domain_objects[req.object_id - 1];
    if (object == nullptr) {
        return 1;
    }
    return object->Invoke(req, reply);
}

Result IpcServer::Dispatch(Request &req, Session *session, Reply &reply) {
    if (session->object == nullptr) {
        return 1;
    }
    return session->object->Invoke(req, reply);
}

Result IpcServer::ForwardRequest(Session *session, Request &req) {
    (void)req;
    if (session->forward_handle == 0) {
        return 1;
    }

    u8 *const tls = static_cast<u8 *>(armGetTls());
    std::memcpy(tls, session->saved_request, sizeof(session->saved_request));

    /* Tag the process id so the target service can tell it is being mitm'd. */
    const HipcParsedRequest parsed = hipcParseRequest(tls);
    if (parsed.meta.send_pid) {
        auto *pid = reinterpret_cast<u64 *>(tls + sizeof(HipcHeader) + sizeof(HipcSpecialHeader));
        *pid = MitmProcessIdTag | (*pid & OldProcessIdMask);
    }

    return svcSendSyncRequest(session->forward_handle);
}

Result IpcServer::ProcessSession(Handle session_handle) {
    Session *session = this->FindSession(session_handle);
    if (session == nullptr || (session->object == nullptr && !session->is_domain)) {
        return 1;
    }

    void *const tls = armGetTls();
    std::memcpy(session->saved_request, tls, sizeof(session->saved_request));

    Request req{};
    req.base    = tls;
    req.session = session_handle;
    ParseCmif(&req, session->is_domain);

    Reply reply(*this, req);

    if (req.IsClose()) {
        if (session->object != nullptr) {
            session->object->Release();
            delete session->object;
            session->object = nullptr;
        }
        for (size_t i = 0; i < MaxDomainObjects; ++i) {
            if (session->domain_objects[i] != nullptr) {
                session->domain_objects[i]->Release();
                delete session->domain_objects[i];
                session->domain_objects[i] = nullptr;
            }
        }
        session->handle = 0;
        return 0;
    }

    Result rc;
    if (req.IsControl()) {
        rc = this->HandleControl(req, session, reply);
    } else if (session->is_domain) {
        rc = this->HandleDomainRequest(req, session, reply);
    } else {
        rc = this->Dispatch(req, session, reply);
    }

    if (rc == ResultShouldForwardToSession) {
        if (!session->is_mitm) {
            return reply.Finish(rc);
        }
        return this->ForwardRequest(session, req);
    }

    return reply.Finish(rc);
}

Result IpcServer::AttachSession(Handle server_handle, ServerObject *object) {
    return this->RegisterSession(server_handle, object, false, 0, 0);
}

Result IpcServer::AcceptMitmSession() {
    if (m_port == 0) {
        return 1;
    }

    Handle session_handle = 0;
    if (R_FAILED(svcAcceptSession(&session_handle, m_port))) {
        return 1;
    }

    /* Ask sm who the client is and for the session to the real service. */
    Handle forward_handle = 0;
    LdnMitmProcessInfo process_info{};
    if (R_FAILED(ldn_sm_mitm_acknowledge(&forward_handle, &process_info, m_name))) {
        svcCloseHandle(session_handle);
        return 1;
    }

    MitmClientInfo client_info{};
    client_info.process_id = process_info.process_id;
    client_info.program_id = process_info.program_id;

    ServerObject *object = this->CreateMitmObject(forward_handle, client_info);
    if (object == nullptr) {
        svcCloseHandle(session_handle);
        return 1;
    }

    if (R_FAILED(this->RegisterSession(session_handle, object, true, forward_handle, client_info.process_id))) {
        svcCloseHandle(session_handle);
        delete object;
        return 1;
    }

    return 0;
}

void IpcServer::Run() {
    if (m_port == 0) {
        return;
    }
    for (;;) {
        s32 index = 0;
        Handle handle = m_port;
        const Result rc = svcReplyAndReceive(&index, &handle, 1, 0, UINT64_MAX);
        if (R_FAILED(rc)) {
            continue;
        }

        if (handle == m_port) {
            this->AcceptMitmSession();
            continue;
        }

        this->ProcessSession(handle);
        svcReplyAndReceive(&index, nullptr, 0, handle, 0);
    }
}

} // namespace ldn_nx
