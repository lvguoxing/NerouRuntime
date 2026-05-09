@echo off
call "D:\AppData\vsc2026\VC\Auxiliary\Build\vcvars64.bat"
cd /d "e:\VIBECode\NerouRuntime"
echo Building...
cmake --build build --config Release --parallel 12
echo.
if errorlevel 1 (
    echo BUILD FAILED
) else (
    echo BUILD SUCCESS
    if exist "build\NerouRuntime_artefacts\Release\NerouRuntime.exe" (
        echo Executable: build\NerouRuntime_artefacts\Release\NerouRuntime.exe
    )
)
pause