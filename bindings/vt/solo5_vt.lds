ENTRY(_start)

SECTIONS {
    . = 0x100000;

    /* Code */
    _stext = .;

    .text :
    {
        *(.text)
        *(.text.*)
    }

    . = ALIGN(0x1000);
    _etext = .;

    /* Read-only data */
    .rodata :
    {
        *(.rodata)
        *(.rodata.*)
    }
    .eh_frame :
    {
        *(.eh_frame)
    }

    . = ALIGN(0x1000);
    _erodata = .;

    /* Read-write data (initialized) */
    .got :
    {
        *(.got.plt)
        *(.got)
    }
    .data :
    {
        *(.data)
        *(.data.*)
    }

    . = ALIGN(0x1000);
    _edata = .;

    /* Read-write data (uninitialized) */
    .bss :
    {
        *(.bss)
        *(COMMON)
    }

    . = ALIGN(0x1000);
    _ebss = .;
    _end = .;
}
