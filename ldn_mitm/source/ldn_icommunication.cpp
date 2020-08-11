#include "ldn_icommunication.hpp"

namespace ams::mitm::ldn {
    static_assert(sizeof(NetworkInfo) == 0x480, "sizeof(NetworkInfo) should be 0x480");
    static_assert(sizeof(ConnectNetworkData) == 0x7C, "sizeof(ConnectNetworkData) should be 0x7C");
    static_assert(sizeof(ScanFilter) == 0x60, "sizeof(ScanFilter) should be 0x60");

    // https://reswitched.github.io/SwIPC/ifaces.html#nn::ldn::detail::IUserLocalCommunicationService

    Result ICommunicationService::Initialize(const sf::ClientProcessId &client_process_id) {
        LogFormat("ICommunicationService::Initialize pid: %" PRIu64, client_process_id);

        if (this->state_event == nullptr) {
            // ClearMode, inter_process
            this->state_event = new os::SystemEvent(ams::os::EventClearMode_AutoClear, true);
        }

        R_TRY(lanDiscovery.initialize([&](){
            this->onEventFired();
        }));

        return ResultSuccess();
    }

    Result ICommunicationService::InitializeSystem2(u64 unk, const sf::ClientProcessId &client_process_id) {
        LogFormat("ICommunicationService::InitializeSystem2 unk: %" PRIu64, unk);
        this->error_state = unk;
        return this->Initialize(client_process_id);
    }

    Result ICommunicationService::Finalize() {
        Result rc = lanDiscovery.finalize();
        if (this->state_event) {
            delete this->state_event;
            this->state_event = nullptr;
        }
        return rc;
    }

    Result ICommunicationService::OpenAccessPoint() {
        return this->lanDiscovery.openAccessPoint();
    }

    Result ICommunicationService::CloseAccessPoint() {
        return this->lanDiscovery.closeAccessPoint();
    }

    Result ICommunicationService::DestroyNetwork() {
        return this->lanDiscovery.destroyNetwork();
    }

    Result ICommunicationService::OpenStation() {
        return this->lanDiscovery.openStation();
    }

    Result ICommunicationService::CloseStation() {
        return this->lanDiscovery.closeStation();
    }

    Result ICommunicationService::Disconnect() {
        return this->lanDiscovery.disconnect();
    }

    Result ICommunicationService::CreateNetwork(CreateNetworkConfig data) {
        return this->lanDiscovery.createNetwork(&data.securityConfig, &data.userConfig, &data.networkConfig);;
    }

    Result ICommunicationService::SetAdvertiseData(sf::InAutoSelectBuffer data) {
        return lanDiscovery.setAdvertiseData(data.GetPointer(), data.GetSize());
    }

    Result ICommunicationService::GetState(sf::Out<u32> state) {
        state.SetValue(static_cast<u32>(this->lanDiscovery.getState()));

        if (this->error_state) {
            if (this->lanDiscovery.disconnect_reason != DisconnectReason::None) {
                return MAKERESULT(0x10, static_cast<u32>(this->lanDiscovery.disconnect_reason));
            }
        }

        return 0;
    }

    Result ICommunicationService::GetIpv4Address(sf::Out<u32> address, sf::Out<u32> netmask) {
        Result rc = ipinfoGetIpConfig(address.GetPointer(), netmask.GetPointer());

        LogFormat("get_ipv4_address %x %x", address.GetValue(), netmask.GetValue());

        return rc;
    }

    Result ICommunicationService::GetNetworkInfo(sf::Out<NetworkInfo> buffer) {
        LogFormat("get_network_info %p state: %d", buffer.GetPointer(), static_cast<u32>(this->lanDiscovery.getState()));

        return lanDiscovery.getNetworkInfo(buffer.GetPointer());
    }

    Result ICommunicationService::GetDisconnectReason(sf::Out<u32> reason) {
        auto dr = static_cast<u32>(this->lanDiscovery.disconnect_reason);
        LogFormat("GetDisconnectReason %p state: %d reason: %u", reason.GetPointer(), static_cast<u32>(this->lanDiscovery.getState()), dr);
        reason.SetValue(dr);

        return 0;
    }

    Result ICommunicationService::GetNetworkInfoLatestUpdate(sf::Out<NetworkInfo> buffer, sf::OutArray<NodeLatestUpdate> pUpdates) {
        LogFormat("get_network_info_latest buffer %p", buffer.GetPointer());
        LogFormat("get_network_info_latest pUpdates %p %" PRIu64, pUpdates.GetPointer(), pUpdates.GetSize());

        return lanDiscovery.getNetworkInfo(buffer.GetPointer(), pUpdates.GetPointer(), pUpdates.GetSize());
    }

    Result ICommunicationService::GetSecurityParameter(sf::Out<SecurityParameter> out) {
        Result rc = 0;

        SecurityParameter data;
        NetworkInfo info;
        rc = lanDiscovery.getNetworkInfo(&info);
        if (R_SUCCEEDED(rc)) {
            NetworkInfo2SecurityParameter(&info, &data);
            out.SetValue(data);
        }

        return rc;
    }

    Result ICommunicationService::GetNetworkConfig(sf::Out<NetworkConfig> out) {
        Result rc = 0;

        NetworkConfig data;
        NetworkInfo info;
        rc = lanDiscovery.getNetworkInfo(&info);
        if (R_SUCCEEDED(rc)) {
            NetworkInfo2NetworkConfig(&info, &data);
            out.SetValue(data);
        }

        return rc;
    }

    Result ICommunicationService::AttachStateChangeEvent(sf::Out<sf::CopyHandle> handle) {
        handle.SetValue(this->state_event->GetReadableHandle());
        return ResultSuccess();
    }

    Result ICommunicationService::Scan(sf::Out<u32> outCount, sf::OutAutoSelectArray<NetworkInfo> buffer, u16 channel, ScanFilter filter) {
        Result rc = 0;
        u16 count = buffer.GetSize();

        rc = lanDiscovery.scan(buffer.GetPointer(), &count, filter);
        outCount.SetValue(count);

        LogFormat("scan %d %d", count, rc);

        return rc;
    }

    Result ICommunicationService::Connect(ConnectNetworkData param, NetworkInfo &data) {
        LogFormat("ICommunicationService::connect");
        LogHex(&data, sizeof(NetworkInfo));
        LogHex(&param, sizeof(param));

        return lanDiscovery.connect(&data, &param.userConfig, param.localCommunicationVersion);
    }

    void ICommunicationService::onEventFired() {
        if (this->state_event) {
            LogFormat("onEventFired signal_event");
            this->state_event->Signal();
        }
    }

    /*nyi*/
    Result ICommunicationService::SetStationAcceptPolicy(u8 policy) {
        return 0;
    }

    Result ICommunicationService::SetWirelessControllerRestriction() {
        return 0;
    }

    Result ICommunicationService::ScanPrivate() {
        return 0;
    }

    Result ICommunicationService::CreateNetworkPrivate() {
        return 0;
    }

    Result ICommunicationService::Reject() {
        return 0;
    }

    Result ICommunicationService::AddAcceptFilterEntry() {
        return 0;
    }

    Result ICommunicationService::ClearAcceptFilter() {
        return 0;
    }

    Result ICommunicationService::ConnectPrivate() {
        return 0;
    }
}
