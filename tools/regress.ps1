# ============================================================================
#  regress.ps1 — 通用真机回归: 编译 → 免按键烧录 → 发命令 → 断言输出
#
#  用法:
#    .\tools\regress.ps1 -Proj 04_duel -Cmd duel -Expect "WINNER"
#    .\tools\regress.ps1 -Proj 08_badusb_terminal -Cmd "badusb list" -Expect "No" -NoFlash
#
#  参数:
#    -Proj     项目名 (apps/<Proj>)
#    -Cmd      发给 shell 的命令 (默认 "duel")
#    -Expect   输出中必须出现的正则 (默认 "PASS|OK")
#    -Platform 构建平台 arm|riscv (默认 arm, 透传给 build.ps1)
#    -NoFlash  跳过编译+烧录, 直接对当前固件发命令验证
#    -NoReboot AutoReboot 失败时手动按键的兜底 (与 -NoFlash 互斥)
#    -WaitMs   捕获窗口 (默认 8000)
#
#  成功: 打印断言命中的行 + "PASS"; 失败: 非零退出码 (2=断言失败, 1=流程失败)
# ============================================================================
param(
    [Parameter(Mandatory = $true)][string]$Proj,
    [string]$Cmd = "duel",
    [string]$Expect = "PASS|OK",
    [ValidateSet("arm", "riscv")][string]$Platform = "arm",
    [switch]$NoFlash,
    [switch]$NoReboot,
    [int]$WaitMs = 8000,
    [string]$MCP = "http://localhost:9721",
    [string]$Root = "D:\Downloads\Agent\RP"
)
$ErrorActionPreference = "Stop"

$Tools = Join-Path $Root "tools"
function Write-Step($m) { Write-Host "`n== $m ==" -ForegroundColor Cyan }

# ---- 1/4 编译 (复用通用 build.ps1) ----
if (-not $NoFlash) {
    Write-Step "1/4 编译 $Proj [$Platform]"
    & (Join-Path $Root "build.ps1") $Proj -Platform $Platform
    if ($LASTEXITCODE -ne 0) { Write-Host "编译失败" -ForegroundColor Red; exit 1 }
    $uf2 = Join-Path $Root "apps\$Proj\build-$Platform\$Proj.uf2"
    if (-not (Test-Path $uf2)) { Write-Host "编译失败: 无 UF2" -ForegroundColor Red; exit 1 }
}

# ---- 2/4 烧录 (免按键; 复用通用 flash.ps1) ----
Write-Step "2/4 烧录"
if ($NoFlash) {
    Write-Host "   (跳过烧录, 直接验证当前固件)"
} else {
    & (Join-Path $Root "flash.ps1") (Join-Path $Root "apps\$Proj\build-$Platform\$Proj.uf2")
    if ($LASTEXITCODE -ne 0) { Write-Host "烧录失败" -ForegroundColor Red; exit 1 }
}

# ---- 3/4 触发命令并捕获 (先连 WS 再发送, 不错过输出) ----
Write-Step "3/4 执行 '$Cmd' 并捕获输出"
Start-Sleep -Seconds 2   # 等固件起来 + shell 就绪
$wsUri = $MCP -replace '^http', 'ws'
$ws = [System.Net.WebSockets.ClientWebSocket]::new()
$ms = New-Object System.IO.MemoryStream
try {
    $null = $ws.ConnectAsync([System.Uri]::new("$wsUri/"), [System.Threading.CancellationToken]::None).GetAwaiter().GetResult()
    $body = @{ clientId = "regress"; command = "$Cmd`n"; lineEnding = "" } | ConvertTo-Json
    Invoke-RestMethod -Uri "$MCP/send" -Method Post -ContentType "application/json" -Body $body -TimeoutSec 5 | Out-Null
    $buf = New-Object byte[] 65536
    $seg = [ArraySegment[byte]]::new($buf)
    $cts = [System.Threading.CancellationTokenSource]::new($WaitMs)
    while ($ws.State -eq [System.Net.WebSockets.WebSocketState]::Open) {
        try { $res = $ws.ReceiveAsync($seg, $cts.Token).GetAwaiter().GetResult() } catch { break }
        if ($res.MessageType -eq [System.Net.WebSockets.WebSocketMessageType]::Binary) { $ms.Write($buf, 0, $res.Count) }
        elseif ($res.MessageType -eq [System.Net.WebSockets.WebSocketMessageType]::Close) { break }
    }
    $t = [System.Text.Encoding]::UTF8.GetString($ms.ToArray())
    if (-not $t) { Write-Host "无串口输出 (串口可能未就绪)" -ForegroundColor Yellow }
} finally { $ws.Dispose() }

# ---- 4/4 断言 ----
Write-Step "4/4 断言 '$Expect'"
Write-Host $t
if ($t -match $Expect) {
    Write-Host "`nPASS: 输出命中 '$Expect'" -ForegroundColor Green
    exit 0
} else {
    Write-Host "`nFAIL: 输出未命中 '$Expect'" -ForegroundColor Red
    exit 2
}
