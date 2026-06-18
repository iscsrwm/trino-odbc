Trino ODBC Driver DLL for Windows

Place the built trino_odbc.dll in this directory (for the manual install flow),
or install the MSI which puts it in C:\Program Files\TrinoODBC\bin.

To build the DLL (from a Visual Studio "x64 Native Tools" / Developer shell at
the repository root, with VCPKG_ROOT set):

   cmake --preset windows-x64
   cmake --build --preset windows-x64

The compiled DLL will be at: build-windows\src\trino_odbc.dll

See ..\..\BUILD_WINDOWS.md and ..\..\installer\README.md for full details.
