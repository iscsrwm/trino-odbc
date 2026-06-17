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

# --------------------------------------------------------------------------
# Corporate TLS-inspection proxies present a self-signed root CA that git and
# vcpkg's curl reject ("SSL certificate ... self-signed certificate in
# certificate chain"). That breaks the vcpkg registry fetch (git) and the
# dependency install, so configure aborts before anything is built. Disable
# TLS verification for the network tools used during the build.
#
# INSECURE: this skips certificate validation and is only appropriate on a
# machine that sits behind a trusted corporate proxy. GIT_SSL_NO_VERIFY is
# process-scoped (it does not mutate the machine-wide git config), and
# VCPKG_KEEP_ENV_VARS ensures vcpkg forwards it to the git subprocesses it
# spawns to fetch its registry.
# --------------------------------------------------------------------------
Write-Warning "TLS certificate verification is DISABLED for git/vcpkg during this build (corporate proxy workaround)."
$env:GIT_SSL_NO_VERIFY   = "true"
$env:VCPKG_KEEP_ENV_VARS = "GIT_SSL_NO_VERIFY"

# Ensure the MSVC build environment is loaded (cl.exe + ninja on PATH). If not,
# locate Visual Studio with vswhere and import its developer environment so this
# works from a plain PowerShell too, not only an "x64 Native Tools" shell.
if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    Write-Host "==> MSVC not on PATH; loading Visual Studio developer environment"
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        throw "Could not find vswhere. Open an 'x64 Native Tools Command Prompt for VS' and re-run, or install Visual Studio Build Tools."
    }
    $vsPath = & $vswhere -latest -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath
    if (-not $vsPath) { throw "No Visual Studio with the C++ toolset was found." }

    # Import the dev environment from VsDevCmd into this PowerShell session.
    $devCmd = Join-Path $vsPath "Common7\Tools\VsDevCmd.bat"
    cmd /c "`"$devCmd`" -arch=x64 -host_arch=x64 && set" | ForEach-Object {
        if ($_ -match '^(.*?)=(.*)$') { Set-Item -Path "env:$($matches[1])" -Value $matches[2] }
    }
    if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
        throw "Failed to load the MSVC environment (cl.exe still not found)."
    }
}

# Keep the vcpkg manifest baseline in sync with the local vcpkg checkout so
# dependency resolution does not fail with "<pkg> does not exist".
Write-Host "==> Updating vcpkg baseline"
& "$env:VCPKG_ROOT\vcpkg.exe" x-update-baseline 2>&1 | Out-Host

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

# Derive a monotonically increasing build version so each rebuilt MSI is seen
# as an upgrade by Windows Installer and always replaces the installed DLL.
# Format: 1.0.<days-since-2020>.<seconds-since-midnight/2> (fits 0-65535).
$now = Get-Date
$build  = [int]((New-TimeSpan -Start (Get-Date '2020-01-01') -End $now).TotalDays)
$revRaw = [int]($now.TimeOfDay.TotalSeconds / 2)
$msiVersion = "1.0.$build.$revRaw"
Write-Host "==> MSI version: $msiVersion"

Write-Host "==> Building MSI with WiX"
wix build installer/trino_odbc.wxs `
    -arch x64 `
    -d "BinDir=$binDir" `
    -d "Version=$msiVersion" `
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
