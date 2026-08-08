/**
 * @file    shell.c
 * @brief   串口 Shell 实现 — 命令注册/解析/回显/提示符
 */

#include "shell.h"
#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "BSP/UART/uart.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#define MAX_CMD  64
#define MAX_ARGS 16

static char prompt_str[8] = "$ ";
static struct { char name[12]; char help[32]; shell_cmd_fn fn; } cmds[MAX_ARGS];
static int cmd_cnt = 0;
static void (*_echo_cb)(char) = NULL;
static void (*_out_cb)(const char *) = NULL;

void shell_set_echo_cb(void (*cb)(char)) { _echo_cb = cb; }
void shell_set_output_cb(void (*cb)(const char *)) { _out_cb = cb; }

/* 命令输出: 串口 + (若设置了)LCD 终端 */
static void _say_line(const char *s)
{
    shell_print(s); shell_print("\r\n");
    if (_out_cb) _out_cb(s);
}

void shell_init(const char *prompt)
{
    /* UART 由调用者初始化 (uart_init_dev) */
    sleep_ms(10);
    while (uart_read_byte() >= 0);
    if (prompt) snprintf(prompt_str, 8, "%s", prompt);
    cmd_cnt = 0;
}

void shell_register(const char *name, const char *help, shell_cmd_fn fn)
{
    if (cmd_cnt >= MAX_ARGS) return;
    snprintf(cmds[cmd_cnt].name, 12, "%s", name);
    snprintf(cmds[cmd_cnt].help, 32, "%s", help);
    cmds[cmd_cnt].fn = fn;
    cmd_cnt++;
}

void shell_print(const char *s)
{
    while (*s) { uart_send_byte((uint8_t)*s++); }
}

void shell_printf(const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    shell_print(buf);
}

/* ---- 内置命令 ---- */
static void _cmd_help(const char *arg)
{
    (void)arg;
    char buf[64];
    _say_line("=== COMMANDS ===");
    for (int i = 0; i < cmd_cnt; i++) {
        snprintf(buf, sizeof(buf), "  %-12s %s", cmds[i].name, cmds[i].help);
        _say_line(buf);
    }
    _say_line("  help          show this");
    _say_line("  reboot        enter bootloader");
}

static void _cmd_reboot(const char *arg)
{
    (void)arg;
    _say_line("rebooting...");
    sleep_ms(100);
    reset_usb_boot(0, 0);
}

void shell_poll(void)
{
    static char cmd[MAX_CMD];
    static int  pos = 0;
    static bool  first = true;

    if (first) { shell_printf("%s", prompt_str); first = false; }

    int ch = uart_read_byte();
    if (ch < 0) return;

    if (ch == '\r' || ch == '\n') {
        cmd[pos] = '\0';
        shell_printf("\r\n");
        if (_echo_cb) _echo_cb('\n');

        /* 解析命令+参数 */
        char *p = cmd, *arg = NULL;
        while (*p && *p == ' ') p++;  /* skip leading spaces */
        char *cmd_name = p;
        while (*p && *p != ' ') p++;
        if (*p) { *p = '\0'; arg = p + 1; }

        /* 空行 → 提示符 */
        if (strlen(cmd_name) == 0) {
            /* skip */
        }
        /* 内置 */
        else if (strcmp(cmd_name, "help") == 0) {
            _cmd_help(arg);
        }
        else if (strcmp(cmd_name, "reboot") == 0) {
            _cmd_reboot(arg);
        }
        /* 用户命令 */
        else {
            bool found = false;
            for (int i = 0; i < cmd_cnt; i++) {
                if (strcmp(cmd_name, cmds[i].name) == 0) {
                    cmds[i].fn(arg);
                    found = true;
                    break;
                }
            }
            if (!found) { char b[64]; snprintf(b, sizeof(b), "? %s", cmd_name); _say_line(b); }
        }

        shell_printf("%s", prompt_str);
        pos = 0;
    } else if (ch == '\b' || ch == 0x7F) {
        if (pos > 0) { pos--; shell_printf("\b \b"); if (_echo_cb) _echo_cb('\b'); }
    } else if (ch >= ' ' && pos < MAX_CMD - 1) {
        cmd[pos++] = (char)ch;
        uart_send_byte((uint8_t)ch);
        if (_echo_cb) _echo_cb((char)ch);
    }
}
