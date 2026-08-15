/**
 * @file    duel_shared.h
 * @brief   ARM vs RISC-V 异构对战 — 共享内存布局与纯算法 (两侧一致)
 *
 *          RP2350 的 RISC-V 核 (Hazard3) 无硬件 FPU, 为公平对战,
 *          双方统一用 Q16.16 定点整数渲染 (ARM 同样用整数, 不占 FPU 便宜)。
 *
 *          固定物理地址 (ARM Core0 与 RISC-V Core1 通过同一片 SRAM 通信):
 *
 *            0x2004C000 ~ 0x2004E000   Core1 栈 (向下增长)
 *            0x20050000 ~ 0x2005FFFF   Core1 代码 (内嵌 RISC-V 二进制)
 *            0x20060000 ~ 0x2006FD20   duel_shared_t (参数/计时/共享帧缓冲)
 *
 *          本头文件为"纯算法层": 不含任何平台头/SDK 依赖,
 *          ARM (Cortex-M33) 与 RISC-V (Hazard3) 两侧以 static inline 方式
 *          包含同一份代码, 保证算法永不漂移。
 */

#ifndef DUEL_SHARED_H
#define DUEL_SHARED_H

#include <stdint.h>
#include <stdbool.h>

#define DUEL_LCD_W     240
#define DUEL_LCD_H     135
#define DUEL_MAX_ITER  256

#define DUEL_SHARED_BASE   0x20060000UL
#define DUEL_CORE1_CODE    0x20050000UL   /* RISC-V 二进制拷贝/运行地址 */
#define DUEL_CORE1_STACK   0x2004E000UL   /* Core1 栈顶 (向下增长) */

/* ---- Q16.16 定点数 (两核共用) ---- */
typedef int32_t q16;
#define Q16(x)   ((q16)((x) * 65536.0))
#define Q16_MUL(a, b) ((q16)((((int64_t)(a)) * (b)) >> 16))

typedef struct {
    /* 对战参数 (Q16.16, Core0 每轮写入, Core1 读取) */
    q16     cx, cy, scale;
    /* 各核渲染计时 (us, 各自只写自己的字段) */
    volatile uint32_t arm_start_us, arm_end_us;
    volatile uint32_t rv_start_us,  rv_end_us;
    volatile bool     rv_done;
    volatile uint32_t rv_trap_cause;    /* core1 异常原因 (mcause, 调试用) */
    /* 共享帧缓冲 (两核渲染同一帧, 像素值相同, 并发写无冲突) */
    uint16_t fb[DUEL_LCD_H][DUEL_LCD_W];
} duel_shared_t;

#define DSHARED ((volatile duel_shared_t *)DUEL_SHARED_BASE)

/* 颜色映射 — 整数彩虹色环 (内部黑色, 外部按逃逸次数循环彩色), 双侧共用 */
static inline uint16_t duel_iter_color(int iter)
{
    if (iter >= DUEL_MAX_ITER) return 0x0000;    /* 集合内部: 黑色 */
    int t = iter & 0xFF;                          /* 0..255 循环 */
    int seg = t >> 5;                             /* 0..7 */
    int p = (t & 0x1F) << 3;                      /* 0..248 */
    int r, g, b;
    switch (seg) {
        case 0:  r = 255;      g = p;        b = 0;        break;   /* 红→黄 */
        case 1:  r = 255 - p;  g = 255;      b = 0;        break;   /* 黄→绿 */
        case 2:  r = 0;        g = 255;      b = p;        break;   /* 绿→青 */
        case 3:  r = 0;        g = 255 - p;  b = 255;      break;   /* 青→蓝 */
        case 4:  r = p;        g = 0;        b = 255;      break;   /* 蓝→紫 */
        case 5:  r = 255;      g = 0;        b = 255 - p;  break;   /* 紫→红 */
        case 6:  r = 255;      g = p;        b = 0;        break;
        default: r = 255 - p;  g = 255;      b = 0;        break;
    }
    uint16_t ri = (uint16_t)(r >> 3), gi = (uint16_t)(g >> 2), bi = (uint16_t)(b >> 3);
    return (uint16_t)((ri << 11) | (gi << 5) | bi);
}

/**
 * @brief 渲染完整 Mandelbrot 帧到共享帧缓冲 (ARM/RISC-V 共用同一份算法)
 *
 *        纯计算, 无任何平台依赖: 不碰寄存器/外设/编译器内建,
 *        两边都以 static inline 方式包含, 保证算法永不漂移。
 *        计时由调用方负责 (core0 在函数外记 arm_start/end_us,
 *        core1 在函数外记 rv_start/end_us)。
 */
static inline void duel_render_frame(void)
{
    q16 cx = DSHARED->cx, cy = DSHARED->cy, scale = DSHARED->scale;
    q16 dx = scale / DUEL_LCD_W;
    q16 x0 = cx - (q16)(scale / 2);

    for (int y = 0; y < DUEL_LCD_H; y++) {
        q16 cyf = cy + ((q16)((q16)y - DUEL_LCD_H / 2) * scale) / DUEL_LCD_H;
        for (int x = 0; x < DUEL_LCD_W; x++) {
            q16 cxf = x0 + (q16)(x * dx);
            q16 zx = 0, zy = 0;
            int iter = 0;
            while (iter < DUEL_MAX_ITER) {
                q16 zx2 = Q16_MUL(zx, zx);
                q16 zy2 = Q16_MUL(zy, zy);
                if (zx2 + zy2 > Q16(4.0)) break;
                zy = (q16)(((int64_t)2 * zx * zy) >> 16) + cyf;
                zx = zx2 - zy2 + cxf;
                iter++;
            }
            DSHARED->fb[y][x] = duel_iter_color(iter);
        }
    }
}

#endif /* DUEL_SHARED_H */
