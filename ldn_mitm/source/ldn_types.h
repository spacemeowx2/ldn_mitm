#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SsidLengthMax 32
#define AdvertiseDataSizeMax 384
#define UserNameBytesMax 32
#define NodeCountMax 8
#define PassphraseLengthMax 64

enum NodeStateChange { 
    NodeStateChange_None = 0, 
    NodeStateChange_Connect = 1, 
    NodeStateChange_Disconnect = 2, 
    NodeStateChange_DisconnectAndConnect = 3 
};

typedef struct {
    uint8_t raw[6];
} MacAddress;

typedef struct {
    MacAddress bssid;
    uint8_t ssidLength;
    char ssid[SsidLengthMax + 1];
    int16_t channel;
    int8_t linkLevel;
    uint8_t networkType;
    uint32_t _unk;
} CommonNetworkInfo;

typedef struct {
    uint32_t ipv4Address;
    MacAddress macAddress;
    int8_t nodeId;
    int8_t isConnected;
    char userName[UserNameBytesMax+1];
    uint8_t _unk1;
    int16_t localCommunicationVersion;
    uint8_t _unk2[16];
} NodeInfo;

typedef struct {
    uint8_t unkRandom[16];
    uint16_t securityMode;
    uint8_t stationAcceptPolicy;
    uint8_t _unk1[3];
    uint8_t nodeCountMax;
    uint8_t nodeCount;
    NodeInfo nodes[NodeCountMax];
    uint16_t _unk2;
    uint16_t advertiseDataSize;
    uint8_t advertiseData[AdvertiseDataSizeMax];
    uint8_t _unk3[148];
} LdnNetworkInfo;

typedef struct {
    uint64_t localCommunicationId;
    uint8_t _unk1[2];
    uint16_t sceneId;
    uint8_t _unk2[4];
} IntentId;

typedef struct {
    uint64_t high;
    uint64_t low;
} SessionId;

typedef struct {
    IntentId intentId;      // 16bytes
    SessionId sessionId;    // 16bytes
} NetworkId;                // 32bytes

typedef struct {
    NetworkId networkId;
    CommonNetworkInfo common;
    LdnNetworkInfo ldn;
} NetworkInfo;

typedef struct {
    uint16_t securityMode;
    uint16_t passphraseSize;
    uint8_t passphrase[PassphraseLengthMax];
} SecurityConfig;

typedef struct {
    char userName[UserNameBytesMax + 1];
    uint8_t _unk[15];
} UserConfig;

typedef struct {
    IntentId intentId;      // 16byte
    uint16_t channel;
    uint8_t nodeCountMax;
    uint8_t _unk1;
    uint16_t localCommunicationVersion;
    uint8_t _unk2[10];
} NetworkConfig;            // 32bytes

typedef struct {
    SecurityConfig securityConfig;
    UserConfig userConfig;
    uint8_t _unk[4];
    NetworkConfig networkConfig;
} CreateNetworkConfig;

typedef struct {
    SecurityConfig securityConfig;
    UserConfig userConfig;
    uint32_t localCommunicationVersion;
    uint32_t option;
} ConnectNetworkData;

typedef struct {
    uint8_t stateChange;
    uint8_t _unk[7];
} NodeLatestUpdate;

typedef struct {
    uint8_t unkRandom[16];
    SessionId sessionId;
} SecurityParameter;

void NetworkInfo2NetworkConfig(NetworkInfo* info, NetworkConfig* out);
void NetworkInfo2SecurityParameter(NetworkInfo* info, SecurityParameter* out);

#ifdef __cplusplus
}
#endif
