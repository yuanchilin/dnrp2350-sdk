/**
 * @file    main.c
 * @brief   DNRP2350A SDK 入门 —— LED 闪烁 (双平台: ARM / RISC-V)
 * @note    板载 LED 以 500ms 周期闪烁 (led_blink 封装)
 * @note    01_led 不初始化 LCD, board_init 默认关灯, 绿灯不刺眼
 */

#include "pico/stdlib.h"
#include "BSP/LED/led.h"

int main(void)
{
    stdio_init_all();               /* USB/串口 标准 IO */
    led_init();                     /* 板载 LED 初始化 */
    led_blink(500);                 /* 500ms 周期闪烁 (不返回) */
    return 0;
}
