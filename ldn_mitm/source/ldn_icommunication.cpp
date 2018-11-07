#include "ldn_icommunication.hpp"

static_assert(sizeof(NetworkInfo) == 0x480, "sizeof(NetworkInfo) should be 0x480");
static_assert(sizeof(ConnectNetworkData) == 0x7C, "sizeof(ConnectNetworkData) should be 0x7C");

static const int ModuleID = 0xFE;
static const int DiscoveryPort = 11452;
static const int BufferSize = 2048;
static const int ScanResultCountMax = 24;
static const u32 LANMagic = 0x114514;

// https://reswitched.github.io/SwIPC/ifaces.html#nn::ldn::detail::IUserLocalCommunicationService

const char *ICommunicationInterface::FakeSsid = "12345678123456781234567812345678";
Service ICommunicationInterface::nifmSrv = {0};
Service ICommunicationInterface::nifmIGS = {0};
u64 ICommunicationInterface::nifmRefCount = 0;

Result ICommunicationInterface::nifmInit() {
    atomicIncrement64(&nifmRefCount);
    if (serviceIsActive(&nifmSrv))
        return 0;

    Result rc = smGetService(&nifmSrv, "nifm:u");
    if (R_FAILED(rc)) {
        rc = MAKERESULT(ModuleID, 5);
        goto quit;
    }

    IpcCommand c;
    IpcParsedCommand r;

    ipcInitialize(&c);
    ipcSendPid(&c);
    struct {
        u64 magic;
        u64 cmd_id;
        u64 param;
    } *raw;

    raw = (decltype(raw))serviceIpcPrepareHeader(&nifmSrv, &c, sizeof(*raw));

    raw->magic = SFCI_MAGIC;
    raw->cmd_id = 5;
    raw->param = 0;

    rc = serviceIpcDispatch(&nifmSrv);

    if (R_FAILED(rc)) {
        rc = MAKERESULT(ModuleID, 6);
        goto quit;
    }
    struct {
        u64 magic;
        u64 result;
    } *resp;

    serviceIpcParse(&nifmSrv, &r, sizeof(*resp));
    resp = (decltype(resp))r.Raw;

    rc = resp->result;

    if (R_FAILED(rc)) {
        rc = MAKERESULT(ModuleID, 7);
        goto quit;
    }

    serviceCreateSubservice(&nifmIGS, &nifmSrv, &r, 0);

quit:
    return rc;
}

void ICommunicationInterface::nifmFinal() {
    if (atomicDecrement64(&nifmRefCount) == 0) {
        serviceClose(&nifmIGS);
        serviceClose(&nifmSrv);
    }
}

Result ICommunicationInterface::nifmGetIpConfig(u32* address) {
    u32 netmask;
    return nifmGetIpConfig(address, &netmask);
}

Result ICommunicationInterface::nifmGetIpConfig(u32* address, u32* netmask) {
    Result rc;
    IpcCommand c;
    IpcParsedCommand r;

    ipcInitialize(&c);
    struct {
        u64 magic;
        u64 cmd_id;
    } *raw;

    raw = (decltype(raw))serviceIpcPrepareHeader(&nifmIGS, &c, sizeof(*raw));

    raw->magic = SFCI_MAGIC;
    raw->cmd_id = 15; // GetCurrentIpConfigInfo

    rc = serviceIpcDispatch(&nifmIGS);
    if (R_FAILED(rc)) {
        return rc;
    }
    struct {
        u64 magic;
        u64 result;
        u8 _unk;
        u32 address;
        u32 netmask;
        u32 gateway;
    } __attribute__((packed)) *resp;

    serviceIpcParse(&nifmIGS, &r, sizeof(*resp));
    resp = (decltype(resp))r.Raw;

    rc = resp->result;
    if (R_FAILED(rc)) {
        return rc;
    }
    *address = resp->address;
    *netmask = resp->netmask;
    // ret = resp->address | ~resp->netmask;

    return rc;
}

Result ICommunicationInterface::get_fake_mac(u8 mac[6]) {
    mac[0] = 0x02;
    mac[1] = 0x00;

    u32 ip;
    Result rc = nifmGetIpConfig(&ip);
    if (R_SUCCEEDED(rc)) {
        memcpy(mac + 2, &ip, sizeof(ip));
    }

    return rc;
}

Result ICommunicationInterface::Initialize(u64 unk, PidDescriptor pid) {
    Result rc = 0;

    char buf[128];
    sprintf(buf, "ICommunicationInterface::initialize unk: %" PRIu64 " pid: %" PRIu64 "\n", unk, pid.pid);
    LogStr(buf);

    rc = nifmInit();

    if (R_SUCCEEDED(rc)) {
        this->set_state(CommState::Initialized);
        if (this->state_event == nullptr) {
            this->state_event = CreateWriteOnlySystemEvent();
        }
    }

    return rc;
}

Result ICommunicationInterface::Finalize() {
    nifmFinal();
    return 0;
}

Result ICommunicationInterface::OpenAccessPoint() {
    Result rc = 0;

    this->init_network_info();
    this->set_state(CommState::AccessPoint);

    return rc;
}

Result ICommunicationInterface::CloseAccessPoint() {
    Result rc = 0;

    this->set_state(CommState::Initialized);

    return rc;
}

Result ICommunicationInterface::DestroyNetwork() {
    Result rc = 0;

    this->set_state(CommState::AccessPoint);

    return rc;
}

Result ICommunicationInterface::OpenStation() {
    Result rc = 0;

    this->init_network_info();
    this->set_state(CommState::Station);

    return rc;
}

Result ICommunicationInterface::CloseStation() {
    Result rc = 0;

    this->set_state(CommState::Initialized);

    return rc;
}

Result ICommunicationInterface::Disconnect() {
    Result rc = 0;

    this->set_state(CommState::Station);

    return rc;
}

Result ICommunicationInterface::CreateNetwork(CreateNetworkConfig data) {
    Result rc = 0;

    LogHex(&data, 0x94);

    u32 address;
    rc = nifmGetIpConfig(&address);
    if (R_FAILED(rc)) {
        return rc;
    }

    this->network_info.ldn.nodeCountMax = data.networkConfig.nodeCountMax;
    this->network_info.ldn.securityMode = data.securityConfig.securityMode;
    if (data.networkConfig.channel == 0) {
        this->network_info.common.channel = 6;
    } else {
        this->network_info.common.channel = data.networkConfig.channel;
    }
    this->network_info.networkId.intentId = data.networkConfig.intentId;
    this->network_info.ldn.nodeCount = 1;
    NodeInfo *nodes = this->network_info.ldn.nodes;
    nodes[0].isConnected = 1;
    strcpy(nodes[0].userName, data.userConfig.userName);
    nodes[0].localCommunicationVersion = data.networkConfig.localCommunicationVersion;

    nodes[0].ipv4Address = address;
    rc = get_fake_mac(nodes[0].macAddress);
    if (R_FAILED(rc)) {
        return rc;
    }

    this->set_state(CommState::AccessPointCreated);

    return rc;
}

Result ICommunicationInterface::SetAdvertiseData(InPointer<u8> data1, InBuffer<u8> data2) {
    Result rc = 0;

    char buf[128];
    sprintf(buf, "ICommunicationInterface::set_advertise_data length data1: %" PRIu64 " data2: %" PRIu64 "\n", data1.num_elements, data2.num_elements);
    LogStr(buf);
    sprintf(buf, "data1: %p data2: %p\n", data1.pointer, data2.buffer);
    LogStr(buf);

    this->network_info.ldn.advertiseDataSize = data1.num_elements;
    memcpy(&this->network_info.ldn.advertiseData, data1.pointer, data1.num_elements);

    return rc;
}

Result ICommunicationInterface::GetState(Out<u32> state) {
    Result rc = 0;

    state.SetValue(static_cast<u32>(this->state));

    return rc;
}

Result ICommunicationInterface::GetIpv4Address(Out<u32> address, Out<u32> netmask) {
    Result rc = nifmGetIpConfig(address.GetPointer(), netmask.GetPointer());
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
        memcpy(buffer.pointer, &this->network_info, sizeof(NetworkInfo));
    } else {
        rc = 0x40CB; // ResultConnectionFailed
    }

    return rc;
}

Result ICommunicationInterface::GetDisconnectReason(Out<u16> reason) {
    Result rc = 0;

    this->state_event->Signal();

    reason.SetValue(0);

    return rc;
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
        memcpy(buffer1.pointer, &this->network_info, sizeof(NetworkInfo));
        memcpy(buffer2.pointer, &update, sizeof(update));
    } else {
        rc = 0x40CB; // ResultConnectionFailed
    }

    return rc;
}

Result ICommunicationInterface::GetSecurityParameter(Out<SecurityParameter> out) {
    Result rc = 0;

    SecurityParameter data;
    NetworkInfo2SecurityParameter(&this->network_info, &data);
    out.SetValue(data);

    return rc;
}

Result ICommunicationInterface::GetNetworkConfig(Out<NetworkConfig> out) {
    Result rc = 0;

    NetworkConfig data;
    NetworkInfo2NetworkConfig(&this->network_info, &data);
    out.SetValue(data);

    return rc;
}

Result ICommunicationInterface::AttachStateChangeEvent(Out<CopiedHandle> handle) {
    handle.SetValue(this->state_event->GetHandle());
    return 0;
}

Result ICommunicationInterface::Scan(Out<u16> count, OutPointerWithServerSize<u8, 0> buffer, OutBuffer<NetworkInfo> data, u16 bufferCount) {
    bufferCount = 8;
    // LANDiscovery::scan(buffer.buffer, count.GetPointer(), bufferCount);

    char buf[128];
    sprintf(buf, "scan %d\n", bufferCount);
    LogStr(buf);

    return 0;
}

Result ICommunicationInterface::Connect(ConnectNetworkData dat, InPointer<u8> data) {
    char buf[64];
    sprintf(buf, "ICommunicationInterface::connect %" PRIu64 "\n", data.num_elements);
    LogStr(buf);
    LogHex(data.pointer, sizeof(NetworkInfo));
    LogHex(&dat, sizeof(dat));

    u32 address;
    Result rc = nifmGetIpConfig(&address);
    if (R_FAILED(rc)) {
        return rc;
    }

    memcpy(&this->network_info, data.pointer, sizeof(NetworkInfo));

    this->network_info.ldn.nodeCount++;
    NodeInfo *nodes = this->network_info.ldn.nodes;
    nodes[1].isConnected = 1;
    strcpy(nodes[1].userName, dat.userConfig.userName);
    nodes[1].localCommunicationVersion = nodes[0].localCommunicationVersion;

    nodes[1].ipv4Address = address;
    rc = get_fake_mac(nodes[1].macAddress);
    if (R_FAILED(rc)) {
        return rc;
    }

    this->set_state(CommState::StationConnected);

    return 0;
}
