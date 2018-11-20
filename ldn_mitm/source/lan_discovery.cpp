#include "lan_discovery.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <cstring>
#include "ipinfo.hpp"

static const int ModuleID = 0xFD;
#define POLL_UNKNOWN (~(POLLIN | POLLPRI | POLLOUT))

const char *LANDiscovery::FakeSsid = "12345678123456781234567812345678";

Result LANDiscovery::setAdvertiseData(const u8 *data, uint16_t size) {
    if (size > AdvertiseDataSizeMax) {
        return MAKERESULT(ModuleID, 10);
    }

    std::memcpy(networkInfo.ldn.advertiseData, data, size);
    networkInfo.ldn.advertiseDataSize = size;

    return 0;
}

Result LANDiscovery::initNetworkInfo() {
    Result rc = getFakeMac(this->networkInfo.common.bssid);
    if (R_FAILED(rc)) {
        return rc;
    }
    this->networkInfo.common.channel = 6;
    this->networkInfo.common.linkLevel = 3;
    this->networkInfo.common.networkType = 2;
    this->networkInfo.common.ssidLength = strlen(FakeSsid);
    strcpy(this->networkInfo.common.ssid, FakeSsid);

    auto nodes = this->networkInfo.ldn.nodes;
    for (int i = 0; i < NodeCountMax; i++) {
        nodes[i].nodeId = i;
        nodes[i].isConnected = 0;
    }
    return 0;
}

u32 LANDiscovery::getBroadcast() {
    u32 address;
    u32 netmask;
    Result rc = ipinfoGetIpConfig(&address, &netmask);
    if (R_FAILED(rc)) {
        return 0xFFFFFFFF;
    }
    u32 ret = address | ~netmask;
    return ret;
}

Result LANDiscovery::getFakeMac(MacAddress mac) {
    mac.raw[0] = 0x02;
    mac.raw[1] = 0x00;

    u32 ip;
    Result rc = ipinfoGetIpConfig(&ip);
    if (R_SUCCEEDED(rc)) {
        memcpy(mac.raw + 2, &ip, sizeof(ip));
    }

    return rc;
}

Result LANDiscovery::setSocketOpts(int fd) {
    int rc;

    {
        int b = 1;
        rc = setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &b, sizeof(b));
        if (rc != 0) {
            return MAKERESULT(ModuleID, 4);
        }
    }
    {
        int yes = 1;
        rc = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
        if (rc != 0) {
            // return MAKERESULT(ModuleID, 5);
            LogStr("SO_REUSEADDR failed\n");
        }
    }

    return 0;
}

Result LANDiscovery::initSocket(bool listening) {
    int fd;
    int rc;
    sockaddr_in addr;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        return MAKERESULT(ModuleID, 1);
    }

    if (listening) {
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htons(INADDR_ANY);
        addr.sin_port = htons(listenPort);
        if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
            return MAKERESULT(ModuleID, 2);
        }
    }
    rc = setSocketOpts(fd);
    if (R_FAILED(rc)) {
        return rc;
    }
    fds[FdUdp].fd = fd;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return MAKERESULT(ModuleID, 6);
    }
    if (listening) {
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htons(INADDR_ANY);
        addr.sin_port = htons(listenPort);
        if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
            return MAKERESULT(ModuleID, 7);
        }
        if (listen(fd, 10) != 0) {
            return MAKERESULT(ModuleID, 8);
        }
    }
    rc = setSocketOpts(fd);
    if (R_FAILED(rc)) {
        return rc;
    }
    fds[FdTcp].fd = fd;

    return 0;
}

Result LANDiscovery::finalize() {
    return 0;
}

void LANDiscovery::Worker(void* args) {
    LANDiscovery* self = (LANDiscovery*)args;

    self->worker();
}

void LANDiscovery::onNodeChanged(int fromIndex) {
    char buf[64];
    sprintf(buf, "node changed from %d %d\n", fromIndex, fds[fromIndex].fd);
    LogStr(buf);

    updateNodes();

    for (auto &i : nodes) {
        if (i.status == NodeStatus::Connected) {
            int fd = i.pfd->fd;
            if (fd == -1) continue;
            int ret = sendTcp(LANPacketType::sync_network, &networkInfo, sizeof(networkInfo), fd);
            if (ret < 0) {
                LogStr("Failed to sendTcp\n");
            }
        }
    }
}

void LANDiscovery::onMessage(int index, LANPacketType type, const void *data, size_t size, ReplyFunc reply) {
    char buf[64];
    switch (type) {
        case LANPacketType::scan: {
            if (isHost) {
                reply(LANPacketType::scan_resp, &networkInfo, sizeof(networkInfo));
            }
            break;
        }
        case LANPacketType::scan_resp: {
            LogStr("scan_resp\n");
            NetworkInfo *info = (decltype(info))data;
            if (size != sizeof(*info)) {
                break;
            }
            scanResults.insert({info->common.bssid, *info});
            break;
        }
        case LANPacketType::connect: {
            LogStr("on connect\n");
            NodeInfo *info = (decltype(info))data;
            if (size != sizeof(*info)) {
                LogStr("NodeInfo size is wrong\n");
                break;
            }
            int nodeId = index - DiscoveryFds;
            if (nodeId < 0) {
                break;
            }
            auto &node = nodes[nodeId];
            *node.nodeInfo = *info;
            node.status = NodeStatus::Connected;
            onNodeChanged(index);

            break;
        }
        case LANPacketType::sync_network: {
            LogStr("sync_network\n");
            NetworkInfo *info = (decltype(info))data;
            if (size != sizeof(*info)) {
                break;
            }
            networkInfo = *info;
            break;
        }
        default: {
            sprintf(buf, "on_message unhandle type %d\n", static_cast<int>(type));
            LogStr(buf);
            break;
        }
    }
}

Result LANDiscovery::scan(NetworkInfo *networkInfo, u16 *count) {
    scanResults.clear();

    int len = sendBroadcast(LANPacketType::scan);
    if (len < 0) {
        return MAKERESULT(ModuleID, 20);
    }

    svcSleepThread(1000000000L); // 1sec

    int i = 0;
    for (auto item : scanResults) {
        if (i >= *count) {
            break;
        }
        networkInfo[i++] = item.second;
    }
    *count = i;

    return 0;
}

int LANDiscovery::sendBroadcast(LANPacketType type, const void *data, size_t size) {
    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(getBroadcast());
    addr.sin_port = htons(listenPort);

    return sendTo(type, data, size, addr);
}

int LANDiscovery::sendBroadcast(LANPacketType type) {
    return sendBroadcast(type, NULL, 0);
}

int LANDiscovery::sendTo(LANPacketType type, const void *data, size_t size, struct sockaddr_in &addr) {
    return sendPacket(type, data, size, [&](const void *d, size_t s) {
        return sendto(fds[FdUdp].fd, d, s, 0, (struct sockaddr *)&addr, sizeof(addr));
    });
}

int LANDiscovery::sendTcp(LANPacketType type, const void *data, size_t size) {
    return sendTcp(type, data, size, nodes[0].pfd->fd);
}

int LANDiscovery::sendTcp(LANPacketType type, const void *data, size_t size, int fd) {
    return sendPacket(type, data, size, [&](const void *d, size_t s) {
        return send(fd, d, s, 0);
    });
}

int LANDiscovery::sendPacket(LANPacketType type, const void *data, size_t size, std::function<int(const void *, size_t)> bindSend) {
    LANPacketHeader header;
    prepareHeader(header, type);
    if (data == NULL) {
        size = 0;
    }
    header.length = size;
    u8 buf[size + sizeof(header)];
    if (size > 0) {
        u8 compressed[size];
        size_t outSize = size;
        if (compress(data, size, compressed, &outSize) == 0) {
            std::memcpy(buf + sizeof(header), compressed, outSize);
            header.decompress_length = header.length;
            header.length = outSize;
            header.compressed = true;
        } else {
            std::memcpy(buf + sizeof(header), data, size);
        }
    }
    std::memcpy(buf, &header, sizeof(header));
    return bindSend(buf, header.length + sizeof(header));
}

void LANDiscovery::prepareHeader(LANPacketHeader &header, LANPacketType type) {
    header.magic = LANMagic;
    header.type = type;
    header.compressed = false;
    header.length = 0;
    header.decompress_length = 0;
    header._reserved[0] = 0;
    header._reserved[1] = 0;
}

int LANDiscovery::nodeCount() {
    int count = 0;

    for (auto const &i : nodes) {
        if (i.status != NodeStatus::Disconnected) {
            count++;
        }
    }

    return count;
}

void LANDiscovery::updateNodes() {
    int count = 0;
    for (int i = 0; i < NodeMaxCount; i++) {
        bool connected = nodes[i].status == NodeStatus::Connected;
        if (connected) {
            count++;
            networkInfo.ldn.nodes[i].nodeId = i;
            networkInfo.ldn.nodes[i].isConnected = 1;
        } else {
            networkInfo.ldn.nodes[i].isConnected = 0;
        }
    }
    networkInfo.ldn.nodeCount = count;
}

void LANDiscovery::nodeClose(int nodeId) {
    char buf[64];
    sprintf(buf, "node %d closed\n", nodeId);
    LogStr(buf);
    LANNode &node = nodes[nodeId];
    close(node.pfd->fd);
    node.pfd->fd = -1;
    node.status = NodeStatus::Disconnected;
}

int LANDiscovery::nodeRecv(int nodeId, u8 *buffer, size_t bufLen) {
    constexpr int HeaderSize = sizeof(LANPacketHeader);
    LANNode &node = nodes[nodeId];
    const int fd = node.pfd->fd;
    ssize_t len = recv(fd, (void *)(node.buffer + node.recvSize), sizeof(node.buffer) - node.recvSize, 0);
    if (len <= 0) {
        nodeClose(nodeId);
    }
    node.recvSize += len;

    if (node.recvSize < HeaderSize) {
        return 0;
    }

    LANPacketHeader *header = (decltype(header))node.buffer;
    if (header->magic != LANMagic) {
        nodeClose(nodeId);
        return -1;
    }

    const auto total = HeaderSize + header->length;
    if (total > BufferSize) {
        nodeClose(nodeId);
        return -1;
    }
    if (node.recvSize < total) {
        return 0;
    }

    std::memcpy(buffer, node.buffer, total);
    node.recvSize -= total;
    std::memcpy(node.buffer, node.buffer + total, node.recvSize);

    return total;
}

void LANDiscovery::onPacket(int index) {
    static u8 buffer[BufferSize];
    static u8 decompressBuffer[BufferSize];
    constexpr int HeaderSize = sizeof(LANPacketHeader);

    const int fd = fds[index].fd;
    ssize_t len;
    std::function<int(const void *, size_t)> bindSend;
    struct sockaddr_in addr;
    socklen_t addr_len;

    // udp
    if (index == FdUdp) {
        addr_len = sizeof(addr);
        len = recvfrom(fd, buffer, sizeof(buffer), 0, (struct sockaddr *)&addr, &addr_len);
        if (len <= 0) {
            LogStr("udp len <= 0\n");
        }
        bindSend = [&](const void *data, size_t size) {
            return sendto(fd, data, size, 0, (struct sockaddr *)&addr, sizeof(addr));
        };
    } else { // tcp
        int nodeId = index - DiscoveryFds;
        len = nodeRecv(nodeId, buffer, sizeof(buffer));
        bindSend = [&](const void *data, size_t size) {
            return send(fd, data, size, 0);
        };
    }
    if ((size_t)len < HeaderSize) {
        return;
    }
    LANPacketHeader *header = (decltype(header))buffer;
    if (header->magic != LANMagic) {
        return;
    }

    auto reply = [&](LANPacketType type, const void *data, size_t size) {
        return sendPacket(type, data, size, bindSend);
    };
    auto body = buffer + HeaderSize;
    auto bodyLen = len - HeaderSize;
    if (header->compressed) {
        size_t outSize = sizeof(decompressBuffer);
        if (decompress(body, bodyLen, decompressBuffer, &outSize) != 0) {
            LogStr("Decompress error\n");
            return;
        }
        if (outSize != header->decompress_length) {
            LogStr("Decompress error length not match\n");
            return;
        }
        body = decompressBuffer;
        bodyLen = outSize;
    }
    onMessage(index, header->type, body, bodyLen, reply);
}

void LANDiscovery::onNodeConnect() {
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    int new_fd = accept(fds[FdTcp].fd, (struct sockaddr *)&addr, &addrlen);
    if (new_fd < 0)
    {
        LogStr("accept failed\n");
        return;
    }
    char buf[64];
    sprintf(buf, "Accepted %d\n", new_fd);
    LogStr(buf);
    if (nodeCount() >= NodeMaxCount) {
        LogStr("Close new_fd. nodes are full\n");
        close(new_fd);
        return;
    }

    int i;
    for (i = 0; i < NodeMaxCount; i++) {
        if (nodes[i].status == NodeStatus::Disconnected) {
            break;
        }
    }
    nodes[i].status = NodeStatus::Connect;
    nodes[i].recvSize = 0;
    nodes[i].pfd->fd = new_fd;
}

void LANDiscovery::loopPoll() {
    char buf[64];
    int rc;
    int nfds = DiscoveryFds + NodeMaxCount;
    if (!inited) {
        return;
    }

    {
        std::scoped_lock<HosMutex> lock(fdsMutex);
        rc = poll(fds.data(), nfds, 100);
    }
    if (rc < 0)
    {
        sprintf(buf, "loopPoll failed %d\n", rc);
        LogStr(buf);
        return;
    } else if (rc > 0) {
        /* check the command socket */
        if (fds[FdUdp].revents != 0) {
            if (fds[FdUdp].revents & POLL_UNKNOWN) {
                sprintf(buf, "cmd_fd: revents=0x%08X\n", fds[FdUdp].revents);
                LogStr(buf);
            }

            if (fds[FdUdp].revents & (POLLERR | POLLHUP)) {
                sprintf(buf, "cmd revents=0x%x\n", fds[FdUdp].revents);
                LogStr(buf);
            } else if (fds[FdUdp].revents & (POLLIN | POLLPRI)) {
                onPacket(0);
            }
        }
        if (fds[FdTcp].revents != 0) {
            if (fds[FdTcp].revents & POLL_UNKNOWN) {
                sprintf(buf, "cmd_fd: revents=0x%08X\n", fds[FdUdp].revents);
                LogStr(buf);
            }
            if (fds[FdTcp].revents & POLLIN)
            {
                onNodeConnect();
            }
        }
        for (int i = DiscoveryFds; i < nfds; i++) {
            if (fds[i].revents & (POLLIN | POLLPRI)) {
                onPacket(i);
            } else if (fds[i].revents & (POLLERR | POLLHUP)) {
                const int nodeId = i - DiscoveryFds;
                nodeClose(nodeId);
            }
        }
    }
}

LANDiscovery::~LANDiscovery() {
    if (inited) {
        stop = true;
        threadWaitForExit(&worker_thread);
        threadClose(&worker_thread);

        for (auto &i : fds) {
            if (i.fd != -1) {
                close(i.fd);
                i.fd = -1;
            }
        }
    }
}

void LANDiscovery::worker() {
    while (!stop) {
        loopPoll();
        svcSleepThread(0);
    }
    LogStr("Worker exit\n");
    svcExitThread();
}

Result LANDiscovery::getNetworkInfo(NetworkInfo *info) {
    std::memcpy(info, &networkInfo, sizeof(networkInfo));
    return 0;
}

Result LANDiscovery::getNodeInfo(NodeInfo *node, const UserConfig *userConfig, u16 localCommunicationVersion) {
    u32 ipAddress;
    Result rc = ipinfoGetIpConfig(&ipAddress);
    if (R_FAILED(rc)) {
        return rc;
    }
    rc = getFakeMac(node->macAddress);
    if (R_FAILED(rc)) {
        return rc;
    }

    node->isConnected = 1;
    strcpy(node->userName, userConfig->userName);
    node->localCommunicationVersion = localCommunicationVersion;
    node->ipv4Address = ipAddress;

    return 0;
}

Result LANDiscovery::createNetwork(const SecurityConfig *securityConfig, const UserConfig *userConfig, const NetworkConfig *networkConfig) {
    Result rc = 0;
    u32 ip;

    rc = ipinfoGetIpConfig(&ip);
    if (R_FAILED(rc)) {
        return rc;
    }

    rc = initNetworkInfo();
    if (R_FAILED(rc)) {
        return rc;
    }
    networkInfo.ldn.nodeCountMax = networkConfig->nodeCountMax;
    networkInfo.ldn.securityMode = securityConfig->securityMode;
    if (networkConfig->channel == 0) {
        networkInfo.common.channel = 6;
    } else {
        networkInfo.common.channel = networkConfig->channel;
    }
    networkInfo.networkId.intentId = networkConfig->intentId;

    rc = getNodeInfo(nodes[0].nodeInfo, userConfig, networkConfig->localCommunicationVersion);
    if (R_FAILED(rc)) {
        return rc;
    }
    nodes[0].status = NodeStatus::Connected;

    updateNodes();

    isHost = true;

    return rc;
}

Result LANDiscovery::destroyNetwork() {
    isHost = false;

    return 0;
}

Result LANDiscovery::disconnect() {
    std::scoped_lock<HosMutex> lock(fdsMutex);
    if (nodes[0].pfd->fd != -1) {
        close(nodes[0].pfd->fd);
        nodes[0].pfd->fd = -1;
    }

    return 0;
}

Result LANDiscovery::connect(NetworkInfo *networkInfo, UserConfig *userConfig, u16 localCommunicationVersion) {
    if (networkInfo->ldn.nodeCount == 0) {
        return MAKERESULT(ModuleID, 30);
    }

    u32 hostIp = networkInfo->ldn.nodes[0].ipv4Address;
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(hostIp);
    addr.sin_port = htons(listenPort);
    char buf[64];
    sprintf(buf, "connect hostIp %x\n", hostIp);
    LogStr(buf);

    {
        std::scoped_lock<HosMutex> lock(fdsMutex);
        if (nodes[0].pfd->fd == -1) {
            int fd = socket(AF_INET, SOCK_STREAM, 0);
            if (fd < 0) {
                return MAKERESULT(ModuleID, 36);
            }
            Result rc = setSocketOpts(fd);
            if (R_FAILED(rc)) {
                return rc;
            }
            nodes[0].pfd->fd = fd;
        }
    }

    int ret = ::connect(nodes[0].pfd->fd, (struct sockaddr *)&addr, sizeof(addr));
    if (ret != 0) {
        LogStr("connect failed\n");
        return MAKERESULT(ModuleID, 31);
    }

    NodeInfo myNode = {0};
    Result rc = getNodeInfo(&myNode, userConfig, localCommunicationVersion);
    if (R_FAILED(rc)) {
        return rc;
    }
    ret = sendTcp(LANPacketType::connect, &myNode, sizeof(myNode));
    if (ret < 0) {
        LogStr("sendTcp failed\n");
        return MAKERESULT(ModuleID, 32);
    }

    isHost = false;

    return 0;
}

Result LANDiscovery::initialize(bool listening) {
    if (inited) {
        return 0;
    }
    for(auto &i : fds) {
        i.fd = -1;
        i.events = POLLIN;
        i.revents = 0;
    }
    for (int i = 0; i < NodeMaxCount; i++) {
        nodes[i].status = NodeStatus::Disconnected;
        nodes[i].pfd = &fds[DiscoveryFds + i];
        nodes[i].nodeInfo = &networkInfo.ldn.nodes[i];
    }
    Result rc = initSocket(listening);
    if (R_FAILED(rc)) {
        char buf[64];
        sprintf(buf, "initSocket %x\n", rc);
        LogStr(buf);
        return 0xF601;
    }

    rc = initNetworkInfo();
    if (R_FAILED(rc)) {
        char buf[64];
        sprintf(buf, "initNetworkInfo %x\n", rc);
        LogStr(buf);
        return rc;
    }

    if (R_FAILED(threadCreate(&worker_thread, &Worker, this, 0x4000, 0x15, 0))) {
        LogStr("LANDiscovery Failed to threadCreate\n");
        return 0xF601;
    }
    if (R_FAILED(threadStart(&worker_thread))) {
        LogStr("LANDiscovery Failed to threadStart\n");
        return 0xF601;
    }

    inited = true;
    return 0;
}

int LANDiscovery::compress(const void *input, size_t input_size, uint8_t *output, size_t *output_size) {
#if DISABLE_COMPRESS
    return -1;
#endif
    const uint8_t *in = (decltype(in))input;
    const uint8_t *in_end = in + input_size;
    uint8_t *out = output;
    uint8_t *out_end = output + *output_size;

    while (out < out_end && in < in_end) {
        uint8_t c = *in++;
        uint8_t count = 0;

        if (c == 0) {
            while (*in == 0 && in < in_end && count < 0xFF) {
                count += 1;
                in++;
            }
        }

        if (c == 0x00) {
            *out++ = 0;

            if (out == out_end)
                return -1;
            *out++ = count;
        } else {
            *out++ = c;
        }
    }

    *output_size = out - output;

    return in == in_end ? 0 : -1;
}

int LANDiscovery::decompress(const void *input, size_t input_size, uint8_t *output, size_t *output_size) {
#if DISABLE_COMPRESS
    return -1;
#endif
    const uint8_t *in = (decltype(in))input;
    const uint8_t *in_end = in + input_size;
    uint8_t *out = output;
    uint8_t *out_end = output + *output_size;

    while (in < in_end && out < out_end) {
        uint8_t c = *in++;

        *out++ = c;
        if (c == 0) {
            if (in == in_end) {
                return -1;
            }
            uint8_t count = *in++;
            for (int i = 0; i < count; i++) {
                if (out == out_end) {
                    break;
                }
                *out++ = c;
            }
        }
    }

    *output_size = out - output;

    return in == in_end ? 0 : -1;
}
