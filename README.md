# DNRP2350A SDK

基于 **RP2350**（树莓派 Pico 2 内核）的嵌入式学习与演示工程，目标板为 DNRP2350A。

仓库包含 7 个从易到难的 ARM (Cortex-M33) 实验、一份可复用的公共库（BSP / Shell / 终端 / TUI / FatFs）、一套一键构建、烧录与真机回归测试脚本，以及 RISC-V 平台的工程骨架。

## 目录结构

```text
RP/
├── arm-dev/            # ARM (Cortex-M33) 实验工程: 01_led ... 07_pio_lcd
├── common/             # 公共库
│   ├── BSP/            #   板级驱动: LCD / LED / KEY / SPI / UART / SDIO
│   ├── console/        #   LCD 终端输出 + ANSI 彩色
│   ├── shell/          #   串口 Shell (历史 / Tab 补全 / help / reboot / reset)
│   ├── commands/       #   标准命令: ls cat view free sysinfo clear uptime echo setcolor snake good
│   ├── tui/            #   30x8 全屏 TUI (tui_demo)
│   ├── pio_lcd/        #   PIO+DMA 加速的 LCD 驱动
│   └── Middlewares/    #   FatFs (ff15) + SD 卡驱动
├── riscv-dev/          # RISC-V (Hazard3) 工程骨架, 当前仅 01_led
├── tools/picotool/     # 预编译 picotool (避免每个工程联网下载/重新编译)
├── pico-sdk/           # Raspberry Pi Pico SDK (git submodule)
├── sdk_project.cmake   # 公共 CMake 工程模板 drp_project()
├── build.ps1           # 一键构建
├── flash.ps1           # 一键烧录
└── tui_smoke.ps1       # TUI 真机冒烟回归测试
```

## 环境要求

- Windows + PowerShell
- [git](https://git-scm.com/)（含 submodule）
- [CMake](https://cmake.org/) ≥ 3.13
- [Ninja](https://ninja-build.org/)
- ARM 工具链 `arm-none-eabi-gcc`（构建 ARM 工程必需）
- RISC-V 工具链 `riscv32-unknown-elf-gcc`（仅 `riscv-dev` 需要，当前环境未安装）
- （可选）MCP serial 服务 `http://localhost:9721`：`flash.ps1` / `tui_smoke.ps1` 通过它向板子发命令

## 快速开始

克隆并初始化 SDK 子模块：

```powershell
git clone --recursive <repo-url>
# 若已克隆: git submodule update --init --recursive
```

列出所有可用工程：

```powershell
.\build.ps1
```

构建指定工程（产物为 `arm-dev/<工程>/build/<工程>.uf2`）：

```powershell
.\build.ps1 07_pio_lcd
.\build.ps1 07_pio_lcd -Clean   # 清理旧缓存后重新配置构建
```

烧录（自动经 MCP 发 `reboot` 进 bootloader，再拷贝 UF2）：

```powershell
.\flash.ps1                          # 默认取最近构建的 UF2
.\flash.ps1 .\arm-dev\07_pio_lcd\build\07_pio_lcd.uf2
```

运行 TUI 回归测试（可选参数 `-Port` / `-Uf2`）：

```powershell
.\tui_smoke.ps1
```

## 实验工程一览

| 工程 | 主题 | 涉及模块 |
| --- | --- | --- |
| `01_led` | LED 闪烁 | LED |
| `02_lcd_uart` | LCD + UART + Shell | LCD、UART、SPI、Shell |
| `03_photo` | 迷你相框：LCD + TF 卡 BMP 解码 | LCD、SDIO、FatFs、BMP（`next`/`prev`/`info`/`auto` 命令） |
| `04_duel` | 双核斗法：Mandelbrot 抢行对战 | LCD、Shell、`pico_multicore`（`core1.c`） |
| `05_badusb` | USB HID 键盘（TinyUSB） | LCD、KEY、SDIO、FatFs、`tinyusb_device`（脚本语法见 [05 项目 README](arm-dev/05_badusb/README.md)） |
| `06_terminal` | 迷你终端（全公共库） | Shell、Console、Commands、SDIO、FatFs |
| `07_pio_lcd` | PIO+DMA LCD 加速终端 + 全屏 TUI | 在 06 基础上加入 `pio_lcd`、`tui`（`tui` 命令） |

> 注：`03`、`06`、`07` 开机/`view` 会读取 SD 卡根目录的 `photo.bmp`，请先把它放到 TF 卡里。

## 公共工程模板

每个 ARM 工程的 `CMakeLists.txt` 都只有 6 行左右，样板逻辑集中在 `sdk_project.cmake` 的 `drp_project()`：

```cmake
cmake_minimum_required(VERSION 3.13)
include(../../sdk_project.cmake)
include(../pico_sdk_import.cmake)
project(07_pio_lcd C CXX ASM)
drp_project(07_pio_lcd
    BSP LCD LED SPI UART SDIO SHELL CONSOLE COMMANDS TUI PIO_LCD
    NEED_FATFS
    PIO lcd_spi.pio
    LIBS pico_stdlib hardware_spi hardware_uart hardware_pio hardware_dma hardware_irq
)
```

常用参数：

- `BSP <模块...>`：加入公共库源文件，可选 `LCD LED KEY SPI UART SDIO SHELL CONSOLE COMMANDS TUI PIO_LCD BMP`
- `SRCS <文件...>`：追加本工程源文件（如 `core1.c`、`hid_keyboard.c`）
- `LIBS <库...>`：额外链接库（如 `pico_multicore`、`tinyusb_device`）
- `NEED_FATFS`：引入 FatFs 中间件
- `PIO <xxx.pio>`：生成 PIO 头文件
- `STDIO_UART` / `STDIO_USB`：启用对应 stdio（默认关闭，Shell 走自定义 UART 收发）
- `DEFS <宏...>`：追加编译宏

## 脚本原理

`flash.ps1` 的烧录链路：

1. 通过 MCP serial（`http://localhost:9721/send`）向串口发回车清缓冲，发 `Ctrl+C` 退出可能的 TUI；
2. 发 `reboot` 让板子进入 bootloader；
3. 轮询检测卷标为 `RP2350` / `RPI-RP2` 的移动盘，把 `.uf2` 拷进去即完成。

若 MCP 服务不可用，脚本会提示手动 `按住 BOOT → 点 RESET → 松开`。

`tui_smoke.ps1` 覆盖：进入 TUI 全屏绘制、方向键最小重绘、Enter/Esc/Ctrl+C 三条退出路径、退出后回到 Shell 提示符，且测试结束绝不把板子留在 TUI。

## 常见问题

**pico-sdk 缺失 / 构建报错**：先执行 `git submodule update --init --recursive`。

**构建时自动重建缓存**：`build.ps1` 会校验 CMake 缓存里的源路径，与当前工程不一致时自动清理重建，无需手动删除 `build/`。

**烧录超时未检测到盘符**：确认 USB 数据线连接，手动按住 BOOT 再点 RESET。

**RISC-V 工程无法构建**：需要安装 `riscv32-unknown-elf-gcc` 工具链；当前仓库仅保留骨架。

## 参考

- [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk)
- `.claude/skills/flash.md`：Claude Code 的 `/flash` 命令（一键构建 + 烧录）
