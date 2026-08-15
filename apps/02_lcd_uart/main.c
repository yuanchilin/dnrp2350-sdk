/**
 * @file    main.c
 * @brief   DNRP2350A LCD+UART — Hello World (公共 shell 版)
 *
 *          串口命令: 借用公共 shell (help/reboot/reset) + 自定义 hello
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include "board/board.h"
#include "BSP/LCD/lcd.h"
#include "BSP/LED/led.h"
#include "BSP/SPI/spi.h"
#include "BSP/UART/uart.h"
#include "shell/shell.h"

/* ========================================================================== */
/*  自定义命令: hello                                                          */
/* ========================================================================== */
static void cmd_hello(const char *arg)
{
    (void)arg;
    shell_printf("Hello Justin ! (RP2350 | Cortex-M33)\r\n");
    lcd_show_string(10, 10, 220, 48, 32, "Hello Justin !", BLUE);
}

/* ========================================================================== */
/*  主函数                                                                     */
/* ========================================================================== */
int main(void)
{
    board_init();
    watchdog_enable(5000, 1);

    /* ---- LCD 启动画面 ---- */
    lcd_show_string(10, 10,  220, 48, 32, "Hello Justin !", BLUE);
    lcd_show_string(10, 70,  220, 32, 24, "DNRP2350A",   RED);
    lcd_show_string(10, 105, 220, 24, 16, "RP2350 | Cortex-M33", MAGENTA);
    LED(0);                                 /* LED 常亮 (低电平点亮) */

    /* ---- 初始化公共 shell ---- */
    shell_init("$ ");
    shell_register("hello", "say hello", cmd_hello);
    /* 内置: help / reboot / reset */

    uart_printf("\r\n========================================\r\n");
    uart_printf(" DNRP2350A — Hello World\r\n");
    uart_printf(" Type 'help' for commands\r\n");
    uart_printf("========================================\r\n\r\n");

    /* ---- shell 主循环 ---- */
    while (1) {
        watchdog_update();
        shell_poll();
        sleep_ms(1);
    }

    return 0;
}