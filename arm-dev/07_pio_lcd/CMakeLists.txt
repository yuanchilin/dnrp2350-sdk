# ---------------------------------------------------------------------------
#  DNRP2350A  07_pio_lcd — PIO+DMA 加速 LCD
# ---------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD        pico2           CACHE STRING "Board type")
set(PICO_PLATFORM     rp2350-arm-s    CACHE STRING "Platform")
set(COMMON_DIR        ${CMAKE_CURRENT_LIST_DIR}/../../common)

include(../pico_sdk_import.cmake)

project(07_pio_lcd C CXX ASM)
pico_sdk_init()

file(GLOB BSP_SRC
    ${COMMON_DIR}/BSP/LCD/*.c
    ${COMMON_DIR}/BSP/LED/*.c
    ${COMMON_DIR}/BSP/SPI/*.c
    ${COMMON_DIR}/BSP/UART/*.c
)
add_executable(${PROJECT_NAME} main.c ${BSP_SRC})
pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/lcd_spi.pio)
target_include_directories(${PROJECT_NAME} PRIVATE ${COMMON_DIR} ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(${PROJECT_NAME}
    pico_stdlib
    hardware_spi
    hardware_uart
    hardware_pio
    hardware_dma
    hardware_irq
)

pico_add_extra_outputs(${PROJECT_NAME})
pico_enable_stdio_uart(${PROJECT_NAME} 0)
pico_enable_stdio_usb(${PROJECT_NAME} 0)
