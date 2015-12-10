#!/bin/bash

# Copyright (c) 2015, IBM
# Author(s): Dan Williams <djwillia@us.ibm.com>
#
# Permission to use, copy, modify, and/or distribute this software for
# any purpose with or without fee is hereby granted, provided that the
# above copyright notice and this permission notice appear in all
# copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
# WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
# AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
# DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
# OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
# TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
# PERFORMANCE OF THIS SOFTWARE.

SZ="0x`objdump -h ../loader/loader |grep bss | awk '{print $3}'`"
START="0x`objdump -h ../loader/loader |grep bss | awk '{print $4}'`"
ADDR=`printf "0x%x000\n" $(($(($SZ + $START - 1)) / 0x1000 + 3))`

cat<<EOF
/* WARNING: this file is generated from gen_ld.bash */

ENTRY(kernel_main)

SECTIONS {


    /* GRUB will put two things in memory: the 32-bit loader (kernel
       from GRUB's perspective) goes at 1MB. It has been putting the
       first module (64-bit kernel elf file) at 0x108000, which means
       the text segment (aligned on a 4K page boundary) is at this
       address.  As long as it continues to do so, we'll be OK and ELF
       parsing in the loader is pretty much unnecessary. */

    . = $ADDR;

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
EOF
