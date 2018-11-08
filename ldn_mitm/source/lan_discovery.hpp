#pragma once
#include <switch.h>
#include <stratosphere.hpp>
#include <memory>
#include <poll.h>
#include "debug.hpp"
#include "ldn_types.h"

class LANDiscovery {
    public:
        static const int DefaultPort = 11452;
        static const u32 LANMagic = 0x114514;
        struct PayloadScanResponse {
            u16 size;
            u8 data[sizeof(NetworkInfo)];
        };
        enum class LANPacketType : u8 {
            scan,
            scan_resp,
            connect,
            disconnect,
        };
        struct LANPacketHeader {
            u32 magic;
            LANPacketType type;
            u8 compressed;
            u8 _reserved[2];
        };
    protected:
        // 0: udp 1: tcp
        struct pollfd discoveryfd[2];
        struct pollfd nodefd[8];
        int nodeCount;
        static void Worker(void* args);
        bool stop;
        bool inited;
        NetworkInfo networkInfo;
        Thread worker_thread;
        void worker();
        u16 listenPort;
    public:
        LANDiscovery() : stop(false), inited(false), listenPort(DefaultPort) {};
        LANDiscovery(u16 port) : stop(false), inited(false), listenPort(port) {};
        Result initialize() {
            for(auto &i : discoveryfd) {
                i.fd = -1;
                i.events = POLLIN;
                i.revents = 0;
            }
            for(auto &i : nodefd) {
                i.fd = -1;
                i.events = POLLIN;
                i.revents = 0;
            }
            if (inited) {
                return 0;
            }
            if (R_FAILED(initSocket())) {
                LogStr("LANDiscovery Failed to initializeSocket\n");
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
    protected:
        Result setSocketOpts(int fd);
        Result initSocket();
        Result finalize();
        int compress(uint8_t *in, size_t input_size, uint8_t *output, size_t *output_size);
        int decompress(uint8_t *in, size_t input_size, uint8_t *output, size_t *output_size);
};
