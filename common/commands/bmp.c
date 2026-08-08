/**
 * @file    bmp.c
 * @brief   24-bit BMP 解码显示 — TF卡 BMP → LCD (ST7789)
 *          从 03_photo 提取的公共解码器, 供 03 与 shell view 命令共用
 */

#include "bmp.h"
#include <stdio.h>
#include <string.h>
#include "BSP/LCD/lcd.h"
#include "ff.h"

#define LCD_WIDTH   240
#define LCD_HEIGHT  135

/* BMP 文件头 (14 bytes) */
typedef struct __attribute__((packed)) {
    uint16_t bfType;        /* "BM" = 0x4D42 */
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;
} bmp_file_hdr_t;

/* BMP 信息头 (BITMAPINFOHEADER) */
typedef struct __attribute__((packed)) {
    uint32_t biSize;
    int32_t  biWidth;
    int32_t  biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    int32_t  biXPelsPerMeter;
    int32_t  biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
} bmp_info_hdr_t;

/* 行缓冲区: 240 像素 × 3 字节 + 4 对齐 */
#define BMP_LINE_MAX    (240 * 3 + 4)
static uint8_t line_buf[BMP_LINE_MAX];

bool bmp_show(const char *path)
{
    FIL     file;
    FRESULT fr;
    UINT    br;

    fr = f_open(&file, path, FA_READ);
    if (fr != FR_OK) return false;

    /* 1. 读文件头 */
    bmp_file_hdr_t fhdr;
    fr = f_read(&file, &fhdr, sizeof(fhdr), &br);
    if (fhdr.bfType != 0x4D42) {    /* 不是 BM 标识 */
        f_close(&file); return false;
    }

    /* 2. 读信息头 */
    bmp_info_hdr_t ihdr;
    fr = f_read(&file, &ihdr, sizeof(ihdr), &br);
    if (ihdr.biBitCount != 24 || ihdr.biCompression != 0) {
        f_close(&file); return false;
    }

    int img_w = ihdr.biWidth;
    int img_h = (ihdr.biHeight > 0) ? ihdr.biHeight : -ihdr.biHeight;
    int row_bytes = (img_w * 3 + 3) & ~3;   /* 每行字节数 (4字节对齐) */
    bool top_down = (ihdr.biHeight < 0);    /* 负高度 = 顶行在前 */

    /* 3. 缩放比例 */
    int scale_x = (img_w > LCD_WIDTH)  ? (img_w * 10 / LCD_WIDTH)  : 10;
    int scale_y = (img_h > LCD_HEIGHT) ? (img_h * 10 / LCD_HEIGHT) : 10;

    /* 4. 居中偏移 */
    int draw_w = (img_w > LCD_WIDTH)  ? LCD_WIDTH  : img_w;
    int draw_h = (img_h > LCD_HEIGHT) ? LCD_HEIGHT : img_h;
    int off_x  = (LCD_WIDTH  - draw_w) / 2;
    int off_y  = (LCD_HEIGHT - draw_h) / 2;

    /* 5. 设置 LCD 窗口 */
    lcd_set_window(off_x, off_y, off_x + draw_w - 1, off_y + draw_h - 1);
    lcd_write_cmd(0x2C);            /* 内存写 */

    /* 6. 跳转到像素数据 */
    f_lseek(&file, fhdr.bfOffBits);

    /* 7. 逐行读取、缩放、写入 LCD */
    for (int lcd_y = 0; lcd_y < draw_h; lcd_y++) {
        /* BMP 行号 (底行在前, 底行=0) */
        int bmp_y = top_down ? (lcd_y * scale_y / 10)
                             : (img_h - 1 - lcd_y * scale_y / 10);

        /* 定位 + 读行 */
        f_lseek(&file, fhdr.bfOffBits + (FSIZE_t)bmp_y * row_bytes);
        f_read(&file, line_buf, row_bytes, &br);

        /* 缩放行 → LCD */
        for (int lcd_x = 0; lcd_x < draw_w; lcd_x++) {
            int bmp_x = lcd_x * scale_x / 10;
            uint8_t *px = &line_buf[bmp_x * 3];
            uint8_t b = px[0], g = px[1], r = px[2];
            uint16_t rgb565 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
            lcd_write_data16(rgb565);
        }
    }

    f_close(&file);
    return true;
}