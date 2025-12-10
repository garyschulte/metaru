/* Linker script for Zisk RISC-V bare metal EVM demo */

OUTPUT_FORMAT("elf64-littleriscv")
OUTPUT_ARCH("riscv")
ENTRY(_start)

MEMORY {
    rom (xa)  : ORIGIN = 0x80000000, LENGTH = 0x10000000
    ram (wxa) : ORIGIN = 0xa0020000, LENGTH = 0x1FFE0000
}

PHDRS {
    text PT_LOAD FLAGS(5);
    rodata PT_LOAD FLAGS(4);
    data PT_LOAD FLAGS(6);
    bss PT_LOAD FLAGS(6);
}

SECTIONS
{
    .text : {
        *(.text.init)
        *(.text .text.*)
    } >rom AT>rom :text

    . = ALIGN(8);
    PROVIDE(_global_pointer = .);

    .rodata : {
        *(.rodata .rodata.*)
    } >rom AT>rom :rodata

    .data : {
        __data_start = .;
        *(.data .data.*)
        *(.sdata .sdata.*)
        __data_end = .;

        /* C++ global constructor/destructor arrays */
        . = ALIGN(8);
        __init_array_start = .;
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
        KEEP(*(.init_array))
        __init_array_end = .;

        . = ALIGN(8);
        __fini_array_start = .;
        KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*)))
        KEEP(*(.fini_array))
        __fini_array_end = .;
    } >ram AT>rom :data

    .bss : {
        __bss_start = .;
        *(.bss .bss.*)
        *(.sbss .sbss.*)
        *(COMMON)
        __bss_end = .;
    } >ram AT>ram :bss

    . = ALIGN(8);
    __heap_start = .;

    /* Stack starts 1MB after current position */
    . = . + 0x100000;
    __stack_top = .;

    _kernel_heap_bottom = __stack_top;
    _kernel_heap_top = ORIGIN(ram) + LENGTH(ram);
    _kernel_heap_size = _kernel_heap_top - _kernel_heap_bottom;

    _end = .;
}
