# ============================================================================
#  DNRP2350A 一键烧录 — 经 MCP serial 发 reboot 进 bootloader + 盘符拷贝 UF2
#  用法:  .\flash.ps1 [uf2文件路径]
#  示例:  .\flash.ps1 .\arm-dev\06_terminal\build\06_terminal.uf2
#  依赖:  MCP serial 服务 (http://localhost:9721) 用于发送 reboot
# ============================================================================
param(
    [string]$Uf2 = ""
)

$ErrorActionPreference = "Stop"

# ---- 默认 UF2: 未指定时取最近修改的 arm-dev/*/build/*.uf2 ----
if (-not $Uf2) {
    $Uf2 = Get-ChildItem "$PSScriptRoot\arm-dev\*\build\*.uf2" |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1 -ExpandProperty FullName
}
if (-not $Uf2 -or -not (Test-Path $Uf2)) {
    Write-Host "UF2 不存在: $Uf2" -ForegroundColor Red
    exit 1
}

$sizeKB = [math]::Round((Get-Item $Uf2).Length / 1KB)
Write-Host "============================================"
Write-Host " DNRP2350A 一键烧录"
Write-Host " 固件: $([System.IO.Path]::GetFileName($Uf2))  ${sizeKB}KB"
Write-Host "============================================"

# ---- 1. 通过 MCP serial HTTP 接口发 reboot（走 MCP 持久连接，可靠且不占用串口）----
# 先发一串回车清空板子 shell 的 UART 输入缓冲，避免残留字符污染 reboot 命令
try {
    $flush = @{ command = "`r"; lineEnding = ""; timeout = 300 } | ConvertTo-Json
    Invoke-RestMethod -Uri "http://localhost:9721/send" -Method Post `
        -ContentType "application/json" -Body $flush -TimeoutSec 3 | Out-Null
} catch { /* 忽略清空失败 */ }

$body = @{ command = "reboot`r"; lineEnding = "" } | ConvertTo-Json
try {
    Invoke-RestMethod -Uri "http://localhost:9721/send" -Method Post `
        -ContentType "application/json" -Body $body -TimeoutSec 3 | Out-Null
    Write-Host "   已通过 MCP 发送 reboot"
} catch {
    Write-Host "   MCP 发送失败: $_" -ForegroundColor Yellow
    Write-Host "   请手动: 按住 BOOT → 点 RESET → 松开" -ForegroundColor Yellow
}

# ---- 2. 轮询 RP2350 盘符并拷贝 UF2 ----
Write-Host "   等待 RP2350 盘符..."
$ok = $false
for ($i = 0; $i -lt 60; $i++) {
    Start-Sleep -Milliseconds 500
    foreach ($d in @("D","E","F","G","H","I","J","K","L","M","N","O","P","Q","R","S","T","U","V","W","X","Y","Z")) {
        $vol = (Get-WmiObject Win32_LogicalDisk -Filter "DeviceID='${d}:'").VolumeName
        if ($vol -match "RP2|RP2350") {
            Copy-Item $Uf2 "${d}:\" -Force
            $ok = $true
            Write-Host "   烧录成功 (${d}:)" -ForegroundColor Green
            break
        }
    }
    if ($ok) { break }
}
if (-not $ok) {
    Write-Host "   超时 (30s) 未检测到 RP2350" -ForegroundColor Red
    Write-Host "   手动: 按住 BOOT → 点 RESET → 松开" -ForegroundColor Yellow
    exit 1
}