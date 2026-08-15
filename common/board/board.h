/**
 * @file    board.h
 * @brief   板级初始化 — 统一各工程 LED/SPI/LCD/UART 的初始化顺序
 */

#ifndef __BOARD_H
#define __BOARD_H

/* 标准板级初始化: UART(最先就绪) → LED → SPI → LCD, 并清 UART 开机噪声。
 * 各工程在此之后按需追加 key_init/hid_init 等。 */
void board_init(void);

#endif
