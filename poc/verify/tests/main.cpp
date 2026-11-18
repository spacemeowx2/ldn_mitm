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
#include <cstring>

#include "ipc_server.hpp"
#include "mock_kernel.h"

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

    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
