/*
 * ldn_services.hpp -- the ldn:u mitm service objects, ported from
 * ldn_mitm/source/ldn_icommunication.cpp + ldnmitm_config.cpp +
 * ldn_client_process_monitor.cpp onto ldn_nx::ServerObject.
 *
 * Wire format is unchanged: same command ids, same payloads.
 */
#pragma once

#include <switch.h>

#include "ipc_server.hpp"
#include "lan_discovery_interface.hpp"
#include "ldn_types.hpp"

namespace ams::mitm::ldn {

    /* IUserLocalCommunicationService -- 0x85280DC3 */
    class ICommunicationService final : public ldn_nx::ServerObject {
        public:
            explicit ICommunicationService(ILanDiscovery &lan);
            ~ICommunicationService() override;

            Result Invoke(ldn_nx::Request &req, ldn_nx::Reply &reply) override;
            void Release() override { }

        private:
            Result InitializeService();
            Result FinalizeService();
            void OnEventFired();

            ILanDiscovery &m_lan;
            Event stateEvent;
            bool stateEventCreated = false;
            u64 errorState = 0;
    };

    /* IClientProcessMonitor -- register clients so they can be tracked */
    class IClientProcessMonitor final : public ldn_nx::ServerObject {
        public:
            Result Invoke(ldn_nx::Request &req, ldn_nx::Reply &reply) override;
    };

    /* ILdnConfig -- 0x14c8af2c, ldn_mitm's own config interface */
    struct LdnMitmVersion {
        char raw[32];
    };

    class ILdnConfigService final : public ldn_nx::ServerObject {
        public:
            Result Invoke(ldn_nx::Request &req, ldn_nx::Reply &reply) override;

            static bool IsEnabled();
            static void SetEnabled(bool enabled);

        private:
            static bool s_enabled;
    };

    /* ILdnMitMService -- 0x1D8F875E, the service sm hands client sessions to */
    class ILdnMitMService final : public ldn_nx::ServerObject {
        public:
            ILdnMitMService(Handle forward_session, ILanDiscovery &lan);

            Result Invoke(ldn_nx::Request &req, ldn_nx::Reply &reply) override;

            Handle forward_session() const { return m_forwardSession; }

        private:
            Handle m_forwardSession;
            ILanDiscovery &m_lan;
    };

} // namespace ams::mitm::ldn
