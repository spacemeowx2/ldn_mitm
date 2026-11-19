/*
 * ipc_server.hpp -- libnx-only replacement for the slice of libstratosphere's
 * sf/cmif/hipc server that ldn_mitm actually needs.
 *
 * What this provides (and libnx does NOT):
 *   - parse an incoming CMIF request out of the TLS message buffer
 *   - dispatch it to a ServerObject with a plain (cmd_id -> handler) table
 *   - build a spec-correct CMIF reply: CmifOutHeader, out data, out objects
 *     (move-handle for plain sessions, object id for domain sessions)
 *   - session table + domain object table
 *   - mitm sessions: "forward the raw request to the real service" when a
 *     handler returns ResultShouldForwardToSession
 *
 * Everything here is plain libnx (switch.h) + freestanding C++ so it compiles
 * both for aarch64 (target) and for the host-side mock kernel (tests).
 */
#pragma once

#include <switch.h>

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace ldn_nx {

/* "SFCI" / "SFCO" */
constexpr u32 CmifInHeaderMagic  = 0x49434653u;
constexpr u32 CmifOutHeaderMagic = 0x4F434653u;

/* ams::sm::mitm::ResultShouldForwardToSession (module 21, desc 1000). */
constexpr Result ResultShouldForwardToSession = (21u << 9) | 1000u;

constexpr size_t TlsMessageBufferSize = 0x1000;
constexpr size_t MaxOutObjects    = 4;
constexpr size_t MaxOutHandles    = 4;
constexpr size_t ReplyScratchSize = 0x2000;

constexpr inline size_t AlignUp(size_t value, size_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

class ServerObject;
class IpcServer;

/* ------------------------------------------------------------------------- */
/* Parsed request                                                            */
/* ------------------------------------------------------------------------- */
struct Request {
    void *base    = nullptr;
    Handle session = 0;
    HipcParsedRequest hipc{};

    u8   type         = 0;   /* CmifCommandType                                   */
    bool is_domain    = false;
    bool close_object = false; /* domain: request asks to destroy one object      */
    u32  object_id    = 0;   /* domain: which object in the session's domain      */
    u32  cmd_id    = 0;
    u32  token     = 0;
    u32  version   = 0;

    const u8 *in_data = nullptr;
    u32  in_size      = 0;

    bool IsRequest() const {
        return this->type == CmifCommandType_Request || this->type == CmifCommandType_RequestWithContext;
    }
    bool IsControl() const {
        return this->type == CmifCommandType_Control || this->type == CmifCommandType_ControlWithContext;
    }
    bool IsClose() const {
        return this->type == CmifCommandType_Close;
    }

    template<typename T>
    const T *In() const { return reinterpret_cast<const T *>(this->in_data); }

    bool InSizeOk(size_t size) const { return this->in_size >= size; }

    /* --- HIPC buffers --------------------------------------------------- */
    /* Server pointers (recv list / send statics) are what an "auto select"
     * buffer resolves to when it fits the pointer buffer; otherwise the client
     * sends a map-alias buffer descriptor. These helpers hide that. */
    const u8 *InPointer(u32 index, size_t *out_size) const;
    const u8 *InBuffer(u32 index, size_t *out_size) const;
    const u8 *InAutoSelect(size_t *out_size) const;

    u8 *OutPointer(u32 index, size_t *out_capacity) const;
    u8 *OutBuffer(u32 index, size_t *out_capacity) const;
    u8 *OutAutoSelect(size_t *out_capacity) const;

    /* Handles attached to the request (copy handles). */
    Handle InHandle(u32 index) const;
    u32 InHandleCount() const { return this->hipc.meta.num_copy_handles; }
};

/* ------------------------------------------------------------------------- */
/* Reply builder                                                             */
/* ------------------------------------------------------------------------- */
class Reply {
public:
    Reply(IpcServer &server, Request &req) : m_server(server), m_req(req) {}

    /* Output payload. Pointers stay valid until Finish(). */
    template<typename T>
    T *Out() {
        return static_cast<T *>(this->Alloc(sizeof(T), alignof(T)));
    }
    void *Alloc(size_t size, size_t alignment = 4);

    /* Return a new object to the client. Ownership passes to the server. */
    void AddObject(ServerObject *object);

    /* Return a handle (the session must already be registered). */
    void AddMoveHandle(Handle handle);

    /* Return a handle by copying it (the caller keeps its own). */
    void AddCopyHandle(Handle handle);

    /* Declare one out server-pointer: the reply carries a send-static
     * descriptor plus a size-table entry with the number of bytes written. */
    void AddOutPointer(const void *address, size_t size);

    Result Finish(Result rc);

private:
    IpcServer &m_server;
    Request &m_req;

    alignas(16) u8 m_scratch[ReplyScratchSize] = {};
    size_t m_used = 0;

    ServerObject *m_objects[MaxOutObjects] = {};
    u32    m_object_ids[MaxOutObjects] = {};
    Handle m_object_handles[MaxOutObjects] = {};
    size_t m_num_objects = 0;

    Handle m_move_handles[MaxOutHandles] = {};
    Handle m_copy_handles[MaxOutHandles] = {};
    size_t m_num_move_handles = 0;
    size_t m_num_copy_handles = 0;

    const void *m_out_pointers[MaxOutObjects] = {};
    size_t m_out_pointer_sizes[MaxOutObjects] = {};
    size_t m_num_out_pointers = 0;
};

/* ------------------------------------------------------------------------- */
/* Service object                                                            */
/* ------------------------------------------------------------------------- */
class ServerObject {
public:
    virtual ~ServerObject() = default;

    /* Handle one CMIF request. Return 0 for success, or an error Result.
     * Returning ResultShouldForwardToSession makes the server forward the raw
     * request to the real service (mitm sessions only). */
    virtual Result Invoke(Request &req, Reply &reply) = 0;

    /* Called when the session/domain object is closed. */
    virtual void Release() {}
};

/* ------------------------------------------------------------------------- */
/* Server                                                                    */
/* ------------------------------------------------------------------------- */

/* The client identity sm reports when a mitm session is acknowledged. */
struct MitmClientInfo {
    u64 process_id = 0;
    u64 program_id = 0;
};

class IpcServer {
public:
    static constexpr size_t MaxSessions      = 6;
    static constexpr size_t MaxDomainObjects = 16;

    IpcServer() = default;
    ~IpcServer();

    /* Attach the mitm port handle returned by sm's InstallMitm. */
    void AttachPort(Handle port, const char *name);

    /* Accept one pending client on the mitm port, acknowledge it with sm and
     * register the resulting session. The real target loop calls this from
     * Run(); tests call it directly to keep the mock kernel synchronous. */
    Result AcceptMitmSession();

    /* Serve a session that is not reached through the port (e.g. the mitm
     * ShouldMitm query session handed to us by InstallMitm). */
    Result AttachSession(Handle server_handle, ServerObject *object);

    /* The real target loop: accepts clients, dispatches, replies. It waits on
     * the mitm port plus every session it is serving (what libstratosphere did
     * with an os::MultiWait). Returns when Stop() is called. */
    void Run();

    /* Asks Run() to return. The caller is responsible for making the wait set
     * wake up (closing the port does it). */
    void Stop() { m_stop = true; }

    /* Test entry point / shared logic: handle exactly one incoming message on
     * `session_handle`. The request is expected to already be in the TLS
     * message buffer. */
    Result ProcessSession(Handle session_handle);

    /* Session bookkeeping. */
    Result RegisterSession(Handle server_handle, ServerObject *object, bool is_mitm, Handle forward_handle, u64 client_pid);
    Result CreateOutSession(ServerObject *object, Handle *out_client_handle);
    Result AllocateDomainObject(Handle session, ServerObject *object, u32 *out_id);

    ServerObject *GetObject(Handle session_handle);
    Handle GetPortHandle() const { return m_port; }

protected:
    /* Builds the service object that serves one acknowledged mitm client.
     * Ownership passes to the server. */
    virtual ServerObject *CreateMitmObject(Handle forward_session, const MitmClientInfo &client_info) {
        (void)forward_session;
        (void)client_info;
        return nullptr;
    }

private:
    struct Session {
        Handle        handle = 0;
        ServerObject *object = nullptr;
        bool          is_mitm = false;
        Handle        forward_handle = 0;
        u64           client_pid = 0;
        bool          is_domain = false;
        u32           domain_ids[MaxDomainObjects] = {};
        ServerObject *domain_objects[MaxDomainObjects] = {};
        size_t        num_domain_objects = 0;
        u8            saved_request[TlsMessageBufferSize] = {};
        size_t        saved_size = 0;
    };

    Session *FindSession(Handle handle);
    Result ForwardRequest(Session *session, Request &req);
    Result Dispatch(Request &req, Session *session, Reply &reply);

    Result HandleControl(Request &req, Session *session, Reply &reply);
    Result HandleDomainRequest(Request &req, Session *session, Reply &reply);
    Result ConvertToDomain(Session *session, Reply &reply);
    Result CopyFromDomain(Session *session, u32 object_id, Reply &reply);

    Session m_sessions[MaxSessions] = {};
    Handle  m_port = 0;
    char    m_name[9] = {};
    std::atomic<bool> m_stop{false};
};

} // namespace ldn_nx
