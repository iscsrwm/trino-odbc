# Building the Windows MSI installer

This directory contains a [WiX Toolset](https://wixtoolset.org/) installer that
packages the Trino ODBC driver as a Windows `.msi`. The MSI installs a single,
self-contained driver DLL into `C:\Program Files\TrinoODBC\bin` and registers it
with the Windows ODBC subsystem (so it appears in the **ODBC Data Source
Administrator**, `odbcad32.exe`).

curl and json-c are **statically linked** into `trino_odbc.dll` (via the vcpkg
`x64-windows-static` triplet), and the **MSVC C runtime is also statically
linked** (`TRINO_ODBC_STATIC_CRT=ON`). The result is a fully self-contained
driver DLL with **no external runtime dependencies** - no dependency DLLs and no
VC++ redistributable required on target machines. This matters for an ODBC
driver because the driver manager loads the DLL in arbitrary host processes; a
missing dependency surfaces as the opaque "system error code 126 / could not be
found" during driver registration.

## Prerequisites (on the Windows build machine)

| Tool | Notes |
|------|-------|
| Visual Studio Build Tools (2022 or newer) | MSVC C toolchain + Ninja (included with the "Desktop development with C++" workload). The free "Build Tools" SKU is sufficient. |
| CMake | 3.21+ (for presets). The one bundled with Visual Studio works. |
| [vcpkg](https://vcpkg.io) | Provides `curl` and `json-c`. Set `VCPKG_ROOT`. |
| [WiX Toolset](https://wixtoolset.org) v4/v5 | `dotnet tool install --global wix --version 5.0.2`. Avoid v7 unless you've accepted its OSMF EULA (see troubleshooting). |

> The driver itself is plain C and links the system `odbc32.lib` / `odbccp32.lib`
> (handled automatically by `src/CMakeLists.txt` on Windows). The only external
> dependencies are libcurl and json-c, declared in the repo-root `vcpkg.json`
> (pinned to a `builtin-baseline` so vcpkg resolves them reproducibly).

> **Use a Developer shell.** The `windows-x64` preset uses the **Ninja**
> generator and picks up `cl.exe` from the surrounding environment, so it works
> with any Visual Studio version. You must run the build from an
> **"x64 Native Tools Command Prompt"** or **"Developer PowerShell"** (Start menu,
> under Visual Studio) so the MSVC compiler is on `PATH`. A plain PowerShell will
> fail to find the compiler.

## One-shot build

From the repository root, in an **x64 Native Tools / Developer PowerShell**:

```powershell
$env:VCPKG_ROOT = "C:\path\to\vcpkg"
pwsh installer/build_msi.ps1
```

This produces `trino_odbc-x64.msi` in the repository root. It:

1. Configures with the `windows-x64` CMake preset (Ninja + the vcpkg toolchain;
   `vcpkg.json` fetches the dependencies).
2. Builds `trino_odbc.dll` (Release, x64, with curl/json-c statically linked).
3. Runs `wix build` against `trino_odbc.wxs`.

## Manual steps (if you prefer to run them yourself)

```powershell
# 1. Configure + build the driver (static curl/json-c)
cmake --preset windows-x64
cmake --build --preset windows-x64

# 2. Build the MSI (single self-contained DLL; no dependency DLLs).
#    With the Ninja generator the DLL is in build-windows\src.
wix build installer/trino_odbc.wxs -arch x64 `
    -d "BinDir=build-windows/src" `
    -o trino_odbc-x64.msi
```

> The one-shot `build_msi.ps1` locates the DLL automatically, so it works
> whether you build with Ninja (`build-windows\src\`) or a multi-config Visual
> Studio generator (`build-windows\src\Release\`).

## How driver registration works

The `.wxs` uses WiX's `<ODBCDriver>` element rather than writing registry keys
directly. WiX calls the ODBC installer API (`SQLInstallDriverEx`), which is the
supported, uninstall-safe way to register an ODBC driver and correctly handles
the 64-bit vs. 32-bit (WoW64) registry views. This replaces the manual
`windows/install.reg` approach for end users.

Driver attributes registered:

| Attribute | Value |
|-----------|-------|
| `APILevel` | 1 |
| `ConnectFunctions` | YYN |
| `DriverODBCVer` | 03.80 |
| `SQLLevel` | 1 |

## Dependencies

The build statically links curl and json-c into `trino_odbc.dll` using the
vcpkg `x64-windows-static-md` triplet (selected by the `windows-x64` CMake
preset, which also sets `-DTRINO_ODBC_STATIC_DEPS=ON`). The MSI therefore ships
a single self-contained DLL with no dependency DLLs.

If you ever want **dynamic** dependencies instead, configure with
`-DTRINO_ODBC_STATIC_DEPS=OFF` and the `x64-windows` triplet, then add `File`
components for the runtime DLLs to `trino_odbc.wxs` (or use `wix harvest`).

## Code signing (recommended for distribution)

Unsigned MSIs trigger SmartScreen/UAC warnings. Sign both the DLL and the MSI
with an Authenticode certificate:

```powershell
signtool sign /fd SHA256 /a /tr http://timestamp.digicert.com /td SHA256 `
    build-windows\src\Release\trino_odbc.dll
signtool sign /fd SHA256 /a /tr http://timestamp.digicert.com /td SHA256 `
    trino_odbc-x64.msi
```

## Troubleshooting

| Symptom | Cause / fix |
|---------|-------------|
| `dotnet tool install --global wix` → *No NuGet sources are defined or enabled* | NuGet's default source is missing/disabled. `dotnet nuget add source https://api.nuget.org/v3/index.json --name nuget.org` (or `dotnet nuget enable source nuget.org`), then retry. |
| `this vcpkg instance requires a manifest with a specified baseline` | `vcpkg.json` must contain `builtin-baseline` (it does). If you changed it, re-add a baseline commit, or run `vcpkg x-update-baseline` in the repo root. |
| `error MSB8020: build tools for Visual Studio 2022 (Platform Toolset 'v143') cannot be found` | You hit the old VS-pinned generator. The preset now uses **Ninja**, which works with any VS version. Make sure you have the latest `CMakePresets.json` and run from an **x64 Native Tools / Developer** shell. |
| `cl.exe` / compiler not found at configure time | You're not in a Developer shell. Launch "x64 Native Tools Command Prompt for VS" (or "Developer PowerShell"), then `powershell` if you want PowerShell. |
| vcpkg can't download (proxy/firewall) | Set `HTTP_PROXY` / `HTTPS_PROXY`, or pre-install: `vcpkg install curl json-c --triplet x64-windows-static-md`. |
| `trino_odbc.dll not found under build-windows\src` | The compile step failed earlier - scroll up for the MSVC/CMake error. |
| `WIX7015: You must accept the Open Source Maintenance Fee (OSMF) EULA` | WiX **v7** gates builds behind the OSMF EULA. Either set `$env:WIX_ACCEPT_OSMF_EULA = "1"` (after reviewing https://wixtoolset.org/osmf/), or use WiX **v5** which has no such gate: `dotnet tool install --global wix --version 5.0.2`. The `.wxs` works unchanged on v4/v5/v7. |
| Install fails: `system error code 126: The specified module could not be found (...trino_odbc.dll)` | The driver DLL has an unresolved dependency. The build statically links curl/json-c **and** the MSVC CRT, so a freshly-built MSI should be self-contained; ensure you rebuilt after pulling the static-CRT change. To diagnose a DLL's dependencies: `dumpbin /dependents "C:\Program Files\TrinoODBC\bin\trino_odbc.dll"` (look for non-system DLLs). As a stopgap on the failing machine, install the VC++ x64 redistributable: https://aka.ms/vs/17/release/vc_redist.x64.exe |

## CI

`.gitlab-ci.yml` defines a `windows-msi` job (manual, tag-triggered) that runs
these steps on a Windows runner and publishes the MSI as a pipeline artifact.
It requires a Windows runner tagged `windows` to be available to the project.

## Testing the installed driver

After installing the MSI on a Windows machine:

1. Open **ODBC Data Source Administrator (64-bit)** → **Drivers** tab → confirm
   "Trino ODBC Driver" is listed.
2. **System DSN** → **Add** → select the driver → configure a connection string
   (e.g. `Server=...;Port=8080;Catalog=tpch;Schema=tiny`).
3. Connect from any ODBC client (Excel, Power BI, `pyodbc`, etc.).
