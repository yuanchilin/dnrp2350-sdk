#!/bin/bash
# ===========================================================================
#  DNRP2350A 一键烧录 — 串口发 reboot 自动进 bootloader
#  用法: ./flash.sh [uf2文件路径]
# ===========================================================================
UF2_FILE="${1:-$(dirname "$0")/01_led/build/01_led.uf2}"
PYTHON="D:/Miniconda3/envs/default/python.exe"

if [ ! -f "$UF2_FILE" ]; then
    echo "❌ UF2 文件不存在: $UF2_FILE"
    exit 1
fi

echo "============================================"
echo " DNRP2350A 一键烧录"
echo " 固件: $(basename "$UF2_FILE") ($(du -h "$UF2_FILE" | cut -f1))"
echo "============================================"

# ---- 串口发 reboot 命令 ----
echo ""
echo "🔌 通过串口发送 reboot..."

COM_PORT=$($PYTHON -c "
import serial.tools.list_ports
# 优先选 CH343 (固件 shell 所在口)，避免误选其他 USB 串口
for p in serial.tools.list_ports.comports():
    if 'CH34' in p.description:
        print(p.device)
        break
else:
    for p in serial.tools.list_ports.comports():
        if 'USB' in p.description or 'Serial' in p.description:
            print(p.device)
            break
" 2>/dev/null)

if [ -n "$COM_PORT" ]; then
    echo "   找到串口: $COM_PORT"
    $PYTHON -c "
import serial, time
try:
    s = serial.Serial('$COM_PORT', 115200, timeout=1)
    s.write(b'reboot\r\n')
    time.sleep(0.5)
    s.close()
    print('   ✅ reboot 已发送')
except Exception as e:
    print(f'   ⚠️ 失败: {e}')
" 2>/dev/null
else
    echo "   ⚠️ 未找到串口，回退手动按键"
    echo "   👉 按住 BOOT → 点 RESET → 松开"
fi

# ---- 等待 RP2350 盘符 ----
echo ""
echo "⏳ 等待 RP2350 盘符..."

for i in $(seq 1 60); do
    for drive in D E F G H I J K L M N O P Q R S T U V W X Y Z; do
        vol=$(powershell.exe -Command "
            \$d = Get-WmiObject Win32_LogicalDisk -Filter \"DeviceID='$drive:'\" | Select-Object -ExpandProperty VolumeName 2>\$null;
            if (\$d) { Write-Output \$d }
        " 2>/dev/null | tr -d '\r\n')

        if echo "$vol" | grep -qiE "RP2350|RPI-RP2|RPI-RP3"; then
            echo ""
            echo "✅ 检测到 $vol (${drive}:)"
            echo "   正在烧录..."
            if cp "$UF2_FILE" "/${drive}/" 2>/dev/null; then
                echo "   ✅ 烧录成功！"
                exit 0
            else
                echo "   ❌ 拷贝失败"
                exit 1
            fi
        fi
    done
    printf "."
    sleep 1
done

echo ""
echo "❌ 超时 (60s) 未检测到 RP2350 设备"
echo "   手动: 按住 BOOT → 点 RESET → 松开"
exit 1
