/**
 * @file    console.h
 * @brief   LCD 文本控制台 — 屏幕缓冲/滚动/绘制
 */

#ifndef __CONSOLE_H
#define __CONSOLE_H

#include <stdint.h>
#include <stdbool.h>

void console_init(int cols, int rows, int font_w, int font_h, uint8_t font_size);
typedef void (*console_char_fn)(int x, int y, char ch, uint16_t fg, uint16_t bg);
typedef void (*console_flush_fn)(int x, int y, int w, int h);
void console_set_pio_mode(console_char_fn render, console_flush_fn flush);
void console_set_color(uint16_t fg, uint16_t bg);  /* 设置前景/背景色 */
void console_clear(void);
void console_putc(char c);
void console_print(const char *s);
void console_println(const char *s);
void console_write_ansi(const char *s);  /* 解析 ANSI 颜色码打印 (与串口同源) */
void console_draw(void);
void console_draw_line(int y);
int  console_cols(void);
int  console_rows(void);

#endif
