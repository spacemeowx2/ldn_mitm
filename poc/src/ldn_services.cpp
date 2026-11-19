#include "ldn_services.hpp"

#include <cstring>

#include "debug.hpp"

namespace ams::mitm::ldn {

namespace {

constexpr u32 Cmd_Communication_GetState                   = 0;
constexpr u32 Cmd_Communication_GetNetworkInfo             = 1;
constexpr u32 Cmd_Communication_GetIpv4Address             = 2;
constexpr u32 Cmd_Communication_GetDisconnectReason        = 3;
constexpr u32 Cmd_Communication_GetSecurityParameter       = 4;
constexpr u32 Cmd_Communication_GetNetworkConfig           = 5;
constexpr u32 Cmd_Communication_AttachStateChangeEvent     = 100;
constexpr u32 Cmd_Communication_GetNetworkInfoLatestUpdate = 101;
constexpr u32 Cmd_Communication_Scan                       = 102;
constexpr u32 Cmd_Communication_ScanPrivate                = 103;
constexpr u32 Cmd_Communication_SetWirelessRestriction     = 104;
constexpr u32 Cmd_Communication_OpenAccessPoint            = 200;
constexpr u32 Cmd_Communication_CloseAccessPoint           = 201;
constexpr u32 Cmd_Communication_CreateNetwork              = 202;
constexpr u32 Cmd_Communication_CreateNetworkPrivate       = 203;
constexpr u32 Cmd_Communication_DestroyNetwork             = 204;
constexpr u32 Cmd_Communication_Reject                     = 205;
constexpr u32 Cmd_Communication_SetAdvertiseData           = 206;
constexpr u32 Cmd_Communication_SetStationAcceptPolicy     = 207;
constexpr u32 Cmd_Communication_AddAcceptFilterEntry       = 208;
constexpr u32 Cmd_Communication_ClearAcceptFilter          = 209;
constexpr u32 Cmd_Communication_OpenStation                = 300;
constexpr u32 Cmd_Communication_CloseStation               = 301;
constexpr u32 Cmd_Communication_Connect                    = 302;
constexpr u32 Cmd_Communication_ConnectPrivate             = 303;
constexpr u32 Cmd_Communication_Disconnect                 = 304;
constexpr u32 Cmd_Communication_Initialize                 = 400;
constexpr u32 Cmd_Communication_Finalize                   = 401;
constexpr u32 Cmd_Communication_InitializeSystem2          = 402;

constexpr u32 Cmd_Mitm_CreateUserLocalCommunicationService = 0;
constexpr u32 Cmd_Mitm_CreateClientProcessMonitor          = 1;
constexpr u32 Cmd_Mitm_CreateLdnMitmConfigService          = 65000;

constexpr u32 Cmd_Config_GetVersion                        = 65001;
constexpr u32 Cmd_Config_GetLogging                        = 65002;
constexpr u32 Cmd_Config_SetLogging                        = 65003;
constexpr u32 Cmd_Config_GetEnabled                        = 65004;
constexpr u32 Cmd_Config_SetEnabled                        = 65005;

constexpr u32 Cmd_ClientProcessMonitor_RegisterClient      = 0;

/* Host-endian number out of network byte order, without pulling <arpa/inet.h>
 * into the host build. */
constexpr u32 ByteSwap32(u32 value) {
    return ((value & 0x000000FFu) << 24) |
           ((value & 0x0000FF00u) << 8)  |
           ((value & 0x00FF0000u) >> 8)  |
           ((value & 0xFF000000u) >> 24);
}

/* ClientProcessId is a u64 on the wire. */
u64 ReadClientProcessId(const ldn_nx::Request &req) {
    if (!req.InSizeOk(sizeof(u64))) {
        return 0;
    }
    return *req.In<u64>();
}

} // namespace

/* ------------------------------------------------------------------------- */
/* ICommunicationService                                                     */
/* ------------------------------------------------------------------------- */
ICommunicationService::ICommunicationService(ILanDiscovery &lan) : m_lan(lan) {
    std::memset(&this->stateEvent, 0, sizeof(this->stateEvent));
    LogFormat("ICommunicationService");
}

ICommunicationService::~ICommunicationService() {
    LogFormat("~ICommunicationService");
    if (this->stateEventCreated) {
        eventClose(&this->stateEvent);
        this->stateEventCreated = false;
    }
    (void)this->m_lan.finalize();
}

Result ICommunicationService::InitializeService() {
    if (!this->stateEventCreated) {
        /* autoclear, inter process: the client waits on the readable handle. */
        if (R_FAILED(eventCreate(&this->stateEvent, true))) {
            return 1;
        }
        this->stateEventCreated = true;
    }

    return this->m_lan.initialize([this]() { this->OnEventFired(); });
}

Result ICommunicationService::FinalizeService() {
    Result rc = this->m_lan.finalize();
    if (this->stateEventCreated) {
        eventClose(&this->stateEvent);
        this->stateEventCreated = false;
    }
    return rc;
}

void ICommunicationService::OnEventFired() {
    if (this->stateEventCreated) {
        LogFormat("onEventFired signal_event");
        eventFire(&this->stateEvent);
    }
}

Result ICommunicationService::Invoke(ldn_nx::Request &req, ldn_nx::Reply &reply) {
    switch (req.cmd_id) {
        case Cmd_Communication_Initialize: {
            LogFormat("ICommunicationService::Initialize pid: %llu", static_cast<unsigned long long>(ReadClientProcessId(req)));
            return this->InitializeService();
        }
        case Cmd_Communication_InitializeSystem2: {
            if (!req.InSizeOk(sizeof(u64) * 2)) {
                return 1;
            }
            const u64 unk = req.In<u64>()[0];
            LogFormat("ICommunicationService::InitializeSystem2 unk: %llu", static_cast<unsigned long long>(unk));
            this->errorState = unk;
            return this->InitializeService();
        }
        case Cmd_Communication_Finalize:
            return this->FinalizeService();

        case Cmd_Communication_OpenAccessPoint:
            return this->m_lan.openAccessPoint();
        case Cmd_Communication_CloseAccessPoint:
            return this->m_lan.closeAccessPoint();
        case Cmd_Communication_DestroyNetwork:
            return this->m_lan.destroyNetwork();
        case Cmd_Communication_OpenStation:
            return this->m_lan.openStation();
        case Cmd_Communication_CloseStation:
            return this->m_lan.closeStation();
        case Cmd_Communication_Disconnect:
            return this->m_lan.disconnect();

        case Cmd_Communication_CreateNetwork: {
            if (!req.InSizeOk(sizeof(CreateNetworkConfig))) {
                return 1;
            }
            const auto *config = req.In<CreateNetworkConfig>();
            return this->m_lan.createNetwork(&config->securityConfig, &config->userConfig, &config->networkConfig);
        }

        case Cmd_Communication_SetAdvertiseData: {
            size_t size = 0;
            const u8 *data = req.InAutoSelect(&size);
            if (data == nullptr) {
                return 1;
            }
            return this->m_lan.setAdvertiseData(data, static_cast<u16>(size));
        }

        case Cmd_Communication_GetState: {
            auto *state = reply.Out<u32>();
            if (state == nullptr) {
                return 1;
            }
            *state = static_cast<u32>(this->m_lan.getState());

            if (this->errorState != 0 && this->m_lan.getDisconnectReason() != DisconnectReason::None) {
                return MAKERESULT(0x10, static_cast<u32>(this->m_lan.getDisconnectReason()));
            }
            return 0;
        }

        case Cmd_Communication_GetIpv4Address: {
            u32 address = 0, netmask = 0, gateway = 0, primaryDns = 0, secondaryDns = 0;
            const Result rc = nifmGetCurrentIpConfigInfo(&address, &netmask, &gateway, &primaryDns, &secondaryDns);

            auto *outAddress = reply.Out<u32>();
            auto *outMask = reply.Out<u32>();
            if (outAddress == nullptr || outMask == nullptr) {
                return 1;
            }
            *outAddress = ByteSwap32(address);
            *outMask = ByteSwap32(netmask);
            LogFormat("get_ipv4_address %x %x", *outAddress, *outMask);
            return rc;
        }

        case Cmd_Communication_GetNetworkInfo: {
            /* NetworkInfo is a LargeData type: the client passes a map-alias out
             * buffer. Fall back to inline data if a client sent it that way. */
            size_t capacity = 0;
            u8 *buffer = req.OutBuffer(0, &capacity);
            NetworkInfo *out = nullptr;
            if (buffer != nullptr && capacity >= sizeof(NetworkInfo)) {
                out = reinterpret_cast<NetworkInfo *>(buffer);
            } else {
                out = reply.Out<NetworkInfo>();
            }
            if (out == nullptr) {
                return 1;
            }
            LogFormat("get_network_info %p state: %d", out, static_cast<u32>(this->m_lan.getState()));
            return this->m_lan.getNetworkInfo(out);
        }

        case Cmd_Communication_GetDisconnectReason: {
            auto *reason = reply.Out<u32>();
            if (reason == nullptr) {
                return 1;
            }
            *reason = static_cast<u32>(this->m_lan.getDisconnectReason());
            LogFormat("GetDisconnectReason state: %d reason: %u", static_cast<u32>(this->m_lan.getState()), *reason);
            return 0;
        }

        case Cmd_Communication_GetNetworkInfoLatestUpdate: {
            size_t bufferCapacity = 0;
            u8 *buffer = req.OutBuffer(0, &bufferCapacity);
            NetworkInfo *info = (buffer != nullptr && bufferCapacity >= sizeof(NetworkInfo))
                ? reinterpret_cast<NetworkInfo *>(buffer)
                : reply.Out<NetworkInfo>();
            if (info == nullptr) {
                return 1;
            }

            size_t updatesCapacity = 0;
            u8 *updates = req.OutPointer(0, &updatesCapacity);
            if (updates == nullptr) {
                updates = req.OutBuffer(1, &updatesCapacity);
            }
            const int count = static_cast<int>(updatesCapacity / sizeof(NodeLatestUpdate));
            LogFormat("get_network_info_latest buffer %p updates %p count %d", info, updates, count);

            const Result rc = this->m_lan.getNetworkInfo(info, reinterpret_cast<NodeLatestUpdate *>(updates), count);
            if (R_SUCCEEDED(rc) && updates != nullptr) {
                reply.AddOutPointer(updates, static_cast<size_t>(count) * sizeof(NodeLatestUpdate));
            }
            return rc;
        }

        case Cmd_Communication_GetSecurityParameter: {
            auto *out = reply.Out<SecurityParameter>();
            if (out == nullptr) {
                return 1;
            }
            NetworkInfo info{};
            const Result rc = this->m_lan.getNetworkInfo(&info);
            if (R_SUCCEEDED(rc)) {
                NetworkInfo2SecurityParameter(&info, out);
            }
            return rc;
        }

        case Cmd_Communication_GetNetworkConfig: {
            auto *out = reply.Out<NetworkConfig>();
            if (out == nullptr) {
                return 1;
            }
            NetworkInfo info{};
            const Result rc = this->m_lan.getNetworkInfo(&info);
            if (R_SUCCEEDED(rc)) {
                NetworkInfo2NetworkConfig(&info, out);
            }
            return rc;
        }

        case Cmd_Communication_AttachStateChangeEvent: {
            if (!this->stateEventCreated) {
                return 1;
            }
            /* Copy handle: the client keeps its own copy, we keep ours. */
            reply.AddCopyHandle(this->stateEvent.revent);
            return 0;
        }

        case Cmd_Communication_Scan: {
            /* libstratosphere aligns each in-argument to its natural alignment,
             * so `filter` (align 8) starts at AlignUp(2, 8) = 8, not at 2. */
            constexpr size_t FilterOffset = (sizeof(u16) + alignof(ScanFilter) - 1) & ~(alignof(ScanFilter) - 1);
            if (!req.InSizeOk(FilterOffset + sizeof(ScanFilter))) {
                return 1;
            }
            auto *count = reply.Out<u32>();
            if (count == nullptr) {
                return 1;
            }

            const u8 *payload = req.in_data;
            const u16 channel = *reinterpret_cast<const u16 *>(payload);
            (void)channel; /* unused by the original implementation too */
            const auto *filter = reinterpret_cast<const ScanFilter *>(payload + FilterOffset);

            size_t capacity = 0;
            u8 *buffer = req.OutAutoSelect(&capacity);
            u16 scanned = static_cast<u16>(capacity / sizeof(NetworkInfo));

            const Result rc = this->m_lan.scan(reinterpret_cast<NetworkInfo *>(buffer), &scanned, *filter);
            *count = scanned;

            if (buffer != nullptr) {
                /* Auto select buffers carry their written size in the table. */
                reply.AddOutPointer(buffer, static_cast<size_t>(scanned) * sizeof(NetworkInfo));
            }
            LogFormat("scan %d %d", scanned, rc);
            return rc;
        }

        case Cmd_Communication_Connect: {
            if (!req.InSizeOk(sizeof(ConnectNetworkData))) {
                return 1;
            }
            size_t size = 0;
            const u8 *buffer = req.InBuffer(0, &size);
            const auto *data = (buffer != nullptr && size >= sizeof(NetworkInfo))
                ? reinterpret_cast<const NetworkInfo *>(buffer)
                : nullptr;
            if (data == nullptr) {
                return 1;
            }
            const auto *param = req.In<ConnectNetworkData>();
            LogFormat("ICommunicationService::connect");
            LogHex(data, sizeof(NetworkInfo));
            LogHex(param, sizeof(*param));
            return this->m_lan.connect(data, const_cast<UserConfig *>(&param->userConfig), static_cast<u16>(param->localCommunicationVersion));
        }

        /* Not implemented in the original either: report success. */
        case Cmd_Communication_ScanPrivate:
        case Cmd_Communication_SetWirelessRestriction:
        case Cmd_Communication_CreateNetworkPrivate:
        case Cmd_Communication_Reject:
        case Cmd_Communication_SetStationAcceptPolicy:
        case Cmd_Communication_AddAcceptFilterEntry:
        case Cmd_Communication_ClearAcceptFilter:
        case Cmd_Communication_ConnectPrivate:
            return 0;

        default:
            return 0x202; /* unknown command */
    }
}

/* ------------------------------------------------------------------------- */
/* IClientProcessMonitor                                                     */
/* ------------------------------------------------------------------------- */
Result IClientProcessMonitor::Invoke(ldn_nx::Request &req, ldn_nx::Reply &reply) {
    (void)reply;
    switch (req.cmd_id) {
        case Cmd_ClientProcessMonitor_RegisterClient:
            /* Stub for firmware 18.0.0+ (Pokemon Legends Z-A). */
            LogFormat("IClientProcessMonitor::RegisterClient pid: %llu",
                      static_cast<unsigned long long>(ReadClientProcessId(req)));
            return 0;
        default:
            return 0x202;
    }
}

/* ------------------------------------------------------------------------- */
/* ILdnConfigService                                                         */
/* ------------------------------------------------------------------------- */
bool ILdnConfigService::s_enabled = true;

bool ILdnConfigService::IsEnabled() {
    return s_enabled;
}

void ILdnConfigService::SetEnabled(bool enabled) {
    s_enabled = enabled;
}

Result ILdnConfigService::Invoke(ldn_nx::Request &req, ldn_nx::Reply &reply) {
    switch (req.cmd_id) {
        case Cmd_Config_GetVersion: {
            auto *version = reply.Out<LdnMitmVersion>();
            if (version == nullptr) {
                return 1;
            }
            std::memset(version->raw, 0, sizeof(version->raw));
            std::strncpy(version->raw, "1.25.1-libnx-poc", sizeof(version->raw) - 1);
            return 0;
        }
        case Cmd_Config_GetLogging: {
            auto *enabled = reply.Out<u32>();
            if (enabled == nullptr) {
                return 1;
            }
            return GetLogging(enabled);
        }
        case Cmd_Config_SetLogging: {
            if (!req.InSizeOk(sizeof(u32))) {
                return 1;
            }
            return SetLogging(*req.In<u32>());
        }
        case Cmd_Config_GetEnabled: {
            auto *enabled = reply.Out<u32>();
            if (enabled == nullptr) {
                return 1;
            }
            *enabled = s_enabled ? 1 : 0;
            return 0;
        }
        case Cmd_Config_SetEnabled: {
            if (!req.InSizeOk(sizeof(u32))) {
                return 1;
            }
            s_enabled = (*req.In<u32>() != 0);
            return 0;
        }
        default:
            return 0x202;
    }
}

/* ------------------------------------------------------------------------- */
/* ILdnMitMService                                                           */
/* ------------------------------------------------------------------------- */
ILdnMitMService::ILdnMitMService(Handle forward_session, ILanDiscovery &lan) : m_forwardSession(forward_session), m_lan(lan) {
    LogFormat("LdnMitMService created");
}

Result ILdnMitMService::Invoke(ldn_nx::Request &req, ldn_nx::Reply &reply) {
    switch (req.cmd_id) {
        case Cmd_Mitm_CreateUserLocalCommunicationService: {
            LogFormat("CreateUserLocalCommunicationService: enabled %d", static_cast<u32>(ILdnConfigService::IsEnabled()));

            if (ILdnConfigService::IsEnabled()) {
                reply.AddObject(new ICommunicationService(this->m_lan));
                return 0;
            }
            /* Let the server forward the request to the real ldn service. */
            return ldn_nx::ResultShouldForwardToSession;
        }
        case Cmd_Mitm_CreateClientProcessMonitor: {
            LogFormat("CreateClientProcessMonitor called");
            /* Always create the monitor, even when the mitm is disabled: firmware
             * 18.0.0+ (Pokemon Legends Z-A) requires it to exist. */
            reply.AddObject(new IClientProcessMonitor());
            return 0;
        }
        case Cmd_Mitm_CreateLdnMitmConfigService: {
            reply.AddObject(new ILdnConfigService());
            return 0;
        }
        default:
            return 0x202;
    }
}

} // namespace ams::mitm::ldn
