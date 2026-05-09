@echo off
echo === NerouRuntime Build (CMD) ===
echo.

:: Step 1: Init MSVC
echo [1/3] Initializing MSVC...
call "D:\AppData\vsc2026\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 (
    echo [ERROR] vcvars64.bat failed
    pause
    exit /b 1
)
echo OK

:: Step 2: CMake Configure (skip if build/build.ninja exists)
cd /d "e:\VIBECode\NerouRuntime"
if not exist "build\build.ninja" (
    echo.
    echo [2/3] CMake Configure...
    cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release
    if errorlevel 1 (
        echo [ERROR] CMake configure failed
        pause
        exit /b 1
    )
    echo OK
) else (
    echo [2/3] Using existing build cache (incremental)
)

:: Step 3: Build
echo.
echo [3/3] Building (Release, parallel)...
cmake --build build --config Release --parallel 12 2>&1
if errorlevel 1 (
    echo.
    echo =============================================
    echo   BUILD FAILED - check errors above
    echo =============================================
    pause
    exit /b 1
)

echo.
echo =============================================
echo   BUILD SUCCESS!
echo =============================================
dir "build\NerouRuntime_artefacts\Release\NerouRuntime.exe" 2>nul
pause
