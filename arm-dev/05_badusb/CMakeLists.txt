cmake_minimum_required(VERSION 3.13)
include(../../sdk_project.cmake)
include(../pico_sdk_import.cmake)
project(05_badusb C CXX ASM)
drp_project(05_badusb
    BSP LCD LED SPI UART SDIO KEY
    SRCS hid_keyboard.c payload.c
    NEED_FATFS
    LIBS pico_stdlib hardware_spi hardware_uart hardware_irq hardware_dma tinyusb_device
    DEFS CFG_TUSB_CONFIG_FILE=\"tusb_config.h\"
)
