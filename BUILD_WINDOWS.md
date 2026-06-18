# Building the Trino ODBC Driver for Windows

The Windows build produces a single, self-contained 64-bit driver DLL
(`trino_odbc.dll`) with curl, json-c, and the MSVC C runtime all statically
linked — so there are no external runtime dependencies and no VC++
redistributable is required on target machines. The same DLL also contains the
**setup GUI** (the `ConfigDSN` dialog shown by the ODBC Data Source
Administrator).

For producing a distributable installer, see
[`installer/README.md`](installer/README.md) — that is the recommended path.
This document covers building the driver DLL directly and the manual
registration details.

## Prerequisites

| Tool | Notes |
|------|-------|
| Visual Studio Build Tools (2022 or newer) | "Desktop development with C++" workload (MSVC C toolchain + Ninja). The free Build Tools SKU is sufficient. Newer VS versions (2026, ...) also work because the build uses the Ninja generator, not a VS-pinned one. |
| CMake | 3.21+ (for presets). The one bundled with Visual Studio works. |
| [vcpkg](https://vcpkg.io) | Provides `curl` and `json-c`. Set the `VCPKG_ROOT` environment variable to its path. |
| Windows SDK | Provides the ODBC headers/libraries (`sql.h`, `odbcinst.h`, `odbc32.lib`, `odbccp32.lib`) and the resource compiler for the setup dialog. Included with the C++ workload. |

> **Use a Developer shell.** Run the build from an *x64 Native Tools Command
> Prompt for VS* or a *Developer PowerShell* so `cl.exe` and `ninja` are on
> `PATH`. `installer/build_msi.ps1` will auto-locate the toolchain via `vswhere`
> if you start from a plain shell, but a Developer shell is the reliable choice.

## Build the driver DLL

From the repository root, in a Developer shell:

```powershell
$env:VCPKG_ROOT = "C:\path\to\vcpkg"

cmake --preset windows-x64
cmake --build --preset windows-x64
```

The driver DLL is produced at:

```
build-windows\src\trino_odbc.dll
```

The `windows-x64` preset uses the Ninja generator, the vcpkg toolchain
(`x64-windows-static` triplet), and statically links the MSVC CRT
(`TRINO_ODBC_STATIC_CRT=ON`).

### Build the MSI

```powershell
pwsh installer/build_msi.ps1   # produces trino_odbc-x64.msi in the repo root
```

See [`installer/README.md`](installer/README.md) for full details, code-signing,
and troubleshooting.

## Installing without the MSI

Installing the MSI is the recommended approach (it registers the driver and the
setup DLL correctly and is uninstall-safe). To install manually for development:

1. Copy `trino_odbc.dll` to `C:\Program Files\TrinoODBC\bin\`.
2. Register it in the 64-bit ODBC registry. The driver DLL serves as **both** the
   driver and the setup (GUI) DLL, so `Driver` and `Setup` both point at it.

`windows/install.reg` contains ready-to-import entries:

```reg
[HKEY_LOCAL_MACHINE\SOFTWARE\ODBC\ODBCINST.INI\ODBC Drivers]
"Trino ODBC Driver"="Installed"

[HKEY_LOCAL_MACHINE\SOFTWARE\ODBC\ODBCINST.INI\Trino ODBC Driver]
"Driver"="C:\\Program Files\\TrinoODBC\\bin\\trino_odbc.dll"
"Setup"="C:\\Program Files\\TrinoODBC\\bin\\trino_odbc.dll"
"APILevel"="1"
"ConnectFunctions"="YYN"
"DriverODBCVer"="03.80"
"SQLLevel"="1"
```

```cmd
regedit /s windows\install.reg
```

> The `Setup` value pointing at `trino_odbc.dll` is what makes the **Configure**
> button in the ODBC Data Source Administrator open the Trino setup dialog. The
> dialog's `ConfigDSN`/`ConfigDSNW` entry points live in the driver DLL.

## Verifying the install

1. Open **ODBC Data Source Administrator (64-bit)** (`C:\Windows\System32\odbcad32.exe`).
2. **Drivers** tab → confirm *Trino ODBC Driver* is listed.
3. **System DSN** (or **User DSN**) → **Add** → select *Trino ODBC Driver*.
4. The Trino setup dialog appears. Fill in Server/Port/Catalog/Schema/User/etc.,
   click **Test Connection**, then **OK** to save the DSN.
5. Connect from any ODBC client. For example, from PowerShell/.NET:

   ```powershell
   $conn = New-Object System.Data.Odbc.OdbcConnection "DSN=YourDsnName"
   $conn.Open()
   $cmd = $conn.CreateCommand()
   $cmd.CommandText = "SELECT 1"
   $cmd.ExecuteScalar()
   $conn.Close()
   ```

## Troubleshooting

| Symptom | Cause / fix |
|---------|-------------|
| Driver missing from the Administrator | Check the `ODBCINST.INI` registry entries (64-bit view) and that the DLL path is correct. |
| `system error code 126: The specified module could not be found` during registration | The DLL has an unresolved dependency. The static build should be self-contained; ensure you rebuilt after pulling the static-CRT change. Diagnose with `dumpbin /dependents trino_odbc.dll`. As a stopgap, install the VC++ x64 redistributable. |
| Configure button does nothing | The `Setup` registry value must point at `trino_odbc.dll`. |
| Want a call trace | Set `TRINO_ODBC_LOG` to a writable file path before launching the client (or `odbcad32.exe`), then inspect the log. |

Cross-compiling from Linux with MinGW-w64 is **not** supported (header
conflicts); build on Windows with MSVC.
