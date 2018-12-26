#pragma once

#include <switch.h>
#include <stratosphere.hpp>

enum LdnConfigCmd {
    LdnConfigCmd_SaveLogToFile = 65000,
    LdnConfigCmd_GetVersion = 65001,
    LdnConfigCmd_GetLogging = 65002,
    LdnConfigCmd_SetLogging = 65003,
    LdnConfigCmd_GetEnabled = 65004,
    LdnConfigCmd_SetEnabled = 65005,
};
struct LdnMitmVersion {
    char raw[32];
};

class LdnConfig : public IServiceObject {
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
            MakeServiceCommandMeta<LdnConfigCmd_SaveLogToFile, &LdnConfig::SaveLogToFile>(),
            MakeServiceCommandMeta<LdnConfigCmd_GetVersion, &LdnConfig::GetVersion>(),
            MakeServiceCommandMeta<LdnConfigCmd_GetLogging, &LdnConfig::GetLogging>(),
            MakeServiceCommandMeta<LdnConfigCmd_SetLogging, &LdnConfig::SetLogging>(),
            MakeServiceCommandMeta<LdnConfigCmd_GetEnabled, &LdnConfig::GetEnabled>(),
            MakeServiceCommandMeta<LdnConfigCmd_SetEnabled, &LdnConfig::SetEnabled>(),
        };
};
