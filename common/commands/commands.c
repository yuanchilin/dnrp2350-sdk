/**
 * @file    commands.c
 * @brief   标准命令: ls cat view free sysinfo clear uptime echo setcolor snake
 */

#include "commands.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include "BSP/LCD/lcd.h"
#include "BSP/UART/uart.h"
#include "BSP/SDIO/spi_sdcard.h"
#include "console/console.h"
#include "shell/shell.h"
#include "commands/bmp.h"
#include "ff.h"

#define COLS 30
#define ROWS 8

static bool _sd_ok = false;

/* 双输出: LCD + 串口; 命令输出一律默认灰 (避免被输入回显的绿色污染) */
static void echo(const char *s) { shell_print(s); shell_print("\r\n"); console_set_color(GRAY, BLACK); console_println(s); }

/* 大小写无关的扩展名比较 */
static bool ext_eq(const char *ext, const char *s) {
    for (; *s; ext++, s++) {
        char a = *ext, b = *s;
        if (a >= 'A' && a <= 'Z') a += 32;
        if (b >= 'A' && b <= 'Z') b += 32;
        if (a != b) return false;
    }
    return *ext == '\0';
}

/* 按类型取色: 目录蓝 / 图片品红 / 文本黄 / 可执行亮绿 / 其他普通 */
static const char *ls_color(const char *name, bool is_dir) {
    if (is_dir) return CLR_DIR;
    const char *ext = strrchr(name, '.');
    if (ext) {
        ext++;  /* 跳过 '.' */
        if (ext_eq(ext, "bmp") || ext_eq(ext, "jpg") || ext_eq(ext, "jpeg") ||
            ext_eq(ext, "png") || ext_eq(ext, "gif")) return CLR_IMG;
        if (ext_eq(ext, "txt") || ext_eq(ext, "log") ||
            ext_eq(ext, "c") || ext_eq(ext, "h")) return CLR_TXT;
        if (ext_eq(ext, "elf") || ext_eq(ext, "bin") ||
            ext_eq(ext, "uf2") || ext_eq(ext, "exe")) return CLR_EXE;
    }
    return CLR_FILE;
}

/* ls 行: 串口与 LCD 共用同一份 ANSI 串 (LCD 由 console_write_ansi 解析着色); 目录加 '/' 后缀 */
static void echo_ls(const char *name, unsigned long size, bool is_dir) {
    char tmp[64];
    snprintf(tmp, sizeof(tmp), "%s%-20s%s%s %6lu", ls_color(name, is_dir), name, is_dir ? "/" : "", ANSI_RESET, size);
    shell_print(tmp); shell_print("\r\n");
    console_write_ansi(tmp); console_putc('\n');
}

void commands_init(bool sd_ok) { _sd_ok = sd_ok; }

static void cmd_ls(const char *arg) {
    (void)arg;
    if (!_sd_ok) { echo("no SD"); return; }
    DIR dir; FILINFO fno;
    if (f_opendir(&dir, "/") != FR_OK) { echo("fail"); return; }
    while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0]) {
        echo_ls(fno.fname, (unsigned long)fno.fsize, (fno.fattrib & AM_DIR) != 0);
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
    if (!bmp_show(arg)) { echo("open/bad BMP"); return; }
    /* 清残留输入后等一个按键返回; 超时 15s 自动跳过, 避免开机/复位后终端一直卡住 */
    while (uart_read_byte() >= 0);
    uint32_t t0 = to_ms_since_boot(get_absolute_time());
    while (uart_read_byte() < 0) {
        watchdog_update();          /* 喂狗: 该等待最长 15s, 超过看门狗 5s 需喂, 否则复位死循环 */
        if (to_ms_since_boot(get_absolute_time()) - t0 > 15000) break;
        sleep_ms(50);
    }
    while (uart_read_byte() >= 0);
    console_draw();
}

static void cmd_free(const char *arg) {
    (void)arg;
    if (!_sd_ok) { echo("no SD"); return; }
    /* 用只读剩余空间 API: 不 mount/unmount, 避免卸载 main 已挂载的卷导致后续命令失败 */
    uint32_t fk, tk; sd_free_kb(&fk, &tk);
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

static void cmd_uptime(const char *arg) {
    (void)arg;
    uint32_t ms = to_ms_since_boot(get_absolute_time());
    char s[32]; snprintf(s, 32, "Uptime: %lus", ms / 1000);
    echo(s);
}

static void cmd_echo(const char *arg) {
    if (!arg) { echo(""); return; }
    echo(arg);
}

static void cmd_setcolor(const char *arg) {
    /* 用法: setcolor <fg> <bg>  (0-65535 RGB565) */
    if (!arg || !*arg) { echo("usage: setcolor <fg> <bg> (RGB565)"); return; }
    char *end;
    long fg = strtol(arg, &end, 0);
    if (*end != ' ') { echo("usage: setcolor <fg> <bg>"); return; }
    long bg = strtol(end + 1, NULL, 0);
    console_set_color((uint16_t)fg, (uint16_t)bg);
    console_draw();
    echo("color set");
}

static void cmd_snake(const char *arg) {
    (void)arg;
    srand((unsigned)(to_ms_since_boot(get_absolute_time()) & 0xFFFF) ^ (unsigned)(uintptr_t)arg);
    echo("Snake! WASD, Q=quit");
    int sx[256], sy[256], len = 3, dx = 1, dy = 0, fx = 10, fy = 3;
    sx[0] = 5; sy[0] = 3; sx[1] = 4; sy[1] = 3; sx[2] = 3; sy[2] = 3;
    lcd_fill(0, 0, 239, 120, BLACK);
    while (1) {
        watchdog_update();          /* 喂狗: 贪吃蛇是长循环, 否则 5s 后看门狗复位 */
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

/* ---- SD 卡文件名补全 (参数补全回调) ----
 * 返回 0=无候选  1=唯一候选(填充 out)  >1=多候选(已列出并重绘) */
static int _file_complete(const char *tok, char *out, int outsz)
{
    if (!_sd_ok) return 0;
    int tlen = (int)strlen(tok);
    DIR dir; FILINFO fno;
    if (f_opendir(&dir, "/") != FR_OK) return 0;

    /* 第一遍: 统计+收集匹配*/
    char hits[16][40];
    int n = 0;
    while (n < 16 && f_readdir(&dir, &fno) == FR_OK && fno.fname[0]) {
        if (tlen == 0 || strncmp(fno.fname, tok, tlen) == 0) {
            snprintf(hits[n], 40, "%s", fno.fname);
            n++;
        }
    }
    f_closedir(&dir);
    if (n == 0) return 0;

    if (n == 1) {
        snprintf(out, outsz, "%s", hits[0]);
        return 1;
    }

    /* 多候选 → 列出候选名 (重绘由 shell 侧完成) */
    shell_print("\r\n");
    char b[48];
    for (int i = 0; i < n; i++) {
        snprintf(b, sizeof(b), "  %s", hits[i]);
        echo(b);
    }
    return n;
}

/* ---- Good: 自我鼓励 ---- */
static void cmd_good(const char *arg)
{
    (void)arg;
    echo("Good! Keep going -- you're doing great!");
    echo("Believe in yourself. One step at a time.");
}

void commands_register_all(void)
{
    shell_register("ls",      "list files",       cmd_ls);
    shell_register("cat",     "show file",        cmd_cat);
    shell_register("view",    "show BMP image",   cmd_view);
    shell_register("free",    "SD free space",    cmd_free);
    shell_register("sysinfo", "system info",      cmd_sysinfo);
    shell_register("clear",   "clear screen",     cmd_clear);
    shell_register("uptime",  "system uptime",    cmd_uptime);
    shell_register("echo",    "print text",       cmd_echo);
    shell_register("setcolor","set terminal color", cmd_setcolor);
    shell_register("snake",   "play snake",       cmd_snake);
    shell_register("good",    "a word of praise", cmd_good);
    shell_set_arg_completer(_file_complete);
}
