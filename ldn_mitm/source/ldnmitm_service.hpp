#pragma once
#include <switch.h>
#include <stratosphere.hpp>
#include <cstring>
#include <atomic>
#include "ldn_icommunication.hpp"
#include "debug.hpp"

enum LdnSrvCmd {
    LdnSrvCmd_CreateUserLocalCommunicationService = 0,
};
enum LdnMitmDebugCmd {
    LdnMitmDebugCmd_SaveLogToFile = 65000,
    LdnMitmDebugCmd_GetVersion = 65001,
    LdnMitmDebugCmd_GetLogging = 65002,
    LdnMitmDebugCmd_SetLogging = 65003,
    LdnMitmDebugCmd_GetEnabled = 65004,
    LdnMitmDebugCmd_SetEnabled = 65005,
};
struct LdnMitmVersion {
    char raw[32];
};

class LdnMitMService : public IMitmServiceObject {      
    public:
        static std::atomic<u32> Enabled;

        LdnMitMService(std::shared_ptr<Service> s, u64 pid) : IMitmServiceObject(s, pid) {
            /* ... */
        }
        
        static bool ShouldMitm(u64 pid, u64 tid) {
            LogFormat("should_mitm pid: %" PRIu64 " tid: %" PRIx64 " enabled: %d", pid, tid, static_cast<u32>(Enabled));
            return Enabled;
        }
        
        static void PostProcess(IMitmServiceObject *obj, IpcResponseContext *ctx) {};
    protected:
        /* Overridden commands. */
        Result CreateUserLocalCommunicationService(Out<std::shared_ptr<ICommunicationInterface>> out);
    public:
        DEFINE_SERVICE_DISPATCH_TABLE {
            MakeServiceCommandMeta<LdnSrvCmd_CreateUserLocalCommunicationService, &LdnMitMService::CreateUserLocalCommunicationService>(),
        };
};

class LdnMitmDebugService final : public IServiceObject {
    protected:
        Result SaveLogToFile() {
            return ::SaveLogToFile();
        }
        Result GetVersion(Out<LdnMitmVersion> version) {
            std::strcpy(version.GetPointer()->raw, GITDESCVER);
            return 0;
        }
        Result GetLogging(Out<u32> enabled) {
            return ::GetLogging(enabled.GetPointer());
        }
        Result SetLogging(u32 enabled) {
            return ::SetLogging(enabled);
        }
        Result GetEnabled(Out<u32> enabled) {
            enabled.SetValue(LdnMitMService::Enabled);
            return 0;
        }
        Result SetEnabled(u32 enabled) {
            LdnMitMService::Enabled = enabled;
            return 0;
        }
    public:
        DEFINE_SERVICE_DISPATCH_TABLE {
            MakeServiceCommandMeta<LdnMitmDebugCmd_SaveLogToFile, &LdnMitmDebugService::SaveLogToFile>(),
            MakeServiceCommandMeta<LdnMitmDebugCmd_GetVersion, &LdnMitmDebugService::GetVersion>(),
            MakeServiceCommandMeta<LdnMitmDebugCmd_GetLogging, &LdnMitmDebugService::GetLogging>(),
            MakeServiceCommandMeta<LdnMitmDebugCmd_SetLogging, &LdnMitmDebugService::SetLogging>(),
            MakeServiceCommandMeta<LdnMitmDebugCmd_GetEnabled, &LdnMitmDebugService::GetEnabled>(),
            MakeServiceCommandMeta<LdnMitmDebugCmd_SetEnabled, &LdnMitmDebugService::SetEnabled>(),
        };
};
