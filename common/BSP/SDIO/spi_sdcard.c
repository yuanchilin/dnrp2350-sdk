/**
 ****************************************************************************************************
 * @file        spi_sdcard.c
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

#include "BSP/SDIO/spi_sdcard.h"

static sd_card_t *pSD = NULL;

/**
 * @brief       定义SPI硬件配置数组
 * @details     每个SPI的硬件配置信息存储在该数组中，多个SD卡可由一个SPI驱动，只要使用不同的从设备选择引脚
 * @note        目前只定义了一个SPI的配置
 */
static spi_t spis[] = {
    {
        .hw_inst = spi1,                /* 使用的SPI硬件实例,指定为spi1 */
        .miso_gpio = 12,                /* SPI的MISO引脚对应的GPIO编号,注意这里是GPIO编号而非引脚编号 */
        .mosi_gpio = 11,                /* SPI的MOSI引脚对应的GPIO编号 */
        .sck_gpio = 10,                 /* SPI的SCK引脚对应的GPIO编号 */
        .baud_rate = 25 * 1000 * 1000,  /* SPI的波特率 */
    }
};

/**
 * @brief       定义SD卡硬件配置数组
 * @details     每个SD卡的硬件配置信息存储在该数组中
 * @note        目前只定义了一个SD卡的配置
 */
static sd_card_t sd_cards[] = {
    {
        .pcName = "0:",             /* SD卡挂载时使用的名称，用于标识该SD卡设备 */
        .spi = &spis[0],            /* 指向驱动该SD卡的SPI对象的指针，表明该SD卡由spis数组中的第一个SPI对象驱动 */
        .ss_gpio = 15,              /* 该SD卡的SPI从设备选择引脚对应的GPIO编号，用于选中该SD卡进行通信 */
        .use_card_detect = false,   /* 是否使用卡检测功能,false表示不使用卡检测功能 */
        .card_detect_gpio = -1,     /* 卡检测引脚对应的GPIO编号,由于不使用卡检测功能，设置为 -1 */
        .card_detected_true = 1     /* 当使用卡检测功能时，卡存在时GPIO读取的值,这里由于不使用卡检测功能，该值无实际意义 */
    }
};

/**
 * @brief       获取SD卡对象的数量
 * @param       无
 * @retval      size_t : SD卡对象的数量
 */
size_t sd_get_num() 
{
    return count_of(sd_cards);
}

/**
 * @brief       根据编号获取对应的SD卡对象指针
 * @param       num : SD卡的编号
 * @retval      sd_card_t* : 对应的SD卡对象指针，若编号超出范围则返回NULL
 */
sd_card_t *sd_get_by_num(size_t num) 
{
    if (num < sd_get_num()) 
    {
        return &sd_cards[num];
    } 
    else 
    {
        return NULL;
    }
}

/**
 * @brief       获取SPI对象的数量
 * @param       无
 * @retval      size_t : SPI对象的数量
 */
size_t spi_get_num() 
{
    return count_of(spis);
}

/**
 * @brief       根据编号获取对应的SPI对象指针
 * @param       num : SPI的编号
 * @retval      spi_t* : 对应的SPI对象指针，若编号超出范围则返回NULL
 */
spi_t *spi_get_by_num(size_t num) 
{
    if (num < spi_get_num()) 
    {
        return &spis[num];
    } 
    else 
    {
        return NULL;
    }
}

/**
 * @brief       SD卡初始化
 * @param       无
 * @retval      esp_err_t
 */
void sd_init(uint32_t *free, uint32_t *total)
{
    pSD = sd_get_by_num(0);     /* 获取SD卡实例 */
    uint8_t fr = f_mount(&pSD->fatfs, pSD->pcName, 1);

    if (FR_OK != fr)
    {
        panic("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
    }

    uint32_t fre_clust = 0, fre_sect = 0, tot_sect = 0;

    FATFS *fs;                  /* FatFs文件系统结构体 */

    fr = f_getfree(pSD->pcName, &fre_clust, &fs);   /* 获取剩余空间（单位：簇） */

    if (FR_OK == fr)            /* 计算剩余空间（单位：字节） */
    {
        fre_sect = fre_clust * fs->csize;
        tot_sect = (fs->n_fatent - 2) * fs->csize;
        *free = fre_sect >> 1;	/* 单位为KB */
        *total = tot_sect >> 1;	/* 单位为KB */
    } 
    else 
    {
        printf("f_getfree error: %s (%d)\n", FRESULT_str(fr), fr);
    }

    f_unmount(pSD->pcName);     /* 卸载文件系统 */
}