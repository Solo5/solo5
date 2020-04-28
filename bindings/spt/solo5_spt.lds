/*
 * Copyright (c) 2015-2019 Contributors as noted in the AUTHORS file
 *
 * This file is part of Solo5, a sandboxed execution environment.
 *
 * Permission to use, copy, modify, and/or distribute this software
 * for any purpose with or without fee is hereby granted, provided
 * that the above copyright notice and this permission notice appear
 * in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This is the custom linker script for the Solo5 'spt' target.
 *
 * The script is tested to work with a minimal set of input sections. If there
 * are unexpected input sections not named here, the result will probably not be
 * correct.
 */
TEXT_START = 0x100000;

ENTRY(_start)

/*
 * Program headers: In order to force the linker to place each of our NOTEs
 * into a separate PT_NOTE header, we need lay these out explicitly.
 */
PHDRS {
    interp PT_INTERP;
    text PT_LOAD FLAGS(5); /* No FILEHDR or PHDRS, force R/E only.
                              FLAGS values come from PF_x in elf.h */
    data PT_LOAD;
    note.not-openbsd PT_NOTE; /* Must come first. */
    note.abi PT_NOTE;
    note.manifest PT_NOTE;
}

/*
 * Output sections.
 */
SECTIONS {
    . = TEXT_START; /* No + SIZEOF_HEADERS */

    /*
     * :text: The following input sections are placed in the R/E :text segment.
     */
    _stext = .;

    .interp : {
        *(.interp)
    } :interp :text

    .text :
    {
        *(.text)
        *(.text.*)
    } :text

    . = ALIGN(CONSTANT(MAXPAGESIZE));
    _etext = .;

    /* Read-only data */

    /* For Spt, the ABI and MFT NOTEs are read-only and can be in :text. */
    .note.solo5.manifest :
    {
        *(.note.solo5.manifest*)
    } :text :note.manifest
    .note.solo5.abi :
    {
        *(.note.solo5.abi*)
    } :text :note.abi
    .note.solo5.not-openbsd :
    {
        *(.note.solo5.not-openbsd*)
    } :text :note.not-openbsd

    .rodata :
    {
        *(.rodata)
        *(.rodata.*)
    } :text
    .eh_frame :
    {
        *(.eh_frame)
    }

    . = ALIGN(CONSTANT(MAXPAGESIZE));
    _erodata = .;

    /*
     * :data: The following input sections are placed in the R/W :data segment.
     */

    /* Read-write data (initialized) */
    .got :
    {
        *(.got.plt)
        *(.got)
    } :data
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

    /* We are not building a GNU executable, so discard any default NOTEs the
       toolchain might generate to prevent any surprises in the final layout. */
    /DISCARD/ : {
        *(.note.gnu.*)
    }
}
