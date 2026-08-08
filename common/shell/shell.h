/**
 * @file    shell.h
 * @brief   串口 Shell — 命令注册/解析/回显/提示符
 */

#ifndef __SHELL_H
#define __SHELL_H

#include <stdint.h>
#include <stdbool.h>
#include "ansi.h"   /* 串口 ANSI 颜色宏 (统一管理) */

typedef void (*shell_cmd_fn)(const char *arg);
typedef int  (*shell_arg_cb)(const char *tok, char *out, int outsz);  /* 参数补全: 返回填充的候选数, out 为唯一候选或首个候选 */

void shell_init(const char *prompt);
void shell_poll(void);
void shell_set_ansi(bool on);  /* 串口 ANSI 颜色开关 (命令绿色/输出默认色) */
void shell_register(const char *name, const char *help, shell_cmd_fn fn);
void shell_set_arg_completer(shell_arg_cb cb);  /* 参数(文件名)补全回调 */
void shell_print(const char *s);
void shell_printf(const char *fmt, ...);
void shell_set_echo_cb(void (*cb)(char));  /* 键入回调 → LCD 显示 */
void shell_set_output_cb(void (*cb)(const char *));  /* 命令输出回调 → LCD 终端 */

/* 公共 reboot/reset — 供各项目复用 (03/04/05 等未迁移 shell 的项目) */
void shell_reboot(void);   /* 进 USB bootloader */
void shell_reset(void);    /* watchdog 软复位回固件 */

#endif
