#include "fake_lan_discovery.h"

namespace ams::mitm::ldn {

FakeLanState &FakeLan() {
    static FakeLanState s_state;
    return s_state;
}

namespace {

class FakeLanDiscovery final : public ILanDiscovery {
    public:
        Result initialize(LanEventFunc lanEvent, bool listening) override {
            (void)listening;
            (void)lanEvent;
            ++FakeLan().initializeCalls;
            this->m_state = FakeLan().state;
            return 0;
        }

        Result finalize() override {
            ++FakeLan().finalizeCalls;
            return 0;
        }

        Result initNetworkInfo() override {
            return 0;
        }

        Result scan(NetworkInfo *networkInfo, u16 *count, ScanFilter filter) override {
            (void)filter;
            ++FakeLan().scanCalls;

            const u16 maximum = *count;
            u16 written = 0;
            while (written < maximum && written < static_cast<u16>(FakeLan().scanCount)) {
                networkInfo[written] = FakeLan().scanResults[written];
                ++written;
            }
            *count = written;
            return 0;
        }

        Result setAdvertiseData(const u8 *data, uint16_t size) override {
            if (size > sizeof(FakeLan().advertiseData)) {
                return 1;
            }
            std::memcpy(FakeLan().advertiseData, data, size);
            FakeLan().advertiseSize = size;
            return 0;
        }

        Result createNetwork(const SecurityConfig *, const UserConfig *, const NetworkConfig *) override {
            ++FakeLan().createNetworkCalls;
            return 0;
        }

        Result destroyNetwork() override {
            ++FakeLan().destroyNetworkCalls;
            return 0;
        }

        Result connect(const NetworkInfo *networkInfo, UserConfig *userConfig, u16 localCommunicationVersion) override {
            ++FakeLan().connectCalls;
            if (networkInfo != nullptr) {
                FakeLan().lastConnectTarget = *networkInfo;
            }
            if (userConfig != nullptr) {
                FakeLan().lastConnectUser = *userConfig;
            }
            FakeLan().lastConnectVersion = localCommunicationVersion;
            return 0;
        }

        Result disconnect() override {
            ++FakeLan().disconnectCalls;
            return 0;
        }

        Result getNetworkInfo(NetworkInfo *pOutNetwork) override {
            ++FakeLan().getNetworkInfoCalls;
            *pOutNetwork = FakeLan().networkInfo;
            return 0;
        }

        Result getNetworkInfo(NetworkInfo *pOutNetwork, NodeLatestUpdate *pOutUpdates, int bufferCount) override {
            ++FakeLan().getNetworkInfoCalls;
            *pOutNetwork = FakeLan().networkInfo;

            const int written = FakeLan().updatesWritten < bufferCount ? FakeLan().updatesWritten : bufferCount;
            for (int i = 0; i < written; ++i) {
                pOutUpdates[i].stateChange = static_cast<u8>(i);
                std::memset(pOutUpdates[i]._unk, 0, sizeof(pOutUpdates[i]._unk));
            }
            return 0;
        }

        Result openAccessPoint() override {
            ++FakeLan().openAccessPointCalls;
            this->m_state = CommState::AccessPoint;
            FakeLan().state = this->m_state;
            return 0;
        }

        Result closeAccessPoint() override {
            ++FakeLan().closeAccessPointCalls;
            this->m_state = CommState::Initialized;
            FakeLan().state = this->m_state;
            return 0;
        }

        Result openStation() override {
            ++FakeLan().openStationCalls;
            this->m_state = CommState::Station;
            FakeLan().state = this->m_state;
            return 0;
        }

        Result closeStation() override {
            ++FakeLan().closeStationCalls;
            this->m_state = CommState::Initialized;
            FakeLan().state = this->m_state;
            return 0;
        }

        CommState getState() const override { return this->m_state; }
        DisconnectReason getDisconnectReason() const override { return FakeLan().disconnect_reason; }
        u16 getListenPort() const override { return DefaultPort; }
        int stationCount() override { return 0; }

    private:
        CommState m_state = CommState::None;
};

} // namespace

ILanDiscovery &FakeLanImpl() {
    static FakeLanDiscovery s_instance;
    return s_instance;
}

} // namespace ams::mitm::ldn
