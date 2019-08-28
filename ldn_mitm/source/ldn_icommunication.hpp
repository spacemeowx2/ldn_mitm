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

class ICommunicationInterface : public IServiceObject {
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
        IEvent *state_event;
    public:
        ICommunicationInterface(): state_event(nullptr) {
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
        Result Initialize(u64 unk, PidDescriptor pid);
        Result InitializeSystem2(u64 unk, PidDescriptor pid);
        Result Finalize();
        Result GetState(Out<u32> state);
        Result GetNetworkInfo(OutPointerWithServerSize<NetworkInfo, 1> buffer);
        Result GetIpv4Address(Out<u32> address, Out<u32> mask);
        Result GetDisconnectReason(Out<u32> reason);
        Result GetSecurityParameter(Out<SecurityParameter> out);
        Result GetNetworkConfig(Out<NetworkConfig> out);
        Result OpenAccessPoint();
        Result CloseAccessPoint();
        Result DestroyNetwork();
        Result CreateNetwork(CreateNetworkConfig data);
        Result OpenStation();
        Result CloseStation();
        Result Disconnect();
        Result SetAdvertiseData(InSmartBuffer<u8> data);
        Result SetStationAcceptPolicy(u8 policy);
        Result AttachStateChangeEvent(Out<CopiedHandle> handle);
        Result Scan(Out<u32> count, OutSmartBuffer<NetworkInfo> buffer, u16 channel, ScanFilter filter);
        Result Connect(ConnectNetworkData dat, InPointer<NetworkInfo> data);
        Result GetNetworkInfoLatestUpdate(OutPointerWithServerSize<NetworkInfo, 1> buffer, OutPointerWithClientSize<NodeLatestUpdate> pUpdates);
        Result SetWirelessControllerRestriction();
    public:
        DEFINE_SERVICE_DISPATCH_TABLE {
            MAKE_SERVICE_COMMAND_META(ICommunicationInterface, GetState),
            MAKE_SERVICE_COMMAND_META(ICommunicationInterface, GetNetworkInfo),
            MAKE_SERVICE_COMMAND_META(ICommunicationInterface, GetIpv4Address),
            MAKE_SERVICE_COMMAND_META(ICommunicationInterface, GetDisconnectReason),
            MAKE_SERVICE_COMMAND_META(ICommunicationInterface, GetSecurityParameter),
            MAKE_SERVICE_COMMAND_META(ICommunicationInterface, GetNetworkConfig),
            MAKE_SERVICE_COMMAND_META(ICommunicationInterface, AttachStateChangeEvent),
            MAKE_SERVICE_COMMAND_META(ICommunicationInterface, GetNetworkInfoLatestUpdate),
            MAKE_SERVICE_COMMAND_META(ICommunicationInterface, Scan),
            MAKE_SERVICE_COMMAND_META(ICommunicationInterface, SetWirelessControllerRestriction),
            MAKE_SERVICE_COMMAND_META(ICommunicationInterface, OpenAccessPoint),
            MAKE_SERVICE_COMMAND_META(ICommunicationInterface, CloseAccessPoint),
            MAKE_SERVICE_COMMAND_META(ICommunicationInterface, CreateNetwork),
            MAKE_SERVICE_COMMAND_META(ICommunicationInterface, DestroyNetwork),
            MAKE_SERVICE_COMMAND_META(ICommunicationInterface, OpenStation),
            MAKE_SERVICE_COMMAND_META(ICommunicationInterface, CloseStation),
            MAKE_SERVICE_COMMAND_META(ICommunicationInterface, Connect),
            MAKE_SERVICE_COMMAND_META(ICommunicationInterface, Disconnect),
            MAKE_SERVICE_COMMAND_META(ICommunicationInterface, SetAdvertiseData),
            MAKE_SERVICE_COMMAND_META(ICommunicationInterface, SetStationAcceptPolicy),
            MAKE_SERVICE_COMMAND_META(ICommunicationInterface, Initialize),
            MAKE_SERVICE_COMMAND_META(ICommunicationInterface, Finalize),
            MAKE_SERVICE_COMMAND_META(ICommunicationInterface, InitializeSystem2),
        };
};
