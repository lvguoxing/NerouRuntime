@echo off
echo ==========================================
echo   NerouRuntime Build (VS 2026)
echo ==========================================
echo.

echo [Step 1] Initializing VS 2026 environment...
call "D:\AppData\vsc2026\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 (
    echo ERROR: Failed to initialize VS 2026 environment
    pause
    exit /b 1
)
echo OK: Visual Studio 2026 x64 environment ready
echo.

echo [Step 2] Cleaning previous build...
if exist build (
    rd /s /q build
    echo OK: Cleaned build directory
) else (
    echo OK: No previous build
)
echo.

echo [Step 3] Configuring CMake (Release)...
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl
if errorlevel 1 (
    echo ERROR: CMake configuration failed
    pause
    exit /b 1
)
echo OK: CMake configuration completed
echo.

echo [Step 4] Compiling (using %NUMBER_OF_PROCESSORS% threads)...
cmake --build build --config Release --parallel %NUMBER_OF_PROCESSORS%
if errorlevel 1 (
    echo ERROR: Build failed
    pause
    exit /b 1
)
echo.

echo ==========================================
echo   BUILD SUCCESSFUL
echo ==========================================
echo.
if exist "build\NerouRuntime_artefacts\Release\NerouRuntime.exe" (
    echo Output: build\NerouRuntime_artefacts\Release\NerouRuntime.exe
) else (
    echo Output: build\Release\NerouRuntime.exe
)
echo.
pause