# Building the Windows MSI installer

This directory contains a [WiX Toolset](https://wixtoolset.org/) installer that
packages the Trino ODBC driver as a Windows `.msi`. The MSI installs a single,
self-contained driver DLL into `C:\Program Files\TrinoODBC\bin` and registers it
with the Windows ODBC subsystem (so it appears in the **ODBC Data Source
Administrator**, `odbcad32.exe`).

curl and json-c are **statically linked** into `trino_odbc.dll` (via the vcpkg
`x64-windows-static-md` triplet), so there are no dependency DLLs to ship. The
`-md` triplet links the dependencies statically while keeping the **dynamic
Universal CRT**, which avoids CRT-mismatch issues and means end users only need
the standard VC++ runtime that ships with Windows.

## Prerequisites (on the Windows build machine)

| Tool | Notes |
|------|-------|
| Visual Studio 2022 Build Tools | MSVC C toolchain. Free "Build Tools" SKU is sufficient. |
| CMake | 3.21+ (for presets). |
| [vcpkg](https://vcpkg.io) | Provides `curl` and `json-c`. Set `VCPKG_ROOT`. |
| [WiX Toolset](https://wixtoolset.org) v4/v5 | `dotnet tool install --global wix` |

> The driver itself is plain C and links the system `odbc32.lib` / `odbccp32.lib`
> (handled automatically by `src/CMakeLists.txt` on Windows). The only external
> dependencies are libcurl and json-c, declared in the repo-root `vcpkg.json`.

## One-shot build

From the repository root, in a Developer PowerShell:

```powershell
$env:VCPKG_ROOT = "C:\path\to\vcpkg"
pwsh installer/build_msi.ps1
```

This produces `trino_odbc-x64.msi` in the repository root. It:

1. Configures with the `windows-x64` CMake preset (uses the vcpkg toolchain and
   `vcpkg.json` to fetch dependencies).
2. Builds `trino_odbc.dll` (Release, x64).
3. Copies the vcpkg runtime DLLs next to the driver.
4. Runs `wix build` against `trino_odbc.wxs`.

## Manual steps (if you prefer to run them yourself)

```powershell
# 1. Configure + build the driver (static curl/json-c)
cmake --preset windows-x64
cmake --build --preset windows-x64

# 2. Build the MSI (single self-contained DLL; no dependency DLLs)
wix build installer/trino_odbc.wxs -arch x64 `
    -d "BinDir=build-windows/src/Release" `
    -o trino_odbc-x64.msi
```

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
