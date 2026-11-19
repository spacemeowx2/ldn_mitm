#requires -Version 5.1
<#
    Host-side verification build for the libnx-only PoC.

    Pipeline:
      1. mirror libnx's headers (only switch/arm/tls.h and BITL are patched,
         both documented below)
      2. compile tests/main.cpp (a *client* written against real libnx:
         service.h / cmif.h / hipc.h) + src/ipc_server.cpp (the code under
         test) + harness/mock_kernel.cpp (simulated kernel)
      3. run the resulting exe

    Usage:
      powershell -ExecutionPolicy Bypass -File verify/build.ps1
      powershell -ExecutionPolicy Bypass -File verify/build.ps1 -NoSync
      powershell -ExecutionPolicy Bypass -File verify/build.ps1 -Verbose   (mock kernel log)

    The MinGW toolchain is expected at toolchain\msys\mingw64 (see
    verify/extract_toolchain.ps1); it only needs to be a GCC that can compile
    GNU C++17 -- nothing from devkitPro is required for the host build.
#>
param(
    [string]$LibNx        = "C:\Users\space\project\libnx\nx\include",
    [string]$ToolchainDir = "C:\Users\space\project\toolchain\msys\mingw64",
    [switch]$NoSync,
    [switch]$NoRun,
    [switch]$Verbose
)

$ErrorActionPreference = "Stop"
$here    = Split-Path -Parent $MyInvocation.MyCommand.Path
$pocRoot = Split-Path -Parent $here
$includeDir = Join-Path $here "harness\libnx_include"
$exe     = Join-Path $here "ipc_tests.exe"
$gxx     = Join-Path $ToolchainDir "bin\g++.exe"
$libDir  = Join-Path $ToolchainDir "lib"

function Sync-LibNxHeaders {
    param([string]$Source, [string]$Destination)

    if (-not (Test-Path (Join-Path $Source "switch.h"))) {
        throw "libnx headers not found at '$Source' (pass -LibNx)"
    }

    if (Test-Path $Destination) {
        Remove-Item -Recurse -Force $Destination
    }
    New-Item -ItemType Directory -Force -Path $Destination | Out-Null
    Copy-Item (Join-Path $Source "*") -Destination $Destination -Recurse -Force

    # (1) aarch64 asm: tpidrro_el0 does not exist on the mock kernel.
    @'
#pragma once
#include "../types.h"

/* Host shim: the real libnx header reads tpidrro_el0, which does not exist
 * on the mock kernel. The implementation lives in harness/mock_kernel.cpp. */
#ifdef __cplusplus
extern "C" {
#endif

void* armGetTls(void);

#ifdef __cplusplus
}
#endif
'@ | Set-Content -Path (Join-Path $Destination "switch\arm\tls.h") -Encoding ascii

    # (2) BITL() must be a 64-bit shift. It already is on AArch64 (long == 64
    #     bit), but the host is LLP64 (long == 32 bit), so hid.h would not
    #     compile. This is a host-only correction of a host-only assumption.
    $typesHeader = Join-Path $Destination "switch\types.h"
    $types = Get-Content $typesHeader -Raw
    $types = $types.Replace('#define BITL(n) (1UL<<(n))', '#define BITL(n) ((uint64_t)1<<(n))')
    Set-Content -Path $typesHeader -Value $types -NoNewline

    $commit = "unknown"
    try { $commit = (git -C $Source rev-parse --short HEAD 2>$null) } catch { }
    Write-Host "synced libnx headers -> $Destination (upstream $commit)"
}

if (-not $NoSync) {
    Sync-LibNxHeaders -Source $LibNx -Destination $includeDir
}

if (-not (Test-Path $gxx)) {
    throw "g++ not found at '$gxx' (pass -ToolchainDir). See verify/extract_toolchain.ps1."
}

# cc1plus/ld need their DLLs; they live next to g++.
$env:PATH = "$(Join-Path $ToolchainDir 'bin');$env:PATH"
if ($Verbose) { $env:LDN_TEST_VERBOSE = "1" } else { Remove-Item Env:\LDN_TEST_VERBOSE -ErrorAction SilentlyContinue }

$sources = @(
    (Join-Path $here    "harness\mock_kernel.cpp"),
    (Join-Path $here    "harness\mock_sm.cpp"),
    (Join-Path $here    "harness\libnx_host_runtime.cpp"),
    (Join-Path $here    "harness\fake_lan_discovery.cpp"),
    (Join-Path $pocRoot "src\ipc_server.cpp"),
    (Join-Path $pocRoot "src\sm_mitm.c"),
    (Join-Path $pocRoot "src\debug.cpp"),
    (Join-Path $pocRoot "src\ldn_types.cpp"),
    (Join-Path $pocRoot "src\ldn_services.cpp"),
    (Join-Path $here    "tests\main.cpp")
)

$arguments = @(
    "-std=gnu++17", "-O1", "-g", "-Wall", "-Wextra", "-Wno-unused-parameter", "-Wno-missing-field-initializers",
    "-I", $includeDir,
    "-I", (Join-Path $here "harness\hostshim"),
    "-I", (Join-Path $pocRoot "src"),
    "-I", (Join-Path $here "harness"),
    "-L", $libDir
) + $sources + @("-o", $exe)

Write-Host "compiling with $gxx"
# Native commands write diagnostics on stderr; do not let that abort the script.
$nativePref = $ErrorActionPreference
$ErrorActionPreference = "Continue"
& $gxx @arguments
$compileExit = $LASTEXITCODE
$ErrorActionPreference = $nativePref
if ($compileExit -ne 0) {
    throw "compile failed ($compileExit)"
}
Write-Host "built $exe"

if (-not $NoRun) {
    $ErrorActionPreference = "Continue"
    & $exe
    $testExit = $LASTEXITCODE
    $ErrorActionPreference = $nativePref
    if ($testExit -ne 0) {
        throw "tests failed ($testExit)"
    }
    Write-Host "all checks passed"
}
