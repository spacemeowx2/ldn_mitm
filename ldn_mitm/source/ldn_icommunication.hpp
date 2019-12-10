/*
 * Copyright (c) 2018 Atmosphère-NX
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
 
#pragma once
#include <switch.h>
#include <stratosphere.hpp>
#include "debug.hpp"
#include "lan_discovery.hpp"
#include "ldn_types.hpp"
#include "ipinfo.hpp"

namespace ams::mitm::ldn {
    class ICommunicationInterface : public sf::IServiceObject {
        private:
            enum class CommandId {
                GetState = 0,
                GetNetworkInfo = 1,
                GetIpv4Address = 2,
                GetDisconnectReason = 3,
                GetSecurityParameter = 4,
                GetNetworkConfig = 5,
                AttachStateChangeEvent = 100,
                GetNetworkInfoLatestUpdate = 101,
                Scan = 102,
                ScanPrivate = 103,                       // nyi
                SetWirelessControllerRestriction = 104,  // nyi
                OpenAccessPoint = 200,
                CloseAccessPoint = 201,
                CreateNetwork = 202,
                CreateNetworkPrivate = 203,              // nyi
                DestroyNetwork = 204,
                Reject = 205,                            // nyi
                SetAdvertiseData = 206,
                SetStationAcceptPolicy = 207,            // nyi
                AddAcceptFilterEntry = 208,              // nyi
                ClearAcceptFilter = 209,                 // nyi
                OpenStation = 300,
                CloseStation = 301,
                Connect = 302,
                ConnectPrivate = 303,                    // nyi
                Disconnect = 304,
                Initialize = 400,
                Finalize = 401,
                InitializeSystem2 = 402,                 // nyi
            };
        private:
            LANDiscovery lanDiscovery;
            os::SystemEvent state_event;
        public:
            ICommunicationInterface() {
                LogFormat("ICommunicationInterface");
                /* ... */
            };
            
            ~ICommunicationInterface() {
                LogFormat("~ICommunicationInterface");
                /* ... */
            };
        private:
            void onEventFired();
        private:
            Result Initialize(u64 unk, sf::ClientProcessId pid);
            Result InitializeSystem2(u64 unk, sf::ClientProcessId pid);
            Result Finalize();
            Result GetState(sf::Out<u32> state);
            Result GetNetworkInfo(sf::Out<NetworkInfo> buffer);
            Result GetIpv4Address(sf::Out<u32> address, sf::Out<u32> mask);
            Result GetDisconnectReason(sf::Out<u32> reason);
            Result GetSecurityParameter(sf::Out<SecurityParameter> out);
            Result GetNetworkConfig(sf::Out<NetworkConfig> out);
            Result OpenAccessPoint();
            Result CloseAccessPoint();
            Result DestroyNetwork();
            Result CreateNetwork(CreateNetworkConfig data);
            Result OpenStation();
            Result CloseStation();
            Result Disconnect();
            Result SetAdvertiseData(sf::InAutoSelectBuffer data);
            Result SetStationAcceptPolicy(u8 policy);
            Result AttachStateChangeEvent(sf::Out<sf::CopyHandle> handle);
            Result Scan(sf::Out<u32> count, sf::OutArray<NetworkInfo> buffer, u16 channel, ScanFilter filter);
            Result Connect(ConnectNetworkData dat, NetworkInfo &data);
            Result GetNetworkInfoLatestUpdate(sf::Out<NetworkInfo> buffer, sf::OutArray<NodeLatestUpdate> pUpdates);
            Result SetWirelessControllerRestriction();
        public:
            DEFINE_SERVICE_DISPATCH_TABLE {
                MAKE_SERVICE_COMMAND_META(GetState),
                MAKE_SERVICE_COMMAND_META(GetNetworkInfo),
                MAKE_SERVICE_COMMAND_META(GetIpv4Address),
                MAKE_SERVICE_COMMAND_META(GetDisconnectReason),
                MAKE_SERVICE_COMMAND_META(GetSecurityParameter),
                MAKE_SERVICE_COMMAND_META(GetNetworkConfig),
                MAKE_SERVICE_COMMAND_META(AttachStateChangeEvent),
                MAKE_SERVICE_COMMAND_META(GetNetworkInfoLatestUpdate),
                MAKE_SERVICE_COMMAND_META(Scan),
                MAKE_SERVICE_COMMAND_META(SetWirelessControllerRestriction),
                MAKE_SERVICE_COMMAND_META(OpenAccessPoint),
                MAKE_SERVICE_COMMAND_META(CloseAccessPoint),
                MAKE_SERVICE_COMMAND_META(CreateNetwork),
                MAKE_SERVICE_COMMAND_META(DestroyNetwork),
                MAKE_SERVICE_COMMAND_META(OpenStation),
                MAKE_SERVICE_COMMAND_META(CloseStation),
                MAKE_SERVICE_COMMAND_META(Connect),
                MAKE_SERVICE_COMMAND_META(Disconnect),
                MAKE_SERVICE_COMMAND_META(SetAdvertiseData),
                MAKE_SERVICE_COMMAND_META(SetStationAcceptPolicy),
                MAKE_SERVICE_COMMAND_META(Initialize),
                MAKE_SERVICE_COMMAND_META(Finalize),
                MAKE_SERVICE_COMMAND_META(InitializeSystem2),
            };
    };
}