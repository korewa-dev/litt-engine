#!/usr/bin/env pwsh
$ErrorActionPreference = "Continue"
$scriptDir = $PSScriptRoot
$nativeDir = Join-Path $scriptDir "native"
$worldgenDir = Join-Path $scriptDir "template\tools\worldgen"
$assetsDir = Join-Path $scriptDir "template\tools\assets"
$mingwBin = "C:\Users\roika\AppData\Local\Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\llvm-mingw-20260616-ucrt-x86_64\bin"
$suites = @("cpp_tests", "c_tests", "gen_props", "worldkit", "forge", "planner", "selftest")
$iterCounts = @{}; $iterFails = @{}; $failDetails = @{}
foreach ($s in $suites) { $iterCounts[$s] = 0; $iterFails[$s] = 0; $failDetails[$s] = @() }
$env:PATH = "$mingwBin;$env:PATH"
function CleanupNative {
    Get-ChildItem "$nativeDir\tests_c.exe" -EA SilentlyContinue | Remove-Item -Force
    Get-ChildItem "$nativeDir\tests_cpp.exe" -EA SilentlyContinue | Remove-Item -Force
    Get-ChildItem "$nativeDir\*.o" -EA SilentlyContinue | Remove-Item -Force
    Get-ChildItem "$nativeDir\t_*.json" -EA SilentlyContinue | Remove-Item -Force
    Get-ChildItem "$nativeDir\t_*.obj" -EA SilentlyContinue | Remove-Item -Force
}
function Run-CppTests {
    Push-Location $nativeDir
    try {
        CleanupNative
        $err = g++ -std=c++17 -I. -o tests_cpp.exe tests.cpp 2>&1
        if ($LASTEXITCODE -ne 0) { return @($false, "cpp compile") }
        $out = .\tests_cpp.exe 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0) { return @($false, "cpp exit:$LASTEXITCODE") }
        return @($true, "")
    } finally { Pop-Location; CleanupNative }
}
function Run-CTests {
    Push-Location $nativeDir
    try {
        CleanupNative
        $cf = "-std=c11 -O2 -Wall -Wextra -I."
        gcc $cf -c tests.c -o tests_main.o 2>$null
        gcc $cf -c littcore/litt_json.c -o litt_json.o 2>$null
        gcc $cf -c littcore/litt_obj.c -o litt_obj.o 2>$null
        gcc $cf -c littcore/litt_world.c -o litt_world.o 2>$null
        if ($LASTEXITCODE -ne 0) { return @($false, "c compile") }
        gcc litt_json.o litt_obj.o litt_world.o tests_main.o -o tests_c.exe 2>$null
        if ($LASTEXITCODE -ne 0) { return @($false, "c link") }
        $out = .\tests_c.exe 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0) { return @($false, "c exit:$LASTEXITCODE") }
        return @($true, "")
    } finally { Pop-Location; CleanupNative }
}
function Run-PythonTest($script, $label) {
    $dir = Split-Path $script -Parent
    $fname = Split-Path $script -Leaf
    Push-Location $dir
    try {
        $out = python $fname 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0) { return @($false, "$label exit:$LASTEXITCODE") }
        return @($true, "")
    } finally { Pop-Location }
}
function MarkResult($key, $iter, $ok, $msg) {
    $iterCounts[$key]++
    if (-not $ok) {
        $iterFails[$key]++
        $failDetails[$key] = $failDetails[$key] + @("Iter${iter}: ${msg}")
    }
}
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Litt Engine 100-Iteration Test Suite" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
for ($iter = 1; $iter -le 100; $iter++) {
    Write-Host ("Iter " + $iter + "/100") -NoNewline -ForegroundColor Yellow
    $r = Run-CppTests; MarkResult "cpp_tests" $iter $r[0] $r[1]
    if ($r[0]) { Write-Host " CPP OK" -ForegroundColor Green } else { Write-Host " CPP FAIL" -ForegroundColor Red }
    $r = Run-CTests; MarkResult "c_tests" $iter $r[0] $r[1]
    if ($r[0]) { Write-Host " C OK" -ForegroundColor Green } else { Write-Host " C FAIL" -ForegroundColor Red }
    $pyTests = @{"gen_props"="$worldgenDir\test_gen_props.py"; "worldkit"="$worldgenDir\test_worldkit.py"; "forge"="$worldgenDir\test_world_forge.py"; "planner"="$worldgenDir\test_world_planner.py"}
    foreach ($entry in $pyTests.GetEnumerator()) {
        $r = Run-PythonTest $entry.Value $entry.Key; MarkResult $entry.Key $iter $r[0] $r[1]
        if ($r[0]) { Write-Host (" " + $entry.Key.ToUpper() + " OK") -ForegroundColor Green } else { Write-Host (" " + $entry.Key.ToUpper() + " FAIL") -ForegroundColor Red }
    }
    $r = Run-PythonTest "$assetsDir\selftest.py" "selftest"; MarkResult "selftest" $iter $r[0] $r[1]
    if ($r[0]) { Write-Host " SELTEST OK" -ForegroundColor Green } else { Write-Host " SELTEST FAIL" -ForegroundColor Red }
    Write-Host ""
}
Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  FINAL RESULTS" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
$totalFailures = 0
$report = @(@{name="C++ Tests";key="cpp_tests"},@{name="C Tests";key="c_tests"},@{name="Gen Props";key="gen_props"},@{name="WorldKit";key="worldkit"},@{name="World Forge";key="forge"},@{name="World Planner";key="planner"},@{name="Asset Selftest";key="selftest"})
foreach ($r in $report) {
    $fails = $iterFails[$r.key]; $totalFailures += $fails
    $status = if ($fails -eq 0) { "ALL PASS" } else { ($fails.ToString() + " FAILURES") }
    $color = if ($fails -eq 0) { "Green" } else { "Red" }
    Write-Host ("  {0,-20} {1,4}/100 runs  {2}" -f $r.name, $iterCounts[$r.key], $status) -ForegroundColor $color
}
Write-Host ""
Write-Host ("  TOTAL FAILURES: " + $totalFailures) -ForegroundColor $(if ($totalFailures -eq 0) { "Green" } else { "Red" })
if ($totalFailures -gt 0) {
    Write-Host ""
    Write-Host "  FAILURE DETAILS:" -ForegroundColor Red
    foreach ($key in $failDetails.Keys) {
        if ($failDetails[$key].Count -gt 0) {
            Write-Host ""
            Write-Host ("  [" + $key + "] " + $failDetails[$key].Count + " failure(s):") -ForegroundColor Red
            foreach ($detail in $failDetails[$key] | Select-Object -First 10) { Write-Host ("    " + $detail) -ForegroundColor DarkRed }
        }
    }
}
exit $totalFailures