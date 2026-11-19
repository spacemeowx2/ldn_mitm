/*
 * End-to-end IPC tests: real libnx client code (switch/sf/service.h,
 * cmif.h, hipc.h) driving our own libnx-only server, over a simulated kernel.
 *
 * This is the "software verification" loop for the libstratosphere removal:
 * every assertion here is a wire-format assertion (the same bytes a Switch
 * game would send), not a mock-vs-mock check.
 */
#include <switch.h>
#include <switch/sf/service.h>

#include <cstdio>
#include <cstdlib>
/* MinGW's <thread> references ::nanosleep; the host shim provides it. */
extern "C" int nanosleep(const struct timespec *req, struct timespec *rem);
#include <thread>
#include <cstring>

#include "ipc_server.hpp"
#include "mitm_query_service.hpp"
#include "mock_kernel.h"
#include "mock_sm.h"
#include "sm_mitm.h"

/* Pinned Atmosphere reference client (see harness/atmosphere_ref/README.md). */
#include "sm_ams.os.horizon.h"

#include <vector>
#include "fake_lan_discovery.h"
#include "ldn_services.hpp"

namespace {

int g_failures = 0;
int g_checks   = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        ++g_checks;                                                              \
        if (!(cond)) {                                                           \
            ++g_failures;                                                        \
            std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
        }                                                                        \
    } while (0)

#define CHECK_EQ(a, b)                                                           \
    do {                                                                         \
        ++g_checks;                                                              \
        const auto _a = (a);                                                     \
        const auto _b = (b);                                                     \
        if (!(_a == _b)) {                                                       \
            ++g_failures;                                                        \
            std::printf("  FAIL %s:%d  %s == %s  (%#x != %#x)\n", __FILE__,      \
                        __LINE__, #a, #b, (unsigned)_a, (unsigned)_b);           \
        }                                                                        \
    } while (0)

/* ------------------------------------------------------------------------- */
/* Test service objects                                                      */
/* ------------------------------------------------------------------------- */
class EchoService : public ldn_nx::ServerObject {
public:
    Result Invoke(ldn_nx::Request &req, ldn_nx::Reply &reply) override {
        switch (req.cmd_id) {
            case 0: { /* echo + 1 */
                if (!req.InSizeOk(sizeof(u32))) {
                    return 1;
                }
                auto *out = reply.Out<u32>();
                *out = *req.In<u32>() + 1;
                return 0;
            }
            case 1: { /* create a child object */
                reply.AddObject(new EchoService());
                return 0;
            }
            case 2: { /* ask the mitm layer to forward */
                return ldn_nx::ResultShouldForwardToSession;
            }
            case 3: { /* return a handle we own (copy/move) */
                return 0;
            }
            case 4: { /* explicit failure */
                return 0x1234;
            }
            default:
                return 0x7777;
        }
    }
};

/* Stands in for the real ldn:u service sitting behind the mitm. */
class RealLdnService : public ldn_nx::ServerObject {
public:
    Result Invoke(ldn_nx::Request &req, ldn_nx::Reply &reply) override {
        if (req.cmd_id == 2) {
            auto *out = reply.Out<u32>();
            *out = 0xCAFE;
            return 0;
        }
        return 0xBEEF;
    }
};

/* ------------------------------------------------------------------------- */
/* Harness helpers                                                           */
/* ------------------------------------------------------------------------- */
struct Fixture {
    ldn_nx::IpcServer server;
    Handle client = 0;

    /* Creates one session pair, registers `object`, installs the kernel-side
     * dispatch trampoline. Returns the client handle. */
    Handle Connect(ldn_nx::ServerObject *object, bool is_mitm = false, Handle forward = 0, u64 pid = 0) {
        client = mock::K().CreatePair("test");
        Handle server_handle = mock::K().GetPairByClient(client)->server_handle;
        if (R_FAILED(server.RegisterSession(server_handle, object, is_mitm, forward, pid))) {
            std::printf("  FAIL could not register session\n");
            ++g_failures;
        }
        mock::K().SetDefaultHandler([this](Handle h) { return server.ProcessSession(h); });
        mock::K().SetHandler(server_handle, [this](Handle h) { return server.ProcessSession(h); });
        return client;
    }
};

void StartCase(const char *name) {
    std::printf("[case] %s\n", name);
    mock::K().Reset();
}

void EndCase() {
    /* nothing for now: fixtures have automatic storage in each case */
}

/* ------------------------------------------------------------------------- */
/* Cases                                                                     */
/* ------------------------------------------------------------------------- */

/* libnx client builds the request, our server answers it, libnx parses it. */
void TestBasicDispatch() {
    StartCase("basic cmif dispatch (libnx client -> our server)");
    {
        Fixture fx;
        const Handle client = fx.Connect(new EchoService());

        Service s{};
        serviceCreate(&s, client); /* also exercises control cmd 3 */
        CHECK(serviceIsActive(&s));
        CHECK_EQ(s.pointer_buffer_size, 0x1000);

        u32 in = 41, out = 0;
        Result rc = serviceDispatchInOut(&s, 0, in, out);
        CHECK(R_SUCCEEDED(rc));
        CHECK_EQ(out, 42u);

        /* Response header must be a valid CMIF out header. */
        const HipcResponse hp = hipcParseResponse(armGetTls());
        auto *out_header = reinterpret_cast<CmifOutHeader *>(
            cmifGetAlignedDataStart(hp.data_words, armGetTls()));
        CHECK_EQ(out_header->magic, ldn_nx::CmifOutHeaderMagic);
        CHECK_EQ(out_header->result, 0u);

        serviceClose(&s);
    }
    EndCase();
}

/* Server-side error results must reach the client unchanged. */
void TestErrorPropagation() {
    StartCase("error results propagate");
    {
        Fixture fx;
        Service s{};
        serviceCreate(&s, fx.Connect(new EchoService()));

        CHECK_EQ(serviceDispatch(&s, 4), 0x1234u);
        CHECK_EQ(serviceDispatch(&s, 99), 0x7777u);
        serviceClose(&s);
    }
    EndCase();
}

/* Out objects on a plain session are marshalled as move handles. */
void TestOutObjectAsHandle() {
    StartCase("out object -> move handle");
    {
        Fixture fx;
        Service s{};
        serviceCreate(&s, fx.Connect(new EchoService()));

        Service child{};
        const Result rc = serviceDispatch(&s, 1,
            .out_num_objects = 1,
            .out_objects     = &child);
        CHECK(R_SUCCEEDED(rc));
        CHECK(serviceIsActive(&child));

        /* The child must be a working service of its own. */
        u32 out = 0;
        const u32 in7 = 7;
        const Result rc2 = serviceDispatchInOut(&child, 0, in7, out);
        CHECK(R_SUCCEEDED(rc2));
        CHECK_EQ(out, 8u);

        serviceClose(&child);
        serviceClose(&s);
    }
    EndCase();
}

/* Domain sessions: the object comes back as a domain object id and later
 * requests carry that id instead of a session handle. */
void TestDomainObjects() {
    StartCase("domain session: convert + object ids");
    {
        Fixture fx;
        Service s{};
        serviceCreate(&s, fx.Connect(new EchoService()));

        Result rc = serviceConvertToDomain(&s);
        CHECK(R_SUCCEEDED(rc));
        CHECK(serviceIsDomain(&s));
        CHECK_EQ(s.object_id, 1u);

        u32 in = 100, out = 0;
        rc = serviceDispatchInOut(&s, 0, in, out);
        CHECK(R_SUCCEEDED(rc));
        CHECK_EQ(out, 101u);

        /* Domain reply must carry a CmifDomainOutHeader. */
        const HipcResponse hp = hipcParseResponse(armGetTls());
        auto *dh = reinterpret_cast<CmifDomainOutHeader *>(
            cmifGetAlignedDataStart(hp.data_words, armGetTls()));
        CHECK_EQ(dh->num_out_objects, 0u);

        serviceClose(&s);
    }
    EndCase();
}

/* Object created inside a domain gets an object id (no new session). */
void TestDomainOutObject() {
    StartCase("domain session: out object -> object id");
    {
        Fixture fx;
        Service s{};
        serviceCreate(&s, fx.Connect(new EchoService()));
        serviceConvertToDomain(&s);

        Service child{};
        const Result rc = serviceDispatch(&s, 1,
            .out_num_objects = 1,
            .out_objects     = &child);
        CHECK(R_SUCCEEDED(rc));
        CHECK(serviceIsDomainSubservice(&child));
        CHECK(child.object_id >= 1);

        u32 in = 1, out = 0;
        CHECK(R_SUCCEEDED(serviceDispatchInOut(&child, 0, in, out)));
        CHECK_EQ(out, 2u);

        serviceClose(&s);
    }
    EndCase();
}

/* ResultShouldForwardToSession must replay the raw request at the real
 * service and return *its* reply untouched. */
void TestForwardToSession() {
    StartCase("mitm forward-to-session");
    {
        Fixture fx;

        /* The "real" ldn:u behind the mitm. */
        Handle real_client = mock::K().CreatePair("real_ldn");
        Handle real_server = mock::K().GetPairByClient(real_client)->server_handle;
        auto *real_object = new RealLdnService();
        CHECK(R_SUCCEEDED(fx.server.RegisterSession(real_server, real_object, false, 0, 0)));
        mock::K().SetHandler(real_server, [&fx](Handle h) { return fx.server.ProcessSession(h); });

        /* Our mitm session, forwarding to the real service. */
        Service s{};
        serviceCreate(&s, fx.Connect(new EchoService(), /*is_mitm=*/true, real_client, 0x1234));

        u32 out = 0;
        const Result rc = serviceDispatch(&s, 2, .out_num_objects = 0);
        CHECK(R_SUCCEEDED(rc));

        /* Read the forwarded reply the same way a client would. */
        const HipcResponse hp = hipcParseResponse(armGetTls());
        auto *out_header = reinterpret_cast<CmifOutHeader *>(
            cmifGetAlignedDataStart(hp.data_words, armGetTls()));
        CHECK_EQ(out_header->magic, ldn_nx::CmifOutHeaderMagic);
        const u32 *payload = reinterpret_cast<const u32 *>(out_header + 1);
        CHECK_EQ(*payload, 0xCAFEu);
        (void)out;

        serviceClose(&s);
        mock::K().CloseHandle(real_client);
    }
    EndCase();
}

/* Closing the client service must tear the server session down. */
void TestSessionClose() {
    StartCase("session close");
    {
        Fixture fx;
        Service s{};
        serviceCreate(&s, fx.Connect(new EchoService()));
        const size_t before = mock::K().LiveSessionCount();
        serviceClose(&s);
        CHECK(mock::K().LiveSessionCount() < before);
    }
    EndCase();
}

/* ------------------------------------------------------------------------- */
/* Mitm chain: mock sm + our sm client + our server + the real service        */
/* ------------------------------------------------------------------------- */

/* The service the client gets from CreateUserLocalCommunicationService. */
class CommunicationService : public ldn_nx::ServerObject {
public:
    Result Invoke(ldn_nx::Request &req, ldn_nx::Reply &reply) override {
        if (req.cmd_id == 5) {
            auto *out = reply.Out<u32>();
            *out = 0x5555;
            return 0;
        }
        return 0x7777;
    }
};

/* The mitm service registered for ldn:u. */
class MitmService : public ldn_nx::ServerObject {
public:
    explicit MitmService(Handle forward) : m_forward(forward) {}

    Result Invoke(ldn_nx::Request &req, ldn_nx::Reply &reply) override {
        switch (req.cmd_id) {
            case 0:   /* CreateUserLocalCommunicationService */
                reply.AddObject(new CommunicationService());
                return 0;
            case 2:   /* let the server forward this one to the real service */
                return ldn_nx::ResultShouldForwardToSession;
            default:
                return 0x7777;
        }
    }

    Handle m_forward;
};

class TestMitmServer : public ldn_nx::IpcServer {
public:
    ldn_nx::ServerObject *CreateMitmObject(Handle forward_session, const ldn_nx::MitmClientInfo &client_info) override {
        forward_handle = forward_session;
        client_info_seen = client_info;
        ++num_accepted;
        return new MitmService(forward_session);
    }

    Handle forward_handle = 0;
    ldn_nx::MitmClientInfo client_info_seen{};
    int num_accepted = 0;
};

class TestQueryService : public ldn_nx::MitmQueryService {
public:
    bool ShouldMitm(const ldn_nx::MitmClientInfo &) override { return true; }
};

/* Full mitm chain: install (real sm protocol) -> client connects -> accept +
 * acknowledge -> dispatch -> forward to the real service. */
void TestMitmFullChain() {
    StartCase("mitm: install -> accept -> acknowledge -> dispatch -> forward");
    {
        ldn_nx::IpcServer real_server;

        /* The service sitting behind the mitm. */
        auto factory = [&real_server]() -> Handle {
            const Handle client = mock::K().CreatePair("real-ldn");
            const Handle server = mock::K().GetPairByClient(client)->server_handle;
            real_server.RegisterSession(server, new RealLdnService(), false, 0, 0);
            mock::K().SetHandler(server, [&real_server](Handle h) { return real_server.ProcessSession(h); });
            return client;
        };
        mock::InstallMockSm(factory);

        CHECK(R_SUCCEEDED(ldn_sm_mitm_initialize()));

        Handle port = 0, query = 0;
        CHECK(R_SUCCEEDED(ldn_sm_mitm_install(&port, &query, "ldn:u")));
        CHECK(port != 0);

        TestMitmServer server;
        server.AttachPort(port, "ldn:u");

        /* Sessions accepted from the port, and sessions created for out
         * objects, are served by the same server. */
        mock::K().SetDefaultHandler([&server](Handle h) { return server.ProcessSession(h); });

        /* The mitm must serve the ShouldMitm query session as well. */
        auto *query_service = new TestQueryService();
        CHECK(R_SUCCEEDED(server.AttachSession(query, query_service)));

        /* A game connects to ldn:u; sm routes it to the mitm port. */
        Handle client = 0;
        CHECK(R_SUCCEEDED(svcConnectToNamedPort(&client, "ldn:u")));

        /* The mitm loop accepts it and acknowledges with sm. */
        CHECK(R_SUCCEEDED(server.AcceptMitmSession()));
        CHECK_EQ(server.num_accepted, 1);
        CHECK_EQ(server.client_info_seen.process_id, 0x00000000CAFEBABEull);
        CHECK(server.forward_handle != 0);

        /* From here on this is an ordinary client session. */
        Service s{};
        serviceCreate(&s, client);
        CHECK(serviceIsActive(&s));

        /* CreateUserLocalCommunicationService returns an object. */
        Service comm{};
        CHECK(R_SUCCEEDED(serviceDispatch(&s, 0, .out_num_objects = 1, .out_objects = &comm)));
        CHECK(serviceIsActive(&comm));

        u32 out = 0;
        CHECK(R_SUCCEEDED(serviceDispatchInOut(&comm, 5, out, out)));
        CHECK_EQ(out, 0x5555u);

        /* A command we do not implement is forwarded to the real service and
         * its reply comes back untouched. */
        CHECK(R_SUCCEEDED(serviceDispatch(&s, 2)));
        {
            const HipcResponse hp = hipcParseResponse(armGetTls());
            auto *out_header = reinterpret_cast<CmifOutHeader *>(
                cmifGetAlignedDataStart(hp.data_words, armGetTls()));
            CHECK_EQ(out_header->magic, ldn_nx::CmifOutHeaderMagic);
            const u32 *payload = reinterpret_cast<const u32 *>(out_header + 1);
            CHECK_EQ(*payload, 0xCAFEu);
        }

        serviceClose(&comm);
        serviceClose(&s);

        /* sm can ask the mitm whether it wants a client. */
        Service qs{};
        serviceCreate(&qs, mock::GetSmQueryClient());
        ldn_nx::MitmClientInfo info{};
        info.process_id = 0x1234;
        bool should = false;
        CHECK(R_SUCCEEDED(serviceDispatchInOut(&qs, ldn_nx::MitmQueryCommandShouldMitm, info, should)));
        CHECK(should);
        serviceClose(&qs);

        CHECK(R_SUCCEEDED(ldn_sm_mitm_uninstall("ldn:u")));

        /* After uninstall a new connection reaches the real service again. */
        Handle client2 = 0;
        CHECK(R_SUCCEEDED(svcConnectToNamedPort(&client2, "ldn:u")));
        {
            Service real{};
            serviceCreate(&real, client2);
            CHECK(R_SUCCEEDED(serviceDispatch(&real, 2)));
            const HipcResponse hp = hipcParseResponse(armGetTls());
            auto *out_header = reinterpret_cast<CmifOutHeader *>(
                cmifGetAlignedDataStart(hp.data_words, armGetTls()));
            const u32 *payload = reinterpret_cast<const u32 *>(out_header + 1);
            CHECK_EQ(*payload, 0xCAFEu);
            serviceClose(&real);
        }

        ldn_sm_mitm_finalize();
    }
    EndCase();
}

/* ------------------------------------------------------------------------- */
/* ldn services: every command, driven over the wire                         */
/* ------------------------------------------------------------------------- */
void TestLdnServices() {
    StartCase("ldn services: config / monitor / communication commands");
    {
        using namespace ams::mitm::ldn;

        FakeLan().Reset();
        FakeLan().networkInfo.networkId.sessionId.high = 0x1122334455667788ull;
        FakeLan().networkInfo.networkId.sessionId.low  = 0x99AABBCCDDEEFF00ull;

        Fixture fx;
        const Handle client = fx.Connect(new ILdnMitMService(0, FakeLanImpl()));
        Service s{};
        serviceCreate(&s, client);

        /* --- mitm level -------------------------------------------------- */
        Service comm{};
        CHECK(R_SUCCEEDED(serviceDispatch(&s, 0, .out_num_objects = 1, .out_objects = &comm)));
        CHECK(serviceIsActive(&comm));

        Service monitor{};
        CHECK(R_SUCCEEDED(serviceDispatch(&s, 1, .out_num_objects = 1, .out_objects = &monitor)));
        CHECK(serviceIsActive(&monitor));

        Service config{};
        CHECK(R_SUCCEEDED(serviceDispatch(&s, 65000, .out_num_objects = 1, .out_objects = &config)));
        CHECK(serviceIsActive(&config));

        /* --- config ------------------------------------------------------ */
        LdnMitmVersion version{};
        CHECK(R_SUCCEEDED(serviceDispatchOut(&config, 65001, version)));
        CHECK(std::strncmp(version.raw, "1.25.1-libnx-poc", sizeof(version.raw)) == 0);

        u32 value = 0;
        CHECK(R_SUCCEEDED(serviceDispatchOut(&config, 65004, value)));
        CHECK_EQ(value, 1u);

        const u32 disable = 0;
        CHECK(R_SUCCEEDED(serviceDispatchIn(&config, 65005, disable)));
        CHECK(!ILdnConfigService::IsEnabled());
        CHECK(R_SUCCEEDED(serviceDispatchOut(&config, 65004, value)));
        CHECK_EQ(value, 0u);

        const u32 enable = 1;
        CHECK(R_SUCCEEDED(serviceDispatchIn(&config, 65005, enable)));
        CHECK(ILdnConfigService::IsEnabled());

        const u32 loggingOn = 1;
        CHECK(R_SUCCEEDED(serviceDispatchIn(&config, 65003, loggingOn)));
        CHECK(R_SUCCEEDED(serviceDispatchOut(&config, 65002, value)));
        CHECK_EQ(value, 1u);
        const u32 loggingOff = 0;
        CHECK(R_SUCCEEDED(serviceDispatchIn(&config, 65003, loggingOff)));

        /* --- client process monitor -------------------------------------- */
        const u64 registerPid = 0x1000;
        CHECK(R_SUCCEEDED(serviceDispatchIn(&monitor, 0, registerPid)));

        /* --- communication: init/state ----------------------------------- */
        const u64 clientPid = 0x2000;
        CHECK(R_SUCCEEDED(serviceDispatchIn(&comm, 400, clientPid)));
        CHECK_EQ(FakeLan().initializeCalls, 1);

        u32 state = 0xFFFF;
        CHECK(R_SUCCEEDED(serviceDispatchOut(&comm, 0, state)));
        CHECK_EQ(state, static_cast<u32>(CommState::None));

        CHECK(R_SUCCEEDED(serviceDispatch(&comm, 200)));   /* OpenAccessPoint */
        CHECK_EQ(FakeLan().openAccessPointCalls, 1);
        CHECK(R_SUCCEEDED(serviceDispatchOut(&comm, 0, state)));
        CHECK_EQ(state, static_cast<u32>(CommState::AccessPoint));

        CHECK(R_SUCCEEDED(serviceDispatch(&comm, 201)));   /* CloseAccessPoint */
        CHECK(R_SUCCEEDED(serviceDispatch(&comm, 300)));   /* OpenStation */
        CHECK(R_SUCCEEDED(serviceDispatch(&comm, 301)));   /* CloseStation */
        CHECK(R_SUCCEEDED(serviceDispatch(&comm, 204)));   /* DestroyNetwork */
        CHECK(R_SUCCEEDED(serviceDispatch(&comm, 304)));   /* Disconnect */
        CHECK_EQ(FakeLan().closeAccessPointCalls, 1);
        CHECK_EQ(FakeLan().openStationCalls, 1);
        CHECK_EQ(FakeLan().closeStationCalls, 1);
        CHECK_EQ(FakeLan().destroyNetworkCalls, 1);
        CHECK_EQ(FakeLan().disconnectCalls, 1);

        /* --- communication: ipv4 / disconnect reason ---------------------- */
        struct { u32 address; u32 mask; } ip = {};
        CHECK(R_SUCCEEDED(serviceDispatchOut(&comm, 2, ip)));
        /* ntohl(0xC0A8010A) == 0x0A01A8C0: the server swaps to host order. */
        CHECK_EQ(ip.address, 0x0A01A8C0u);
        CHECK_EQ(ip.mask, 0x00FFFFFFu);

        u32 reason = 0xFFFF;
        CHECK(R_SUCCEEDED(serviceDispatchOut(&comm, 3, reason)));
        CHECK_EQ(reason, static_cast<u32>(DisconnectReason::None));

        /* --- communication: network info (LargeData -> out buffer) -------- */
        NetworkInfo info{};
        CHECK(R_SUCCEEDED(serviceDispatch(&comm, 1,
            .buffer_attrs = { SfBufferAttr_Out | SfBufferAttr_HipcMapAlias },
            .buffers      = { { &info, sizeof(info) } })));
        CHECK_EQ(info.networkId.sessionId.high, 0x1122334455667788ull);
        CHECK_EQ(info.networkId.sessionId.low, 0x99AABBCCDDEEFF00ull);

        /* --- communication: security parameter / network config ----------- */
        SecurityParameter security{};
        CHECK(R_SUCCEEDED(serviceDispatchOut(&comm, 4, security)));
        CHECK_EQ(security.sessionId.high, 0x1122334455667788ull);

        NetworkConfig netConfig{};
        CHECK(R_SUCCEEDED(serviceDispatchOut(&comm, 5, netConfig)));

        /* --- communication: advertise data (auto-select in buffer) -------- */
        u8 *advertise = static_cast<u8 *>(mock::AllocLow(0x40));
        for (size_t i = 0; i < 0x40; ++i) {
            advertise[i] = static_cast<u8>(i);
        }
        CHECK(R_SUCCEEDED(serviceDispatch(&comm, 206,
            .buffer_attrs = { SfBufferAttr_In | SfBufferAttr_HipcAutoSelect },
            .buffers      = { { advertise, 0x40 } })));
        CHECK_EQ(FakeLan().advertiseSize, 0x40u);
        CHECK(std::memcmp(FakeLan().advertiseData, advertise, 0x40) == 0);

        /* --- communication: scan, pointer-buffer path (small request) ----- */
        FakeLan().scanCount = 2;
        FakeLan().scanResults[0].networkId.sessionId.high = 0xAA;
        FakeLan().scanResults[1].networkId.sessionId.high = 0xBB;

        struct ScanIn {
            u16 channel;
            u8  pad[6];
            ScanFilter filter;
        } scanIn{};
        scanIn.channel = 6;
        scanIn.filter.flag = ScanFilterFlag_NetworkId;

        u32 scanCount = 0;
        auto *scanSmall = static_cast<ams::mitm::ldn::NetworkInfo *>(mock::AllocLow(sizeof(ams::mitm::ldn::NetworkInfo) * 2));
        CHECK(R_SUCCEEDED(serviceDispatchInOut(&comm, 102, scanIn, scanCount,
            .buffer_attrs = { SfBufferAttr_Out | SfBufferAttr_HipcAutoSelect },
            .buffers      = { { scanSmall, sizeof(ams::mitm::ldn::NetworkInfo) * 2 } })));
        CHECK_EQ(scanCount, 2u);
        CHECK_EQ(scanSmall[0].networkId.sessionId.high, 0xAAull);
        CHECK_EQ(scanSmall[1].networkId.sessionId.high, 0xBBull);

        /* --- communication: scan, HIPC buffer path (too big for pointers) -- */
        FakeLan().scanCount = 8;
        for (int i = 0; i < 8; ++i) {
            FakeLan().scanResults[i].networkId.sessionId.high = static_cast<u64>(0x100 + i);
        }
        auto *scanBig = static_cast<ams::mitm::ldn::NetworkInfo *>(mock::AllocLow(sizeof(ams::mitm::ldn::NetworkInfo) * 8));
        CHECK(R_SUCCEEDED(serviceDispatchInOut(&comm, 102, scanIn, scanCount,
            .buffer_attrs = { SfBufferAttr_Out | SfBufferAttr_HipcAutoSelect },
            .buffers      = { { scanBig, sizeof(ams::mitm::ldn::NetworkInfo) * 8 } })));
        CHECK_EQ(scanCount, 8u);
        CHECK_EQ(scanBig[7].networkId.sessionId.high, 0x107ull);

        /* --- communication: network info latest update (buffer + pointer) -- */
        FakeLan().updatesWritten = 3;
        NetworkInfo latest{};
        auto *updates = static_cast<ams::mitm::ldn::NodeLatestUpdate *>(mock::AllocLow(sizeof(ams::mitm::ldn::NodeLatestUpdate) * 4));
        CHECK(R_SUCCEEDED(serviceDispatch(&comm, 101,
            .buffer_attrs = {
                SfBufferAttr_Out | SfBufferAttr_HipcMapAlias,
                SfBufferAttr_Out | SfBufferAttr_HipcPointer,
            },
            .buffers = {
                { &latest, sizeof(latest) },
                { updates, sizeof(ams::mitm::ldn::NodeLatestUpdate) * 4 },
            })));
        CHECK_EQ(latest.networkId.sessionId.high, 0x1122334455667788ull);
        CHECK_EQ(updates[0].stateChange, 0);
        CHECK_EQ(updates[2].stateChange, 2);

        /* --- communication: connect (inline data + LargeData in buffer) --- */
        ConnectNetworkData connectData{};
        connectData.localCommunicationVersion = 0x00010002; /* truncated to u16, as the original does */
        std::strncpy(connectData.userConfig.userName, "player-one", sizeof(connectData.userConfig.userName) - 1);

        NetworkInfo target{};
        target.networkId.sessionId.high = 0xDEADBEEF;
        CHECK(R_SUCCEEDED(serviceDispatchIn(&comm, 302, connectData,
            .buffer_attrs = { SfBufferAttr_In | SfBufferAttr_HipcMapAlias },
            .buffers      = { { &target, sizeof(target) } })));
        CHECK_EQ(FakeLan().connectCalls, 1);
        CHECK_EQ(FakeLan().lastConnectTarget.networkId.sessionId.high, 0xDEADBEEFull);
        CHECK_EQ(FakeLan().lastConnectVersion, static_cast<u16>(0x00010002u));
        CHECK(std::strncmp(FakeLan().lastConnectUser.userName, "player-one", sizeof(FakeLan().lastConnectUser.userName)) == 0);

        /* --- communication: state change event (copy handle) -------------- */
        Handle event = 0;
        CHECK(R_SUCCEEDED(serviceDispatch(&comm, 100,
            .out_handle_attrs = { SfOutHandleAttr_HipcCopy },
            .out_handles      = &event)));
        CHECK(event != 0);

        /* --- communication: finalize ------------------------------------- */
        CHECK(R_SUCCEEDED(serviceDispatch(&comm, 401)));
        CHECK_EQ(FakeLan().finalizeCalls, 1);

        serviceClose(&config);
        serviceClose(&monitor);
        serviceClose(&comm);
        serviceClose(&s);
    }
    EndCase();
}

/* ------------------------------------------------------------------------- */
/* Accept loop: the real Run() on its own thread                             */
/* ------------------------------------------------------------------------- */
void TestAcceptLoop() {
    StartCase("accept loop: real Run() thread, client over the wire");
    {
        ldn_nx::IpcServer real_server;
        auto factory = [&real_server]() -> Handle {
            const Handle client = mock::K().CreatePair("real-ldn");
            const Handle server = mock::K().GetPairByClient(client)->server_handle;
            real_server.RegisterSession(server, new RealLdnService(), false, 0, 0);
            mock::K().SetHandler(server, [&real_server](Handle h) { return real_server.ProcessSession(h); });
            return client;
        };
        mock::InstallMockSm(factory);

        CHECK(R_SUCCEEDED(ldn_sm_mitm_initialize()));

        Handle port = 0, query = 0;
        CHECK(R_SUCCEEDED(ldn_sm_mitm_install(&port, &query, "ldn:u")));

        TestMitmServer server;
        server.AttachPort(port, "ldn:u");
        server.AttachSession(query, new TestQueryService());

        /* The sysmodule's real loop, on its own thread. It blocks in
         * svcReplyAndReceive() until a client shows up. */
        std::thread loop([&server]() { server.Run(); });

        Handle client = 0;
        CHECK(R_SUCCEEDED(svcConnectToNamedPort(&client, "ldn:u")));

        Service s{};
        serviceCreate(&s, client);
        CHECK(serviceIsActive(&s));

        Service comm{};
        CHECK(R_SUCCEEDED(serviceDispatch(&s, 0, .out_num_objects = 1, .out_objects = &comm)));
        CHECK(serviceIsActive(&comm));

        u32 out = 0;
        CHECK(R_SUCCEEDED(serviceDispatchInOut(&comm, 5, out, out)));
        CHECK_EQ(out, 0x5555u);

        /* Forwarded to the real service, through the loop thread. */
        CHECK(R_SUCCEEDED(serviceDispatch(&s, 2)));
        {
            const HipcResponse hp = hipcParseResponse(armGetTls());
            auto *out_header = reinterpret_cast<CmifOutHeader *>(
                cmifGetAlignedDataStart(hp.data_words, armGetTls()));
            const u32 *payload = reinterpret_cast<const u32 *>(out_header + 1);
            CHECK_EQ(*payload, 0xCAFEu);
        }

        CHECK_EQ(server.num_accepted, 1);

        serviceClose(&comm);
        serviceClose(&s);

        /* Stop the loop: ask it to return, then unblock the wait. */
        server.Stop();
        mock::K().CloseHandle(port);
        loop.join();

        ldn_sm_mitm_finalize();
    }
    EndCase();
}

/* ------------------------------------------------------------------------- */
/* Differential test: our sm client vs Atmosphere's (pinned reference)        */
/* ------------------------------------------------------------------------- */
void TestSmClientParity() {
    StartCase("parity: our sm mitm client vs Atmosphere's, byte for byte");
    {
        /* The reference client's serviceCreate() queries the pointer buffer size
         * on the returned session, so the fake real service must answer. */
        mock::InstallMockSm([]() -> Handle {
            const Handle client = mock::K().CreatePair("real-ldn");
            const Handle server = mock::K().GetPairByClient(client)->server_handle;
            mock::K().SetHandler(server, [](Handle) {
                std::memset(armGetTls(), 0, 0x100);
                return 0;
            });
            return client;
        });
        /* The mock only trusts the pid a client sends when it is non-zero;
         * start from a clean TLS so both clients look like pid 0. */
        std::memset(armGetTls(), 0, mock::TlsMessageBufferSize);
        CHECK(R_SUCCEEDED(ldn_sm_mitm_initialize()));

        std::vector<u8> refInstall, refUninstall, refAck;
        std::vector<u8> ourInstall, ourUninstall, ourAck;

        /* Capture the last request for a given TIPC command id. The reference
         * client sends a control request of its own inside serviceCreate(), so
         * "last request" is not necessarily the one we want. */
        auto capture = [](u32 command, std::vector<u8> &out) {
            u8 buffer[mock::TlsMessageBufferSize];
            size_t size = 0;
            if (!mock::K().FindLastRequest(command + 16 /* TIPC */, buffer, sizeof(buffer), &size)) {
                out.clear();
                return;
            }
            out.assign(buffer, buffer + size);
        };
        auto reset_tls = []() { std::memset(armGetTls(), 0, mock::TlsMessageBufferSize); };

        /* --- InstallMitm (65000) --- */
        reset_tls();
        TipcService forward_session{};
        CHECK(R_SUCCEEDED(smAtmosphereOpenSession(&forward_session)));

        reset_tls();
        Handle ref_port = 0, ref_query = 0;
        CHECK(R_SUCCEEDED(smAtmosphereMitmInstall(&forward_session, &ref_port, &ref_query, smEncodeName("ldn:u"))));
        capture(65000, refInstall);

        reset_tls();
        Handle our_port = 0, our_query = 0;
        CHECK(R_SUCCEEDED(ldn_sm_mitm_install(&our_port, &our_query, "ldn:u")));
        capture(65000, ourInstall);

        CHECK(refInstall.size() > 0);
        CHECK_EQ(refInstall.size(), ourInstall.size());
        CHECK(refInstall == ourInstall);

        /* --- UninstallMitm (65001) --- */
        reset_tls();
        CHECK(R_SUCCEEDED(smAtmosphereMitmUninstall(smEncodeName("ldn:u"))));
        capture(65001, refUninstall);

        reset_tls();
        CHECK(R_SUCCEEDED(ldn_sm_mitm_uninstall("ldn:u")));
        capture(65001, ourUninstall);

        CHECK_EQ(refUninstall.size(), ourUninstall.size());
        CHECK(refUninstall == ourUninstall);

        /* --- AcknowledgeSession (65003) --- */
        CHECK(R_SUCCEEDED(smAtmosphereMitmInitialize()));

        reset_tls();
        Service ref_service{};
        u8 ref_info[0x20] = {};
        CHECK(R_SUCCEEDED(smAtmosphereMitmAcknowledgeSession(&ref_service, ref_info, smEncodeName("ldn:u"))));
        capture(65003, refAck);
        smAtmosphereMitmExit();

        reset_tls();
        Handle our_forward = 0;
        LdnMitmProcessInfo our_info{};
        CHECK(R_SUCCEEDED(ldn_sm_mitm_acknowledge(&our_forward, &our_info, "ldn:u")));
        capture(65003, ourAck);

        CHECK_EQ(refAck.size(), ourAck.size());
        CHECK(refAck == ourAck);

        /* Both implementations must also decode the reply identically. */
        CHECK(std::memcmp(ref_info, &our_info, sizeof(our_info)) == 0);
        CHECK_EQ(our_info.process_id, 0x00000000CAFEBABEull);
        CHECK(our_forward != 0);
        CHECK(ref_service.session != 0);

        smAtmosphereCloseSession(&forward_session);
        ldn_sm_mitm_finalize();
    }
    EndCase();
}

} // namespace

int main() {
    std::printf("== ldn_mitm libnx-only IPC verification ==\n");
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (std::getenv("LDN_TEST_VERBOSE") != nullptr) { mock::K().SetVerbose(true); }

    TestBasicDispatch();
    TestErrorPropagation();
    TestOutObjectAsHandle();
    TestDomainObjects();
    TestDomainOutObject();
    TestForwardToSession();
    TestSessionClose();
    TestMitmFullChain();
    TestLdnServices();
    TestAcceptLoop();
    TestSmClientParity();

    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
