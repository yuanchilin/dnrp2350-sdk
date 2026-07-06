/**
 * @file    lcd.h
 * @brief   DNRP2350A 1.14" LCD 驱动 (ST7789, 240x135, SPI)
 * @note    SPI1: SCK=GPIO10, MOSI=GPIO11, CS=GPIO9, DC=GPIO8, BL=GPIO25
 */

#ifndef __BSP_LCD_H
#define __BSP_LCD_H

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/pwm.h"

/* ========================================================================== */
/*  硬件引脚定义                                                               */
/* ========================================================================== */
#define LCD_SPI_PORT    spi1
#define LCD_SCK_PIN     10          /* SPI1 时钟 */
#define LCD_MOSI_PIN    11          /* SPI1 数据 */
#define LCD_CS_PIN      9           /* 片选 (Chip Select) */
#define LCD_DC_PIN      8           /* 数据/命令 (Data/Command) */
#define LCD_BL_PIN      25          /* 背光 PWM */

/* ========================================================================== */
/*  屏幕参数                                                                   */
/* ========================================================================== */
#define LCD_WIDTH       240
#define LCD_HEIGHT      135

/* ========================================================================== */
/*  常用颜色 (RGB565)                                                          */
/* ========================================================================== */
#define COLOR_BLACK     0x0000
#define COLOR_WHITE     0xFFFF
#define COLOR_RED       0xF800
#define COLOR_GREEN     0x07E0
#define COLOR_BLUE      0x001F
#define COLOR_YELLOW    0xFFE0
#define COLOR_CYAN      0x07FF
#define COLOR_MAGENTA   0xF81F
#define COLOR_GRAY      0x8430

/* ========================================================================== */
/*  控制宏                                                                     */
/* ========================================================================== */
#define LCD_CS_LOW()    gpio_put(LCD_CS_PIN, 0)
#define LCD_CS_HIGH()   gpio_put(LCD_CS_PIN, 1)
#define LCD_DC_CMD()    gpio_put(LCD_DC_PIN, 0)   /* DC=0: 命令 */
#define LCD_DC_DATA()   gpio_put(LCD_DC_PIN, 1)   /* DC=1: 数据 */

/* ========================================================================== */
/*  API                                                                        */
/* ========================================================================== */
void lcd_init(void);
void lcd_set_backlight(uint8_t brightness);     /* 0~100 */
void lcd_clear(uint16_t color);
void lcd_fill(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void lcd_draw_pixel(uint16_t x, uint16_t y, uint16_t color);
void lcd_show_char(uint16_t x, uint16_t y, char ch, uint16_t color, uint16_t bg);
void lcd_show_string(uint16_t x, uint16_t y, const char *str, uint16_t color, uint16_t bg);
void lcd_show_num(uint16_t x, uint16_t y, int num, uint8_t len, uint16_t color, uint16_t bg);

#endif /* __BSP_LCD_H */
