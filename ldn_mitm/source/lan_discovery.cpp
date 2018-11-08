#include "lan_discovery.hpp"
#include <sys/socket.h>
#include <sys/sockio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>

static const int ModuleID = 0xFD;
static const int BufferSize = 2048;

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
            return MAKERESULT(ModuleID, 5);
        }
    }

    return 0;
}

Result LANDiscovery::initSocket() {
    int fd;
    int rc;
    sockaddr_in addr;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        return MAKERESULT(ModuleID, 1);
    }

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(listenPort);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        return MAKERESULT(ModuleID, 2);
    }
    rc = setSocketOpts(fd);
    if (R_FAILED(rc)) {
        return rc;
    }
    discoveryfd[0].fd = fd;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return MAKERESULT(ModuleID, 6);
    }
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(listenPort);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        return MAKERESULT(ModuleID, 7);
    }
    rc = setSocketOpts(fd);
    if (R_FAILED(rc)) {
        return rc;
    }
    discoveryfd[1].fd = fd;

    return 0;
}

Result LANDiscovery::finalize() {
    return 0;
}

void LANDiscovery::Worker(void* args) {
    LANDiscovery* self = (LANDiscovery*)args;

    self->worker();
}

void LANDiscovery::worker() {
    while (!stop) {

    }
}

int LANDiscovery::compress(uint8_t *in, size_t input_size, uint8_t *output, size_t *output_size) {
    uint8_t *in_end = in + input_size;
    uint8_t *out = output;
    uint8_t *out_end = output + *output_size;

    while (out < out_end && in < in_end) {
        uint8_t c = *in++;
        uint8_t count = 1;

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

int LANDiscovery::decompress(uint8_t *input, size_t input_size, uint8_t *output, size_t *output_size) {
    uint8_t *in = input;
    uint8_t *in_end = input + input_size;
    uint8_t *out = output;
    uint8_t *out_end = output + *output_size;

    while (in < in_end && out < out_end) {
        uint8_t c = *in++;
        uint8_t count = 1;

        *out++ = c;
        if (c == 0) {
            if (in == in_end) {
                return -1;
            }
            count = *in++;
            for (int i = 1; i < count; i++) {
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
