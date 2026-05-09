@echo off
setlocal EnableDelayedExpansion

:: ============================================================================
:: NerouRuntime Intelligent Build Script (Optimized CI Build System)
:: Version: 2.2.0
:: Features: Ninja generator, per-BuildType cache mgmt, env check,
::           real cpp-level parallel compile, detailed logs
:: NOTE: kept ASCII-only so cmd.exe parses it reliably on any codepage.
:: CHANGELOG 2.2.0:
::   - switched default generator to Ninja (true per-file parallelism; the VS
::     generator needed /MP to parallelize cpp files inside a single project)
::   - auto-detects cached CMAKE_BUILD_TYPE; reconfigures when it changes
::   - PARALLEL_JOBS defaults to full NUMBER_OF_PROCESSORS (was N-1)
:: ============================================================================

:: Default configuration
set "BUILD_TYPE=Release"
set "CLEAN_BUILD=0"
set "VERBOSE=0"
set "CI_MODE=0"
set "SKIP_CONFIG=0"
set "PARALLEL_JOBS=0"
set "TARGET="
set "EXIT_CODE=0"
set "AUTO_VCVARS=1"
set "GENERATOR=Ninja"
set "ORIGINAL_ARGS=%*"

:: Color codes (ANSI escape). Detect Win10+ for color support.
set "ESC="
set "RESET="
set "RED="
set "GREEN="
set "YELLOW="
set "BLUE="
set "CYAN="

for /f "tokens=2 delims=[]" %%a in ('ver') do set "WIN_VER=%%a"
set "WIN_VER=!WIN_VER:Version =!"
for /f "tokens=1,2 delims=." %%a in ("!WIN_VER!") do (
    set "WIN_MAJOR=%%a"
    set "WIN_MINOR=%%b"
)
if !WIN_MAJOR! GEQ 10 (
    for /f %%a in ('echo prompt $E ^| cmd') do set "ESC=%%a"
    set "RESET=!ESC![0m"
    set "RED=!ESC![31m"
    set "GREEN=!ESC![32m"
    set "YELLOW=!ESC![33m"
    set "BLUE=!ESC![34m"
    set "CYAN=!ESC![36m"
)

:: ============================================================================
:: Argument parsing
:: ============================================================================
:parse_args
if "%~1"=="" goto :main

if /I "%~1"=="clean"       ( set "CLEAN_BUILD=1" & shift & goto :parse_args )
if /I "%~1"=="rebuild"     ( set "CLEAN_BUILD=1" & set "SKIP_CONFIG=0" & shift & goto :parse_args )
if /I "%~1"=="debug"       ( set "BUILD_TYPE=Debug" & shift & goto :parse_args )
if /I "%~1"=="release"     ( set "BUILD_TYPE=Release" & shift & goto :parse_args )
if /I "%~1"=="ci"          ( set "CI_MODE=1" & set "VERBOSE=1" & shift & goto :parse_args )
if /I "%~1"=="verbose"     ( set "VERBOSE=1" & shift & goto :parse_args )
if /I "%~1"=="skip-config" ( set "SKIP_CONFIG=1" & shift & goto :parse_args )
if /I "%~1"=="no-vcvars"   ( set "AUTO_VCVARS=0" & shift & goto :parse_args )
if /I "%~1"=="ninja"       ( set "GENERATOR=Ninja" & shift & goto :parse_args )
if /I "%~1"=="vs"          ( set "GENERATOR=Visual Studio 17 2022" & shift & goto :parse_args )

if /I "%~1"=="-j" (
    rem NOTE: inside a parenthesized block, shift does NOT affect %~1,
    rem so we read %~2 directly, then shift twice to consume both tokens.
    if not "%~2"=="" set "PARALLEL_JOBS=%~2"
    shift
    shift
    goto :parse_args
)

if /I "%~1"=="help" goto :show_help
if /I "%~1"=="-h"   goto :show_help
if /I "%~1"=="/?"   goto :show_help

echo %RED%Unknown option: %~1%RESET%
shift
goto :parse_args

:show_help
echo.
echo %CYAN%NerouRuntime Build Script v2.2.0%RESET%
echo.
echo Usage: build_auto_ci.cmd [options]
echo.
echo Options:
echo   clean          Clean build directory before building
echo   rebuild        Force complete rebuild
echo   debug          Build Debug config (default: Release)
echo   release        Build Release config
echo   ninja          Use Ninja generator (default, fastest)
echo   vs             Use "Visual Studio 17 2022" generator
echo   ci             CI mode (non-interactive, verbose output)
echo   verbose        Show detailed build output
echo   skip-config    Skip CMake configure step (incremental compile only)
echo   no-vcvars      Do not auto-locate / call vcvars64.bat
echo   -j [N]         Parallel jobs (default: all CPU cores)
echo   help, /?, -h   Show this help
echo.
echo Examples:
echo   build_auto_ci.cmd
echo   build_auto_ci.cmd debug
echo   build_auto_ci.cmd clean release
echo   build_auto_ci.cmd ci -j 16
echo   build_auto_ci.cmd rebuild verbose
echo   build_auto_ci.cmd vs release        (fallback to VS generator)
echo.
exit /b 0

:: ============================================================================
:: Main
:: ============================================================================
:main
if "%CI_MODE%"=="0" cls
echo %CYAN%==========================================%RESET%
echo %CYAN%  NerouRuntime Intelligent Build System%RESET%
echo %CYAN%==========================================%RESET%
echo.

set "START_TIME=%TIME: =0%"
for /f "tokens=1-4 delims=:." %%a in ("%START_TIME%") do (
    set /a "START_HOUR=1%%a-100"
    set /a "START_MIN=1%%b-100"
    set /a "START_SEC=1%%c-100"
)

if %PARALLEL_JOBS% EQU 0 (
    rem Windows 11 removed wmic, use NUMBER_OF_PROCESSORS (always defined)
    if defined NUMBER_OF_PROCESSORS (
        set "PARALLEL_JOBS=!NUMBER_OF_PROCESSORS!"
    ) else (
        set "PARALLEL_JOBS=8"
    )
    if !PARALLEL_JOBS! LSS 1 set "PARALLEL_JOBS=1"
)

echo [Config] Generator     : %YELLOW%%GENERATOR%%RESET%
echo [Config] Build type    : %YELLOW%%BUILD_TYPE%%RESET%
echo [Config] Parallel jobs : %YELLOW%%PARALLEL_JOBS%%RESET%
echo [Config] CI mode       : %YELLOW%%CI_MODE%%RESET%
echo.

:: ============================================================================
:: Step 1: Environment check (+ auto vcvars if needed)
:: ============================================================================
echo %BLUE%[1/6] Checking build environment...%RESET%

:: CMake
cmake --version >nul 2>&1
if errorlevel 1 (
    echo %RED%[ERROR] CMake not found in PATH%RESET%
    echo %YELLOW%  Download: https://cmake.org/download/ ^(3.20+ required^)%RESET%
    set "EXIT_CODE=1"
    goto :cleanup
)
for /f "tokens=3" %%a in ('cmake --version ^| findstr /R /C:"cmake version"') do set "CMAKE_VERSION=%%a"
echo %GREEN%  OK CMake %CMAKE_VERSION%%RESET%

:: MSVC cl.exe - try to auto-init vcvars if not available
cl >nul 2>&1
if errorlevel 1 (
    if "%AUTO_VCVARS%"=="1" (
        call :auto_init_vcvars
        exit /b !ERRORLEVEL!
    )
)
cl >nul 2>&1
if errorlevel 1 (
    echo %RED%  FAIL MSVC cl.exe not available%RESET%
    echo %YELLOW%  Tip: Run via .\build.ps1 from any PowerShell window, or open%RESET%
    echo %YELLOW%       "Developer Command Prompt for VS 2022" and re-run this script.%RESET%
    set "EXIT_CODE=1"
    goto :cleanup
) else (
    for /f "tokens=1*" %%a in ('cl 2^>^&1 ^| findstr /C:"Version"') do (
        echo %GREEN%  OK MSVC %%b%RESET%
    )
)

:: Ninja ? re-check AFTER vcvars (vcvars adds VS's bundled ninja to PATH)
if /I "%GENERATOR%"=="Ninja" (
    ninja --version >nul 2^>^&1
    if errorlevel 1 (
        echo %YELLOW%  WARN Ninja not found on PATH even after vcvars - falling back to "Visual Studio 17 2022"%RESET%
        echo %YELLOW%       Install Ninja from https://ninja-build.org or via winget: winget install Ninja-build.ninja%RESET%
        set "GENERATOR=Visual Studio 17 2022"
    ) else (
        for /f %%a in ('ninja --version') do echo %GREEN%  OK Ninja %%a%RESET%
    )
)

:: Git (optional)
git --version >nul 2>&1
if errorlevel 1 (
    echo %YELLOW%  WARN Git not found ^(FetchContent may fail^)%RESET%
) else (
    for /f "tokens=3" %%a in ('git --version') do echo %GREEN%  OK Git %%a%RESET%
)

echo.

:: ============================================================================
:: Step 2: Clean / inspect build dir, detect generator + build-type mismatch
:: ============================================================================
if %CLEAN_BUILD% EQU 1 (
    echo %BLUE%[2/6] Cleaning build directory...%RESET%
    if exist build (
        rd /s /q build
        if errorlevel 1 (
            echo %RED%[ERROR] Failed to clean build/%RESET%
            set "EXIT_CODE=1"
            goto :cleanup
        )
        echo %GREEN%  OK build/ removed%RESET%
    ) else (
        echo %YELLOW%  build/ does not exist, nothing to clean%RESET%
    )
    set "SKIP_CONFIG=0"
) else (
    echo %BLUE%[2/6] Inspecting build directory...%RESET%
    if exist build (
        set "_CACHE_GEN="
        set "_CACHE_TYPE="
        if exist "build\CMakeCache.txt" (
            for /f "usebackq tokens=2 delims==" %%a in (`findstr /B /C:"CMAKE_GENERATOR:INTERNAL=" "build\CMakeCache.txt"`) do set "_CACHE_GEN=%%a"
            for /f "usebackq tokens=2 delims==" %%a in (`findstr /B /C:"CMAKE_BUILD_TYPE:" "build\CMakeCache.txt"`) do set "_CACHE_TYPE=%%a"
        )
        set "_GEN_MISMATCH=0"
        if defined _CACHE_GEN if /I not "!_CACHE_GEN!"=="%GENERATOR%" set "_GEN_MISMATCH=1"
        set "_TYPE_MISMATCH=0"
        if /I "%GENERATOR%"=="Ninja" (
            if defined _CACHE_TYPE if /I not "!_CACHE_TYPE!"=="%BUILD_TYPE%" set "_TYPE_MISMATCH=1"
        )
        if !_GEN_MISMATCH! EQU 1 (
            echo %YELLOW%  WARN existing cache generator "!_CACHE_GEN!" != requested "%GENERATOR%"%RESET%
            echo %YELLOW%       wiping build/ to avoid configure failure%RESET%
            rd /s /q build
            set "SKIP_CONFIG=0"
        ) else (
            if !_TYPE_MISMATCH! EQU 1 (
                echo %YELLOW%  WARN Ninja is single-config; cached type "!_CACHE_TYPE!" != "%BUILD_TYPE%"%RESET%
                echo %YELLOW%       forcing reconfigure%RESET%
                set "SKIP_CONFIG=0"
            ) else (
                echo %GREEN%  OK using existing build/ ^(incremental^)%RESET%
            )
        )
    ) else (
        echo %YELLOW%  build/ will be created%RESET%
        set "SKIP_CONFIG=0"
    )
)
echo.

:: ============================================================================
:: Step 3: CMake configure
:: ============================================================================
if %SKIP_CONFIG% EQU 0 (
    echo %BLUE%[3/6] Running CMake configure ^(%BUILD_TYPE%^)...%RESET%

    if not exist build mkdir build

    if /I "%GENERATOR%"=="Ninja" (
        set "CMAKE_ARGS=-B build -S . -G Ninja -DCMAKE_BUILD_TYPE=%BUILD_TYPE%"
    ) else (
        set "CMAKE_ARGS=-B build -S . -G ""%GENERATOR%"" -A x64 -DCMAKE_BUILD_TYPE=%BUILD_TYPE%"
    )
    if %VERBOSE% EQU 1 (
        set "CMAKE_ARGS=!CMAKE_ARGS! -DCMAKE_VERBOSE_MAKEFILE=ON"
    )

    if %VERBOSE% EQU 1 (
        cmake !CMAKE_ARGS!
    ) else (
        cmake !CMAKE_ARGS! >"build/cmake_configure.log" 2^>^&1
    )

    if errorlevel 1 (
        echo %RED%[ERROR] CMake configure failed%RESET%
        if exist "build/cmake_configure.log" (
            echo.
            echo %YELLOW%Last errors:%RESET%
            powershell -NoProfile -Command "Get-Content 'build/cmake_configure.log' | Select-String -Pattern 'error|Error' | Select-Object -Last 20"
        )
        set "EXIT_CODE=1"
        goto :cleanup
    )

    echo %GREEN%  OK CMake configure done%RESET%
    if %VERBOSE% EQU 0 echo %YELLOW%  Log: build/cmake_configure.log%RESET%
) else (
    echo %BLUE%[3/6] Skipping CMake configure ^(using existing cache^)%RESET%
    echo %GREEN%  OK existing cache preserved%RESET%
)
echo.

:: ============================================================================
:: Step 4: Pre-build sanity
:: ============================================================================
echo %BLUE%[4/6] Pre-build sanity check...%RESET%

set "MISSING_FILES=0"
if not exist "Source\Main.cpp"  ( echo %RED%  FAIL missing Source/Main.cpp%RESET%  & set /a "MISSING_FILES+=1" )
if not exist "CMakeLists.txt"   ( echo %RED%  FAIL missing CMakeLists.txt%RESET%   & set /a "MISSING_FILES+=1" )

if not exist "Source\UI\Theme\DesignTokens.h" (
    echo %YELLOW%  WARN UI/Theme/DesignTokens.h missing ^(UI upgrade may be incomplete^)%RESET%
)

if !MISSING_FILES! GTR 0 (
    echo %RED%[ERROR] %MISSING_FILES% critical file^(s^) missing%RESET%
    set "EXIT_CODE=1"
    goto :cleanup
)
echo %GREEN%  OK all critical files present%RESET%
echo.

:: ============================================================================
:: Step 5: Build
:: ============================================================================
echo %BLUE%[5/6] Building ^(%BUILD_TYPE%^)...%RESET%
echo %CYAN%  Parallel jobs: %PARALLEL_JOBS%%RESET%
echo.

set "BUILD_ARGS=--build build --config %BUILD_TYPE% --parallel %PARALLEL_JOBS%"

if %VERBOSE% EQU 1 (
    set "BUILD_ARGS=!BUILD_ARGS! --verbose"
    cmake !BUILD_ARGS!
) else (
    echo %YELLOW%  Compiling... ^(detailed log: build/compile.log^)%RESET%
    cmake !BUILD_ARGS! >"build/compile.log" 2^>^&1
)

if errorlevel 1 (
    echo.
    echo %RED%[ERROR] Build failed%RESET%
    echo.
    echo %YELLOW%Last 20 error lines:%RESET%
    if exist "build/compile.log" (
        powershell -NoProfile -Command "Get-Content 'build/compile.log' | Select-String -Pattern 'error' -CaseSensitive:$false | Select-Object -Last 20"
    )
    set "EXIT_CODE=1"
    goto :cleanup
)

echo.
echo %GREEN%  OK build succeeded%RESET%
echo.

:: ============================================================================
:: Step 6: Post-processing
:: ============================================================================
echo %BLUE%[6/6] Post-processing...%RESET%

if exist "build\onnxruntime-src\lib\onnxruntime.dll" (
    if not exist "build\NerouRuntime_artefacts\%BUILD_TYPE%\onnxruntime.dll" (
        copy /Y "build\onnxruntime-src\lib\onnxruntime.dll" "build\NerouRuntime_artefacts\%BUILD_TYPE%\" >nul 2^>^&1
        if not errorlevel 1 echo %GREEN%  OK copied onnxruntime.dll%RESET%
    )
)

set "END_TIME=%TIME: =0%"
for /f "tokens=1-4 delims=:." %%a in ("%END_TIME%") do (
    set /a "END_HOUR=1%%a-100"
    set /a "END_MIN=1%%b-100"
    set /a "END_SEC=1%%c-100"
)
if %END_HOUR% LSS %START_HOUR% set /a "END_HOUR+=24"
set /a "ELAPSED_SEC=(END_HOUR - START_HOUR) * 3600 + (END_MIN - START_MIN) * 60 + (END_SEC - START_SEC)"
set /a "ELAPSED_MIN=ELAPSED_SEC / 60"
set /a "ELAPSED_SEC=ELAPSED_SEC %% 60"

echo.
echo %CYAN%==========================================%RESET%
echo %GREEN%  [SUCCESS] Build completed%RESET%
echo %CYAN%==========================================%RESET%
echo.
echo Build info:
echo   Build type    : %YELLOW%%BUILD_TYPE%%RESET%
echo   Elapsed       : %YELLOW%%ELAPSED_MIN%m %ELAPSED_SEC%s%RESET%
echo   Parallel jobs : %YELLOW%%PARALLEL_JOBS%%RESET%
echo   CMake version : %YELLOW%%CMAKE_VERSION%%RESET%
echo.
echo Output:
if exist "build\NerouRuntime_artefacts\%BUILD_TYPE%\NerouRuntime.exe" (
    echo   Executable: %GREEN%build\NerouRuntime_artefacts\%BUILD_TYPE%\NerouRuntime.exe%RESET%
    for %%F in ("build\NerouRuntime_artefacts\%BUILD_TYPE%\NerouRuntime.exe") do (
        set "FILE_SIZE=%%~zF"
        set /a "SIZE_MB=FILE_SIZE / 1024 / 1024"
        echo   Size      : %YELLOW%!SIZE_MB! MB%RESET%
    )
) else if exist "build\%BUILD_TYPE%\NerouRuntime.exe" (
    echo   Executable: %GREEN%build\%BUILD_TYPE%\NerouRuntime.exe%RESET%
) else (
    echo   Executable: %YELLOW%^(inspect build/ manually^)%RESET%
)
echo.

echo Build Report > "build\build_report.txt"
echo Build Type: %BUILD_TYPE% >> "build\build_report.txt"
echo Build Time: %ELAPSED_MIN%m %ELAPSED_SEC%s >> "build\build_report.txt"
echo CMake Version: %CMAKE_VERSION% >> "build\build_report.txt"
echo Build Date: %DATE% %TIME% >> "build\build_report.txt"

:: ============================================================================
:: Cleanup / exit
:: ============================================================================
:cleanup
if %CI_MODE% EQU 0 (
    if %EXIT_CODE% NEQ 0 (
        echo.
        echo %RED%Build failed. Check the output above.%RESET%
        pause
    ) else (
        echo.
        echo %CYAN%Press any key to exit...%RESET%
        pause >nul
    )
)
exit /b %EXIT_CODE%


:: ============================================================================
:: Subroutine: attempt to locate and call vcvars64.bat automatically
::   Tries VS2022 Community/Professional/Enterprise/BuildTools, then VS2019.
::   Also honors %VCVARS64% override if exported by the caller.
:: ============================================================================
:auto_init_vcvars
echo %YELLOW%  cl.exe not on PATH - attempting to initialize MSVC environment...%RESET%
set "VCVARS="

if defined VCVARS64 (
    if exist "%VCVARS64%" set "VCVARS=%VCVARS64%"
)

if not defined VCVARS if exist "D:\AppData\vsc2026\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=D:\AppData\vsc2026\VC\Auxiliary\Build\vcvars64.bat"

for %%E in (Community Professional Enterprise BuildTools) do (
    if not defined VCVARS if exist "%ProgramFiles%\Microsoft Visual Studio\2022\%%E\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\%%E\VC\Auxiliary\Build\vcvars64.bat"
)
for %%E in (Community Professional Enterprise BuildTools) do (
    if not defined VCVARS if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\%%E\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=%ProgramFiles(x86)%\Microsoft Visual Studio\2019\%%E\VC\Auxiliary\Build\vcvars64.bat"
)

if not defined VCVARS (
    echo %YELLOW%  Could not locate vcvars64.bat automatically%RESET%
    exit /b 1
)

echo %CYAN%  Using: %VCVARS%%RESET%
echo %CYAN%  Relaunching build inside the MSVC environment...%RESET%
set "_RELAUNCH_CMD=%TEMP%\nerou_relaunch_%RANDOM%_%RANDOM%.cmd"
> "%_RELAUNCH_CMD%" echo @echo off
>> "%_RELAUNCH_CMD%" echo call "%VCVARS%" ^>nul
>> "%_RELAUNCH_CMD%" echo call "%~f0" no-vcvars %ORIGINAL_ARGS%
call "%_RELAUNCH_CMD%"
set "_RELAUNCH_EXIT=%ERRORLEVEL%"
del /q "%_RELAUNCH_CMD%" >nul 2^>^&1
exit /b %_RELAUNCH_EXIT%
