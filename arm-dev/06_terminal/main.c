/**
 * @file    main.c
 * @brief   DNRP2350A 迷你终端 — LCD 显示器 + 串口键盘
 *
 *          8x16 字体, 30列×8行 + 状态栏
 *          命令: help ls cat free sysinfo clear snake reboot
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "BSP/LCD/lcd.h"
#include "BSP/LED/led.h"
#include "BSP/SPI/spi.h"
#include "BSP/UART/uart.h"
#include "BSP/SDIO/spi_sdcard.h"
#include "ff.h"

/* ========================================================================== */
/*  终端常量 (16号字体: 8x16, 30列×8行)                                      */
/* ========================================================================== */
#define COLS        30          /* 240/8=30 列 */
#define ROWS        8           /* 135/16=8 行 */
#define SCROLLBACK  64
#define FONT_W      8
#define FONT_H      16
#define FONT_SIZE   16
#define STATUSBAR_Y (ROWS * FONT_H)  /* 128 */

/* ========================================================================== */
/*  终端缓冲区                                                                 */
/* ========================================================================== */
static char screen[ROWS][COLS + 1];         /* 当前显示 */
static char scroll[SCROLLBACK][COLS + 1];   /* 回滚缓冲 */
static int  scroll_idx = 0;
static int  cursor_x  = 0;
static int  cursor_y  = 0;
static bool sd_mounted = false;

/* 官方 lcd_show_string mode=0 会涂白底, 自己写透明版 */
static void term_puts(int x, int y, const char *s, uint8_t size, uint16_t color)
{
    while (*s) {
        if (*s != ' ')  /* 空格跳过, 保留底色 */
            lcd_show_char(x, y, *s, size, 1, color);
        x += (size / 2);        /* 16号字体: 8px宽 */
        s++;
    }
}

/* ========================================================================== */
/*  局部刷新单行 (消除闪烁)                                                    */
static void term_draw_line(int y)
{
    lcd_fill(0, y * FONT_H, 239, (y + 1) * FONT_H - 1, BLACK);
    term_puts(0, y * FONT_H, screen[y], FONT_SIZE, GREEN);
}

/*  全屏刷新 (仅滚屏时用)                                                      */
static void term_draw(void)
{
    lcd_fill(0, 0, 239, STATUSBAR_Y - 1, BLACK);
    for (int y = 0; y < ROWS; y++) {
        term_puts(0, y * FONT_H, screen[y], FONT_SIZE, GREEN);
    }
    /* 状态栏 */
    lcd_fill(0, STATUSBAR_Y, 239, 134, 0x1082);
    char bar[32];
    snprintf(bar, 32, "SD:%s ?=help", sd_mounted ? "OK" : "NO");
    lcd_show_string(2, STATUSBAR_Y + 2, 100, 12, 12, bar, WHITE);
}

/*  光标                                                                       */
static void term_draw_cursor(void)
{
    lcd_show_char(cursor_x * FONT_W, cursor_y * FONT_H, '_', FONT_SIZE, 1, GREEN);
}

/* ========================================================================== */
/*  终端滚动                                                                   */
/* ========================================================================== */
static void term_scroll(void)
{
    /* 存入回滚 */
    strncpy(scroll[scroll_idx % SCROLLBACK], screen[0], COLS);
    scroll_idx++;
    /* 缓冲区上滚 */
    for (int i = 0; i < ROWS - 1; i++) {
        strncpy(screen[i], screen[i + 1], COLS);
    }
    memset(screen[ROWS - 1], ' ', COLS);
    screen[ROWS - 1][COLS] = '\0';

    /* LCD 逐行上移 (避免全屏刷新闪烁) */
    for (int y = 0; y < ROWS; y++) {
        term_draw_line(y);
    }
}

static void term_putc(char c)
{
    if (c == '\n') {
        screen[cursor_y][cursor_x] = '\0';
        cursor_x = 0;
        cursor_y++;
        if (cursor_y >= ROWS) {
            term_scroll();
            cursor_y = ROWS - 1;
        }
    } else if (c == '\r') {
        cursor_x = 0;
    } else if (c == '\b') {
        if (cursor_x > 0) cursor_x--;
    } else if (c >= ' ') {
        if (cursor_x >= COLS) {
            cursor_x = 0;
            cursor_y++;
            if (cursor_y >= ROWS) {
                term_scroll();
                cursor_y = ROWS - 1;
            }
        }
        screen[cursor_y][cursor_x++] = c;
    }
}

static void term_print(const char *s)
{
    while (*s) term_putc(*s++);
    term_draw();
}

static void term_println(const char *s)
{
    term_print(s);
    term_putc('\n');
    term_draw();
}

/* ========================================================================== */
/*  命令实现                                                                   */
/* ========================================================================== */
static void cmd_help(void)
{
    term_println("=== COMMANDS ===");
    term_println("help     - this");
    term_println("clear    - clear screen");
    term_println("ls       - list SD files");
    term_println("cat <f>  - show file");
    term_println("view <f> - show BMP image");
    term_println("free     - SD free space");
    term_println("sysinfo  - system info");
    term_println("snake    - play snake");
    term_println("reboot   - bootloader");
}

static void cmd_clear(void)
{
    for (int i = 0; i < ROWS; i++) memset(screen[i], ' ', COLS);
    cursor_x = cursor_y = 0;
    term_draw();
}

static void cmd_ls(void)
{
    if (!sd_mounted) { term_println("SD not mounted"); return; }
    DIR dir; FILINFO fno;
    if (f_opendir(&dir, "/") != FR_OK) { term_println("opendir fail"); return; }
    char line[COLS + 1];
    while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0]) {
        bool is_dir = fno.fattrib & AM_DIR;
        snprintf(line, COLS + 1, "%c %-20s %6lu",
                 is_dir ? 'D' : 'F', fno.fname, (unsigned long)fno.fsize);
        line[COLS] = '\0';
        term_println(line);
    }
    f_closedir(&dir);
}

static void cmd_cat(const char *path)
{
    if (!sd_mounted) { term_println("SD not mounted"); return; }
    FIL fil;
    if (f_open(&fil, path, FA_READ) != FR_OK) {
        term_println("open fail"); return;
    }
    char line[COLS + 1];
    int li = 0;
    uint8_t buf[64]; UINT br;
    while (f_read(&fil, buf, sizeof(buf), &br) == FR_OK && br > 0) {
        for (UINT i = 0; i < br; i++) {
            char c = (char)buf[i];
            if (c == '\n' || li >= COLS) {
                line[li] = '\0';
                term_println(line);
                li = 0;
            } else if (c >= ' ' && c <= '~') {
                line[li++] = c;
            }
        }
    }
    if (li > 0) { line[li] = '\0'; term_println(line); }
    f_close(&fil);
}

/* ---- 图片查看 ---- */
typedef struct __attribute__((packed)) { uint16_t bfType; uint32_t bfSize; uint16_t bfReserved1, bfReserved2; uint32_t bfOffBits; } bmp_hdr_t;
typedef struct __attribute__((packed)) { uint32_t biSize; int32_t biWidth, biHeight; uint16_t biPlanes, biBitCount; uint32_t biCompression, biSizeImage; int32_t biXPelsPerMeter, biYPelsPerMeter; uint32_t biClrUsed, biClrImportant; } bmp_info_t;
static uint8_t bmp_line[240*3+4];

static void cmd_view(const char *path)
{
    if (!sd_mounted || !path || !*path) { term_println("usage: view <file.bmp>"); return; }
    FIL fil; UINT br;
    if (f_open(&fil, path, FA_READ) != FR_OK) { term_println("open fail"); return; }
    bmp_hdr_t fh; bmp_info_t ih;
    f_read(&fil, &fh, sizeof(fh), &br);
    f_read(&fil, &ih, sizeof(ih), &br);
    if (fh.bfType != 0x4D42 || ih.biBitCount != 24 || ih.biCompression != 0)
    { term_println("bad BMP"); f_close(&fil); return; }
    int iw = ih.biWidth, ihgt = (ih.biHeight > 0) ? ih.biHeight : -ih.biHeight;
    int rb = (iw * 3 + 3) & ~3;
    int sx = (iw > 240) ? (iw * 10 / 240) : 10;
    int sy = (ihgt > 135) ? (ihgt * 10 / 135) : 10;
    int dw = (iw > 240) ? 240 : iw, dh = (ihgt > 135) ? 135 : ihgt;
    int ox = (240 - dw) / 2, oy = (135 - dh) / 2;
    bool td = (ih.biHeight < 0);
    lcd_set_window(ox, oy, ox + dw - 1, oy + dh - 1);
    lcd_write_cmd(0x2C);
    f_lseek(&fil, fh.bfOffBits);
    for (int ly = 0; ly < dh; ly++) {
        int by = td ? (ly * sy / 10) : (ihgt - 1 - ly * sy / 10);
        f_lseek(&fil, fh.bfOffBits + (FSIZE_t)by * rb);
        f_read(&fil, bmp_line, rb, &br);
        for (int lx = 0; lx < dw; lx++) {
            uint8_t *p = &bmp_line[(lx * sx / 10) * 3];
            lcd_write_data16(((p[2]>>3)<<11) | ((p[1]>>2)<<5) | (p[0]>>3));
        }
    }
    f_close(&fil);
    /* 清 UART 缓冲 */
    while (uart_read_byte() >= 0);
    /* 等按键 */
    while (uart_read_byte() < 0) sleep_ms(50);
    while (uart_read_byte() >= 0);
    /* 全屏刷新恢复终端 */
    lcd_fill(0, 0, 239, STATUSBAR_Y - 1, BLACK);
    for (int y = 0; y < ROWS; y++) term_draw_line(y);
    term_draw_cursor();
}

static void cmd_free(void)
{
    if (!sd_mounted) { term_println("SD not mounted"); return; }
    uint32_t free_kb, total_kb;
    sd_init(&free_kb, &total_kb);
    char line[COLS + 1];
    snprintf(line, COLS + 1, "Free: %luMB  Total: %luMB", free_kb >> 10, total_kb >> 10);
    term_println(line);
}

static void cmd_sysinfo(void)
{
    char line[COLS + 1];
    snprintf(line, COLS, "RP2350A | Cortex-M33 | 150MHz"); term_println(line);
    snprintf(line, COLS, "SRAM: 520KB | Flash: 8MB"); term_println(line);
    snprintf(line, COLS, "LCD: 240x135 ST7789 SPI1"); term_println(line);
    snprintf(line, COLS, "UART: 115200 8N1 CH343"); term_println(line);
    snprintf(line, COLS, "SD: %s", sd_mounted ? "OK" : "N/A"); term_println(line);
}

/* ---- 贪吃蛇 ---- */
static void cmd_snake(void)
{
    term_println("SNAKE! Use W/A/S/D, Q=quit");
    term_draw();

    int sx[256], sy[256], len = 3, dx = 1, dy = 0;
    int fx = 10, fy = 3;  /* 食物位置 */
    sx[0]=5; sy[0]=3; sx[1]=4; sy[1]=3; sx[2]=3; sy[2]=3;

    /* 擦除区域 */
    lcd_fill(0, 0, 239, STATUSBAR_Y - 1, BLACK);

    while (1) {
        /* 输入 */
        int ch = uart_read_byte();
        if (ch >= 0) {
            switch (toupper(ch)) {
                case 'W': if (dy != 1)  { dx=0; dy=-1; } break;
                case 'S': if (dy != -1) { dx=0; dy=1;  } break;
                case 'A': if (dx != 1)  { dx=-1; dy=0; } break;
                case 'D': if (dx != -1) { dx=1; dy=0;  } break;
                case 'Q': term_println("QUIT"); return;
            }
            while (uart_read_byte() >= 0); /* 清缓冲 */
        }

        /* 移动 */
        int nx = sx[0] + dx, ny = sy[0] + dy;
        if (nx < 0 || nx >= COLS || ny < 0 || ny >= ROWS) {
            term_println("GAME OVER"); return;
        }

        /* 吃到自己? */
        for (int i = 0; i < len; i++) {
            if (sx[i] == nx && sy[i] == ny) {
                char s[COLS+1];
                snprintf(s, COLS, "GAME OVER! Score: %d", len - 3);
                term_println(s);
                return;
            }
        }

        /* 移动蛇身 */
        for (int i = len; i > 0; i--) { sx[i] = sx[i-1]; sy[i] = sy[i-1]; }
        sx[0] = nx; sy[0] = ny;

        /* 吃到食物? */
        if (nx == fx && ny == fy) {
            len++;
            fx = rand() % COLS; fy = rand() % ROWS;
        }

        /* 绘制 */
        lcd_fill(0, 0, 239, STATUSBAR_Y - 1, BLACK);
        for (int i = 0; i < len; i++) {
            lcd_fill(sx[i]*FONT_W, sy[i]*FONT_H, sx[i]*FONT_W+10, sy[i]*FONT_H+22, GREEN);
        }
        lcd_fill(fx*FONT_W, fy*FONT_H, fx*FONT_W+10, fy*FONT_H+22, RED);

        sleep_ms(150);
    }
}

/* ========================================================================== */
/*  主函数                                                                     */
/* ========================================================================== */
int main(void)
{
    stdio_init_all();
    uart_init_dev();
    sleep_ms(100);
    while (uart_read_byte() >= 0);
    led_init();
    spi1_init();
    lcd_init();

    /* 挂载 SD */
    FATFS fs;
    sd_mounted = (f_mount(&fs, "0:", 1) == FR_OK);

    /* 开机自动展示照片 */
    if (sd_mounted) {
        FIL test;
        if (f_open(&test, "/photo.bmp", FA_READ) == FR_OK) {
            f_close(&test);
            cmd_view("/photo.bmp");
        }
    }

    /* 终端初始化 */
    for (int i = 0; i < ROWS; i++) memset(screen[i], ' ', COLS);

    uart_printf("\r\nTERMINAL READY\r\n");

    term_println("DNRP2350A Mini Terminal");
    term_println("Type 'help' for commands.");
    term_draw();

    /* 命令缓冲 */
    char cmd[128]; int pos = 0;

    while (1) {
        int ch = uart_read_byte();
        if (ch < 0) { sleep_ms(10); continue; }

        if (ch == '\r' || ch == '\n') {
            cmd[pos] = '\0';
            /* 清除光标 */
            lcd_fill(cursor_x * FONT_W, cursor_y * FONT_H,
                     cursor_x * FONT_W + FONT_W - 1, cursor_y * FONT_H + FONT_H - 1, BLACK);
            term_putc('\n');
            term_draw_line(cursor_y > 0 ? cursor_y - 1 : 0);
            term_draw_line(cursor_y);

            /* 解析命令 */
            char *arg = NULL;
            for (char *p = cmd; *p; p++) {
                if (*p == ' ') { *p = '\0'; arg = p + 1; break; }
            }

            if (strlen(cmd) == 0) {
                /* 空行 */
            } else if (strcmp(cmd, "help") == 0) {
                cmd_help();
            } else if (strcmp(cmd, "clear") == 0) {
                cmd_clear();
            } else if (strcmp(cmd, "ls") == 0) {
                cmd_ls();
            } else if (strcmp(cmd, "cat") == 0) {
                cmd_cat(arg ? arg : "");
            } else if (strcmp(cmd, "view") == 0) {
                cmd_view(arg ? arg : "");
            } else if (strcmp(cmd, "free") == 0) {
                cmd_free();
            } else if (strcmp(cmd, "sysinfo") == 0) {
                cmd_sysinfo();
            } else if (strcmp(cmd, "snake") == 0) {
                cmd_snake();
                cmd_clear();
            } else if (strcmp(cmd, "reboot") == 0) {
                term_println("Rebooting...");
                sleep_ms(100);
                reset_usb_boot(0, 0);
            } else {
                term_println("? type 'help'");
            }

            uart_printf("\r\n> ");
            pos = 0;
        } else if (ch == '\b' || ch == 0x7F) {
            if (pos > 0) {
                pos--;
                /* 擦除 LCD 上的字符 */
                term_putc('\b');
                screen[cursor_y][cursor_x] = ' ';
                term_draw_line(cursor_y);
                uart_printf("\b \b");
            }
        } else if (ch == '\t') {
            /* TAB 补全 */
            if (pos > 0 && sd_mounted) {
                cmd[pos] = '\0';
                DIR dir; FILINFO fno;
                if (f_opendir(&dir, "/") == FR_OK) {
                    char match[64] = ""; int matches = 0;
                    while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0]) {
                        if (strncmp(fno.fname, cmd, strlen(cmd)) == 0) {
                            if (matches == 0) strncpy(match, fno.fname, 63);
                            matches++;
                        }
                    }
                    f_closedir(&dir);
                    if (matches == 1) {
                        /* 唯一匹配 — 补全 */
                        for (int i = pos; match[i]; i++) {
                            if (pos >= 127) break;
                            cmd[pos++] = match[i];
                            term_putc(match[i]);
                            uart_send_byte((uint8_t)match[i]);
                        }
                        term_draw_line(cursor_y);
                    } else if (matches > 1) {
                        uart_printf("\r\n%d matches:\r\n", matches);
                        /* 列表不刷屏, 只串口输出 */
                    }
                }
            }
        } else if (ch >= ' ' && pos < 127) {
            cmd[pos++] = (char)ch;
            term_putc((char)ch);
            term_draw_line(cursor_y);
            term_draw_cursor();
            uart_send_byte((uint8_t)ch);
        }
    }
    return 0;
}
