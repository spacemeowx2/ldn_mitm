#pragma once
#include <switch.h>
#include <stratosphere.hpp>
#include "ldn_icommunication.hpp"
#include "debug.hpp"

enum LdnSrvCmd {
    LdnSrvCmd_CreateUserLocalCommunicationService = 0,
};

class LdnMitMService : public IMitmServiceObject {      
    private:
        // std::shared_ptr<ICommunicationInterface> comm;
    public:
        LdnMitMService(std::shared_ptr<Service> s) : IMitmServiceObject(s) {
            // comm = std::make_shared<ICommunicationInterface>();
            LogStr("LdnMitMService\n");
            /* ... */
        }
        
        static bool ShouldMitm(u64 pid, u64 tid) {
            char buf[128];
            sprintf(buf, "should_mitm pid: %" PRIu64 " tid: %" PRIu64 "\n", pid, tid);
            LogStr(buf);
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
