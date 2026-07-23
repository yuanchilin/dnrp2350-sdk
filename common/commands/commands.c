/**
 * @file    commands.c
 * @brief   标准命令: ls cat view free sysinfo clear snake
 */

#include "commands.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "pico/stdlib.h"
#include "BSP/LCD/lcd.h"
#include "BSP/UART/uart.h"
#include "BSP/SDIO/spi_sdcard.h"
#include "console/console.h"
#include "shell/shell.h"
#include "ff.h"

#define COLS 30
#define ROWS 8

static bool _sd_ok = false;

/* 双输出: LCD + 串口 */
static void echo(const char *s) { shell_print(s); shell_print("\r\n"); console_println(s); }

void commands_init(bool sd_ok) { _sd_ok = sd_ok; }

/* ---- BMP decoder ---- */
typedef struct __attribute__((packed)){uint16_t bfType;uint32_t bfSize;uint16_t bfReserved1,bfReserved2;uint32_t bfOffBits;}bmp_hdr_t;
typedef struct __attribute__((packed)){uint32_t biSize;int32_t biWidth,biHeight;uint16_t biPlanes,biBitCount;uint32_t biCompression,biSizeImage;}bmp_info_t;

static void cmd_ls(const char *arg) {
    (void)arg;
    if (!_sd_ok) { echo("no SD"); return; }
    DIR dir; FILINFO fno;
    if (f_opendir(&dir, "/") != FR_OK) { echo("fail"); return; }
    char l[32];
    while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0]) {
        snprintf(l, 32, "%-20s %6lu", fno.fname, (unsigned long)fno.fsize);
        echo(l);
    }
    f_closedir(&dir);
}

static void cmd_cat(const char *arg) {
    if (!_sd_ok || !arg || !*arg) { echo("usage: cat <file>"); return; }
    FIL fil;
    if (f_open(&fil, arg, FA_READ) != FR_OK) { echo("open fail"); return; }
    char l[32]; int li = 0; uint8_t b[64]; UINT br;
    while (f_read(&fil, b, 64, &br) == FR_OK && br > 0) {
        for (UINT i = 0; i < br; i++) {
            char c = (char)b[i];
            if (c == '\n' || li >= 30) { l[li] = 0; echo(l); li = 0; }
            else if (c >= ' ' && c <= '~') l[li++] = c;
        }
    }
    if (li > 0) { l[li] = 0; echo(l); }
    f_close(&fil);
}

static void cmd_view(const char *arg) {
    if (!_sd_ok || !arg || !*arg) { echo("usage: view <file.bmp>"); return; }
    FIL fil; UINT br;
    if (f_open(&fil, arg, FA_READ) != FR_OK) { echo("open fail"); return; }
    bmp_hdr_t fh; bmp_info_t ih;
    f_read(&fil, &fh, sizeof(fh), &br);
    f_read(&fil, &ih, sizeof(ih), &br);
    if (fh.bfType != 0x4D42 || ih.biBitCount != 24) { f_close(&fil); echo("bad BMP"); return; }
    int iw = ih.biWidth, ihgt = ih.biHeight > 0 ? ih.biHeight : -ih.biHeight;
    int rb = (iw * 3 + 3) & ~3; bool td = ih.biHeight < 0;
    int sx = iw > 240 ? iw * 10 / 240 : 10, sy = ihgt > 135 ? ihgt * 10 / 135 : 10;
    int dw = iw > 240 ? 240 : iw, dh = ihgt > 135 ? 135 : ihgt;
    int ox = (240 - dw) / 2, oy = (135 - dh) / 2;
    uint8_t line[240 * 3 + 4];
    f_lseek(&fil, fh.bfOffBits);
    lcd_set_window(ox, oy, ox + dw - 1, oy + dh - 1);
    lcd_write_cmd(0x2C);
    for (int ly = 0; ly < dh; ly++) {
        int by = td ? ly * sy / 10 : ihgt - 1 - ly * sy / 10;
        f_lseek(&fil, fh.bfOffBits + (FSIZE_t)by * rb);
        f_read(&fil, line, rb, &br);
        for (int lx = 0; lx < dw; lx++) {
            uint8_t *px = &line[lx * sx / 10 * 3];
            lcd_write_data16(((px[2] >> 3) << 11) | ((px[1] >> 2) << 5) | (px[0] >> 3));
        }
    }
    f_close(&fil);
    while (uart_read_byte() >= 0);
    while (uart_read_byte() < 0) sleep_ms(50);
    while (uart_read_byte() >= 0);
    console_draw();
}

static void cmd_free(const char *arg) {
    (void)arg;
    if (!_sd_ok) { echo("no SD"); return; }
    uint32_t fk, tk; sd_init(&fk, &tk);
    char s[32]; snprintf(s, 32, "Free: %luMB  Total: %luMB", fk >> 10, tk >> 10);
    echo(s);
}

static void cmd_sysinfo(const char *arg) {
    (void)arg;
    echo("RP2350A | Cortex-M33 | 150MHz");
    echo("LCD: 240x135 ST7789 | SPI1");
    echo(_sd_ok ? "SD: OK" : "SD: N/A");
}

static void cmd_clear(const char *arg) {
    (void)arg;
    console_clear();
}

static void cmd_snake(const char *arg) {
    (void)arg;
    echo("Snake! WASD, Q=quit");
    int sx[256], sy[256], len = 3, dx = 1, dy = 0, fx = 10, fy = 3;
    sx[0] = 5; sy[0] = 3; sx[1] = 4; sy[1] = 3; sx[2] = 3; sy[2] = 3;
    lcd_fill(0, 0, 239, 120, BLACK);
    while (1) {
        int ch = uart_read_byte();
        if (ch >= 0) {
            switch (toupper(ch)) {
                case 'W': if (dy != 1) { dx = 0; dy = -1; } break;
                case 'S': if (dy != -1) { dx = 0; dy = 1; } break;
                case 'A': if (dx != 1) { dx = -1; dy = 0; } break;
                case 'D': if (dx != -1) { dx = 1; dy = 0; } break;
                case 'Q': echo("QUIT"); return;
            }
            while (uart_read_byte() >= 0);
        }
        int nx = sx[0] + dx, ny = sy[0] + dy;
        if (nx < 0 || nx >= COLS || ny < 0 || ny >= ROWS) { echo("GAME OVER"); return; }
        for (int i = 0; i < len; i++)
            if (sx[i] == nx && sy[i] == ny) {
                char s[32]; snprintf(s, 32, "Score: %d", len - 3); echo(s); return;
            }
        for (int i = len; i > 0; i--) { sx[i] = sx[i - 1]; sy[i] = sy[i - 1]; }
        sx[0] = nx; sy[0] = ny;
        if (nx == fx && ny == fy) { len++; fx = rand() % COLS; fy = rand() % ROWS; }
        lcd_fill(0, 0, 239, 120, BLACK);
        for (int i = 0; i < len; i++)
            lcd_fill(sx[i] * 8, sy[i] * 16, sx[i] * 8 + 6, sy[i] * 16 + 14, GREEN);
        lcd_fill(fx * 8, fy * 16, fx * 8 + 6, fy * 16 + 14, RED);
        sleep_ms(150);
    }
}

void commands_view_file(const char *path)
{
    cmd_view(path);
    lcd_fill(0, 0, 239, 134, BLACK);  /* 全屏清黑 */
    console_draw();  /* 恢复终端画面 */
}

void commands_register_all(void)
{
    shell_register("ls",      "list files",       cmd_ls);
    shell_register("cat",     "show file",        cmd_cat);
    shell_register("view",    "show BMP image",   cmd_view);
    shell_register("free",    "SD free space",    cmd_free);
    shell_register("sysinfo", "system info",      cmd_sysinfo);
    shell_register("clear",   "clear screen",     cmd_clear);
    shell_register("snake",   "play snake",       cmd_snake);
}
