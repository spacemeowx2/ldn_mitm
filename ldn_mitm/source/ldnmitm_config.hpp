#pragma once

#include <switch.h>
#include <stratosphere.hpp>
#include "interfaces/iconfig.hpp"

namespace ams::mitm::ldn {

    class LdnConfig final {
        public:
            static bool getEnabled() {
                return LdnEnabled;
            }
        protected:
            static std::atomic_bool LdnEnabled;
        public:

            Result SaveLogToFile();
            Result GetVersion(sf::Out<LdnMitmVersion> version);
            Result GetLogging(sf::Out<u32> enabled);
            Result SetLogging(u32 enabled);
            Result GetEnabled(sf::Out<u32> enabled);
            Result SetEnabled(u32 enabled);
    };
    static_assert(ams::mitm::ldn::IsILdnConfig<LdnConfig>);
    
}