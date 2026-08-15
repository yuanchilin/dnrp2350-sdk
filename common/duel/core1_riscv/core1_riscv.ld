/* core1_riscv.ld — RISC-V Core1 裸机链接脚本 (运行于 DUEL_CORE1_CODE) */
OUTPUT_ARCH(riscv)
ENTRY(core1_entry)

MEMORY {
    RAM (rwx) : ORIGIN = 0x20050000, LENGTH = 64K
}

SECTIONS {
    .text : {
        *(.text.entry)      /* core1_entry 位于二进制起始 → 入口 = 0x20050000 */
        *(.text*)
    } > RAM
    .rodata : { *(.rodata*) } > RAM
    .data : { *(.data*) } > RAM
    .bss (NOLOAD) : { *(.bss*) *(COMMON) } > RAM
}
