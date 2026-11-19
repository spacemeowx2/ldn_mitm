#pragma once
#include <switch.h>
#include "debug.hpp"
#include <memory>
#include <array>
#include <unordered_map>
#include <stdint.h>
#include <unistd.h>
#include <cstring>
#include "ldn_types.hpp"
#include "lan_protocol.hpp"
#include "lan_discovery_interface.hpp"

namespace ams::mitm::ldn {
    const size_t StackSize = 0x4000;
    const size_t ThreadStackAlignment = 0x1000;

    /* libstratosphere's os::Mutex has lock()/unlock() so it works with
     * std::scoped_lock; libnx's Mutex is a bare _LOCK_T. This keeps the call
     * sites unchanged. */
    class LdnMutex {
        public:
            explicit LdnMutex(bool recursive = false) {
                (void)recursive; /* libnx mutexes are not recursive */
                mutexInit(&this->mutex);
            }
            void lock() { mutexLock(&this->mutex); }
            void unlock() { mutexUnlock(&this->mutex); }
        private:
            Mutex mutex;
    };

    /* Scope guard for LdnMutex. We do not use std::lock_guard / std::mutex:
     * they drag in winpthreads on the host and are not what libnx offers on
     * the target. */
    template<typename M>
    class LdnLock {
        public:
            explicit LdnLock(M &mutex) : m_mutex(mutex) { this->m_mutex.lock(); }
            ~LdnLock() { this->m_mutex.unlock(); }
            LdnLock(const LdnLock &) = delete;
            LdnLock &operator=(const LdnLock &) = delete;
        private:
            M &m_mutex;
    };

    enum class NodeStatus : u8 {
        Disconnected,
        Connect,
        Connected,
    };
    class LANDiscovery;

    class LanStation : public Pollable {
        protected:
            friend class LANDiscovery;
            NodeInfo *nodeInfo;
            NodeStatus status;
            std::unique_ptr<TcpLanSocketBase> socket;
            int nodeId;
            LANDiscovery *discovery;
        public:
            LanStation(int nodeId, LANDiscovery *discovery)
                : nodeInfo(nullptr),
                status(NodeStatus::Disconnected),
                nodeId(nodeId),
                discovery(discovery)
            {}
            NodeStatus getStatus() const {
                return this->status;
            }
            void reset() {
                this->socket.reset();
                this->status = NodeStatus::Disconnected;
            };
            void link(int fd) {
                this->socket = std::make_unique<TcpLanSocketBase>(fd);
                this->status = NodeStatus::Connect;
            };
            int getFd() override {
                if (!this->socket) {
                    return -1;
                }
                return this->socket->getFd();
            };
            int onRead() override;
            void onClose() override;
            int sendPacket(LANPacketType type, const void *data, size_t size) {
                if (!this->socket) {
                    return -1;
                }
                return this->socket->sendPacket(type, data, size);
            };
            void overrideInfo() {
                bool connected = this->getStatus() == NodeStatus::Connected;
                this->nodeInfo->nodeId = this->nodeId;
                if (connected) {
                    this->nodeInfo->isConnected = 1;
                } else {
                    this->nodeInfo->isConnected = 0;
                }
            }
    };

    class LDUdpSocket : public UdpLanSocketBase, public Pollable {
        protected:
            struct MacHash {
                std::size_t operator() (const MacAddress &t) const {
                    return *reinterpret_cast<const u32*>(t.raw + 2);
                }
            };
            virtual u32 getBroadcast() override;
            LANDiscovery *discovery;
        public:
            std::unordered_map<MacAddress, NetworkInfo, MacHash> scanResults;
        public:
            LDUdpSocket(int fd, LANDiscovery *discovery);
            int getFd() override {
                return UdpLanSocketBase::getFd();
            }
            int onRead() override;
            void onClose() override;
    };

    class LDTcpSocket : public TcpLanSocketBase, public Pollable {
        protected:
            LANDiscovery *discovery;
        public:
            LDTcpSocket(int fd, LANDiscovery *discovery) : TcpLanSocketBase(fd), discovery(discovery) {};
            int getFd() override {
                return TcpLanSocketBase::getFd();
            }
            int onRead() override;
            void onClose() override;
    };

    class LANDiscovery : public ILanDiscovery {
        public:
            static const int DefaultPort = 11452;
            static const char *FakeSsid;
            typedef std::function<int(LANPacketType, const void *, size_t)> ReplyFunc;
                    static const LanEventFunc EmptyFunc;
            DisconnectReason disconnect_reason;
        protected:
            friend class LDUdpSocket;
            friend class LDTcpSocket;
            friend class LanStation;
            // 0: udp 1: tcp 2: client
            LdnMutex pollMutex;
            std::unique_ptr<LDUdpSocket> udp;
            std::unique_ptr<LDTcpSocket> tcp;
            std::array<LanStation, StationCountMax> stations;
            std::array<NodeLatestUpdate, NodeCountMax> nodeChanges;
            std::array<u8, NodeCountMax> nodeLastStates;
            static void Worker(void* args);
            NifmRequest request;
            int originalMtu;
            bool stop;
            bool initialized;
            NetworkInfo networkInfo;
            u16 listenPort;
            Thread workerThread;
            CommState state;
            void worker();
            int loopPoll();
            void onSyncNetwork(NetworkInfo *info);
            void onConnect(int new_fd);
            void onDisconnectFromHost();
            void onNetworkInfoChanged();

            void updateNodes();
            void resetStations();
            Result getFakeMac(MacAddress *mac);
            Result getNodeInfo(NodeInfo *node, const UserConfig *userConfig, u16 localCommunicationVersion);
            LanEventFunc lanEvent;
            std::unique_ptr<u8[]> stack;
        public:
            Result initialize(LanEventFunc lanEvent = EmptyFunc, bool listening = true);
            Result finalize();
            Result initNetworkInfo();
            Result scan(NetworkInfo *networkInfo, u16 *count, ScanFilter filter);
            Result setAdvertiseData(const u8 *data, uint16_t size);
            Result createNetwork(const SecurityConfig *securityConfig, const UserConfig *userConfig, const NetworkConfig *networkConfig);
            Result destroyNetwork();
            Result connect(const NetworkInfo *networkInfo, UserConfig *userConfig, u16 localCommunicationVersion);
            Result disconnect();
            Result getNetworkInfo(NetworkInfo *pOutNetwork);
            Result getNetworkInfo(NetworkInfo *pOutNetwork, NodeLatestUpdate *pOutUpdates, int bufferCount);
            Result openAccessPoint();
            Result closeAccessPoint();
            Result openStation();
            Result closeStation();
        public:
            LANDiscovery(u16 port = DefaultPort) :
                disconnect_reason(DisconnectReason::None),
                pollMutex(false),
                stations({{{1, this}, {2, this}, {3, this}, {4, this}, {5, this}, {6, this}, {7, this}}}),
                stop(false), initialized(false),
                networkInfo({}), listenPort(port),
                state(CommState::None)
            {
                this->stack = std::make_unique<u8[]>(ThreadStackAlignment + StackSize);
                LogFormat("LANDiscovery");
            };
            ~LANDiscovery();
            u16 getListenPort() const override {
                return this->listenPort;
            }
            CommState getState() const override {
                return this->state;
            }
            DisconnectReason getDisconnectReason() const override {
                return this->disconnect_reason;
            }
            void setState(CommState v) {
                this->state = v;
                this->lanEvent();
            };
            int stationCount() override;
        protected:
            Result setSocketOpts(int fd);
            Result initTcp(bool listening);
            Result initUdp(bool listening);
            void initNodeStateChange();
            bool isNodeStateChanged();
    };
}
