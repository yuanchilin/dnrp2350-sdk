/**
 * @file    led_cmd.c
 * @brief   LED shell 命令层 — 串口动态控制板载 LED
 *
 *          命令:  led on / off / toggle / blink [ms]
 *          依赖:  SHELL + BSP/LED
 */

#include <string.h>
#include <stdlib.h>
#include "BSP/LED/led.h"
#include "shell/shell.h"

static void cmd_led(const char *arg)
{
    if (!arg || !*arg) { shell_print("led: need on|off|toggle|blink [ms]\r\n"); return; }

    if (strncmp(arg, "on", 2) == 0) {
        LED(0); shell_print("led: ON\r\n");
    } else if (strncmp(arg, "off", 3) == 0) {
        LED(1); shell_print("led: OFF\r\n");
    } else if (strncmp(arg, "toggle", 6) == 0) {
        LED_TOGGLE(); shell_print("led: toggled\r\n");
    } else if (strncmp(arg, "blink", 5) == 0) {
        uint32_t ms = 500;
        const char *p = arg + 5;
        while (*p == ' ') p++;
        if (*p) {
            long v = strtol(p, NULL, 10);
            if (v > 0) ms = (uint32_t)v;
        }
        shell_printf("led: blink %lu ms\r\n", (unsigned long)ms);
        led_blink(ms);              /* 不返回 */
    } else {
        shell_printf("led: unknown '%s' (on|off|toggle|blink [ms])\r\n", arg);
    }
}

void led_register(void)
{
    shell_register("led", "control LED: on/off/toggle/blink", cmd_led);
}
