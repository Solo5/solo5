/* WARNING: this file is generated from gen_ld.bash */

ENTRY(kernel_main)

SECTIONS {


    /* GRUB will put two things in memory: the 32-bit loader (kernel
       from GRUB's perspective) goes at 1MB. It has been putting the
       first module (64-bit kernel elf file) at 0x108000, which means
       the text segment (aligned on a 4K page boundary) is at this
       address.  As long as it continues to do so, we'll be OK and ELF
       parsing in the loader is pretty much unnecessary. */

    . = 0x10a000;

	.text BLOCK(4K) : ALIGN(4K)
	{
		*(.text)
	}

	/* Read-only data. */
	.rodata BLOCK(4K) : ALIGN(4K)
	{
		*(.rodata)
	}

	/* Read-write data (initialized) */
	.data BLOCK(4K) : ALIGN(4K)
	{
		*(.data)
	}

	/* Read-write data (uninitialized) */
    bss_start = .;
	.bss BLOCK(4K) : ALIGN(4K)
	{
		*(COMMON)
		*(.bss)
	}
    bss_end = .;

	/* The compiler may produce other sections, by default it will put them in
	   a segment with the same name. Simply add stuff here as needed. */
}
