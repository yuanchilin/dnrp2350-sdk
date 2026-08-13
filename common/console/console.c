/**
 * @file    console.c
 * @brief   LCD 文本控制台 — CPU / 自定义 flush 双后端
 */

#include "console.h"
#include "BSP/LCD/lcd.h"
#include <string.h>
#include <stdio.h>

/* 缓冲固定尺寸 — 调用方传入的 cols/rows 必须不超过此上限, 否则缓冲溢出 */
#define CONSOLE_MAX_COLS 64
#define CONSOLE_MAX_ROWS 16

static int  _cols, _rows, _fw, _fh, _fs;
static int  _cx, _cy;
static char _screen[CONSOLE_MAX_ROWS][CONSOLE_MAX_COLS];
static uint16_t _scolor[CONSOLE_MAX_ROWS][CONSOLE_MAX_COLS];   /* per-字符前景色, 与 _screen 对应 (支持行内多色/滚动还原) */
static uint16_t _sbkg[CONSOLE_MAX_ROWS][CONSOLE_MAX_COLS];     /* per-字符背景色 (TUI 反显/高亮用) */
static char _scroll[128][CONSOLE_MAX_COLS];
static int  _scroll_idx;

/* 默认配色: 灰字黑底 (与串口终端默认前景一致) */
static uint16_t _fg = GRAY;
static uint16_t _bg = BLACK;

/* ANSI 8色 + 8亮色 → RGB565 (见 ansi.h 的颜色序号约定) */
static const uint16_t _ansi_rgb[16] = {
    BLACK, RED,   GREEN, YELLOW,                  /*   0-7 标准色 */
    BLUE,  MAGENTA, CYAN, WHITE,
    GRAY,  RED,   0x3FE0, YELLOW,                 /*   8-15 亮色: 亮绿用提亮黄绿色, 与暗绿(输入)区分 */
    BLUE,  MAGENTA, CYAN, WHITE,
};

/* PIO 模式回调 */
static console_char_fn  _render_fn = NULL;
static console_flush_fn _flush_fn  = NULL;

void console_set_color(uint16_t fg, uint16_t bg) { _fg = fg; _bg = bg; }

void console_put_cell(int x, int y, char ch, uint16_t fg, uint16_t bg)
{
    if (x < 0 || x >= _cols || y < 0 || y >= _rows) return;
    _screen[y][x] = (ch >= ' ') ? ch : ' ';
    _scolor[y][x] = fg;
    _sbkg[y][x]   = bg;
}

void console_set_pio_mode(console_char_fn render, console_flush_fn flush)
    { _render_fn = render; _flush_fn = flush; }

void console_init(int cols, int rows, int fw, int fh, uint8_t fs)
{
    if (cols < 1) cols = 1;
    if (rows < 1) rows = 1;
    if (cols > CONSOLE_MAX_COLS) cols = CONSOLE_MAX_COLS;   /* 防越界: 缓冲尺寸固定 */
    if (rows > CONSOLE_MAX_ROWS) rows = CONSOLE_MAX_ROWS;
    _cols = cols; _rows = rows; _fw = fw; _fh = fh; _fs = fs;
    _cx = 0; _cy = 0;
    for (int i = 0; i < rows; i++) {
        memset(_screen[i], ' ', cols);
        for (int j = 0; j < cols; j++) { _scolor[i][j] = _fg; _sbkg[i][j] = _bg; }
    }
}

void console_clear(void)
{
    for (int i = 0; i < _rows; i++) {
        memset(_screen[i], ' ', _cols);
        for (int j = 0; j < _cols; j++) { _scolor[i][j] = _fg; _sbkg[i][j] = _bg; }
    }
    _cx = _cy = 0;
    console_draw();
}

void console_draw_line(int y)
{
    if (_render_fn && _flush_fn) {
        for (int x = 0; x < _cols; x++) {
            char ch = _screen[y][x];
            _render_fn(x, y, (ch >= ' ') ? ch : ' ', _scolor[y][x], _sbkg[y][x]);
        }
        _flush_fn(0, y * _fh, _cols * _fw, _fh);
        return;
    }
    lcd_fill(0, y * _fh, _cols * _fw - 1, (y + 1) * _fh - 1, _sbkg[y][0]);
    for (int x = 0; x < _cols; x++) {
        char ch = _screen[y][x];
        if (ch >= ' ') lcd_show_char(x * _fw, y * _fh, ch, _fs, 1, _scolor[y][x]);
    }
}

/* 只刷新单个字符 (8x16/对应字宽) — 局部刷新, 避免整行 DMA 卡顿 */
static void console_draw_char(int x, int y)
{
    char ch = _screen[y][x];
    if (_render_fn && _flush_fn) {
        _render_fn(x, y, (ch >= ' ') ? ch : ' ', _scolor[y][x], _sbkg[y][x]);
        _flush_fn(x * _fw, y * _fh, _fw, _fh);
        return;
    }
    lcd_fill(x * _fw, y * _fh, (x + 1) * _fw - 1, (y + 1) * _fh - 1, _sbkg[y][x]);
    if (ch >= ' ') lcd_show_char(x * _fw, y * _fh, ch, _fs, 1, _scolor[y][x]);
}

void console_draw(void)
{
    if (_render_fn && _flush_fn) {
        for (int y = 0; y < _rows; y++)
            for (int x = 0; x < _cols; x++) {
                char ch = _screen[y][x];
                _render_fn(x, y, (ch >= ' ') ? ch : ' ', _scolor[y][x], _sbkg[y][x]);
            }
        _flush_fn(0, 0, _cols * _fw, _rows * _fh);
        return;
    }
    lcd_fill(0, 0, _cols * _fw - 1, _rows * _fh - 1, _bg);
    for (int y = 0; y < _rows; y++)
        for (int x = 0; x < _cols; x++) {
            char ch = _screen[y][x];
            if (ch >= ' ') lcd_show_char(x * _fw, y * _fh, ch, _fs, 1, _scolor[y][x]);
        }
}

static void _scroll_up(void)
{
    snprintf(_scroll[_scroll_idx % 128], 64, "%s", _screen[0]);
    _scroll_idx++;
    for (int i = 0; i < _rows - 1; i++) {
        memcpy(_screen[i], _screen[i + 1], _cols);
        memcpy(_scolor[i], _scolor[i + 1], _cols * sizeof(uint16_t));
        memcpy(_sbkg[i],  _sbkg[i + 1],  _cols * sizeof(uint16_t));
    }
    memset(_screen[_rows - 1], ' ', _cols);
    for (int j = 0; j < _cols; j++) { _scolor[_rows - 1][j] = _fg; _sbkg[_rows - 1][j] = _bg; }
    /* 逐行重绘: 必须重画 fb 后再 flush, 否则 PIO 模式下滚动后旧内容不更新 */
    for (int y = 0; y < _rows; y++) console_draw_line(y);
}

void console_putc(char c)
{
    if (c == '\n') {
        _cx = 0; _cy++;
        if (_cy >= _rows) { _scroll_up(); _cy = _rows - 1; }
    } else if (c == '\r') {
        _cx = 0;
    } else if (c == '\b') {
        if (_cx > 0) _cx--;
        _screen[_cy][_cx] = ' ';
        _scolor[_cy][_cx] = _fg;
        _sbkg[_cy][_cx] = _bg;
        console_draw_char(_cx, _cy);
    } else if (c >= ' ') {
        if (_cx >= _cols) { _cx = 0; _cy++; if (_cy >= _rows) { _scroll_up(); _cy = _rows - 1; } }
        _scolor[_cy][_cx] = _fg;
        _sbkg[_cy][_cx] = _bg;
        _screen[_cy][_cx++] = c;
        console_draw_char(_cx - 1, _cy);
    }
}

void console_print(const char *s) { while (*s) console_putc(*s++); }
void console_println(const char *s) { console_print(s); console_putc('\n'); }
int  console_cols(void) { return _cols; }

/* 解析 ANSI CSI 序列, 切换前景色; 与串口使用同一份含 ANSI 码的字符串, 实现 LCD 彩色打印 */
void console_write_ansi(const char *s)
{
    while (*s) {
        if (*s == 0x1B && s[1] == '[') {
            s += 2;                       /* 跳过 ESC [ */
            int bright = 0, color = -1;
            while (*s && *s != 'm') {
                if (*s == ';') { s++; continue; }
                if (*s >= '0' && *s <= '9') {
                    int val = 0;
                    while (*s >= '0' && *s <= '9') { val = val * 10 + (*s - '0'); s++; }
                    if (val == 0) { _fg = GRAY; }        /* reset → 默认前景(灰) */
                    else if (val == 1) { bright = 1; }
                    else if (val >= 30 && val <= 37) { color = val - 30; }
                } else s++;
            }
            if (*s == 'm') s++;           /* 跳过结尾 m */
            if (color >= 0) _fg = _ansi_rgb[color + (bright ? 8 : 0)];
        } else {
            console_putc(*s++);
        }
    }
}
int  console_rows(void) { return _rows; }
