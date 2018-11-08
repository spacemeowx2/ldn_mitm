#include "lan_discovery.hpp"

Result LANProtocol::initialize() {
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
