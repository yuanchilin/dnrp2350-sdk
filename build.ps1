# ============================================================================
#  DNRP2350A 一键构建 (arm-dev) — cmake + ninja
#  用法:  .\build.ps1 [项目名]
#  示例:  .\build.ps1 06_terminal
#  无参:  列出所有可用项目
#  产物:  arm-dev/<proj>/build/<proj>.uf2
#  依赖:  cmake, ninja, ARM 工具链 (arm-none-eabi-gcc)
# ============================================================================
param(
    [string]$Proj = ""
)

$ErrorActionPreference = "Stop"
$ROOT    = $PSScriptRoot
$SDK     = "$ROOT\pico-sdk"
$ARM_DIR = "$ROOT\arm-dev"

# ---- 无参: 列出所有项目 ----
if (-not $Proj) {
    Write-Host "用法: .\build.ps1 <项目名>`n"
    Get-ChildItem $ARM_DIR -Directory | ForEach-Object { Write-Host "  $($_.Name)" }
    exit 0
}

$projDir = "$ARM_DIR\$Proj"
if (-not (Test-Path $projDir)) {
    Write-Host "项目不存在: $Proj" -ForegroundColor Red
    exit 1
}

$buildDir = "$projDir\build"
$uf2 = "$buildDir\$Proj.uf2"

Write-Host "============================================"
Write-Host " Build: $Proj"
Write-Host "============================================"

# ---- 1. cmake 配置 ----
if (-not (Test-Path "$buildDir\CMakeCache.txt")) {
    New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
    cmake -S $projDir -B $buildDir -G Ninja -DPICO_SDK_PATH=$SDK
    if ($LASTEXITCODE -ne 0) { Write-Host "CMake 配置失败" -ForegroundColor Red; exit 1 }
} else {
    Write-Host "  使用已有 CMake 缓存"
}

# ---- 2. ninja 编译 ----
cmake --build $buildDir
if ($LASTEXITCODE -ne 0) { Write-Host "构建失败" -ForegroundColor Red; exit 1 }

if (-not (Test-Path $uf2)) {
    Write-Host "未生成 $uf2" -ForegroundColor Red
    exit 1
}
$sizeKB = [math]::Round((Get-Item $uf2).Length / 1KB)
Write-Host "  构建成功: $Proj.uf2  ${sizeKB}KB" -ForegroundColor Green
Write-Host "  产物: $uf2"