/**
 * @file    tui.c
 * @brief   全屏 TUI 框架实现 (串口 ANSI 原语 + 菜单小部件)
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include "BSP/UART/uart.h"
#include "shell/ansi.h"
#include "tui/tui.h"

/* ESC 序列解码时等待后续字节的超时 (ms) */
#define TUI_SEQ_TO  120


/* 镜像 hook: 唤醒 LCD 同步后端 (可空) */
static void (*_mirror)(const char *buf, int len) = NULL;

void tui_set_mirror(void (*fn)(const char *, int)) { _mirror = fn; }

static void tui_send(const char *s, int len)
{
    uart_send_buf((const uint8_t *)s, (uint16_t)len);
    if (_mirror) _mirror(s, len);
}

static void tui_send_str(const char *s)
{
    tui_send(s, (int)strlen(s));
}

/* 带超时的单字节读取: 返回字节或 -1 */
static int _rb(int timeout_ms)
{
    int n = timeout_ms / 10;
    if (n <= 0) n = 1;
    for (int i = 0; i < n; i++) {
        watchdog_update();
        int c = uart_read_byte();
        if (c >= 0) return c;
        sleep_ms(10);
    }
    return -1;
}

void tui_start(void)
{
    tui_send_str("\x1b[?1049h\x1b[2J\x1b[H\x1b[?25l" ANSI_RESET);
}

void tui_stop(void)
{
    tui_send_str("\x1b[?25h" ANSI_RESET "\x1b[?1049l");
}

void tui_move(int row, int col)
{
    if (row < 1 || row > TUI_ROWS || col < 1 || col > TUI_COLS) return;
    char b[16];
    int n = snprintf(b, sizeof(b), "\x1b[%d;%dH", row, col);
    tui_send(b, n);
}

void tui_printf(const char *fmt, ...)
{
    char b[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof(b), fmt, ap);
    va_end(ap);
    tui_send_str(b);
}

void tui_reverse(bool on)
{
    /* 反显关闭必须用 SGR 27 而非 ANSI_RESET(0): 不少串口终端对 0m 不关反显,
     * 否则曾高亮过的行会一直保持反显, 上下翻时整列菜单像"全被选中"。 */
    tui_send_str(on ? "\x1b[7m" : "\x1b[27m");
}

/* 满宽反显标题栏 (第 1 行): 全屏界面统一模板, 各界面共用保证布局一致 */
void tui_title(const char *title)
{
    int len = (int)strlen(title);
    if (len > TUI_COLS) len = TUI_COLS;
    tui_move(1, 1);
    tui_reverse(true);
    tui_printf("%-*s", TUI_COLS, "");
    tui_move(1, (TUI_COLS - len) / 2 + 1);
    tui_printf("%.*s", len, title);
    tui_reverse(false);
}

/* 底部提示栏: 末行(TUI_ROWS)居中, 弱化灰显, 全屏界面统一模板 */
void tui_hint(const char *text)
{
    int len = (int)strlen(text);
    if (len > TUI_COLS) len = TUI_COLS;
    tui_move(TUI_ROWS, (TUI_COLS - len) / 2 + 1);
    tui_printf(ANSI_BRIGHT_BLACK "%-*.*s" ANSI_RESET, len, len, text);
}

void tui_fill(int r1, int c1, int r2, int c2, char ch)
{
    if (r1 < 1) r1 = 1;
    if (r2 > TUI_ROWS) r2 = TUI_ROWS;
    if (c1 < 1) c1 = 1;
    if (c2 > TUI_COLS) c2 = TUI_COLS;
    if (r2 < r1 || c2 < c1) return;

    char b[TUI_COLS * 2];
    int len = c2 - c1 + 1;
    if (len > (int)sizeof(b) - 1) len = (int)sizeof(b) - 1;
    memset(b, ch, (size_t)len);
    b[len] = '\0';
    for (int r = r1; r <= r2; r++) {
        tui_move(r, c1);
        tui_send_str(b);
    }
}

void tui_box(int r1, int c1, int r2, int c2, const char *title)
{
    int w = c2 - c1 - 1;
    if (w <= 0) return;
    char bar[256];
    for (int i = 0; i < w; i++) bar[i] = '-';
    bar[w] = '\0';

    tui_move(r1, c1);
    tui_send_str("+");
    tui_send_str(bar);
    tui_send_str("+");

    for (int r = r1 + 1; r < r2; r++) {
        tui_move(r, c1); tui_send_str("|");
        tui_move(r, c2); tui_send_str("|");
    }

    tui_move(r2, c1);
    tui_send_str("+");
    tui_send_str(bar);
    tui_send_str("+");

    if (title && *title) {
        int tlen = (int)strlen(title);
        if (tlen + 2 <= w) {
            int off = (w - tlen) / 2;
            tui_move(r1, c1 + 1 + off);
            tui_send_str(title);
        }
    }
}

tui_event_t tui_getch(void)
{
    tui_event_t e = { TUI_KEY_CHAR, 0 };
    int c;
    while ((c = uart_read_byte()) < 0) {
        watchdog_update();
        sleep_ms(1);
    }

    if (c == 0x1b) {                     /* ESC 或 ESC[ 方向键序列 */
        int c1 = _rb(TUI_SEQ_TO);
        if (c1 == '[') {
            int c2 = _rb(TUI_SEQ_TO);
            switch (c2) {
            case 'A': e.type = TUI_KEY_UP; break;
            case 'B': e.type = TUI_KEY_DOWN; break;
            case 'C': e.type = TUI_KEY_RIGHT; break;
            case 'D': e.type = TUI_KEY_LEFT; break;
            default:  e.type = TUI_KEY_ESC; break;
            }
        } else {
            e.type = TUI_KEY_ESC;        /* 裸 ESC */
        }
        return e;
    }

    if (c == '\r' || c == '\n')    { e.type = TUI_KEY_ENTER; return e; }
    if (c == 0x7f || c == '\b')    { e.type = TUI_KEY_BACK;  return e; }
    if (c == 0x03)                 { e.type = TUI_KEY_CTRLC; return e; }  /* Ctrl+C */

    e.type = TUI_KEY_CHAR;
    e.ch = (char)c;
    return e;
}

#define _MENU_HINT   "move:UD  select:ENT  quit:ESC"   /* 长度≤30, 适配 LCD 单行, 避免超行 */

/* 重绘单个菜单行: 取消高亮(false) 或 高亮(true); 超长截断防越界。
   高亮只包 label 实际字符, 右侧补正常色空格, 避免 LCD 上整行反显条向右延伸 */
static void _menu_row(int row, int col, int w, bool sel, const char *label)
{
    int len = (int)strlen(label);
    if (len > w) len = w;
    tui_move(row, col);
    if (sel) tui_reverse(true);
    tui_printf("%-*.*s", len, len, label);
    if (sel) tui_reverse(false);
    if (len < w) tui_printf("%*s", w - len, "");
}

/* 右侧滚动条: '.'=轨道  '#'=滑块位置 */
static void _menu_bar(int r0, int col, int vis, int n, int s)
{
    if (n <= 1) return;
    int t = (s * (vis - 1)) / (n - 1);
    for (int k = 0; k < vis; k++) {
        tui_move(r0 + 1 + k, col);
        tui_send_str((k == t) ? "#" : ".");
    }
}

int tui_menu(const char *title, const char **items, int n, int *sel)
{
    if (n <= 0) return -1;
    int s = *sel;
    if (s < 0 || s >= n) s = 0;

    int w = 0;                            /* 最宽一项 → 决定内容宽 */
    for (int i = 0; i < n; i++) {
        int l = (int)strlen(items[i]);
        if (l > w) w = l;
    }
    if (w > TUI_COLS - 4) w = TUI_COLS - 4;

    /* 全屏铺满布局: 标题栏 row1, 内容区 row2..TUI_ROWS-1, 提示栏 row TUI_ROWS (末行) */
    int content_top = 2;
    int content_h  = TUI_ROWS - 2;        /* 内容行数 (标题栏 1 + 提示栏 1 之外), 提示栏占末行 */
    bool scrollable = (n > content_h);
    int vis = scrollable ? content_h : n;
    int col = 1;                          /* 内容左对齐 (顶格) */
    int bcol = TUI_COLS;                  /* 滚动条列 (最右) */

    /* 标题栏: 满宽反显 */
    tui_title(title);

    /* 初始窗口: 保证选中项可见 */
    int top = 0;
    if (s >= top + vis) top = s - vis + 1;
    if (scrollable && top > n - vis) top = n - vis;
    if (top < 0) top = 0;

    /* 窗口条目 + 滚动条 + 提示栏: 只画一次 */
    int oldS = s, oldTop = top;
    for (int k = 0; k < vis; k++)
        _menu_row(content_top + k, col, w, (top + k == s), items[top + k]);
    if (scrollable) _menu_bar(content_top - 1, bcol, vis, n, s);
    tui_hint(_MENU_HINT);

    for (;;) {
        watchdog_update();
        tui_event_t e = tui_getch();
        if (e.type == TUI_KEY_ENTER)       { *sel = s; return s; }
        if (e.type == TUI_KEY_ESC)         { *sel = s; return TUI_QUIT_ESC; }
        if (e.type == TUI_KEY_CTRLC)       { *sel = s; return TUI_QUIT_CTRC; }

        int ns = s;
        if (e.type == TUI_KEY_UP)        ns--;
        else if (e.type == TUI_KEY_DOWN) ns++;
        else                             continue;
        if (ns < 0)  ns = n - 1;
        if (ns >= n) ns = 0;
        if (ns == s) continue;

        /* 校正窗口, 保证选中项可见 */
        int ntop = top;
        if (ns < ntop) ntop = ns;
        else if (ns >= ntop + vis) ntop = ns - vis + 1;
        if (scrollable && ntop > n - vis) ntop = n - vis;
        if (ntop < 0) ntop = 0;

        if (ntop != oldTop) {
            /* 翻页: 整窗重绘 */
            for (int k = 0; k < vis; k++)
                _menu_row(content_top + k, col, w, (ntop + k == ns), items[ntop + k]);
            top = ntop;
        } else {
            /* 行内移动: 只重绘旧/新两行, 避免整屏闪烁 */
            _menu_row(content_top + oldS - top, col, w, false, items[oldS]);
            top = ntop;
            _menu_row(content_top + ns - top, col, w, true, items[ns]);
        }
        if (scrollable) _menu_bar(content_top - 1, bcol, vis, n, ns);
        s = ns;
        oldS = s;
        oldTop = top;
    }
}
