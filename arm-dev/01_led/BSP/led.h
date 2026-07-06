/**
 * @file    led.h
 * @brief   DNRP2350A 板载 LED 驱动
 * @note    红色 LED 接 GPIO3，灌电流接法：低电平点亮，高电平熄灭
 */

#ifndef __BSP_LED_H
#define __BSP_LED_H

#include "pico/stdlib.h"

/* -------------------------------------------------------------------------- */
/*  硬件定义                                                                   */
/* -------------------------------------------------------------------------- */
#define LED_GPIO_PIN    3           /* 板载红色 LED */

/* 电平语义 */
#define LED_ON_LEVEL    0           /* 低电平点亮（灌电流接法） */
#define LED_OFF_LEVEL   1           /* 高电平熄灭 */

/* ========================================================================== */
/*  控制宏                                                                     */
/* ========================================================================== */
#define LED_ON()        gpio_put(LED_GPIO_PIN, LED_ON_LEVEL)
#define LED_OFF()       gpio_put(LED_GPIO_PIN, LED_OFF_LEVEL)
#define LED_TOGGLE()    gpio_put(LED_GPIO_PIN, !gpio_get(LED_GPIO_PIN))

/* ========================================================================== */
/*  API                                                                        */
/* ========================================================================== */
void led_init(void);

#endif /* __BSP_LED_H */
