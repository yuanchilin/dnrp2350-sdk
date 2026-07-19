/**
 * @file    main.c
 * @brief   DNRP2350A 迷你相框 — TF卡 BMP → ST7789 LCD 显示
 * @note    串口命令: n=下一张 p=上一张 a=自动播放 s=文件列表 r=重启bootloader
 *
 *          BMP 解码: 24-bit 格式, BGR→RGB565, 最近邻缩放, 居中
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "BSP/LCD/lcd.h"
#include "BSP/LED/led.h"
#include "BSP/SPI/spi.h"
#include "BSP/SDIO/spi_sdcard.h"
#include "BSP/UART/uart.h"
#include "ff.h"
#include "photo_data.h"     /* 嵌入式 BMP 数据, 上电自动写入 TF 卡 */

/* ========================================================================== */
/*  屏幕常量 (横屏: 240×135)                                                  */
/* ========================================================================== */
#define LCD_WIDTH   240
#define LCD_HEIGHT  135

/* ========================================================================== */
/*  首次启动: 将嵌入式 BMP 写入 TF 卡                                          */
/* ========================================================================== */
static void ensure_photo_on_sd(void)
{
    FIL     fil;
    FILINFO fno;

    /* 检查文件是否存在且大小正确 */
    FRESULT fr = f_stat("/photo.bmp", &fno);
    if (fr == FR_OK && fno.fsize == PHOTO_BMP_SIZE) {
        uart_printf("photo.bmp OK (%lu bytes).\r\n", (unsigned long)fno.fsize);
        return;
    }

    /* 存在但大小不对 → 删除 */
    if (fr == FR_OK) {
        uart_printf("photo.bmp size mismatch (%lu bytes), deleting...\r\n",
                    (unsigned long)fno.fsize);
        f_unlink("/photo.bmp");
    }

    uart_printf("Creating photo.bmp (%d bytes)...\r\n", PHOTO_BMP_SIZE);

    fr = f_open(&fil, "/photo.bmp", FA_WRITE | FA_CREATE_NEW);
    uart_printf("  f_open = %d\r\n", fr);
    if (fr != FR_OK) {
        uart_printf("ERROR: Cannot create file (%d)\r\n", fr);
        return;
    }

    /* 分块写入，每次 512 字节 (一个扇区) */
    #define CHUNK 512
    const uint8_t *p = photo_bmp;
    int remaining = PHOTO_BMP_SIZE;
    int offset = 0;
    bool ok = true;

    while (remaining > 0) {
        int n = (remaining > CHUNK) ? CHUNK : remaining;
        UINT bw = 0;
        fr = f_write(&fil, p, n, &bw);
        if (fr != FR_OK || bw != (UINT)n) {
            uart_printf("  WRITE ERR @%d: fr=%d bw=%u/%d\r\n", offset, fr, bw, n);
            ok = false;
            break;
        }
        p += n;
        offset += n;
        remaining -= n;
    }
    f_close(&fil);

    if (ok) {
        uart_printf("  Write OK! %d bytes\r\n", PHOTO_BMP_SIZE);
    } else {
        uart_printf("  Write FAILED\r\n");
    }
}

/* ========================================================================== */
/*  图片缓冲区: 存储扫描到的 BMP 文件路径                                      */
/* ========================================================================== */
#define MAX_FILES   64
static char file_list[MAX_FILES][64];
static int  file_count = 0;
static int  file_index = 0;

/* ========================================================================== */
/*  行缓冲区: 存 BMP 一行像素的原始数据 (240 像素 × 3 字节 + padding)          */
/* ========================================================================== */
#define BMP_LINE_MAX    (240 * 3 + 4)
static uint8_t line_buf[BMP_LINE_MAX];

/* ========================================================================== */
/*  24-bit BMP 解码器                                                         */
/* ========================================================================== */

/* BMP 文件头 (14 bytes) */
typedef struct __attribute__((packed)) {
    uint16_t bfType;        /* "BM" = 0x4D42 */
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;
} bmp_file_hdr_t;

/* BMP 信息头 (40 bytes = BITMAPINFOHEADER) */
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

/**
 * @brief   解码并显示 24-bit BMP 文件到 LCD
 * @param   path: 文件路径
 * @return  true=成功 false=失败
 */
static bool bmp_show(const char *path)
{
    FIL     file;
    FRESULT fr;
    UINT    br;

    fr = f_open(&file, path, FA_READ);
    if (fr != FR_OK) {
        uart_printf("Open failed: %s (%d)\r\n", path, fr);
        return false;
    }

    /* 1. 读文件头 */
    bmp_file_hdr_t fhdr;
    fr = f_read(&file, &fhdr, sizeof(fhdr), &br);
    if (fhdr.bfType != 0x4D42) {    /* 不是 BM 标识 */
        uart_printf("Not BMP: %s\r\n", path);
        f_close(&file); return false;
    }

    /* 2. 读信息头 */
    bmp_info_hdr_t ihdr;
    fr = f_read(&file, &ihdr, sizeof(ihdr), &br);
    if (ihdr.biBitCount != 24 || ihdr.biCompression != 0) {
        uart_printf("Only 24-bit uncompressed BMP supported\r\n");
        f_close(&file); return false;
    }

    int img_w = ihdr.biWidth;
    int img_h = (ihdr.biHeight > 0) ? ihdr.biHeight : -ihdr.biHeight;
    int row_bytes = (img_w * 3 + 3) & ~3;   /* 每行字节数 (4字节对齐) */
    int padding  = row_bytes - img_w * 3;

    bool top_down = (ihdr.biHeight < 0);    /* 负高度 = 顶行在前 */

    uart_printf("BMP: %dx%d (%d bytes/row)\r\n", img_w, img_h, row_bytes);

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
        /* BMP 行号 (底行在前，底行=0) */
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
    uart_printf("OK: %s (%dx%d → %dx%d)\r\n", path, img_w, img_h, draw_w, draw_h);
    return true;
}

/* ========================================================================== */
/*  扫描 SD 卡根目录 .bmp 文件                                                 */
/* ========================================================================== */
static void scan_bmp_files(void)
{
    DIR     dir;
    FILINFO fno;

    file_count = 0;
    FRESULT fr = f_opendir(&dir, "/");
    if (fr != FR_OK) {
        uart_printf("opendir failed: %d\r\n", fr);
        return;
    }

    uart_printf("Scanning SD card...\r\n");
    while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0]) {
        if (fno.fattrib & AM_DIR) continue;    /* 跳过目录 */

        /* 检查 .bmp / .BMP 后缀 */
        char *ext = strrchr(fno.fname, '.');
        if (ext && (strcasecmp(ext, ".bmp") == 0)) {
            if (file_count < MAX_FILES) {
                snprintf(file_list[file_count], 64, "/%s", fno.fname);
                uart_printf("  [%d] %s  (%lu bytes)\r\n",
                            file_count, fno.fname, (unsigned long)fno.fsize);
                file_count++;
            }
        }
    }
    f_closedir(&dir);
    uart_printf("Found %d BMP files.\r\n", file_count);
}

/* ========================================================================== */
/*  显示 SD 卡信息                                                              */
/* ========================================================================== */
static void show_info(void)
{
    uint32_t free_kb, total_kb;
    sd_init(&free_kb, &total_kb);

    lcd_clear(WHITE);
    lcd_show_string(0, 0,   240, 32, 32, "SD Card Info", BLUE);
    lcd_show_string(0, 40,  240, 24, 24, "Files:", RED);
    lcd_show_num(80, 40, file_count, 3, 24, BLUE);
    lcd_show_string(0, 70,  240, 24, 24, "Free:", RED);
    lcd_show_num(80, 70, (int)(free_kb >> 10), 5, 24, BLUE);
    lcd_show_string(160, 70, 200, 24, 24, "MB", RED);
    lcd_show_string(0, 100, 240, 24, 24, "Total:", RED);
    lcd_show_num(80, 100, (int)(total_kb >> 10), 5, 24, BLUE);
    lcd_show_string(160, 100, 200, 24, 24, "MB", RED);

    uart_printf("SD: Free=%luMB Total=%luMB Files=%d\r\n",
                free_kb >> 10, total_kb >> 10, file_count);
}

/* ========================================================================== */
/*  命令处理                                                                   */
/* ========================================================================== */
static void process_cmd(const char *cmd)
{
    if (strcmp(cmd, "n") == 0) {
        if (file_count == 0) { uart_printf("No BMP files on SD.\r\n"); return; }
        file_index = (file_index + 1) % file_count;
        uart_printf("Showing [%d/%d]\r\n", file_index + 1, file_count);
        bmp_show(file_list[file_index]);

    } else if (strcmp(cmd, "p") == 0) {
        if (file_count == 0) { uart_printf("No BMP files on SD.\r\n"); return; }
        file_index = (file_index - 1 + file_count) % file_count;
        uart_printf("Showing [%d/%d]\r\n", file_index + 1, file_count);
        bmp_show(file_list[file_index]);

    } else if (strcmp(cmd, "s") == 0) {
        show_info();

    } else if (strcmp(cmd, "r") == 0) {
        uart_printf("Reboot to bootloader...\r\n");
        sleep_ms(100);
        reset_usb_boot(0, 0);

    } else if (strcmp(cmd, "?") == 0) {
        uart_printf("\r\nCommands:\r\n");
        uart_printf("  n - next picture\r\n");
        uart_printf("  p - previous picture\r\n");
        uart_printf("  a - auto-play (3s cycle)\r\n");
        uart_printf("  s - show SD info & file list\r\n");
        uart_printf("  r - reboot to bootloader\r\n");
        uart_printf("  ? - this help\r\n\r\n> ");

    } else if (strlen(cmd) > 0) {
        uart_printf("Unknown: '%s'  (? for help)\r\n> ", cmd);
    }
}

/* ========================================================================== */
/*  主函数                                                                     */
/* ========================================================================== */
int main(void)
{
    stdio_init_all();
    uart_init_dev();
    led_init();
    spi1_init();
    lcd_init();

    uart_printf("\r\n========================================\r\n");
    uart_printf(" DNRP2350A Mini Photo Frame\r\n");
    uart_printf("========================================\r\n");

    /* 初始化 SD 卡 */
    uart_printf("Mounting SD card...\r\n");
    FATFS fs;
    FRESULT fr = f_mount(&fs, "0:", 1);
    if (fr != FR_OK) {
        uart_printf("SD mount failed! (%d)\r\n", fr);
        uart_printf("Check: FAT32 formatted? Card inserted?\r\n");
        lcd_show_string(10, 40, 220, 32, 24, "No SD Card!", RED);
    } else {
        uart_printf("SD card OK.\r\n");
        ensure_photo_on_sd();       /* 自动写入嵌入式 BMP */
        scan_bmp_files();

        if (file_count > 0) {
            bmp_show(file_list[0]);
        } else {
            lcd_clear(BLACK);
            lcd_show_string(10, 50, 220, 32, 24, "No BMP files", BLUE);
            lcd_show_string(10, 80, 220, 24, 16, "Put .bmp on FAT32 SD", RED);
        }
    }

    /* 清空 RX 缓冲 (排除启动噪声) */
    while (uart_read_byte() >= 0);
    uart_printf("\r\n> ");

    /* ---- 主循环 ---- */
    char     cmd_buf[16];
    int      cmd_pos = 0;
    bool     autoplay = false;
    uint32_t last_auto = 0;
    LED(0);     /* LED 常亮 */

    while (1) {
        /* 自动播放 */
        if (autoplay && file_count > 0) {
            uint32_t now = to_ms_since_boot(get_absolute_time());
            if (now - last_auto > 3000) {
                last_auto = now;
                file_index = (file_index + 1) % file_count;
                bmp_show(file_list[file_index]);
                uart_printf("[AUTO %d/%d]\r\n> ", file_index + 1, file_count);
            }
        }

        /* 串口命令 */
        int ch = uart_read_byte();
        if (ch >= 0) {
            if (ch == '\r' || ch == '\n') {
                cmd_buf[cmd_pos] = '\0';
                uart_printf("\r\n");

                /* 'a' 切换自动播放 */
                if (strcmp(cmd_buf, "a") == 0) {
                    autoplay = !autoplay;
                    if (autoplay) {
                        uart_printf("Auto-play ON (3s)\r\n");
                        last_auto = to_ms_since_boot(get_absolute_time());
                        lcd_show_string(0, 0, 240, 16, 12, "[AUTO] a=stop",
                                        RED);
                    } else {
                        uart_printf("Auto-play OFF\r\n");
                    }
                } else {
                    process_cmd(cmd_buf);
                }
                cmd_pos = 0;

            } else if (ch == '\b' || ch == 0x7F) {
                if (cmd_pos > 0) { cmd_pos--; uart_printf("\b \b"); }

            } else if (cmd_pos < (int)sizeof(cmd_buf) - 1) {
                cmd_buf[cmd_pos++] = (char)ch;
                uart_send_byte((uint8_t)ch);
            }
        }

        sleep_ms(10);
    }

    return 0;
}
