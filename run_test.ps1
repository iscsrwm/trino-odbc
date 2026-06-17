# Test script for verifying ODBC driver Unicode support
# This script compiles and runs a simple C test program

param(
    [string]$Password = "YOUR_PASSWORD"
)

Write-Host "=== Trino ODBC Test Script ===" -ForegroundColor Cyan
Write-Host ""

# Set logging
$env:TRINO_ODBC_LOG = "C:\Temp\trino_odbc.log"
Remove-Item C:\Temp\trino_odbc.log -ErrorAction SilentlyContinue

# Update password in test file if provided
if ($Password -ne "YOUR_PASSWORD") {
    Write-Host "Updating password in test_query.c..." -ForegroundColor Yellow
    $content = Get-Content test_query.c -Raw
    $content = $content -replace 'Password=YOUR_PASSWORD', "Password=$Password"
    Set-Content test_query.c -Value $content
}

# Find Visual Studio
Write-Host "Looking for Visual Studio..." -ForegroundColor Yellow
$vsPath = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" `
    -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath

if (-not $vsPath) {
    Write-Host "ERROR: Visual Studio not found" -ForegroundColor Red
    exit 1
}

Write-Host "Found: $vsPath" -ForegroundColor Green
Write-Host ""

# Load VS environment
$vcvarsPath = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $vcvarsPath)) {
    Write-Host "ERROR: vcvars64.bat not found" -ForegroundColor Red
    exit 1
}

# Compile the test program
Write-Host "Compiling test_query.c..." -ForegroundColor Yellow
cmd /c "`"$vcvarsPath`" && cl test_query.c odbc32.lib /Fe:test_query.exe 2>&1"

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Compilation failed" -ForegroundColor Red
    exit 1
}

Write-Host "Compilation successful" -ForegroundColor Green
Write-Host ""

# Run the test
Write-Host "Running test..." -ForegroundColor Yellow
Write-Host "----------------------------------------" -ForegroundColor Gray
.\test_query.exe
Write-Host "----------------------------------------" -ForegroundColor Gray
Write-Host ""

# Show the driver log
if (Test-Path C:\Temp\trino_odbc.log) {
    Write-Host "=== DRIVER LOG ===" -ForegroundColor Cyan
    Write-Host "----------------------------------------" -ForegroundColor Gray
    Get-Content C:\Temp\trino_odbc.log
    Write-Host "----------------------------------------" -ForegroundColor Gray
} else {
    Write-Host "WARNING: No driver log found" -ForegroundColor Yellow
}
