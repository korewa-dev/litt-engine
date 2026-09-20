param(
    [ValidateSet("debug", "release")]
    [string]$Config = "release"
)

$ErrorActionPreference = "Stop"
$NativeDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir = Join-Path $NativeDir "bin\windows\$Config"
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

$Opt = if ($Config -eq "debug") { @("/Od", "/Zi") } else { @("/O2") }
$Common = @("/nologo", "/std:c++17", "/EHsc", "/W4", "/I", (Join-Path $NativeDir "littcore"))

function Invoke-Cl {
    param([string[]]$Args)
    & cl @Args
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

Write-Host "[build] Litt native windows $Config"

$littJson = Join-Path $BuildDir "litt_json.obj"
$littObj = Join-Path $BuildDir "litt_obj.obj"
$littWorld = Join-Path $BuildDir "litt_world.obj"

Invoke-Cl (@("/nologo", "/TC", "/W4", "/O2", "/I", (Join-Path $NativeDir "littcore"),
    "/c", (Join-Path $NativeDir "littcore\litt_json.c"), "/Fo:$littJson"))
Invoke-Cl (@("/nologo", "/TC", "/W4", "/O2", "/I", (Join-Path $NativeDir "littcore"),
    "/c", (Join-Path $NativeDir "littcore\litt_obj.c"), "/Fo:$littObj"))
Invoke-Cl (@("/nologo", "/TC", "/W4", "/O2", "/I", (Join-Path $NativeDir "littcore"),
    "/c", (Join-Path $NativeDir "littcore\litt_world.c"), "/Fo:$littWorld"))

$littCli = Join-Path $BuildDir "littcli.exe"
Invoke-Cl (@("/nologo", "/TC", "/W4", "/O2", "/I", (Join-Path $NativeDir "littcore"),
    (Join-Path $NativeDir "littcli.c"), $littJson, $littObj, $littWorld, "/Fe:$littCli"))

$littView = Join-Path $BuildDir "littview.exe"
Invoke-Cl ($Common + $Opt + @(
    (Join-Path $NativeDir "littview.cpp"), $littJson, $littObj, $littWorld,
    "/Fe:$littView", "gdi32.lib", "user32.lib", "winmm.lib"))

$coreTests = Join-Path $BuildDir "littcore_tests.exe"
Invoke-Cl (@("/nologo", "/TC", "/W4", "/O2", "/I", (Join-Path $NativeDir "littcore"),
    (Join-Path $NativeDir "tests.c"), $littJson, $littObj, $littWorld, "/Fe:$coreTests"))
& $coreTests
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$stabilization = Join-Path $BuildDir "stabilization_tests.exe"
Invoke-Cl ($Common + $Opt + @(
    (Join-Path $NativeDir "littcore\litt_stabilization_tests.cpp"),
    "/Fe:$stabilization", "gdi32.lib", "user32.lib", "winmm.lib"))
& $stabilization
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$engineLifecycle = Join-Path $BuildDir "engine_lifecycle_tests.exe"
Invoke-Cl ($Common + $Opt + @(
    (Join-Path $NativeDir "littcore\litt_engine_lifecycle_tests.cpp"),
    (Join-Path $NativeDir "littcore\litt_math.cpp"),
    "/Fe:$engineLifecycle", "gdi32.lib", "user32.lib", "winmm.lib"))
& $engineLifecycle
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "[done] supported native tools and tests passed"
