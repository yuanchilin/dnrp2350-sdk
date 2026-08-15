/**
 * @file    main.c
 * @brief   08 — BadUSB 注入 + PIO 终端 (汇总工程)
 *
 *          07 的终端能力 (Shell / Console / TUI / PIO LCD / FatFs)
 *          + 05 的 BadUSB 注入能力 (HID 键盘 + payload 引擎), 注入变成 shell 命令:
 *
 *            badusb          列出 SD 卡 .txt 脚本
 *            badusb demo     注入内置示例
 *            badusb <n>      注入第 n 个脚本
 *            badusb <path>   注入指定脚本
 *            KEY1            一键注入当前选中脚本
 */

#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include "board/board.h"
#include "BSP/SPI/spi.h"
#include "BSP/LCD/lcd.h"
#include "BSP/UART/uart.h"
#include "pio_lcd/pio_lcd.h"
#include "console/console.h"
#include "shell/shell.h"
#include "commands/commands.h"
#include "tui/tui.h"
#include "badusb/badusb.h"
#include "duel/duel_core.h"
#include "duel/duel_cmd.h"
#include "BSP/LED/led_cmd.h"
#include "ff.h"

static void echo_colored(char c) { console_set_color(0x03E0, BLACK); console_putc(c); }
static void out_colored(const char *s) { console_set_color(GRAY, BLACK); console_write_ansi(s); console_putc('\n'); }

/* 战斗结束恢复终端界面: 重绘 console 全屏 + 清掉文本区之外的底部残留 (135-128=7px) */
static void duel_restore_console(void)
{
    console_draw();
    lcd_fill(0, 8 * 16, LCD_W - 1, LCD_H - 1, BLACK);
}

/* 战斗画面用 PIO+DMA 整帧刷屏 (比 SPI 逐行快一个量级) */
static void duel_show_pio(void)
{
    pio_lcd_blit_frame((const uint16_t *)DSHARED->fb);
}

int main(void)
{
    board_init();
    uart_send_string("[V2]\r\n");

    /* 看门狗: 串口自检后开启 */
    watchdog_enable(5000, 1);

    /* PIO+DMA LCD 硬件加速 */
    if (pio_lcd_init()) console_set_pio_mode(pio_lcd_char, pio_lcd_flush);

    console_init(30, 8, 8, 16, 16);
    console_clear();                            /* 全屏铺黑底 (否则白底残留) */
    /* 清掉 console 文本区之外的 LCD 残留: LCD 高 135px, 文本区只占 240x128 (8行x16),
     * 底部 7px 是 lcd_init 清屏时的白色, 不盖掉会留一条粗白条 */
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
    badusb_init(sd_ok);        /* 自动写示例脚本 + 扫描 .txt + KEY */
    badusb_register();         /* badusb 命令 */
    led_register();            /* led 命令: on/off/toggle/blink */

    /* 异构对战 (ARM vs RISC-V): PIO 整帧刷屏 + 战报停留 5s + 恢复 console 终端 */
    duel_init();
    duel_set_frame_display_cb(duel_show_pio);
    duel_set_hold_ms(5000);
    duel_set_restore_cb(duel_restore_console);
    duel_register_cmds();

    shell_print("\r\n== DNRP2350A BadUSB Terminal ==\r\n");
    shell_print("SD: "); shell_print(sd_ok ? "OK" : "ERR"); shell_print("\r\n");
    shell_print("Type 'help' for commands, 'badusb' to inject\r\n");
    console_println("08 BadUSB Terminal");
    console_println("badusb = inject");

    while (1) {
        watchdog_update();
        shell_poll();          /* 串口 shell */
        badusb_task();         /* TinyUSB 设备循环 + KEY1 轮询注入 */
        sleep_ms(1);
    }
    return 0;
}
