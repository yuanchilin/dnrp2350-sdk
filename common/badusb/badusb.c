/**
 * @file    badusb.c
 * @brief   BadUSB shell 命令层 — 终端下 list/inject 脚本 + KEY1 一键注入
 *          注入核心 (扫描/写内置文件/执行) 在 badusb_core.c, 与 05 共用。
 *
 *          命令:
 *            badusb          列出 SD 卡上的 .txt 脚本
 *            badusb demo     写入内置示例 /hello.txt 并注入
 *            badusb <n>      选中第 n 个脚本并注入 (n=1..)
 *            badusb <path>   注入指定脚本
 *          KEY1:              注入当前选中脚本 (默认第一个)
 */

#include "badusb.h"
#include "badusb_core.h"
#include "hid_keyboard.h"
#include "sample_script.h"
#include "shell/shell.h"
#include "console/console.h"
#include "BSP/LCD/lcd.h"
#include "BSP/UART/uart.h"
#include "BSP/KEY/key.h"
#include <stdio.h>
#include <string.h>

#define MAX_FILES 32

static char files[MAX_FILES][BADUSB_NAME_MAX];
static int  file_cnt = 0;
static int  selected = 0;
static bool sd_ok = false;

/* 双输出: 串口 + LCD 终端 (灰色避免被输入回显污染) */
static void say(const char *s)
{
    shell_print(s); shell_print("\r\n");
    console_set_color(GRAY, BLACK); console_println(s);
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
        if (!badusb_ensure_file("/hello.txt", sample_payload, SAMPLE_PAYLOAD_SIZE))
            say("badusb: write demo script failed");
        file_cnt = badusb_scan_ext(".txt", files, MAX_FILES);
        badusb_inject_file("/hello.txt");
        return;
    }

    /* 数字索引: badusb 1/2/3... */
    if (arg[0] >= '1' && arg[0] <= '9' && arg[1] == '\0') {
        int idx = arg[0] - '0' - 1;
        if (idx < file_cnt) { selected = idx; badusb_inject_file(files[idx]); }
        else say("badusb: bad index");
        return;
    }

    /* 路径 (自动补前导 /) */
    char path[BADUSB_NAME_MAX];
    if (arg[0] == '/') snprintf(path, BADUSB_NAME_MAX, "%.47s", arg);
    else               snprintf(path, BADUSB_NAME_MAX, "/%.46s", arg);
    badusb_inject_file(path);
}

void badusb_init(bool ok)
{
    sd_ok = ok;
    selected = 0;
    hid_init();     /* tusb_init() — TinyUSB 设备栈初始化 (USB PLL 由 runtime 已配好) */
    key_init();
    badusb_set_log(say);            /* 注入日志同时镜像到 LCD 终端 */
    if (sd_ok) {
        if (!badusb_ensure_file("/hello.txt", sample_payload, SAMPLE_PAYLOAD_SIZE))
            say("badusb: write demo script failed");
        file_cnt = badusb_scan_ext(".txt", files, MAX_FILES);
    }
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
        badusb_inject_file(files[selected]);
    }
    last_key = k;
}
