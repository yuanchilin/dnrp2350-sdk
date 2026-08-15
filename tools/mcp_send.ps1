# ============================================================================
#  mcp_send.ps1 — MCP 串口服务一键发送 (免控制权) + 可选输出捕获
#
#  用法:
#    .\tools\mcp_send.ps1 -Cmd reboot              # 发命令 (内嵌换行, 免控制权)
#    .\tools\mcp_send.ps1 -Cmd duel -Capture 8     # 发命令并抓 8s 串口输出
#    .\tools\mcp_send.ps1 -Cmd help -Capture 3 -LineEnding LF
#
#  说明:
#    - MCP /send 的 lineEnding="" 分支不需要控制权 (原样写 command),
#      所以把换行符内嵌进 command 即可, 无需 request-control 那套。
#    - -Capture N 时: 先连 WebSocket 监听, 再发命令, 保证不错过输出。
# ============================================================================
param(
    [Parameter(Mandatory = $true)][string]$Cmd,
    [string]$MCP = "http://localhost:9721",
    [string]$ClientId = "mcp-send",
    [int]$Capture = 0,          # >0 时抓取 N 秒串口输出并打印
    [ValidateSet("LF", "CRLF", "NONE")][string]$LineEnding = "LF"
)

$ErrorActionPreference = "Stop"

# 换行处理: 默认 LF; NONE = 完全原样 (不带换行)
$le = switch ($LineEnding) {
    "LF"   { "" }       # 换行已内嵌进 command, 服务端不再追加
    "CRLF" { "" }
    "NONE" { "none" }   # 占位, 下面单独处理
}
$payload = $Cmd
if ($LineEnding -eq "LF")   { $payload = $Cmd + "`n" }
elseif ($LineEnding -eq "CRLF") { $payload = $Cmd + "`r`n" }
# NONE: 原样

function Send-Raw {
    $body = @{ clientId = $ClientId; command = $payload; lineEnding = "" } | ConvertTo-Json
    Invoke-RestMethod -Uri "$MCP/send" -Method Post -ContentType "application/json" -Body $body -TimeoutSec 5
}

if ($Capture -le 0) {
    $r = Send-Raw
    $r | ConvertTo-Json -Depth 2
    exit 0
}

# ---- 捕获模式: 先连 WS 再发命令 ----
$ws = [System.Net.WebSockets.ClientWebSocket]::new()
$ms = New-Object System.IO.MemoryStream
try {
    $wsUri = $MCP -replace '^http', 'ws'
$null = $ws.ConnectAsync([System.Uri]::new("$wsUri/"), [System.Threading.CancellationToken]::None).GetAwaiter().GetResult()
    $r = Send-Raw
    $buf = New-Object byte[] 65536
    $seg = [ArraySegment[byte]]::new($buf)
    $cts = [System.Threading.CancellationTokenSource]::new($Capture * 1000)
    while ($ws.State -eq [System.Net.WebSockets.WebSocketState]::Open) {
        try { $res = $ws.ReceiveAsync($seg, $cts.Token).GetAwaiter().GetResult() }
        catch { break }
        if ($res.MessageType -eq [System.Net.WebSockets.WebSocketMessageType]::Binary) { $ms.Write($buf, 0, $res.Count) }
        elseif ($res.MessageType -eq [System.Net.WebSockets.WebSocketMessageType]::Close) { break }
    }
    $t = [System.Text.Encoding]::UTF8.GetString($ms.ToArray())
    if ($t) { Write-Output $t.TrimEnd() } else { Write-Output "(no data)" }
}
finally { $ws.Dispose() }
