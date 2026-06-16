# Building Trino ODBC Driver for Windows

## Prerequisites on Windows

1. **Visual Studio Build Tools** (2022 or later)
   - Install with C++ workload
   - Include "MSVC v143" compiler

2. **CMake** (3.20+)

3. **Windows ODBC Driver Manager**
   - Comes with Windows SDK

4. **Dependencies**:
   - libcurl for Windows (precompiled binaries or build from source)
   - json-c for Windows

## Build Steps

### Option 1: Direct Build on Windows

```powershell
# Clone the repository
git clone <your-repo>
cd trino-odbc-driver

# Create build directory
mkdir build && cd build

# Configure with CMake (adjust paths as needed)
cmake -DCMAKE_GENERATOR="Visual Studio 17 2022" ^
      -DCMAKE_SYSTEM_VERSION=10.0.19041.0 ^
      ..

# Build
cmake --build . --config Release

# The DLL will be at:
# build\src\Release\trino_odbc.dll
```

### Option 2: Using MinGW-w64 on Linux (Cross-Compilation)

Due to header conflicts between MinGW-w64 and Linux system headers, 
this approach is not currently supported. Please use Option 1.

## Creating Windows Package

After building, create the following structure:

```
trino-odbc-windows-x64/
├── bin/
│   └── trino_odbc.dll
├── lib/
│   ├── libcurl.lib (if statically linking)
│   └── libjson-c.lib (if statically linking)
├── etc/
│   ├── odbcinst.ini
│   └── odbc.ini
├── doc/
│   ├── README.md
│   └── LICENSE
└── install.bat
```

## ODBC Configuration Files

### odbcinst.ini
```ini
[ODBC Drivers]
Trino ODBC Driver=Installed

[Trino ODBC Driver]
Driver=C:\Program Files\TrinoODBC\bin\trino_odbc.dll
Setup=C:\Program Files\TrinoODBC\bin\trino_odbc_setup.dll
APILevel=1
ConnectFunctions=YYN
DriverODBCVer=03.51
SQLLevel=1
```

### odbc.ini
```ini
[TrinoDSN]
Driver=Trino ODBC Driver
Host=your-trino-server.com
Port=443
Schema=default
Catalog=trino
AuthMech=3
SSL=1
```

## Registry Setup

On Windows, the driver needs to be registered:

### 64-bit registry (HKLM\SOFTWARE\ODBC\ODBCINST.INI)
```reg
[HKEY_LOCAL_MACHINE\SOFTWARE\ODBC\ODBCINST.INI\ODBC Drivers]
"Trino ODBC Driver"="Installed"

[HKEY_LOCAL_MACHINE\SOFTWARE\ODBC\ODBCINST.INI\Trino ODBC Driver]
"Driver"="C:\Program Files\TrinoODBC\bin\trino_odbc.dll"
"APILevel"=dword:00000001
"ConnectFunctions"="YYN"
"DriverODBCVer"="03.51"
"SQLLevel"=dword:00000001
```

### 32-bit registry (HKLM\SOFTWARE\WOW6432Node\ODBC\ODBCINST.INI)
Same entries for 32-bit compatibility.

## Installation Script Example

```batch
@echo off
setlocal

copy bin\trino_odbc.dll "C:\Program Files\TrinoODBC\bin\" /Y
copy etc\odbcinst.ini "%WINDIR%\System32\" /Y
copy etc\odbc.ini "%WINDIR%\System32\" /Y

regedit /s install.reg

echo Installation complete!
```
