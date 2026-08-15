cmake_minimum_required(VERSION 3.13)
include(../../sdk_project.cmake)
include(../pico_sdk_import.cmake)
project(02_lcd_uart C CXX ASM)
drp_project(02_lcd_uart
    BSP BOARD LCD LED SPI UART SHELL
    LIBS pico_stdlib hardware_spi hardware_uart hardware_irq hardware_watchdog
)
