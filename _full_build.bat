@echo off
echo === NerouRuntime Build ===
call "D:\AppData\vsc2026\VC\Auxiliary\Build\vcvars64.bat"
cd /d "e:\VIBECode\NerouRuntime"
echo.
echo [CMake Configure]
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 (
    echo CMAKE FAILED
    pause
    exit /b 1
)
echo.
echo [Build]
cmake --build build --config Release --parallel 12
if errorlevel 1 (
    echo BUILD FAILED
    pause
    exit /b 1
)
echo.
echo SUCCESS!
dir "build\NerouRuntime_artefacts\Release\NerouRuntime.exe" 2>nul
pause