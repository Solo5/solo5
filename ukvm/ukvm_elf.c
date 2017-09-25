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
 * Load code from an ELF file or elf_mem (if not NULL) into *mem and return the ELF
 * entry point and the last byte of the program when loaded into memory. This
 * accounts not only for the last loaded piece of code from the ELF, but also
 * for the zeroed out pieces that are not loaded and sould be reserved.
 *
 * Memory will look like this after the ELF is loaded:
 *
 * *mem                    *p_entry                   *p_end
 *   |             |                    |                |
 *   |    ...      | .text .rodata      |   .data .bss   |
 *   |             |        code        |   00000000000  |
 *   |             |  [PROT_EXEC|READ]  |                |
 *
 */
void _ukvm_elf_load(const char *file, uint8_t *mem, size_t mem_size,
                    ukvm_gpa_t *p_entry, ukvm_gpa_t *p_end,
                    uint8_t *elf_mem, size_t elf_mem_len)
{
    int fd_kernel = 0;
    ssize_t numb;
    size_t buflen;
    Elf64_Off ph_off;
    Elf64_Half ph_entsz;
    Elf64_Half ph_cnt;
    Elf64_Half ph_i;
    Elf64_Phdr *phdr = NULL;
    Elf64_Ehdr hdr;
    uint64_t result;

    /* elf entry point (on physical memory) */
    *p_entry = 0;
    /* highest byte of the program (on physical memory) */
    *p_end = 0;

    if (!elf_mem) {
        fd_kernel = open(file, O_RDONLY);
        if (fd_kernel == -1)
            err(1, "Failed open");
    }

    if (elf_mem) {
        if (sizeof(Elf64_Ehdr) >= elf_mem_len)
            goto out_invalid;
        memmove(&hdr, elf_mem, sizeof(Elf64_Ehdr));
        numb = sizeof(Elf64_Ehdr);
    } else
        numb = pread_in_full(fd_kernel, &hdr, sizeof(Elf64_Ehdr), 0);
    if (numb < 0)
        err(1, "Failed header read");
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
        err(1, "Failed malloc");
    if (elf_mem) {
        if ((ph_off >= elf_mem_len)
                || add_overflow(ph_off, buflen, result)
                || (result >= elf_mem_len))
            goto out_invalid;
        /*
         * It was checked at hypercall_exec that (without overflows):
         *     (elf_mem + elf_mem_len) < mem_len
         */
        memmove(phdr, elf_mem + ph_off, buflen);
        numb = buflen;
    } else
        numb = pread_in_full(fd_kernel, phdr, buflen, ph_off);
    if (numb < 0)
        err(1, "Failed section header read");
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
        if (elf_mem) {
            if ((offset >= elf_mem_len)
                    || add_overflow(offset, filesz, result)
                    || (result >= elf_mem_len))
                goto out_invalid;
            memmove(daddr, elf_mem + offset, filesz);
            numb = filesz;
        } else
            numb = pread_in_full(fd_kernel, daddr, filesz, offset);
        if (numb < 0)
            err(1, "Failed section read");
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

void ukvm_elf_load_file(const char *file, uint8_t *mem, size_t mem_size,
                        ukvm_gpa_t *p_entry, ukvm_gpa_t *p_end)
{
    _ukvm_elf_load(file, mem, mem_size, p_entry, p_end, NULL, 0);
}

void ukvm_elf_load_mem(uint8_t *elf_mem, size_t elf_mem_len, uint8_t *mem,
                       size_t mem_size, ukvm_gpa_t *p_entry, ukvm_gpa_t *p_end)
{
    _ukvm_elf_load(NULL, mem, mem_size, p_entry, p_end, elf_mem, elf_mem_len);
}
