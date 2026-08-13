# ============================================================================
#  rawkey_listener.ps1 — 目标机侧键盘事件监听器 (Raw Input, 编译为 DLL 运行)
#
#  用途: 验证 BadUSB 注入时, 目标机实际收到的按键事件及其来源设备。
#  原理: 注册 RAW INPUT (UsagePage=1, Usage=6, RIDEV_INPUTSINK),
#        系统级捕获所有键盘事件, 通过 RIDI_DEVICENAME 关联到具体设备
#        (板子的 HID 键盘会显示为 VID_CAFE&PID_4001)。
#
#  用法:  .\tools\rawkey_listener.ps1 [-DurationSec 120] [-LogFile rawkey_demo.log]
#         （首次运行会自动编译 tools\RawKeyListener.dll）
# ============================================================================
param(
    [int]$DurationSec = 120,
    [string]$LogFile = "rawkey_demo.log"
)

$src = @"
using System;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Text.RegularExpressions;
using System.Threading;

public static class RawKeyListener
{
    [StructLayout(LayoutKind.Sequential)]
    public struct RAWINPUTDEVICE
    {
        public ushort usUsagePage;
        public ushort usUsage;
        public uint dwFlags;
        public IntPtr hwndTarget;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct RAWINPUTHEADER
    {
        public uint dwType;
        public uint dwSize;
        public IntPtr hDevice;
        public IntPtr wParam;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct RAWKEYBOARD
    {
        public ushort MakeCode;
        public ushort Flags;
        public ushort Reserved;
        public ushort VKey;
        public uint Message;
        public uint ExtraInformation;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct RAWINPUT
    {
        public RAWINPUTHEADER header;
        public RAWKEYBOARD keyboard;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct WNDCLASS
    {
        public uint style;
        public IntPtr lpfnWndProc;
        public int cbClsExtra;
        public int cbWndExtra;
        public IntPtr hInstance;
        public IntPtr hIcon;
        public IntPtr hCursor;
        public IntPtr hbrBackground;
        public string lpszMenuName;
        public string lpszClassName;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct MSG
    {
        public IntPtr hwnd;
        public uint message;
        public IntPtr wParam;
        public IntPtr lParam;
        public uint time;
        public int ptX;
        public int ptY;
    }

    public delegate IntPtr WndProcDelegate(IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam);

    private const uint WM_INPUT        = 0x00FF;
    private const uint WM_QUIT         = 0x0012;
    private const uint RID_INPUT       = 0x10000003;
    private const uint RIDI_DEVICENAME = 0x20000007;
    private const uint RIDEV_INPUTSINK = 0x00000100;
    private const uint RIM_TYPEKEYBOARD = 1;
    private const int  HWND_MESSAGE    = -3;

    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool RegisterRawInputDevices(RAWINPUTDEVICE[] pRawInputDevices, uint uiNumDevices, uint cbSize);

    [DllImport("user32.dll", SetLastError = true)]
    private static extern uint GetRawInputData(IntPtr hRawInput, uint uiCommand, IntPtr pData, ref uint pcbSize, uint cbSizeHeader);

    [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern uint GetRawInputDeviceInfo(IntPtr hDevice, uint uiCommand, IntPtr pData, ref uint pcbSize);

    [DllImport("user32.dll", SetLastError = true)]
    private static extern ushort RegisterClass(ref WNDCLASS lpWndClass);

    [DllImport("user32.dll", SetLastError = true)]
    private static extern IntPtr CreateWindowEx(uint dwExStyle, string lpClassName, string lpWindowName,
        uint dwStyle, int x, int y, int nWidth, int nHeight, IntPtr hWndParent, IntPtr hMenu, IntPtr hInstance, IntPtr lpParam);

    [DllImport("user32.dll")]
    private static extern bool PeekMessage(out MSG lpMsg, IntPtr hWnd, uint wMsgFilterMin, uint wMsgFilterMax, uint wRemoveMsg);

    [DllImport("user32.dll")]
    private static extern bool TranslateMessage(ref MSG lpMsg);

    [DllImport("user32.dll")]
    private static extern IntPtr DispatchMessage(ref MSG lpMsg);

    [DllImport("user32.dll")]
    private static extern IntPtr DefWindowProc(IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam);

    [DllImport("kernel32.dll")]
    private static extern IntPtr GetModuleHandle(string lpModuleName);

    private static WndProcDelegate _wndProc;
    private static string _logPath;
    private static uint _count;

    private static IntPtr WndProc(IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam)
    {
        if (msg == WM_INPUT)
        {
            try { HandleRawInput(lParam); }
            catch (Exception ex) { try { File.AppendAllText(_logPath, "!! handler error: " + ex + "\r\n"); } catch { } }
        }
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }

    private static string KeyName(ushort vk)
    {
        if (vk >= 0x41 && vk <= 0x5A) return ((char)vk).ToString();          // A-Z
        if (vk >= 0x30 && vk <= 0x39) return ((char)vk).ToString();          // 0-9
        switch (vk)
        {
            case 0x20: return "SPACE"; case 0x0D: return "ENTER"; case 0x09: return "TAB";
            case 0x1B: return "ESC";   case 0x08: return "BACKSPACE"; case 0x2E: return "DEL";
            case 0x24: return "HOME";  case 0x23: return "END"; case 0x21: return "PGUP";
            case 0x22: return "PGDN";  case 0x25: return "LEFT"; case 0x26: return "UP";
            case 0x27: return "RIGHT"; case 0x28: return "DOWN"; case 0x14: return "CAPS";
            case 0x10: return "SHIFT"; case 0x11: return "CTRL"; case 0x12: return "ALT";
            case 0x5B: return "LWIN";  case 0x5C: return "RWIN"; case 0x5D: return "APPS";
            case 0x90: return "NUMLOCK"; case 0x2C: return "PRTSC";
            default: break;
        }
        if (vk >= 0x70 && vk <= 0x7B) return "F" + (vk - 0x70 + 1);
        if (vk >= 0x21 && vk <= 0x2F) return "0x" + vk.ToString("X2");
        return "0x" + vk.ToString("X2");
    }

    private static string ShortDev(string dev)
    {
        var m = Regex.Match(dev, @"VID_([0-9A-Fa-f]{4})&PID_([0-9A-Fa-f]{4})");
        if (m.Success) return "VID_" + m.Groups[1].Value.ToUpperInvariant() + "&PID_" + m.Groups[2].Value.ToUpperInvariant();
        if (dev.Length > 48) return "..." + dev.Substring(dev.Length - 48);
        return dev;
    }

    private static void HandleRawInput(IntPtr hRawInput)
    {
        uint size = 0;
        GetRawInputData(hRawInput, RID_INPUT, IntPtr.Zero, ref size, (uint)Marshal.SizeOf(typeof(RAWINPUTHEADER)));
        if (size == 0) return;
        IntPtr buf = Marshal.AllocHGlobal((int)size);
        try
        {
            uint got = GetRawInputData(hRawInput, RID_INPUT, buf, ref size, (uint)Marshal.SizeOf(typeof(RAWINPUTHEADER)));
            if (got == 0 || got > size) return;
            RAWINPUT ri = (RAWINPUT)Marshal.PtrToStructure(buf, typeof(RAWINPUT));
            if (ri.header.dwType != RIM_TYPEKEYBOARD) return;

            RAWKEYBOARD kb = ri.keyboard;
            bool isUp = (kb.Flags & 0x01) != 0;
            bool isE0 = (kb.Flags & 0x02) != 0;

            uint n = 0;
            GetRawInputDeviceInfo(ri.header.hDevice, RIDI_DEVICENAME, IntPtr.Zero, ref n);
            string dev = "?";
            if (n > 0)
            {
                // 注意: RIDI_DEVICENAME 的 pcbSize 在不同 Windows 版本上返回的是
                // "字符数"或"字节数", 一律按字节数分配会缓冲区溢出 → 堆损坏。
                // 这里按最宽松的 (n+1)*2 字节分配, 读取长度钳制在缓冲区范围内。
                int bufBytes = ((int)n + 1) * 2;
                IntPtr pn = Marshal.AllocHGlobal(bufBytes);
                try
                {
                    uint n2 = (uint)bufBytes;
                    GetRawInputDeviceInfo(ri.header.hDevice, RIDI_DEVICENAME, pn, ref n2);
                    Marshal.WriteInt16(pn, bufBytes - 2, 0);   // 尾部强制 NUL, 防越界读
                    dev = Marshal.PtrToStringUni(pn) ?? "?";
                }
                finally { Marshal.FreeHGlobal(pn); }
            }

            _count++;
            string line = string.Format("{0:HH:mm:ss.fff}  [{1,-18}]  {2,-6} {3}{4}  Make=0x{5:X2} Msg=0x{6:X4}",
                DateTime.Now, ShortDev(dev), isUp ? "UP" : "DOWN", KeyName(kb.VKey), isE0 ? "(E0)" : "",
                kb.MakeCode, kb.Message);
            File.AppendAllText(_logPath, line + "\r\n");
        }
        finally { Marshal.FreeHGlobal(buf); }
    }

    public static int Main(string[] args)
    {
        int durationSec = 120;
        string logPath = "rawkey_demo.log";
        if (args.Length > 0) int.TryParse(args[0], out durationSec);
        if (args.Length > 1) logPath = args[1];

        _logPath = logPath;
        _wndProc = new WndProcDelegate(WndProc);

        WNDCLASS wc = new WNDCLASS();
        wc.lpfnWndProc = Marshal.GetFunctionPointerForDelegate(_wndProc);
        wc.hInstance = GetModuleHandle(null);
        wc.lpszClassName = "RawKeySinkWnd";
        if (RegisterClass(ref wc) == 0) { Console.WriteLine("RegisterClass failed: " + Marshal.GetLastWin32Error()); return -1; }

        IntPtr hwnd = CreateWindowEx(0, "RawKeySinkWnd", "rawkeysink", 0, 0, 0, 0, 0,
                                     new IntPtr(HWND_MESSAGE), IntPtr.Zero, wc.hInstance, IntPtr.Zero);
        if (hwnd == IntPtr.Zero) { Console.WriteLine("CreateWindowEx failed: " + Marshal.GetLastWin32Error()); return -2; }

        RAWINPUTDEVICE[] rid = new RAWINPUTDEVICE[1];
        rid[0].usUsagePage = 0x01;
        rid[0].usUsage     = 0x06;
        rid[0].dwFlags     = RIDEV_INPUTSINK;
        rid[0].hwndTarget  = hwnd;
        if (!RegisterRawInputDevices(rid, 1, (uint)Marshal.SizeOf(typeof(RAWINPUTDEVICE))))
        { Console.WriteLine("RegisterRawInputDevices failed: " + Marshal.GetLastWin32Error()); return -3; }

        File.AppendAllText(logPath, "== rawkey_listener started, duration " + durationSec + "s ==\r\n");
        Console.WriteLine("listener started, capturing for " + durationSec + "s ...");

        Stopwatch sw = Stopwatch.StartNew();
        while (sw.Elapsed.TotalSeconds < durationSec)
        {
            MSG msg;
            while (PeekMessage(out msg, IntPtr.Zero, 0, 0, 1))
            {
                TranslateMessage(ref msg);
                DispatchMessage(ref msg);
                if (msg.message == WM_QUIT) { File.AppendAllText(logPath, "== stopped by WM_QUIT ==\r\n"); return 0; }
            }
            Thread.Sleep(25);
        }
        File.AppendAllText(logPath, "== listener done, total " + _count + " events ==\r\n");
        Console.WriteLine("listener done, total " + _count + " events");
        return 0;
    }
}
"@

$dll = Join-Path $PSScriptRoot "RawKeyListener.dll"
$needsCompile = (-not (Test-Path $dll))
if (-not $needsCompile) {
    $md5 = [System.Security.Cryptography.MD5]::Create()
    $srcHash = [Convert]::ToHexString($md5.ComputeHash([System.Text.Encoding]::UTF8.GetBytes($src)))
    $dllHash = (Get-FileHash $dll -Algorithm MD5).Hash
    $needsCompile = ($srcHash -ne $dllHash)
}
if ($needsCompile) {
    Write-Host "编译 RawKeyListener.dll ..."
    Add-Type -TypeDefinition $src -OutputAssembly $dll -OutputType Library
}
Add-Type -Path $dll
[RawKeyListener]::Main(@($DurationSec, $LogFile)) | Out-Null
exit $LASTEXITCODE
