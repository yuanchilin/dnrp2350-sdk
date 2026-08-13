/**
 * @file    hid_keyboard.c
 * @brief   USB HID 键盘 — TinyUSB HID 报告
 */

#include "hid_keyboard.h"
#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include "tusb.h"
#include "device/usbd.h"
#include <string.h>

/* 键盘扫描码 → USB HID Usage ID 映射 (仅可打印 ASCII) */
static const uint8_t ascii_to_hid[128] = {
    ['a']=0x04,['b']=0x05,['c']=0x06,['d']=0x07,['e']=0x08,['f']=0x09,
    ['g']=0x0A,['h']=0x0B,['i']=0x0C,['j']=0x0D,['k']=0x0E,['l']=0x0F,
    ['m']=0x10,['n']=0x11,['o']=0x12,['p']=0x13,['q']=0x14,['r']=0x15,
    ['s']=0x16,['t']=0x17,['u']=0x18,['v']=0x19,['w']=0x1A,['x']=0x1B,
    ['y']=0x1C,['z']=0x1D,
    ['1']=0x1E,['2']=0x1F,['3']=0x20,['4']=0x21,['5']=0x22,
    ['6']=0x23,['7']=0x24,['8']=0x25,['9']=0x26,['0']=0x27,
    [' ']=0x2C,['-']=0x2D,['=']=0x2E,['[']=0x2F,[']']=0x30,
    ['\\']=0x31,[';']=0x33,['\'']=0x34,['`']=0x35,
    [',']=0x36,['.']=0x37,['/']=0x38,
};

/* HID 键盘报告描述符 (无 Report ID, 与 boot protocol 兼容) */
static const uint8_t report_desc[] = {
    TUD_HID_REPORT_DESC_KEYBOARD()
};

void hid_init(void) {
    tusb_init();
}

static bool _hid_ready = false;

bool hid_ready(void) {
    return _hid_ready;
}

void hid_task(void) {
    tud_task();
    _hid_ready = tud_hid_ready();
}

uint8_t hid_keycode(uint8_t ch)
{
    if (ch >= 'A' && ch <= 'Z') ch += 32;
    return (ch < 128) ? ascii_to_hid[ch] : 0;
}

/* ---- HID 报告描述符回调 ---- */
uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance) {
    (void)instance;
    return report_desc;
}

/* ---- HID 报告回调 ---- */
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                                hid_report_type_t report_type, uint8_t *buffer,
                                uint16_t reqlen) {
    (void)instance; (void)report_id; (void)report_type; (void)buffer; (void)reqlen;
    return 0;
}
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                           hid_report_type_t report_type,
                           uint8_t const *buffer, uint16_t bufsize) {
    (void)instance; (void)report_id; (void)report_type; (void)buffer; (void)bufsize;
}

/* ---- USB 设备描述符 ---- */
static const tusb_desc_device_t device_desc = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0,
    .bDeviceSubClass    = 0,
    .bDeviceProtocol    = 0,
    .bMaxPacketSize0    = 64,
    .idVendor           = 0xCafe,
    .idProduct          = 0x4001,
    .bcdDevice          = 0x0100,
    .iManufacturer      = 1,
    .iProduct           = 2,
    .iSerialNumber      = 0,
    .bNumConfigurations = 1,
};

uint8_t const *tud_descriptor_device_cb(void) {
    return (uint8_t const *)&device_desc;
}

/* ---- USB 配置描述符 (纯 HID Keyboard, 无 CDC) ---- */
enum { ITF_NUM_HID = 0, ITF_NUM_TOTAL };

#define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)
#define EPNUM_HID         0x81
#define HID_REPORT_LEN    8

static const uint8_t config_desc[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN,
                          TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_HID_DESCRIPTOR(ITF_NUM_HID, 0, 1 /* keyboard boot */,
                       sizeof(report_desc), EPNUM_HID,
                       CFG_TUD_HID_EP_BUFSIZE, 10),
};

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    return config_desc;
}

/* ---- USB 字符串描述符 ---- */
static const char *string_desc_arr[] = {
    (const char[]){0x09, 0x04},  // 0: language ID
    "RP2350 Lab",                 // 1: manufacturer
    "BadUSB Injector",            // 2: product
};

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;
    static uint16_t buf[32];
    /* 语言描述符 (index 0): 固定 4 字节 {STRING, len=4, 0x09, 0x04},
     * 不能对二进制字面量调 strlen (无 NUL 结尾, 会越界读到垃圾长度) */
    if (index == 0) {
        buf[0] = (TUSB_DESC_STRING << 8) | 0x04;
        buf[1] = 0x0409;                            /* English (US) */
        return buf;
    }
    if (index >= sizeof(string_desc_arr) / sizeof(string_desc_arr[0]))
        return NULL;
    const char *str = string_desc_arr[index];
    uint8_t len = (uint8_t)strlen(str);
    if (len > 31) len = 31;                         /* 静态缓冲上限 */
    buf[0] = (TUSB_DESC_STRING << 8) | (len * 2);
    for (uint8_t i = 0; i < len; i++) {
        buf[1 + i] = (uint16_t)str[i];
    }
    return buf;
}

/* ========================================================================== */
/*  发送接口                                                                   */
/* ========================================================================== */
void hid_key_press_multi(uint8_t modifiers, const uint8_t *keycodes, uint8_t count) {
    if (!_hid_ready) return;
    uint8_t report[8] = { modifiers, 0, 0, 0, 0, 0, 0, 0 };
    if (count > 6) count = 6;                       /* 键盘报告最多 6 个非修饰键 */
    for (uint8_t i = 0; i < count; i++) report[2 + i] = keycodes[i];
    tud_hid_report(0, report, sizeof(report));
}

void hid_key_press(uint8_t modifiers, uint8_t keycode) {
    uint8_t keys[1] = { keycode };
    hid_key_press_multi(modifiers, keys, 1);
}

void hid_key_release(void) {
    if (!_hid_ready) return;
    uint8_t report[8] = { 0 };
    /* 注意: 此 TinyUSB 版本 tud_hid_report 第一个参数是 report_id (instance 固定为 0)。
     * 传 1 会把 0x01 前插进报告并多发 1 字节, 主机按无 Report ID 的描述符解析时
     * 会把每次松键误判成 "左 Ctrl 被按住"。 */
    tud_hid_report(0, report, sizeof(report));
}

void hid_key_tap(uint8_t modifiers, uint8_t keycode) {
    hid_key_press(modifiers, keycode);
    hid_delay_ms(10);   /* 服务 USB 栈, 确保按下报告发出后再释放 (sleep_ms 会饿死 tud_task) */
    hid_key_release();
    hid_delay_ms(10);
}

void hid_string(const char *str) {
    while (*str) {
        char ch = *str;
        bool shift = false;

        if (ch >= 'A' && ch <= 'Z') { ch += 32; shift = true; }  /* 大写→小写+SHIFT */
        /* 特殊符号 */
        switch (ch) {
            case '!': ch='1'; shift=true; break;
            case '@': ch='2'; shift=true; break;
            case '#': ch='3'; shift=true; break;
            case '$': ch='4'; shift=true; break;
            case '%': ch='5'; shift=true; break;
            case '^': ch='6'; shift=true; break;
            case '&': ch='7'; shift=true; break;
            case '*': ch='8'; shift=true; break;
            case '(': ch='9'; shift=true; break;
            case ')': ch='0'; shift=true; break;
            case '_': ch='-'; shift=true; break;
            case '+': ch='='; shift=true; break;
            case '{': ch='['; shift=true; break;
            case '}': ch=']'; shift=true; break;
            case '|': ch='\\'; shift=true; break;
            case ':': ch=';'; shift=true; break;
            case '"': ch='\''; shift=true; break;
            case '~': ch='`'; shift=true; break;
            case '<': ch=','; shift=true; break;
            case '>': ch='.'; shift=true; break;
            case '?': ch='/'; shift=true; break;
        }

        uint8_t code = (ch < 128) ? ascii_to_hid[(int)ch] : 0;
        if (code) {
            hid_key_press(shift ? KEY_MOD_SHIFT : 0, code);
            hid_delay_ms(15);                       /* 延迟期间保持 USB 栈服务 */
            hid_key_release();
            hid_delay_ms(15);
        }
        str++;
    }
}

void hid_delay_ms(uint32_t ms) {
    uint32_t start = to_ms_since_boot(get_absolute_time());
    while (to_ms_since_boot(get_absolute_time()) - start < ms) {
        hid_task();
        watchdog_update();                          /* 长脚本注入期间喂狗 */
        sleep_ms(1);
    }
}
