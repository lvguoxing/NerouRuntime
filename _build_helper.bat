@echo off
setlocal EnableDelayedExpansion

echo ==========================================
echo   NerouRuntime Build Helper
echo ==========================================
echo.

echo [1/4] Initializing Visual Studio environment...
call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 (
    echo ERROR: Failed to initialize VS environment
    exit /b 1
)
echo OK: VS environment initialized
echo.

echo [2/4] Cleaning build directory...
if exist build (
    rd /s /q build
    if errorlevel 1 (
        echo WARNING: Failed to clean build directory
    ) else (
        echo OK: build directory cleaned
    )
) else (
    echo OK: build directory does not exist
)
echo.

echo [3/4] Configuring with CMake...
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 (
    echo ERROR: CMake configuration failed
    exit /b 1
)
echo OK: CMake configuration completed
echo.

echo [4/4] Building...
cmake --build build --config Release --parallel %NUMBER_OF_PROCESSORS%
if errorlevel 1 (
    echo ERROR: Build failed
    exit /b 1
)
echo OK: Build completed
echo.

echo ==========================================
echo   Build successful!
echo ==========================================
echo.
echo Output: build\NerouRuntime_artefacts\Release\NerouRuntime.exe

exit /b 0