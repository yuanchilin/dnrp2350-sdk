/**
 * @file    core1_riscv.c
 * @brief   RISC-V (Hazard3) 战士 — Q16.16 定点渲染完整 Mandelbrot 帧 + 计时
 *
 *          裸机 freestanding 代码, 由 ARM 核拷贝到 DUEL_CORE1_CODE 后
 *          经 multicore_launch_core1_raw 启动。编译参数:
 *
 *            riscv-none-elf-gcc -march=rv32imac_zicsr -mabi=ilp32 \
 *                -ffreestanding -O2 -nostdlib
 *
 *          注意: RP2350 的 Hazard3 核无硬件 FPU, 全部用整数定点运算。
 *          渲染算法复用 duel_shared.h 的 static inline 纯函数 (与 ARM 同源)。
 */

#include <stdint.h>
#include <stdbool.h>
#include "duel_shared.h"

/* RP2350 TIMER0 只读微秒计数 (timerawl) — 不依赖 SDK, 裸机直接读 */
#define TIMER0_AWL (*(volatile uint32_t *)0x400B0028UL)

/* 陷阱处理器: 记录 mcause 并停住 (调试用) */
__attribute__((interrupt("machine"))) void trap_handler(void)
{
    uint32_t cause;
    __asm__ volatile("csrr %0, mcause" : "=r"(cause));
    DSHARED->rv_trap_cause = cause;
    while (1) { __asm volatile ("wfi"); }
}

/* 入口放在 .text 最前, 确保链接后位于二进制起始 (地址 = DUEL_CORE1_CODE) */
void core1_entry(void) __attribute__((section(".text.entry")));
void core1_entry(void)
{
    /* 陷阱处理: mtvec → trap_handler */
    __asm__ volatile("la t0, trap_handler\n\tcsrw mtvec, t0" ::: "t0");
    DSHARED->rv_start_us = TIMER0_AWL;

    /* 渲染与 ARM 共用 duel_shared.h 里同一份算法 (static inline, 无平台依赖) */
    duel_render_frame();

    DSHARED->rv_end_us = TIMER0_AWL;
    __asm volatile ("fence rw,rw");     /* 确保 fb 写完后才置完成标志 */
    DSHARED->rv_done = true;
    while (1) { __asm volatile ("wfi"); }
}
