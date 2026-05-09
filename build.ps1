#Requires -Version 5.1
# ==============================================================================
# NerouRuntime — Build Script (PowerShell)
#
# Usage:
#   .\build.ps1                    # incremental Release build
#   .\build.ps1 debug              # Debug build
#   .\build.ps1 clean              # clean + Release build
#   .\build.ps1 clean debug        # clean + Debug build
#   .\build.ps1 skip-config debug  # skip cmake configure, just compile
#
# Works from any PowerShell window — no "Developer PS" required.
# ==============================================================================

param(
    [Parameter(ValueFromRemainingArguments=$true)]
    [string[]]$Args = @()
)

# ── Parse args ────────────────────────────────────────────────────────────────
$BuildType   = "Release"
$CleanBuild  = $false
$SkipConfig  = $false

foreach ($a in $Args) {
    switch ($a.ToLower()) {
        "debug"       { $BuildType  = "Debug"   }
        "release"     { $BuildType  = "Release"  }
        "clean"       { $CleanBuild = $true       }
        "rebuild"     { $CleanBuild = $true       }
        "skip-config" { $SkipConfig = $true       }
    }
}

# ── Helpers ───────────────────────────────────────────────────────────────────
function Write-Step([int]$n, [int]$total, [string]$msg) {
    Write-Host ""
    Write-Host "  [$n/$total] $msg" -ForegroundColor Cyan
}
function Write-OK([string]$msg)   { Write-Host "        OK  $msg" -ForegroundColor Green  }
function Write-Warn([string]$msg) { Write-Host "      WARN  $msg" -ForegroundColor Yellow }
function Write-Fail([string]$msg) { Write-Host "      FAIL  $msg" -ForegroundColor Red    }
function Write-Banner([string]$msg, [string]$color = "Cyan") {
    $line = "=" * 54
    Write-Host ""
    Write-Host "  $line" -ForegroundColor $color
    Write-Host "    $msg"            -ForegroundColor $color
    Write-Host "  $line" -ForegroundColor $color
    Write-Host ""
}

$ScriptDir = $PSScriptRoot

# ── Step 1: Load MSVC environment ─────────────────────────────────────────────
Write-Banner "NerouRuntime Build System  ·  $BuildType"

Write-Step 1 5 "Locating Visual Studio toolchain..."

# Ordered search — project-local VS2026 first, then standard installs
$vcvarsCandidates = @(
    "D:\AppData\vsc2026\VC\Auxiliary\Build\vcvars64.bat",
    "$env:ProgramFiles\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat",
    "$env:ProgramFiles\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat",
    "$env:ProgramFiles\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat",
    "$env:ProgramFiles\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
)
if ($env:VCVARS64 -and (Test-Path $env:VCVARS64)) {
    $vcvarsCandidates = @($env:VCVARS64) + $vcvarsCandidates
}

$vcvarsPath = $vcvarsCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $vcvarsPath) {
    Write-Fail "Cannot locate vcvars64.bat. Set `$env:VCVARS64 to its full path."
    exit 1
}
Write-OK "vcvars64: $vcvarsPath"

# Load the MSVC environment into the current PS session
$envOutput = cmd.exe /c "`"$vcvarsPath`" > nul 2>&1 && set" 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Fail "vcvars64.bat exited with code $LASTEXITCODE"
    exit 1
}
foreach ($line in $envOutput) {
    if ($line -match "^([^=]+)=(.*)$") {
        [System.Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], "Process")
    }
}
Write-OK "MSVC x64 environment loaded."

# Verify cl.exe
$clExe = Get-Command cl.exe -ErrorAction SilentlyContinue
if (-not $clExe) {
    Write-Fail "cl.exe not found after loading vcvars64.bat. VS install may be corrupted."
    exit 1
}
Write-OK "cl.exe: $($clExe.Source)"

# Find cmake — VS bundles one; also check PATH
$cmakePaths = @(
    "D:\AppData\vsc2026\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
    "$env:ProgramFiles\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
    "$env:ProgramFiles\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
    "$env:ProgramFiles\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
)
$cmakeExe = Get-Command cmake.exe -ErrorAction SilentlyContinue
if (-not $cmakeExe) {
    $cmakeExe = $cmakePaths | Where-Object { Test-Path $_ } | Select-Object -First 1
    if ($cmakeExe) {
        # Add cmake dir to PATH for this session
        $cmakeDir = Split-Path $cmakeExe
        $env:PATH = "$cmakeDir;$env:PATH"
        Write-OK "cmake (VS bundled): $cmakeExe"
        $cmakeExe = Get-Command cmake.exe -ErrorAction SilentlyContinue
    } else {
        Write-Fail "cmake.exe not found. Install CMake or add it to PATH."
        exit 1
    }
} else {
    Write-OK "cmake: $($cmakeExe.Source)"
}

# Find ninja
$ninjaExe = Get-Command ninja.exe -ErrorAction SilentlyContinue
$generator = "Ninja"
if (-not $ninjaExe) {
    $ninjaPaths = @(
        "D:\AppData\vsc2026\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe",
        "$env:ProgramFiles\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe",
        "$env:ProgramFiles\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe",
        "$env:ProgramFiles\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
    )
    $ninjaExe = $ninjaPaths | Where-Object { Test-Path $_ } | Select-Object -First 1
    if ($ninjaExe) {
        $ninjaDir = Split-Path $ninjaExe
        $env:PATH = "$ninjaDir;$env:PATH"
        Write-OK "ninja (VS bundled): $ninjaExe"
    } else {
        Write-Warn "Ninja not found — falling back to 'Visual Studio 17 2022' generator."
        $generator = "Visual Studio 17 2022"
    }
} else {
    Write-OK "ninja: $($ninjaExe.Source)"
}

# ── Step 2: Clean ─────────────────────────────────────────────────────────────
Write-Step 2 5 "Preparing build directory..."

$buildDir = Join-Path $ScriptDir "build"

if ($CleanBuild -and (Test-Path $buildDir)) {
    Write-Host "        Removing $buildDir ..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $buildDir
    Write-OK "Cleaned."
    $SkipConfig = $false
}

# Auto-detect generator/type mismatch in existing cache
if (-not $CleanBuild -and (Test-Path "$buildDir\CMakeCache.txt")) {
    $cacheContent = Get-Content "$buildDir\CMakeCache.txt" -ErrorAction SilentlyContinue
    $cachedGen  = ($cacheContent | Where-Object { $_ -match "^CMAKE_GENERATOR:INTERNAL=(.+)" } | Select-Object -First 1) -replace "^CMAKE_GENERATOR:INTERNAL=", ""
    $cachedType = ($cacheContent | Where-Object { $_ -match "^CMAKE_BUILD_TYPE:" } | Select-Object -First 1) -replace "^CMAKE_BUILD_TYPE:[^=]+=", ""

    if ($cachedGen -and $cachedGen -ne $generator) {
        Write-Warn "Generator mismatch (cache=$cachedGen, wanted=$generator) — wiping build/."
        Remove-Item -Recurse -Force $buildDir
        $SkipConfig = $false
    } elseif ($cachedType -and $cachedType -ne $BuildType -and $generator -eq "Ninja") {
        Write-Warn "Build type mismatch (cache=$cachedType, wanted=$BuildType) — reconfiguring."
        $SkipConfig = $false
    } else {
        Write-OK "Existing build cache compatible — incremental build."
    }
} elseif (-not (Test-Path $buildDir)) {
    $SkipConfig = $false
}

# ── Step 3: CMake configure ───────────────────────────────────────────────────
if (-not $SkipConfig) {
    Write-Step 3 5 "CMake configure  ($generator  /  $BuildType)..."

    if (-not (Test-Path $buildDir)) { New-Item -ItemType Directory $buildDir | Out-Null }

    $configLog = "$buildDir\cmake_configure.log"
    $cmakeArgs = @("-B", $buildDir, "-S", $ScriptDir, "-G", $generator, "-DCMAKE_BUILD_TYPE=$BuildType")
    if ($generator -ne "Ninja") { $cmakeArgs += @("-A", "x64") }

    cmake @cmakeArgs 2>&1 | Tee-Object -FilePath $configLog | Write-Host
    if ($LASTEXITCODE -ne 0) {
        Write-Fail "CMake configure failed (exit $LASTEXITCODE). See: $configLog"
        exit 1
    }
    Write-OK "Configure done."
} else {
    Write-Step 3 5 "CMake configure — skipped (incremental)."
    Write-OK "Using existing CMake cache."
}

# ── Step 4: Sanity check ──────────────────────────────────────────────────────
Write-Step 4 5 "Pre-build sanity check..."
$missing = 0
foreach ($f in @("Source\Main.cpp", "CMakeLists.txt")) {
    if (-not (Test-Path (Join-Path $ScriptDir $f))) {
        Write-Fail "Missing: $f"
        $missing++
    }
}
if ($missing -gt 0) { exit 1 }
Write-OK "Critical files present."

# ── Step 5: Build ─────────────────────────────────────────────────────────────
Write-Step 5 5 "Compiling  ($BuildType  /  $env:NUMBER_OF_PROCESSORS threads)..."

$compileLog = "$buildDir\compile.log"
$jobs = if ($env:NUMBER_OF_PROCESSORS) { $env:NUMBER_OF_PROCESSORS } else { "8" }
$buildArgs = @("--build", $buildDir, "--config", $BuildType, "--parallel", $jobs)

$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
cmake @buildArgs 2>&1 | Tee-Object -FilePath $compileLog | Write-Host
$exitCode = $LASTEXITCODE
$stopwatch.Stop()

$elapsed = $stopwatch.Elapsed
$elapsedStr = "{0}m {1}s" -f [int]$elapsed.TotalMinutes, $elapsed.Seconds

if ($exitCode -ne 0) {
    Write-Host ""
    Write-Fail "Build failed (exit $exitCode). Last errors:"
    Get-Content $compileLog -ErrorAction SilentlyContinue |
        Where-Object { $_ -match "error" } |
        Select-Object -Last 25 |
        ForEach-Object { Write-Host "    $_" -ForegroundColor Red }
    Write-Host ""
    Write-Warn "Full log: $compileLog"
    exit 1
}

# ── Result ────────────────────────────────────────────────────────────────────
Write-Banner "BUILD SUCCEEDED  ·  $elapsedStr" "Green"

$exePaths = @(
    "$buildDir\NerouRuntime_artefacts\$BuildType\NerouRuntime.exe",
    "$buildDir\$BuildType\NerouRuntime.exe",
    "$buildDir\NerouRuntime.exe"
)
$exeFound = $exePaths | Where-Object { Test-Path $_ } | Select-Object -First 1

if ($exeFound) {
    $size = (Get-Item $exeFound).Length / 1MB
    Write-OK ("Output : " + $exeFound)
    Write-OK ("Size   : {0:F1} MB" -f $size)
    Write-Host ""
    $launch = Read-Host "  Launch now? [y/N]"
    if ($launch -match "^[yY]") {
        Start-Process $exeFound
    }
} else {
    Write-Warn "Executable not found at expected paths — check $buildDir manually."
}

Write-Host ""
exit 0
