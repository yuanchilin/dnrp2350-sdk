/**
 * @file    main.c
 * @brief   DNRP2350A 双核斗法 — Mandelbrot 抢行对战
 *
 *          Core0 (裁判+战士): 抢行渲染 + LCD 显示 + 串口战报
 *          Core1 (战士)    : 抢行渲染，写共享帧缓冲
 *
 *          串口命令: n=下一轮 z=放大 x=缩小 r=bootloader
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "pico/multicore.h"
#include "hardware/sync.h"
#include "BSP/LCD/lcd.h"
#include "BSP/LED/led.h"
#include "BSP/SPI/spi.h"
#include "BSP/UART/uart.h"

/* Core1 入口 (core1.c) */
extern void core1_entry(void);

/* ========================================================================== */
/*  常量                                                                       */
/* ========================================================================== */
#define LCD_W        240
#define LCD_H        135
#define MAX_ITER     64
#define TINT_WARM    0x1800    /* Core0 行: 红色调 */
#define TINT_COOL    0x0018    /* Core1 行: 蓝色调 */

/* ========================================================================== */
/*  共享状态 (Core0 + Core1 都可访问)                                          */
/* ========================================================================== */
uint16_t        fb[LCD_H][LCD_W];       /* 共享帧缓冲 */
double          g_cx     = -0.5;        /* Mandelbrot 中心 X */
double          g_cy     =  0.0;        /* Mandelbrot 中心 Y */
double          g_scale  =  3.0;        /* 显示范围 */
volatile int    g_next_row   = 0;       /* 原子抢占: 下一行号 */
volatile int    g_core0_rows = 0;       /* Core0 抢到的行数 */
volatile int    g_core1_rows = 0;       /* Core1 抢到的行数 */
volatile bool   g_duel_start = false;   /* 发令枪 */
spin_lock_t *g_lock = NULL;             /* 自旋锁 (main.c 定义, core1.c extern) */

/* ========================================================================== */
/*  渲染一行 Mandelbrot (带归属染色)                                            */
/* ========================================================================== */
static uint16_t iter_color(int iter, uint16_t tint)
{
    if (iter >= MAX_ITER) return tint;
    float t = (float)iter / (float)MAX_ITER;
    int r = (int)(9.0f * (1.0f-t) * t*t*t * 255);
    int g = (int)(15.0f * (1.0f-t)*(1.0f-t) * t*t * 255);
    int b = (int)(8.5f * (1.0f-t)*(1.0f-t)*(1.0f-t) * t * 255);
    if (r>255)r=255; if (g>255)g=255; if (b>255)b=255;
    if (r<0)r=0; if (g<0)g=0; if (b<0)b=0;
    return (((r>>3)<<11) | ((g>>2)<<5) | (b>>3)) | tint;
}

static void render_row(int y, uint16_t tint)
{
    double dx = g_scale / (double)LCD_W;
    double x0 = g_cx - g_scale * 0.5;
    double cy = g_cy + ((double)y - (double)LCD_H * 0.5) * g_scale / (double)LCD_H;

    for (int x = 0; x < LCD_W; x++) {
        double cx = x0 + (double)x * dx;
        double zx = 0.0, zy = 0.0;
        int iter = 0;
        while (iter < MAX_ITER) {
            double zx2 = zx*zx, zy2 = zy*zy;
            if (zx2 + zy2 > 4.0) break;
            zy = 2.0*zx*zy + cy;
            zx = zx2 - zy2 + cx;
            iter++;
        }
        fb[y][x] = iter_color(iter, tint);
    }
}

/* ========================================================================== */
/*  写入 LCD 一行                                                              */
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
    /* 复位状态 */
    g_next_row   = 0;
    g_core0_rows = 0;
    g_core1_rows = 0;
    g_duel_start = false;

    uart_printf("\r\n=== ROUND ===\r\n");
    uart_printf("Center: (%f, %f) Scale: %f\r\n", g_cx, g_cy, g_scale);

    /* 发令枪: 启动 Core1 */
    multicore_reset_core1();
    multicore_launch_core1(core1_entry);
    g_duel_start = true;

    /* Core0 也抢行 */
    absolute_time_t t0 = get_absolute_time();
    int my_rows = 0;

    while (1) {
        /* 原子抢行 */
        spin_lock_unsafe_blocking(g_lock);
        int row = g_next_row;
        if (row < LCD_H) g_next_row++;
        spin_unlock_unsafe(g_lock);

        if (row >= LCD_H) break;

        render_row(row, TINT_WARM);       /* Core0 = 暖色 */
        my_rows++;
        lcd_write_row(row);               /* 即时显示 */
    }

    absolute_time_t t1 = get_absolute_time();

    /* 等待 Core1 完成 */
    while (g_core1_rows == 0) tight_loop_contents();

    /* 显示 Core1 的行 */
    for (int y = 1; y < LCD_H; y++) {
        if ((fb[y][0] & TINT_WARM) == 0) { /* Core1 的行 */
            lcd_write_row(y);
        }
    }

    /* 画分界线 */
    lcd_fill(0, g_core0_rows, LCD_W - 1, g_core0_rows, WHITE);

    /* 计时 */
    uint64_t elapsed_us = absolute_time_diff_us(t0, t1);
    int core1_rows = g_core1_rows;

    /* ---- 战报 ---- */
    uart_printf("========================================\r\n");
    uart_printf("  MANDELBROT DUEL — RESULTS\r\n");
    uart_printf("========================================\r\n");
    uart_printf("  Core 0 (WARM) : %3d rows  %s\r\n",
                my_rows, my_rows > core1_rows ? "★ WINNER!" : "");
    uart_printf("  Core 1 (COOL) : %3d rows  %s\r\n",
                core1_rows, core1_rows > my_rows ? "★ WINNER!" : "");
    uart_printf("  Total time    : %llu ms\r\n", elapsed_us / 1000);
    uart_printf("========================================\r\n");

    if (my_rows > core1_rows) {
        uart_printf("  🏆 Core 0 WINS by %d rows!\r\n", my_rows - core1_rows);
        LED(0);  /* 红灯亮 (low active) */
    } else if (core1_rows > my_rows) {
        uart_printf("  🏆 Core 1 WINS by %d rows!\r\n", core1_rows - my_rows);
        LED(1);  /* 红灯灭 */
    } else {
        uart_printf("  🤝 DRAW!\r\n");
    }

    uart_printf("\r\nn=next z=zoom-in x=zoom-out r=reboot\r\n> ");
}

/* ========================================================================== */
/*  主函数                                                                     */
/* ========================================================================== */
int main(void)
{
    stdio_init_all();
    uart_init_dev();
    while (uart_read_byte() >= 0);    /* 清启动噪声 */
    led_init();
    spi1_init();
    lcd_init();

    /* 初始化自旋锁 */
    g_lock = spin_lock_init(spin_lock_claim_unused(true));

    uart_printf("\r\n========================================\r\n");
    uart_printf(" DNRP2350A — MANDELBROT DUEL\r\n");
    uart_printf(" 2x Cortex-M33 race for rows!\r\n");
    uart_printf("========================================\r\n");

    lcd_clear(BLACK);
    lcd_show_string(0, 40,  240, 32, 24, "Mandelbrot", BLUE);
    lcd_show_string(0, 70,  240, 24, 16, "DUAL CORE DUEL", RED);
    sleep_ms(1000);

    /* 第一轮 */
    run_duel();

    /* 主循环: 串口命令 */
    char cmd[16];
    int  pos = 0;

    while (1) {
        int ch = uart_read_byte();
        if (ch < 0) { sleep_ms(10); continue; }

        if (ch == '\r' || ch == '\n') {
            if (pos > 0) {
                cmd[pos] = '\0';
                uart_printf("\r\n");
                if (strcmp(cmd, "n") == 0) {
                    g_cx += g_scale * 0.3;      /* 平移 */
                    run_duel();
                } else if (strcmp(cmd, "z") == 0) {
                    g_scale *= 0.5;              /* 放大 */
                    run_duel();
                } else if (strcmp(cmd, "x") == 0) {
                    g_scale *= 2.0;              /* 缩小 */
                    run_duel();
                } else if (strcmp(cmd, "r") == 0) {
                    uart_printf("Reboot to bootloader...\r\n");
                    sleep_ms(100);
                    reset_usb_boot(0, 0);
                } else {
                    uart_printf("? n=next z=zoom x=shrink r=reboot\r\n> ");
                }
                pos = 0;
            }
        } else if (ch == '\b' || ch == 0x7F) {
            if (pos > 0) { pos--; uart_printf("\b \b"); }
        } else if (pos < 15) {
            cmd[pos++] = (char)ch;
            uart_send_byte((uint8_t)ch);
        }
    }

    return 0;
}
