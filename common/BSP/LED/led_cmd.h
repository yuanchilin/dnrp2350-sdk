/**
 * @file    led_cmd.h
 * @brief   LED shell 命令层 — 串口动态控制板载 LED
 *
 *          用法 (在 shell_init 后调用 led_register()):
 *            led on             点亮
 *            led off            熄灭
 *            led toggle         翻转
 *            led blink [ms]     闪烁 (默认 500ms, 阻塞不返回)
 *
 *          注意: DNRP2350A 的 GPIO25 与 LCD 背光复用,
 *          led blink 会同时驱动背光, LCD 工程慎用。
 */

#ifndef __LED_CMD_H
#define __LED_CMD_H

/* 注册 led 命令 (依赖 SHELL + BSP/LED) */
void led_register(void);

#endif /* __LED_CMD_H */
