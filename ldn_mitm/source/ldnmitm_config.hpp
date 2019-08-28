#pragma once

#include <switch.h>
#include <stratosphere.hpp>

struct LdnMitmVersion {
    char raw[32];
};

class LdnConfig : public IServiceObject {
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
        Result GetVersion(Out<LdnMitmVersion> version);
        Result GetLogging(Out<u32> enabled);
        Result SetLogging(u32 enabled);
        Result GetEnabled(Out<u32> enabled);
        Result SetEnabled(u32 enabled);
    public:
        DEFINE_SERVICE_DISPATCH_TABLE {
            MAKE_SERVICE_COMMAND_META(LdnConfig, SaveLogToFile),
            MAKE_SERVICE_COMMAND_META(LdnConfig, GetVersion),
            MAKE_SERVICE_COMMAND_META(LdnConfig, GetLogging),
            MAKE_SERVICE_COMMAND_META(LdnConfig, SetLogging),
            MAKE_SERVICE_COMMAND_META(LdnConfig, GetEnabled),
            MAKE_SERVICE_COMMAND_META(LdnConfig, SetEnabled),
        };
};
