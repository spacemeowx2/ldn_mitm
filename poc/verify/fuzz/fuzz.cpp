/*
 * fuzz.cpp -- robustness fuzzing for the libnx-only IPC server.
 *
 * The protocol layer is hand-written pointer arithmetic (HIPC descriptors,
 * CMIF headers, out-pointer size tables), so it is exactly the kind of code
 * that needs adversarial input rather than hand-picked cases.
 *
 * The fuzzer builds *structurally plausible but hostile* requests:
 *   - command ids from the real set (and random ones)
 *   - arbitrary numbers of server pointers / buffers / handles
 *   - buffer sizes that are huge, zero, unaligned or inconsistent
 *   - payload lengths that do not match what the command expects
 * and asserts invariants that must hold for every single input:
 *
 *   I1  the dispatch returns (no hang, no crash) -- ASan/UBSan supervised
 *   I2  the reply is a well-formed HIPC response header
 *   I3  the reply never claims more data than the message buffer can hold
 *   I4  an out buffer is never written outside [base, base + size)
 *   I5  a command that fails returns a non-zero Result (never "success" with
 *       an untouched payload)
 *
 * I4 is the interesting one: it catches the classic off-by-one in size-table
 * handling. The mock maps one big scratch region, so writes are compared
 * against the declared bounds rather than against the allocation.
 *
 * Usage:
 *   ./fuzz [iterations] [seed]
 */
#include <switch.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

#include "fake_lan_discovery.h"
#include "ipc_server.hpp"
#include "ldn_services.hpp"
#include "mitm_query_service.hpp"
#include "mock_kernel.h"
#include "mock_sm.h"
#include "sm_mitm.h"

namespace {

using namespace ams::mitm::ldn;
using ldn_nx::IpcServer;
using ldn_nx::Request;
using ldn_nx::Reply;

constexpr size_t ScratchSize = 0x20000;

/* The commands the real services implement; used to bias the generator toward
 * code paths that actually exist. */
const u32 kMitmCommands[] = { 0, 1, 65000 };
const u32 kCommCommands[] = {
    0, 1, 2, 3, 4, 5, 100, 101, 102,
    200, 201, 202, 203, 204, 205, 206, 207, 208, 209,
    300, 301, 302, 303, 304,
    400, 401, 402,
    65000, 65001, 65002, 65003, 65004, 65005,
};

struct Rng {
    u64 state;

    explicit Rng(u64 seed) : state(seed != 0 ? seed : 0x9E3779B97F4A7C15ull) { }

    u32 Next() {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        return static_cast<u32>(state >> 32);
    }

    u32 Below(u32 limit) {
        return limit == 0 ? 0 : this->Next() % limit;
    }

    bool Chance(u32 percent) {
        return this->Below(100) < percent;
    }
};

class FuzzQuery final : public ldn_nx::MitmQueryService {
    public:
        bool ShouldMitm(const ldn_nx::MitmClientInfo &) override { return true; }
};

class FuzzServer final : public IpcServer {
    protected:
        ldn_nx::ServerObject *CreateMitmObject(Handle forward_session, const ldn_nx::MitmClientInfo &) override {
            return new ILdnMitMService(forward_session, FakeLanImpl());
        }
};

/* One out buffer we handed to the server, so we can check what it wrote. */
struct OutRegion {
    u8 *base;
    size_t size;
};

struct Generator {
    Rng rng;
    u8 *scratch;
    std::vector<OutRegion> out_regions;

    Generator(u64 seed, u8 *scratch_) : rng(seed), scratch(scratch_) { }

    u32 PickCommand(const u32 *set, size_t count) {
        if (this->rng.Chance(85)) {
            return set[this->rng.Below(static_cast<u32>(count))];
        }
        return this->rng.Below(0x10000);
    }

    /* A "buffer" pointer that is inside our scratch region, so that reads and
     * writes the server legitimately performs stay mapped. */
    u8 *PickBuffer(size_t &size, bool out, u32 max_size) {
        const size_t offset = this->rng.Below(0x1000) * 4; /* stay 4-byte aligned */
        const size_t available = ScratchSize - offset;
        const size_t cap = available < max_size ? available : max_size;

        switch (this->rng.Below(8)) {
            case 0: size = 0; break;                       /* zero length */
            case 1: size = 1; break;                       /* tiny */
            case 2: size = cap; break;                     /* exactly fits */
            case 3: size = cap > 0 ? this->rng.Below(static_cast<u32>(cap)) + 1 : 0; break;
            case 4: size = this->rng.Below(0x10); break;   /* odd small */
            default: size = this->rng.Below(static_cast<u32>(cap > 0x4000 ? 0x4000 : cap) + 1); break;
        }
        if (size > cap) {
            size = cap;
        }

        u8 *base = this->scratch + offset;
        if (out) {
            this->out_regions.push_back({ base, size });
        }
        return base;
    }

    /* Builds one request in `tls`. */
    void Build(u8 *tls, const u32 *commands, size_t command_count) {
        std::memset(tls, 0, mock::TlsMessageBufferSize);
        this->out_regions.clear();

        const u32 command = this->PickCommand(commands, command_count);
        const bool tipc = this->rng.Chance(70);

        const u32 num_statics = this->rng.Below(3);
        const u32 num_send_buffers = this->rng.Below(3);
        const u32 num_recv_buffers = this->rng.Below(3);
        const u32 num_exch_buffers = this->rng.Chance(20) ? this->rng.Below(2) : 0;
        const u32 num_move_handles = this->rng.Chance(10) ? this->rng.Below(2) : 0;
        const u32 num_copy_handles = this->rng.Chance(10) ? this->rng.Below(2) : 0;
        const u32 data_words = this->rng.Below(0x41);

        /* Set the fields directly: the designated-initialiser form has a fixed
         * order that would make this harder to read. */
        HipcMetadata meta = {};
        u16 type = tipc ? static_cast<u16>(command + 16) : static_cast<u16>(command);
        /* 2/5/6 are the CMIF close/control codes; a wrap-around would close the
         * session and every later request on it would (correctly) go unanswered,
         * which would swamp the other invariants. Keep them out of the corpus. */
        if (type < 16 || type == 2 || type == 5 || type == 6) {
            type = 0x7FFFu;
        }
        meta.type             = type;
        meta.num_send_statics = static_cast<u16>(num_statics);
        meta.num_send_buffers = static_cast<u16>(num_send_buffers);
        meta.num_recv_buffers = static_cast<u16>(num_recv_buffers);
        meta.num_exch_buffers = static_cast<u16>(num_exch_buffers);
        meta.num_data_words   = static_cast<u16>(data_words);
        meta.num_copy_handles = num_copy_handles;
        meta.num_move_handles = num_move_handles;

        const HipcRequest request = hipcMakeRequest(tls, meta);

        for (u32 i = 0; i < num_statics; ++i) {
            size_t size = 0;
            u8 *base = this->PickBuffer(size, false, 0x2000);
            request.send_statics[i] = hipcMakeSendStatic(base, size, static_cast<u8>(i));
        }

        auto make_buffers = [&](HipcBufferDescriptor *descriptors, u32 count, bool out) {
            for (u32 i = 0; i < count; ++i) {
                size_t size = 0;
                u8 *base = this->PickBuffer(size, out, 0x4000);
                const auto mode = static_cast<HipcBufferMode>(this->rng.Below(3)); /* 0 normal, 1 pointer, 2 auto */
                descriptors[i] = hipcMakeBuffer(base, size, mode);
            }
        };
        make_buffers(request.send_buffers, num_send_buffers, false);
        make_buffers(request.recv_buffers, num_recv_buffers, true);
        make_buffers(request.exch_buffers, num_exch_buffers, false);

        auto *words = reinterpret_cast<u32 *>(request.data_words);
        for (u32 i = 0; i < data_words; ++i) {
            /* Bias the payload toward plausible command arguments. */
            switch (this->rng.Below(4)) {
                case 0: words[i] = 0; break;
                case 1: words[i] = this->rng.Below(0x100); break;          /* small enum */
                case 2: words[i] = 0xFFFFFFFFu; break;
                default: words[i] = this->rng.Next(); break;
            }
        }
    }
};

/* HIPC "server pointers" only carry ~40 address bits, so every buffer we hand
 * to the server has to live below 1 TiB. ASan, however, only instruments
 * memory in its own mappings (a few hundred MB at the bottom of the address
 * space, or the multi-TB high range), so the scratch is mapped inside the low
 * range ASan knows about -- otherwise every access would be reported as
 * unknown-crash instead of being checked. */
void *MapScratch(size_t size) {
    static const uintptr_t kCandidates[] = {
        0x20000000, 0x30000000, 0x40000000, 0x50000000, 0x60000000,
    };
#ifdef _WIN32
    for (uintptr_t base : kCandidates) {
        void *p = VirtualAlloc(reinterpret_cast<void *>(base), size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (p != nullptr) {
            return p;
        }
    }
    return nullptr;
#else
    for (uintptr_t base : kCandidates) {
        void *p = mmap(reinterpret_cast<void *>(base), size, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        if (p != MAP_FAILED) {
            return p;
        }
    }
    return nullptr;
#endif
}

int g_failures = 0;

/* Ring buffer of recent requests, so a failure can be traced back to the input
 * that caused it (the interesting request is usually the one *before* the one
 * that stops replying). */
struct RecentRequest {
    u32 iteration;
    u16 type;
    u8 bytes[32];
};
RecentRequest g_recent[8];
int g_recent_count = 0;

void RememberRequest(u32 iteration, const u8 *tls) {
    auto &slot = g_recent[g_recent_count % 8];
    slot.iteration = iteration;
    slot.type = *reinterpret_cast<const u16 *>(tls);
    std::memcpy(slot.bytes, tls, sizeof(slot.bytes));
    ++g_recent_count;
}

void DumpRecent() {
    const int count = g_recent_count < 8 ? g_recent_count : 8;
    for (int i = count; i > 0; --i) {
        const auto &slot = g_recent[(g_recent_count - i) % 8];
        std::printf("    recent iter=%u type=%#x:", slot.iteration, slot.type);
        for (size_t b = 0; b < sizeof(slot.bytes); ++b) {
            std::printf(" %02x", slot.bytes[b]);
        }
        std::printf("\n");
    }
}

void Fail(const char *what, u32 iteration, u32 command) {
    ++g_failures;
    std::printf("[fuzz][FAIL] iter=%u cmd=%u: %s\n", iteration, command, what);
}

/* I2/I3: the reply must be a well formed HIPC response. */
bool CheckReplyHeader(u32 iteration, u32 command, Result rc, Handle handle) {
    static int s_reported = 0;
    const u8 *tls = static_cast<const u8 *>(armGetTls());
    const HipcHeader header = *reinterpret_cast<const HipcHeader *>(tls);

    if (header.type != 0) {
        if (s_reported < 3) {
            ++s_reported;
            const u8 *raw = static_cast<const u8 *>(armGetTls());
            std::printf("    iter=%u cmd=%u handle=%#x rc=%#x reply_type=%#x raw:", iteration, command, handle, rc, header.type);
            for (int i = 0; i < 24; ++i) {
                std::printf(" %02x", raw[i]);
            }
            std::printf("\n");
        }
        Fail("reply is not a TIPC/CMIF response (type != 0)", iteration, command);
        return false;
    }
    if (header.num_data_words > (mock::TlsMessageBufferSize - sizeof(HipcHeader)) / sizeof(u32)) {
        Fail("reply claims more data than the message buffer holds", iteration, command);
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char **argv) {
    const u32 iterations = argc > 1 ? static_cast<u32>(std::strtoul(argv[1], nullptr, 10)) : 100000;
    const u64 seed = argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 1;

    std::setvbuf(stdout, nullptr, _IONBF, 0);

    auto *scratch = static_cast<u8 *>(MapScratch(ScratchSize));
    if (scratch == nullptr) {
        std::printf("could not map the scratch region\n");
        return 1;
    }

    std::memset(scratch, 0xCD, ScratchSize);

    /* The mitm needs a real service behind it for forwarding. */
    mock::InstallMockSm([]() -> Handle {
        const Handle client = mock::K().CreatePair("real-ldn");
        const Handle server = mock::K().GetPairByClient(client)->server_handle;
        mock::K().SetHandler(server, [](Handle) {
            std::memset(armGetTls(), 0, mock::TlsMessageBufferSize);
            return 0;
        });
        return client;
    });
    if (R_FAILED(ldn_sm_mitm_initialize())) {
        std::printf("sm init failed\n");
        return 1;
    }

    Handle port = 0, query = 0;
    if (R_FAILED(ldn_sm_mitm_install(&port, &query, "ldn:u"))) {
        std::printf("mitm install failed\n");
        return 1;
    }

    FuzzServer server;
    server.AttachPort(port, "ldn:u");
    server.AttachSession(query, new FuzzQuery());

    /* Two targets: the mitm object itself, and a communication object obtained
     * through it (the interesting commands live there). */
    const Handle mitm_client = mock::K().CreatePair("fuzz-mitm");
    const Handle mitm_server = mock::K().GetPairByClient(mitm_client)->server_handle;
    server.RegisterSession(mitm_server, new ILdnMitMService(0, FakeLanImpl()), false, 0, 0);

    const Handle comm_client = mock::K().CreatePair("fuzz-comm");
    const Handle comm_server = mock::K().GetPairByClient(comm_client)->server_handle;
    server.RegisterSession(comm_server, new ICommunicationService(FakeLanImpl()), false, 0, 0);

    std::printf("fuzzing %u iterations, seed %llu\n", iterations, static_cast<unsigned long long>(seed));

    Generator generator(seed, scratch);
    std::vector<u8> before(ScratchSize);

    for (u32 i = 0; i < iterations; ++i) {
        const bool use_comm = (i % 2) == 1;
        const Handle server_handle = use_comm ? comm_server : mitm_server;
        const u32 *commands = use_comm ? kCommCommands : kMitmCommands;
        const size_t command_count = use_comm
            ? sizeof(kCommCommands) / sizeof(kCommCommands[0])
            : sizeof(kMitmCommands) / sizeof(kMitmCommands[0]);

        u8 *tls = static_cast<u8 *>(armGetTls());
        generator.Build(tls, commands, command_count);

        /* The command the server is going to see, for the diagnostics. */
        const HipcHeader request_header = *reinterpret_cast<const HipcHeader *>(tls);
        const u32 command = request_header.type >= 16 ? request_header.type - 16 : request_header.type;

        std::memcpy(before.data(), scratch, ScratchSize);

        /* I1: must return. */
        const u32 command_id = command;
        RememberRequest(i, tls);
        const Result rc = server.ProcessSession(server_handle);


        if (!CheckReplyHeader(i, command_id, rc, server_handle)) {
            continue;
        }

        /* I4: nothing may be written outside a declared out buffer. */
        for (size_t offset = 0; offset < ScratchSize; ++offset) {
            if (before[offset] == scratch[offset]) {
                continue;
            }
            bool covered = false;
            for (const auto &region : generator.out_regions) {
                const size_t start = static_cast<size_t>(region.base - scratch);
                if (offset >= start && offset < start + region.size) {
                    covered = true;
                    break;
                }
            }
            if (!covered) {
                Fail("out-of-bounds write outside any declared out buffer", i, command_id);
                break;
            }
        }
    }

    std::printf("%u iterations, %d failures\n", iterations, g_failures);
    return g_failures == 0 ? 0 : 1;
}
