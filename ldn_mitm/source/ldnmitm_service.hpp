#pragma once
#include <switch.h>
#include <stratosphere.hpp>
#include "ldn_icommunication.hpp"
#include "debug.hpp"

enum LdnSrvCmd {
    LdnSrvCmd_CreateUserLocalCommunicationService = 0,
};

class LdnMitMService : public IMitmServiceObject {      
    public:
        LdnMitMService(std::shared_ptr<Service> s, u64 pid) : IMitmServiceObject(s, pid) {
            /* ... */
        }
        
        static bool ShouldMitm(u64 pid, u64 tid) {
            LogFormat("should_mitm pid: %" PRIu64 " tid: %" PRIu64, pid, tid);
            return true;
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
