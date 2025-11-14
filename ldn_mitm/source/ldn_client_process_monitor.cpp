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

#include "ldn_client_process_monitor.hpp"

namespace ams::mitm::ldn {
    Result IClientProcessMonitor::RegisterClient(const sf::ClientProcessId &client_process_id) {
        LogFormat("IClientProcessMonitor::RegisterClient pid: %" PRIu64, client_process_id.GetValue());
        
        // This is a stub implementation for firmware 18.0.0+ compatibility
        // Pokemon Legends Z-A requires this function but doesn't use its functionality
        // Just return success
        return ResultSuccess();
    }
}