# VERITY GRAPH build script (Windows PowerShell).
# Usage: powershell -ExecutionPolicy Bypass -File scripts\build.ps1 [-Sanitize] [-Werror] [-Tests] [-Clean]
[CmdletBinding()]
param(
  [switch]$Sanitize,
  [switch]$Werror,
  [switch]$Tests,
  [switch]$Clean
)
$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RootDir   = Split-Path -Parent $ScriptDir
$BuildDir  = Join-Path $RootDir "build"

function Find-Cxx {
  if ($env:CXX -and (Get-Command $env:CXX -ErrorAction SilentlyContinue)) { return (Get-Command $env:CXX).Source }
  foreach ($c in @("clang++","g++")) {
    $cmd = Get-Command $c -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
  }
  foreach ($p in @("C:\Program Files\LLVM\bin\clang++.exe", "C:\msys64\mingw64\bin\g++.exe")) {
    if (Test-Path $p) { return $p }
  }
  return $null
}

$Cxx = Find-Cxx
if (-not $Cxx) { Write-Error "No C++ compiler found (clang++ or g++ required)."; exit 1 }
Write-Host "==> Compiler: $Cxx"

# Windows+clang needs an RC compiler; prefer llvm-rc next to clang++.
$RcArgs = @()
if ($Cxx -like "*clang++*") {
  $RcBin = $null
  $cmd = Get-Command llvm-rc -ErrorAction SilentlyContinue
  if ($cmd) { $RcBin = $cmd.Source }
  if (-not $RcBin) {
    foreach ($cand in @((Join-Path (Split-Path -Parent $Cxx) "llvm-rc.exe"))) {
      if (Test-Path $cand) { $RcBin = $cand; break }
    }
  }
  if ($RcBin) { $RcArgs += "-DCMAKE_RC_COMPILER=$RcBin" }
}

if ($Clean -and (Test-Path $BuildDir)) { Remove-Item -Recurse -Force $BuildDir }
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

Write-Host "==> Configuring (CMake + Ninja)"
& cmake -S $RootDir -B $BuildDir -G Ninja `
  -DCMAKE_CXX_COMPILER="$Cxx" `
  @RcArgs `
  -DCMAKE_BUILD_TYPE=Release `
  -DVERITY_SANITIZE=$(if ($Sanitize) {"ON"} else {"OFF"}) `
  -DVERITY_WERROR=$(if ($Werror) {"ON"} else {"OFF"})
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "==> Building"
& cmake --build $BuildDir
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if ($Tests) {
  Write-Host "==> Running tests"
  & ctest --test-dir $BuildDir --output-on-failure
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
Write-Host "==> Done. CLI: $BuildDir\verity.exe   Tests: $BuildDir\verity_tests.exe"
