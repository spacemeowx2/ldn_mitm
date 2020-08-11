#pragma once
#include <switch.h>
#include <stratosphere.hpp>
#include <cstring>
#include <atomic>
#include "ldn_icommunication.hpp"
#include "ldnmitm_config.hpp"
#include "debug.hpp"
#include "prov/iservice.hpp"

namespace ams::mitm::ldn {
    // class LdnMitMService : public sf::IMitmServiceObject {
    class LdnMitMService final{
        protected:
            std::shared_ptr<::Service> forward_service;
            sm::MitmProcessInfo client_info;
        public:
            LdnMitMService(std::shared_ptr<::Service> &&s, const sm::MitmProcessInfo &c) : forward_service(std::move(s)), client_info(c) { /* ... */ }
            
            static bool ShouldMitm(const sm::MitmProcessInfo &client_info) {
                LogFormat("should_mitm pid: %" PRIu64 " tid: %" PRIx64, client_info.process_id, client_info.program_id);
                return true;
            }
        // protected:
        public:
            /* Overridden commands. */
            Result CreateUserLocalCommunicationService(sf::Out<std::shared_ptr<ILdnCommunication>> out);
            Result CreateLdnMitmConfigService(sf::Out<std::shared_ptr<ILdnConfig>> out);
    };
    static_assert(ams::mitm::ldn::IsILdnMitMService<LdnMitMService>);
}
