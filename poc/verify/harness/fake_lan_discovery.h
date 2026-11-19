/*
 * fake_lan_discovery.h -- scripted ILanDiscovery for the host tests.
 *
 * The real LANDiscovery owns sockets and a worker thread, which is neither
 * portable to the host nor useful when what we want to verify is the IPC
 * marshalling. The host tests inject this instead.
 */
#pragma once

#include <cstring>

#include "lan_discovery_interface.hpp"

namespace ams::mitm::ldn {

struct FakeLanState {
    CommState state = CommState::None;
    DisconnectReason disconnect_reason = DisconnectReason::None;
    NetworkInfo networkInfo{};

    /* scan() */
    int scanCount = 0;
    NetworkInfo scanResults[NodeCountMax] = {};

    /* setAdvertiseData() */
    u16 advertiseSize = 0;
    u8 advertiseData[AdvertiseDataSizeMax] = {};

    /* getNetworkInfo(..., updates, count) */
    int updatesWritten = 0;

    /* call counters */
    int initializeCalls = 0;
    int finalizeCalls = 0;
    int openAccessPointCalls = 0;
    int closeAccessPointCalls = 0;
    int openStationCalls = 0;
    int closeStationCalls = 0;
    int destroyNetworkCalls = 0;
    int disconnectCalls = 0;
    int connectCalls = 0;
    int createNetworkCalls = 0;
    int scanCalls = 0;
    int getNetworkInfoCalls = 0;

    /* last connect() arguments */
    NetworkInfo lastConnectTarget{};
    UserConfig lastConnectUser{};
    u16 lastConnectVersion = 0;

    void Reset() { *this = FakeLanState{}; }
};

FakeLanState &FakeLan();

ILanDiscovery &FakeLanImpl();

} // namespace ams::mitm::ldn
