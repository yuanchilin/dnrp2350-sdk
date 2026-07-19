/**
 * @file    payload.c
 * @brief   BadUSB 脚本解析引擎
 *
 *          命令格式 (大小写不敏感):
 *            STRING <text>    — 逐字输出
 *            ENTER            — 回车
 *            TAB              — Tab 键
 *            GUI <key>        — Win+<key> (如: GUI r = 运行)
 *            CTRL <key>       — Ctrl+<key>
 *            ALT <key>        — Alt+<key>
 *            DELAY <ms>       — 延迟 (毫秒)
 *            REM <comment>    — 注释
 *            ESC              — Escape 键
 */

#include "payload.h"
#include "hid_keyboard.h"
#include "pico/stdlib.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

static char *trim(char *s)
{
    while (isspace(*s)) s++;
    if (!*s) return s;
    char *e = s + strlen(s) - 1;
    while (e > s && isspace(*e)) *e-- = '\0';
    return s;
}

static int key_from_name(const char *name)
{
    if (!name) return 0;
    char c = toupper(name[0]);
    if (c >= 'A' && c <= 'Z' && !name[1]) return c;  /* 单字母 */
    if (strcasecmp(name, "ESC") == 0)      return HID_KEY_ESCAPE;
    if (strcasecmp(name, "TAB") == 0)      return HID_KEY_TAB;
    if (strcasecmp(name, "ENTER") == 0)    return HID_KEY_ENTER;
    if (strcasecmp(name, "SPACE") == 0)    return HID_KEY_SPACE;
    if (strcasecmp(name, "BS") == 0)       return HID_KEY_BACKSPACE;
    return 0;
}

void payload_execute(const uint8_t *data, uint32_t len)
{
    /* 逐行解析 */
    char line[256];
    uint32_t pos = 0;
    int line_idx = 0;

    while (pos < len) {
        /* 读一行 */
        int li = 0;
        while (pos < len && li < 254) {
            char c = (char)data[pos++];
            if (c == '\r') continue;
            if (c == '\n') break;
            line[li++] = c;
        }
        line[li] = '\0';

        char *cmd = trim(line);
        if (!*cmd || cmd[0] == '#') continue;  /* 空行 / 注释 */

        line_idx++;

        /* 解析命令 */
        char *arg = NULL;
        {
            char *p = cmd;
            while (*p && !isspace(*p)) p++;
            if (*p) { *p = '\0'; arg = trim(p + 1); }
        }

        /* REM / # 注释 */
        if (strcasecmp(cmd, "REM") == 0 || cmd[0] == '#') {
            continue;
        }
        /* STRING */
        else if (strcasecmp(cmd, "STRING") == 0 && arg) {
            hid_string(arg);
        }
        /* ENTER */
        else if (strcasecmp(cmd, "ENTER") == 0) {
            hid_key_tap(0, HID_KEY_ENTER);
        }
        /* TAB */
        else if (strcasecmp(cmd, "TAB") == 0) {
            hid_key_tap(0, HID_KEY_TAB);
        }
        /* ESC */
        else if (strcasecmp(cmd, "ESC") == 0) {
            hid_key_tap(0, HID_KEY_ESCAPE);
        }
        /* SPACE */
        else if (strcasecmp(cmd, "SPACE") == 0) {
            hid_key_tap(0, HID_KEY_SPACE);
        }
        /* GUI */
        else if (strcasecmp(cmd, "GUI") == 0 && arg) {
            uint8_t k = key_from_name(arg);
            if (k) {
                hid_key_press(KEY_MOD_GUI, k);
                sleep_ms(10);
                hid_key_release();
            }
        }
        /* CTRL */
        else if (strcasecmp(cmd, "CTRL") == 0 && arg) {
            uint8_t k = key_from_name(arg);
            if (k) {
                hid_key_press(KEY_MOD_CTRL, k);
                sleep_ms(10);
                hid_key_release();
            }
        }
        /* ALT */
        else if (strcasecmp(cmd, "ALT") == 0 && arg) {
            uint8_t k = key_from_name(arg);
            if (k) {
                hid_key_press(KEY_MOD_ALT, k);
                sleep_ms(10);
                hid_key_release();
            }
        }
        /* DELAY */
        else if (strcasecmp(cmd, "DELAY") == 0 && arg) {
            int ms = atoi(arg);
            if (ms > 0 && ms < 60000) {
                hid_delay_ms((uint32_t)ms);
            }
        }
    }
}
