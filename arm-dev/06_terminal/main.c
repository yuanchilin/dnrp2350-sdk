/**
 * @file    main.c
 * @brief   06 — 迷你终端 (全公共库)
 */

#include "pico/stdlib.h"
#include "BSP/LCD/lcd.h"
#include "BSP/LED/led.h"
#include "BSP/SPI/spi.h"
#include "BSP/UART/uart.h"
#include "console/console.h"
#include "shell/shell.h"
#include "commands/commands.h"
#include "ff.h"

int main(void)
{
    led_init(); LED(0);
    spi1_init(); lcd_init();
    uart_init_dev(); sleep_ms(50); while (uart_read_byte() >= 0);
    console_init(30, 8, 8, 16, 16);

    FATFS fs;
    bool sd_ok = (f_mount(&fs, "0:", 1) == FR_OK);
    shell_init("$ ");
    shell_set_echo_cb(console_putc);  /* 串口输入 → LCD 同步 */
    commands_init(sd_ok);
    commands_register_all();

    if (sd_ok) commands_view_file("/photo.bmp");
    console_println("06 Terminal");
    console_println("Type help");

    while (1) { shell_poll(); sleep_ms(1); }
    return 0;
}
