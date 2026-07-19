#!/bin/bash
# ===========================================================================
#  DNRP2350A 烧录脚本
#  用法: ./flash.sh [uf2文件路径]
# ===========================================================================
UF2_FILE="${1:-$(dirname "$0")/01_led/build/01_led.uf2}"

if [ ! -f "$UF2_FILE" ]; then
    echo "❌ UF2 文件不存在: $UF2_FILE"
    exit 1
fi

echo "============================================"
echo " DNRP2350A 固件烧录"
echo " 固件: $(basename "$UF2_FILE") ($(du -h "$UF2_FILE" | cut -f1))"
echo "============================================"
echo ""
echo "⏳ 等待 RP2350 进入 bootloader..."
echo "   👉 按住 BOOT → 点 RESET → 松开"
echo ""

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
echo "❌ 超时 (60s) 未检测到 RP2350 设备。"
echo "   请确认:"
echo "   1. USB 线已连接"
echo "   2. 按住 BOOT → 点按 RESET → 松开 BOOT"
echo "   3. 电脑弹出名为 RP2350 或 RPI-RP2 的 U 盘"
exit 1
