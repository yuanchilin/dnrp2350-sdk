/**
 * @file    main.c
 * @brief   06 — 迷你终端 (全公共库)
 */

#include "pico/stdlib.h"
#include "BSP/LED/led.h"
#include "BSP/SPI/spi.h"
#include "BSP/LCD/lcd.h"
#include "BSP/UART/uart.h"
#include "console/console.h"
#include "shell/shell.h"
#include "commands/commands.h"
#include "ff.h"

int main(void)
{
    /* 板级初始化: UART 先就绪, 保证串口日志可见 */
    led_init(); LED(0);
    spi1_init();
    lcd_init();

    uart_init_dev();
    sleep_ms(50);
    while (uart_read_byte() >= 0);   /* 清 UART 噪声 */

    console_init(30, 8, 8, 16, 16);
    console_clear();                            /* 全屏铺黑底 (否则白底残留) */
    lcd_fill(0, 8 * 16, 239, 134, BLACK);       /* 盖掉文本区外的底部残留 (240x135 屏) */

    FATFS fs;
    bool sd_ok = (f_mount(&fs, "0:", 1) == FR_OK);

    shell_init("$ ");
    shell_set_echo_cb(console_putc);

    commands_init(sd_ok);
    commands_register_all();

    if (sd_ok) commands_view_file("/photo.bmp");
    console_println("06 Terminal");
    console_println("Type help");

    while (1) { shell_poll(); sleep_ms(1); }
    return 0;
}
