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

# ---- 1. 通过 MCP serial HTTP 接口发命令（走 MCP 持久连接，可靠且不占用串口）----
# 封装 /send, 失败返回 $false 让调用方能优雅降级到手动
function Send-Mcp([string]$cmd, [string]$le = "", [int]$to = 200) {
    $body = @{ command = $cmd; lineEnding = $le; timeout = $to } | ConvertTo-Json
    try {
        Invoke-RestMethod -Uri "http://localhost:9721/send" -Method Post `
            -ContentType "application/json" -Body $body -TimeoutSec 3 | Out-Null
        return $true
    } catch { return $false }
}

# 先发一串回车清空板子 shell 的 UART 输入缓冲，避免残留字符污染 reboot 命令
for ($k = 0; $k -lt 5; $k++) {
    if (-not (Send-Mcp "`r")) { break }
    Start-Sleep -Milliseconds 150
}

# 若板子正停留在 TUI (tui 命令进入的全屏界面), 发 Ctrl+C 强制退出 TUI,
# 否则 reboot 会被 TUI 当成按键拦截, 无法进入 bootloader
for ($k = 0; $k -lt 3; $k++) {
    if (-not (Send-Mcp ([char]0x03))) { break }
    Start-Sleep -Milliseconds 200
}

if (Send-Mcp "reboot`r") {
    Write-Host "   已通过 MCP 发送 reboot"
} else {
    Write-Host "   MCP 发送失败: 请手动按住 BOOT → 点 RESET → 松开" -ForegroundColor Yellow
}

# ---- 2. 轮询 RP2350 盘符并拷贝 UF2 ----
# 单次 WMI 查询移动盘 (DriveType=2) 再过滤卷标, 替代逐盘符轮询的串行开销
Write-Host "   等待 RP2350 盘符..."
$ok = $false
$waitMs = 30000; $poll = 250
for ($elapsed = 0; $elapsed -le $waitMs; $elapsed += $poll) {
    Start-Sleep -Milliseconds $poll
    $drives = Get-CimInstance Win32_LogicalDisk -Filter "DriveType=2" |
        Where-Object { $_.DeviceID -and $_.VolumeName -match "^(RP2350|RPI-RP2)$" }
    foreach ($d in $drives) {
        try {
            Copy-Item $Uf2 "$($d.DeviceID)\" -Force -ErrorAction Stop
            $ok = $d.DeviceID
            break
        } catch { }
    }
    if ($ok) { break }
}
if ($ok) {
    Write-Host "   烧录成功 ($ok)" -ForegroundColor Green
} else {
    Write-Host "   超时 (${waitMs}s) 未检测到 RP2350" -ForegroundColor Red
    Write-Host "   手动: 按住 BOOT → 点 RESET → 松开" -ForegroundColor Yellow
    exit 1
}
