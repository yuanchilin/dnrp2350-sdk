# ============================================================================
#  DNRP2350A 一键构建 (apps) — cmake + ninja, 原生支持 ARM / RISC-V 双平台
#  用法:  .\build.ps1 <项目名> [-Platform arm|riscv] [-Clean]
#  示例:  .\build.ps1 06_terminal                  # ARM (默认)
#         .\build.ps1 06_terminal -Platform riscv  # 整机纯 RISC-V (Hazard3)
#         .\build.ps1 04_duel -Platform arm        # 对战工程建议 ARM (DUEL 需要 core0=ARM)
#  无参:  列出所有可用项目
#  产物:  apps/<proj>/build-<plat>/<proj>.uf2
#  依赖:  cmake, ninja; ARM 构建需 arm-none-eabi-gcc, RISC-V 构建需 riscv-none-elf-gcc
# ============================================================================
param(
    [string]$Proj = "",
    [ValidateSet("arm", "riscv")][string]$Platform = "arm",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"
$ROOT     = $PSScriptRoot
$SDK      = "$ROOT\pico-sdk"
$APPS_DIR = "$ROOT\apps"

# ---- 平台参数 → SDK 平台名 + 构建目录 ----
$platSdk = if ($Platform -eq "riscv") { "rp2350-riscv" } else { "rp2350-arm-s" }
$platDir = "build-" + $Platform
Write-Host "目标平台: $($Platform.ToUpper()) ($platSdk)" -ForegroundColor Cyan

# ---- 预检: 依赖工具与子模块 ----
foreach ($tool in @("cmake", "ninja")) {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
        Write-Host "缺少依赖工具: $tool (请安装并加入 PATH)" -ForegroundColor Red
        exit 1
    }
}

# ---- 平台工具链: RISC-V 需要 riscv-none-elf-gcc (ARM 是 SDK 默认, 已装) ----
if ($Platform -eq "riscv" -and -not (Get-Command riscv-none-elf-gcc -ErrorAction SilentlyContinue)) {
    $rvPath = [Environment]::GetEnvironmentVariable('Path', 'User')
    $cand = $null
    foreach ($p in $rvPath -split ';') {
        if ($p -match 'xpack-riscv-none-elf-gcc') { $cand = $p; break }
    }
    if (-not $cand) {
        $def = "D:\Tools\xpack-riscv-none-elf-gcc-15.2.0-1\bin"
        if (Test-Path "$def\riscv-none-elf-gcc.exe") { $cand = $def }
    }
    if ($cand) {
        $env:Path += ";$cand"
        Write-Host "  RISC-V 工具链已自动加入 PATH: $cand" -ForegroundColor DarkGray
    } else {
        Write-Host "错误: 未找到 riscv-none-elf-gcc (RISC-V 构建必需)" -ForegroundColor Red
        exit 1
    }
}

if (-not (Test-Path "$SDK\pico_sdk_init.cmake")) {
    Write-Host "pico-sdk 缺失: 请先执行 'git submodule update --init --recursive'" -ForegroundColor Red
    exit 1
}

# ---- 无参: 列出项目 ----
if (-not $Proj) {
    Write-Host "用法: .\build.ps1 <项目名> [-Platform arm|riscv] [-Clean]`n"
    Get-ChildItem $APPS_DIR -Directory | ForEach-Object { Write-Host "  $($_.Name)" }
    exit 0
}

$projDir = "$APPS_DIR\$Proj"
if (-not (Test-Path $projDir)) {
    Write-Host "项目不存在: $Proj" -ForegroundColor Red
    exit 1
}

$buildDir = "$projDir\$platDir"
$uf2 = "$buildDir\$Proj.uf2"

Write-Host "============================================"
Write-Host " Build: $Proj [$($Platform.ToUpper())]"
Write-Host "============================================"

# ---- 1. cmake 配置 (缓存按平台隔离, 平台变更自动重建) ----
$needCfg = $false
if (-not (Test-Path "$buildDir\CMakeCache.txt") -or -not (Test-Path "$buildDir\build.ninja")) {
    # 缓存或 ninja 缺失 (含上次配置中断的半截缓存) → 重新配置
    if (Test-Path $buildDir) { Remove-Item -Recurse -Force $buildDir }
    $needCfg = $true
} elseif ($Clean) {
    Write-Host "  -Clean: 删除旧缓存并重新配置"
    Remove-Item -Recurse -Force $buildDir
    $needCfg = $true
} else {
    # 校验缓存源路径 + 平台, 防止坏缓存/跨平台误用
    $cacheHome = Select-String -Path "$buildDir\CMakeCache.txt" -Pattern "^CMAKE_HOME_DIRECTORY:INTERNAL=(.+)$"
    if ($cacheHome) {
        $cachedSrc = $cacheHome.Matches[0].Groups[1].Value
        $normalized = $projDir.Replace('\', '/').ToLowerInvariant()
        if ($cachedSrc.ToLowerInvariant() -ne $normalized) {
            Write-Host "  缓存源路径不匹配 ($cachedSrc), 自动清理重建"
            Remove-Item -Recurse -Force $buildDir
            $needCfg = $true
        }
    }
    # 缓存已因源路径不匹配被删时, 不再尝试读平台 (文件不存在)
    if (-not $needCfg) {
        $cachedPlat = Select-String -Path "$buildDir\CMakeCache.txt" -Pattern "^PICO_PLATFORM:STRING=(.+)$"
        if ($cachedPlat -and $cachedPlat.Matches[0].Groups[1].Value -ne $platSdk) {
            Write-Host "  缓存平台不匹配 ($($cachedPlat.Matches[0].Groups[1].Value) -> $platSdk), 自动清理重建"
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
        cmake -S $projDir -B $buildDir -G Ninja "-DCMAKE_PREFIX_PATH=$picotoolDir" `
            "-DPICO_PLATFORM=$platSdk" -DCMAKE_BUILD_TYPE=Release
    } else {
        cmake -S $projDir -B $buildDir -G Ninja "-DPICO_PLATFORM=$platSdk" -DCMAKE_BUILD_TYPE=Release
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
Write-Host "  构建成功: $Proj.uf2  ${sizeKB}KB  [$($Platform.ToUpper())]" -ForegroundColor Green
Write-Host "  产物: $uf2"
