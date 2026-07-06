/**
 * @file    led.c
 * @brief   DNRP2350A 板载 LED 驱动实现 (RISC-V 版)
 */

#include "led.h"

/**
 * @brief   初始化 LED GPIO
 * @note    配置 GPIO3 为输出，初始状态为熄灭
 */
void led_init(void)
{
    gpio_init(LED_GPIO_PIN);
    gpio_set_dir(LED_GPIO_PIN, GPIO_OUT);
    LED_OFF();                      /* 初始熄灭 */
}
