@echo off
:: ============================================================================
:: NerouRuntime 开发构建脚本
:: 提供常用的开发构建场景
:: ============================================================================

setlocal EnableDelayedExpansion

:menu
cls
echo ==========================================
echo   NerouRuntime 开发构建工具
echo ==========================================
echo.
echo  [1] 快速构建 (Release, 增量编译)
echo  [2] 完整重建 (Clean + Release)
echo  [3] Debug构建 (调试版本)
echo  [4] CI模式构建 (详细日志，无交互)
echo  [5] 仅配置 (不编译)
echo  [6] 运行程序
echo  [7] 清理构建目录
echo  [8] 打开构建目录
echo.
echo  [0] 退出
echo.
echo ==========================================
set /p choice="请选择操作 [0-8]: "

if "%choice%"=="1" goto :quick
if "%choice%"=="2" goto :rebuild
if "%choice%"=="3" goto :debug
if "%choice%"=="4" goto :ci
if "%choice%"=="5" goto :configure
if "%choice%"=="6" goto :run
if "%choice%"=="7" goto :clean
if "%choice%"=="8" goto :open_dir
if "%choice%"=="0" exit /b 0
goto :menu

:quick
echo.
echo [快速构建] 增量编译 Release 版本...
call build_auto_ci.cmd skip-config release
if errorlevel 1 pause
goto :menu

:rebuild
echo.
echo [完整重建] 清理后重新构建 Release 版本...
call build_auto_ci.cmd clean release
goto :menu

:debug
echo.
echo [Debug构建] 构建调试版本...
call build_auto_ci.cmd debug
goto :menu

:ci
echo.
echo [CI模式] 详细日志，非交互式构建...
call build_auto_ci.cmd ci
if errorlevel 1 pause
goto :menu

:configure
echo.
echo [仅配置] 运行 CMake 配置...
if not exist build mkdir build
cmake -B build -S . -G "Visual Studio 17 2022" -A x64
if errorlevel 1 (
    echo 配置失败
) else (
    echo 配置完成
)
pause
goto :menu

:run
echo.
echo [运行程序] 启动 NerouRuntime...
if exist "build\NerouRuntime_artefacts\Release\NerouRuntime.exe" (
    start "" "build\NerouRuntime_artefacts\Release\NerouRuntime.exe"
) else if exist "build\Release\NerouRuntime.exe" (
    start "" "build\Release\NerouRuntime.exe"
) else if exist "build\Debug\NerouRuntime.exe" (
    start "" "build\Debug\NerouRuntime.exe"
) else (
    echo 未找到可执行文件，请先构建项目
    pause
)
goto :menu

:clean
echo.
echo [清理] 删除构建目录...
if exist build (
    rd /s /q build
    echo 构建目录已清理
) else (
    echo 构建目录不存在
)
pause
goto :menu

:open_dir
echo.
echo [打开目录] 打开构建目录...
if exist build (
    explorer build
) else (
    echo 构建目录不存在
    pause
)
goto :menu
