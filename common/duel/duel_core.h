/**
 * @file    duel_core.h
 * @brief   ARM vs RISC-V 异构对战 — ARM 侧驱动 API
 *
 *          依赖: pico_multicore (core1 启动), hardware_watchdog (等待期喂狗),
 *                duel_shared.h (共享内存/算法)。
 *          不依赖 shell / LCD — 交互层见 duel_cmd.h。
 *
 *          整机 RISC-V 平台 (PICO_RISCV=1) 时对战无意义 (需 core0=ARM),
 *          所有 API 降级为空实现, 宿主代码无需 #ifdef。
 */

#ifndef DUEL_CORE_H
#define DUEL_CORE_H

#include <stdint.h>
#include <stdbool.h>
#include "duel_shared.h"

/* 一轮对战的结果 (由 duel_run_round 填充) */
typedef struct {
    uint32_t arm_us, rv_us;      /* 各核渲染耗时 (us) */
    bool     rv_done;            /* RISC-V 是否在超时前完成 */
    uint32_t rv_trap_cause;      /* 若未完成: mcause (调试) */
    bool     arm_won;            /* 胜负判定 (都完成时) */
} duel_result_t;

#ifdef __riscv
static inline void duel_init(void) {}
static inline duel_result_t duel_run_round(void) { duel_result_t r = {0, 0, false, 0, false}; return r; }
static inline void duel_pan_right(void) {}
static inline void duel_zoom_in(void) {}
static inline void duel_zoom_out(void) {}
#else

/**
 * @brief 初始化对战环境 (调用一次, 在 shell 注册命令之前)
 *
 *        拷贝内嵌 RISC-V 二进制到共享代码区, 切换 Core1 架构为 RISC-V
 *        (archsel 普通寄存器, 非 OTP 持久), 复位 Core1, 设置默认视图参数。
 */
void duel_init(void);

/**
 * @brief 跑一轮对战 (ARM 渲染 + 启动 RISC-V 并行渲染 + 等待结果)
 *
 *        阻塞至双方完成或 RISC-V 超时 (30s, 内部喂看门狗, 永不卡死 shell)。
 *        Core1 完成渲染后进入 wfi 空闲, 下次调用会再次 reset+launch。
 */
duel_result_t duel_run_round(void);

/* 视图参数调整 (供 shell 命令调用, 调整后自行 duel_run_round) */
void duel_pan_right(void);       /* 视口右移 */
void duel_zoom_in(void);         /* 放大 2x */
void duel_zoom_out(void);        /* 缩小 2x */

#endif /* __riscv */

#endif /* DUEL_CORE_H */
