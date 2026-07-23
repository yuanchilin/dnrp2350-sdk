/**
 * @file    main.c
 * @brief   PIO+DMA 照片显示 — 开机加载 /photo.bmp
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "lcd_spi.pio.h"
#include "BSP/LCD/lcd.h"
#include "BSP/LED/led.h"
#include "BSP/SPI/spi.h"
#include "BSP/UART/uart.h"
#include "BSP/SDIO/spi_sdcard.h"
#include "ff.h"

#define LCD_W 240
#define LCD_H 135
#define BUF_SIZE (LCD_W * LCD_H * 2)
#define PIO_INST pio0
#define MOSI_PIN 11
#define SCK_PIN  10

static uint8_t fb[BUF_SIZE];
static int pio_sm = -1, pio_ofs = -1, dma_ch = -1;
static bool pio_ok = false;

static bool pio_init(void)
{
    if (pio_ok) return true;
    if (!pio_can_add_program(PIO_INST, &lcd_spi8_program)) return false;
    pio_ofs = pio_add_program(PIO_INST, &lcd_spi8_program);
    pio_sm = pio_claim_unused_sm(PIO_INST, false);
    if (pio_sm < 0) return false;
    lcd_spi8_program_init(PIO_INST, pio_sm, pio_ofs, MOSI_PIN, SCK_PIN);
    pio_sm_set_clkdiv(PIO_INST, pio_sm, 2.0f);
    dma_ch = dma_claim_unused_channel(false);
    if (dma_ch < 0) return false;
    dma_channel_config cfg = dma_channel_get_default_config(dma_ch);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_8);
    channel_config_set_read_increment(&cfg, true);
    channel_config_set_write_increment(&cfg, false);
    channel_config_set_dreq(&cfg, pio_get_dreq(PIO_INST, pio_sm, true));
    dma_channel_configure(dma_ch, &cfg, &PIO_INST->txf[pio_sm], fb, BUF_SIZE, false);
    pio_ok = true;
    return true;
}

static void pio_flush(void)
{
    if (!pio_init()) return;
    lcd_set_window(0, 0, LCD_W - 1, LCD_H - 1);
    lcd_write_cmd(0x2C);
    LCD_WR(1); LCD_CS(0);
    gpio_set_function(MOSI_PIN, GPIO_FUNC_PIO0);
    gpio_set_function(SCK_PIN,  GPIO_FUNC_PIO0);
    dma_channel_set_read_addr(dma_ch, fb, false);
    dma_channel_set_trans_count(dma_ch, BUF_SIZE, true);
    dma_channel_wait_for_finish_blocking(dma_ch);
    LCD_CS(1);
    gpio_set_function(MOSI_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SCK_PIN,  GPIO_FUNC_SPI);
}

typedef struct __attribute__((packed)) {
    uint16_t bfType; uint32_t bfSize; uint16_t bfReserved1, bfReserved2; uint32_t bfOffBits;
} bmp_hdr_t;
typedef struct __attribute__((packed)) {
    uint32_t biSize; int32_t biWidth, biHeight; uint16_t biPlanes, biBitCount;
    uint32_t biCompression, biSizeImage; int32_t biXPelsPerMeter, biYPelsPerMeter;
    uint32_t biClrUsed, biClrImportant;
} bmp_info_t;

static bool load_bmp(const char *path)
{
    FIL fil; UINT br;
    if (f_open(&fil, path, FA_READ) != FR_OK) return false;
    bmp_hdr_t fh; bmp_info_t ih;
    f_read(&fil, &fh, sizeof(fh), &br);
    f_read(&fil, &ih, sizeof(ih), &br);
    if (fh.bfType != 0x4D42 || ih.biBitCount != 24) { f_close(&fil); return false; }
    int iw = ih.biWidth, ihgt = (ih.biHeight > 0) ? ih.biHeight : -ih.biHeight;
    int rb = (iw * 3 + 3) & ~3;
    bool td = (ih.biHeight < 0);
    int sx = (iw > LCD_W) ? (iw * 10 / LCD_W) : 10;
    int sy = (ihgt > LCD_H) ? (ihgt * 10 / LCD_H) : 10;
    int dw = (iw > LCD_W) ? LCD_W : iw, dh = (ihgt > LCD_H) ? LCD_H : ihgt;
    int ox = (LCD_W - dw) / 2, oy = (LCD_H - dh) / 2;
    uint8_t line[240 * 3 + 4];
    f_lseek(&fil, fh.bfOffBits);
    for (int ly = 0; ly < dh; ly++) {
        int by = td ? (ly * sy / 10) : (ihgt - 1 - ly * sy / 10);
        f_lseek(&fil, fh.bfOffBits + (FSIZE_t)by * rb);
        f_read(&fil, line, rb, &br);
        for (int lx = 0; lx < dw; lx++) {
            uint8_t *p = &line[(lx * sx / 10) * 3];
            uint16_t c = ((p[2] >> 3) << 11) | ((p[1] >> 2) << 5) | (p[0] >> 3);
            int idx = ((oy + ly) * LCD_W + (ox + lx)) * 2;
            fb[idx] = c >> 8; fb[idx + 1] = c & 0xFF;
        }
    }
    f_close(&fil);
    return true;
}

int main(void)
{
    led_init();
    for (int i = 0; i < 2; i++) { LED(0); sleep_ms(100); LED(1); sleep_ms(100); }

    uart_init_dev();
    sleep_ms(100); while (uart_read_byte() >= 0);

    spi1_init();
    lcd_init();
    lcd_clear(BLACK);

    FATFS fs;
    if (f_mount(&fs, "0:", 1) == FR_OK) {
        if (load_bmp("/photo.bmp")) {
            pio_flush();
        }
    }

    char cmd[16]; int pos = 0;
    while (1) {
        int ch = uart_read_byte();
        if (ch < 0) { sleep_ms(10); continue; }
        if (ch == '\r' || ch == '\n') {
            cmd[pos] = '\0';
            if (strcmp(cmd, "reboot") == 0) { sleep_ms(100); reset_usb_boot(0, 0); }
            pos = 0;
        } else if (ch >= ' ' && pos < 15) {
            cmd[pos++] = (char)ch;
        }
    }
    return 0;
}
