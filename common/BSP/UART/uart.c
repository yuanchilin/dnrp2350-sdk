/**
 * @file    uart.c
 * @brief   UART0 驱动实现 (CH343 USB 转串口, 115200-8-N-1)
 */

#include "uart.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* ========================================================================== */
/*  RX 环形缓冲区                                                              */
/* ========================================================================== */
static uint8_t  rx_buf[UART_RX_BUF_SIZE];
static volatile uint16_t rx_head = 0;
static volatile uint16_t rx_tail = 0;

/* UART0 中断回调 */
static void uart_rx_irq(void)
{
    while (uart_is_readable(UART_PORT)) {
        uint8_t ch = uart_getc(UART_PORT);
        uint16_t next = (rx_head + 1) % UART_RX_BUF_SIZE;
        if (next != rx_tail) {              /* 缓冲区未满 */
            rx_buf[rx_head] = ch;
            rx_head = next;
        }
        /* 缓冲区满则丢弃新数据 (保留最旧, 与环形缓冲语义一致) */
    }
}

/* ========================================================================== */
/*  初始化 UART0                                                               */
/* ========================================================================== */
void uart_init_dev(void)
{
    uart_init(UART_PORT, UART_BAUDRATE);

    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    uart_set_format(UART_PORT, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(UART_PORT, true);

    /* 注册 RX 中断 */
    irq_set_exclusive_handler(UART0_IRQ, uart_rx_irq);
    irq_set_enabled(UART0_IRQ, true);
    uart_set_irq_enables(UART_PORT, true, false);   /* RX IRQ on, TX IRQ off */
}

/* ========================================================================== */
/*  发送 API                                                                   */
/* ========================================================================== */
void uart_send_byte(uint8_t data)
{
    uart_putc(UART_PORT, data);
}

void uart_send_string(const char *str)
{
    while (*str) {
        uart_putc(UART_PORT, *str++);
    }
}

void uart_send_buf(const uint8_t *buf, uint16_t len)
{
    uart_write_blocking(UART_PORT, buf, len);
}

/* ========================================================================== */
/*  接收 API                                                                   */
/* ========================================================================== */
int uart_rx_available(void)
{
    return (rx_head - rx_tail + UART_RX_BUF_SIZE) % UART_RX_BUF_SIZE;
}

int uart_read_byte(void)
{
    if (rx_head == rx_tail) return -1;      /* 无数据 */
    uint8_t ch = rx_buf[rx_tail];
    rx_tail = (rx_tail + 1) % UART_RX_BUF_SIZE;
    return ch;
}

/* ========================================================================== */
/*  简易格式化输出 (UART)                                                      */
/* ========================================================================== */
void uart_printf(const char *fmt, ...)
{
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    uart_send_string(buf);
}
