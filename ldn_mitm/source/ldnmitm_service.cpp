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
 
#include <switch.h>
#include "debug.hpp"

#include "ldnmitm_service.hpp"
#include "ldn_icommunication.hpp"
#include "ldn_client_process_monitor.hpp"

namespace ams::mitm::ldn {

    using ObjectFactory = ams::sf::ObjectFactory<ams::sf::StdAllocationPolicy<std::allocator>>;

    LdnMitMService::LdnMitMService(std::shared_ptr<::Service> &&s, const sm::MitmProcessInfo &c) : sf::MitmServiceImplBase(std::forward<std::shared_ptr<::Service>>(s), c)
    {
        LogFormat("LdnMitMService created");
    }

    Result LdnMitMService::CreateUserLocalCommunicationService(sf::Out<sf::SharedPointer<ICommunicationInterface>> out) {
        LogFormat("CreateUserLocalCommunicationService: enabled %d", static_cast<u32>(LdnConfig::getEnabled()));

        if (LdnConfig::getEnabled()) {
            out.SetValue(sf::CreateSharedObjectEmplaced<ICommunicationInterface, ICommunicationService>());
            return 0;
        }

        return sm::mitm::ResultShouldForwardToSession();
    }

    Result LdnMitMService::CreateClientProcessMonitor(sf::Out<sf::SharedPointer<IClientProcessMonitorInterface>> out) {
        LogFormat("CreateClientProcessMonitor called");

        // Always create the monitor, regardless of ldn_mitm being enabled
        // This is required for firmware 18.0.0+ compatibility (Pokemon Legends Z-A)
        out.SetValue(sf::CreateSharedObjectEmplaced<IClientProcessMonitorInterface, IClientProcessMonitor>());
        return 0;
    }

    Result LdnMitMService::CreateLdnMitmConfigService(sf::Out<sf::SharedPointer<ILdnConfig>> out) {
        out.SetValue(sf::CreateSharedObjectEmplaced<ILdnConfig, LdnConfig>());
        return 0;
    }
}