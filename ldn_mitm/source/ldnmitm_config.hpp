#pragma once

#include <switch.h>
#include <stratosphere.hpp>

namespace ams::mitm::ldn {
    struct LdnMitmVersion {
        char raw[32];
    };

    class LdnConfig : public sf::IServiceObject {
        private:
            /* enum class CommandId {
                SaveLogToFile = 65000,
                GetVersion = 65001,
                GetLogging = 65002,
                SetLogging = 65003,
                GetEnabled = 65004,
                SetEnabled = 65005,
            }; */
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
        public:
            /* DEFINE_SERVICE_DISPATCH_TABLE {
                MAKE_SERVICE_COMMAND_META(SaveLogToFile),
                MAKE_SERVICE_COMMAND_META(GetVersion),
                MAKE_SERVICE_COMMAND_META(GetLogging),
                MAKE_SERVICE_COMMAND_META(SetLogging),
                MAKE_SERVICE_COMMAND_META(GetEnabled),
                MAKE_SERVICE_COMMAND_META(SetEnabled),
            }; */
    };
    static_assert(sf::IsServiceObject<LdnConfig>);

    /* static std::atomic_bool LdnEnabled;
    #define AMS_LDN_CONFIG(C, H)                                                                \
        AMS_SF_METHOD_INFO(C, H, 65000, Result, SaveLogToFile, ())                              \
        AMS_SF_METHOD_INFO(C, H, 65001, Result, GetVersion, (sf::Out<LdnMitmVersion> version))  \
        AMS_SF_METHOD_INFO(C, H, 65002, Result, GetLogging, (sf::Out<u32> enabled))             \
        AMS_SF_METHOD_INFO(C, H, 65003, Result, SetLogging, (u32 enabled))                      \
        AMS_SF_METHOD_INFO(C, H, 65004, Result, GetEnabled, (sf::Out<u32> enabled))             \
        AMS_SF_METHOD_INFO(C, H, 65005, Result, SetEnabled, (u32 enabled))

    AMS_SF_DECLARE_INTERFACE_METHODS(AMS_LDN_CONFIG)



    AMS_SF_DEFINE_INTERFACE(LdnConfig, AMS_LDN_CONFIG) */
}