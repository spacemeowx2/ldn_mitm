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
#include "interfaces/icommunication.hpp"

namespace ams::mitm::ldn {
    class ICommunicationService {
        private:
            LANDiscovery lanDiscovery;
            os::SystemEvent *state_event;
            u64 error_state;
        public:
            ICommunicationService() : state_event(nullptr), error_state(0) {
                LogFormat("ICommunicationService");
                /* ... */
            };
            
            ~ICommunicationService() {
                LogFormat("~ICommunicationService");
                if (this->state_event != nullptr) {
                    delete this->state_event;
                    this->state_event = nullptr;
                }
            };
        private:
            void onEventFired();
        // private:
        public:
            Result Initialize(const sf::ClientProcessId &client_process_id);
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
            Result AttachStateChangeEvent(sf::Out<sf::CopyHandle> handle);
            Result Scan(sf::Out<u32> count, sf::OutAutoSelectArray<NetworkInfo> buffer, u16 channel, ScanFilter filter);
            Result Connect(ConnectNetworkData dat, const NetworkInfo &data);
            Result GetNetworkInfoLatestUpdate(sf::Out<NetworkInfo> buffer, sf::OutArray<NodeLatestUpdate> pUpdates);

            /*nyi----------------------------------------------------------------------------*/
            Result SetWirelessControllerRestriction();
            Result ScanPrivate();
            Result CreateNetworkPrivate();
            Result Reject();
            Result AddAcceptFilterEntry();
            Result ClearAcceptFilter();
            Result ConnectPrivate();
            Result SetStationAcceptPolicy(u8 policy);
            Result InitializeSystem2(u64 unk, const sf::ClientProcessId &client_process_id);
            /*-------------------------------------------------------------------------------*/
    };
    static_assert(ams::mitm::ldn::IsICommunicationInterface<ICommunicationService>);
}