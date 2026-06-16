@echo off
setlocal

echo Installing Trino ODBC Driver...
echo.

rem Copy DLLs
copy /Y "%~dp0bin	rino_odbc.dll" "C:\Program Files\TrinoODBCin" 2>nul
if errorlevel 1 (
    echo Error: Could not copy trino_odbc.dll
    exit /b 1
)

echo DLL copied successfully.
echo.

rem Import registry entries
regedit /S "%~dp0install.reg"
if errorlevel 1 (
    echo Warning: Could not import registry entries. Please run manually as Administrator.
) else (
    echo Registry installed successfully.
)

echo.
echo Installation complete!
echo.
echo To configure a DSN, open ODBC Data Sources (odbcad32.exe)
echo and add a new DSN using "Trino ODBC Driver".
pause
