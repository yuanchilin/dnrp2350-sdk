---
name: flash
description: 一键编译+烧录 DNRP2350A 项目。用法 /flash [项目名]
argument-hint: 06_terminal
---

# Flash Skill

编译指定项目并通过 MCP 串口自动烧录到 DNRP2350A 开发板。

## 执行

先构建，再烧录：
```
powershell -ExecutionPolicy Bypass -File .\build.ps1 $ARGUMENTS
powershell -ExecutionPolicy Bypass -File .\flash.ps1
```

## 不带参数时

`build.ps1` 无参运行会列出所有可用项目。

## 原理

1. `build.ps1` 用 cmake + ninja 编译项目
2. `flash.ps1` 通过 MCP serial 向 COM5 发送 `reboot\r` → 板子进 bootloader
3. 检测 RP2350 盘符 → 拷贝 .uf2 → 烧录完成
