/*
 * Host stand-in for POSIX <poll.h>. Windows has no poll(); WSAPoll provides the
 * same semantics, so the host verification build can still exercise the code
 * paths that wait on sockets. Winsock already defines POLLIN/POLLOUT/..., so we
 * only alias the function.
 */
#pragma once

#include <winsock2.h>

#define nfds_t unsigned long

static inline int poll(struct pollfd *fds, nfds_t nfds, int timeout) {
    return WSAPoll(fds, nfds, timeout);
}
