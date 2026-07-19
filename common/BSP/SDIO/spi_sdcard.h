/**
 * @file    spi_sdcard.h
 * @brief   SD 卡 SPI 硬件配置 (需要 FatFs 中间件)
 * @note    仅用于包含 FatFs 的项目 (如 03_photo)
 */

#ifndef __SPI_SDCARD_H
#define __SPI_SDCARD_H

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hw_config.h"
#include "ff.h"
#include "f_util.h"
#include "diskio.h"
#include "my_debug.h"

void sd_init(uint32_t *free, uint32_t *total);

#endif
