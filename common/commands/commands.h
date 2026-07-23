/**
 * @file    commands.h
 * @brief   标准命令库 — ls/cat/view/free/sysinfo/clear/snake
 */

#ifndef __COMMANDS_H
#define __COMMANDS_H

#include <stdbool.h>

void commands_init(bool sd_ok);
void commands_register_all(void);
void commands_view_file(const char *path);  /* 启动时直接显示图片 */

#endif
