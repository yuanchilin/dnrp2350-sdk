# ---------------------------------------------------------------------------
#  DNRP2350A  03_photo  (ARM Cortex-M33)
# ---------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD        pico2           CACHE STRING "Board type")
set(PICO_PLATFORM     rp2350-arm-s    CACHE STRING "Platform")
set(COMMON_DIR        ${CMAKE_CURRENT_LIST_DIR}/../../common)

include(../pico_sdk_import.cmake)

project(03_photo C CXX ASM)
pico_sdk_init()

add_subdirectory(${COMMON_DIR}/Middlewares/FatFs_SPI build)

file(GLOB BSP_SRC
    ${COMMON_DIR}/BSP/LCD/*.c
    ${COMMON_DIR}/BSP/LED/*.c
    ${COMMON_DIR}/BSP/SPI/*.c
    ${COMMON_DIR}/BSP/UART/*.c
    ${COMMON_DIR}/BSP/SDIO/*.c
)
add_executable(${PROJECT_NAME} main.c ${BSP_SRC})
target_include_directories(${PROJECT_NAME} PRIVATE
    ${COMMON_DIR}
    ${CMAKE_CURRENT_LIST_DIR}
    ${COMMON_DIR}/Middlewares/FatFs_SPI/sd_driver
    ${COMMON_DIR}/Middlewares/FatFs_SPI/include
    ${COMMON_DIR}/Middlewares/FatFs_SPI/ff15/source
)
target_link_libraries(${PROJECT_NAME}
    pico_stdlib hardware_spi hardware_uart hardware_irq hardware_dma
    FatFs_SPI
)

pico_add_extra_outputs(${PROJECT_NAME})
pico_enable_stdio_uart(${PROJECT_NAME} 0)
pico_enable_stdio_usb(${PROJECT_NAME} 0)
