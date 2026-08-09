cmake_minimum_required(VERSION 3.13)
include(../../sdk_project.cmake)
include(../pico_sdk_import.cmake)
project(03_photo C CXX ASM)
drp_project(03_photo
    BSP LCD LED SPI UART SDIO SHELL bmp
    NEED_FATFS
    LIBS pico_stdlib hardware_spi hardware_uart hardware_irq hardware_dma hardware_watchdog
)
