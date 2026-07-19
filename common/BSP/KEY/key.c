/**
 ****************************************************************************************************
 * @file        key.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-03
 * @brief       按键驱动代码
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

#include "key.h"


/**
 * @brief       初始化按键
 * @param       无
 * @retval      无
 */
void key_init(void)
{
    gpio_init(KEY_GPIO_PIN);                /* 初始化引脚 */
    gpio_set_dir(KEY_GPIO_PIN, GPIO_IN);    /* 配置引脚为输入模式 */
    gpio_pull_up(KEY_GPIO_PIN);             /* 配置引脚上拉 */
}

/**
 * @brief       按键扫描函数
 * @param       mode:0 / 1, 具体含义如下:
 *              0,  不支持连续按(当按键按下不放时, 只有第一次调用会返回键值,
 *                  必须松开以后, 再次按下才会返回其他键值)
 *              1,  支持连续按(当按键按下不放时, 每次调用该函数都会返回键值)
 * @retval      键值, 定义如下:
 *              KEY_PRES, 1, KEY按下
 */
uint8_t key_scan(uint8_t mode)
{
    uint8_t keyval = 0;
    static uint8_t key_up = 1;    /* 按键松开标志 */

    if(mode)
    {
        key_up = 1;
    }

    if (key_up && (KEY == 0))    /* 按键松开标志为1，且有任意一个按键按下了 */
    {
        sleep_ms(10);               /* 去抖动 */
        key_up = 0;

        if (KEY == 0)
        {
            keyval = KEY_PRES;
        }
    }
    else if (KEY == 1)
    {
        key_up = 1;
    }

    return keyval;                  /* 返回键值 */
}
