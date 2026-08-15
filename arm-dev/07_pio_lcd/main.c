/**
 * @file    main.c
 * @brief   07 — PIO+DMA 终端 (全公共库)
 */

#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include "BSP/LED/led.h"
#include "BSP/SPI/spi.h"
#include "BSP/LCD/lcd.h"
#include "BSP/UART/uart.h"
#include "pio_lcd/pio_lcd.h"
#include "console/console.h"
#include "shell/shell.h"
#include "commands/commands.h"
#include "tui/tui.h"
#include "ff.h"

static void echo_colored(char c) { console_set_color(0x03E0, BLACK); console_putc(c); }
static void out_colored(const char *s) { console_set_color(GRAY, BLACK); console_write_ansi(s); console_putc('\n'); }

int main(void)
{
    /* 板级初始化: UART 最先就绪, 保证串口自检/日志可见 */
    led_init(); LED(0);
    spi1_init();
    lcd_init();

    uart_init_dev();
    sleep_ms(50);
    while (uart_read_byte() >= 0);   /* 清 UART 噪声 */
    uart_send_string("[V2]\r\n");    /* 自检标记: 串口链路可用 */

    /* 看门狗: 串口自检后开启, 与已验证的 HEAD 顺序一致 */
    watchdog_enable(5000, 1);

    /* PIO+DMA LCD 硬件加速 (渲染回调供 console 使用) */
    if (pio_lcd_init()) console_set_pio_mode(pio_lcd_char, pio_lcd_flush);

    console_init(30, 8, 8, 16, 16);
    console_clear();                            /* 全屏铺黑底 (否则白底残留) */
    /* 清掉 console 文本区之外的 LCD 残留 (LCD 高 135px, 文本区只占 240x128) */
    lcd_fill(0, 8 * 16, LCD_W - 1, LCD_H - 1, BLACK);

    FATFS fs;
    bool sd_ok = (f_mount(&fs, "0:", 1) == FR_OK);

    shell_init("$ ");
    shell_set_ansi(true);
    shell_set_echo_cb(echo_colored);
    shell_set_output_cb(out_colored);
    shell_register("tui", "fullscreen TUI demo", tui_demo_run);

    commands_init(sd_ok);
    commands_register_all();

    shell_print("\r\n== DNRP2350A PIO LCD Terminal ==\r\n");
    shell_print("SD: "); shell_print(sd_ok ? "OK" : "ERR"); shell_print("\r\n");
    shell_print("Type 'help' for commands\r\n");

    if (sd_ok) commands_view_file("/photo.bmp");
    console_println("07 PIO Terminal");
    console_println("Type help");

    while (1) { watchdog_update(); shell_poll(); sleep_ms(1); }
    return 0;
}
