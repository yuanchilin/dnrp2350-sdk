#!/bin/bash
# ============================================================================
#  DNRP2350A DAP 烧录脚本 (CMSIS-DAP / pyOCD)
#  用法: ./flash_dap.sh [elf文件路径]
#  无需按键，无需串口命令，插上 DAP 即烧
# ============================================================================
PYTHON="D:/Miniconda3/envs/stm32n6_ai/python.exe"
ELF_FILE="${1:-$(dirname "$0")/01_led/build/01_led.elf}"

if [ ! -f "$ELF_FILE" ]; then
    echo "❌ ELF 文件不存在: $ELF_FILE"
    exit 1
fi

echo "============================================"
echo " DNRP2350A DAP 烧录"
echo " 固件: $(basename "$ELF_FILE")"
echo " 目标: rp2350 (Cortex-M33 + RISC-V)"
echo "============================================"

$PYTHON -m pyocd flash \
    -t rp2350 \
    --frequency 5000000 \
    "$ELF_FILE"

if [ $? -eq 0 ]; then
    echo ""
    echo "✅ 烧录成功！板子已自动复位运行。"
else
    echo ""
    echo "❌ 烧录失败！请检查："
    echo "   1. DAP 调试器是否已连接"
    echo "   2. SWD 接线: GND + SWCLK + SWDIO"
    echo "   3. 板子是否已上电"
    exit 1
fi
