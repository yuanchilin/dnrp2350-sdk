/**
 * @file    bmp.h
 * @brief   24-bit BMP 解码显示 — TF卡 BMP → LCD (ST7789)
 *          供 03_photo 与 shell view 命令共用, 消除重复实现
 */

#ifndef __BMP_H
#define __BMP_H

#include <stdbool.h>

/* 解码并显示 24-bit BMP 到 LCD (缩放适配 + 居中)
 * @param path  BMP 文件路径 (如 "/photo.bmp")
 * @return true=显示成功  false=打开/格式失败 */
bool bmp_show(const char *path);

#endif