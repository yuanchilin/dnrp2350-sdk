/**
 * @file    main.c
 * @brief   PIO+DMA 加速 LCD — 先 CPU 基准, 再 PIO 加速
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "lcd_spi.pio.h"
#include "BSP/LCD/lcd.h"
#include "BSP/LED/led.h"
#include "BSP/UART/uart.h"

#define LCD_W 240
#define LCD_H 135
#define TOTAL_PX (LCD_W * LCD_H)
#define BUF_SIZE (TOTAL_PX * 2)

#define PIO_INST  pio0
#define MOSI_PIN  11
#define SCK_PIN   10

static uint8_t fb[BUF_SIZE];

/* ========================================================================== */
/*  CPU 填充 (基准)                                                            */
/* ========================================================================== */
static uint32_t cpu_fill(uint16_t color)
{
    uint8_t hi = color >> 8, lo = color & 0xFF;
    absolute_time_t t0 = get_absolute_time();

    lcd_set_window(0, 0, LCD_W - 1, LCD_H - 1);
    lcd_write_cmd(0x2C);
    LCD_WR(1); LCD_CS(0);
    for (int i = 0; i < TOTAL_PX; i++) {
        uint8_t b[2] = {hi, lo};
        spi_write_blocking(spi1, b, 2);
    }
    LCD_CS(1);

    return absolute_time_diff_us(t0, get_absolute_time());
}

/* ========================================================================== */
/*  PIO+DMA 填充                                                               */
/* ========================================================================== */
static int  pio_sm  = -1;
static int  pio_ofs = -1;
static int  dma_ch  = -1;
static bool pio_ok  = false;

static bool pio_dma_init(void)
{
    if (pio_ok) return true;

    if (!pio_can_add_program(PIO_INST, &lcd_spi8_program)) {
        uart_printf("PIO: program too big\n");
        return false;
    }
    pio_ofs = pio_add_program(PIO_INST, &lcd_spi8_program);

    pio_sm = pio_claim_unused_sm(PIO_INST, false);
    if (pio_sm < 0) { uart_printf("PIO: no free SM\n"); return false; }

    lcd_spi8_program_init(PIO_INST, pio_sm, pio_ofs, MOSI_PIN, SCK_PIN);

    /* PIO 时钟: 150MHz/2 = 75MHz */
    pio_sm_set_clkdiv(PIO_INST, pio_sm, 2.0f);

    /* DMA: framebuf → PIO TX FIFO */
    dma_ch = dma_claim_unused_channel(false);
    if (dma_ch < 0) { uart_printf("PIO: no DMA ch\n"); return false; }

    dma_channel_config cfg = dma_channel_get_default_config(dma_ch);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_8);
    channel_config_set_read_increment(&cfg, true);
    channel_config_set_write_increment(&cfg, false);
    channel_config_set_dreq(&cfg, pio_get_dreq(PIO_INST, pio_sm, true));
    dma_channel_configure(dma_ch, &cfg,
                          &PIO_INST->txf[pio_sm],
                          fb, BUF_SIZE, false);

    uart_printf("PIO+DMA init OK (sm=%d ofs=%d dma=%d)\n", pio_sm, pio_ofs, dma_ch);
    pio_ok = true;
    return true;
}

static uint32_t pio_fill(uint16_t color)
{
    if (!pio_ok && !pio_dma_init()) return 0;

    /* 帧缓冲 32位快速填充 */
    uint16_t c = color;
    uint32_t w = ((uint32_t)c << 16) | c;   /* 两个像素打包成一个 32位字 */
    uint32_t *p = (uint32_t *)fb;
    for (int i = 0; i < BUF_SIZE / 4; i++) p[i] = w;

    /* SPI1 → CPU 发命令 */
    lcd_set_window(0, 0, LCD_W - 1, LCD_H - 1);
    lcd_write_cmd(0x2C);
    LCD_WR(1); LCD_CS(0);

    /* 引脚切给 PIO */
    gpio_set_function(MOSI_PIN, GPIO_FUNC_PIO0);
    gpio_set_function(SCK_PIN,  GPIO_FUNC_PIO0);

    absolute_time_t t0 = get_absolute_time();

    /* DMA 从 fb 搬运 → PIO TX FIFO → SPI 输出 */
    dma_channel_set_read_addr(dma_ch, fb, false);
    dma_channel_set_trans_count(dma_ch, BUF_SIZE, true);
    dma_channel_wait_for_finish_blocking(dma_ch);

    uint32_t us = absolute_time_diff_us(t0, get_absolute_time());

    LCD_CS(1);

    /* 引脚还给 SPI1 */
    gpio_set_function(MOSI_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SCK_PIN,  GPIO_FUNC_SPI);

    return us;
}

/* ========================================================================== */
int main(void)
{
    /* 极早诊断: LED 闪 3 次 = CPU 活着 */
    led_init();
    for (int i = 0; i < 3; i++) { LED(0); sleep_ms(200); LED(1); sleep_ms(200); }

    uart_init_dev();
    sleep_ms(100);
    while (uart_read_byte() >= 0);

    /* 串口直接写 — 绕过 stdio */
    uart_printf("\nBOOT OK\n");
    spi1_init();               /* SPI1 必须先初始化! */
    uart_printf("SPI1 OK\n");
    lcd_init();
    uart_printf("LCD OK\n");

    uart_printf("\n========================================\n");
    uart_printf(" PIO+DMA LCD Benchmark\n");
    uart_printf(" %dx%d RGB565 = %d bytes\n", LCD_W, LCD_H, BUF_SIZE);
    uart_printf("========================================\n");

    uint16_t colors[] = {RED, GREEN, BLUE, YELLOW, MAGENTA, CYAN};
    int ci = 0;

    while (1) {
        uint16_t c = colors[ci++ % 6];
        LED_TOGGLE();

        uint32_t cpu_us = cpu_fill(c);
        uart_printf("CPU: %6lu us  |  ", cpu_us);
        sleep_ms(300);

        uint32_t pio_us = pio_fill(c);
        if (pio_us > 0) {
            uart_printf("PIO: %6lu us  |  %dx\n", pio_us, (int)(cpu_us / pio_us));
        }
        sleep_ms(300);
    }
    return 0;
}
