/**
 * @file    main.c
 * @brief   DNRP2350A LCD+UART — Hello World
 *
 *          串口命令:
 *            r  - 重启进入 USB bootloader (免按键烧录)
 *            ?  - 打印帮助
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "BSP/LCD/lcd.h"
#include "BSP/LED/led.h"
#include "BSP/SPI/spi.h"
#include "BSP/uart.h"

/* ========================================================================== */
/*  命令行缓冲区                                                               */
/* ========================================================================== */
#define CMD_BUF_SIZE    32
static char cmd_buf[CMD_BUF_SIZE];
static int  cmd_pos = 0;

static void process_cmd(const char *cmd)
{
    if (strcmp(cmd, "r") == 0) {
        uart_printf("\r\nRebooting to USB bootloader...\r\n\r\n");
        sleep_ms(100);
        reset_usb_boot(0, 0);               /* 一键进 bootloader */
    } else if (strcmp(cmd, "?") == 0) {
        uart_printf("\r\nCommands:\r\n");
        uart_printf("  r  - reboot to USB bootloader\r\n");
        uart_printf("  ?  - this help\r\n\r\n> ");
    } else if (strlen(cmd) > 0) {
        uart_printf("\r\nUnknown: '%s' (type ? for help)\r\n> ", cmd);
    }
}

/* ========================================================================== */
/*  主函数                                                                     */
/* ========================================================================== */
int main(void)
{
    stdio_init_all();
    uart_init_dev();
    led_init();
    spi1_init();
    lcd_init();

    /* ---- LCD 显示 ---- */
    lcd_show_string(10, 10,  220, 48, 32, "Hello Justin !", BLUE);
    lcd_show_string(10, 70,  220, 32, 24, "DNRP2350A",   RED);
    lcd_show_string(10, 105, 220, 24, 16, "RP2350 | Cortex-M33", MAGENTA);
    LED(0);                                 /* LED 常亮 (低电平点亮) */

    /* ---- 串口 ---- */
    uart_printf("\r\n========================================\r\n");
    uart_printf(" DNRP2350A — Hello World\r\n");
    uart_printf(" Type 'r' to reboot to bootloader\r\n");
    uart_printf("========================================\r\n\r\n> ");

    /* ---- 主循环 ---- */
    while (1) {
        int ch = uart_read_byte();
        if (ch >= 0) {
            if (ch == '\r' || ch == '\n') {
                cmd_buf[cmd_pos] = '\0';
                uart_printf("\r\n");
                process_cmd(cmd_buf);
                cmd_pos = 0;
            } else if (ch == '\b' || ch == 0x7F) {
                if (cmd_pos > 0) {
                    cmd_pos--;
                    uart_printf("\b \b");
                }
            } else if (cmd_pos < CMD_BUF_SIZE - 1) {
                cmd_buf[cmd_pos++] = (char)ch;
                uart_send_byte((uint8_t)ch);
            }
        }
        sleep_ms(1);
    }

    return 0;
}
