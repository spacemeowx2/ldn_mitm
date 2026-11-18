#requires -Version 5.1
<#
    Unpacks the MSYS2 mingw-w64 packages downloaded into toolchain\ into a
    local MSYS2-like root (toolchain\msys\mingw64\...).

    `tar` on Windows 10+ is bsdtar and handles .pkg.tar.zst, so no pacman and
    no admin rights are required.
#>
param(
    [string]$Root = "C:\Users\space\project\toolchain"
)

$ErrorActionPreference = "Stop"

$dest = Join-Path $Root "msys"
New-Item -ItemType Directory -Force -Path $dest | Out-Null

$packages = Get-ChildItem (Join-Path $Root "*.pkg.tar.zst") -ErrorAction SilentlyContinue
if (-not $packages) {
    throw "no .pkg.tar.zst packages in $Root"
}

foreach ($package in $packages) {
    Write-Host ("extracting {0}" -f $package.Name)
    & tar -xf $package.FullName -C $dest
    if ($LASTEXITCODE -ne 0) {
        throw ("tar failed for {0}" -f $package.Name)
    }
}

$gxx = Join-Path $dest "mingw64\bin\g++.exe"
if (-not (Test-Path $gxx)) {
    throw "g++.exe not found after extraction"
}

& $gxx --version
