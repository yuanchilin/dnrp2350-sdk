/**
 * @file    console.h
 * @brief   LCD 文本控制台 — 屏幕缓冲/滚动/绘制
 */

#ifndef __CONSOLE_H
#define __CONSOLE_H

#include <stdint.h>
#include <stdbool.h>

void console_init(int cols, int rows, int font_w, int font_h, uint8_t font_size);
void console_clear(void);
void console_putc(char c);
void console_print(const char *s);
void console_println(const char *s);
void console_draw(void);
void console_draw_line(int y);
int  console_cols(void);
int  console_rows(void);

#endif
