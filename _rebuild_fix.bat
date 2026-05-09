@echo off
echo ==========================================
echo   NerouRuntime Rebuild (Bug Fixes)
echo ==========================================
echo.

call "D:\AppData\vsc2026\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 (
    echo ERROR: VS 2026 environment setup failed
    pause
    exit /b 1
)
echo OK: Visual Studio 2026 environment initialized
echo.

cd /d "e:\VIBECode\NerouRuntime"

echo [Step 1] Cleaning build directory...
if exist build (
    rd /s /q build
    echo OK: Cleaned
echo OK: Nothing to clean
echo.

echo [Step 2] Configuring CMake (Release)...
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl
echo.

if errorlevel 1 (
    echo ERROR: CMake configuration failed
    pause
    exit /b 1
)
echo OK: CMake configuration completed
echo.

echo [Step 3] Building (12 threads)...
cmake --build build --config Release --parallel 12
echo.

if errorlevel 1 (
    echo ERROR: Build failed
    pause
    exit /b 1
)

echo ==========================================
echo   BUILD SUCCESSFUL
echo ==========================================
echo.
if exist "build\NerouRuntime_artefacts\Release\NerouRuntime.exe" (
    echo Output: build\NerouRuntime_artefacts\Release\NerouRuntime.exe
    for %%F in ("build\NerouRuntime_artefacts\Release\NerouRuntime.exe") do (
        echo Size: %%~zF bytes
    )
) else (
    echo Output: build\Release\NerouRuntime.exe
)
echo.
pause