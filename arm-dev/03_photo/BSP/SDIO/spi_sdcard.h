/**
 ****************************************************************************************************
 * @file        spi_sdcard.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-03
 * @brief       SD卡 驱动代码
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

#ifndef __SPI_SDCARD_H
#define __SPI_SDCARD_H

#include <unistd.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "pico/stdlib.h"
#include "BSP/SPI/spi.h"
#include "BSP/LCD/lcd.h"
#include "my_debug.h"
#include "hw_config.h"
#include "ff.h"
#include "f_util.h"
#include "diskio.h"

/* SD卡初始化 */
void sd_init(uint32_t *free, uint32_t *total);

#endif
