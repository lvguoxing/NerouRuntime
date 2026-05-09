@echo off
chcp 65001 >nul
echo ==============================================================
echo Launching NerouRuntime (Native C++ Edition)...
echo ==============================================================

cd /d "%~dp0"

set EXE_PATH="%~dp0build\NerouRuntime_artefacts\Release\NerouRuntime.exe"

if not exist %EXE_PATH% (
    echo [ERROR] Executable not found at %EXE_PATH%
    echo Please run _build_now.bat first!
    pause
    exit /b 1
)

rem Ensure workspace directories and demo data exist
if not exist "data\npz\demo_bci_4class.npz" (
    echo [INFO] First launch - initializing workspace...
    call init_workspace.bat
)

echo Starting Application from %EXE_PATH% ...
start "" %EXE_PATH%
