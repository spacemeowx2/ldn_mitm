/*
 * mitm_query_service.hpp -- answers sm's "should this mitm handle this client?"
 * query.
 *
 * InstallMitm hands the mitm a query session in addition to the accept port;
 * libstratosphere serves it from inside its ServerManager and calls the
 * interface's ShouldMitm() (source/sf/hipc/sf_hipc_mitm_query_api.cpp, command
 * 65000). Without libstratosphere that server is ours to provide.
 */
#pragma once

#include <switch.h>

#include "ipc_server.hpp"

namespace ldn_nx {

constexpr u32 MitmQueryCommandShouldMitm = 65000;

class MitmQueryService : public ServerObject {
public:
    virtual bool ShouldMitm(const MitmClientInfo &client_info) = 0;

    Result Invoke(Request &req, Reply &reply) override {
        if (req.cmd_id != MitmQueryCommandShouldMitm) {
            return 0x202; /* InvalidCommandId-ish */
        }
        if (!req.InSizeOk(sizeof(MitmClientInfo))) {
            return 0x201;
        }

        auto *out = reply.Out<bool>();
        if (out == nullptr) {
            return 0x204;
        }
        *out = this->ShouldMitm(*reinterpret_cast<const MitmClientInfo *>(req.in_data));
        return 0;
    }
};

} // namespace ldn_nx
