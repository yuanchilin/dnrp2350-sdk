/**
 * @file    pio_lcd.c
 * @brief   PIO+DMA LCD 驱动实现
 */

#include "pio_lcd.h"
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/sync.h"
#include "lcd_spi.pio.h"
#include "BSP/LCD/lcd.h"
#include "BSP/SPI/spi.h"

#define MOSI    11
#define SCK     10
#define FB_SIZE (LCD_W * LCD_H * 2)

static uint8_t  fb[FB_SIZE];
static int      pio_sm = -1, dma_ch = -1;
static bool     ok = false;
static uint32_t packed[(FB_SIZE + 3) / 4];

extern const unsigned char asc2_1608[95][16];

void pio_lcd_char(int col, int row, char ch, uint16_t fg, uint16_t bg)
{
    int px = col * 8, py = row * 16;
    int fi = ch - ' '; if (fi < 0 || fi >= 95) fi = 0;
    uint8_t fhi = fg >> 8, flo = fg & 0xFF, bhi = bg >> 8, blo = bg & 0xFF;
    for (int fy = 0; fy < 16; fy++) {
        int idx = (py + fy) * LCD_W * 2;
        uint8_t r = asc2_1608[fi][fy];
        for (int fx = 0; fx < 8; fx++) {
            bool on = (r & (0x80 >> fx)) != 0;
            int i = idx + (px + fx) * 2;
            if (on) { fb[i] = fhi; fb[i + 1] = flo; }
            else    { fb[i] = bhi; fb[i + 1] = blo; }
        }
    }
}

void pio_lcd_flush(int x, int y, int w, int h)
{
    if (!ok) return;
    int nbytes = w * h * 2, nwords = (nbytes + 3) / 4;
    for (int i = 0; i < nwords; i++) {
        uint32_t wd = 0;
        for (int k = 0; k < 4; k++) {
            int bi = i * 4 + k;
            if (bi >= nbytes) break;
            int ry = bi / (w * 2), rem = bi % (w * 2), rx = rem / 2, bo = rem & 1;
            uint8_t b = fb[((y + ry) * LCD_W + x + rx) * 2 + bo];
            wd |= ((uint32_t)b) << (24 - k * 8);
        }
        packed[i] = wd;
    }
    pio_sm_set_enabled(pio0, pio_sm, false);
    pio_sm_restart(pio0, pio_sm);
    pio_sm_clear_fifos(pio0, pio_sm);
    lcd_set_window(x, y, x + w - 1, y + h - 1);
    lcd_write_cmd(0x2C);
    LCD_WR(1); LCD_CS(0);
    gpio_set_function(MOSI, GPIO_FUNC_PIO0);
    gpio_set_function(SCK,  GPIO_FUNC_PIO0);
    pio_sm_set_enabled(pio0, pio_sm, true);
    __dsb();
    dma_channel_set_read_addr(dma_ch, packed, false);
    dma_channel_set_trans_count(dma_ch, nwords, true);
    uint32_t t0 = to_ms_since_boot(get_absolute_time());
    while (dma_channel_is_busy(dma_ch)) {
        if (to_ms_since_boot(get_absolute_time()) - t0 > 500) {
            dma_channel_abort(dma_ch);
            break;
        }
    }
    LCD_CS(1);
    gpio_set_function(MOSI, GPIO_FUNC_SPI);
    gpio_set_function(SCK,  GPIO_FUNC_SPI);
}

bool pio_lcd_ok(void) { return ok; }

bool pio_lcd_init(void)
{
    if (!pio_can_add_program(pio0, &lcd_spi8_program)) return false;
    int off = pio_add_program(pio0, &lcd_spi8_program);
    pio_sm = pio_claim_unused_sm(pio0, false);
    if (pio_sm < 0) return false;

    lcd_spi8_program_init(pio0, pio_sm, off, MOSI, SCK);
    pio_sm_set_clkdiv(pio0, pio_sm, 6.0f);
    pio_sm_set_enabled(pio0, pio_sm, true);

    dma_ch = dma_claim_unused_channel(false);
    if (dma_ch < 0) return false;

    dma_channel_config dcfg = dma_channel_get_default_config(dma_ch);
    channel_config_set_transfer_data_size(&dcfg, DMA_SIZE_32);
    channel_config_set_read_increment(&dcfg, true);
    channel_config_set_write_increment(&dcfg, false);
    channel_config_set_dreq(&dcfg, pio_get_dreq(pio0, pio_sm, true));
    dma_channel_configure(dma_ch, &dcfg, &pio0->txf[pio_sm], packed, (FB_SIZE + 3) / 4, false);

    ok = true;
    return true;
}