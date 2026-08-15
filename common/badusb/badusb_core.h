/**
 * @file    badusb_core.h
 * @brief   BadUSB 注入核心 — SD 脚本扫描 / 内置文件写入 / 注入执行
 *          05 与 08 共用, 消除各自重复实现。
 */

#ifndef __BADUSB_CORE_H
#define __BADUSB_CORE_H

#include <stdint.h>
#include <stdbool.h>

#define BADUSB_NAME_MAX 48

/* 日志回调 (默认走 UART; 可设置后同时镜像到 LCD 终端等) */
typedef void (*badusb_log_fn)(const char *line);
void badusb_set_log(badusb_log_fn fn);

/* 扫描 SD 根目录指定扩展名文件到 list (带前导 '/'), 返回数量 */
int  badusb_scan_ext(const char *ext, char list[][BADUSB_NAME_MAX], int max);

/* 确保内置文件存在且与内置数据大小一致 (不一致时重写) */
bool badusb_ensure_file(const char *path, const uint8_t *data, uint32_t size);

/* 读取脚本 → 等待 USB HID 枚举 → 注入执行 */
bool badusb_inject_file(const char *path);

#endif
