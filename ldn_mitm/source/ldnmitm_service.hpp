#pragma once
#include <switch.h>
#include <stratosphere.hpp>
#include <cstring>
#include <atomic>
#include "ldn_icommunication.hpp"
#include "ldnmitm_config.hpp"
#include "debug.hpp"

namespace ams::mitm::ldn {
    class LdnMitMService : public sf::IMitmServiceObject {
        private:
            enum class CommandId {
                CreateUserLocalCommunicationService = 0,
                CreateLdnMitmConfigService = 65000,
            };
        public:
            SF_MITM_SERVICE_OBJECT_CTOR(LdnMitMService) { /* ... */ }
            
            static bool ShouldMitm(const sm::MitmProcessInfo &client_info) {
                LogFormat("should_mitm pid: %" PRIu64 " tid: %" PRIx64, client_info.process_id, client_info.program_id);
                return true;
            }
        protected:
            /* Overridden commands. */
            Result CreateUserLocalCommunicationService(sf::Out<std::shared_ptr<ICommunicationInterface>> out);
            Result CreateLdnMitmConfigService(sf::Out<std::shared_ptr<LdnConfig>> out);
        public:
            DEFINE_SERVICE_DISPATCH_TABLE {
                MAKE_SERVICE_COMMAND_META(CreateUserLocalCommunicationService),
                MAKE_SERVICE_COMMAND_META(CreateLdnMitmConfigService),
            };
    };
}
