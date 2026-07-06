/**
 * @file    main.c
 * @brief   DNRP2350A LCD + UART 综合实验 (ARM Cortex-M33)
 * @note    LCD:   ST7789 240x135, SPI1
 *          UART:  UART0, 115200-8-N-1, CH343 USB 转串口
 *
 *          功能:
 *            - LCD 显示欢迎页 + 系统信息
 *            - 串口接收数据并回显
 *            - LCD 实时显示串口接收的内容
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "BSP/lcd.h"
#include "BSP/uart.h"

/* ========================================================================== */
/*  颜色主题                                                                   */
/* ========================================================================== */
#define THEME_BG        COLOR_BLACK
#define THEME_FG        COLOR_WHITE
#define THEME_ACCENT    COLOR_CYAN
#define THEME_WARN      0xFDA0              /* 橙色 */

/* ========================================================================== */
/*  串口显示缓冲区 (LCD 用)                                                    */
/* ========================================================================== */
static char lcd_line_buf[5][31];            /* 5 行 x 30 字 */
static int  lcd_line_idx = 0;

/* ========================================================================== */
/*  LCD 刷新信息栏                                                             */
/* ========================================================================== */
static void lcd_draw_header(void)
{
    lcd_fill(0, 0, LCD_WIDTH - 1, 19, COLOR_BLUE);
    lcd_show_string(2, 2, "DNRP2350A | LCD+UART", COLOR_WHITE, COLOR_BLUE);
}

static void lcd_draw_status_bar(void)
{
    lcd_fill(0, LCD_HEIGHT - 17, LCD_WIDTH - 1, LCD_HEIGHT - 1, 0x18E3);
    lcd_show_string(2, LCD_HEIGHT - 15, "115200-8N1 | CH343", COLOR_BLACK, 0x18E3);
    char buf[20];
    snprintf(buf, sizeof(buf), "RX:%d", lcd_line_idx);
    lcd_show_string(180, LCD_HEIGHT - 15, buf, COLOR_BLACK, 0x18E3);
}

/* ========================================================================== */
/*  串口 RX 行添加到 LCD 滚动区域                                              */
/* ========================================================================== */
static void lcd_add_rx_line(const char *line)
{
    /* 滚动：所有行上移 */
    for (int i = 0; i < 4; i++) {
        memcpy(lcd_line_buf[i], lcd_line_buf[i + 1], 30);
    }
    strncpy(lcd_line_buf[4], line, 30);
    lcd_line_buf[4][29] = '\0';
    lcd_line_idx++;

    /* 刷新 LCD 滚动区 (Y: 20 ~ 118) */
    lcd_fill(0, 20, LCD_WIDTH - 1, LCD_HEIGHT - 18, THEME_BG);
    for (int i = 0; i < 5; i++) {
        uint16_t c = (i == 4) ? THEME_ACCENT : 0x7BEF;   /* 最新行高亮 */
        lcd_show_string(2, 22 + i * 18, lcd_line_buf[i], c, THEME_BG);
    }
    lcd_draw_status_bar();
}

/* ========================================================================== */
/*  主函数                                                                     */
/* ========================================================================== */
int main(void)
{
    /* ---- 初始化 ---- */
    stdio_init_all();
    lcd_init();
    uart_init_dev();

    /* ---- LCD 欢迎页 ---- */
    lcd_clear(THEME_BG);
    lcd_draw_header();

    lcd_show_string(5, 30,  "=== DNRP2350A ===", THEME_ACCENT, THEME_BG);
    lcd_show_string(5, 50,  "Chip:  RP2350A",    THEME_FG, THEME_BG);
    lcd_show_string(5, 68,  "Core:  Cortex-M33",  THEME_FG, THEME_BG);
    lcd_show_string(5, 86,  "LCD:   240x135 SPI",  THEME_FG, THEME_BG);
    lcd_show_string(5, 104, "UART:  115200 8N1",  THEME_FG, THEME_BG);

    lcd_draw_status_bar();

    /* 初始化滚动缓冲区 */
    for (int i = 0; i < 5; i++) lcd_line_buf[i][0] = '\0';

    /* ---- 串口启动提示 ---- */
    uart_printf("\r\n========================================\r\n");
    uart_printf(" DNRP2350A LCD+UART Demo\r\n");
    uart_printf(" RP2350A | Cortex-M33 | 150MHz\r\n");
    uart_printf("========================================\r\n");
    uart_printf("\r\n> ");

    /* ---- 主循环 ---- */
    char rx_line[128];
    int  rx_pos = 0;

    while (1) {
        int ch = uart_read_byte();

        if (ch >= 0) {
            /* 回显 */
            uart_send_byte((uint8_t)ch);

            if (ch == '\r' || ch == '\n') {
                if (rx_pos > 0) {
                    rx_line[rx_pos] = '\0';
                    lcd_add_rx_line(rx_line);
                    rx_pos = 0;
                }
                uart_printf("\r\n> ");
            } else if (ch == '\b' || ch == 0x7F) {
                if (rx_pos > 0) rx_pos--;
            } else if (rx_pos < (int)sizeof(rx_line) - 1) {
                rx_line[rx_pos++] = (char)ch;
            }
        }

        sleep_ms(1);    /* 让出 CPU */
    }

    return 0;
}
