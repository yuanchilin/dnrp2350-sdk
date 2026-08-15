/**
 ****************************************************************************************************
 * @file        led.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-03
 * @brief       LED驱动代码
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 * @attention
 *
 * 实验平台:正点原子 RP2350A 最小系统板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司网址:www.alientek.com
 * 购买地址:openedv.taobao.com
 *
 ****************************************************************************************************
 */

#include "led.h"


/**
 * @brief       初始化LED
 * @param       无 
 * @retval      无
 */
void led_init(void)
{
    gpio_init(LED_GPIO_PIN);
    gpio_set_dir(LED_GPIO_PIN, GPIO_OUT);
    LED(1);     /* 关闭LED */
}

/**
 * @brief       LED 以固定周期闪烁 (阻塞, 不返回)
 * @param       period_ms: 闪烁周期 (亮+灭 一个完整周期)
 * @retval      无
 * @note        DNRP2350A 的 GPIO25 与 LCD 背光复用: 调用本函数会同时
 *              驱动背光, 需要 LCD 的工程请勿调用 (用 LED_TOGGLE 自控)。
 */
void led_blink(uint32_t period_ms)
{
    if (period_ms < 2) period_ms = 2;
    for (;;) {
        LED(0);                 /* 亮 (低电平点亮) */
        sleep_ms(period_ms / 2);
        LED(1);                 /* 灭 */
        sleep_ms(period_ms / 2);
    }
}
