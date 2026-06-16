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

# Copy the vcpkg runtime DLLs (curl, json-c and their transitive deps) next to
# the driver so they can be bundled. vcpkg places them under the installed tree.
$vcpkgBin = Join-Path $repoRoot "build-windows\vcpkg_installed\x64-windows\bin"
if (Test-Path $vcpkgBin) {
    Write-Host "==> Bundling runtime DLLs from $vcpkgBin"
    Copy-Item (Join-Path $vcpkgBin "*.dll") $binDir -Force
}

Write-Host "==> Building MSI with WiX"
$depsBundled = if (Test-Path (Join-Path $binDir "libcurl.dll")) { "true" } else { "false" }
wix build installer/trino_odbc.wxs `
    -arch x64 `
    -d "BinDir=$binDir" `
    -d "DEPS_BUNDLED=$depsBundled" `
    -o $Output

Write-Host "==> Done: $Output"
