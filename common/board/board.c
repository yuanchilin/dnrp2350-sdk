/**
 * @file    board.c
 * @brief   板级初始化实现 — 各工程共用的初始化顺序
 */

#include "board.h"
#include "BSP/LED/led.h"
#include "BSP/SPI/spi.h"
#include "BSP/LCD/lcd.h"
#include "BSP/UART/uart.h"

void board_init(void)
{
    uart_init_dev();            /* UART 最先就绪, 保证调试日志可见 */
    sleep_ms(50);
    uart_flush();               /* 清 UART 开机噪声 (原各工程重复的 while(uart_read_byte()>=0)) */

    led_init(); LED(1);         /* 默认关灯: GPIO25 与 LCD 背光复用, LED(0) 会点亮背光+绿灯; LCD 工程由 lcd_init 点亮 */
    spi1_init();
    lcd_init();
}
