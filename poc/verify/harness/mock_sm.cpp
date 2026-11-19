#include "mock_sm.h"

#include <switch/sf/hipc.h>

#include <cstring>
#include <string>

namespace mock {

namespace {

constexpr u32 SmCommand_TipcOffset = 16; /* TIPC encodes cmd id as type + 16 */

constexpr u32 SmCommand_Initialize        = 0;
constexpr u32 SmCommand_DetachClient      = 4;
constexpr u32 SmCommand_InstallMitm       = 65000;
constexpr u32 SmCommand_UninstallMitm     = 65001;
constexpr u32 SmCommand_AcknowledgeSession = 65003;

struct MockSmState {
    Handle mitm_port = InvalidHandle;
    Handle query_client = InvalidHandle;
    Handle query_server = InvalidHandle;
    Handle real_client = InvalidHandle;
    std::string name;
    RealServiceFactory factory;
    u64 client_pid = 0x00000000CAFEBABEull;
    u64 client_program_id = 0x0100000000001000ull;
};

MockSmState g_sm;

void SetSmClientPid(u64 pid) { g_sm.client_pid = pid; }

void SetSmClientProgramId(u64 program_id) { g_sm.client_program_id = program_id; }

void WriteResponse(Result rc, const void *payload, u32 size, const Handle *handles, u32 num_handles) {
    const u32 num_words = (sizeof(u32) + size + sizeof(u32) - 1) / sizeof(u32);
    const HipcRequest response = hipcMakeRequestInline(armGetTls(),
        .type             = 0, /* TIPC response */
        .num_data_words   = num_words,
        .num_move_handles = num_handles);

    auto *data = reinterpret_cast<u32 *>(response.data_words);
    data[0] = rc;
    if (size != 0) {
        std::memcpy(data + 1, payload, size);
    }
    for (u32 i = 0; i < num_handles; ++i) {
        response.move_handles[i] = handles[i];
    }
}

Result SmServerProcess(Handle server_handle) {
    (void)server_handle;

    const HipcParsedRequest request = hipcParseRequest(armGetTls());
    const u32 command = request.meta.type - SmCommand_TipcOffset;
    const auto *in = reinterpret_cast<const u8 *>(request.data.data_words);

    Kernel::Get().Log("sm: command %u", command);

    switch (command) {
        case SmCommand_Initialize: {
            /* On hardware the *kernel* fills in the sender pid; libnx never
             * writes it, so whatever bytes sit at that offset are meaningless
             * (they are often part of the service name). Keep the canned
             * identity; tests use SetSmClientPid() when they need another. */
            WriteResponse(0, nullptr, 0, nullptr, 0);
            return 0;
        }
        case SmCommand_DetachClient: {
            WriteResponse(0, nullptr, 0, nullptr, 0);
            return 0;
        }
        case SmCommand_InstallMitm: {
            char name[9] = {};
            std::memcpy(name, in, 8);
            g_sm.name = name;

            /* The mitm owns the port it accepts clients on... */
            g_sm.mitm_port = Kernel::Get().CreatePort(name);
            Kernel::Get().SetMitmPort(name, g_sm.mitm_port);

            /* ...and the server side of the query session sm uses to ask
             * ShouldMitm(). The client side stays with "sm". */
            const Handle query_client = Kernel::Get().CreatePair("mitm-query");
            g_sm.query_client = query_client;
            g_sm.query_server = Kernel::Get().GetPairByClient(query_client)->server_handle;

            /* Session to the real service, created once per install. */
            g_sm.real_client = g_sm.factory ? g_sm.factory() : InvalidHandle;

            const Handle handles[2] = { g_sm.mitm_port, g_sm.query_server };
            WriteResponse(0, nullptr, 0, handles, 2);
            return 0;
        }
        case SmCommand_UninstallMitm: {
            if (!g_sm.name.empty()) {
                Kernel::Get().ClearMitmPort(g_sm.name.c_str());
            }
            WriteResponse(0, nullptr, 0, nullptr, 0);
            return 0;
        }
        case SmCommand_AcknowledgeSession: {
            /* Returns MitmProcessInfo (0x20 bytes) + the real service session. */
            struct {
                u64 process_id;
                u64 program_id;
                u64 keys_held;
                u64 flags;
            } info = {};

            info.process_id = g_sm.client_pid;
            info.program_id = g_sm.client_program_id;

            const Handle handle = g_sm.real_client;
            WriteResponse(0, &info, sizeof(info), &handle, 1);
            return 0;
        }
        default: {
            WriteResponse(0x2FFFFF, nullptr, 0, nullptr, 0);
            return 0;
        }
    }
}

Handle SmPortConnect(const char *name) {
    (void)name;
    const Handle client = Kernel::Get().CreatePair("sm");
    const Handle server = Kernel::Get().GetPairByClient(client)->server_handle;
    Kernel::Get().SetHandler(server, SmServerProcess);
    return client;
}

} // namespace

void InstallMockSm(RealServiceFactory factory) {
    g_sm = MockSmState{};
    g_sm.factory = std::move(factory);
    Kernel::Get().RegisterNamedPort("sm:", SmPortConnect);

    /* The service the mitm is installed for, reachable directly as long as no
     * mitm is registered for that name. */
    Kernel::Get().RegisterNamedPort("ldn:u", [](const char *) -> Handle {
        return g_sm.factory ? g_sm.factory() : InvalidHandle;
    });
}

Handle GetSmQueryClient() {
    return g_sm.query_client;
}

Handle GetMitmPort() {
    return g_sm.mitm_port;
}

} // namespace mock
