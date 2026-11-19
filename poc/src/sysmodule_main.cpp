/*
 * sysmodule_main.cpp -- the sysmodule entry point for the libnx-only PoC.
 *
 * Wiring (what ldn_mitm/source/ldnmitm_main.cpp does with libstratosphere):
 *   1. install the mitm for ldn:u with sm          (sm_mitm.c)
 *   2. serve the mitm port + the ShouldMitm query  (ipc_server + ldn_services)
 *   3. run the real LANDiscovery behind it         (lan_discovery.cpp)
 *
 * Built as a normal .elf by poc/Makefile so the whole thing is link-checked
 * against libnx before it ever reaches hardware.
 */
#include <switch.h>

#include <cstdio>
#include <cstring>

#include "debug.hpp"
#include "ipc_server.hpp"
#include "lan_discovery.hpp"
#include "ldn_services.hpp"
#include "mitm_query_service.hpp"
#include "sm_mitm.h"

namespace {

constexpr const char *MitmServiceName = "ldn:u";
constexpr const char *LogPath = "sdmc:/ldn_mitm_poc.log";

/* libnx's thread stack for the accept loop. */
constexpr size_t ServerStackSize = 0x4000;
alignas(0x1000) u8 g_server_stack[ServerStackSize];
Thread g_server_thread;

FILE *g_log_file = nullptr;

void FileLogSink(const char *text, size_t size) {
    if (g_log_file == nullptr) {
        return;
    }
    std::fwrite(text, 1, size, g_log_file);
    std::fflush(g_log_file);
}

/* ShouldMitm: this mitm always wants the client (matching the original). */
class LdnMitmQuery final : public ldn_nx::MitmQueryService {
    public:
        bool ShouldMitm(const ldn_nx::MitmClientInfo &client) override {
            LogFormat("should_mitm pid: %llu", static_cast<unsigned long long>(client.process_id));
            return true;
        }
};

class LdnMitmServer final : public ldn_nx::IpcServer {
    public:
        explicit LdnMitmServer(ams::mitm::ldn::LANDiscovery &lan) : m_lan(lan) { }

    protected:
        ldn_nx::ServerObject *CreateMitmObject(Handle forward_session, const ldn_nx::MitmClientInfo &) override {
            return new ams::mitm::ldn::ILdnMitMService(forward_session, m_lan);
        }

    private:
        ams::mitm::ldn::LANDiscovery &m_lan;
};

} // namespace

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    g_log_file = std::fopen(LogPath, "a");
    ams::log::SetSink(FileLogSink);
    (void)SetLogging(g_log_file != nullptr);

    LogFormat("ldn_mitm (libnx-only poc) starting");

    ams::mitm::ldn::LANDiscovery lan;

    LdnMitmServer *server = new LdnMitmServer(lan);
    LdnMitmQuery *query = new LdnMitmQuery();

    if (R_FAILED(ldn_sm_mitm_initialize())) {
        LogFormat("ldn_sm_mitm_initialize failed");
        return 1;
    }

    Handle port = 0;
    Handle query_session = 0;
    if (R_FAILED(ldn_sm_mitm_install(&port, &query_session, MitmServiceName))) {
        LogFormat("InstallMitm failed");
        return 1;
    }

    server->AttachPort(port, MitmServiceName);
    if (R_FAILED(server->AttachSession(query_session, query))) {
        LogFormat("could not serve the ShouldMitm query session");
        return 1;
    }

    LogFormat("registered");

    /* Serve the accept loop on its own thread so main can keep the process
     * alive even if the loop ever returns. */
    if (R_FAILED(threadCreate(&g_server_thread, [](void *arg) { static_cast<ldn_nx::IpcServer *>(arg)->Run(); },
                              server, g_server_stack, sizeof(g_server_stack), 0x15, 2))) {
        LogFormat("threadCreate failed");
        return 1;
    }
    threadStart(&g_server_thread);
    threadWaitForExit(&g_server_thread);

    return 0;
}
