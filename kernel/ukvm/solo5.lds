ENTRY(kernel_main)

SECTIONS {
    . = 0x100000;
    _begin = .;

    .bootstrap :
    {
        *(.bootstrap)
    }

    .text ALIGN(0x1000):
    {
        *(.text)
	*(.text.*)
    }

    .rodata ALIGN(0x1000):
    {
        *(.rodata)
	*(.rodata.*)
    }

    .data ALIGN(0x1000):
    {
        *(.data)
	*(.data.*)
    }
    _edata = .;

    .bss ALIGN(0x1000):
    {
        *(.bss)
	*(.bss.*)
        *(COMMON)
    }
    _ebss = .;
    _end = .;
}
