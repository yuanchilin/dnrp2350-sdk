# ---------------------------------------------------------------------------
#  DNRP2350A  SDK 工程: 01_led  (ARM Cortex-M33)
# ---------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.13)

# ⚠️ 必须在 include(pico_sdk_import) 之前设置，否则默认 rp2040！
set(PICO_BOARD        pico2           CACHE STRING "Board type")
set(PICO_PLATFORM     rp2350-arm-s    CACHE STRING "Platform")

include(../pico_sdk_import.cmake)

project(01_led C CXX ASM)
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
)

# ---- 输出格式 ---------------------------------------------------------------
pico_add_extra_outputs(${PROJECT_NAME})   # .uf2 / .elf / .bin / .hex

# ---- 串口映射 ---------------------------------------------------------------
pico_enable_stdio_usb(${PROJECT_NAME}  0) # USB CDC 需 TinyUSB submodule
pico_enable_stdio_uart(${PROJECT_NAME} 0) # 硬件 UART 关闭
