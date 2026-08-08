/**
 * @file    core1.c
 * @brief   Core1 战士 —— 渲染右半边 Mandelbrot (x=120~239)
 */

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/sync.h"

/* ========================================================================== */
/*  外部共享变量 (main.c)                                                      */
/* ========================================================================== */
#define LCD_W        240
#define LCD_H        135
#define MAX_ITER     512
#define C1_X_START   120
#define C1_X_END     239

extern uint16_t       fb[LCD_H][LCD_W];
extern double         g_cx, g_cy, g_scale;
extern volatile bool  g_go;
extern volatile bool  g_c1_done;

/* ========================================================================== */
/*  颜色映射                                                                   */
/* ========================================================================== */
static uint16_t iter_color(int iter)
{
    if (iter >= MAX_ITER) return 0x0008;
    float t = (float)iter / (float)MAX_ITER;
    int r = (int)(9.0f * (1.0f-t) * t*t*t * 255);
    int g = (int)(15.0f * (1.0f-t)*(1.0f-t) * t*t * 255);
    int b = (int)(8.5f * (1.0f-t)*(1.0f-t)*(1.0f-t) * t * 255);
    if (r>255)r=255; if (g>255)g=255; if (b>255)b=255;
    if (r<0)r=0; if (g<0)g=0; if (b<0)b=0;
    return ((r>>3)<<11) | ((g>>2)<<5) | (b>>3);
}

/* ========================================================================== */
/*  Core1 入口                                                                 */
/* ========================================================================== */
void core1_entry(void)
{
    /* 等发令枪 */
    while (!g_go) tight_loop_contents();

    double dx = g_scale / (double)LCD_W;
    double x0 = g_cx - g_scale * 0.5;

    for (int y = 0; y < LCD_H; y++) {
        for (int x = C1_X_START; x <= C1_X_END; x++) {
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

    /* 内存屏障: 确保 fb 全部写入完成后再置完成标志,
     * 否则 Core0 可能读到未写完的数据 */
    __dmb();
    g_c1_done = true;

    while (1) __wfe();
}
