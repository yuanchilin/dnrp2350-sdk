/**
 * @file    duel_cmd.c
 * @brief   ARM vs RISC-V 异构对战 — shell 命令层 + LCD 战报条
 *
 *          命令:  duel=跑一轮  next=平移  zoom=放大  out=缩小
 *
 *          LCD 战报条: 顶部蓝条白字标题 + 底部灰条白字战报,
 *          战斗画面直接刷共享帧缓冲 (lcd_set_window + lcd_write_data16),
 *          与 PIO 加速终端共用同一 LCD 驱动层, 无后端冲突;
 *          结束后调 restore_cb 恢复宿主终端 (08) 或保持画面 (04)。
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include "BSP/LCD/lcd.h"
#include "BSP/UART/uart.h"
#include "shell/shell.h"
#include "duel_core.h"

/* 战斗结束后回调 (宿主注册, 用于恢复 console/TUI 界面) */
static void (*_restore_cb)(void) = NULL;
/* 自定义刷屏回调 (宿主注册, 08 用 PIO 整帧 blit; NULL = 默认 SPI) */
static void (*_frame_display_cb)(void) = NULL;
/* 战报停留时长 (0 = 不额外停留) */
static uint32_t _hold_ms = 0;

void duel_set_restore_cb(void (*cb)(void)) { _restore_cb = cb; }
void duel_set_frame_display_cb(void (*cb)(void)) { _frame_display_cb = cb; }
void duel_set_hold_ms(uint32_t ms) { _hold_ms = ms; }

/* ========================================================================== */
/*  LCD 显示共享帧缓冲                                                          */
/* ========================================================================== */
static void lcd_show_frame(void)
{
    for (int y = 0; y < DUEL_LCD_H; y++) {
        watchdog_update();
        lcd_set_window(0, y, DUEL_LCD_W - 1, y);
        lcd_write_cmd(0x2C);
        for (int x = 0; x < DUEL_LCD_W; x++) {
            lcd_write_data16(DSHARED->fb[y][x]);
        }
    }
}

/* ========================================================================== */
/*  跑一轮对战: 驱动 + LCD 战报条 + 串口战报                                    */
/* ========================================================================== */
static void run_round(void)
{
    duel_result_t r = duel_run_round();
    uint32_t arm_ms = r.arm_us / 1000;
    uint32_t rv_ms  = r.rv_us  / 1000;

    /* ---- 战斗画面 + 叠加战报条 (让屏幕自解释) ---- */
    if (_frame_display_cb) _frame_display_cb();   /* PIO 整帧 blit (08) */
    else                    lcd_show_frame();      /* 默认 SPI 逐行 (04) */
    /* 注意: 12 号字体(1206)在此驱动下窗口/点阵不匹配会乱码, 叠加文字统一用 16 号 */
    lcd_fill(0, 0, DUEL_LCD_W - 1, 15, BLUE);
    lcd_show_string_bg(2, 0, 236, 16, 16, "ARM vs RISC-V Mandelbrot Duel", WHITE, BLUE);
    lcd_fill(0, DUEL_LCD_H - 18, DUEL_LCD_W - 1, DUEL_LCD_H - 1, 0x1082);
    char b[32];
    snprintf(b, sizeof(b), "M33:%lums H3:%lums WIN:%s", (unsigned long)arm_ms,
             (unsigned long)rv_ms, r.arm_won ? "ARM" : "RISCV");
    lcd_show_string_bg(2, DUEL_LCD_H - 16, 236, 16, 16, b, WHITE, 0x1082);

    /* ---- 串口战报 ---- */
    uart_printf("========================================\r\n");
    uart_printf("  ARM   (M33)  : %lu ms\r\n", (unsigned long)arm_ms);
    uart_printf("  RISC-V(H3)  : %lu ms%s\r\n", (unsigned long)rv_ms,
                r.rv_done ? "" : " (TIMEOUT)");
    if (!r.rv_done)
        uart_printf("  rv trap    : mcause=0x%lx\r\n", (unsigned long)r.rv_trap_cause);
    uart_printf("  WINNER      : %s\r\n", r.rv_done ?
                (r.arm_won ? "ARM (Cortex-M33)" : "RISC-V (Hazard3)") : "RISC-V 未完成");
    uart_printf("========================================\r\n");

    /* 战报停留: 让用户在 LCD 上看清结果; 任意键 (串口输入) 提前退出 */
    if (_hold_ms) {
        uint32_t t0 = to_ms_since_boot(get_absolute_time());
        while (to_ms_since_boot(get_absolute_time()) - t0 < _hold_ms) {
            watchdog_update();
            if (uart_rx_available() > 0) break;   /* 任意串口输入即恢复终端 */
            sleep_ms(10);
        }
    }
    if (_restore_cb) _restore_cb();
}

/* ========================================================================== */
/*  shell 命令回调                                                             */
/* ========================================================================== */
static void cmd_duel(const char *arg)
{
    (void)arg;
    uart_printf("\r\n=== ROUND (%.3f, %.3f) scale=%.3f ===\r\n",
                (double)DSHARED->cx / 65536.0, (double)DSHARED->cy / 65536.0,
                (double)DSHARED->scale / 65536.0);
    run_round();
}

static void cmd_next(const char *arg) { (void)arg; duel_pan_right(); cmd_duel(arg); }
static void cmd_zoom(const char *arg) { (void)arg; duel_zoom_in();   cmd_duel(arg); }
static void cmd_out (const char *arg) { (void)arg; duel_zoom_out();  cmd_duel(arg); }

void duel_register_cmds(void)
{
    shell_register("duel", "run ARM vs RISC-V round", cmd_duel);
    shell_register("next", "pan view + run", cmd_next);
    shell_register("zoom", "zoom in + run", cmd_zoom);
    shell_register("out",  "zoom out + run", cmd_out);
}
