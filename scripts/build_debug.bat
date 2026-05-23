@echo off
chcp 65001 >nul
title NPC World -- Debug Build
color 0E

set PROJ_ROOT=%~dp0..
pushd "%PROJ_ROOT%"

echo ============================================
echo   NPC World -- Debug 构建
echo ============================================
echo.

set "MSBUILD=D:\Visual Studio\MSBuild\Current\Bin\MSBuild.exe"
if not exist "%MSBUILD%" (
    echo [错误] 找不到 MSBuild: %MSBUILD%
    pause
    popd
    exit /b 1
)

echo [1/2] 构建后台...
"%MSBUILD%" build\npc_backend.vcxproj /p:Configuration=Debug /v:minimal /t:Build
if %ERRORLEVEL% neq 0 (
    echo [失败] 后台构建出错！
    pause
    popd
    exit /b 1
)
echo       后台: 完成

echo [2/2] 构建前端...
"%MSBUILD%" build\npc_frontend.vcxproj /p:Configuration=Debug /v:minimal /t:Build
if %ERRORLEVEL% neq 0 (
    echo [失败] 前端构建出错！
    pause
    popd
    exit /b 1
)
echo       前端: 完成

echo.
echo ============================================
echo   Debug 构建成功！
echo   scripts\launch.bat ^(选择 Debug^) 启动
echo ============================================
echo.

popd
pause
