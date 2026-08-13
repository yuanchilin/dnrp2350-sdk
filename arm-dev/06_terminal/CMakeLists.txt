cmake_minimum_required(VERSION 3.13)
include(../../sdk_project.cmake)
include(../pico_sdk_import.cmake)
project(06_terminal C CXX ASM)
drp_project(06_terminal
    BSP LCD LED SPI UART SDIO SHELL CONSOLE COMMANDS
    NEED_FATFS
    LIBS pico_stdlib hardware_spi hardware_uart hardware_irq hardware_dma
)
