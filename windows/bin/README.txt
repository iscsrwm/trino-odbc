Trino ODBC Driver DLL for Windows

This DLL must be compiled separately using Visual Studio Build Tools.

To build:
1. Install Visual Studio 2022 (Community or Build Tools)
2. Clone this repository to your Windows machine
3. Run:
   cd trino-odbc-driver
   mkdir build && cd build
   cmake -DCMAKE_GENERATOR="Visual Studio 17 2022" ..
   cmake --build . --config Release

The compiled DLL will be at: build\src\Release\trino_odbc.dll
