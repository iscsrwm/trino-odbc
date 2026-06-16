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

# CMake/Visual Studio places the Release output here.
$binDir = Join-Path $repoRoot "build-windows\src\Release"
if (-not (Test-Path (Join-Path $binDir "trino_odbc.dll"))) {
    throw "trino_odbc.dll not found in $binDir"
}

# curl and json-c are statically linked into trino_odbc.dll (the windows-x64
# preset uses the x64-windows-static-md vcpkg triplet), so there are no runtime
# dependency DLLs to bundle - the MSI ships a single self-contained driver DLL.

Write-Host "==> Building MSI with WiX"
wix build installer/trino_odbc.wxs `
    -arch x64 `
    -d "BinDir=$binDir" `
    -o $Output

Write-Host "==> Done: $Output"
