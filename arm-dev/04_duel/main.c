/**
 * @file    main.c
 * @brief   DNRP2350A 双核斗法 — Mandelbrot 左右分屏对战
 *
 *          Core0 (左半边): x=0~119, 裁判, LCD 驱动, 串口战报
 *          Core1 (右半边): x=120~239
 *
 *          串口: n=下一轮 z=放大 x=缩小 r=bootloader
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/sync.h"
#include "BSP/LCD/lcd.h"
#include "BSP/LED/led.h"
#include "BSP/SPI/spi.h"
#include "BSP/UART/uart.h"
#include "shell/shell.h"

extern void core1_entry(void);

/* ========================================================================== */
/*  常量                                                                       */
/* ========================================================================== */
#define LCD_W        240
#define LCD_H        135
#define MAX_ITER     512

/* 分屏: Core0 左半边, Core1 右半边 */
#define C0_X_START   0
#define C0_X_END     119
#define C1_X_START   120
#define C1_X_END     239

/* ========================================================================== */
/*  共享状态                                                                   */
/* ========================================================================== */
uint16_t       fb[LCD_H][LCD_W];         /* 共享帧缓冲 */
double         g_cx    = -0.75;          /* 中心 X */
double         g_cy    =  0.1;           /* 中心 Y */
double         g_scale =  2.5;           /* 显示范围 */
volatile bool  g_go    = false;          /* 发令枪 */
volatile bool  g_c1_done = false;        /* Core1 完成 */

/* ========================================================================== */
/*  颜色映射                                                                   */
/* ========================================================================== */
static uint16_t iter_color(int iter)
{
    if (iter >= MAX_ITER) return 0x0008;        /* 暗蓝 (不纯黑) */
    float t = (float)iter / (float)MAX_ITER;
    int r = (int)(9.0f * (1.0f-t) * t*t*t * 255);
    int g = (int)(15.0f * (1.0f-t)*(1.0f-t) * t*t * 255);
    int b = (int)(8.5f * (1.0f-t)*(1.0f-t)*(1.0f-t) * t * 255);
    if (r>255)r=255; if (g>255)g=255; if (b>255)b=255;
    if (r<0)r=0; if (g<0)g=0; if (b<0)b=0;
    return ((r>>3)<<11) | ((g>>2)<<5) | (b>>3);
}

/* ========================================================================== */
/*  Core0 渲染自己的半屏 (x=0~119)                                             */
/* ========================================================================== */
static void core0_render(void)
{
    double dx = g_scale / (double)LCD_W;
    double x0 = g_cx - g_scale * 0.5;

    for (int y = 0; y < LCD_H; y++) {
        for (int x = C0_X_START; x <= C0_X_END; x++) {
            double cx = x0 + (double)x * dx;
            double cy = g_cy + ((double)y - (double)LCD_H*0.5) * g_scale / (double)LCD_H;
            double zx=0.0, zy=0.0;
            int iter = 0;
            while (iter < MAX_ITER) {
                double zx2=zx*zx, zy2=zy*zy;
                if (zx2+zy2 > 4.0) break;
                zy = 2.0*zx*zy + cy;
                zx = zx2 - zy2 + cx;
                iter++;
            }
            fb[y][x] = iter_color(iter);
        }
    }
}

/* ========================================================================== */
/*  LCD: 写一行                                                                */
/* ========================================================================== */
static void lcd_write_row(int y)
{
    lcd_set_window(0, y, LCD_W - 1, y);
    lcd_write_cmd(0x2C);
    for (int x = 0; x < LCD_W; x++) {
        lcd_write_data16(fb[y][x]);
    }
}

/* ========================================================================== */
/*  跑一轮对战                                                                 */
/* ========================================================================== */
static void run_duel(void)
{
    /* 复位 */
    g_c1_done = false;
    g_go = false;

    uart_printf("\r\n=== ROUND (%.3f, %.3f) scale=%.3f ===\r\n", g_cx, g_cy, g_scale);

    /* 发令: 启动 Core1 */
    multicore_reset_core1();
    multicore_launch_core1(core1_entry);
    /* 内存屏障: 确保 g_cx/g_cy/g_scale 参数已写入后再发令,
     * 否则 Core1 可能读到旧参数 */
    __dmb();
    g_go = true;

    /* Core0 渲染左半边 */
    absolute_time_t t0 = get_absolute_time();
    core0_render();

    /* 等待 Core1 完成 (内存屏障: 确保 fb 写入对 Core0 可见) */
    while (!g_c1_done) tight_loop_contents();
    __dmb();
    absolute_time_t t1 = get_absolute_time();

    /* LCD: 补写 Core1 刚开始时的行 (Core0 可能先写了部分) */
    for (int y = 0; y < LCD_H; y++) {
        lcd_write_row(y);
    }

    /* 画中间分界线 */
    lcd_fill(C0_X_END, 0, C1_X_START, LCD_H - 1, WHITE);

    /* ---- 战报 ---- */
    uint64_t ms = absolute_time_diff_us(t0, t1) / 1000;
    uart_printf("========================================\r\n");
    uart_printf("  MANDELBROT DUEL  —  %llu ms\r\n", ms);
    uart_printf("  Core 0 (LEFT)  : x=0~119\r\n");
    uart_printf("  Core 1 (RIGHT) : x=120~239\r\n");
    uart_printf("========================================\r\n\r\nn=next z=zoom x=shrink r=reboot\r\n> ");
}

/* ========================================================================== */
/*  主函数                                                                     */
/* ========================================================================== */
int main(void)
{
    stdio_init_all();
    uart_init_dev();
    while (uart_read_byte() >= 0);    /* 清噪声 */
    led_init();
    spi1_init();
    lcd_init();
    lcd_clear(BLACK);

    uart_printf("\r\n========================================\r\n");
    uart_printf(" MANDELBROT DUEL — 2x Cortex-M33\r\n");
    uart_printf("========================================\r\n");

    run_duel();

    /* 主循环 */
    char cmd[16]; int pos = 0;
    while (1) {
        int ch = uart_read_byte();
        if (ch < 0) { sleep_ms(10); continue; }
        if (ch == '\r' || ch == '\n') {
            if (pos > 0) {
                cmd[pos] = '\0'; uart_printf("\r\n");
                if      (strcmp(cmd, "n") == 0) { g_cx += g_scale*0.3; run_duel(); }
                else if (strcmp(cmd, "z") == 0) { g_scale *= 0.5;    run_duel(); }
                else if (strcmp(cmd, "x") == 0) { g_scale *= 2.0;    run_duel(); }
                else if (strcmp(cmd, "r") == 0) { shell_reboot(); }
                else uart_printf("? n=next z=zoom x=shrink r=reboot\r\n> ");
                pos = 0;
            }
        } else if (ch == '\b' || ch == 0x7F) {
            if (pos > 0) { pos--; uart_printf("\b \b"); }
        } else if (pos < 15) {
            cmd[pos++] = (char)ch; uart_send_byte((uint8_t)ch);
        }
    }
    return 0;
}
