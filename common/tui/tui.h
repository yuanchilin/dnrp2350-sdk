/**
 * @file    tui.h
 * @brief   全屏 TUI 框架 — ANSI 原语 + 菜单小部件 (串口 xterm 全屏界面)
 *
 * 设计: 与 shell 并列。TUI 以 shell 命令形式进入 (如 `tui`),
 * 运行期间独占 UART 输入/输出, shell_poll 暂停; Esc / Ctrl+C 退出,
 * 恢复 shell 提示符。目标终端 30x8 (8x16 字体, LCD 完整显示, 无切省)。
 */

#ifndef __TUI_H
#define __TUI_H

#include <stdint.h>
#include <stdbool.h>

#define TUI_ROWS    8
#define TUI_COLS    30

/* 界面返回码: 选中索引为 0..n-1; 以下为特殊退出 */
#define TUI_QUIT_ESC   (-1)   /* Esc: 返回上一层 */
#define TUI_QUIT_CTRC  (-2)   /* Ctrl+C: 强制退出整个 TUI */

/* ---- 按键事件 ---- */
typedef enum {
    TUI_KEY_CHAR = 0,   /* 可打印字符, ev.ch 有效 */
    TUI_KEY_UP,
    TUI_KEY_DOWN,
    TUI_KEY_LEFT,
    TUI_KEY_RIGHT,
    TUI_KEY_ENTER,
    TUI_KEY_ESC,
    TUI_KEY_CTRLC,      /* Ctrl+C: 强制退出 */
    TUI_KEY_BACK,
} tui_key_t;

typedef struct {
    tui_key_t type;
    char      ch;
} tui_event_t;

/* ---- 生命周期: 进入/退出全屏 (备用屏 + 隐藏光标) ---- */
void tui_start(void);
void tui_stop(void);

/* ---- 输出原语 ---- */
void tui_move(int row, int col);                     /* 1-based, 越界忽略 */
void tui_printf(const char *fmt, ...);
void tui_fill(int r1, int c1, int r2, int c2, char ch);      /* 矩形填充 */
void tui_box(int r1, int c1, int r2, int c2, const char *title);  /* 单线 ASCII 边框 */
void tui_reverse(bool on);                           /* 反显/高亮开关 */
void tui_title(const char *title);                   /* 满宽反显标题栏 (第 1 行), 全屏界面统一模板 */
void tui_hint(const char *text);                     /* 底部提示栏 (末行, 居中, 统一样式) */

/* ---- 输入: 阻塞读取一个按键/字符 (含 ESC 方向键序列) ---- */
tui_event_t tui_getch(void);

/* ---- 小部件: 上下选择菜单 ----
 * 返回选中索引; Esc→TUI_QUIT_ESC, Ctrl+C→TUI_QUIT_CTRC。*sel 为初始项, 退出时更新。 */
int tui_menu(const char *title, const char **items, int n, int *sel);

/* ---- 镜像输出: TUI 直写的 ANSI 字节流同时转发给外部显示后端 ----
 * 用于 LCD 同步展示 (tui_lcd)。可在 tui_start 后/前随时挂载。
 * fn 为 NULL 时解除镜像。 */
void tui_set_mirror(void (*fn)(const char *buf, int len));

/* ---- TUI 演示: 注册为 shell 命令 "tui" 即可使用 ---- */
void tui_demo_run(const char *arg);

#endif /* __TUI_H */
