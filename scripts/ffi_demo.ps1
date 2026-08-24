# Builds the Rust FFI library and runs the C++ consumer against a shipped game.
#
# Compiler resolution order:
#   1. llvm-mingw g++ (WinGet) - proven path, links the DLL directly
#   2. MSVC via vcvars64.bat   - requires the Windows 10 SDK headers/libs;
#                                the relocated VS at D:\Program Files\Program
#                                currently lacks them ("stddef.h not found")
param(
    [string]$Game = "kingsfall-hollow"
)
$ErrorActionPreference = "Stop"

$cargo = "C:\Users\roika\.cargo\bin\cargo.exe"
if (-not (Test-Path $cargo)) { $cargo = "cargo" }

& $cargo build -p litt-ffi --release
if ($LASTEXITCODE -ne 0) { throw "cargo build failed" }

$targetDir = @("target\x86_64-pc-windows-gnu\release", "target\release") |
    Where-Object { Test-Path (Join-Path $_ "litt_ffi.dll") } |
    Select-Object -First 1
if (-not $targetDir) { throw "litt_ffi build output not found" }

$gxx = Get-ChildItem "$env:LOCALAPPDATA\Microsoft\WinGet\Packages" -Recurse -Depth 4 `
        -Filter 'x86_64-w64-mingw32-g++.exe' -ErrorAction SilentlyContinue |
       Select-Object -First 1 -ExpandProperty FullName

$scene = "Project\$Game\assets\scenes\world.lscn.json"
$assets = "Project\$Game\assets"

if ($gxx) {
    # llvm-mingw links the DLL directly (no import lib needed)
    & $gxx -std=c++17 -I include examples\cpp\load_world.cpp `
        "$targetDir\litt_ffi.dll" -o "$targetDir\litt_cpp_demo.exe"
    if ($LASTEXITCODE -ne 0) { throw "g++ compile failed" }
} else {
    # MSVC fallback: needs Windows SDK installed
    $vcvars = "D:\Program Files\Program\VC\Auxiliary\Build\vcvars64.bat"
    if (-not (Test-Path $vcvars)) { throw "no compiler found" }
    $bat = [IO.Path]::GetTempFileName() + ".cmd"
    @"
@echo off
call "$vcvars" >nul
cl /nologo /EHsc /std:c++17 /I include examples\cpp\load_world.cpp ^
   /Fe:$targetDir\litt_cpp_demo.exe ^
   /link /LIBPATH:$targetDir litt_ffi.lib user32.lib gdi32.lib shell32.lib ^
   advapi32.lib ws2_32.lib bcrypt.lib ntdll.lib ole32.lib oleaut32.lib kernel32.lib
"@ | Set-Content $bat -Encoding ascii
    & $bat
    Remove-Item $bat -ErrorAction SilentlyContinue
}

$env:PATH = "$(Resolve-Path $targetDir);$env:PATH"
& ".\$targetDir\litt_cpp_demo.exe" $scene $assets
exit $LASTEXITCODE
