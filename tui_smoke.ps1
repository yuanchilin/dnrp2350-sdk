# ============================================================================
#  tui_smoke.ps1 — DNRP2350A TUI 冒烟回归测试 (真机)
#  依赖: MCP serial 服务 (http://localhost:9721)
#  用法:
#    .\tui_smoke.ps1                                              # 测当前固件
#    .\tui_smoke.ps1 -Uf2 .\arm-dev\07_pio_lcd\build\07_pio_lcd.uf2   # 先刷再测
#  检查项: 进入 TUI 全屏绘制 / 方向键最小重绘 / Enter 与 Esc 两条退出路径 /
#          退出后回到 shell 提示符 (绝不把板子留在 TUI)
# ============================================================================
param(
    [string]$Port = "COM5",
    [int]$Baud = 115200,
    [string]$Uf2 = "",
    [string]$Api = "http://localhost:9721",
    [int]$DownDeltaMax = 150     # 每次方向键允许的最大重绘字节数 (防闪阈值)
)
$ErrorActionPreference = "Stop"

# PowerShell 5.1 需显式加载 System.Net.Http, 否则 [System.Net.Http.HttpClient] 类型不可用
try { Add-Type -AssemblyName System.Net.Http -ErrorAction Stop } catch { }

$script:pass = 0; $script:fail = 0
function Report([string]$name, [bool]$ok, [string]$info) {
    if ($ok) { $script:pass++ } else { $script:fail++ }
    Write-Host ("{0}  {1,-30} {2}" -f ($(if ($ok) { "PASS" } else { "FAIL" })), $name, $info)
}
$CID = "tui-smoke-" + [guid]::NewGuid().ToString()
$ESC = [char]0x1b
Write-Host ("== TUI smoke test  (port={0} @ {1} baud) ==" -f $Port, $Baud)

function Send([string]$cmd) {
    $json = @{ command = $cmd; lineEnding = "" } | ConvertTo-Json
    Invoke-RestMethod -Uri "$Api/send" -Method Post -ContentType "application/json" -Body $json -TimeoutSec 5 | Out-Null
}
# 读串口累积历史 (SSE 首条含 text 的 data 即全量缓冲) 并把 JSON 转义解码成真实字符
function Snap {
    $hc = [System.Net.Http.HttpClient]::new()
    try {
        $resp = $hc.GetAsync("$Api/events?clientId=$CID&name=smoke", [System.Net.Http.HttpCompletionOption]::ResponseHeadersRead).GetAwaiter().GetResult()
        $sr = [System.IO.StreamReader]::new($resp.Content.ReadAsStreamAsync().GetAwaiter().GetResult())
        $t = ""
        for ($k = 0; $k -lt 8; $k++) {
            $l = $sr.ReadLine()
            if ($null -eq $l) { break }
            if ($l -like 'data: *') {
                $s = $l.Substring(6)
                if ($s -match '"text":"(.*)"\s*}$') {
                    $t = $Matches[1] -replace '\\u001b', ([string]$ESC) -replace '\\n', "`n" -replace '\\r', '' -replace '\\t', "`t" -replace '\\"', '"'
                    break
                }
            }
        }
    } finally { $hc.Dispose() }
    return $t
}
# 本次按键产生的增量文本: 断言只看增量, 不受全量历史污染
function DeltaOf([string]$cur, [string]$prev) {
    if ($prev.Length -ge $cur.Length) { return "" }
    return $cur.Substring($prev.Length)
}
function HasEnterAlt([string]$t) { return $t.Contains([string]::new($ESC) + "[?1049h") }
function HasExitAlt ([string]$t) { return $t.Contains([string]::new($ESC) + "[?1049l") }
function HasPrompt   ([string]$t) { return $t.Contains("$ ") }
# 顶过开机 photo 显示后的按键等待 (1 次回车即可), 再轮询等待 shell 提示符就绪, 就绪即停
function PrimeShell {
    Send "`r"
    $deadline = [DateTime]::UtcNow.AddSeconds(8)
    do {
        if ((Snap).IndexOf("$ ") -ge 0) { return }   # shell 就绪
        Start-Sleep -Milliseconds 250
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "shell 未就绪 (开机 photo 等待超时)"
}

try {
    # ---- 接管串口 ----
    try { Invoke-RestMethod -Uri "$Api/disconnect" -Method Post -ContentType "application/json" -Body '{}' -TimeoutSec 5 | Out-Null } catch {}
    $json = @{ port = $Port; baudRate = $Baud; clientId = $CID } | ConvertTo-Json
    Invoke-RestMethod -Uri "$Api/connect" -Method Post -ContentType "application/json" -Body $json -TimeoutSec 5 | Out-Null
    Write-Host "[connected]"

    # ---- 前置: 若板子停在 TUI 先退出来 (Ctrl+C 强制退出) ----
    for ($i = 0; $i -lt 3; $i++) { Send ([char]0x03); Start-Sleep -Milliseconds 300 }

    if ($Uf2) {
        Send "reboot`r"
        $dsk = $null
        for ($el = 0; $el -le 40000; $el += 300) {
            Start-Sleep -Milliseconds 300
            $drives = Get-CimInstance Win32_LogicalDisk -Filter "DriveType=2" | Where-Object { $_.VolumeName -match "^(RP2350|RPI-RP2)$" }
            foreach ($d in $drives) {
                try { Copy-Item $Uf2 "$($d.DeviceID)\" -Force -ErrorAction Stop; $dsk = $d.DeviceID; break } catch {}
            }
            if ($dsk) { break }
        }
        if (-not $dsk) { throw "bootloader 盘符超时, 请手动 BOOT+RESET" }
        Report "flash uf2" $true $dsk
        Start-Sleep -Milliseconds 2500
    }

    # ---- 让 shell 越过开机 photo_wait, 就绪 ----
    PrimeShell

    # ---- 1. 进入 TUI ----
    $prev = Snap
    $s = $null; Send "tui`r"; Start-Sleep -Milliseconds 400; $s = Snap
    $d0 = $s.Length - $prev.Length
    $d0t = DeltaOf $s $prev
    # 30x8 屏全量绘制约 500B, 阈值取 400 (原 80x24 是 >900)
    # 缓冲持久累积, 增量 d0t 的 ?1049h 可能被历史污染截断, 用全量 $s 判断备用屏切换
    Report "enter TUI (full paint)" ($d0 -gt 400 -and $s.Contains([string]::new($ESC) + "[?1049h")) "+$d0 bytes"

    # ---- 2. About 界面 (第 1 项, 初始即选中) ----
    $prevSnap = $s
    Send "`r"; Start-Sleep -Milliseconds 400; $s = Snap
    $dt = DeltaOf $s $prevSnap
    Report "About page renders" ($dt.Contains("RP2350A M33")) "+$($dt.Length) bytes"
    $prevSnap = $s
    Send $ESC; Start-Sleep -Milliseconds 400; $s = Snap
    $dt = DeltaOf $s $prevSnap
    Report "About back to main menu" ($dt.Contains("DNRP2350A TUI")) "+$($dt.Length) bytes"

    # ---- 3. DOWN → SD files (最小重绘) ----
    $prevSnap = $s
    Send ($ESC + "[B"); Start-Sleep -Milliseconds 400; $s = Snap
    $dt = DeltaOf $s $prevSnap
    Report "DOWN -> SD files (min redraw)" ($dt.Length -ge 1 -and $dt.Length -le $DownDeltaMax) "+$($dt.Length) bytes"

    # ---- 4. SD files 界面 (第 2 项) ----
    $prevSnap = $s
    Send "`r"; Start-Sleep -Milliseconds 400; $s = Snap
    $dt = DeltaOf $s $prevSnap
    Report "SD files page renders" ($dt.Contains("SD root") -or $dt.Contains("No SD card")) "+$($dt.Length) bytes"
    $prevSnap = $s
    Send $ESC; Start-Sleep -Milliseconds 400; $s = Snap
    $dt = DeltaOf $s $prevSnap
    Report "SD files back to main menu" ($dt.Contains("DNRP2350A TUI")) "+$($dt.Length) bytes"

    # ---- 5. DOWN → Exit TUI (最小重绘) ----
    $prevSnap = $s
    Send ($ESC + "[B"); Start-Sleep -Milliseconds 400; $s = Snap
    $dt = DeltaOf $s $prevSnap
    Report "DOWN -> Exit TUI (min redraw)" ($dt.Length -ge 1 -and $dt.Length -le $DownDeltaMax) "+$($dt.Length) bytes"

    # ---- 6. Enter → 退出 (第 3 项 Exit TUI) ----
    $prevSnap = $s
    Send "`r"; Start-Sleep -Milliseconds 400; $s = Snap
    $dt = DeltaOf $s $prevSnap
    Report "ENTER -> exit to shell" ((HasExitAlt $dt) -and (HasPrompt $dt) -and (-not $dt.Contains("move:")))

    # ---- 7. Ctrl+C → 强制退出 (全新进入 TUI 后直接 Ctrl+C) ----
    $prevSnap = $s
    Send "tui`r"; Start-Sleep -Milliseconds 400
    Send ([char]0x03); Start-Sleep -Milliseconds 500
    $s = Snap
    # 断言: 增量里应出现 ?1049l(退出备用屏), 其后紧接 shell 提示符
    $dt = DeltaOf $s $prevSnap
    $escIdx = $dt.LastIndexOf([string]::new($ESC) + "[?1049l")
    $afterEsc = if ($escIdx -ge 0) { $dt.Substring($escIdx) } else { "" }
    Report "Ctrl+C -> force exit to shell" (($escIdx -ge 0) -and $afterEsc.Contains("$ "))

    Write-Host ""
    Write-Host ("Result: {0} pass, {1} fail" -f $script:pass, $script:fail)
}
finally {
    # ---- 收尾: 绝不把板子留在 TUI; 释放串口 ----
    try {
        for ($i = 0; $i -lt 3; $i++) { Send ([char]0x03); Start-Sleep -Milliseconds 300 }
    } catch {}
    # 不调用 /disconnect: MCP 串口由服务持久持有, 保持打开供终端继续使用
    Write-Host "[cleanup done (serial kept open)]"
}

if ($script:fail -gt 0) { exit 1 }