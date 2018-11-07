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
#include "ldn_types.h"

enum LdnCommCmd {
    LdnCommCmd_GetState = 0,
    LdnCommCmd_GetNetworkInfo = 1,
    LdnCommCmd_GetIpv4Address = 2,
    LdnCommCmd_GetDisconnectReason = 3,
    LdnCommCmd_GetSecurityParameter = 4,
    LdnCommCmd_GetNetworkConfig = 5,
    LdnCommCmd_AttachStateChangeEvent = 100,
    LdnCommCmd_GetNetworkInfoLatestUpdate = 101,
    LdnCommCmd_Scan = 102,
    // 103
    LdnCommCmd_OpenAccessPoint = 200,
    LdnCommCmd_CloseAccessPoint = 201,
    LdnCommCmd_CreateNetwork = 202,
    // 203
    LdnCommCmd_DestroyNetwork = 204,
    // 205
    LdnCommCmd_SetAdvertiseData = 206,
    // 207-209
    LdnCommCmd_OpenStation = 300,
    LdnCommCmd_CloseStation = 301,
    LdnCommCmd_Connect = 302,
    // 303
    LdnCommCmd_Disconnect = 304,
    LdnCommCmd_Initialize = 400,
    LdnCommCmd_Finalize = 401,
};

enum class CommState {
    None,
    Initialized,
    AccessPoint,
    AccessPointCreated,
    Station,
    StationConnected,
    Error
};

class ICommunicationInterface : public IServiceObject {
    private:
        static Service nifmSrv;
        static Service nifmIGS;
        static const char *FakeSsid;
        CommState state;
        IEvent *state_event;
        NetworkInfo network_info;
    public:
        ICommunicationInterface(): state(CommState::None), network_info({0}) {
            LogStr("ICommunicationInterface\n");
            /* ... */
        };
        
        ~ICommunicationInterface() {
            LogStr("~ICommunicationInterface\n");
            /* ... */
        };
    private:
        static Result nifmInit();
        void get_fake_mac(u8 mac[6]);
        void set_state(CommState new_state) {
            this->state = new_state;
            if (this->state_event) {
                LogStr("state_event signal_event\n");
                this->state_event->Signal();
            }
        }
        void init_network_info() {
            memset(&this->network_info, 0, sizeof(NetworkInfo));
            this->network_info.common.channel = 6;
            this->network_info.common.linkLevel = 3;
            this->network_info.common.networkType = 2;
            this->network_info.common.ssidLength = strlen(FakeSsid);

            get_fake_mac(this->network_info.common.bssid);
            strcpy(this->network_info.common.ssid, FakeSsid);
            NodeInfo *nodes = this->network_info.ldn.nodes;
            for (int i = 0; i < NodeCountMax; i++) {
                nodes[i].nodeId = i;
            }
        }
    private:
        Result ReturnSuccess() {
            return 0;
        }        
        Result Initialize(u64 unk, PidDescriptor pid);
        Result Finalize();
        Result GetState(Out<u32> state);
        Result GetNetworkInfo(OutPointerWithServerSize<NetworkInfo, 1> buffer);
        Result GetIpv4Address(Out<u32> address, Out<u32> mask);
        Result GetDisconnectReason(Out<u16> reason);
        Result GetSecurityParameter(Out<SecurityParameter> out);
        Result GetNetworkConfig(Out<NetworkConfig> out);
        Result OpenAccessPoint();
        Result CloseAccessPoint();
        Result DestroyNetwork();
        Result CreateNetwork(CreateNetworkConfig data);
        Result OpenStation();
        Result CloseStation();
        Result Disconnect();
        Result SetAdvertiseData(InPointer<u8> data1, InBuffer<u8> data2);
        Result AttachStateChangeEvent(Out<CopiedHandle> handle);
        Result Scan(Out<u16> count, OutPointerWithServerSize<u8, 0> buffer, OutBuffer<NetworkInfo> data, u16 bufferCount);
        Result Connect(ConnectNetworkData dat, InPointer<u8> data);
        Result GetNetworkInfoLatestUpdate(OutPointerWithServerSize<NetworkInfo, 1> buffer1, OutPointerWithServerSize<NodeLatestUpdate, 1> buffer2);
    public:
        DEFINE_SERVICE_DISPATCH_TABLE {
            MakeServiceCommandMeta<LdnCommCmd_GetState, &ICommunicationInterface::GetState>(),
            MakeServiceCommandMeta<LdnCommCmd_GetNetworkInfo, &ICommunicationInterface::GetNetworkInfo>(),
            MakeServiceCommandMeta<LdnCommCmd_GetIpv4Address, &ICommunicationInterface::GetIpv4Address>(),
            MakeServiceCommandMeta<LdnCommCmd_GetDisconnectReason, &ICommunicationInterface::GetDisconnectReason>(),
            MakeServiceCommandMeta<LdnCommCmd_GetNetworkConfig, &ICommunicationInterface::GetNetworkConfig>(),
            MakeServiceCommandMeta<LdnCommCmd_AttachStateChangeEvent, &ICommunicationInterface::AttachStateChangeEvent>(),
            MakeServiceCommandMeta<LdnCommCmd_GetNetworkInfoLatestUpdate, &ICommunicationInterface::GetNetworkInfoLatestUpdate>(),
            MakeServiceCommandMeta<LdnCommCmd_Scan, &ICommunicationInterface::Scan>(),
            MakeServiceCommandMeta<LdnCommCmd_OpenAccessPoint, &ICommunicationInterface::OpenAccessPoint>(),
            MakeServiceCommandMeta<LdnCommCmd_CloseAccessPoint, &ICommunicationInterface::CloseAccessPoint>(),
            MakeServiceCommandMeta<LdnCommCmd_CreateNetwork, &ICommunicationInterface::CreateNetwork>(),
            MakeServiceCommandMeta<LdnCommCmd_DestroyNetwork, &ICommunicationInterface::DestroyNetwork>(),
            MakeServiceCommandMeta<LdnCommCmd_OpenStation, &ICommunicationInterface::OpenStation>(),
            MakeServiceCommandMeta<LdnCommCmd_CloseStation, &ICommunicationInterface::CloseStation>(),
            MakeServiceCommandMeta<LdnCommCmd_Connect, &ICommunicationInterface::Connect>(),
            MakeServiceCommandMeta<LdnCommCmd_Disconnect, &ICommunicationInterface::Disconnect>(),
            MakeServiceCommandMeta<LdnCommCmd_SetAdvertiseData, &ICommunicationInterface::SetAdvertiseData>(),
            MakeServiceCommandMeta<LdnCommCmd_Initialize, &ICommunicationInterface::Initialize>(),
            MakeServiceCommandMeta<LdnCommCmd_Finalize, &ICommunicationInterface::Finalize>(),
        };
};
