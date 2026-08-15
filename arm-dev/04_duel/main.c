/**
 * @file    main.c
 * @brief   DNRP2350A 异构对战 — ARM (Cortex-M33) vs RISC-V (Hazard3)
 *
 *          对战功能已抽象为公共模块 common/duel (DUEL BSP 模块):
 *            - duel_shared.h  纯算法层 (Q16.16 渲染, 两侧同源)
 *            - duel_core      平台驱动 (core1 启动/计时/等待)
 *            - duel_cmd       shell 命令 + LCD 战报条
 *          本文件只是宿主: 初始化 + 注册命令 + 主循环。
 *
 *          命令:  duel=跑一轮  next=平移  zoom=放大  out=缩小
 */

#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include "board/board.h"
#include "BSP/LCD/lcd.h"
#include "BSP/UART/uart.h"
#include "shell/shell.h"
#include "duel/duel_core.h"
#include "duel/duel_cmd.h"

int main(void)
{
    board_init();
    watchdog_enable(5000, 1);
    lcd_clear(BLACK);

    /* 对战环境初始化: 拷贝 RISC-V 二进制 + archsel 切换 + 默认视图 */
    duel_init();

    /* ---- shell ---- */
    shell_init("> ");
    duel_register_cmds();

    uart_printf("\r\n========================================\r\n");
    uart_printf(" MANDELBROT DUEL — ARM vs RISC-V\r\n");
    uart_printf(" Core0: Cortex-M33   Core1: Hazard3\r\n");
    uart_printf(" Type 'duel' to battle, 'help' for commands\r\n");
    uart_printf("========================================\r\n");

    /* 开机不自动跑对战: 先进入 shell 保持可 reboot; 输入 duel 才开战 */
    while (1) {
        watchdog_update();
        shell_poll();
        sleep_ms(1);
    }
    return 0;
}
