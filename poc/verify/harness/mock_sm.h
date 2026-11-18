/*
 * Host-side mock of Atmosphere's sm, limited to what a mitm sysmodule uses.
 *
 * It speaks the real wire protocol (TIPC: hipc header with type = cmd + 16, the
 * response payload starts with the Result u32) so the code under test is our
 * real libnx-only client (src/sm_mitm.c), not a stub.
 */
#pragma once

#include <functional>

#include "mock_kernel.h"

namespace mock {

/* Creates the client-side session to the "real" service that sm hands to the
 * mitm on acknowledge. */
using RealServiceFactory = std::function<Handle()>;

/* Installs the "sm:" named port and the mitm bookkeeping. */
void InstallMockSm(RealServiceFactory factory);

/* Handle sm uses to query ShouldMitm() on the mitm (client side of the session
 * whose server side was handed to the mitm by InstallMitm). */
Handle GetSmQueryClient();

/* The port the mitm registered, if any. */
Handle GetMitmPort();

} // namespace mock
