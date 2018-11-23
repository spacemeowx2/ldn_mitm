#pragma once
#include <stratosphere.hpp>
#include "debug.hpp"
#include <memory>
#include <thread>
#include <array>
#include <mutex>
#include <unordered_map>
#include <poll.h>
#include <stdint.h>
#include <unistd.h>
#include <cstring>
#include "ldn_types.h"

// Only used when debuging
#define DISABLE_COMPRESS 1

class LANDiscovery {
    public:
        static const int BufferSize = 2048;
        static const int DefaultPort = 11452;
        static const int DiscoveryFds = 2;
        static const int NodeMaxCount= 8;
        static const int FdUdp = 0;
        static const int FdTcp = 1;
        static const u32 LANMagic = 0x11451400;
        static const char *FakeSsid;
        struct PayloadScanResponse {
            u16 size;
            u8 data[sizeof(NetworkInfo)];
        };
        enum class LANPacketType : u8 {
            scan,
            scan_resp,
            connect,
            sync_network,
        };
        enum class NodeStatus : u8 {
            Disconnected,
            Connect,
            Connected,
        };
        struct LANPacketHeader {
            u32 magic;
            LANPacketType type;
            u8 compressed;
            u16 length;
            u16 decompress_length;
            u8 _reserved[2];
        };
        struct LANNode {
            NodeInfo *nodeInfo;
            NodeStatus status;
            u8 buffer[BufferSize];
            u16 recvSize;
            struct pollfd *pfd;
        };
        struct MacHash {
            std::size_t operator() (const MacAddress &t) const {
                return *reinterpret_cast<const u32*>(t.raw + 2);
            }
        };
        struct MacEquals {
            bool operator() (const MacAddress& lhs, const MacAddress& rhs) const {
                return std::memcmp(&lhs, &rhs, sizeof(MacAddress)) == 0;
            }
        };
        typedef std::function<int(LANPacketType, const void *, size_t)> ReplyFunc;
        typedef std::function<void()> NodeEventFunc;
        static const NodeEventFunc EmptyFunc;
    protected:
        // 0: udp 1: tcp
        std::array<struct pollfd, DiscoveryFds + NodeMaxCount> fds;
        std::array<struct LANNode, NodeMaxCount> nodes;
        HosMutex fdsMutex;
        std::unordered_map<MacAddress, NetworkInfo, MacHash, MacEquals> scanResults;
        int nodeCount();
        static void Worker(void* args);
        bool stop;
        bool inited;
        NetworkInfo networkInfo;
        u16 listenPort;
        Thread worker_thread;
        void worker();
        int loopPoll();
        void onMessage(int index, LANPacketType type, const void *data, size_t size, ReplyFunc reply);
        void onPacket(int index);
        void onNodeConnect();
        void onNodeChanged(int fromIndex);
        void prepareHeader(LANPacketHeader &header, LANPacketType type);
        void updateNodes();
        int sendBroadcast(LANPacketType type, const void *data, size_t size);
        int sendBroadcast(LANPacketType type);
        int sendTo(LANPacketType type, const void *data, size_t size, struct sockaddr_in &addr);
        int sendTcp(LANPacketType type, const void *data, size_t size);
        int sendTcp(LANPacketType type, const void *data, size_t size, int index);
        int sendPacket(LANPacketType type, const void *data, size_t size, std::function<int(const void *, size_t)> bindSend);
        int nodeRecv(int nodeId, u8 *buffer, size_t bufLen);
        void nodeClose(int nodeId);
        Result getFakeMac(MacAddress *mac);
        Result getNodeInfo(NodeInfo *node, const UserConfig *userConfig, u16 localCommunicationVersion);
        u32 getBroadcast();
        NodeEventFunc nodeEvent;
    public: 
        bool isHost;
        LANDiscovery(u16 port = DefaultPort) : stop(false), inited(false), networkInfo({0}), listenPort(port), isHost(false) {
            LogStr("LANDiscovery\n");
        };
        Result initialize(NodeEventFunc nodeEvent = EmptyFunc, bool listening = true);
        ~LANDiscovery();
        Result initNetworkInfo();
        Result scan(NetworkInfo *networkInfo, u16 *count);
        Result setAdvertiseData(const u8 *data, uint16_t size);
        Result createNetwork(const SecurityConfig *securityConfig, const UserConfig *userConfig, const NetworkConfig *networkConfig);
        Result destroyNetwork();
        Result connect(NetworkInfo *networkInfo, UserConfig *userConfig, u16 localCommunicationVersion);
        Result disconnect();
        Result getNetworkInfo(NetworkInfo *info);
    protected:
        Result setSocketOpts(int fd);
        Result initSocket(bool listening);
        Result finalize();
        int compress(const void *input, size_t input_size, uint8_t *output, size_t *output_size);
        int decompress(const void *input, size_t input_size, uint8_t *output, size_t *output_size);
};
