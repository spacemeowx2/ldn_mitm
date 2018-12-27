#include "ipinfo.hpp"
#include <arpa/inet.h>

static Service g_nifmSrv;
static Service g_nifmIGS;
static Service g_nifmIReq;
static u64 g_nifmRefCount = 0;
static const int ModuleID = 0xFE;

Result _nifmGetIGS();
Result _nifmGetIReq();

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
    Result rc;
    IpcCommand c;
    IpcParsedCommand r;

    ipcInitialize(&c);
    struct {
        u64 magic;
        u64 cmd_id;
    } *raw;

    raw = (decltype(raw))serviceIpcPrepareHeader(&g_nifmIGS, &c, sizeof(*raw));

    raw->magic = SFCI_MAGIC;
    raw->cmd_id = 15; // GetCurrentIpConfigInfo

    rc = serviceIpcDispatch(&g_nifmIGS);
    if (R_FAILED(rc)) {
        return rc;
    }
    struct {
        u64 magic;
        u64 result;
        u8 _unk;
        u32 address;
        u32 netmask;
        u32 gateway;
    } __attribute__((packed)) *resp;

    serviceIpcParse(&g_nifmIGS, &r, sizeof(*resp));
    resp = (decltype(resp))r.Raw;

    rc = resp->result;
    if (R_FAILED(rc)) {
        return rc;
    }
    *address = ntohl(resp->address);
    *netmask = ntohl(resp->netmask);
    // ret = resp->address | ~resp->netmask;

    return rc;
}

Result nifmSubmitRequestAndWait() {
    return 0;
}
Result nifmCancelRequest() {
    return 0;
}
Result nifmSetLocalNetworkMode(bool isLocalNetworkMode) {
    return 0;
}

Result _nifmGetIGS() {
    IpcCommand c;
    IpcParsedCommand r;

    ipcInitialize(&c);
    ipcSendPid(&c);
    struct {
        u64 magic;
        u64 cmd_id;
        u64 param;
    } *raw;

    raw = (decltype(raw))serviceIpcPrepareHeader(&g_nifmSrv, &c, sizeof(*raw));

    raw->magic = SFCI_MAGIC;
    raw->cmd_id = 5;
    raw->param = 0;

    Result rc = serviceIpcDispatch(&g_nifmSrv);

    if (R_FAILED(rc)) {
        return MAKERESULT(ModuleID, 6);
    }
    struct {
        u64 magic;
        u64 result;
    } *resp;

    serviceIpcParse(&g_nifmSrv, &r, sizeof(*resp));
    resp = (decltype(resp))r.Raw;

    rc = resp->result;

    if (R_FAILED(rc)) {
        return MAKERESULT(ModuleID, 7);
    }

    serviceCreateSubservice(&g_nifmIGS, &g_nifmSrv, &r, 0);

    return 0;
}

Result _nifmGetIReq() {
    IpcCommand c;
    IpcParsedCommand r;

    ipcInitialize(&c);
    ipcSendPid(&c);
    struct {
        u64 magic;
        u64 cmd_id;
        s32 param;
    } *raw;

    raw = (decltype(raw))serviceIpcPrepareHeader(&g_nifmIGS, &c, sizeof(*raw));

    raw->magic = SFCI_MAGIC;
    raw->cmd_id = 4;
    raw->param = 0;

    Result rc = serviceIpcDispatch(&g_nifmIGS);

    if (R_FAILED(rc)) {
        return MAKERESULT(ModuleID, 8);
    }
    struct {
        u64 magic;
        u64 result;
    } *resp;

    serviceIpcParse(&g_nifmIGS, &r, sizeof(*resp));
    resp = (decltype(resp))r.Raw;

    rc = resp->result;

    if (R_FAILED(rc)) {
        return MAKERESULT(ModuleID, 9);
    }

    serviceCreateSubservice(&g_nifmIReq, &g_nifmIGS, &r, 0);

    return 0;
}