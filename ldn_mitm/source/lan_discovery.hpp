#pragma once
#include <switch.h>
#include <stratosphere.hpp>
#include <memory>
#include <poll.h>
#include "debug.hpp"

class LANProtocol {
    private:
        // 0: udp 1: tcp
        struct pollfd pollinfo[2];
        u16 listenPort;
    public:
        LANProtocol(u16 port) : listenPort(port) {};
        Result initialize();
        Result sendBroadcast();
        int loop();
};

class LANDiscovery {
    public:
        static const int NodeRawSize = 64;
        static const int NodeCountMax = 8;
        static const int DefaultPort = 11452;
        struct LANNode {
            u8 id;
            u8 rawData[NodeRawSize];
        };
    private:
        static void Worker(void* args);
        bool stop;
        bool inited;
        struct LANNode nodes[NodeCountMax];
        Thread worker_thread;
        void worker();
        LANProtocol protocol;
    public:
        LANDiscovery() : stop(false), inited(false), protocol(DefaultPort) {};
        LANDiscovery(u16 port) : stop(false), inited(false), protocol(port) {};
        Result initialize() {
            if (inited) {
                return 0;
            }
            if (R_FAILED(protocol.initialize())) {
                LogStr("LANDiscovery Failed to init protocol\n");
                return 0xF601;
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
        ~LANDiscovery() {
            if (inited) {
                stop = true;
                threadWaitForExit(&worker_thread);
                threadClose(&worker_thread);
            }
        };
    private:
};
