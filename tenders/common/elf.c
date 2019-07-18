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
 * elf.c: ELF loader.
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
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "cc.h"
#include "mft_abi.h"
#include "elf.h"

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

static bool ehdr_is_valid(const Elf64_Ehdr *hdr)
{
    /*
     * Validate program is in ELF64 format, of type ET_EXEC and for the correct
     * target architecture.
     */
    return (hdr->e_ident[EI_MAG0] == ELFMAG0
            && hdr->e_ident[EI_MAG1] == ELFMAG1
            && hdr->e_ident[EI_MAG2] == ELFMAG2
            && hdr->e_ident[EI_MAG3] == ELFMAG3
            && hdr->e_ident[EI_CLASS] == ELFCLASS64
            && hdr->e_type == ET_EXEC
#if defined(__x86_64__)
            && hdr->e_machine == EM_X86_64
#elif defined(__aarch64__)
            && hdr->e_machine == EM_AARCH64
#else
#error Unsupported target
#endif
        );
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
void elf_load(void *t, const char *file, uint8_t *mem, size_t mem_size,
       guest_mprotect_fn_t guest_mprotect_fn,
       uint64_t *p_entry, uint64_t *p_end)
{
    int fd_kernel = -1;
    ssize_t nbytes;
    size_t ph_size;
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

    nbytes = pread_in_full(fd_kernel, &hdr, sizeof(Elf64_Ehdr), 0);
    if (nbytes < 0)
        goto out_error;
    if (nbytes != sizeof(Elf64_Ehdr))
        goto out_invalid;
    if (!ehdr_is_valid(&hdr))
        goto out_invalid;

    ph_off = hdr.e_phoff;
    ph_entsz = hdr.e_phentsize;
    ph_cnt = hdr.e_phnum;
    ph_size = ph_entsz * ph_cnt;
    phdr = malloc(ph_size);
    if (!phdr)
        goto out_error;
    nbytes = pread_in_full(fd_kernel, phdr, ph_size, ph_off);
    if (nbytes < 0)
        goto out_error;
    if (nbytes != ph_size)
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
        nbytes = pread_in_full(fd_kernel, daddr, filesz, offset);
        if (nbytes < 0)
            goto out_error;
        if (nbytes != filesz)
            goto out_invalid;
        memset(daddr + filesz, 0, memsz - filesz);

        prot = PROT_NONE;
        if (phdr[ph_i].p_flags & PF_R)
            prot |= PROT_READ;
        if (phdr[ph_i].p_flags & PF_W)
            prot |= PROT_WRITE;
        if (phdr[ph_i].p_flags & PF_X)
            prot |= PROT_EXEC;
        if (prot & PROT_WRITE && prot & PROT_EXEC) {
            warnx("%s: Error: phdr[%u] requests WRITE and EXEC permissions",
                  file, ph_i);
            goto out_invalid;
        }
        if (guest_mprotect_fn(t, daddr, _end - paddr, prot) == -1)
            goto out_error;
    }

    free (phdr);
    close (fd_kernel);
    *p_entry = hdr.e_entry;
    return;

out_error:
    warn("%s", file);
    free (phdr);
    if (fd_kernel != -1)
        close (fd_kernel);
    exit(1);

out_invalid:
    warnx("%s: Exec format error", file);
    free (phdr);
    if (fd_kernel != -1)
        close (fd_kernel);
    exit(1);
}

void elf_load_mft(const char *file, struct mft **mft, size_t *mft_size)
{
    int fd_kernel = -1;
    ssize_t nbytes;
    size_t ph_size;
    Elf64_Off ph_off;
    Elf64_Half ph_entsz;
    Elf64_Half ph_cnt;
    Elf64_Half ph_i;
    Elf64_Phdr *phdr = NULL;
    Elf64_Ehdr hdr;
    struct mft *note = NULL;
    size_t note_offset, note_size = 0;

    fd_kernel = open(file, O_RDONLY);
    if (fd_kernel == -1)
        goto out_error;

    nbytes = pread_in_full(fd_kernel, &hdr, sizeof(Elf64_Ehdr), 0);
    if (nbytes < 0)
        goto out_error;
    if (nbytes != sizeof(Elf64_Ehdr))
        goto out_invalid;
    if (!ehdr_is_valid(&hdr))
        goto out_invalid;

    ph_off = hdr.e_phoff;
    ph_entsz = hdr.e_phentsize;
    ph_cnt = hdr.e_phnum;
    ph_size = ph_entsz * ph_cnt;
    phdr = malloc(ph_size);
    if (!phdr)
        goto out_error;
    nbytes = pread_in_full(fd_kernel, phdr, ph_size, ph_off);
    if (nbytes < 0)
        goto out_error;
    if (nbytes != ph_size)
        goto out_invalid;

    /*
     * Find the NOTE containing the Solo5 manifest and sanity check its headers.
     */
    bool mft_note_found = false;
    for (ph_i = 0; ph_i < ph_cnt; ph_i++) {
        if (phdr[ph_i].p_type != PT_NOTE)
            continue; /* Not a NOTE, next */

        struct mft_note_header nhdr;
        if (phdr[ph_i].p_filesz < sizeof nhdr)
            continue; /* Too small to be a (valid) Solo5 NOTE, skip */
        nbytes = pread_in_full(fd_kernel, &nhdr, sizeof nhdr,
                phdr[ph_i].p_offset);
        if (nbytes < 0)
            goto out_error;
        if (nbytes != sizeof nhdr)
            goto out_invalid;
        if (strncmp(nhdr.name, SOLO5_NOTE_NAME, sizeof(SOLO5_NOTE_NAME)) != 0)
            continue; /* Not a Solo5 NOTE, next */

        if (nhdr.type != SOLO5_NOTE_MANIFEST) {
            warnx("%s: phdr[%u] contains invalid Solo5 NOTE", file, ph_i);
            goto out_invalid;
        }
        if (nhdr.descsz > MFT_NOTE_MAX_SIZE) {
            warnx("%s: phdr[%u] Solo5 NOTE size out of range", file, ph_i);
            goto out_invalid;
        }

        mft_note_found = true;
        break;
    }
    if (!mft_note_found) {
        warnx("%s: No manifest found, not a Solo5 executable?", file);
        goto out_invalid;
    }

    /*
     * At this point we have verified that the NOTE at phdr[ph_i] could be a
     * Solo5 manifest, and it's sizes are within some sane limits. Adjust size
     * and offset to skip the note header itself and read the full note
     * contents (the manifest) into dynamically allocated memory.
     */
    note_offset = phdr[ph_i].p_offset + offsetof(struct mft_note, m);
    note_size = phdr[ph_i].p_filesz - offsetof(struct mft_note, m);
    note = malloc(note_size);
    if (note == NULL)
        goto out_error;
    nbytes = pread_in_full(fd_kernel, note, note_size, note_offset);
    if (nbytes < 0)
        goto out_error;
    if (nbytes != note_size)
        goto out_invalid;

    *mft = note;
    *mft_size = note_size;
    free(phdr);
    close(fd_kernel);
    return;

out_error:
    warn("%s", file);
    free (phdr);
    free (note);
    if (fd_kernel != -1)
        close (fd_kernel);
    exit(1);

out_invalid:
    warnx("%s: Exec format error", file);
    free (phdr);
    free (note);
    if (fd_kernel != -1)
        close (fd_kernel);
    exit(1);
}
