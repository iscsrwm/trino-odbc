# Trino ODBC Driver - Windows Package

## Files Included

```
trino-odbc-windows-x64/
├── bin/
│   └── trino_odbc.dll          # ODBC driver DLL (requires compilation)
├── etc/
│   ├── odbcinst.ini            # Driver installation config
│   └── odbc.ini                # Sample DSN configuration
├── install.bat                 # Automated installer script
└── install.reg                 # Registry entries for manual installation
```

## Prerequisites

- Windows 7/8/10/11 (64-bit)
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

1. Open ODBC Data Sources (64-bit):
   ```
   C:\Windows\SysWOW64\odbcad32.exe  # 32-bit
   C:\Windows\System32\odbcad32.exe  # 64-bit
   ```

2. Go to "System DSN" tab and click "Add"
3. Select "Trino ODBC Driver"
4. Configure your connection:
   - **Host**: Your Trino server hostname
   - **Port**: Default `8080`
   - **Schema**: `default`
   - **Catalog**: `trino`
   - **Authentication**: Choose appropriate mechanism

## Build from Source

To build the DLL on Windows:

```powershell
cd trino-odbc-driver
mkdir build && cd build
cmake -DCMAKE_GENERATOR="Visual Studio 17 2022" ..
cmake --build . --config Release
```

The compiled `trino_odbc.dll` should be placed in the `bin/` folder.

## Troubleshooting

### Driver not found
- Ensure DLL is in the correct path: `C:\Program Files\TrinoODBC\bin\`
- Verify registry entries are imported correctly

### Connection failures
- Check Trino server is accessible
- Verify authentication credentials
- Ensure SSL settings match your Trino configuration

## Support

For issues or questions, please refer to the main project documentation.
