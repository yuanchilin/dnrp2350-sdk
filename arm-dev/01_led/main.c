/**
 * @file    main.c
 * @brief   DNRP2350A SDK 入门 —— LED 闪烁 (ARM Cortex-M33)
 * @note    板载红色 LED 以 500ms 周期闪烁
 */

#include "pico/stdlib.h"
#include "BSP/led.h"

int main(void)
{
    stdio_init_all();               /* USB/串口 标准 IO */
    led_init();                     /* 板载 LED 初始化 */

    while (1) {
        LED_TOGGLE();               /* 翻转 LED */
        sleep_ms(500);              /* 500ms */
    }

    return 0;
}
