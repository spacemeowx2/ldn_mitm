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

extern "C" {

#include <switch/services/bsd.h>

}

#include "ldnmitm_service.hpp"

namespace ams {

    namespace {

        constexpr size_t MallocBufferSize = 1_MB;
        alignas(os::MemoryPageSize) constinit u8 g_malloc_buffer[MallocBufferSize];

        consteval size_t GetLibnxBsdTransferMemorySize(const ::SocketInitConfig *config) {
            const u32 tcp_tx_buf_max_size = config->tcp_tx_buf_max_size != 0 ? config->tcp_tx_buf_max_size : config->tcp_tx_buf_size;
            const u32 tcp_rx_buf_max_size = config->tcp_rx_buf_max_size != 0 ? config->tcp_rx_buf_max_size : config->tcp_rx_buf_size;
            const u32 sum = tcp_tx_buf_max_size + tcp_rx_buf_max_size + config->udp_tx_buf_size + config->udp_rx_buf_size;

            return config->sb_efficiency * util::AlignUp(sum, os::MemoryPageSize);
        }

        constexpr const ::SocketInitConfig LibnxSocketInitConfig = {
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

        alignas(os::MemoryPageSize) constinit u8 g_socket_tmem_buffer[GetLibnxBsdTransferMemorySize(std::addressof(LibnxSocketInitConfig))];

        constexpr const ::BsdInitConfig LibnxBsdInitConfig = {
            .version             = LibnxSocketInitConfig.bsdsockets_version,

            .tmem_buffer         = g_socket_tmem_buffer,
            .tmem_buffer_size    = sizeof(g_socket_tmem_buffer),

            .tcp_tx_buf_size     = LibnxSocketInitConfig.tcp_tx_buf_size,
            .tcp_rx_buf_size     = LibnxSocketInitConfig.tcp_rx_buf_size,
            .tcp_tx_buf_max_size = LibnxSocketInitConfig.tcp_tx_buf_max_size,
            .tcp_rx_buf_max_size = LibnxSocketInitConfig.tcp_rx_buf_max_size,

            .udp_tx_buf_size     = LibnxSocketInitConfig.udp_tx_buf_size,
            .udp_rx_buf_size     = LibnxSocketInitConfig.udp_rx_buf_size,

            .sb_efficiency       = LibnxSocketInitConfig.sb_efficiency,
        };

    }

    namespace mitm {

        const s32 ThreadPriority = 6;
        const size_t TotalThreads = 2;
        const size_t NumExtraThreads = TotalThreads - 1;
        const size_t ThreadStackSize = 0x4000;

        alignas(os::MemoryPageSize) u8 g_thread_stack[ThreadStackSize];
        os::ThreadType g_thread;

        alignas(0x40) constinit u8 g_heap_memory[128_KB];
        constinit lmem::HeapHandle g_heap_handle;
        constinit bool g_heap_initialized;
        constinit os::SdkMutex g_heap_init_mutex;

        lmem::HeapHandle GetHeapHandle()
        {
            if (AMS_UNLIKELY(!g_heap_initialized))
            {
                std::scoped_lock lk(g_heap_init_mutex);

                if (AMS_LIKELY(!g_heap_initialized))
                {
                    g_heap_handle = lmem::CreateExpHeap(g_heap_memory, sizeof(g_heap_memory), lmem::CreateOption_ThreadSafe);
                    g_heap_initialized = true;
                }
            }

            return g_heap_handle;
        }

        void *Allocate(size_t size)
        {
            return lmem::AllocateFromExpHeap(GetHeapHandle(), size);
        }

        void Deallocate(void *p, size_t size)
        {
            AMS_UNUSED(size);
            return lmem::FreeToExpHeap(GetHeapHandle(), p);
        }

        namespace {

            struct LdnMitmManagerOptions {
                static constexpr size_t PointerBufferSize = 0x1000;
                static constexpr size_t MaxDomains = 0x10;
                static constexpr size_t MaxDomainObjects = 0x100;
                static constexpr bool   CanDeferInvokeRequest = false;
                static constexpr bool   CanManageMitmServers  = true;
            };

            constexpr size_t MaxSessions = 3;

            class ServerManager final : public sf::hipc::ServerManager<1, LdnMitmManagerOptions, MaxSessions> {
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

            alignas(os::MemoryPageSize) u8 g_extra_thread_stacks[NumExtraThreads][ThreadStackSize];
            os::ThreadType g_extra_threads[NumExtraThreads];

            void LoopServerThread(void *)
            {
                g_server_manager.LoopProcess();
            }

            void ProcessForServerOnAllThreads(void *)
            {
                /* Initialize threads. */
                if constexpr (NumExtraThreads > 0)
                {
                    const s32 priority = os::GetThreadCurrentPriority(os::GetCurrentThread());
                    for (size_t i = 0; i < NumExtraThreads; i++)
                    {
                        R_ABORT_UNLESS(os::CreateThread(g_extra_threads + i, LoopServerThread, nullptr, g_extra_thread_stacks[i], ThreadStackSize, priority));
                        os::SetThreadNamePointer(g_extra_threads + i, "ldn_mitm::Thread");
                    }
                }

                /* Start extra threads. */
                if constexpr (NumExtraThreads > 0)
                {
                    for (size_t i = 0; i < NumExtraThreads; i++)
                    {
                        os::StartThread(g_extra_threads + i);
                    }
                }

                /* Loop this thread. */
                LoopServerThread(nullptr);

                /* Wait for extra threads to finish. */
                if constexpr (NumExtraThreads > 0)
                {
                    for (size_t i = 0; i < NumExtraThreads; i++)
                    {
                        os::WaitThread(g_extra_threads + i);
                    }
                }
            }
        }

    }

    namespace init {

        void InitializeSystemModule() {
            /* Initialize our connection to sm. */
            R_ABORT_UNLESS(sm::Initialize());

            /* Initialize fs. */
            fs::InitializeForSystem();
            fs::SetAllocator(mitm::Allocate, mitm::Deallocate);
            fs::SetEnabledAutoAbort(false);

            /* Mount the SD card. */
            R_ABORT_UNLESS(fs::MountSdCard("sdmc"));

            /* Initialize other services. */

            R_ABORT_UNLESS(ipinfoInit());
            R_ABORT_UNLESS(bsdInitialize(&LibnxBsdInitConfig, LibnxSocketInitConfig.num_bsd_sessions, LibnxSocketInitConfig.bsd_service_type));
            R_ABORT_UNLESS(socketInitialize(&LibnxSocketInitConfig));
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
        R_ABORT_UNLESS(log::Initialize());
        LogFormat("main");

        constexpr sm::ServiceName MitmServiceName = sm::ServiceName::Encode("ldn:u");
        //sf::hipc::ServerManager<2, LdnMitmManagerOptions, 3> server_manager;
        R_ABORT_UNLESS((mitm::g_server_manager.RegisterMitmServer<mitm::ldn::LdnMitMService>(0, MitmServiceName)));
        LogFormat("registered");

        R_ABORT_UNLESS(os::CreateThread(
            &mitm::g_thread,
            mitm::ProcessForServerOnAllThreads,
            nullptr,
            mitm::g_thread_stack,
            mitm::ThreadStackSize,
            mitm::ThreadPriority));

        os::SetThreadNamePointer(&mitm::g_thread, "ldn_mitm::MainThread");
        os::StartThread(&mitm::g_thread);

        os::WaitThread(&mitm::g_thread);
    }

}

void *operator new(size_t size)
{
    return ams::mitm::Allocate(size);
}

void *operator new(size_t size, const std::nothrow_t &)
{
    return ams::mitm::Allocate(size);
}

void operator delete(void *p)
{
    return ams::mitm::Deallocate(p, 0);
}

void operator delete(void *p, size_t size)
{
    return ams::mitm::Deallocate(p, size);
}

void *operator new[](size_t size)
{
    return ams::mitm::Allocate(size);
}

void *operator new[](size_t size, const std::nothrow_t &)
{
    return ams::mitm::Allocate(size);
}

void operator delete[](void *p)
{
    return ams::mitm::Deallocate(p, 0);
}

void operator delete[](void *p, size_t size)
{
    return ams::mitm::Deallocate(p, size);
}
