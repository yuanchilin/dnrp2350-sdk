# ============================================================================
#  sdk_project.cmake — DNRP2350A 公共工程模板
#  把各项目重复的 board/pico_sdk_init/BSP glob/链接库/输出开关收敛为一次调用。
#
#  用法 (项目 CMakeLists.txt):
#     include(../pico_sdk_import.cmake)          # SDK 导入 (相对本项目)
#     include(../../sdk_project.cmake)           # 本模板
#     project(<名字> C CXX ASM)                  # 必须在顶层调用 project()
#     drp_project(<名字>
#         BSP    <模块...>                        # shell console commands tui pio_lcd bmp + BSP/LCD LED SPI ...
#         [SRCS  <额外源文件...>]                 # 如 core1.c hid_keyboard.c payload.c
#         [LIBS  <额外链接库...>]                 # pico_multicore hardware_pio tinyusb_device ...
#         [STDIO_UART]                           # 启用 UART stdio (默认关)
#         [STDIO_USB]                            # 启用 USB stdio (默认关)
#         [NEED_FATFS]                           # 需要 SD/FatFs (add_subdirectory + 头文件 + FatFs_SPI)
#         [PIO   <xxx.pio>]                      # 生成 PIO 头文件并加入 include
#         [DEFS  <编译宏...>]
#     )
# ============================================================================

cmake_minimum_required(VERSION 3.13)

# ---- 公共路径 (本文件位于仓库根) ------------------------------------------
get_filename_component(DNRP_COMMON_DIR "${CMAKE_CURRENT_LIST_DIR}/common" ABSOLUTE)

# ---- 默认板型 (各项目可忽略; -D 命令行传入优先) ----------------------------
set(PICO_BOARD        pico2           CACHE STRING "Board type")
set(PICO_PLATFORM     rp2350-arm-s    CACHE STRING "Platform")

# ============================================================================
#  drp_duel_embed — 把 RISC-V Core1 二进制交叉编译并内嵌进 ARM 目标
#
#  用法 (drp_project 内部调用, 宿主一般不需要直接调):
#     drp_duel_embed(<target> <src_dir>)
#
#  流程: riscv-none-elf-gcc 裸机编译 core1_riscv.c → objcopy 成 .bin →
#        目标以 .incbin 内嵌 (duel_core.c 读取 CORE1_BIN_PATH 宏)。
#  依赖: RISC-V 工具链在 PATH (xPack riscv-none-elf-gcc 15.2.0)
# ============================================================================
function(drp_duel_embed TARGET SRC_DIR)
    set(CORE1_RV_GCC     riscv-none-elf-gcc)
    set(CORE1_RV_OBJCOPY riscv-none-elf-objcopy)
    set(CORE1_RV_FLAGS   -march=rv32imac_zicsr -mabi=ilp32 -ffreestanding -O2
                         -nostdlib -I${SRC_DIR})
    set(CORE1_BIN ${CMAKE_CURRENT_BINARY_DIR}/core1_riscv.bin)

    add_custom_command(
        OUTPUT ${CORE1_BIN}
        COMMAND ${CORE1_RV_GCC} ${CORE1_RV_FLAGS}
                -c ${SRC_DIR}/core1_riscv/core1_riscv.c
                -o ${CMAKE_CURRENT_BINARY_DIR}/core1_riscv.o
        COMMAND ${CORE1_RV_GCC} -nostdlib
                -T ${SRC_DIR}/core1_riscv/core1_riscv.ld
                ${CMAKE_CURRENT_BINARY_DIR}/core1_riscv.o
                -o ${CMAKE_CURRENT_BINARY_DIR}/core1_riscv.elf
        COMMAND ${CORE1_RV_OBJCOPY} -O binary
                ${CMAKE_CURRENT_BINARY_DIR}/core1_riscv.elf ${CORE1_BIN}
        DEPENDS ${SRC_DIR}/core1_riscv/core1_riscv.c
                ${SRC_DIR}/core1_riscv/core1_riscv.ld
                ${SRC_DIR}/duel_shared.h
        COMMENT "Building RISC-V core1 binary"
        VERBATIM
    )

    # 内嵌 .incbin 的目标必须在编译前生成 .bin
    get_target_property(_tgt_src ${TARGET} SOURCES)
    foreach(_s IN LISTS _tgt_src)
        if(_s MATCHES "duel_core\\.c$")
            set_source_files_properties(${_s} PROPERTIES OBJECT_DEPENDS ${CORE1_BIN})
        endif()
    endforeach()
    string(REPLACE "\\" "/" CORE1_BIN_PATH_FWD ${CORE1_BIN})
    target_compile_definitions(${TARGET} PRIVATE "CORE1_BIN_PATH=${CORE1_BIN_PATH_FWD}")
endfunction()

function(drp_project PROJ_NAME)
    cmake_parse_arguments(P "NEED_FATFS;STDIO_UART;STDIO_USB" "PIO" "BSP;SRCS;LIBS;DEFS" ${ARGN})

    pico_sdk_init()

    # ---- FatFs 中间件 ------------------------------------------------------
    if(P_NEED_FATFS)
        add_subdirectory(${DNRP_COMMON_DIR}/Middlewares/FatFs_SPI build)
    endif()

    # ---- 收集源文件: main.c + 额外源 + BSP 模块 ----------------------------
    set(_src main.c ${P_SRCS})
    foreach(m IN LISTS P_BSP)
        string(TOLOWER "${m}" _m)
        if(_m STREQUAL "bmp")
            list(APPEND _src ${DNRP_COMMON_DIR}/commands/bmp.c)
            continue()
        endif()
        if(_m STREQUAL "shell")
            set(_m_dir ${DNRP_COMMON_DIR}/shell)
        elseif(_m STREQUAL "console")
            set(_m_dir ${DNRP_COMMON_DIR}/console)
        elseif(_m STREQUAL "commands")
            set(_m_dir ${DNRP_COMMON_DIR}/commands)
        elseif(_m STREQUAL "tui")
            set(_m_dir ${DNRP_COMMON_DIR}/tui)
        elseif(_m STREQUAL "pio_lcd")
            set(_m_dir ${DNRP_COMMON_DIR}/pio_lcd)
        elseif(_m STREQUAL "board")
            set(_m_dir ${DNRP_COMMON_DIR}/board)
        elseif(_m STREQUAL "badusb")
            # BadUSB 注入引擎 (hid_keyboard + payload + 注入核心), 需额外链接 tinyusb_device
            list(APPEND _src
                ${DNRP_COMMON_DIR}/badusb/hid_keyboard.c
                ${DNRP_COMMON_DIR}/badusb/payload.c
                ${DNRP_COMMON_DIR}/badusb/badusb_core.c)
            set(_need_badusb 1)
            continue()
        elseif(_m STREQUAL "badusb_cmd")
            # BadUSB shell 命令层 (依赖 SHELL/CONSOLE/BADUSB)
            list(APPEND _src ${DNRP_COMMON_DIR}/badusb/badusb.c)
            continue()
        elseif(_m STREQUAL "led_cmd")
            # LED shell 命令层 (依赖 SHELL + BSP/LED)
            list(APPEND _src ${DNRP_COMMON_DIR}/BSP/LED/led_cmd.c)
            continue()
        elseif(_m STREQUAL "duel")
            # 异构对战模块: ARM 侧驱动 + shell 命令层 (依赖 SHELL/LCD/UART/MULTICORE)
            # 注意: 对战要求 core0=ARM + core1=RISC-V, 整机 RISC-V (PICO_RISCV=1)
            #       时无意义, 自动跳过该模块并给出提示。
            if(PICO_RISCV)
                message(STATUS "drp_project: 整机 RISC-V 平台, 跳过 DUEL 模块 (对战需 core0=ARM)")
            else()
                list(APPEND _src
                    ${DNRP_COMMON_DIR}/duel/duel_core.c
                    ${DNRP_COMMON_DIR}/duel/duel_cmd.c)
                set(_need_duel 1)
            endif()
            continue()
        else()
            set(_m_dir ${DNRP_COMMON_DIR}/BSP/${m})
        endif()
        # BSP 模块名拼错时立刻报错, 而不是 GLOB 到空目录后静默漏编译
        if(NOT IS_DIRECTORY "${_m_dir}")
            message(FATAL_ERROR "drp_project: 未知 BSP 模块 '${m}' (目录不存在: ${_m_dir})")
        endif()
        file(GLOB _files CONFIGURE_DEPENDS ${_m_dir}/*.c)
        # 独立命令层 (*_cmd.c) 由显式模块名启用, 从 BSP GLOB 中排除
        list(FILTER _files EXCLUDE REGEX ".*/_cmd\\.c$")
        list(APPEND _src ${_files})
    endforeach()

    add_executable(${PROJ_NAME} ${_src})

    # ---- PIO 头文件生成 ----------------------------------------------------
    if(P_PIO)
        pico_generate_pio_header(${PROJ_NAME} ${CMAKE_CURRENT_LIST_DIR}/${P_PIO})
    endif()

    # ---- 头文件目录 --------------------------------------------------------
    target_include_directories(${PROJ_NAME} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${DNRP_COMMON_DIR}
    )
    if(P_NEED_FATFS)
        target_include_directories(${PROJ_NAME} PRIVATE
            ${DNRP_COMMON_DIR}/Middlewares/FatFs_SPI/sd_driver
            ${DNRP_COMMON_DIR}/Middlewares/FatFs_SPI/include
            ${DNRP_COMMON_DIR}/Middlewares/FatFs_SPI/ff15/source
        )
    endif()
    if(P_PIO)
        target_include_directories(${PROJ_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    endif()

    # ---- 链接库 ------------------------------------------------------------
    target_link_libraries(${PROJ_NAME} pico_stdlib ${P_LIBS})
    if(P_NEED_FATFS)
        target_link_libraries(${PROJ_NAME} FatFs_SPI)
    endif()
    if(_need_badusb)
        target_include_directories(${PROJ_NAME} PRIVATE ${DNRP_COMMON_DIR}/badusb)
        target_link_libraries(${PROJ_NAME} tinyusb_device)
        target_compile_definitions(${PROJ_NAME} PRIVATE CFG_TUSB_CONFIG_FILE=\"tusb_config.h\")
    endif()
    if(_need_duel)
        target_include_directories(${PROJ_NAME} PRIVATE ${DNRP_COMMON_DIR}/duel)
        target_link_libraries(${PROJ_NAME} pico_multicore)
        drp_duel_embed(${PROJ_NAME} ${DNRP_COMMON_DIR}/duel)
    endif()

    # ---- 编译宏 ------------------------------------------------------------
    if(P_DEFS)
        target_compile_definitions(${PROJ_NAME} PRIVATE ${P_DEFS})
    endif()

    # ---- 输出与 stdio (默认关闭 UART/USB stdio) ----------------------------
    if(NOT DEFINED P_STDIO_UART)
        set(P_STDIO_UART 0)
    endif()
    if(NOT DEFINED P_STDIO_USB)
        set(P_STDIO_USB 0)
    endif()
    pico_add_extra_outputs(${PROJ_NAME})
    pico_enable_stdio_uart(${PROJ_NAME} ${P_STDIO_UART})
    pico_enable_stdio_usb(${PROJ_NAME}  ${P_STDIO_USB})
endfunction()
