#include "ldn_types.h"
#include <string.h>

void NetworkInfo2NetworkConfig(NetworkInfo* info, NetworkConfig* out) {
    out->intentId = info->networkId.intentId;
    out->channel = info->common.channel;
    out->nodeCountMax = info->ldn.nodeCountMax;
    out->localCommunicationVersion = info->ldn.nodes[0].localCommunicationVersion;
}

void NetworkInfo2SecurityParameter(NetworkInfo* info, SecurityParameter* out) {
    out->sessionId = info->networkId.sessionId;
    memcpy(out->unkRandom, info->ldn.unkRandom, 16);
}
