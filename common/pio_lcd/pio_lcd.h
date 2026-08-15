/**
 * @file    pio_lcd.h
 * @brief   PIO+DMA LCD 驱动 (ST7789 SPI 硬件加速)
 *
 * 使用 PIO 状态机 + DMA 通道实现 LCD 像素刷新, CPU 零开销。
 * 字体渲染使用 asc2_1608 (8x16), 由 BSP/LCD/lcdfont.h 提供。
 */

#ifndef __PIO_LCD_H
#define __PIO_LCD_H

#include <stdbool.h>
#include <stdint.h>

#define LCD_W 240
#define LCD_H 135

/* 初始化 PIO + DMA, 返回是否成功 */
bool pio_lcd_init(void);

/* 查询 PIO 是否就绪 (用于 console_set_pio_mode 的条件分支) */
bool pio_lcd_ok(void);

/* 刷屏: 将 framebuffer 区域 (x,y,w,h) 通过 PIO+DMA 发送到 LCD */
void pio_lcd_flush(int x, int y, int w, int h);

/* 整帧 blit: 把 RGB565 像素数组 (LCD_W*LCD_H) 拷入 PIO fb 并全屏 DMA 刷新 */
void pio_lcd_blit_frame(const uint16_t *frame);

/* 单字符渲染: 在 (col,row) 位置绘制字符, 8x16 字体 */
void pio_lcd_char(int col, int row, char ch, uint16_t fg, uint16_t bg);

#endif /* __PIO_LCD_H */