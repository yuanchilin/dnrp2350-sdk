/**
 * @file    shell.c
 * @brief   串口 Shell 实现 — 命令注册/解析/回显/提示符
 */

#include "shell.h"
#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "hardware/watchdog.h"
#include "BSP/UART/uart.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#define MAX_CMD  64
#define MAX_ARGS 16
#define MAX_HIST 16

static char prompt_str[8] = "$ ";
static struct { char name[12]; char help[32]; shell_cmd_fn fn; } cmds[MAX_ARGS];
static int cmd_cnt = 0;
static void (*_echo_cb)(char) = NULL;
static void (*_out_cb)(const char *) = NULL;
static shell_arg_cb _arg_cb = NULL;

/* 命令历史 (上下键浏览) */
static char hist[MAX_HIST][MAX_CMD];
static int  hist_cnt = 0;   /* 已存命令数 */
static int  hist_pos = 0;   /* 当前浏览位置 (0=新行/临时槽, >0=历史索引) */
static char hist_tmp[MAX_CMD];  /* 临时槽: 保存未执行的当前输入 */

static bool _ansi = false;  /* 串口 ANSI 颜色开关 */

void shell_set_ansi(bool on) { _ansi = on; }

void shell_set_echo_cb(void (*cb)(char)) { _echo_cb = cb; }
void shell_set_output_cb(void (*cb)(const char *)) { _out_cb = cb; }
void shell_set_arg_completer(shell_arg_cb cb) { _arg_cb = cb; }

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
/* help 行: 串口命令名亮绿色, 帮助说明普通色; LCD 用普通文本(避免 ANSI 乱码) */
static void _help_line(const char *name, const char *help)
{
    char tmp[64];
    snprintf(tmp, sizeof(tmp), "  " CLR_CMD "%-12s" ANSI_RESET " %s", name, help);
    shell_print(tmp); shell_print("\r\n");          /* 串口: 命令名亮绿 */
    if (_out_cb) _out_cb(tmp);                       /* LCD: 同源 ANSI 串, 由 console_write_ansi 解析 */
}

static void _cmd_help(const char *arg)
{
    (void)arg;
    _say_line("=== COMMANDS ===");
    for (int i = 0; i < cmd_cnt; i++) {
        _help_line(cmds[i].name, cmds[i].help);
    }
    _help_line("help",   "show this");
    _help_line("reboot", "enter bootloader");
    _help_line("reset",  "soft reset firmware");
}

/* ---- 公共 reboot/reset (供各项目复用) ---- */
void shell_reboot(void)
{
    _say_line("rebooting...");
    sleep_ms(100);
    reset_usb_boot(0, 0);
}

void shell_reset(void)
{
    _say_line("resetting...");
    sleep_ms(100);
    watchdog_enable(1, 1);   /* 1ms 后触发 watchdog → 软复位回到固件 */
    for (;;) { sleep_ms(10); }
}

static void _cmd_reboot(const char *arg)
{
    (void)arg;
    shell_reboot();
}

static void _cmd_reset(const char *arg)
{
    (void)arg;
    shell_reset();
}

/* ---- Tab 补全 (prefix match over 内置 + 已注册命令) ---- */
static void _tab_complete(char *cmd, int *pos)
{
    /* 确保前缀以 '\0' 结尾以便 strlen */
    cmd[*pos] = '\0';

    char *name = cmd;
    while (*name && *name == ' ') name++;
    if (*name == 0) return;

    /* 只对第一个词(命令名)做补全: 遇到空格即截断 */
    int plen = 0;
    while (name[plen] && name[plen] != ' ') plen++;

    /* 命令名后已有空格 → 补全第二个词(参数/文件名) */
    if (name[plen] == ' ') {
        if (!_arg_cb) return;                       /* 无参数补全器 → 静默 */
        const char *tok = name + plen;              /* 指向空格后的参数前缀 */
        while (*tok == ' ') tok++;
        int tlen = (int)strlen(tok);
        char out[64];
        int n = _arg_cb(tok, out, sizeof(out));
        if (n <= 0) return;                          /* 无候选 → 静默 */
        if (n == 1) {
            /* 唯一候选 → 追加剩余部分 (跳过已输入前缀) */
            const char *full = out + tlen;           /* 从已输入前缀之后开始 */
            for (int i = 0; full[i] && *pos < MAX_CMD - 1; i++) {
                cmd[(*pos)++] = full[i];
                uart_send_byte((uint8_t)full[i]);
                if (_echo_cb) _echo_cb(full[i]);
            }
            return;
        }
        /* 多候选 → 回调已列出候选名, 这里重绘提示符+当前输入 */
        shell_printf("%s", prompt_str);
        for (int i = 0; i < *pos; i++) {
            uart_send_byte((uint8_t)cmd[i]);
            if (_echo_cb) _echo_cb(cmd[i]);
        }
        return;
    }

    /* 收集候选: 内置命令 + 用户命令 */
    static const char *builtin[] = { "help", "reboot", "reset" };
    char hits[16][12];
    int n = 0;
    for (int i = 0; i < 3 && n < 16; i++)
        if (strncmp(builtin[i], name, plen) == 0) snprintf(hits[n++], 12, "%s", builtin[i]);
    for (int i = 0; i < cmd_cnt && n < 16; i++)
        if (strncmp(cmds[i].name, name, plen) == 0) snprintf(hits[n++], 12, "%s", cmds[i].name);

    if (n == 0) { shell_print("\a"); return; }  /* 无匹配 → 蜂鸣 */

    if (n == 1) {
        /* 唯一匹配 → 补全剩余字符 */
        const char *full = hits[0];
        for (int i = plen; full[i] && *pos < MAX_CMD - 1; i++) {
            cmd[(*pos)++] = full[i];
            uart_send_byte((uint8_t)full[i]);  /* 串口回显单字符 */
            if (_echo_cb) _echo_cb(full[i]);   /* LCD 同步 */
        }
        return;
    }

    /* 多匹配 → 列出候选, 再重绘提示符+当前输入 */
    shell_print("\r\n");
    char b[24];
    for (int i = 0; i < n; i++) {
        snprintf(b, sizeof(b), "  %s", hits[i]);
        _say_line(b);
    }
    shell_printf("%s", prompt_str);
    for (int i = 0; i < *pos; i++) {
        uart_send_byte((uint8_t)cmd[i]);
        if (_echo_cb) _echo_cb(cmd[i]);
    }
}

void shell_poll(void)
{
    static char cmd[MAX_CMD];
    static int  pos = 0;
    static bool  first = true;

    if (first) { if (_ansi) shell_print(ANSI_RESET); shell_printf("%s", prompt_str); if (_ansi) shell_print(CLR_INPUT); first = false; }

    int ch = uart_read_byte();
    if (ch < 0) return;

    if (ch == '\r' || ch == '\n') {
        if (_ansi) shell_print(ANSI_RESET);   /* 命令执行 → 复位颜色 */
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
        else if (strcmp(cmd_name, "help") == 0) {
            _cmd_help(arg);
        }
        else if (strcmp(cmd_name, "reboot") == 0) {
            _cmd_reboot(arg);
        }
        else if (strcmp(cmd_name, "reset") == 0) {
            _cmd_reset(arg);
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

        /* 保存历史 (非空行) */
        if (cmd[0]) {
            if (hist_cnt == 0 || strcmp(hist[hist_cnt - 1], cmd) != 0) {
                if (hist_cnt < MAX_HIST) {
                    snprintf(hist[hist_cnt++], MAX_CMD, "%s", cmd);
                } else {
                    for (int i = 0; i < MAX_HIST - 1; i++) memcpy(hist[i], hist[i + 1], MAX_CMD);
                    snprintf(hist[MAX_HIST - 1], MAX_CMD, "%s", cmd);
                }
            }
        }
        hist_pos = 0;

        shell_printf("%s", prompt_str);
        if (_ansi) shell_print(CLR_INPUT);   /* 新命令输入 → 绿色 */
        pos = 0;
    } else if (ch == 0x1B) {   /* ESC → 箭头序列 (上下键浏览历史) */
        /* 等待 '[' 和 A/B (上下) */
        int do_hist = 0;
        int c1 = uart_read_byte();
        if (c1 == '[') {
            int c2 = uart_read_byte();
            if (c2 == 'A') do_hist = 1;    /* up: 浏览更旧 (索引 +1) */
            else if (c2 == 'B') do_hist = -1; /* down: 回更新/新行 (索引 -1) */
        }
        if (do_hist) {
            /* 临时槽: 保存当前未执行输入, 供 down 回到新行时恢复 */
            {
                int k = 0;
                while (k < pos && k < MAX_CMD - 1) { hist_tmp[k] = cmd[k]; k++; }
                hist_tmp[k] = '\0';
            }

            int new_pos = hist_pos + do_hist;
            if (new_pos >= 0 && new_pos <= hist_cnt) {
                hist_pos = new_pos;
                /* 清空当前行并回显 */
                for (int i = 0; i < pos; i++) { shell_print("\b \b"); if (_echo_cb) _echo_cb('\b'); }
                pos = 0;
                const char *h;
                if (hist_pos == 0) h = hist_tmp;  /* 临时槽 (当前未执行输入) */
                else h = hist[hist_cnt - hist_pos];    /* 历史命令 */
                for (int i = 0; h[i] && i < MAX_CMD - 1; i++) {
                    cmd[pos++] = h[i];
                    uart_send_byte((uint8_t)h[i]);
                    if (_echo_cb) _echo_cb(h[i]);
                }
            }
        }
    } else if (ch == '\t') {
        _tab_complete(cmd, &pos);
    } else if (ch == '\b' || ch == 0x7F) {
        if (pos > 0) { pos--; shell_printf("\b \b"); if (_echo_cb) _echo_cb('\b'); }
    } else if (ch >= ' ' && pos < MAX_CMD - 1) {
        cmd[pos++] = (char)ch;
        uart_send_byte((uint8_t)ch);
        if (_echo_cb) _echo_cb((char)ch);
    }
}
