/*
 * Copyright (c) 2018 Atmosphère-NX
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
 
#include <switch.h>
#include "ldnmitm_service.hpp"

#include "mitm_query_service.hpp"
#include "debug.hpp"

Result LdnMitMService::dispatch(IpcParsedCommand &r, IpcCommand &out_c, u64 cmd_id, u8 *pointer_buffer, size_t pointer_buffer_size) {
    Result rc = 0xF601;
    if (this->has_initialized) {
        switch (static_cast<FspSrvCmd>(cmd_id)) {
            case FspSrvCmd::OpenDataStorageByCurrentProcess:
                break;
            case FspSrvCmd::OpenDataStorageByDataId:
                break;
            default:
                break;
        }
    } else {
        if (static_cast<FspSrvCmd>(cmd_id) == FspSrvCmd::SetCurrentProcess) {
            if (r.HasPid) {
                this->init_pid = r.Pid;
            }
        }
    }
    return rc;
}

void LdnMitMService::postprocess(IpcParsedCommand &r, IpcCommand &out_c, u64 cmd_id, u8 *pointer_buffer, size_t pointer_buffer_size) {
    struct {
        u64 magic;
        u64 result;
    } *resp = (decltype(resp))r.Raw;
    
    u64 *tls = (u64 *)armGetTls();
    std::array<u64, 0x100/sizeof(u64)> backup_tls;
    std::copy(tls, tls + backup_tls.size(), backup_tls.begin());
    
    Result rc = (Result)resp->result;
    switch (static_cast<FspSrvCmd>(cmd_id)) {
        case FspSrvCmd::SetCurrentProcess:
            if (R_SUCCEEDED(rc)) {
                this->has_initialized = true;
            }
            this->process_id = this->init_pid;
            this->title_id = this->process_id;
            if (R_FAILED(MitMQueryUtils::get_associated_tid_for_pid(this->process_id, &this->title_id))) {
                /* Log here, if desired. */
            }
            std::copy(backup_tls.begin(), backup_tls.end(), tls);
            break;
        default:
            break;
    }
    resp->result = rc;
}

Result LdnMitMService::handle_deferred() {
    /* This service is never deferrable. */
    return 0;
}
