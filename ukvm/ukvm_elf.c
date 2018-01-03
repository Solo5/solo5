/*
 * Copyright (c) 2015-2017 Contributors as noted in the AUTHORS file
 *
 * This file is part of ukvm, a unikernel monitor.
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
 * ukvm_elf.c: ELF loader.
 *
 * This module should be kept backend-independent and architectural
 * dependencies should be self-contained.
 */

#define _GNU_SOURCE
#include <err.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "ukvm.h"

static ssize_t pread_in_full(int fd, void *buf, size_t count, off_t offset)
{
    ssize_t total = 0;
    char *p = buf;

    if (count > SSIZE_MAX) {
        errno = E2BIG;
        return -1;
    }

    while (count > 0) {
        ssize_t nr;

        nr = pread(fd, p, count, offset);
        if (nr == 0)
            return total;
        else if (nr == -1 && errno == EINTR)
            continue;
        else if (nr == -1)
            return -1;

        count -= nr;
        total += nr;
        p += nr;
        offset += nr;
    }

    return total;
}

/*
 * Load code from elf file into *mem and return the elf entry point
 * and the last byte of the program when loaded into memory. This
 * accounts not only for the last loaded piece of code from the elf,
 * but also for the zeroed out pieces that are not loaded and sould be
 * reserved.
 *
 * Memory will look like this after the elf is loaded:
 *
 * *mem                    *p_entry                   *p_end
 *   |             |                    |                |
 *   |    ...      | .text .rodata      |   .data .bss   |
 *   |             |        code        |   00000000000  |
 *   |             |  [PROT_EXEC|READ]  |                |
 *
 */
void ukvm_elf_load(const char *file, uint8_t *mem, size_t mem_size,
       ukvm_gpa_t *p_entry, ukvm_gpa_t *p_end)
{
    int fd_kernel;
    ssize_t numb;
    size_t buflen;
    Elf64_Off ph_off;
    Elf64_Half ph_entsz;
    Elf64_Half ph_cnt;
    Elf64_Half ph_i;
    Elf64_Phdr *phdr = NULL;
    Elf64_Ehdr hdr;

    /* elf entry point (on physical memory) */
    *p_entry = 0;
    /* highest byte of the program (on physical memory) */
    *p_end = 0;

    fd_kernel = open(file, O_RDONLY);
    if (fd_kernel == -1)
        goto out_error;

    numb = pread_in_full(fd_kernel, &hdr, sizeof(Elf64_Ehdr), 0);
    if (numb < 0)
        goto out_error;
    if (numb != sizeof(Elf64_Ehdr))
        goto out_invalid;

    /*
     * Validate program is in ELF64 format:
     * 1. EI_MAG fields 0, 1, 2, 3 spell ELFMAG('0x7f', 'E', 'L', 'F'),
     * 2. File contains 64-bit objects,
     * 3. Objects are Executable,
     * 4. Target instruction must be set to the correct architecture.
     */
    if (hdr.e_ident[EI_MAG0] != ELFMAG0
            || hdr.e_ident[EI_MAG1] != ELFMAG1
            || hdr.e_ident[EI_MAG2] != ELFMAG2
            || hdr.e_ident[EI_MAG3] != ELFMAG3
            || hdr.e_ident[EI_CLASS] != ELFCLASS64
            || hdr.e_type != ET_EXEC
#if defined(__x86_64__)
            || hdr.e_machine != EM_X86_64
#elif defined(__aarch64__)
            || hdr.e_machine != EM_AARCH64
#else
#error Unsupported target
#endif
        )
        goto out_invalid;

    ph_off = hdr.e_phoff;
    ph_entsz = hdr.e_phentsize;
    ph_cnt = hdr.e_phnum;
    buflen = ph_entsz * ph_cnt;

    phdr = malloc(buflen);
    if (!phdr)
        goto out_error;
    numb = pread_in_full(fd_kernel, phdr, buflen, ph_off);
    if (numb < 0)
        goto out_error;
    if (numb != buflen)
        goto out_invalid;

    /*
     * Load all segments with the LOAD directive from the elf file at offset
     * p_offset, and copy that into p_addr in memory. The amount of bytes
     * copied is p_filesz.  However, each segment should be given
     * p_memsz aligned up to p_align bytes on memory.
     */
    for (ph_i = 0; ph_i < ph_cnt; ph_i++) {
        uint8_t *daddr;
        uint64_t _end;
        size_t offset = phdr[ph_i].p_offset;
        size_t filesz = phdr[ph_i].p_filesz;
        size_t memsz = phdr[ph_i].p_memsz;
        uint64_t paddr = phdr[ph_i].p_paddr;
        uint64_t align = phdr[ph_i].p_align;
        uint64_t result;
        int prot;

        if (phdr[ph_i].p_type != PT_LOAD)
            continue;

        if ((paddr >= mem_size) || add_overflow(paddr, filesz, result)
                || (result >= mem_size))
            goto out_invalid;
        if (add_overflow(paddr, memsz, result) || (result >= mem_size))
            goto out_invalid;
        /*
         * Verify that align is a non-zero power of 2 and safely compute
         * ((_end + (align - 1)) & -align).
         */
        if (align > 0 && (align & (align - 1)) == 0) {
            if (add_overflow(result, (align - 1), _end))
                goto out_invalid;
            _end = _end & -align;
        }
        else {
            _end = result;
        }
        if (_end > *p_end)
            *p_end = _end;

        daddr = mem + paddr;
        numb = pread_in_full(fd_kernel, daddr, filesz, offset);
        if (numb < 0)
            goto out_error;
        if (numb != filesz)
            goto out_invalid;
        memset(daddr + filesz, 0, memsz - filesz);

        prot = PROT_NONE;
        if (phdr[ph_i].p_flags & PF_R)
            prot |= PROT_READ;
        if (phdr[ph_i].p_flags & PF_W)
            prot |= PROT_WRITE;
        if (phdr[ph_i].p_flags & PF_X)
            prot |= PROT_EXEC;
        if (prot & PROT_WRITE && prot & PROT_EXEC)
            warnx("%s: Warning: phdr[%u] requests WRITE and EXEC permissions",
                  file, ph_i);
        if (mprotect(daddr, _end - paddr, prot) == -1)
            goto out_error;
    }

    free (phdr);
    close (fd_kernel);
    *p_entry = hdr.e_entry;
    return;

out_error:
    err(1, "%s", file);

out_invalid:
    errx(1, "%s: Exec format error", file);
}

/*
 * the Unikernel's core format is:
 *   --------------
 *   |  elf header |
 *   --------------
 *   |  PT_NOTE    |
 *   --------------
 *   |  PT_LOAD    |
 *   --------------
 *   |  ......     |
 *   --------------
 *   |  PT_LOAD    |
 *   --------------
 *   |  elf note   |
 *   --------------
 *   |  memory     |
 *   --------------
 *
 * we only know where the memory is saved after we write elf note into
 * vmcore.
 */
void ukvm_dump_core(struct ukvm_hv *hv, struct ukvm_dump_core *info)
{
    int core_fd, elf_fd = 0, num_notes = 0;
    size_t offset, note_size;
    Elf64_Ehdr hdr;
    Elf64_Phdr phdr;

    memset((void *)&hdr, 0, sizeof(Elf64_Ehdr));
    memset((void *)&phdr, 0, sizeof(Elf64_Phdr));

    core_fd = open("core.unikernel", O_RDWR|O_CREAT|O_TRUNC|O_APPEND, S_IRUSR|S_IWUSR);
    if (core_fd < 0) {
        goto failure;
    }

    /* Read the elf file and re-write the elf header to the core */
    elf_fd = open(hv->elffile, O_RDONLY);
    if (elf_fd < 0) {
        goto failure;
    }

    if (pread_in_full(elf_fd, &hdr, sizeof(Elf64_Ehdr), 0) !=
            sizeof(Elf64_Ehdr)) {
        goto failure;
    }

    /* Update the elf header to indicate this is a core file */
    hdr.e_type = ET_CORE;

    /* No section header in core file */
    hdr.e_shoff = 0;
    hdr.e_shentsize = 0;
    hdr.e_shnum = 0;
    hdr.e_shstrndx = 0;

    note_size = ukvm_hv_get_notes_size(&num_notes);
    hdr.e_phnum = 1 + num_notes;

    if (write(core_fd, &hdr, sizeof(Elf64_Ehdr)) < 0) {
        goto failure;
    }

    offset = sizeof(hdr) + (hdr.e_phnum * sizeof(Elf64_Phdr));

    if (num_notes) {
        /* Write a core PT_NOTE, data for more useful notes is not available for now */
        phdr.p_type = PT_NOTE;
        phdr.p_offset = offset;
        phdr.p_filesz = note_size;
        phdr.p_memsz = note_size;
        if (write(core_fd, &phdr, sizeof(phdr)) < 0 ) {
            goto failure;
        }
        offset += note_size;
    }

    /* Write program headers for memory segments. We have only one? */
    memset(&phdr, 0, sizeof(phdr));
    phdr.p_type     = PT_LOAD;
    phdr.p_align    = 0;
    phdr.p_paddr    = (size_t)hv->mem;
    phdr.p_offset = offset;
    phdr.p_vaddr  = 0;
    phdr.p_memsz  = hv->mem_size;
    phdr.p_filesz = hv->mem_size;
    phdr.p_flags  = 0;

    if (write(core_fd, &phdr, sizeof(phdr)) < 0) {
        goto failure;
    }

    /* Write note section */
    if (num_notes &&
        ukvm_hv_dump_notes(core_fd, hv, info) < 0) {
        goto failure;
    }

    /* Write memory */
    if (write(core_fd, hv->mem, hv->mem_size) < 0) {
        goto failure;
    }
    goto cleanup;

failure:
    warnx("%s: Failed to generate the core file", __FUNCTION__);

cleanup:
    close(core_fd);
    close(elf_fd);
}
