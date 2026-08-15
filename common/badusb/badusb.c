/**
 * @file    badusb.c
 * @brief   BadUSB shell 命令层 — 终端下 list/inject 脚本 + KEY1 一键注入
 *
 *          命令:
 *            badusb          列出 SD 卡上的 .txt 脚本
 *            badusb demo     写入内置示例 /hello.txt 并注入
 *            badusb <n>      选中第 n 个脚本并注入 (n=1..)
 *            badusb <path>   注入指定脚本
 *          KEY1:              注入当前选中脚本 (默认第一个)
 */

#include "badusb.h"
#include "hid_keyboard.h"
#include "payload.h"
#include "sample_script.h"
#include "shell/shell.h"
#include "console/console.h"
#include "BSP/LCD/lcd.h"
#include "BSP/UART/uart.h"
#include "BSP/KEY/key.h"
#include "tusb.h"
#include "ff.h"
#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include <stdio.h>
#include <string.h>

#define MAX_FILES  32
#define MAX_NAME   48

static char files[MAX_FILES][MAX_NAME];
static int  file_cnt = 0;
static int  selected = 0;
static bool sd_ok = false;

/* 双输出: 串口 + LCD 终端 (灰色避免被输入回显污染) */
static void say(const char *s)
{
    shell_print(s); shell_print("\r\n");
    console_set_color(GRAY, BLACK); console_println(s);
}

/* ---- 扫描 SD 卡 .txt 脚本 ---- */
static void scan_scripts(void)
{
    DIR dir; FILINFO fno;
    file_cnt = 0;
    if (f_opendir(&dir, "/") != FR_OK) return;
    while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0] && file_cnt < MAX_FILES) {
        if (fno.fattrib & AM_DIR) continue;
        char *e = strrchr(fno.fname, '.');
        if (e && strcasecmp(e, ".txt") == 0) {
            snprintf(files[file_cnt], MAX_NAME, "/%.46s", fno.fname);
            file_cnt++;
        }
    }
    f_closedir(&dir);
}

/* ---- 不存在或与内置示例不一致时刷新 /hello.txt ---- */
static void ensure_demo(void)
{
    FIL f;
    bool need = false;
    if (f_open(&f, "/hello.txt", FA_READ) != FR_OK) {
        need = true;
    } else {
        need = (f_size(&f) != SAMPLE_PAYLOAD_SIZE);
        f_close(&f);
    }
    if (need) {
        if (f_open(&f, "/hello.txt", FA_WRITE | FA_CREATE_ALWAYS) == FR_OK) {
            UINT bw;
            f_write(&f, sample_payload, SAMPLE_PAYLOAD_SIZE, &bw);
            f_close(&f);
            say("wrote demo script /hello.txt");
        }
    }
}

/* ---- 读取脚本 → 等待 USB HID → 注入 ---- */
static void do_inject(const char *path)
{
    FIL fil;
    if (f_open(&fil, path, FA_READ) != FR_OK) {
        say("badusb: cannot open script");
        return;
    }
    uint32_t size = f_size(&fil);
    if (size > 16384) {
        say("badusb: script too large (>16KB)");
        f_close(&fil);
        return;
    }
    static uint8_t buf[16384];
    UINT br;
    f_read(&fil, buf, size, &br);
    f_close(&fil);

    char tmp[64];
    snprintf(tmp, sizeof(tmp), "Executing %s (%u bytes)", path, br);
    say(tmp);

    /* 等待 USB 枚举 (最多 5s, 期间喂狗 + 服务 USB 栈) */
    int wait = 0;
    while (!hid_ready() && wait < 5000) {
        hid_task();
        watchdog_update();
        sleep_ms(1);
        wait++;
    }
    if (!hid_ready()) {
        say("ERROR: USB HID not ready (check USB_OTG)");
        return;
    }
    say("USB HID ready, injecting...");
    hid_delay_ms(500);          /* 等目标机识别键盘 (同时服务 USB 栈) */

    payload_execute(buf, br);
    say("Done.");
}

/* ---- badusb 命令 ---- */
static void cmd_badusb(const char *arg)
{
    if (!sd_ok) { say("no SD"); return; }

    if (!arg || !*arg) {
        if (file_cnt == 0) { say("no .txt scripts (try: badusb demo)"); return; }
        char b[64];
        for (int i = 0; i < file_cnt; i++) {
            snprintf(b, sizeof(b), "[%d] %s", i + 1, files[i]);
            say(b);
        }
        return;
    }

    if (strcasecmp(arg, "demo") == 0) {
        ensure_demo();
        scan_scripts();
        do_inject("/hello.txt");
        return;
    }

    /* 数字索引: badusb 1/2/3... */
    if (arg[0] >= '1' && arg[0] <= '9' && arg[1] == '\0') {
        int idx = arg[0] - '0' - 1;
        if (idx < file_cnt) { selected = idx; do_inject(files[idx]); }
        else say("badusb: bad index");
        return;
    }

    /* 路径 (自动补前导 /) */
    char path[MAX_NAME];
    if (arg[0] == '/') snprintf(path, MAX_NAME, "%.47s", arg);
    else               snprintf(path, MAX_NAME, "/%.46s", arg);
    do_inject(path);
}

void badusb_init(bool ok)
{
    sd_ok = ok;
    selected = 0;
    hid_init();     /* tusb_init() — TinyUSB 设备栈初始化 (USB PLL 由 runtime 已配好) */
    key_init();
    if (sd_ok) { ensure_demo(); scan_scripts(); }
}

void badusb_register(void)
{
    shell_register("badusb", "list/inject HID script", cmd_badusb);
}

void badusb_task(void)
{
    hid_task();
    static bool last_key = false;
    bool k = (key_scan(0) == KEY_PRES);
    if (k && !last_key && sd_ok && file_cnt > 0) {
        char b[64];
        snprintf(b, sizeof(b), "KEY1 -> inject [%d] %s", selected + 1, files[selected]);
        say(b);
        do_inject(files[selected]);
    }
    last_key = k;
}
