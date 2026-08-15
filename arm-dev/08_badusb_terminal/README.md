# 08 — BadUSB 注入终端（汇总工程）

07（PIO+DMA LCD 终端：Shell / Console / TUI / FatFs）与 05（BadUSB HID 键盘注入）的合并工程。终端为主界面，BadUSB 注入成为 shell 命令。

## 与 05 / 07 的关系

- 注入引擎（`hid_keyboard` / `payload` / `sample_script` / `tusb_config`）已沉淀到 `common/badusb/`，05 与 08 共用。
- `common/badusb/badusb.c` 是命令层，把注入封装成 shell 命令 + KEY1 一键注入。

## 命令

```
badusb              列出 SD 卡根目录的 .txt 脚本
badusb demo         写入内置示例 /hello.txt 并注入
badusb 1            注入第 1 个脚本（数字索引）
badusb /x.txt       注入指定脚本
```

物理按键 **KEY1** = 注入当前选中脚本（默认第一个）。其余命令同 07（`ls` `cat` `view` `tui` `help` …）。

## 构建 / 烧录

```powershell
.\build.ps1 08_badusb_terminal
.\flash.ps1 .\arm-dev\08_badusb_terminal\build\08_badusb_terminal.uf2
```

## 接线

- **USB_OTG（USB0）** → 目标电脑（注入对象）
- **CH343 调试串口（COM5）** → 开发机（shell 终端）
- SD 卡根目录放 `.txt` 脚本

> 中文输入法注意：目标机为中文拼音输入法时，注入文本会被组词乱码。脚本开头用 `ALT SHIFT`（切英文键盘布局）规避——内置示例已处理，详见 05 的 README。
