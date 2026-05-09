@echo off
:: ============================================================================
:: NerouRuntime 标准构建脚本
:: 这是向后兼容的包装脚本，调用新的 build_auto_ci.cmd
:: ============================================================================

echo ==========================================
echo Starting NerouRuntime C++ Native Build...
echo ==========================================
echo.
echo NOTE: This script now calls build_auto_ci.cmd for enhanced functionality
echo Run 'build_auto_ci.cmd help' for advanced options
echo.

:: 检查是否存在新的构建脚本
if exist "build_auto_ci.cmd" (
    :: 调用新的智能构建脚本
    call build_auto_ci.cmd %*
    exit /b %errorlevel%
) else (
    :: 回退到旧版简单构建
    echo [WARNING] build_auto_ci.cmd not found, using fallback build...
    goto :fallback_build
)

:fallback_build
echo [1/3] Checking build env...
if not exist build mkdir build

echo [2/3] CMake Configure and Fetch JUCE...
cmake -B build -S .

if errorlevel 1 (
    echo [ERROR] CMake configuration failed.
    pause
    exit /b %errorlevel%
)

echo [3/3] Compiling Release Build...
cmake --build build --config Release -j 8

if errorlevel 1 (
    echo [ERROR] C++ Compilation failed.
    pause
    exit /b %errorlevel%
)

echo.
echo ==========================================
echo [SUCCESS] Build Complete!
echo Find your Executable in build/NerouRuntime_artefacts/Release/
echo ==========================================
pause
