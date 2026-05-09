@echo off
setlocal
call "D:\AppData\vsc2026\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if not exist build mkdir build
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 exit /b %errorlevel%
cmake --build build --config Release -j 8
exit /b %errorlevel%
