@echo off
call "D:\AppData\vsc2026\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d "e:\VIBECode\NerouRuntime"
if exist "build\NerouRuntime_artefacts\Release\NerouRuntime.exe" (
    echo SUCCESS
    dir "build\NerouRuntime_artefacts\Release\NerouRuntime.exe" | findstr "NerouRuntime.exe"
) else (
    echo BUILD_INCOMPLETE
)
exit /b 0