/**
 * @file    tui_lcd.c
 * @brief   TUI → LCD 同步镜像实现
 *
 * 设计:
 *  - TUI 已按 LCD 尺寸定制 (30x8, 8x16 字体), 虚拟屏即 LCD 视口, 全屏显示无切省。
 *  - 解析 TUI 输出的 ANSI (光标 H/f、清屏 2J、反显 7/0、色 30-37/90-97、字符),
 *    写入 30x8 虚拟字符屏。
 *  - 每行像素由本地合成一次批量写回 LCD, 绝不逐字符/逐像素切窗。
 *  - 只画脏行/脏格 → 渲染开销低, 不拖慢 UART 吞吐。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "BSP/LCD/lcd.h"
#include "BSP/UART/uart.h"
#include "console/console.h"
#include "tui/tui.h"
#include "tui/tui_lcd.h"

#define VS_ROWS     8
#define VS_COLS     30

#define CFG_SEL     LGRAYBLUE
#define CFG_BG      BLACK

static char          vs[VS_ROWS][VS_COLS];
static uint8_t       vs_fg[VS_ROWS][VS_COLS];
static bool          vs_sel[VS_ROWS][VS_COLS];
static bool          vs_dirty[VS_ROWS][VS_COLS];
static bool          row_dirty[VS_ROWS];

static int           _cx, _cy;
static int           _sel;
static uint8_t       _fg;

enum { ST_TXT, ST_ESC, ST_CSI, ST_SKIP };
static int           _st = ST_TXT;
static char          _par[16];
static int           _plen;

/* 8 色 → RGB565 (与 tui 的 30-37/90-97 约定对应) */
static const uint16_t _rgbtab[8] = { GRAY, RED, GREEN, YELLOW, BLUE, MAGENTA, CYAN, WHITE };

static void vs_reset(void)
{
    for (int r = 0; r < VS_ROWS; r++) {
        memset(vs[r], ' ', VS_COLS);
        memset(vs_fg[r], 0, VS_COLS);
        memset(vs_sel[r], 0, VS_COLS);
        memset(vs_dirty[r], 1, VS_COLS);
    }
    memset(row_dirty, 1, sizeof(row_dirty));
    _cx = _cy = _sel = 0; _fg = 0;
}

static void vs_goto(int row, int col)
{
    if (row < 1) row = 1;
    if (row > VS_ROWS) row = VS_ROWS;
    if (col < 1) col = 1;
    if (col > VS_COLS) col = VS_COLS;
    _cy = row - 1;
    _cx = col - 1;
}

static void vs_put(char ch)
{
    if (_cx >= VS_COLS) { _cx = 0; _cy++; }
    if (_cy >= VS_ROWS) _cy = VS_ROWS - 1;
    vs[_cy][_cx] = ch;
    vs_fg[_cy][_cx] = _fg;
    vs_sel[_cy][_cx] = (_sel != 0);
    vs_dirty[_cy][_cx] = true;
    row_dirty[_cy] = true;
    _cx++;
}

static void vs_clear(void)
{
    for (int r = 0; r < VS_ROWS; r++) {
        memset(vs[r], ' ', VS_COLS);
        memset(vs_fg[r], 0, VS_COLS);
        memset(vs_sel[r], 0, VS_COLS);
        memset(vs_dirty[r], 1, VS_COLS);
        row_dirty[r] = true;
    }
    _cx = _cy = 0;
}

static int par_int(int *ix)
{
    int v = 0;
    while (*ix < _plen && _par[*ix] == ';') (*ix)++;   /* 跳过 ';' 分隔符 */
    while (*ix < _plen && _par[*ix] >= '0' && _par[*ix] <= '9') {
        v = v * 10 + (_par[*ix] - '0');
        (*ix)++;
    }
    return v;
}

static void vs_csi(char final)
{
    int i = 0;
    switch (final) {
    case 'H': case 'f':   vs_goto(par_int(&i), par_int(&i)); break;
    case 'J':             if (_plen == 0 || _par[0] == '2') vs_clear(); break;
    case 'm': {
        if (_plen == 0) { _sel = 0; _fg = 0; break; }
        while (i < _plen) {
            int a = i;
            int v = par_int(&i);
            if (i == a) i++;          /* 非法字符: 跳过, 防死循环 */
            if      (v == 0)             { _sel = 0; _fg = 0; }
            else if (v == 7)             { _sel = 1; }
            else if (v == 27)            { _sel = 0; }   /* 反显关闭 (tui_reverse off 用 SGR 27) */
            else if (v >= 30 && v <= 37) { _fg = (uint8_t)(v - 30); _sel = 0; }
            else if (v >= 90 && v <= 97) { _fg = (uint8_t)(v - 90); _sel = 0; }
        }
        break;
    }
    default: break;
    }
    _st = ST_TXT;
}

static void consume(const char *buf, int len)
{
    for (int k = 0; k < len; k++) {
        unsigned char c = (unsigned char)buf[k];
        switch (_st) {
        case ST_TXT:
            if      (c == 0x1B)           _st = ST_ESC;
            else if (c == '\r')           /* ignore */;
            else if (c == '\n') { _cy++; if (_cy >= VS_ROWS) _cy = VS_ROWS - 1; _cx = 0; }
            else if (c < ' ')             /* other ctrl */;
            else                          vs_put((char)c);
            break;
        case ST_ESC:
            if (c == '[') { _st = ST_CSI; _plen = 0; }
            else           _st = ST_TXT;
            break;
        case ST_CSI:
            if      (c == 'H' || c == 'f' || c == 'J' || c == 'm') {
                _par[_plen] = '\0'; vs_csi((char)c);
            }
            else if (c == '?')            _st = ST_SKIP;
            else if ((c >= '0' && c <= '9') || c == ';') {
                if (_plen < (int)sizeof(_par) - 1) _par[_plen++] = (char)c;
            }
            else                          _st = ST_TXT;
            break;
        case ST_SKIP:
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) _st = ST_TXT;
            break;
        }
    }
}

/* 渲染一行: 把全屏 30 列字符合成进 console 缓冲 (8x16, 前景+背景),
 * 再由 console 的 PIO+DMA flush 一次性上屏 (与开机 photo/console 同一渲染通道)。
 * 只画脏格, 增量渲染, 不拖慢 UART 吞吐。 */
static void render_row(int vr)
{
    if (!row_dirty[vr]) return;
    row_dirty[vr] = false;
    if (vr >= console_rows()) return;

    for (int c = 0; c < VS_COLS; c++) {
        if (!vs_dirty[vr][c]) continue;
        vs_dirty[vr][c] = false;
        uint16_t fg = vs_sel[vr][c] ? WHITE : _rgbtab[vs_fg[vr][c] & 7];
        uint16_t bg = vs_sel[vr][c] ? CFG_SEL : CFG_BG;
        console_put_cell(c, vr, vs[vr][c], fg, bg);
    }
    console_draw_line(vr);
}

static void render(void)
{
    for (int i = 0; i < VS_ROWS; i++)
        render_row(i);
}

static void feed(const char *buf, int len)
{
    consume(buf, len);
    render();
}

void tui_lcd_attach(void)
{
    vs_reset();
    tui_set_mirror(feed);
    console_clear();
}
void tui_lcd_detach(void)
{
    tui_set_mirror(NULL);
    /* 退出 TUI 后清屏重绘, 让 LCD 离开 TUI 画面, 回到 shell 提示符 */
    console_clear();
}