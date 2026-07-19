/**
 * @file    hid_keyboard.h
 * @brief   USB HID 键盘 — TinyUSB 设备端封装
 */

#ifndef __HID_KEYBOARD_H
#define __HID_KEYBOARD_H

#include <stdint.h>
#include <stdbool.h>

/* 修饰键掩码 */
#define KEY_MOD_CTRL    0x01
#define KEY_MOD_SHIFT   0x02
#define KEY_MOD_ALT     0x04
#define KEY_MOD_GUI     0x08

/* 非打印键 (USB HID Usage ID) */
#define HID_KEY_ENTER       0x28
#define HID_KEY_ESCAPE      0x29
#define HID_KEY_BACKSPACE   0x2A
#define HID_KEY_TAB         0x2B
#define HID_KEY_SPACE       0x2C
#define HID_KEY_CAPS_LOCK   0x39
#define HID_KEY_F1          0x3A
#define HID_KEY_F12         0x45
#define HID_KEY_RIGHT       0x4F
#define HID_KEY_LEFT        0x50
#define HID_KEY_DOWN        0x51
#define HID_KEY_UP          0x52
#define HID_KEY_DELETE      0x4C
#define HID_KEY_HOME        0x4A
#define HID_KEY_END         0x4D

/* API */
void hid_init(void);
bool hid_ready(void);                   /* USB 已枚举? */
void hid_task(void);                    /* TinyUSB 设备任务 (需在主循环调用) */

/* 发送按键 */
void hid_key_press(uint8_t modifiers, uint8_t keycode);
void hid_key_release(void);
void hid_key_tap(uint8_t modifiers, uint8_t keycode);  /* 按下→释放 */
void hid_string(const char *str);                       /* 逐字发送字符串 */
void hid_delay_ms(uint32_t ms);                         /* 延迟 (保持键盘心跳) */

#endif
