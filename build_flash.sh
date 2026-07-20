#!/bin/bash
# ===========================================================================
#  DNRP2350A 一键构建+烧录
#  用法: ./build_flash.sh 06_terminal
# ===========================================================================
set -e
ROOT="d:/Downloads/RP"
PROJ="$1"
PYTHON="D:/Miniconda3/envs/default/python.exe"

if [ -z "$PROJ" ]; then
    echo "Usage: ./build_flash.sh <project>"
    ls -d "$ROOT"/arm-dev/*/ | sed "s|$ROOT/arm-dev/||;s|/||"
    exit 1
fi

UF2="$ROOT/arm-dev/$PROJ/build/$PROJ.uf2"
BUILD="$ROOT/arm-dev/$PROJ/build"

if [ ! -d "$ROOT/arm-dev/$PROJ" ]; then
    echo "ERROR: $PROJ not found"
    exit 1
fi

echo "============================================"
echo " Build+Flash: $PROJ"
echo "============================================"

# 1. build
echo "[1/3] Build..."
[ ! -d "$BUILD" ] && mkdir -p "$BUILD"
cd "$BUILD"
cmake .. -G Ninja -DPICOTOOL_FETCH_FROM_GIT_PATH="d:/Downloads/RP/arm-dev/02_lcd_uart/build/_deps" > /dev/null 2>&1
ninja > /dev/null 2>&1
if [ ! -f "$UF2" ]; then echo "BUILD FAIL"; exit 1; fi
echo "  OK ($(du -h "$UF2" | cut -f1))"

# 2. serial reboot via MCP serial server (http://localhost:9721)
echo "[2/3] Serial reboot..."
curl -s -X POST http://localhost:9721/send \
    -H "Content-Type: application/json" \
    -d '{"command":"reboot\r","lineEnding":""}' > /dev/null 2>&1 && echo "  reboot sent via MCP" || echo "  MCP send failed"
sleep 2

# 3. flash
echo "[3/3] Flash..."
for i in $(seq 1 60); do
    for drive in D E F G H I J K L M N O P Q R S T U V W X Y Z; do
        vol=$(powershell.exe -Command "\$d=Get-WmiObject Win32_LogicalDisk -Filter \"DeviceID='$drive:'\"|Select-Object -ExpandProperty VolumeName 2>\$null; if(\$d){Write-Output \$d}" 2>/dev/null | tr -d '\r\n')
        if echo "$vol" | grep -qiE "RP2350|RPI-RP2"; then
            if cp "$UF2" "/${drive}/" 2>/dev/null; then
                echo "  FLASH OK ($drive:)"
                exit 0
            else
                sleep 1
                if cp "$UF2" "/${drive}/" 2>/dev/null; then
                    echo "  FLASH OK retry ($drive:)"
                    exit 0
                fi
            fi
        fi
    done
    printf "."; sleep 1
done
echo "  TIMEOUT - press BOOT+RST"
exit 1
