/**
 * @file    main.c
 * @brief   06 — 迷你终端 (全公共库)
 */

#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include "board/board.h"
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
    board_init();
    watchdog_enable(5000, 1);

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

    while (1) { watchdog_update(); shell_poll(); sleep_ms(1); }
    return 0;
}
