#pragma once
#include <stratosphere.hpp>
#include "../debug.hpp"
#include "../lan_discovery.hpp"
#include "../ldn_types.hpp"
#include "../ipinfo.hpp"

namespace ams::mitm::ldn {
    #define AMS_LDN_ICOMMUNICATION(C, H)            \
        AMS_SF_METHOD_INFO(C, H, 0, Result, GetState, (sf::Out<u32> state))                                                                             \
        AMS_SF_METHOD_INFO(C, H, 1, Result, GetNetworkInfo, (sf::Out<NetworkInfo> buffer))                                                              \
        AMS_SF_METHOD_INFO(C, H, 2, Result, GetIpv4Address, (sf::Out<u32> address, sf::Out<u32> mask))                                                  \
        AMS_SF_METHOD_INFO(C, H, 3, Result, GetDisconnectReason, (sf::Out<u32> reason))                                                                 \
        AMS_SF_METHOD_INFO(C, H, 4, Result, GetSecurityParameter, (sf::Out<SecurityParameter> out))                                                     \
        AMS_SF_METHOD_INFO(C, H, 5, Result, GetNetworkConfig, (sf::Out<NetworkConfig> out))                                                             \
        AMS_SF_METHOD_INFO(C, H, 100, Result, AttachStateChangeEvent, (sf::Out<sf::CopyHandle> handle))                                                 \
        AMS_SF_METHOD_INFO(C, H, 101, Result, GetNetworkInfoLatestUpdate, (sf::Out<NetworkInfo> buffer, sf::OutArray<NodeLatestUpdate> pUpdates))       \
        AMS_SF_METHOD_INFO(C, H, 102, Result, Scan, (sf::Out<u32> count, sf::OutAutoSelectArray<NetworkInfo> buffer, u16 channel, ScanFilter filter))   \
        /*nyi*/AMS_SF_METHOD_INFO(C, H, 103, Result, ScanPrivate, ())                                                                                   \
        /*nyi*/AMS_SF_METHOD_INFO(C, H, 104, Result, SetWirelessControllerRestriction, ())                                                              \
        AMS_SF_METHOD_INFO(C, H, 200, Result, OpenAccessPoint, ())                                                                                      \
        AMS_SF_METHOD_INFO(C, H, 201, Result, CloseAccessPoint, ())                                                                                     \
        AMS_SF_METHOD_INFO(C, H, 202, Result, CreateNetwork, (CreateNetworkConfig data))                                                                \
        /*nyi*/AMS_SF_METHOD_INFO(C, H, 203, Result, CreateNetworkPrivate, ())                                                                          \
        AMS_SF_METHOD_INFO(C, H, 204, Result, DestroyNetwork, ())                                                                                       \
        /*nyi*/AMS_SF_METHOD_INFO(C, H, 205, Result, Reject, ())                                                                                        \
        AMS_SF_METHOD_INFO(C, H, 206, Result, SetAdvertiseData, (sf::InAutoSelectBuffer data))                                                          \
        /*nyi*/AMS_SF_METHOD_INFO(C, H, 207, Result, SetStationAcceptPolicy, (u8 policy))                                                               \
        /*nyi*/AMS_SF_METHOD_INFO(C, H, 208, Result, AddAcceptFilterEntry, ())                                                                          \
        /*nyi*/AMS_SF_METHOD_INFO(C, H, 209, Result, ClearAcceptFilter, ())                                                                             \
        AMS_SF_METHOD_INFO(C, H, 300, Result, OpenStation, ())                                                                                          \
        AMS_SF_METHOD_INFO(C, H, 301, Result, CloseStation, ())                                                                                         \
        AMS_SF_METHOD_INFO(C, H, 302, Result, Connect, (ConnectNetworkData dat, NetworkInfo &data))                                                     \
        /*nyi*/AMS_SF_METHOD_INFO(C, H, 303, Result, ConnectPrivate, ())                                                                                \
        AMS_SF_METHOD_INFO(C, H, 304, Result, Disconnect, ())                                                                                           \
        AMS_SF_METHOD_INFO(C, H, 400, Result, Initialize, (const sf::ClientProcessId &client_process_id))                                               \
        AMS_SF_METHOD_INFO(C, H, 401, Result, Finalize, ())                                                                                             \
        AMS_SF_METHOD_INFO(C, H, 402, Result, InitializeSystem2, (u64 unk, const sf::ClientProcessId &client_process_id))

    AMS_SF_DEFINE_INTERFACE(ICommunicationInterface, AMS_LDN_ICOMMUNICATION)
}