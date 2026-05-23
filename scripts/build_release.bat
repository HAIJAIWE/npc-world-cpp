@echo off
chcp 65001 >nul
title NPC World -- Release Build
color 0A

set PROJ_ROOT=%~dp0..
pushd "%PROJ_ROOT%"

echo ============================================
echo   NPC World -- Release 构建
echo ============================================
echo.

echo [0/3] 检查 CMake 配置...
if not exist "build\CMakeCache.txt" (
    echo       需要先配置 CMake...
    cmake -B build -G "Visual Studio 17 2022" -A x64
    if %ERRORLEVEL% neq 0 (
        echo [失败] CMake configure 失败！
        pause
        popd
        exit /b 1
    )
)

echo [1/3] 构建外部依赖...
cmake --build build --config Release --target sqlite3_static -j %NUMBER_OF_PROCESSORS%
if %ERRORLEVEL% neq 0 echo [警告] 外部依赖构建非零退出码

echo [2/3] 构建后台...
cmake --build build --config Release --target npc_backend -j %NUMBER_OF_PROCESSORS%
if %ERRORLEVEL% neq 0 (
    echo [失败] 后台构建出错！
    pause
    popd
    exit /b 1
)
echo       后台: 完成

echo [3/3] 构建前端...
cmake --build build --config Release --target npc_frontend -j %NUMBER_OF_PROCESSORS%
if %ERRORLEVEL% neq 0 (
    echo [失败] 前端构建出错！
    pause
    popd
    exit /b 1
)
echo       前端: 完成

echo.
echo ============================================
echo   Release 构建成功！
echo   产物: build\Release\npc_backend.exe
echo          build\Release\npc_frontend.exe
echo ============================================
echo.

popd
pause
