<#
.SYNOPSIS
    Builds the Trino ODBC driver (64-bit) and packages it as an MSI.

.DESCRIPTION
    Run on Windows with the following prerequisites installed:
      * Visual Studio 2022 Build Tools (MSVC, C toolchain)
      * CMake 3.21+
      * vcpkg (with the VCPKG_ROOT environment variable set)
      * WiX Toolset v4/v5  (dotnet tool install --global wix)

    The script configures with the "windows-x64" CMake preset (which uses the
    vcpkg toolchain and the vcpkg.json manifest to fetch curl + json-c), builds
    the driver DLL, gathers the runtime dependency DLLs next to it, and runs
    `wix build` to produce trino_odbc-x64.msi.

.PARAMETER Version
    Product version embedded in the MSI (default 1.0.0.0).

.EXAMPLE
    pwsh installer/build_msi.ps1
#>
[CmdletBinding()]
param(
    [string]$Version = "1.0.0.0",
    [string]$Output  = "trino_odbc-x64.msi"
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

if (-not $env:VCPKG_ROOT) {
    throw "VCPKG_ROOT is not set. Install vcpkg and set VCPKG_ROOT to its path."
}

Write-Host "==> Configuring (CMake preset: windows-x64)"
cmake --preset windows-x64

Write-Host "==> Building driver (Release)"
cmake --build --preset windows-x64

# Locate the built DLL. Single-config generators (Ninja) place it in
# build-windows\src\; multi-config generators (Visual Studio) use a Release\
# subfolder. Search both.
$dll = Get-ChildItem -Path (Join-Path $repoRoot "build-windows\src") `
    -Filter "trino_odbc.dll" -Recurse -ErrorAction SilentlyContinue |
    Select-Object -First 1
if (-not $dll) {
    throw "trino_odbc.dll not found under build-windows\src - the build step likely failed."
}
$binDir = $dll.DirectoryName
Write-Host "==> Found driver DLL at $binDir"

# curl and json-c are statically linked into trino_odbc.dll (the windows-x64
# preset uses the x64-windows-static-md vcpkg triplet), so there are no runtime
# dependency DLLs to bundle - the MSI ships a single self-contained driver DLL.

Write-Host "==> Building MSI with WiX"
wix build installer/trino_odbc.wxs `
    -arch x64 `
    -d "BinDir=$binDir" `
    -o $Output
if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "WiX failed. If you saw error WIX7015 (OSMF EULA), WiX v7 requires"
    Write-Host "accepting the Open Source Maintenance Fee EULA. Either:"
    Write-Host "  * accept it:  `$env:WIX_ACCEPT_OSMF_EULA = '1'   (see https://wixtoolset.org/osmf/)"
    Write-Host "  * or use WiX v5 (no OSMF gate, same .wxs syntax):"
    Write-Host "      dotnet tool uninstall --global wix"
    Write-Host "      dotnet tool install   --global wix --version 5.0.2"
    throw "wix build failed (exit code $LASTEXITCODE)."
}

Write-Host "==> Done: $Output"
