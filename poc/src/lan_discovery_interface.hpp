/*
 * lan_discovery_interface.hpp -- the seam between the IPC layer and the LAN
 * implementation.
 *
 * ICommunicationService only needs these operations, so tests can script them
 * without dragging in sockets, threads and the real LANDiscovery vtable, and the
 * sysmodule still uses the real one.
 */
#pragma once

#include <switch.h>

#include <functional>

#include "ldn_types.hpp"

namespace ams::mitm::ldn {

    enum class DisconnectReason {
        None,
        DisconnectedByUser,
        DisconnectedBySystem,
        DestroyedByUser,
        DestroyedBySystem,
        Rejected,
        SignalLost
    };

    using LanEventFunc = std::function<void()>;

    class ILanDiscovery {
        public:
            static constexpr u16 DefaultPort = 11452;

            virtual ~ILanDiscovery() = default;

            virtual Result initialize(LanEventFunc lanEvent, bool listening = true) = 0;
            virtual Result finalize() = 0;
            virtual Result initNetworkInfo() = 0;

            virtual Result scan(NetworkInfo *networkInfo, u16 *count, ScanFilter filter) = 0;
            virtual Result setAdvertiseData(const u8 *data, uint16_t size) = 0;
            virtual Result createNetwork(const SecurityConfig *securityConfig, const UserConfig *userConfig, const NetworkConfig *networkConfig) = 0;
            virtual Result destroyNetwork() = 0;
            virtual Result connect(const NetworkInfo *networkInfo, UserConfig *userConfig, u16 localCommunicationVersion) = 0;
            virtual Result disconnect() = 0;

            virtual Result getNetworkInfo(NetworkInfo *pOutNetwork) = 0;
            virtual Result getNetworkInfo(NetworkInfo *pOutNetwork, NodeLatestUpdate *pOutUpdates, int bufferCount) = 0;

            virtual Result openAccessPoint() = 0;
            virtual Result closeAccessPoint() = 0;
            virtual Result openStation() = 0;
            virtual Result closeStation() = 0;

            virtual CommState getState() const = 0;
            virtual DisconnectReason getDisconnectReason() const = 0;
            virtual u16 getListenPort() const = 0;
            virtual int stationCount() = 0;
    };

} // namespace ams::mitm::ldn
