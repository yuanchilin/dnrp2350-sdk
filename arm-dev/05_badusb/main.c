/**
 * @file    main.c
 * @brief   BadUSB — USB HID 键盘注入器
 *
 *          LCD 菜单 → TF 卡 .txt 脚本 → KEY1 注入 / 串口命令
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "hardware/watchdog.h"
#include "BSP/LCD/lcd.h"
#include "BSP/LED/led.h"
#include "BSP/SPI/spi.h"
#include "BSP/UART/uart.h"
#include "BSP/SDIO/spi_sdcard.h"
#include "BSP/KEY/key.h"
#include "badusb/hid_keyboard.h"
#include "tusb.h"
#include "badusb/payload.h"
#include "badusb/sample_script.h"
#include "ff.h"

/* ========================================================================== */
/*  常量                                                                       */
/* ========================================================================== */
#define LCD_W         240
#define LCD_H         135
#define MAX_FILES     32
#define MAX_NAME      48

/* ========================================================================== */
/*  全局                                                                       */
/* ========================================================================== */
static char files[MAX_FILES][MAX_NAME];
static int  file_cnt = 0;
static int  selected = 0;
static int  scroll_top = 0;         /* 菜单滚动偏移 (支持 >8 个文件) */

/* ========================================================================== */
/*  扫描 SD 卡 .txt 文件                                                       */
/* ========================================================================== */
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

/* ========================================================================== */
/*  读取并执行脚本文件                                                          */
/* ========================================================================== */
static void run_script(const char *path)
{
    FIL fil;
    if (f_open(&fil, path, FA_READ) != FR_OK) {
        uart_printf("Cannot open: %s\r\n", path);
        return;
    }

    uint32_t size = f_size(&fil);
    if (size > 16384) {          /* 最大 16KB */
        uart_printf("Script too large (%lu bytes)\r\n", (unsigned long)size);
        f_close(&fil);
        return;
    }

    static uint8_t buf[16384];
    UINT br;
    f_read(&fil, buf, size, &br);
    f_close(&fil);

    uart_printf("Executing: %s (%u bytes)\r\n", path, br);

    LED(0);                         /* 注入进行中: LED 亮 */

    /* LCD 提示 */
    lcd_fill(0, LCD_H - 20, LCD_W - 1, LCD_H - 1, RED);
    lcd_show_string(2, LCD_H - 18, 200, 16, 12, "INJECTING...", WHITE);

    /* 等待 USB 枚举 (最多等 5 秒) */
    uart_printf("Waiting for USB HID ready...\r\n");
    int wait_cnt = 0;
    uint32_t last_state = 0;
    while (!hid_ready() && wait_cnt < 5000) {
        hid_task();
        watchdog_update();          /* 等待枚举最长 5s, 需喂狗 */
        uint32_t st = (tud_connected() ? 1u : 0u) | (tud_mounted() ? 2u : 0u) | (tud_ready() ? 4u : 0u);
        if (st != last_state) {
            uart_printf("USB state: conn=%d mount=%d ready=%d\r\n",
                        tud_connected(), tud_mounted(), tud_ready());
            last_state = st;
        }
        sleep_ms(1);
        wait_cnt++;
    }
    if (!hid_ready()) {
        uart_printf("ERROR: USB HID NOT READY after 5s!\r\n");
        uart_printf("Check: USB_OTG connected? Port correct?\r\n");
        lcd_fill(0, LCD_H - 20, LCD_W - 1, LCD_H - 1, RED);
        lcd_show_string(2, LCD_H - 18, 200, 16, 12, "USB NOT READY!", WHITE);
        LCD_PWR(0);   /* GPIO25 与 LED 复用, 高电平会关背光 — 保持错误提示可见 */
        return;
    }
    uart_printf("USB HID ready, injecting...\r\n");
    sleep_ms(500);   /* 等目标电脑识别键盘 */

    /* 执行 */
    payload_execute(buf, br);

    /* 完成 */
    lcd_fill(0, LCD_H - 20, LCD_W - 1, LCD_H - 1, GREEN);
    lcd_show_string(2, LCD_H - 18, 200, 16, 12, "DONE!", WHITE);
    LED(0);
    uart_printf("Done.\r\n> ");
}

/* ========================================================================== */
/*  绘制 LCD 菜单                                                              */
/* ========================================================================== */
static void draw_menu(void)
{
    lcd_clear(BLACK);

    /* 标题栏 */
    lcd_fill(0, 0, LCD_W - 1, 18, BLUE);
    lcd_show_string(2, 2, 220, 16, 12, "BADUSB — Payload Menu", WHITE);

    /* 文件列表 (支持滚动, 一屏显示 8 项) */
    if (file_cnt == 0) {
        lcd_show_string(4, 30, 220, 24, 16, "No .txt files", RED);
        lcd_show_string(4, 50, 220, 20, 12, "Put scripts on FAT32 SD", MAGENTA);
    } else {
        int shown = 0;
        for (int i = scroll_top; i < file_cnt && shown < 8; i++, shown++) {
            int y = 22 + shown * 13;
            uint16_t bg = (i == selected) ? BLUE : BLACK;
            uint16_t fg = (i == selected) ? WHITE : CYAN;

            lcd_fill(2, y, LCD_W - 3, y + 12, bg);
            char label[32];
            snprintf(label, 32, "[%d] %s", i + 1, files[i] + 1);  /* skip leading / */
            lcd_show_string(4, y + 1, 230, 12, 12, label, fg);
        }
    }

    /* 底栏 */
    lcd_fill(0, LCD_H - 34, LCD_W - 1, LCD_H - 1, 0x1082);
    lcd_show_string(2, LCD_H - 32, 230, 12, 12, "KEY1:INJECT  UART:n/p/e/s", WHITE);
    lcd_show_string(2, LCD_H - 18, 230, 12, 12,
                    hid_ready() ? "USB HID: READY" : "USB HID: waiting...",
                    hid_ready() ? GREEN : RED);
    /* 保持背光点亮: GPIO25 与板载 LED 复用 (LCD_PWR 低电平 = 背光 ON),
     * 用 LED(1) 会拉高 GPIO25 把背光关掉, 菜单将不可见 */
    LCD_PWR(0);
}

/* ========================================================================== */
/*  主函数                                                                     */
/* ========================================================================== */
int main(void)
{
    stdio_init_all();
    uart_init_dev();
    sleep_ms(100);
    while (uart_read_byte() >= 0);    /* 清噪声 */
    led_init();
    key_init();
    spi1_init();
    lcd_init();
    hid_init();
    /* 确保背光 — 防止 board_init 干扰 */
    gpio_set_dir(25, GPIO_OUT);
    gpio_put(25, 0);              /* LCD_PWR(0) = 背光 ON */

    uart_printf("\r\n========================================\r\n");
    uart_printf(" DNRP2350A — BadUSB HID Injector\r\n");
    uart_printf(" USB_OTG -> Target PC\r\n");
    uart_printf("========================================\r\n");

    /* 挂载 SD */
    FATFS fs;
    if (f_mount(&fs, "0:", 1) != FR_OK) {
        uart_printf("SD mount failed!\r\n");
    } else {
        uart_printf("SD OK. ");
        /* 自动写入示例脚本: 不存在或与内置示例不一致时刷新
         * (烧录新固件后 hello.txt 会自动更新, 用户自定义脚本请用别的文件名) */
        FIL test;
        bool need_demo = false;
        if (f_open(&test, "/hello.txt", FA_READ) != FR_OK) {
            need_demo = true;
        } else {
            need_demo = (f_size(&test) != SAMPLE_PAYLOAD_SIZE);
            f_close(&test);
        }
        if (need_demo) {
            uart_printf("Writing demo script... ");
            if (f_open(&test, "/hello.txt", FA_WRITE | FA_CREATE_ALWAYS) == FR_OK) {
                UINT bw;
                f_write(&test, sample_payload, SAMPLE_PAYLOAD_SIZE, &bw);
                f_close(&test);
                uart_printf("OK! ");
            }
        }
        scan_scripts();
        uart_printf("%d scripts found.\r\n", file_cnt);
    }

    draw_menu();
    uart_printf("Ready. Plug USB_OTG to target, press KEY1 to inject.\r\n> ");

    /* 主循环 */
    char cmd[16]; int pos = 0;
    bool last_key = false;

    while (1) {
        watchdog_update();
        hid_task();   /* TinyUSB 设备循环 */

        /* KEY1 物理按键 */
        bool key_now = (key_scan(0) == KEY_PRES);
        if (key_now && !last_key && file_cnt > 0) {
            uart_printf("\r\nKEY1 pressed — inject [%d] %s\r\n",
                        selected + 1, files[selected]);
            run_script(files[selected]);
            draw_menu();
        }
        last_key = key_now;

        /* 串口命令 */
        int ch = uart_read_byte();
        if (ch < 0) { sleep_ms(10); continue; }
        if (ch == '\r' || ch == '\n') {
            if (pos > 0) {
                cmd[pos] = '\0'; uart_printf("\r\n");
                if (strcmp(cmd, "n") == 0 && file_cnt > 0) {
                    selected = (selected + 1) % file_cnt;
                    if (selected - scroll_top >= 8) scroll_top = selected - 7;
                    draw_menu();
                } else if (strcmp(cmd, "p") == 0 && file_cnt > 0) {
                    selected = (selected - 1 + file_cnt) % file_cnt;
                    if (selected < scroll_top) scroll_top = selected;
                    draw_menu();
                } else if (strcmp(cmd, "e") == 0 && file_cnt > 0) {
                    run_script(files[selected]);
                    draw_menu();
                } else if (strcmp(cmd, "s") == 0) {
                    scan_scripts();
                    selected = selected < file_cnt ? selected : (file_cnt ? file_cnt - 1 : 0);
                    uart_printf("%d scripts found.\r\n", file_cnt);
                    draw_menu();
                } else if (strcmp(cmd, "r") == 0 || strcmp(cmd, "reboot") == 0) {
                    uart_printf("rebooting to bootloader...\r\n");
                    sleep_ms(100);
                    reset_usb_boot(0, 0);
                } else if (strlen(cmd) > 0) {
                    uart_printf("? n=next p=prev e=inject s=rescan r=reboot\r\n> ");
                }
                pos = 0;
            }
        } else if (ch == '\b' || ch == 0x7F) {
            if (pos > 0) { pos--; uart_printf("\b \b"); }
        } else if (ch == 0x03 || ch == 0x15) {      /* Ctrl+C / Ctrl+U: 取消当前输入 */
            pos = 0;
        } else if (ch >= ' ' && pos < 15) {
            cmd[pos++] = (char)ch; uart_send_byte((uint8_t)ch);
        }
    }
    return 0;
}
