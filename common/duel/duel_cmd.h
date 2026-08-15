/**
 * @file    duel_cmd.h
 * @brief   ARM vs RISC-V 异构对战 — shell 命令层 + LCD 战报条
 *
 *          依赖: duel_core (驱动), shell (命令注册), BSP/LCD (战报条),
 *                BSP/UART (串口战报)。
 *          不依赖 console/TUI — 战斗结束后若宿主需要恢复终端界面,
 *          通过 duel_set_restore_cb() 注册回调 (宿主 main 里做)。
 */

#ifndef DUEL_CMD_H
#define DUEL_CMD_H

#include <stdint.h>

/* 整机 RISC-V 平台: 对战无意义 (需 core0=ARM), 提供空实现, 宿主代码无需 #ifdef */
#ifdef __riscv
static inline void duel_register_cmds(void) {}
static inline void duel_set_restore_cb(void (*cb)(void)) { (void)cb; }
static inline void duel_set_frame_display_cb(void (*cb)(void)) { (void)cb; }
static inline void duel_set_hold_ms(uint32_t ms) { (void)ms; }
#else
/**
 * @brief 注册 duel/next/zoom/out 四个 shell 命令 (在 shell_init 之后调用)
 */
void duel_register_cmds(void);

/**
 * @brief 注册战斗结束后回调 (如 08 里重绘 console 终端界面)
 * @param cb  回调函数指针; 传 NULL 表示不恢复 (04 用)
 */
void duel_set_restore_cb(void (*cb)(void));

/**
 * @brief 注册自定义刷屏回调 (替代默认 SPI 逐行刷帧)
 * @param cb  回调; 传 NULL 用默认 (SPI lcd_show_frame)
 *
 *        08 用 PIO+DMA 加速时传 pio_lcd_blit_frame 的包装 (整帧 DMA 刷新,
 *        比 SPI 逐行快一个量级)。回调只负责把 DSHARED->fb 刷到 LCD,
 *        战报条仍由本模块用 SPI 绘制叠加。
 */
void duel_set_frame_display_cb(void (*cb)(void));

/**
 * @brief 设置战报停留时长 (战斗画面+战报条在 LCD 上停留多久)
 * @param ms  毫秒; 0 = 不额外停留 (04 保持画面直到下次命令)
 *
 *        停留结束后调用 restore_cb (若注册)。08 设几千 ms 让用户看清战报,
 *        再自动恢复终端界面。
 */
void duel_set_hold_ms(uint32_t ms);

#endif /* __riscv */

#endif /* DUEL_CMD_H */
