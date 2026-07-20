---
name: flash
description: 一键编译+烧录 DNRP2350A 项目。用法 /flash [项目名]
argument-hint: 06_terminal
---

# Flash Skill

编译指定项目并通过 MCP 串口自动烧录到 DNRP2350A 开发板。

## 执行

```
bash build_flash.sh $ARGUMENTS
```

## 不带参数时

列出所有可用项目：
```
ls -d arm-dev/*/
```

## 原理

1. `ninja` 编译项目
2. 通过 MCP serial 工具向 COM5 发送 `reboot\r` → 板子进 bootloader
3. 检测 RP2350 盘符 → 拷贝 .uf2 → 烧录完成
