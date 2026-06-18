# Trino ODBC Driver - Manual Windows Install

> **The recommended way to install on Windows is the MSI** — see
> [`../installer/README.md`](../installer/README.md). It builds a single
> self-contained DLL and registers the driver (and setup GUI) for you. The
> manual steps below are for development or environments where the MSI is not
> used.

## Files in this directory

```
windows/
├── etc/
│   ├── odbcinst.ini            # Driver registration (INI form)
│   └── odbc.ini                # Sample DSN configuration
├── install.bat                 # Automated installer script
├── install.reg                 # Registry entries for manual installation
└── README.md                   # this file
```

You build `trino_odbc.dll` yourself (see *Build from Source* below) and place it
at `C:\Program Files\TrinoODBC\bin\`.

> The single `trino_odbc.dll` is **both** the ODBC driver and the setup-GUI DLL;
> the registry entries point `Driver` and `Setup` at the same file.

## Prerequisites

- Windows 10/11 (64-bit)
- ODBC Driver Manager (included with Windows)

## Installation

### Option 1: Automated Installer (Recommended)

```cmd
# Run as Administrator
install.bat
```

### Option 2: Manual Installation

1. Copy `bin\trino_odbc.dll` to `C:\Program Files\TrinoODBC\bin\`
2. Import registry entries:
   ```cmd
   regedit /S install.reg
   ```

## Configuration

After installation:

1. Open **ODBC Data Source Administrator (64-bit)**:
   ```
   C:\Windows\System32\odbcad32.exe
   ```
   (The driver is 64-bit; the 32-bit administrator in `SysWOW64` will not show it.)

2. Go to the **System DSN** (or **User DSN**) tab and click **Add**.
3. Select **Trino ODBC Driver**. The Trino setup dialog opens.
4. Configure your connection:
   - **Server**: Your Trino coordinator hostname
   - **Port**: e.g. `443` (TLS) or `8080`
   - **Catalog** / **Schema**: e.g. `tpch` / `tiny`
   - **User** / **Password**
   - **Authentication**: `NONE`, `PASSWORD`, `CERTIFICATE`, or `KERBEROS`
   - **TLS/SSL**: enable and set certificate verification as appropriate
5. Click **Test Connection** to verify, then **OK** to save the DSN.

You can then connect using just the DSN name, e.g. `DSN=YourDsnName`.

## Build from Source

The recommended build is the MSI (single self-contained DLL). See
[`../installer/README.md`](../installer/README.md) and
[`../BUILD_WINDOWS.md`](../BUILD_WINDOWS.md).

In short, from a Developer shell at the repo root:

```powershell
$env:VCPKG_ROOT = "C:\path\to\vcpkg"
cmake --preset windows-x64
cmake --build --preset windows-x64
# DLL: build-windows\src\trino_odbc.dll
```

## Troubleshooting

### Driver not found
- Ensure the DLL is at `C:\Program Files\TrinoODBC\bin\trino_odbc.dll`
- Verify the `ODBCINST.INI` registry entries (64-bit view) are present
- Use the 64-bit `odbcad32.exe`

### Configure button does nothing
- The `Setup` registry value must point at `trino_odbc.dll` (the driver DLL also
  contains the setup GUI). The bundled `install.reg` sets this.

### Connection failures
- Check the Trino server is reachable from this machine
- Verify authentication credentials
- Ensure SSL settings match your Trino configuration
- Set `TRINO_ODBC_LOG` to a writable file path to capture a driver trace

## Support

For issues or questions, please refer to the main project documentation
([`../README.md`](../README.md)).
