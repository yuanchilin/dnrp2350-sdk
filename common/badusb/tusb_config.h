/**
 * @file    tusb_config.h
 * @brief   TinyUSB 配置 — HID 键盘 (仅 Device 模式)
 */

#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

#define CFG_TUSB_RHPORT0_MODE     OPT_MODE_DEVICE
#define CFG_TUSB_OS               OPT_OS_PICO
#define CFG_TUSB_MEM_SECTION
#define CFG_TUSB_DEBUG            0

/* ---- Device 模式 ---- */
#define CFG_TUD_ENABLED           1
#define CFG_TUD_MAX_SPEED         OPT_MODE_DEFAULT_SPEED

/* HID 键盘 */
#define CFG_TUD_HID               1
#define CFG_TUD_HID_EP_BUFSIZE    64

/* CDC (关闭 — SDK stdio 会覆盖描述符) */
#define CFG_TUD_CDC               0
#define CFG_TUD_MSC               0
#define CFG_TUD_MIDI              0
#define CFG_TUD_VENDOR            0

#ifdef __cplusplus
}
#endif

#endif
