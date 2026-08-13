/**
 * @file    tui_demo.c
 * @brief   TUI 演示界面 — 主菜单 + About + SD 文件浏览
 *
 * 注册为 shell 命令 "tui", 进入全屏 TUI 交互界面。
 */

#include "tui/tui.h"
#include "tui/tui_lcd.h"
#include "ff.h"
#include <stdio.h>
#include <string.h>

static int tui_demo_about(void)
{
    tui_fill(1, 1, TUI_ROWS, TUI_COLS, ' ');
    tui_title("About");
    tui_move(2, 1);  tui_printf("DNRP2350A RP2350A M33 150MHz");
    tui_move(3, 1);  tui_printf("LCD 240x135 ST7789 PIO+DMA");
    tui_move(4, 1);  tui_printf("UART CH343 115200 8N1");
    tui_move(5, 1);  tui_printf("SD SPI mode FatFs");
    tui_move(6, 1);  tui_printf("TUI ANSI widgets");
    tui_hint("Esc:back  any:key");
    tui_event_t e = tui_getch();
    return (e.type == TUI_KEY_CTRLC) ? TUI_QUIT_CTRC : 0;
}

static int tui_demo_files(void)
{
    char lab[20][40];
    const char *items[20];
    int n = 0;
    DIR dir; FILINFO fno;
    if (f_opendir(&dir, "/") == FR_OK) {
        while (n < 20 && f_readdir(&dir, &fno) == FR_OK && fno.fname[0]) {
            if (fno.fattrib & AM_DIR)
                snprintf(lab[n], sizeof(lab[n]), "%-14.14s %7s", fno.fname, "<DIR>");
            else
                snprintf(lab[n], sizeof(lab[n]), "%-14.14s %7lu", fno.fname, (unsigned long)fno.fsize);
            items[n] = lab[n];
            n++;
        }
        f_closedir(&dir);
    }

    if (n == 0) {
        tui_fill(1, 1, TUI_ROWS, TUI_COLS, ' ');
        tui_title("SD root");
        tui_move(4, 1);  tui_printf("No SD card / no files");
        tui_hint("any:back");
        tui_event_t e = tui_getch();
        return (e.type == TUI_KEY_CTRLC) ? TUI_QUIT_CTRC : 0;
    }

    int sel = 0;
    while (1) {
        tui_fill(1, 1, TUI_ROWS, TUI_COLS, ' ');
        int r = tui_menu("SD root", items, n, &sel);
        if (r == TUI_QUIT_CTRC) return TUI_QUIT_CTRC;
        if (r < 0) return 0;
    }
}

void tui_demo_run(const char *arg)
{
    (void)arg;
    tui_start();
    tui_lcd_attach();
    const char *items[] = { "About", "SD files", "Exit TUI" };
    int sel = 0;
    while (1) {
        tui_fill(1, 1, TUI_ROWS, TUI_COLS, ' ');
        int r = tui_menu("DNRP2350A TUI", items, 3, &sel);
        if (r < 0)                       break;
        if (r == 0)      { if (tui_demo_about() == TUI_QUIT_CTRC) break; }
        else if (r == 1) { if (tui_demo_files() == TUI_QUIT_CTRC) break; }
        else             break;
    }
    tui_lcd_detach();
    tui_stop();
}