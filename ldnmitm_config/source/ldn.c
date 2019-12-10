#include "ldn.h"
#include <string.h>

void NetworkInfo2NetworkConfig(NetworkInfo* info, NetworkConfig* out) {
    out->intentId = info->networkId.intentId;
    out->channel = info->common.channel;
    out->nodeCountMax = info->ldn.nodeCountMax;
    out->localCommunicationVersion = 1;
}

void NetworkInfo2SecurityParameter(NetworkInfo* info, SecurityParameter* out) {
    out->sessionId = info->networkId.sessionId;
    memcpy(out->unkRandom, info->ldn.unkRandom, 16);
}

Result ldnMitmGetConfig(LdnMitmConfigService *out) {
    Handle handle;
    Result rc = svcConnectToNamedPort(&handle, "ldnmitm");
    if (R_SUCCEEDED(rc)) {
        serviceCreate(&out->s, handle);
    }
    return rc;
}

Result ldnMitmGetConfigFromService(Service* s, LdnMitmConfigService *out) {
    return serviceDispatch(s, 65000,
        .out_num_objects = 1,
        .out_objects = &out->s
    );
}

Result ldnMitmGetLogging(LdnMitmConfigService *s, u32 *enabled) {
    return serviceDispatchOut(&s->s, 65002, *enabled);
}

Result ldnMitmSetLogging(LdnMitmConfigService *s, u32 enabled) {
    return serviceDispatchIn(&s->s, 65003, enabled);
}

Result ldnMitmGetEnabled(LdnMitmConfigService *s, u32 *enabled) {
    return serviceDispatchOut(&s->s, 65004, *enabled);
}

Result ldnMitmSetEnabled(LdnMitmConfigService *s, u32 enabled) {
    return serviceDispatchIn(&s->s, 65005, enabled);
}

Result ldnMitmGetVersion(LdnMitmConfigService *s, char *version) {
    char version_s[32];
    Result rc = serviceDispatchOut(&s->s, 65001, version_s);
    if (R_SUCCEEDED(rc)) {
        strcpy(version, version_s);
    }
    return rc;
}

Result ldnMitmSaveLogToFile(LdnMitmConfigService *s) {
    return serviceDispatch(&s->s, 65000);
}