# build-all.ps1 - Windows-native builder for the Litt live viewer.
# Usage:  .\build-all.ps1            # build live.exe (auto toolchain)
#         .\build-all.ps1 linux      # cross to linux (zigbuild or WSL)
#         .\build-all.ps1 android    # .so via NDK

param([string]$Target = "windows")
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $here
$cargo = Join-Path $env:USERPROFILE ".cargo\bin\cargo.exe"
if (-not (Test-Path $cargo)) { $cargo = "cargo" }

function Resolve-BuiltBinary {
    $candidates = @(
        "target\x86_64-pc-windows-gnu\release\live.exe",
        "target\x86_64-pc-windows-msvc\release\live.exe",
        "target\release\live.exe"
    )
    foreach ($c in $candidates) { if (Test-Path $c) { return $c } }
    return $null
}

switch ($Target.ToLower()) {
  "windows" {
    # Engine repo pins gnu+mingw via root .cargo/config.toml; fall back to MSVC.
    $hasGnuLinker = (Get-Command x86_64-w64-mingw32-gcc -ErrorAction SilentlyContinue) -ne $null
    if ($hasGnuLinker) {
      & $cargo build --release
    } elseif ((& rustup target list --installed) -contains "x86_64-pc-windows-msvc") {
      Write-Host '[..] mingw linker absent -> building for msvc' -ForegroundColor Yellow
      & $cargo build --release --target x86_64-pc-windows-msvc
    } else {
      & $cargo build --release   # last try: whatever default exists
    }
    $bin = Resolve-BuiltBinary
    if (-not $bin) { Write-Host "[fail] binary not found under target/" -ForegroundColor Red; exit 1 }
    Copy-Item $bin .\live.exe -Force
    Write-Host ("[ok] .\live.exe  " + [math]::Round((Get-Item .\live.exe).Length/1KB,0) + " KB") -ForegroundColor Green
  }
  "linux" {
    if (Get-Command cargo-zigbuild -ErrorAction SilentlyContinue) {
      rustup target add x86_64-unknown-linux-gnu 2>$null
      & $cargo zigbuild --release --target x86_64-unknown-linux-gnu
      Copy-Item target\x86_64-unknown-linux-gnu\release\live .\live-linux-x64 -Force
      Write-Host "[ok] .\live-linux-x64" -ForegroundColor Green
    }
    elseif (Get-Command wsl -ErrorAction SilentlyContinue) {
      Write-Host '[..] building inside WSL...'
      wsl bash -lc ('cd ' + (wslpath $here) + ' && bash ./build-all.sh')
    }
    else {
      Write-Host '[skip] linux: install zig+cargo-zigbuild, or use WSL.' -ForegroundColor Yellow
    }
  }
  "android" {
    # Termux users: just run ./build-all.sh ON the device instead (native build).
    # This path is for shipping a .so inside an app: needs ANDROID_NDK_HOME.
    $ndk = $env:ANDROID_NDK_HOME
    if (-not $ndk) { Write-Host '[skip] android: set ANDROID_NDK_HOME (or build natively in Termux).' -ForegroundColor Yellow; break }
    foreach ($arch in @('aarch64-linux-android','armv7-linux-androideabi','x86_64-linux-android')) {
      rustup target add $arch 2>$null
      $filter = "*" + $arch + "*-clang.cmd"
      $linker = Get-ChildItem (Join-Path $ndk "toolchains\llvm\prebuilt") -Recurse -Filter $filter | Select-Object -First 1 -ExpandProperty FullName
      if (-not $linker) { Write-Host ('[skip] no NDK linker for ' + $arch); continue }
      $envVar = "CARGO_TARGET_" + ($arch.ToUpper() -replace "-","_") + "_LINKER"
      Set-Item -Path ("Env:" + $envVar) -Value $linker
      & $cargo build --release --target $arch
      Write-Host ("[ok] liblive.so -> target\" + $arch + "\release\") -ForegroundColor Green
    }
    Write-Host 'NOTE: app embedding needs [lib] crate-type=["cdylib"] + JNI shim (GUI_INSTRUCTIONS.md section 4).'
  }
  default { Write-Host ("unknown target: " + $Target) }
}