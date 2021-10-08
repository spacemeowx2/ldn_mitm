/*
 * Copyright (c) 2018 Atmosphère-NX
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stratosphere.hpp>

#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <malloc.h>

#include <switch.h>

#include "ldnmitm_service.hpp"

namespace ams {

    namespace {

        constexpr size_t MallocBufferSize = 1_MB;
        alignas(os::MemoryPageSize) constinit u8 g_malloc_buffer[MallocBufferSize];

    }

    namespace mitm {

        namespace {

            struct LdnMitmManagerOptions {
                static constexpr size_t PointerBufferSize = 0x1000;
                static constexpr size_t MaxDomains = 0x10;
                static constexpr size_t MaxDomainObjects = 0x100;
            };

            class ServerManager final : public sf::hipc::ServerManager<1, LdnMitmManagerOptions, 3> {
                        private:
                            virtual ams::Result OnNeedsToAccept(int port_index, Server *server) override;
            };

            ServerManager g_server_manager;

            Result ServerManager::OnNeedsToAccept(int port_index, Server *server) {
                AMS_UNUSED(port_index);
                /* Acknowledge the mitm session. */
                std::shared_ptr<::Service> fsrv;
                sm::MitmProcessInfo client_info;
                server->AcknowledgeMitmSession(std::addressof(fsrv), std::addressof(client_info));
                return this->AcceptMitmImpl(server, sf::CreateSharedObjectEmplaced<mitm::ldn::ILdnMitMService, mitm::ldn::LdnMitMService>(decltype(fsrv)(fsrv), client_info), fsrv);
            }

        }

    }

    namespace init {

        void InitializeSystemModule() {
            /* Sleep 10 seconds (seems unnecessary). */
            os::SleepThread(TimeSpan::FromSeconds(10));

            /* Initialize our connection to sm. */
            R_ABORT_UNLESS(sm::Initialize());

            /* Initialize fs. */
            fs::InitializeForSystem();
            fs::SetEnabledAutoAbort(false);

            /* Initialize other services. */
            const SocketInitConfig socketInitConfig = {
                .bsdsockets_version = 1,

                .tcp_tx_buf_size = 0x800,
                .tcp_rx_buf_size = 0x1000,
                .tcp_tx_buf_max_size = 0x2000,
                .tcp_rx_buf_max_size = 0x2000,

                .udp_tx_buf_size = 0x2000,
                .udp_rx_buf_size = 0x2000,

                .sb_efficiency = 4,

                .num_bsd_sessions = 3,
                .bsd_service_type = BsdServiceType_User,
            };

            R_ABORT_UNLESS(ipinfoInit());
            R_ABORT_UNLESS(socketInitialize(&socketInitConfig));
            R_ABORT_UNLESS(fsdevMountSdmc());

            LogFormat("InitializeSystemModule done");
        }

        void FinalizeSystemModule() { /* ... */ }

        void Startup() {
            /* Initialize the global malloc allocator. */
            init::InitializeAllocator(g_malloc_buffer, sizeof(g_malloc_buffer));
        }

    }

    void NORETURN Exit(int rc) {
        AMS_UNUSED(rc);
        AMS_ABORT("Exit called by immortal process");
    }

    void Main() {
        LogFormat("main");

        constexpr sm::ServiceName MitmServiceName = sm::ServiceName::Encode("ldn:u");
        //sf::hipc::ServerManager<2, LdnMitmManagerOptions, 3> server_manager;
        R_ABORT_UNLESS((mitm::g_server_manager.RegisterMitmServer<mitm::ldn::LdnMitMService>(0, MitmServiceName)));
        LogFormat("registered");

        mitm::g_server_manager.LoopProcess();
    }

}
