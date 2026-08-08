/**
 * @file    main.c
 * @brief   DNRP2350A 迷你相框 — TF卡 BMP → ST7789 LCD 显示
 * @note    串口命令: next=下一张 prev=上一张 auto=自动播放 info=文件列表 reboot=重启bootloader
 *
 *          BMP 解码: 24-bit 格式, BGR→RGB565, 最近邻缩放, 居中
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "BSP/LCD/lcd.h"
#include "BSP/LED/led.h"
#include "BSP/SPI/spi.h"
#include "BSP/SDIO/spi_sdcard.h"
#include "BSP/UART/uart.h"
#include "shell/shell.h"
#include "commands/bmp.h"
#include "ff.h"
#include "photo_data.h"     /* 嵌入式 BMP 数据, 上电自动写入 TF 卡 */

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
/*  自动播放开关                                                               */
/* ========================================================================== */
static bool     autoplay = false;
static uint32_t last_auto = 0;

/* ========================================================================== */
/*  Shell 命令回调: 图片上一张/下一张/信息/自动播放/重启                        */
/* ========================================================================== */
static void cmd_next(const char *arg)
{
    if (file_count == 0) { shell_printf("No BMP files on SD.\r\n"); return; }
    file_index = (file_index + 1) % file_count;
    shell_printf("Showing [%d/%d]\r\n", file_index + 1, file_count);
    bmp_show(file_list[file_index]);
}

static void cmd_prev(const char *arg)
{
    if (file_count == 0) { shell_printf("No BMP files on SD.\r\n"); return; }
    file_index = (file_index - 1 + file_count) % file_count;
    shell_printf("Showing [%d/%d]\r\n", file_index + 1, file_count);
    bmp_show(file_list[file_index]);
}

static void cmd_info(const char *arg)
{
    show_info();
}

static void cmd_auto(const char *arg)
{
    autoplay = !autoplay;
    if (autoplay) {
        last_auto = to_ms_since_boot(get_absolute_time());
        shell_printf("Auto-play ON (3s cycle)\r\n");
        lcd_show_string(0, 0, 240, 16, 12, "[AUTO] auto=stop", RED);
    } else {
        shell_printf("Auto-play OFF\r\n");
    }
}

static void cmd_reboot(const char *arg)
{
    shell_reboot();
}

/* 自动播放定时检查 — 在主循环调用 */
static void autoplay_tick(void)
{
    if (!autoplay || file_count == 0) return;
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (now - last_auto > 3000) {
        last_auto = now;
        file_index = (file_index + 1) % file_count;
        bmp_show(file_list[file_index]);
        shell_printf("[AUTO %d/%d]\r\n", file_index + 1, file_count);
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

    /* 注册 shell 命令 */
    shell_init("> ");
    shell_register("next",  "next  - show next picture", cmd_next);
    shell_register("prev",  "prev  - show previous picture", cmd_prev);
    shell_register("info",  "info  - show SD info & file list", cmd_info);
    shell_register("auto",  "auto  - toggle auto-play (3s)", cmd_auto);
    shell_register("reboot", "reboot - reboot to bootloader", cmd_reboot);

    LED(0);     /* LED 常亮 */

    /* ---- 主循环: shell + autoplay 定时 ---- */
    while (1) {
        shell_poll();
        autoplay_tick();
        sleep_ms(10);
    }

    return 0;
}
