#include "ldn_icommunication.hpp"

static_assert(sizeof(NetworkInfo) == 0x480, "sizeof(NetworkInfo) should be 0x480");
static_assert(sizeof(ConnectNetworkData) == 0x7C, "sizeof(ConnectNetworkData) should be 0x7C");

// https://reswitched.github.io/SwIPC/ifaces.html#nn::ldn::detail::IUserLocalCommunicationService

Result ICommunicationInterface::Initialize(u64 unk, PidDescriptor pid) {
    Result rc = 0;

    char buf[128];
    sprintf(buf, "ICommunicationInterface::Initialize unk: %" PRIu64 " pid: %" PRIu64 "\n", unk, pid.pid);
    LogStr(buf);

    if (this->state_event == nullptr) {
        this->state_event = CreateWriteOnlySystemEvent();
    }

    rc = lanDiscovery.initialize([&](){
        this->onNodeChanged();
    });
    if (R_FAILED(rc)) {
        return rc;
    }

    this->set_state(CommState::Initialized);

    return rc;
}

Result ICommunicationInterface::Finalize() {
    return 0;
}

Result ICommunicationInterface::OpenAccessPoint() {
    Result rc = 0;

    this->set_state(CommState::AccessPoint);

    return rc;
}

Result ICommunicationInterface::CloseAccessPoint() {
    Result rc = 0;

    this->set_state(CommState::Initialized);

    return rc;
}

Result ICommunicationInterface::DestroyNetwork() {
    Result rc = lanDiscovery.destroyNetwork();

    if (R_SUCCEEDED(rc)) {
        this->set_state(CommState::AccessPoint);
    }

    return rc;
}

Result ICommunicationInterface::OpenStation() {
    Result rc = 0;

    this->set_state(CommState::Station);

    return rc;
}

Result ICommunicationInterface::CloseStation() {
    Result rc = 0;

    this->set_state(CommState::Initialized);

    return rc;
}

Result ICommunicationInterface::Disconnect() {
    Result rc = lanDiscovery.disconnect();

    if (R_SUCCEEDED(rc)) {
        this->set_state(CommState::Station);
    }

    return rc;
}

Result ICommunicationInterface::CreateNetwork(CreateNetworkConfig data) {
    Result rc = 0;

    LogHex(&data, 0x94);

    rc = lanDiscovery.createNetwork(&data.securityConfig, &data.userConfig, &data.networkConfig);
    if (R_SUCCEEDED(rc)) {
        this->set_state(CommState::AccessPointCreated);
    }

    return rc;
}

Result ICommunicationInterface::SetAdvertiseData(InPointer<u8> data1, InBuffer<u8> data2) {
    Result rc = 0;

    char buf[256];
    sprintf(buf, "ICommunicationInterface::SetAdvertiseData length data1: %" PRIu64 " data2: %" PRIu64 "\n", data1.num_elements, data2.num_elements);
    LogStr(buf);
    sprintf(buf, "data1: %p data2: %p\n", data1.pointer, data2.buffer);
    LogStr(buf);

    rc = lanDiscovery.setAdvertiseData(data1.pointer, data1.num_elements);

    return rc;
}

Result ICommunicationInterface::SetStationAcceptPolicy(u8 policy) {
    return 0;
}

Result ICommunicationInterface::GetState(Out<u32> state) {
    Result rc = 0;

    state.SetValue(static_cast<u32>(this->state));

    return rc;
}

Result ICommunicationInterface::GetIpv4Address(Out<u32> address, Out<u32> netmask) {
    Result rc = ipinfoGetIpConfig(address.GetPointer(), netmask.GetPointer());
    char buf[64];

    sprintf(buf, "get_ipv4_address %x %x\n", address.GetValue(), netmask.GetValue());
    LogStr(buf);

    return rc;
}

Result ICommunicationInterface::GetNetworkInfo(OutPointerWithServerSize<NetworkInfo, 1> buffer) {
    Result rc = 0;

    char buf[128];
    sprintf(buf, "get_network_info %p %" PRIu64 " state: %d\n", buffer.pointer, buffer.num_elements, static_cast<u32>(this->state));
    LogStr(buf);

    if (this->state == CommState::AccessPointCreated || this->state == CommState::StationConnected) {
        // NetworkInfo info;
        // rc = lanDiscovery.getNetworkInfo(&info);
        // std::memcpy(buffer.pointer, &info, sizeof(info));
        rc = lanDiscovery.getNetworkInfo(buffer.pointer);
    } else {
        rc = 0x40CB; // ResultConnectionFailed
    }

    return rc;
}

Result ICommunicationInterface::GetDisconnectReason(Out<u32> reason) {
    reason.SetValue(0);

    return 0;
}

Result ICommunicationInterface::GetNetworkInfoLatestUpdate(OutPointerWithServerSize<NetworkInfo, 1> buffer1, OutPointerWithServerSize<NodeLatestUpdate, 1> buffer2) {
    Result rc = 0;

    char buf[128];
    sprintf(buf, "get_network_info_latest_update1 %p %" PRIu64 "\n", buffer1.pointer, buffer1.num_elements);
    LogStr(buf);
    sprintf(buf, "get_network_info_latest_update2 %p %" PRIu64 "\n", buffer2.pointer, buffer2.num_elements);
    LogStr(buf);

    NodeLatestUpdate update = {0};
    update.stateChange = 0; // None

    if (this->state == CommState::AccessPointCreated || this->state == CommState::StationConnected) {
        rc = lanDiscovery.getNetworkInfo(buffer1.pointer);
        memcpy(buffer2.pointer, &update, sizeof(update));
    } else {
        rc = 0x40CB; // ResultConnectionFailed
    }

    return rc;
}

Result ICommunicationInterface::GetSecurityParameter(Out<SecurityParameter> out) {
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

Result ICommunicationInterface::GetNetworkConfig(Out<NetworkConfig> out) {
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

Result ICommunicationInterface::AttachStateChangeEvent(Out<CopiedHandle> handle) {
    handle.SetValue(this->state_event->GetHandle());
    return 0;
}

Result ICommunicationInterface::Scan(Out<u32> outCount, OutBuffer<NetworkInfo> buffer, OutPointerWithServerSize<u8, 0> _) {
    Result rc = 0;
    u16 count = buffer.num_elements;

    rc = lanDiscovery.scan(buffer.buffer, &count);
    outCount.SetValue(count);

    char buf[128];
    sprintf(buf, "scan %d %d\n", count, rc);
    LogStr(buf);

    return rc;
}

Result ICommunicationInterface::Connect(ConnectNetworkData param, InPointer<NetworkInfo> data) {
    char buf[64];
    sprintf(buf, "ICommunicationInterface::connect %" PRIu64 "\n", data.num_elements);
    LogStr(buf);
    LogHex(data.pointer, sizeof(NetworkInfo));
    LogHex(&param, sizeof(param));

    Result rc = lanDiscovery.connect(data.pointer, &param.userConfig, param.localCommunicationVersion);

    if (R_SUCCEEDED(rc)) {
        this->set_state(CommState::StationConnected);
    }

    return rc;
}

void ICommunicationInterface::onNodeChanged() {
    if (this->state_event) {
        LogStr("onNodeChanged signal_event\n");
        this->state_event->Signal();
    }
    NetworkInfo info;
    Result rc = lanDiscovery.getNetworkInfo(&info);
    if (R_SUCCEEDED(rc)) {
        LogStr("networkinfo:\n");
        LogHex(&info, sizeof(info));
    } else {
        LogStr("Networkinfo failed\n");
    }
}