/* Auto-generated sample BadUSB payload */
static const char sample_payload[] =
    "REM === BadUSB Demo ===\r\n"
    "REM 中文输入法规避: ALT SHIFT 把键盘布局切到英文 (目标已是英文布局时可删掉本行)\r\n"
    "ALT SHIFT\r\n"
    "DELAY 100\r\n"
    "GUI r\r\n"
    "DELAY 300\r\n"
    "STRING notepad\r\n"
    "ENTER\r\n"
    "DELAY 500\r\n"
    "STRING Hello from RP2350 BadUSB!\r\n"
    "ENTER\r\n"
    "STRING This was injected via USB HID.\r\n"
    "ENTER\r\n"
    "STRING DNRP2350A | TinyUSB | FAT32\r\n"
    ;
#define SAMPLE_PAYLOAD_SIZE (sizeof(sample_payload) - 1)
