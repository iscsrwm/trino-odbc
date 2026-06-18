@echo off
setlocal

echo Installing Trino ODBC Driver...
echo.

rem Ensure the install directory exists
if not exist "C:\Program Files\TrinoODBC\bin" (
    mkdir "C:\Program Files\TrinoODBC\bin"
)

rem Copy the driver DLL (also serves as the setup/GUI DLL)
copy /Y "%~dp0bin\trino_odbc.dll" "C:\Program Files\TrinoODBC\bin\" 2>nul
if errorlevel 1 (
    echo Error: Could not copy trino_odbc.dll
    echo Build it first ^(see ..\BUILD_WINDOWS.md^) and place it in this folder's bin\ directory.
    exit /b 1
)

echo DLL copied successfully.
echo.

rem Import registry entries (Driver + Setup point at trino_odbc.dll)
regedit /S "%~dp0install.reg"
if errorlevel 1 (
    echo Warning: Could not import registry entries. Please run as Administrator.
) else (
    echo Registry installed successfully.
)

echo.
echo Installation complete!
echo.
echo To configure a DSN, open the 64-bit ODBC Data Source Administrator
echo (C:\Windows\System32\odbcad32.exe), go to System DSN, click Add, and
echo select "Trino ODBC Driver".
pause
