#include "ipinfo.hpp"
#include "debug.hpp"
#include <arpa/inet.h>

static Service g_nifmSrv;
static Service g_nifmIGS;
static Service g_nifmIReq;
static u64 g_nifmRefCount = 0;
static const int ModuleID = 0xFE;

Result _nifmGetIGS();
Result _nifmGetIReq();
Result _nifmGetRequestState(s32 *state);
Result _nifmGetResult();
Result _nifmSetConnectionConfirmationOption(s8 option);
Result _nifmSubmitRequest();

Result ipinfoInit() {
    atomicIncrement64(&g_nifmRefCount);
    if (serviceIsActive(&g_nifmSrv))
        return 0;

    Result rc = smGetService(&g_nifmSrv, "nifm:u");
    if (R_FAILED(rc)) {
        return MAKERESULT(ModuleID, 5);
    }

    rc = _nifmGetIGS();
    if (R_FAILED(rc)) {
        return rc;
    }

    rc = _nifmGetIReq();
    if (R_FAILED(rc)) {
        return rc;
    }

    return rc;
}

void ipinfoExit() {
    if (atomicDecrement64(&g_nifmRefCount) == 0) {
        serviceClose(&g_nifmIReq);
        serviceClose(&g_nifmIGS);
        serviceClose(&g_nifmSrv);
    }
}

Result ipinfoGetIpConfig(u32* address) {
    u32 netmask;
    return ipinfoGetIpConfig(address, &netmask);
}

Result ipinfoGetIpConfig(u32* address, u32* netmask) {
    struct {
        u8 _unk;
        u32 address;
        u32 netmask;
        u32 gateway;
    } __attribute__((packed)) resp;
    R_TRY(serviceDispatchOut(&g_nifmIGS, 15, resp));
    *address = ntohl(resp.address);
    *netmask = ntohl(resp.netmask);

    return 0;
}

Result nifmSetLocalNetworkMode(bool isLocalNetworkMode) {
    s8 option = isLocalNetworkMode ? 2 : 4;
    return _nifmSetConnectionConfirmationOption(option);
}

Result nifmSubmitRequestAndWait() {
    Result rc = 0;
    s32 state;

    rc = _nifmSubmitRequest();
    if (R_SUCCEEDED(rc)) {
        while (R_SUCCEEDED(_nifmGetRequestState(&state))) {
            // pending
            if (state != 2) {
                break;
            }
            svcSleepThread(100000000L); // 100ms
        }
    }

    rc = _nifmGetRequestState(&state);
    if (R_SUCCEEDED(rc)) {
        if (state == 3) {
            rc = 0;
        } else {
            rc = _nifmGetResult();
        }
    }

    return rc;
}

Result nifmCancelRequest() {
    return serviceDispatch(&g_nifmIReq, 3);
}

Result _nifmGetIGS() {
    u64 param = 0;
    return serviceDispatchIn(&g_nifmSrv, 5, param,
        .out_num_objects = 1,
        .out_objects = &g_nifmIGS
    );
}

Result _nifmGetIReq() {
    s32 param = 2;
    return serviceDispatchIn(&g_nifmIGS, 4, param,
        .out_num_objects = 1,
        .out_objects = &g_nifmIReq
    );
}

Result _nifmGetRequestState(s32 *state) {
    R_TRY(serviceDispatchOut(&g_nifmIReq, 0, *state));
    LogFormat("_nifmGetRequestState %d", *state);
    if (*state == 1) {
        LogFormat("result %d", _nifmGetResult());
    }

    return 0;
}

Result _nifmSetConnectionConfirmationOption(s8 option) {
    s32 option_32 = option;
    return serviceDispatchIn(&g_nifmIReq, 11, option_32);
}

Result _nifmSubmitRequest() {
    return serviceDispatch(&g_nifmIReq, 4);
}

Result _nifmGetResult() {
    return serviceDispatch(&g_nifmIReq, 1);
}
