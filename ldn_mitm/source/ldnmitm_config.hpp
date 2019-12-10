#pragma once

#include <switch.h>
#include <stratosphere.hpp>

namespace ams::mitm::ldn {
    struct LdnMitmVersion {
        char raw[32];
    };

    class LdnConfig : public sf::IServiceObject {
        private:
            enum class CommandId {
                SaveLogToFile = 65000,
                GetVersion = 65001,
                GetLogging = 65002,
                SetLogging = 65003,
                GetEnabled = 65004,
                SetEnabled = 65005,
            };
        public:
            static bool getEnabled() {
                return LdnEnabled;
            }
        protected:
            static std::atomic_bool LdnEnabled;
            Result SaveLogToFile();
            Result GetVersion(sf::Out<LdnMitmVersion> version);
            Result GetLogging(sf::Out<u32> enabled);
            Result SetLogging(u32 enabled);
            Result GetEnabled(sf::Out<u32> enabled);
            Result SetEnabled(u32 enabled);
        public:
            DEFINE_SERVICE_DISPATCH_TABLE {
                MAKE_SERVICE_COMMAND_META(SaveLogToFile),
                MAKE_SERVICE_COMMAND_META(GetVersion),
                MAKE_SERVICE_COMMAND_META(GetLogging),
                MAKE_SERVICE_COMMAND_META(SetLogging),
                MAKE_SERVICE_COMMAND_META(GetEnabled),
                MAKE_SERVICE_COMMAND_META(SetEnabled),
            };
    };
}