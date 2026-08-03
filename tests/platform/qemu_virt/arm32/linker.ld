OUTPUT_ARCH(arm)
ENTRY(Reset_Handler)

MEMORY
{
    flash (rx)  : ORIGIN = 0x00000000, LENGTH = 256K
    sram  (rwx) : ORIGIN = 0x20000000, LENGTH = 64K
}

SECTIONS
{
    .isr_vector :
    {
        KEEP(*(.isr_vector))
    } > flash

    .text :
    {
        *(.text .text.*)
        *(.rodata .rodata.*)
        . = ALIGN(4);
    } > flash

    .data :
    {
        . = ALIGN(4);
        __data_start = .;
        *(.data .data.*)
        . = ALIGN(4);
        __data_end = .;
    } > sram AT> flash
    __data_load = LOADADDR(.data);

    .bss (NOLOAD) :
    {
        . = ALIGN(4);
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > sram

    __stack_top = ORIGIN(sram) + LENGTH(sram);
    ASSERT(__bss_end < __stack_top, "SRAM overflow")
}
