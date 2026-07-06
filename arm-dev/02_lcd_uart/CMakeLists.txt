# ---------------------------------------------------------------------------
#  DNRP2350A  SDK 工程: 02_lcd_uart  (ARM Cortex-M33)
# ---------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD        pico2           CACHE STRING "Board type")
set(PICO_PLATFORM     rp2350-arm-s    CACHE STRING "Platform")

include(../pico_sdk_import.cmake)

project(02_lcd_uart C CXX ASM)
pico_sdk_init()

# ---- 源文件 -----------------------------------------------------------------
file(GLOB_RECURSE SRC_FILES BSP/*.c)
add_executable(${PROJECT_NAME}
    main.c
    ${SRC_FILES}
)

# ---- 链接库 -----------------------------------------------------------------
target_link_libraries(${PROJECT_NAME}
    pico_stdlib
    hardware_spi
    hardware_uart
    hardware_pwm
    hardware_irq
)

# ---- 输出格式 ---------------------------------------------------------------
pico_add_extra_outputs(${PROJECT_NAME})

# ---- 串口映射 ---------------------------------------------------------------
pico_enable_stdio_usb(${PROJECT_NAME}  0)
pico_enable_stdio_uart(${PROJECT_NAME} 0)
