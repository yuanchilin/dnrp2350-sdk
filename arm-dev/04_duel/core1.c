/**
 * @file    core1.c
 * @brief   Core1 战士 —— 与 Core0 抢行渲染 Mandelbrot
 * @note    通过硬件 spinlock 原子抢占行号，写入共享帧缓冲
 *          每行染色标记归属方
 */

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/sync.h"

/* ========================================================================== */
/*  外部共享变量 (main.c)                                                      */
/* ========================================================================== */
#define LCD_W        240
#define LCD_H        135
#define MAX_ITER     64

extern uint16_t        fb[LCD_H][LCD_W];     /* 共享帧缓冲 */
extern double          g_cx, g_cy, g_scale;  /* Mandelbrot 参数 */
extern spin_lock_t    *g_lock;               /* 硬件自旋锁 (原子抢占) */
extern volatile int    g_next_row;           /* 待抢占的行号 */
extern volatile int    g_core0_rows;         /* Core0 抢到的行数 */
extern volatile int    g_core1_rows;         /* Core1 抢到的行数 */
extern volatile bool   g_duel_start;         /* 对战开始信号 */

/* 行归属染色: Core0=暖色偏移, Core1=冷色偏移 */
#define TINT_WARM    0x1800    /* +红 */
#define TINT_COOL    0x0018    /* +蓝 */

/* ========================================================================== */
/*  颜色映射                                                                   */
/* ========================================================================== */
static uint16_t iter_color(int iter, uint16_t tint)
{
    if (iter >= MAX_ITER) return tint;      /* 集内: 显示归属色 */
    float t = (float)iter / (float)MAX_ITER;
    int r = (int)(9.0f * (1.0f-t) * t*t*t * 255);
    int g = (int)(15.0f * (1.0f-t)*(1.0f-t) * t*t * 255);
    int b = (int)(8.5f * (1.0f-t)*(1.0f-t)*(1.0f-t) * t * 255);
    if (r>255)r=255; if (g>255)g=255; if (b>255)b=255;
    if (r<0)r=0; if (g<0)g=0; if (b<0)b=0;
    uint16_t c = ((r>>3)<<11) | ((g>>2)<<5) | (b>>3);
    return c | tint;    /* 叠加归属色 */
}

/* ========================================================================== */
/*  渲染一行                                                                   */
/* ========================================================================== */
static void render_row(int y, uint16_t tint)
{
    double dx = g_scale / (double)LCD_W;
    double x0 = g_cx - g_scale * 0.5;

    for (int x = 0; x < LCD_W; x++) {
        double cx = x0 + (double)x * dx;
        double cy = g_cy + ((double)y - (double)LCD_H * 0.5) * g_scale / (double)LCD_H;
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
/*  Core1 入口                                                                 */
/* ========================================================================== */
void core1_entry(void)
{
    /* 等发令枪 */
    while (!g_duel_start) tight_loop_contents();

    int my_rows = 0;

    while (1) {
        /* 抢行 */
        spin_lock_unsafe_blocking(g_lock);
        int row = g_next_row;
        if (row < LCD_H) g_next_row++;
        spin_unlock_unsafe(g_lock);

        if (row >= LCD_H) break;    /* 没行可抢了 */

        render_row(row, TINT_COOL);  /* Core1 = 冷色 */
        my_rows++;
    }

    g_core1_rows = my_rows;

    /* 哨兵: 等下一轮 reset */
    while (1) __wfe();
}
