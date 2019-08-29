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

    . = ALIGN(CONSTANT(MAXPAGESIZE));
    _etext = .;

    /* Read-only data */
    .rodata :
    {
        *(.rodata)
        *(.rodata.*)
    }
    /* For Hvt, the manifest NOTE(s) are read-only. */
    .note.solo5.manifest :
    {
        *(.note.solo5.manifest*)
    }
    .eh_frame :
    {
        *(.eh_frame)
    }

    . = ALIGN(CONSTANT(MAXPAGESIZE));
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
    .tdata :
    {
        *(.tdata)
    }

    . = ALIGN(CONSTANT(MAXPAGESIZE));
    _edata = .;

    /* Read-write data (uninitialized) */
    .tbss :
    {
        *(.tbss)
    }
    .bss :
    {
        *(.bss)
        *(COMMON)
    }

    . = ALIGN(CONSTANT(MAXPAGESIZE));
    _ebss = .;
    _end = .;
}
