# pico_sdk_import.cmake
# 从 pico-sdk 导入 CMake 构建系统
#
# 用法: 在工程 CMakeLists.txt 中 include(../pico_sdk_import.cmake)
#
# 注意: RISC-V 编译需要 riscv32-unknown-elf-gcc 工具链
#       当前环境未安装，本目录为预留骨架

# 从当前文件位置向上两级找到 pico-sdk 根目录
get_filename_component(_PICO_SDK_PARENT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(PICO_SDK_PATH "${_PICO_SDK_PARENT}/pico-sdk" CACHE PATH "PICO-SDK root")

if(NOT EXISTS "${PICO_SDK_PATH}/pico_sdk_init.cmake")
    message(FATAL_ERROR "PICO_SDK_PATH not found: ${PICO_SDK_PATH}\n"
                        "Run: git clone https://github.com/raspberrypi/pico-sdk.git\n"
                        "(expected at ${_PICO_SDK_PARENT}/pico-sdk)")
endif()

unset(_PICO_SDK_PARENT)

include("${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
