@echo off
chcp 65001 >nul
title NPC World Launcher
mode con cols=60 lines=24
color 0B

set PROJ_ROOT=%~dp0..
pushd "%PROJ_ROOT%"

:menu
cls
echo.
echo   ============================================
echo     NPC World v3.0 -- 三层架构启动器
echo   ============================================
echo.
echo     [1] Debug   模式 (开发/调试)
echo     [2] Release 模式 (正式运行)
echo     [3] 仅启动后端 (Debug)
echo     [4] 仅启动后端 (Release)
echo     [0] 退出
echo.
set /p choice="   请选择 (0-4): "

if "%choice%"=="1" set CONFIG=Debug& goto run
if "%choice%"=="2" set CONFIG=Release& goto run
if "%choice%"=="3" set CONFIG=Debug& goto backend_only
if "%choice%"=="4" set CONFIG=Release& goto backend_only
if "%choice%"=="0" goto :eof
goto menu

:backend_only
set EXE_DIR=%PROJ_ROOT%\build\%CONFIG%
if not exist "%EXE_DIR%\npc_backend.exe" (
    echo.
    echo [错误] 找不到 %CONFIG% 编译产物，请先构建！
    echo       运行: scripts\build_debug.bat 或 scripts\build_release.bat
    pause
    goto menu
)
echo [1/2] 清理旧进程...
taskkill /f /im npc_frontend.exe >nul 2>&1
taskkill /f /im npc_backend.exe >nul 2>&1
echo       完成
echo [2/2] 启动后端引擎 (%CONFIG%)...
start "NPC Backend (%CONFIG%)" "%EXE_DIR%\npc_backend.exe"
echo       后端已启动，按任意键停止...
pause >nul
taskkill /f /im npc_backend.exe >nul 2>&1
echo       已停止。
pause
goto menu

:run
set EXE_DIR=%PROJ_ROOT%\build\%CONFIG%
if not exist "%EXE_DIR%\npc_backend.exe" (
    echo.
    echo [错误] 找不到 %CONFIG%\npc_backend.exe
    echo       请先构建: scripts\build_debug.bat 或 scripts\build_release.bat
    pause
    goto menu
)
if not exist "%EXE_DIR%\npc_frontend.exe" (
    echo.
    echo [错误] 找不到 %CONFIG%\npc_frontend.exe
    echo       请先构建: scripts\build_debug.bat 或 scripts\build_release.bat
    pause
    goto menu
)

cls
echo.
echo   ============================================
echo     NPC World v3.0 -- %CONFIG% 模式
echo   ============================================
echo.

echo [1/3] 清理旧进程...
taskkill /f /im npc_frontend.exe >nul 2>&1
taskkill /f /im npc_backend.exe >nul 2>&1
echo       完成

echo [2/3] 启动后端引擎...
start "NPC Backend (%CONFIG%)" /min "%EXE_DIR%\npc_backend.exe"
echo       等待后端就绪...
ping 127.0.0.1 -n 2 >nul
echo       后端已启动

echo [3/3] 启动前端界面...
echo.
start "" /wait "%EXE_DIR%\npc_frontend.exe"

echo.
echo 正在停止后端...
taskkill /f /im npc_backend.exe >nul 2>&1
echo 已全部关闭。
popd
goto :eof
