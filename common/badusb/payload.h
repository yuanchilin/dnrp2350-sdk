/**
 * @file    payload.h
 * @brief   载荷脚本解析器
 */

#ifndef __PAYLOAD_H
#define __PAYLOAD_H

#include <stdint.h>

void payload_execute(const uint8_t *data, uint32_t len);

#endif
