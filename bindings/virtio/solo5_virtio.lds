ENTRY(_start)

SECTIONS {
    . = 0x100000;

    /* Code */
    _stext = .;

    .text :
    {
        *(.data.multiboot)
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
    .note.solo5 :
    {
        *(.note.solo5*)
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
