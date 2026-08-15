/**
 * @file    badusb_core.c
 * @brief   BadUSB 注入核心实现 — 05 与 08 共用
 */

#include "badusb_core.h"
#include "hid_keyboard.h"
#include "payload.h"
#include "BSP/UART/uart.h"
#include "ff.h"
#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#define INJECT_MAX_SIZE 16384

static badusb_log_fn _log = NULL;

void badusb_set_log(badusb_log_fn fn) { _log = fn; }

/* 日志: 默认走 UART, 可被回调接管 (如同时镜像到 LCD 终端) */
static void blog(const char *fmt, ...)
{
    char b[128];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof(b), fmt, ap);
    va_end(ap);
    if (_log) _log(b);
    else      uart_printf("%s\r\n", b);
}

int badusb_scan_ext(const char *ext, char list[][BADUSB_NAME_MAX], int max)
{
    int n = 0;
    DIR dir; FILINFO fno;
    if (f_opendir(&dir, "/") != FR_OK) return 0;
    while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0] && n < max) {
        if (fno.fattrib & AM_DIR) continue;
        char *e = strrchr(fno.fname, '.');
        if (e && strcasecmp(e, ext) == 0) {
            snprintf(list[n], BADUSB_NAME_MAX, "/%.46s", fno.fname);
            n++;
        }
    }
    f_closedir(&dir);
    return n;
}

bool badusb_ensure_file(const char *path, const uint8_t *data, uint32_t size)
{
    FIL f;
    bool need = false;
    if (f_open(&f, path, FA_READ) != FR_OK) {
        need = true;
    } else {
        need = (f_size(&f) != size);
        f_close(&f);
    }
    if (!need) return true;                     /* 已存在且大小一致 */

    if (f_open(&f, path, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) return false;
    UINT bw;
    f_write(&f, data, size, &bw);
    f_close(&f);
    return bw == size;
}

bool badusb_inject_file(const char *path)
{
    FIL fil;
    if (f_open(&fil, path, FA_READ) != FR_OK) {
        blog("badusb: cannot open %s", path);
        return false;
    }
    uint32_t size = f_size(&fil);
    if (size > INJECT_MAX_SIZE) {
        blog("badusb: %s too large (%lu bytes)", path, (unsigned long)size);
        f_close(&fil);
        return false;
    }
    static uint8_t buf[INJECT_MAX_SIZE];
    UINT br;
    f_read(&fil, buf, size, &br);
    f_close(&fil);

    blog("Executing %s (%u bytes)", path, br);

    /* 等待 USB 枚举 (最多 5s, 期间喂狗 + 服务 USB 栈) */
    int wait = 0;
    while (!hid_ready() && wait < 5000) {
        hid_task();
        watchdog_update();
        sleep_ms(1);
        wait++;
    }
    if (!hid_ready()) {
        blog("ERROR: USB HID not ready (check USB_OTG)");
        return false;
    }
    blog("USB HID ready, injecting...");
    hid_delay_ms(500);          /* 等目标机识别键盘 (同时服务 USB 栈) */

    payload_execute(buf, br);
    blog("Done.");
    return true;
}
