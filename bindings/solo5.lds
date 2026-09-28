/*
 * Copyright (c) 2015-2026 Contributors as noted in the AUTHORS file
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
 * This is the custom linker script for the Solo5 targets.
 *
 * The script is tested to work with a minimal set of input sections. If there
 * are unexpected input sections not named here, the result will probably not be
 * correct.
 */
#if defined(SOLO5_STUB) || defined(SOLO5_SPT) || defined(SOLO5_HVT) \
  || defined(SOLO5_VIRTIO) || defined(SOLO5_XEN)
TEXT_START = 0x100000;
#elif defined(SOLO5_MUEN)
TEXT_START = 0x200000;
#endif

ENTRY(_start)

/*
 * Program headers: In order to force the linker to place each of our NOTEs
 * into a separate PT_NOTE header, we need to lay these out explicitly.
 */
PHDRS {
    interp PT_INTERP;
#if defined(SOLO5_STUB) || defined(SOLO5_SPT) || defined(SOLO5_MUEN) \
  || defined(SOLO5_VIRTIO) || defined(SOLO5_XEN)
    text PT_LOAD FLAGS(5); /* No FILEHDR or PHDRS, force R/E only.
                              FLAGS values come from PF_x in elf.h */
#elif defined(SOLO5_HVT)
    text PT_LOAD FLAGS(1); /* No FILEHDR or PHDRS, force E only.
                              FLAGS values come from PF_x in elf.h */
#endif

    rodata PT_LOAD FLAGS(4);
    data PT_LOAD FLAGS(6);
    tdata PT_LOAD FLAGS(4); /* RO: this have to be copied into each thread */
    tbss PT_TLS FLAGS(0); /* no perm needed */
    bss PT_LOAD FLAGS(6);
    note.not_openbsd PT_NOTE; /* Must come first. */
#if defined(SOLO5_XEN)
    note.xen PT_NOTE;
#endif
    note.abi PT_NOTE;
    note.manifest PT_NOTE;
}

/*
 * Output sections.
 */
SECTIONS {
    . = TEXT_START; /* No + SIZEOF_HEADERS */

    /*
     * :text: The following input sections are placed in the :text segment.
     */
    _stext = .;

#if !defined(SOLO5_XEN)
    .interp : {
        *(.interp)
    } :interp :text
#endif

    .text :
    {
#if defined(SOLO5_VIRTIO) || defined(SOLO5_XEN)
        *(.data.multiboot)
#endif
        *(.text)
        *(.text.*)
    } :text

#if defined(SOLO5_XEN)
    .interp : {
        *(.interp)
    } :interp :text
#endif

    . = ALIGN(CONSTANT(MAXPAGESIZE));
    _etext = .;

    /* Read-only data */

#if defined(SOLO5_STUB) || defined(SOLO5_HVT) || defined(SOLO5_SPT) \
  || defined(SOLO5_MUEN)
    /* For Hvt, Spt and Muen, the ABI and MFT NOTEs are read-only and can be in
     * :rodata. */
    .note.solo5.manifest :
    {
        *(.note.solo5.manifest*)
    } :rodata :note.manifest
    .note.solo5.abi :
    {
        *(.note.solo5.abi*)
    } :rodata :note.abi
    .note.solo5.not_openbsd :
    {
        *(.note.solo5.not_openbsd*)
    } :rodata :note.not_openbsd
#endif

    .rodata :
    {
        *(.rodata)
        *(.rodata.*)
    } :rodata
#if defined(SOLO5_SPT)
    .eh_frame_hdr :
    {
        *(.eh_frame_hdr)
    } :rodata
#endif
    .eh_frame :
    {
        *(.eh_frame)
    } :rodata

    . = ALIGN(CONSTANT(MAXPAGESIZE));
    _erodata = .;

    /*
     * :data: The following input sections are placed in the R/W :data segment.
     */

#if defined(SOLO5_VIRTIO) || defined(SOLO5_XEN)
    /* For Virtio and Xen, the ABI and MFT NOTEs must be in an R/W segment, as
     * they may be modified in-place by the bindings. */
    .note.solo5.manifest :
    {
        *(.note.solo5.manifest*)
    } :data :note.manifest
    .note.solo5.abi :
    {
        *(.note.solo5.abi*)
    } :data :note.abi
    .note.solo5.not_openbsd :
    {
        *(.note.solo5.not_openbsd*)
    } :data :note.not_openbsd
#endif
#if defined(SOLO5_XEN)
    .note.solo5.xen :
    {
        *(.note.solo5.xen*)
    } :data :note.xen
#endif

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
    } :data

    . = ALIGN(CONSTANT(MAXPAGESIZE));
    /*
     * tdata and tbss have to be side by side to help the linker calculate the
     * various __thread variables offsets.
     */
    .tdata :
    {
        _stdata = .;
        *(.tdata)
        . = ALIGN(CONSTANT(MAXPAGESIZE));
    } :tdata

    /*
     * _edata is the address of the end of the area to be copied with multiboot
     * (virtio and xen)
     */
    _edata = .;

    /* Read-write data (uninitialized) */
    .tbss :
    {
        *(.tbss)
    } :tbss
    .bss :
    {
        *(.bss)
        *(COMMON)
    } :bss

    . = ALIGN(CONSTANT(MAXPAGESIZE));
    _ebss = .;
    _end = .;

    /* We are not building a GNU executable, so discard any default NOTEs the
       toolchain might generate to prevent any surprises in the final layout. */
    /DISCARD/ : {
        *(.note.gnu.*)
    }

#if defined(SOLO5_SPT) || defined(SOLO5_HVT) || defined(SOLO5_VIRTIO) \
  || defined(SOLO5_XEN)
    _ltdata = _edata - _stdata;
#elif defined(SOLO5_STUB) || defined(SOLO5_MUEN)
    _ltdata = SIZEOF(.tdata);
#endif
    _ltbss = SIZEOF(.tbss);
}
