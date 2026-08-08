/**
 * @file    ansi.h
 * @brief   串口 ANSI 颜色宏 — 终端颜色统一管理
 *
 * 用法示例:
 *   shell_print(CLR_CMD);  shell_print("ls");  shell_print(ANSI_RESET);
 * 或直接嵌入格式串:
 *   snprintf(b, sizeof(b), CLR_CMD "%-12s" ANSI_RESET " %s", name, help);
 */

#ifndef __ANSI_H
#define __ANSI_H

/* ---- 基础控制 ---- */
#define ANSI_RESET         "\x1b[0m"     /* 复位到默认颜色 */
#define ANSI_BOLD          "\x1b[1m"     /* 加粗 */

/* ---- 标准色 (前景) ---- */
#define ANSI_BLACK         "\x1b[30m"
#define ANSI_RED           "\x1b[31m"
#define ANSI_GREEN         "\x1b[32m"
#define ANSI_YELLOW        "\x1b[33m"
#define ANSI_BLUE          "\x1b[34m"
#define ANSI_MAGENTA       "\x1b[35m"
#define ANSI_CYAN          "\x1b[36m"
#define ANSI_WHITE         "\x1b[37m"

/* ---- 亮色 (加粗前景) ---- */
#define ANSI_BRIGHT_BLACK  "\x1b[1;30m"
#define ANSI_BRIGHT_RED    "\x1b[1;31m"
#define ANSI_BRIGHT_GREEN  "\x1b[1;32m"
#define ANSI_BRIGHT_YELLOW "\x1b[1;33m"
#define ANSI_BRIGHT_BLUE   "\x1b[1;34m"
#define ANSI_BRIGHT_MAGENTA "\x1b[1;35m"
#define ANSI_BRIGHT_CYAN   "\x1b[1;36m"
#define ANSI_BRIGHT_WHITE  "\x1b[1;37m"

/* ---- 语义色彩: 业务统一从这里取用, 便于统一调整 ---- */
#define CLR_CMD      ANSI_BRIGHT_GREEN  /* 命令名 / 可执行体   → 亮绿 */
#define CLR_INPUT    ANSI_GREEN         /* 用户输入的命令       → 绿   */
#define CLR_DIR      ANSI_BRIGHT_BLUE   /* 目录                 → 蓝   */
#define CLR_IMG      ANSI_BRIGHT_MAGENTA/* 图片 (.bmp/.jpg/..)  → 品红 */
#define CLR_TXT      ANSI_BRIGHT_YELLOW /* 文本 (.txt/.c/..)    → 黄   */
#define CLR_EXE      ANSI_BRIGHT_GREEN  /* 可执行 (.elf/.bin/.) → 亮绿 */
#define CLR_FILE     ANSI_RESET         /* 其他文件(默认色)     → 普通 */

#endif /* __ANSI_H */