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
 
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <malloc.h>

#include <switch.h>
#include <stratosphere.hpp>

#include "ldnmitm_service.hpp"

extern "C" {
    extern u32 __start__;

    #define INNER_HEAP_SIZE 0x100000
    size_t nx_inner_heap_size = INNER_HEAP_SIZE;
    char   nx_inner_heap[INNER_HEAP_SIZE];
}

namespace ams {
    ncm::ProgramId CurrentProgramId = { 0x4200000000000010ul };

    namespace result {
        bool CallFatalOnResultAssertion = false;
    }
}
using namespace ams;

struct LdnMitmManagerOptions {
    static constexpr size_t PointerBufferSize = 0x1000;
    static constexpr size_t MaxDomains = 0x10;
    static constexpr size_t MaxDomainObjects = 0x100;
    static constexpr bool CanDeferInvokeRequest = false;
    static constexpr bool CanManageMitmServers  = true;
};

class ServerManager final : public sf::hipc::ServerManager<1, LdnMitmManagerOptions, 3> {
    private:
        virtual ams::Result OnNeedsToAccept(int port_index, Server *server) override;
};

ServerManager g_server_manager;

ams::Result ServerManager::OnNeedsToAccept(int port_index, Server *server) {
    (void)port_index;

    /* Acknowledge the mitm session. */
    std::shared_ptr<::Service> fsrv;
    sm::MitmProcessInfo client_info;
    server->AcknowledgeMitmSession(std::addressof(fsrv), std::addressof(client_info));
    return this->AcceptMitmImpl(server, sf::CreateSharedObjectEmplaced<ams::mitm::ldn::ILdnMitMService, ams::mitm::ldn::LdnMitMService>(decltype(fsrv)(fsrv), client_info), fsrv);   
}

namespace ams {
    void Main() {
        LogFormat("main");

        constexpr sm::ServiceName MitmServiceName = sm::ServiceName::Encode("ldn:u");
        //sf::hipc::ServerManager<2, LdnMitmManagerOptions, 3> server_manager;
        R_ABORT_UNLESS((g_server_manager.RegisterMitmServer<ams::mitm::ldn::LdnMitMService>(0, MitmServiceName)));
        LogFormat("registered");

        g_server_manager.LoopProcess();
    }
}
