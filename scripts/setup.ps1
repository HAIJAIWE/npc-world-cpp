# ============================================================================
# NPC World C++ — 一键安装脚本
# 位置: scripts/setup.ps1
# 用法: powershell -ExecutionPolicy Bypass -File scripts/setup.ps1
# ============================================================================

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $root

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  NPC World C++ — 依赖安装" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

Write-Host "[1/6] 检查环境..." -ForegroundColor Yellow

$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmake) {
    Write-Host "  错误: 未找到 CMake。请从 https://cmake.org/download/ 安装" -ForegroundColor Red
    Write-Host "  安装时勾选 'Add CMake to system PATH'" -ForegroundColor Red
    pause
    exit 1
}
Write-Host "  CMake: $($cmake.Source)" -ForegroundColor Green

$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vsWhere -latest -property installationPath 2>$null
if (-not $vs) {
    Write-Host "  警告: 未检测到 Visual Studio。需要 VS2022 (含'使用C++的桌面开发')" -ForegroundColor Yellow
}
else {
    Write-Host "  Visual Studio: $vs" -ForegroundColor Green
}

Write-Host ""
Write-Host "[2/6] llama.cpp..." -ForegroundColor Yellow
$llamaDir = "$root\external\llama.cpp"

if (Test-Path "$llamaDir\CMakeLists.txt") {
    Write-Host "  已存在，跳过" -ForegroundColor Green
}
else {
    Write-Host "  正在 git clone (约 50MB，可能需要几分钟)..." -ForegroundColor Gray
    git clone --depth 1 https://github.com/ggerganov/llama.cpp.git $llamaDir 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  git clone 失败，尝试手动下载..." -ForegroundColor Yellow
        Write-Host "  请手动执行: git clone --depth 1 https://github.com/ggerganov/llama.cpp.git $llamaDir" -ForegroundColor Yellow
    }
    else {
        Write-Host "  llama.cpp: 完成" -ForegroundColor Green
    }
}

Write-Host ""
Write-Host "[3/6] Dear ImGui..." -ForegroundColor Yellow
$imguiDir = "$root\external\imgui"

if (Test-Path "$imguiDir\imgui.cpp") {
    Write-Host "  已存在 ($((Get-ChildItem $imguiDir -Filter *.cpp).Count) 个 .cpp 文件)" -ForegroundColor Green
}
else {
    Write-Host "  正在下载..." -ForegroundColor Gray
    New-Item -ItemType Directory -Force -Path "$imguiDir\backends" | Out-Null
    $base = "https://raw.githubusercontent.com/ocornut/imgui/docking"
    $files = @("imgui.h","imgui.cpp","imgui_demo.cpp","imgui_draw.cpp","imgui_tables.cpp",
               "imgui_widgets.cpp","imgui_internal.h","imconfig.h")
    foreach ($f in $files) {
        Invoke-WebRequest -Uri "$base/$f" -OutFile "$imguiDir/$f" -TimeoutSec 30
    }
    $backends = @("backends/imgui_impl_dx11.h","backends/imgui_impl_dx11.cpp",
                  "backends/imgui_impl_win32.h","backends/imgui_impl_win32.cpp")
    foreach ($f in $backends) {
        Invoke-WebRequest -Uri "$base/$f" -OutFile "$imguiDir/$f" -TimeoutSec 30
    }
    Write-Host "  Dear ImGui: 完成" -ForegroundColor Green
}

Write-Host ""
Write-Host "[4/6] SQLite3..." -ForegroundColor Yellow
if (Test-Path "$root\external\sqlite3\sqlite3.c") {
    Write-Host "  已存在" -ForegroundColor Green
}
else {
    Write-Host "  正在下载..." -ForegroundColor Gray
    New-Item -ItemType Directory -Force -Path "$root\external\sqlite3" | Out-Null
    $zip = "$env:TEMP\sqlite.zip"
    Invoke-WebRequest -Uri "https://www.sqlite.org/2025/sqlite-amalgamation-3490100.zip" -OutFile $zip
    Expand-Archive -Path $zip -DestinationPath "$env:TEMP\sqlite_extract" -Force
    $extracted = Get-ChildItem "$env:TEMP\sqlite_extract" -Directory | Select-Object -First 1
    Copy-Item "$($extracted.FullName)\sqlite3.h" "$root\external\sqlite3\"
    Copy-Item "$($extracted.FullName)\sqlite3.c" "$root\external\sqlite3\"
    Write-Host "  SQLite3: 完成" -ForegroundColor Green
}

Write-Host ""
Write-Host "[5/6] nlohmann/json..." -ForegroundColor Yellow
if (Test-Path "$root\external\nlohmann\json.hpp") {
    Write-Host "  已存在" -ForegroundColor Green
}
else {
    Write-Host "  正在下载..." -ForegroundColor Gray
    New-Item -ItemType Directory -Force -Path "$root\external\nlohmann" | Out-Null
    Invoke-WebRequest -Uri "https://raw.githubusercontent.com/nlohmann/json/develop/single_include/nlohmann/json.hpp" `
        -OutFile "$root\external\nlohmann\json.hpp" -TimeoutSec 30
    Write-Host "  nlohmann/json: 完成" -ForegroundColor Green
}

Write-Host ""
Write-Host "[6/6] CMake 配置..." -ForegroundColor Yellow
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  依赖安装完成！开始 CMake 配置..." -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

$buildDir = "$root\build"
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

Write-Host "  cmake configure..." -ForegroundColor Gray
cmake -B build -G "Visual Studio 17 2022" -A x64
if ($LASTEXITCODE -ne 0) {
    Write-Host "  CMake configure 失败！检查上面的错误信息。" -ForegroundColor Red
    pause
    exit 1
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Host "  依赖安装完成！CMake 已配置。" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host ""
Write-Host "  下一步:" -ForegroundColor White
Write-Host "    Debug 构建:   scripts\build_debug.bat" -ForegroundColor White
Write-Host "    Release 构建: scripts\build_release.bat" -ForegroundColor White
Write-Host "    启动程序:     scripts\launch.bat" -ForegroundColor White
Write-Host ""
Write-Host "  注意: 需要将 .gguf 模型文件放到 models\ 目录" -ForegroundColor Yellow
Write-Host ""

pause
