#pragma once
#include <switch.h>
#include <stratosphere.hpp>
#include <cstring>
#include <atomic>
#include "ldn_icommunication.hpp"
#include "ldnmitm_config.hpp"
#include "debug.hpp"

class LdnMitMService : public IMitmServiceObject {
    private:
        enum class CommandId {
            CreateUserLocalCommunicationService = 0,
            CreateLdnMitmConfigService = 65000,
        };
    public:
        LdnMitMService(std::shared_ptr<Service> s, u64 pid) : IMitmServiceObject(s, pid) {
            /* ... */
        }
        
        static bool ShouldMitm(u64 pid, u64 tid) {
            LogFormat("should_mitm pid: %" PRIu64 " tid: %" PRIx64, pid, tid);
            return true;
        }
        
        static void PostProcess(IMitmServiceObject *obj, IpcResponseContext *ctx) {};
    protected:
        /* Overridden commands. */
        Result CreateUserLocalCommunicationService(Out<std::shared_ptr<ICommunicationInterface>> out);
        Result CreateLdnMitmConfigService(Out<std::shared_ptr<LdnConfig>> out);
    public:
        DEFINE_SERVICE_DISPATCH_TABLE {
            MAKE_SERVICE_COMMAND_META(LdnMitMService, CreateUserLocalCommunicationService),
            MAKE_SERVICE_COMMAND_META(LdnMitMService, CreateLdnMitmConfigService),
        };
};
