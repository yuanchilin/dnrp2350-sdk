# ============================================================================
#  monitor.ps1 — 一键启动 DNRP2350A 注入监控台
#
#  功能: 启动目标机按键监听器 (rawkey_listener.ps1) + 内嵌网页服务,
#        自动打开浏览器。监控台左侧嵌板子侧 MCP 串口终端, 右侧实时显示
#        目标机收到的按键流 (板子来源自动标绿)。
#
#  用法:  .\tools\monitor.ps1 [-Port 8088]
#  打开:  http://localhost:<Port>/
#  停止:  关闭两个最小化的 PowerShell 窗口
# ============================================================================
param([int]$Port = 8088)

$ErrorActionPreference = "Stop"
$LOG = Join-Path $PSScriptRoot "rawkey_demo.log"

# 找 python
$py = (Get-Command python -ErrorAction SilentlyContinue).Source
if (-not $py) { $py = "D:\Miniconda3\envs\default\python.exe" }
if (-not (Test-Path $py)) { Write-Host "未找到 python, 请修改本脚本里的 \$py" -ForegroundColor Red; exit 1 }

# ---- 内嵌监控台服务 (python, 页面 HTML 也在里面) ------------------------------
$serverPy = @'
import http.server, json, os, re, sys
LOG = sys.argv[1] if len(sys.argv) > 1 else "rawkey_demo.log"
PORT = int(sys.argv[2]) if len(sys.argv) > 2 else 8088
HTML = r"""<!DOCTYPE html>
<html lang="zh-CN">
<head><meta charset="utf-8"><title>DNRP2350A 注入监控台</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#0d1117;color:#c9d1d9;font:13px Consolas,Monaco,monospace;height:100vh;display:flex;flex-direction:column}
#bar{background:#161b22;padding:8px 14px;border-bottom:1px solid #30363d;display:flex;align-items:center;gap:12px}
#bar h1{font-size:14px;color:#58a6ff;font-weight:600}
#bar .hint{color:#8b949e;font-size:11px}
#wrap{flex:1;display:flex;min-height:0}
#board{flex:1 1 45%;border-right:1px solid #30363d;background:#000}
#keys{flex:1 1 55%;display:flex;flex-direction:column;min-width:0}
.cap{padding:6px 12px;background:#161b22;border-bottom:1px solid #30363d;color:#8b949e;font-size:11px}
#log{flex:1;overflow-y:auto;padding:8px 12px;line-height:1.55}
.line{white-space:pre}
.line.down{color:#7ee787}
.line.up{color:#6e7681}
.line.board{color:#3fb950;font-weight:bold}
.line.board.down{color:#7ee787}
.line.sep{color:#f0883e;margin:6px 0}
</style></head>
<body>
<div id="bar"><h1>DNRP2350A 注入监控台</h1><span class="hint">左: 板子侧 UART &nbsp;|&nbsp; 右: 目标机实时按键流 (板子=VID_CAFE&amp;PID_4001)</span></div>
<div id="wrap">
<iframe id="board" src="http://localhost:9721/"></iframe>
<div id="keys">
<div class="cap">目标机收到的按键事件 (DOWN 亮绿 / UP 灰, 板子来源加粗)</div>
<div id="log"><div class="line" style="color:#484f58">等待按键事件… (板子发 e 注入后这里实时滚动)</div></div>
</div>
</div>
<script>
let since=0;
async function poll(){
  try{
    const r=await fetch('/api/log?since='+since,{cache:'no-store'});
    if(!r.ok)return;
    const j=await r.json();
    since=j.size;
    if(j.lines){
      const el=document.getElementById('log');
      el.innerHTML='';
      for(const ln of j.lines.split('\n')){
        if(!ln)continue;
        const div=document.createElement('div');
        div.className='line';
        div.textContent=ln;
        if(ln.includes('VID_CAFE'))div.classList.add('board');
        if(ln.includes('DOWN'))div.classList.add('down');
        if(ln.includes('UP'))div.classList.add('up');
        if(ln.includes('==')){div.classList.remove('board','down','up');div.classList.add('sep');}
        el.appendChild(div);
      }
      el.scrollTop=el.scrollHeight;
    }
  }catch(e){}
}
setInterval(poll,400);
poll();
</script>
</body></html>"""

class Handler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path == "/" or self.path.startswith("/?"):
            body = HTML.encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(body)
        elif self.path.startswith("/api/log"):
            since = 0
            m = re.search(r"since=(\d+)", self.path)
            if m:
                since = int(m.group(1))
            size = 0
            data = b""
            try:
                size = os.path.getsize(LOG)
                with open(LOG, "rb") as f:
                    f.seek(since if since < size else size)
                    data = f.read()
            except OSError:
                pass
            body = json.dumps({"size": size, "lines": data.decode("utf-8", "replace")}).encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(body)
        else:
            self.send_response(404)
            self.end_headers()

    def log_message(self, *args):
        pass

if __name__ == "__main__":
    print("monitor server: http://localhost:%d (log: %s)" % (PORT, LOG))
    http.server.HTTPServer(("127.0.0.1", PORT), Handler).serve_forever()
'@

$serverPath = Join-Path $env:TEMP "dnrp_monitor_server.py"
Set-Content -Path $serverPath -Value $serverPy -Encoding UTF8

# 1. 清空旧日志
Remove-Item $LOG -ErrorAction SilentlyContinue

# 2. 启动按键监听器 (本窗口后台任务)
$j1 = Start-Job -ScriptBlock {
    param($script, $log)
    & $script -DurationSec 3600 -LogFile $log
} -ArgumentList (Join-Path $PSScriptRoot "rawkey_listener.ps1"), $LOG
Write-Host "按键监听器已启动 (job $($j1.Id))"

# 3. 启动监控台服务 (本窗口后台任务)
$j2 = Start-Job -ScriptBlock {
    param($py, $server, $log, $port)
    & $py $server $log $port
} -ArgumentList $py, $serverPath, $LOG, $Port
Write-Host "监控台服务已启动 (job $($j2.Id))"

Start-Sleep -Seconds 2

# 4. 打开浏览器
Start-Process "http://localhost:$Port/"
Write-Host "`n=== 注入监控台: http://localhost:$Port/ ==="
Write-Host "左侧: 板子侧 UART 终端   右侧: 目标机实时按键流"
Write-Host "按任意键停止监控 (或直接关闭本窗口)"

# 5. 保持窗口打开 (关闭窗口/按任意键即停止全部)
Read-Host | Out-Null
Stop-Job $j1, $j2 -ErrorAction SilentlyContinue
Remove-Job $j1, $j2 -Force -ErrorAction SilentlyContinue
Write-Host "监控已停止。"
