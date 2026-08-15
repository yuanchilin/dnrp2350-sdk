/**
 * @file    badusb.h
 * @brief   BadUSB shell 命令层 — 终端下 list/inject 脚本 + KEY1 一键注入
 */

#ifndef __BADUSB_H
#define __BADUSB_H

#include <stdbool.h>

void badusb_init(bool sd_ok);     /* 初始化: 自动写示例脚本 + 扫描 .txt + KEY */
void badusb_register(void);       /* 注册 badusb 命令到 shell */
void badusb_task(void);           /* 主循环调用: hid_task + KEY1 轮询注入 */

#endif
