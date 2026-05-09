@echo off
call "D:\AppData\vsc2026\VC\Auxiliary\Build\vcvars64.bat"
cd /d "e:\VIBECode\NerouRuntime"
cmake --build build --config Release --parallel 12 2>&1
echo Exit code: %errorlevel%
pause