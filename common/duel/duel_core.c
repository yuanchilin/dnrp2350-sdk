/**
 * @file    duel_core.c
 * @brief   ARM vs RISC-V 异构对战 — ARM (Cortex-M33) 侧驱动
 *
 *          Core0 = ARM: 渲染完整 Mandelbrot 帧 + 计时 + 启动/等待 Core1
 *          Core1 = Hazard3 (RISC-V): 渲染同一帧 + 计时 (内嵌二进制, archsel 切换)
 *
 *          注意: RISC-V bootrom 启动协议是 7 个词且"收到 0 即回握手", 见
 *          launch_core1_riscv() 注释 — 这不是 SDK 默认的 6 词 ARM 协议。
 */

#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/watchdog.h"
#include "hardware/structs/otp.h"
#include "duel_core.h"

/* ========================================================================== */
/*  内嵌的 RISC-V Core1 二进制 (CMake 生成 core1_riscv.bin)                     */
/* ========================================================================== */
#ifndef CORE1_BIN_PATH
#error "CORE1_BIN_PATH 未定义 (drp_project 未启用 DUEL 交叉编译?)"
#endif
#define DUEL_STR2(x) #x
#define DUEL_STR(x)  DUEL_STR2(x)
__asm__(
    ".section .rodata\n"
    ".balign 4\n"
    ".global core1_bin_start\n"
    "core1_bin_start:\n"
    ".incbin \"" DUEL_STR(CORE1_BIN_PATH) "\"\n"
    ".global core1_bin_end\n"
    "core1_bin_end:\n"
);
extern const uint8_t core1_bin_start[];
extern const uint8_t core1_bin_end[];

/* ========================================================================== */
/*  启动 RISC-V Core1 — RISC-V bootrom 协议是 7 个词且"收到 0 即回握手":      */
/*  (pico-bootrom-rp2350 riscv_bootrom_rt0.S: {0,0,1, vt, sp, 未用, ip})       */
/*  注意: vt 与"未用"词必须非零 (receive_and_check_zero 遇 0 回握手循环)!      */
/* ========================================================================== */
static void launch_core1_riscv(void)
{
    /* RISC-V bootrom 协议 (pico-bootrom-rp2350 riscv_bootrom_rt0.S):
     *   {0,0,1, vt, sp, 未用, ip} — 前 6 词逐一回显; 第 7 词 (ip) 收到后
     *   bootrom 立即跳转, 不再回显! (SDK 的 ARM 循环在最后一步等回显会卡死)
     *   且 vt/未用 词必须非零 (receive_and_check_zero 遇 0 回握手循环)。 */
    const uint32_t seq[7] = {
        0, 0, 1,
        DUEL_CORE1_CODE,         /* vt → mtvec (非零) */
        DUEL_CORE1_STACK,        /* sp → 栈顶 */
        1,                       /* 未用词 (非零) */
        DUEL_CORE1_CODE          /* ip → 实际跳转入口 (不回显) */
    };
    for (int i = 0; i < 6; i++) {
        uint cmd = seq[i];
        if (!cmd) { multicore_fifo_drain(); __sev(); }
        multicore_fifo_push_blocking(cmd);
        multicore_fifo_pop_blocking();     /* 回显确认 */
    }
    /* 第 7 词: 只推不等回显 */
    __sev();
    multicore_fifo_push_blocking(seq[6]);
}

/* ========================================================================== */
/*  公共 API                                                                   */
/* ========================================================================== */
void duel_init(void)
{
    /* 内嵌 RISC-V 二进制拷到共享代码区 */
    memcpy((void *)DUEL_CORE1_CODE, core1_bin_start,
           (size_t)(core1_bin_end - core1_bin_start));
    __dmb();

    /* 切换 Core1 架构 → RISC-V (archsel 是普通寄存器, 非 OTP 持久) */
    hw_write_masked(&otp_hw->archsel,
                    (uint32_t)OTP_ARCHSEL_CORE1_VALUE_RISCV << OTP_ARCHSEL_CORE1_LSB,
                    OTP_ARCHSEL_CORE1_BITS);
    __dmb();
    multicore_reset_core1();

    /* 默认视图: Mandelbrot 中心区 */
    DSHARED->cx = Q16(-0.75); DSHARED->cy = Q16(0.1); DSHARED->scale = Q16(2.5);
}

duel_result_t duel_run_round(void)
{
    duel_result_t r;
    memset(&r, 0, sizeof(r));

    multicore_reset_core1();
    DSHARED->rv_done = false;
    DSHARED->rv_start_us = 0;
    launch_core1_riscv();
    __dmb();

    DSHARED->arm_start_us = time_us_32();
    duel_render_frame();
    DSHARED->arm_end_us = time_us_32();
    __dmb();

    /* 等 RISC-V 完成, 最多 30s — 超时也返回, 保证 shell 永不卡死 */
    uint32_t t0 = time_us_32();
    while (!DSHARED->rv_done && (time_us_32() - t0) < 30000000) {
        watchdog_update();
        sleep_ms(1);
    }

    r.arm_us        = DSHARED->arm_end_us - DSHARED->arm_start_us;
    r.rv_us         = DSHARED->rv_end_us  - DSHARED->rv_start_us;
    r.rv_done       = DSHARED->rv_done;
    r.rv_trap_cause = DSHARED->rv_trap_cause;
    r.arm_won       = r.rv_done && (r.arm_us <= r.rv_us);
    return r;
}

void duel_pan_right(void) { DSHARED->cx += (q16)(((int64_t)DSHARED->scale * 3) / 10); }
void duel_zoom_in(void)   { DSHARED->scale >>= 1; }
void duel_zoom_out(void)  { DSHARED->scale <<= 1; }
