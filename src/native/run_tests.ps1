# Build and run Litt Engine tests (PowerShell, works on Windows)
$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

Write-Host "Building Litt Engine tests..."
g++ -std=c++17 -O2 -I. tests.cpp -o tests.exe
if ($LASTEXITCODE -ne 0) { Write-Error "Build failed"; exit 1 }

Write-Host "Running tests..."
& .\tests.exe
exit $LASTEXITCODE
