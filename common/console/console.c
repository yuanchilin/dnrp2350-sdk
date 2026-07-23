/**
 * @file    console.c
 * @brief   LCD 文本控制台实现
 */

#include "console.h"
#include "BSP/LCD/lcd.h"
#include <string.h>
#include <stdio.h>

static int  _cols, _rows, _fw, _fh, _fs;
static int  _cx, _cy;
static char _screen[16][64];    /* 最大 64 列 × 16 行 */
static char _scroll[128][64];
static int  _scroll_idx;

void console_init(int cols, int rows, int fw, int fh, uint8_t fs)
{
    _cols = cols; _rows = rows;
    _fw = fw; _fh = fh; _fs = fs;
    _cx = 0; _cy = 0;
    for (int i = 0; i < rows; i++) memset(_screen[i], ' ', cols);
}

void console_clear(void)
{
    for (int i = 0; i < _rows; i++) memset(_screen[i], ' ', _cols);
    _cx = _cy = 0;
    console_draw();
}

void console_draw_line(int y)
{
    lcd_fill(0, y * _fh, _cols * _fw - 1, (y + 1) * _fh - 1, BLACK);
    for (int x = 0; x < _cols; x++) {
        char ch = _screen[y][x];
        if (ch >= ' ') lcd_show_char(x * _fw, y * _fh, ch, _fs, 1, GREEN);
    }
}

void console_draw(void)
{
    lcd_fill(0, 0, _cols * _fw - 1, _rows * _fh - 1, BLACK);
    for (int y = 0; y < _rows; y++)
        for (int x = 0; x < _cols; x++) {
            char ch = _screen[y][x];
            if (ch >= ' ') lcd_show_char(x * _fw, y * _fh, ch, _fs, 1, GREEN);
        }
}

static void _scroll_up(void)
{
    snprintf(_scroll[_scroll_idx % 128], 64, "%s", _screen[0]);
    _scroll_idx++;
    for (int i = 0; i < _rows - 1; i++) memcpy(_screen[i], _screen[i + 1], _cols);
    memset(_screen[_rows - 1], ' ', _cols);
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
    } else if (c >= ' ') {
        if (_cx >= _cols) { _cx = 0; _cy++; if (_cy >= _rows) { _scroll_up(); _cy = _rows - 1; } }
        _screen[_cy][_cx++] = c;
        console_draw_line(_cy);
    }
}

void console_print(const char *s) { while (*s) console_putc(*s++); }
void console_println(const char *s) { console_print(s); console_putc('\n'); }
int  console_cols(void) { return _cols; }
int  console_rows(void) { return _rows; }
