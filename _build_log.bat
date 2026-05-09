@echo off
echo === NerouRuntime Build Started ===
echo Started at: %DATE% %TIME%
echo.

echo [1/3] Setting up VS environment...
call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 (
    echo ERROR: VS environment setup failed
    exit /b 1
)
echo OK
echo.

echo [2/3] Configuring CMake...
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release > build_config.log 2>&1
if errorlevel 1 (
    echo ERROR: CMake configuration failed
    type build_config.log
    exit /b 1
)
echo OK
echo.

echo [3/3] Building Release...
cmake --build build --config Release --parallel %NUMBER_OF_PROCESSORS% > build_compile.log 2>&1
if errorlevel 1 (
    echo ERROR: Build failed
    type build_compile.log
    exit /b 1
)
echo OK
echo.

echo === Build Successful ===
echo Completed at: %DATE% %TIME%
echo Output: build\NerouRuntime_artefacts\Release\NerouRuntime.exe
exit /b 0