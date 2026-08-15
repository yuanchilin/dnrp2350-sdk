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
#include "BSP/UART/uart.h"
#include "pico/stdlib.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

static char *trim(char *s)
{
    while (isspace((unsigned char)*s)) s++;
    if (!*s) return s;
    char *e = s + strlen(s) - 1;
    while (e > s && isspace((unsigned char)*e)) *e-- = '\0';
    return s;
}

static int key_from_name(const char *name)
{
    if (!name) return 0;
    char c = toupper((unsigned char)name[0]);
    if (c >= 'A' && c <= 'Z' && !name[1]) return hid_keycode(c);  /* 单字母 → HID 键码 */
    if (strcasecmp(name, "ESC") == 0)      return HID_KEY_ESCAPE;
    if (strcasecmp(name, "TAB") == 0)      return HID_KEY_TAB;
    if (strcasecmp(name, "ENTER") == 0)    return HID_KEY_ENTER;
    if (strcasecmp(name, "SPACE") == 0)    return HID_KEY_SPACE;
    if (strcasecmp(name, "BS") == 0 || strcasecmp(name, "BACKSPACE") == 0)
                                           return HID_KEY_BACKSPACE;
    if (strcasecmp(name, "DELETE") == 0 || strcasecmp(name, "DEL") == 0)
                                           return HID_KEY_DELETE;
    if (strcasecmp(name, "HOME") == 0)     return HID_KEY_HOME;
    if (strcasecmp(name, "END") == 0)      return HID_KEY_END;
    if (strcasecmp(name, "PGUP") == 0 || strcasecmp(name, "PAGEUP") == 0)
                                           return HID_KEY_PGUP;
    if (strcasecmp(name, "PGDN") == 0 || strcasecmp(name, "PAGEDOWN") == 0)
                                           return HID_KEY_PGDN;
    if (strcasecmp(name, "CAPSLOCK") == 0 || strcasecmp(name, "CAPS") == 0)
                                           return HID_KEY_CAPS_LOCK;
    if (strcasecmp(name, "UP") == 0 || strcasecmp(name, "UPARROW") == 0)
                                           return HID_KEY_UP;
    if (strcasecmp(name, "DOWN") == 0 || strcasecmp(name, "DOWNARROW") == 0)
                                           return HID_KEY_DOWN;
    if (strcasecmp(name, "LEFT") == 0 || strcasecmp(name, "LEFTARROW") == 0)
                                           return HID_KEY_LEFT;
    if (strcasecmp(name, "RIGHT") == 0 || strcasecmp(name, "RIGHTARROW") == 0)
                                           return HID_KEY_RIGHT;
    /* F1-F12 */
    if (name[0] == 'F' && name[1] >= '1' && name[1] <= '9' && !name[2])
        return HID_KEY_F1 + (name[1] - '1');
    if (strcasecmp(name, "F10") == 0) return HID_KEY_F1 + 9;
    if (strcasecmp(name, "F11") == 0) return HID_KEY_F1 + 10;
    if (strcasecmp(name, "F12") == 0) return HID_KEY_F1 + 11;
    return 0;
}

/* 组合键: 修饰键前缀 + 一个或多个键同时按下 (如 CTRL ALT DEL / ALT TAB / GUI r) */
static void combo_keys(uint8_t mods, const char *rest)
{
    uint8_t keys[8];
    int nk = 0;
    const char *p = rest;
    while (p && *p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        const char *w = p;
        while (*p && !isspace((unsigned char)*p)) p++;
        char word[16];
        int wl = (int)(p - w);
        if (wl >= (int)sizeof(word)) wl = (int)sizeof(word) - 1;
        memcpy(word, w, (size_t)wl);
        word[wl] = '\0';

        if      (strcasecmp(word, "CTRL") == 0)  mods |= KEY_MOD_CTRL;
        else if (strcasecmp(word, "ALT") == 0)   mods |= KEY_MOD_ALT;
        else if (strcasecmp(word, "GUI") == 0)   mods |= KEY_MOD_GUI;
        else if (strcasecmp(word, "SHIFT") == 0) mods |= KEY_MOD_SHIFT;
        else {
            uint8_t k = (uint8_t)key_from_name(word);
            if (k && nk < 8) keys[nk++] = k;
        }
    }
    if (nk == 0) {
        /* 仅修饰键组合 (如 ALT SHIFT = 切换键盘布局/输入法):
         * 中文系统注入前先切到英文布局, 避免拼音组词乱码 */
        if (mods == 0) return;
        hid_key_press_multi(mods, NULL, 0);
        hid_delay_ms(30);
        hid_key_release();
        return;
    }
    hid_key_press_multi(mods, keys, (uint8_t)nk);
    hid_delay_ms(10);
    hid_key_release();
}

static uint32_t g_default_delay = 0;    /* DEFAULTDELAY: 行间默认延迟 */

void payload_execute(const uint8_t *data, uint32_t len)
{
    /* 逐行解析 */
    char line[256];
    uint32_t pos = 0;

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

        /* 串口回显当前命令 (注入日志) */
        uart_printf("> %s\r\n", cmd);

        /* 解析命令 */
        char *arg = NULL;
        {
            char *p = cmd;
            while (*p && !isspace((unsigned char)*p)) p++;
            if (*p) { *p = '\0'; arg = trim(p + 1); }
        }

        /* REM / # 注释 */
        if (strcasecmp(cmd, "REM") == 0) {
            continue;
        }
        /* STRING */
        else if (strcasecmp(cmd, "STRING") == 0 && arg) {
            hid_string(arg);
        }
        /* STRINGLN: 输入 + 回车 */
        else if (strcasecmp(cmd, "STRINGLN") == 0 && arg) {
            hid_string(arg);
            hid_key_tap(0, HID_KEY_ENTER);
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
        /* 修饰键组合: CTRL/ALT/GUI/SHIFT + 一个或多个键 */
        else if (strcasecmp(cmd, "CTRL") == 0 && arg)  combo_keys(KEY_MOD_CTRL, arg);
        else if (strcasecmp(cmd, "ALT") == 0 && arg)   combo_keys(KEY_MOD_ALT, arg);
        else if (strcasecmp(cmd, "GUI") == 0 && arg)   combo_keys(KEY_MOD_GUI, arg);
        else if (strcasecmp(cmd, "SHIFT") == 0 && arg) combo_keys(KEY_MOD_SHIFT, arg);
        /* DELAY / WAIT */
        else if ((strcasecmp(cmd, "DELAY") == 0 || strcasecmp(cmd, "WAIT") == 0) && arg) {
            int ms = atoi(arg);
            if (ms > 0 && ms < 60000) {
                hid_delay_ms((uint32_t)ms);
            }
        }
        /* DEFAULTDELAY: 行间默认延迟 */
        else if (strcasecmp(cmd, "DEFAULTDELAY") == 0 && arg) {
            int ms = atoi(arg);
            if (ms >= 0 && ms < 60000) g_default_delay = (uint32_t)ms;
        }
        /* 单键命令: DELETE/HOME/END/PGUP/PGDN/CAPSLOCK/方向键/F1-F12/BS */
        else if (!arg && key_from_name(cmd) != 0) {
            hid_key_tap(0, (uint8_t)key_from_name(cmd));
        }
        /* 单独按修饰键 (无普通键):
         *   SHIFT — 微软拼音等输入法的 中/英 模式切换键, 注入前先切到英文可避免中文组词;
         *   CTRL / ALT / GUI — 单独单击 (对英文系统无副作用) */
        else if (!arg &&
                 (strcasecmp(cmd, "SHIFT") == 0 || strcasecmp(cmd, "CTRL") == 0 ||
                  strcasecmp(cmd, "ALT")   == 0 || strcasecmp(cmd, "GUI")  == 0)) {
            uint8_t mod = KEY_MOD_GUI;
            if      (strcasecmp(cmd, "SHIFT") == 0) mod = KEY_MOD_SHIFT;
            else if (strcasecmp(cmd, "CTRL")  == 0) mod = KEY_MOD_CTRL;
            else if (strcasecmp(cmd, "ALT")   == 0) mod = KEY_MOD_ALT;
            hid_key_press(mod, 0);      /* 修饰键按下 (无普通键) */
            hid_delay_ms(30);           /* 保持片刻再释放, 确保目标识别为一次单击 */
            hid_key_release();
        }
        else {
            uart_printf("? unknown payload cmd: %s\r\n", cmd);
        }

        /* 行间默认延迟 */
        if (g_default_delay > 0) hid_delay_ms(g_default_delay);
    }
}
