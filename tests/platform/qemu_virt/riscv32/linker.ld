OUTPUT_ARCH(riscv)
ENTRY(_start)

PHDRS
{
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
}

MEMORY
{
    ram (rwx) : ORIGIN = 0x80000000, LENGTH = 2M
}

SECTIONS
{
    . = ORIGIN(ram);
    .init : { KEEP(*(.init)) } > ram :text
    .text : { *(.text .text.*) } > ram :text
    .rodata : { *(.rodata .rodata.*) *(.srodata .srodata.*) } > ram :text

    .data :
    {
        *(.data .data.*)
        . = ALIGN(8);
        PROVIDE(__global_pointer$ = . + 0x800);
        *(.sdata .sdata.*)
    } > ram :data

    .bss (NOLOAD) :
    {
        . = ALIGN(8);
        __bss_start = .;
        *(.sbss .sbss.*)
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(8);
        __bss_end = .;
    } > ram :data

    . = ALIGN(16);
    . += 4096;
    __stack_top = .;
    ASSERT(__stack_top < ORIGIN(ram) + LENGTH(ram), "RAM overflow")
}
