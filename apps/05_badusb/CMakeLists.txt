cmake_minimum_required(VERSION 3.13)
include(../../sdk_project.cmake)
include(../pico_sdk_import.cmake)
project(05_badusb C CXX ASM)
drp_project(05_badusb
    BSP BOARD LCD LED SPI UART SDIO KEY BADUSB
    NEED_FATFS
    LIBS pico_stdlib hardware_spi hardware_uart hardware_irq hardware_dma
)
