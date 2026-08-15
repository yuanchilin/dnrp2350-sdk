cmake_minimum_required(VERSION 3.13)
include(../../sdk_project.cmake)
include(../pico_sdk_import.cmake)
project(07_pio_lcd C CXX ASM)
drp_project(07_pio_lcd
    BSP BOARD LCD LED SPI UART SDIO SHELL CONSOLE COMMANDS TUI PIO_LCD
    NEED_FATFS
    PIO lcd_spi.pio
    LIBS pico_stdlib hardware_spi hardware_uart hardware_pio hardware_dma hardware_irq
)
