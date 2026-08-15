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

# ---- 预检: 依赖工具与子模块 ----
foreach ($tool in @("cmake", "ninja")) {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
        Write-Host "缺少依赖工具: $tool (请安装并加入 PATH)" -ForegroundColor Red
        exit 1
    }
}

# ---- RISC-V 交叉工具链: 若不在当前进程 PATH, 自动从用户 PATH / 默认安装点补入 ----
if (-not (Get-Command riscv-none-elf-gcc -ErrorAction SilentlyContinue)) {
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
        Write-Host "警告: 未找到 riscv-none-elf-gcc (04_duel 等含 RISC-V 核的工程需要它)" -ForegroundColor Yellow
    }
}
if (-not (Test-Path "$SDK\pico_sdk_init.cmake")) {
    Write-Host "pico-sdk 缺失: 请先执行 'git submodule update --init --recursive'" -ForegroundColor Red
    exit 1
}

# ---- 无参: 列出项目 ----
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
    if ($cacheHome) {
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
        cmake -S $projDir -B $buildDir -G Ninja "-DCMAKE_PREFIX_PATH=$picotoolDir"
    } else {
        cmake -S $projDir -B $buildDir -G Ninja
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
