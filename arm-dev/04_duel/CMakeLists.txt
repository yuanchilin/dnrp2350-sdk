cmake_minimum_required(VERSION 3.13)
include(../../sdk_project.cmake)
include(../pico_sdk_import.cmake)
project(04_duel C CXX ASM)
drp_project(04_duel
    BSP BOARD LCD LED SPI UART SHELL
    SRCS core1.c
    LIBS pico_stdlib pico_multicore hardware_spi hardware_uart hardware_irq hardware_watchdog
)
