# ---------------------------------------------------------------------------
#  DNRP2350A  SDK 工程: 03_photo  (ARM Cortex-M33)
#  迷你相框 — TF卡 BMP → ST7789 LCD
# ---------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD        pico2           CACHE STRING "Board type")
set(PICO_PLATFORM     rp2350-arm-s    CACHE STRING "Platform")
set(COMMON_DIR        ${CMAKE_CURRENT_LIST_DIR}/../../common)

include(../pico_sdk_import.cmake)

project(03_photo C CXX ASM)
pico_sdk_init()

# ---- FatFs 中间件 (仅本项目) ------------------------------------------------
add_subdirectory(Middlewares/FatFs_SPI build)

# ---- 源文件: common BSP + 本地 SDIO + main ----------------------------------
file(GLOB_RECURSE BSP_SRC  ${COMMON_DIR}/BSP/*.c)
file(GLOB_RECURSE SDIO_SRC BSP/SDIO/*.c)
add_executable(${PROJECT_NAME} main.c ${BSP_SRC} ${SDIO_SRC})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${COMMON_DIR}
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/Middlewares/FatFs_SPI/sd_driver
    ${CMAKE_CURRENT_LIST_DIR}/Middlewares/FatFs_SPI/include
    ${CMAKE_CURRENT_LIST_DIR}/Middlewares/FatFs_SPI/ff15/source
)

target_link_libraries(${PROJECT_NAME}
    pico_stdlib hardware_spi hardware_uart hardware_irq hardware_dma
    FatFs_SPI
)

pico_add_extra_outputs(${PROJECT_NAME})
pico_enable_stdio_uart(${PROJECT_NAME} 0)
pico_enable_stdio_usb(${PROJECT_NAME} 0)
