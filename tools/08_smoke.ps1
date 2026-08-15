# ============================================================================
#  08_smoke.ps1 — 08 BadUSB 注入终端 全量冒烟测试
#
#  覆盖:  shell 命令 / SD(FatFs) / TUI 进入退出 / console(LCD) 路径 /
#         OTG 注入 (rawkey_listener 抓目标机按键流 + 解码比对)
#
#  依赖:  板子烧录 08 固件 + MCP serial (http://localhost:9721) +
#         USB_OTG 接到本机 (注入会在这台电脑上打开记事本)
#
#  用法:  .\tools\08_smoke.ps1
# ============================================================================
param([int]$WaitSec = 20)

$ErrorActionPreference = "Stop"
$MCP = "http://localhost:9721"
$script:PASS = 0; $script:FAIL = 0

function Check([string]$name, [bool]$ok, [string]$detail = "") {
    if ($ok) { Write-Host ("  PASS: {0}" -f $name) -ForegroundColor Green; $script:PASS++ }
    else     { Write-Host ("  FAIL: {0}  {1}" -f $name, $detail) -ForegroundColor Red; $script:FAIL++ }
}

# ---- MCP: 发送命令并抓取回显 ----
function Send-Mcp([string]$cmd, [int]$timeoutMs = 4000) {
    $ws = [System.Net.WebSockets.ClientWebSocket]::new()
    $ms = New-Object System.IO.MemoryStream
    try {
        $body = @{ command = $cmd; lineEnding = ""; timeout = 200 } | ConvertTo-Json
        Invoke-RestMethod -Uri "$MCP/send" -Method Post -ContentType "application/json" -Body $body -TimeoutSec 3 | Out-Null
        $null = $ws.ConnectAsync([System.Uri]::new("ws://localhost:9721/"), [System.Threading.CancellationToken]::None).GetAwaiter().GetResult()
        $buf = New-Object byte[] 65536
        $seg = [ArraySegment[byte]]::new($buf)
        $cts = [System.Threading.CancellationTokenSource]::new($timeoutMs)
        while ($ws.State -eq [System.Net.WebSockets.WebSocketState]::Open) {
            try { $res = $ws.ReceiveAsync($seg, $cts.Token).GetAwaiter().GetResult() } catch { break }
            if ($res.MessageType -eq [System.Net.WebSockets.WebSocketMessageType]::Binary) { $ms.Write($buf, 0, $res.Count) }
            elseif ($res.MessageType -eq [System.Net.WebSockets.WebSocketMessageType]::Close) { break }
        }
        return [System.Text.Encoding]::UTF8.GetString($ms.ToArray())
    } finally { $ws.Dispose() }
}

# ---- 按键流解码 (目标机收到的键 → 文本) ----
function Decode-Keys([string[]]$lines) {
    $base = @{ '0xBE'='.'; '0xBC'=','; '0xBF'='/'; '0xBA'=';'; '0xDB'='['; '0xDD'=']'; '0xDC'='\'; '0xBD'='-'; '0xBB'='='; '0xC0'='`' }
    $shifted = @{ '1'='!'; '2'='@'; '3'='#'; '4'='$'; '5'='%'; '6'='^'; '7'='&'; '8'='*'; '9'='('; '0'=')'; '0xBD'='_'; '0xBB'='+'; '0xDB'='{'; '0xDD'='}'; '0xDC'='|'; '0xBA'=':'; '0xDE'='"'; '0xBC'='<'; '0xBE'='>'; '0xBF'='?'; '0xC0'='~' }
    $sb = New-Object System.Text.StringBuilder
    $shift = $false
    foreach ($ln in $lines) {
        if ($ln -notmatch 'VID_CAFE' -or $ln -notmatch '\sDOWN\s') { continue }
        if ($ln -match '\sDOWN\s+SHIFT\s') { $shift = $true; continue }
        if ($ln -match '\sDOWN\s+LWIN\s')  { [void]$sb.Append('[GUI]'); continue }
        if ($ln -match '\sDOWN\s+ENTER\s') { [void]$sb.AppendLine(); continue }
        if ($ln -match '\sDOWN\s+SPACE\s') { [void]$sb.Append(' '); continue }
        if ($ln -match '\sDOWN\s+(\S+)\s+Make=') {
            $k = $Matches[1]
            if ($k -match '^[A-Z]$') { [void]$sb.Append($shift ? $k : $k.ToLower()) }
            elseif ($k -match '^\d$') { [void]$sb.Append($shift ? $shifted[$k] : $k) }
            elseif ($shift -and $shifted.ContainsKey($k)) { [void]$sb.Append($shifted[$k]) }
            elseif ($base.ContainsKey($k)) { [void]$sb.Append($base[$k]) }
            $shift = $false
        }
    }
    return $sb.ToString()
}

Write-Host "=== 08 全量冒烟测试 ==="

# ---- 0. 链路 ----
Write-Host "`n[0] MCP 链路"
$boot = Send-Mcp "`r" 3000
Check "串口响应 (提示符)" ($boot -match '\$') "MCP/板子未响应"

# ---- 1. shell ----
Write-Host "`n[1] Shell 命令"
$r = Send-Mcp "help`r"
Check "help 列出 badusb"  ($r -match 'badusb')
Check "help 列出 tui"     ($r -match 'tui')
Check "help 列出 ls"      ($r -match 'ls')
$r = Send-Mcp "sysinfo`r"
Check "sysinfo 有响应"     ($r -match 'RP2350|RP2040|MHz|SD')

# ---- 2. SD / FatFs ----
Write-Host "`n[2] SD / FatFs"
$r = Send-Mcp "ls`r"
Check "ls 列出 hello.txt"  ($r -match 'hello\.txt')

# ---- 3. TUI 进入/退出 ----
Write-Host "`n[3] TUI (进入/退出)"
Send-Mcp "tui`r" 1500 | Out-Null
Start-Sleep -Milliseconds 800
$r = Send-Mcp ([string][char]0x1B) 2500        # Esc 退出 TUI
Check "TUI 退出回到 shell 提示符" ($r -match '\$')

# ---- 3.5 异构对战 (duel 模块) ----
Write-Host "`n[3.5] 异构对战 (ARM vs RISC-V)"
$r = Send-Mcp "help`r" 2500
Check "help 列出 duel/out" ($r -match 'duel' -and $r -match '\bout\b')
$r = Send-Mcp "duel`r" 10000
Check "duel 出战报 WINNER" ($r -match 'WINNER')
Check "duel 超时前回到提示符" ($r -match '\$')

# ---- 4. console/LCD 代码路径 (draw 不崩溃 + 恢复提示符) ----
Write-Host "`n[4] console/LCD 路径"
$r = Send-Mcp "clear`r"
Check "clear 后 shell 仍响应" ($r -match '\$')
$r = Send-Mcp "echo hello-lcd`r"
Check "echo 输出"            ($r -match 'hello-lcd')

# ---- 5. OTG 注入: 真实按键流 ----
Write-Host "`n[5] OTG 注入 (目标机按键捕获)"
$LOG = Join-Path $PSScriptRoot "smoke_keys.log"
Remove-Item $LOG -ErrorAction SilentlyContinue
$j = Start-Job -ScriptBlock {
    param($script, $log)
    & $script -DurationSec 45 -LogFile $log
} -ArgumentList (Join-Path $PSScriptRoot "rawkey_listener.ps1"), $LOG
Start-Sleep -Seconds 3

$r = Send-Mcp "badusb demo`r" ($WaitSec * 1000)
Check "注入执行 (USB HID ready + Done)" ($r -match 'USB HID ready' -and $r -match 'Done\.')

Start-Sleep -Seconds 4
Stop-Job $j -ErrorAction SilentlyContinue; Remove-Job $j -Force -ErrorAction SilentlyContinue

if (Test-Path $LOG) {
    $keys = Get-Content $LOG | Where-Object { $_ -match 'VID_CAFE' }
    Check "捕获到板子按键事件" ($keys.Count -gt 30) "仅 $($keys.Count) 个"
    $decoded = Decode-Keys $keys
    Write-Host "  --- 目标机实际收到的文本 ---"
    Write-Host $decoded
    Check "按键流解码含示例文本" ($decoded -match 'Hello from RP2350 BadUSB')
} else {
    Check "监听器日志生成" $false "未生成 $LOG"
}

# ---- 汇总 ----
Write-Host "`n================================"
Write-Host (" 结果: {0} PASS / {1} FAIL" -f $script:PASS, $script:FAIL)
Write-Host "================================"
if ($script:FAIL -gt 0) { exit 1 } else { exit 0 }
