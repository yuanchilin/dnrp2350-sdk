/**
 * @file    uart.h
 * @brief   DNRP2350A UART0 驱动 (CH343 USB 转串口)
 * @note    UART0: TX=GPIO0, RX=GPIO1, 接板载 CH343 → USB_UART 口
 */

#ifndef __BSP_UART_H
#define __BSP_UART_H

#include "pico/stdlib.h"
#include "hardware/uart.h"

/* ========================================================================== */
/*  硬件定义                                                                   */
/* ========================================================================== */
#define UART_PORT       uart0
#define UART_TX_PIN     0
#define UART_RX_PIN     1
#define UART_BAUDRATE   115200

/* ========================================================================== */
/*  RX 环形缓冲区                                                              */
/* ========================================================================== */
#define UART_RX_BUF_SIZE    256

/* ========================================================================== */
/*  API                                                                        */
/* ========================================================================== */
void uart_init_dev(void);                       /* 初始化 UART0 */
void uart_send_byte(uint8_t data);
void uart_send_string(const char *str);
void uart_send_buf(const uint8_t *buf, uint16_t len);
int  uart_rx_available(void);                   /* 返回可读字节数 */
int  uart_read_byte(void);                      /* 读一个字节，无数据返回 -1 */
void uart_flush(void);                          /* 清空接收缓冲残留 (开机噪声/脏数据) */
void uart_printf(const char *fmt, ...);         /* 简易格式化输出 */

#endif /* __BSP_UART_H */
