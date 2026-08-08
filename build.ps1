# ============================================================================
#  DNRP2350A 一键构建 (arm-dev) — cmake + ninja
#  用法:  .\build.ps1 [项目名]
#  示例:  .\build.ps1 06_terminal
#  无参:  列出所有可用项目
#  产物:  arm-dev/<proj>/build/<proj>.uf2
#  依赖:  cmake, ninja, ARM 工具链 (arm-none-eabi-gcc)
# ============================================================================
param(
    [string]$Proj = "",
    [switch]$Clean
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
$needCfg = $false
if (-not (Test-Path "$buildDir\CMakeCache.txt")) {
    $needCfg = $true
} elseif ($Clean) {
    Write-Host "  -Clean: 删除旧缓存并重新配置"
    Remove-Item -Recurse -Force $buildDir
    $needCfg = $true
} else {
    # 校验缓存源路径是否与当前项目一致 (防止历史坏缓存 D:/Downloads/RP)
    $cacheHome = Select-String -Path "$buildDir\CMakeCache.txt" -Pattern "^CMAKE_HOME_DIRECTORY:INTERNAL=(.+)$"
    if ($cacheHome -and (Test-Path "$buildDir\CMakeCache.txt")) {
        $cachedSrc = $cacheHome.Matches[0].Groups[1].Value
        # CMake 缓存路径用正斜杠写盘, $projDir 是反斜杠, 统一为小写正斜杠再比较, 避免每次误判重建
        $normalized = $projDir.Replace('\', '/').ToLowerInvariant()
        if ($cachedSrc.ToLowerInvariant() -ne $normalized) {
            Write-Host "  缓存源路径不匹配 ($cachedSrc), 自动清理重建"
            Remove-Item -Recurse -Force $buildDir
            $needCfg = $true
        }
    }
}

if ($needCfg) {
    New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
    # 使用公共预编译 picotool, 避免每个项目联网下载/重新编译
    $picotoolDir = "$ROOT\tools\picotool"
    if (Test-Path "$picotoolDir\picotoolConfig.cmake") {
        Write-Host "  使用公共 picotool: $picotoolDir"
        cmake -S $projDir -B $buildDir -G Ninja "-DPICO_SDK_PATH=$SDK" "-DCMAKE_PREFIX_PATH=$picotoolDir"
    } else {
        cmake -S $projDir -B $buildDir -G Ninja "-DPICO_SDK_PATH=$SDK"
    }
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