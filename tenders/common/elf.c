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
#include <assert.h>
#include <err.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/types.h>
#include <unistd.h>

#include "cc.h"
#include "elf.h"

/*
 * Define EM_TARGET, EM_PAGE_SIZE and EI_DATA_TARGET for the architecture we
 * are compiling on.
 */
#if defined(__x86_64__)
#define EM_TARGET EM_X86_64
#define EM_PAGE_SIZE 0x1000
#elif defined(__aarch64__)
#define EM_TARGET EM_AARCH64
#define EM_PAGE_SIZE 0x1000
#elif defined(__powerpc64__)
#define EM_TARGET EM_PPC64
#define EM_PAGE_SIZE 0x10000
#else
#error Unsupported target
#endif

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define EI_DATA_TARGET ELFDATA2LSB
#else
#define EI_DATA_TARGET ELFDATA2MSB
#endif

/*
 * Solo5-owned ELF notes are identified by an n_name of "Solo5".
 */
#define SOLO5_NOTE_NAME "Solo5"

/*
 * Defines an Elf64_Nhdr with n_name filled in and padded to a 4-byte boundary,
 * i.e. the common part of a Solo5-owned Nhdr.
 */
struct solo5_nhdr {
    Elf64_Nhdr h;
    char n_name[(sizeof(SOLO5_NOTE_NAME) + 3) & -4];
    /*
     * Note content ("descriptor" in ELF terms) follows in the file here,
     * possibly with some internal alignment before the first struct member
     * (see below).
     */
};

_Static_assert((sizeof(struct solo5_nhdr)) ==
        (sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint32_t) + 8),
        "struct solo5_nhdr alignment issue");

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
     * 1. Validate that this is an ELF64 header we support.
     *
     * Note: e_ident[EI_OSABI] and e_ident[EI_ABIVERSION] are deliberately NOT
     * checked as compilers do not provide a way to override this without
     * building the entire toolchain from scratch.
     */
    if (!(hdr->e_ident[EI_MAG0] == ELFMAG0
            && hdr->e_ident[EI_MAG1] == ELFMAG1
            && hdr->e_ident[EI_MAG2] == ELFMAG2
            && hdr->e_ident[EI_MAG3] == ELFMAG3
            && hdr->e_ident[EI_CLASS] == ELFCLASS64
            && hdr->e_ident[EI_DATA] == EI_DATA_TARGET
            && hdr->e_version == EV_CURRENT))
        return false;
    /*
     * 2. Validate ELF64 header internal sizes match what we expect, and that
     * at least one program header entry is present.
     */
    if (hdr->e_ehsize != sizeof (Elf64_Ehdr))
        return false;
    if (hdr->e_phnum < 1)
        return false;
    if (hdr->e_phentsize != sizeof (Elf64_Phdr))
        return false;
    /*
     * 3. Validate that this is an executable for our target architecture.
     */
    if (hdr->e_type != ET_EXEC)
        return false;
    if (hdr->e_machine != EM_TARGET)
        return false;

    return true;
}

/*
 * Align (addr) down to (align) boundary. Returns 1 if (align) is not a
 * non-zero power of 2.
 */
static int align_down(Elf64_Addr addr, Elf64_Xword align,
        Elf64_Addr *out_result)
{
    if (align > 0 && (align & (align - 1)) == 0) {
        *out_result = addr & -align;
        return 0;
    }
    else
        return 1;
}

/*
 * Align (addr) up to (align) boundary. Returns 1 if an overflow would occur or
 * (align) is not a non-zero power of 2, otherwise result in (*out_result) and
 * 0.
 */
static int align_up(Elf64_Addr addr, Elf64_Xword align, Elf64_Addr *out_result)
{
    Elf64_Addr result;

    if (align > 0 && (align & (align - 1)) == 0) {
        if (add_overflow(addr, (align - 1), result))
            return 1;
        result = result & -align;
        *out_result = result;
        return 0;
    }
    else
        return 1;
}

void elf_load(int bin_fd, const char *bin_name, uint8_t *mem, size_t mem_size,
        uint64_t p_min_loadaddr, guest_mprotect_fn_t t_guest_mprotect,
        void *t_guest_mprotect_arg, uint64_t *p_entry, uint64_t *p_end)
{
    ssize_t nbytes;
    Elf64_Phdr *phdr = NULL;
    Elf64_Ehdr *ehdr = NULL;
    Elf64_Addr e_entry;                 /* Program entry point */
    Elf64_Addr e_end;                   /* Highest memory address occupied */

    ehdr = malloc(sizeof(Elf64_Ehdr));
    if (ehdr == NULL)
        goto out_error;
    nbytes = pread_in_full(bin_fd, ehdr, sizeof(Elf64_Ehdr), 0);
    if (nbytes < 0)
        goto out_error;
    if (nbytes != sizeof(Elf64_Ehdr))
        goto out_invalid;
    if (!ehdr_is_valid(ehdr))
        goto out_invalid;
    /*
     * e_entry must be non-zero and within range of our memory allocation.
     */
    if (ehdr->e_entry < p_min_loadaddr || ehdr->e_entry >= mem_size)
        goto out_invalid;
    e_entry = ehdr->e_entry;

    size_t ph_size = ehdr->e_phnum * ehdr->e_phentsize;
    phdr = malloc(ph_size);
    if (!phdr)
        goto out_error;
    nbytes = pread_in_full(bin_fd, phdr, ph_size, ehdr->e_phoff);
    if (nbytes < 0)
        goto out_error;
    if (nbytes != ph_size)
        goto out_invalid;

    /*
     * Load all program segments with the PT_LOAD directive.
     */
    e_end = 0;
    Elf64_Addr plast_vaddr = 0;
    for (Elf64_Half ph_i = 0; ph_i < ehdr->e_phnum; ph_i++) {
        Elf64_Addr p_vaddr = phdr[ph_i].p_vaddr;
        Elf64_Xword p_filesz = phdr[ph_i].p_filesz;
        Elf64_Xword p_memsz = phdr[ph_i].p_memsz;
        Elf64_Xword p_align = phdr[ph_i].p_align;
        Elf64_Addr temp, p_vaddr_start, p_vaddr_end;

        if (phdr[ph_i].p_type != PT_LOAD)
            continue;

        if (p_vaddr < p_min_loadaddr)
            goto out_invalid;
        /*
         * The ELF specification mandates that program headers are sorted on
         * p_vaddr in ascending order. Enforce this, at the same time avoiding
         * any surprises later.
         */
        if (p_vaddr < plast_vaddr)
            goto out_invalid;
        else
            plast_vaddr = p_vaddr;
        /*
         * Compute p_vaddr_start = p_vaddr, aligned down to requested alignment
         * and verify result is within range.
         */
        if (align_down(p_vaddr, p_align, &p_vaddr_start))
            goto out_invalid;
        if (p_vaddr_start < p_min_loadaddr)
            goto out_invalid;
        /*
         * Disallow overlapping segments. This may be overkill, but in practice
         * the Solo5 toolchains do not produce such executables.
         */
        if (p_vaddr_start < e_end)
            goto out_invalid;
        /*
         * Verify p_vaddr + p_filesz is within range.
         */
        if (p_vaddr >= mem_size)
            goto out_invalid;
        if (add_overflow(p_vaddr, p_filesz, temp))
            goto out_invalid;
        if (temp > mem_size)
            goto out_invalid;
        /*
         * Compute p_vaddr_end = p_vaddr + p_memsz, aligned up to requested
         * alignment and verify result is within range.
         */
        if (p_memsz < p_filesz)
            goto out_invalid;
        if (add_overflow(p_vaddr, p_memsz, p_vaddr_end))
            goto out_invalid;
        if (align_up(p_vaddr_end, p_align, &p_vaddr_end))
            goto out_invalid;
        if (p_vaddr_end > mem_size)
            goto out_invalid;
        /*
         * Keep track of the highest byte of memory occupied by the program.
         */
        if (p_vaddr_end > e_end) {
            e_end = p_vaddr_end;
            /*
             * Double check result for host (caller) address space overflow.
             */
            assert((mem + e_end) >= (mem + p_min_loadaddr));
        }

        /*
         * Load the segment (p_vaddr .. p_vaddr + p_filesz) into host memory at
         * host_vaddr and ensure any BSS (p_memsz - p_filesz) is initialised to
         * zero.
         */
        uint8_t *host_vaddr = mem + p_vaddr;
        /*
         * Double check result for host (caller) address space overflow.
         */
        assert(host_vaddr >= (mem + p_min_loadaddr));
        nbytes = pread_in_full(bin_fd, host_vaddr, p_filesz,
                phdr[ph_i].p_offset);
        if (nbytes < 0)
            goto out_error;
        if (nbytes != p_filesz)
            goto out_invalid;
        memset(host_vaddr + p_filesz, 0, p_memsz - p_filesz);

        /*
         * Memory protection flags should be applied to the aligned address
         * range (p_vaddr_start .. p_vaddr_end). Before we apply them, also
         * verify that the address range is aligned to the architectural page
         * size.
         */
        if (p_vaddr_start & (EM_PAGE_SIZE - 1))
            goto out_invalid;
        if (p_vaddr_end & (EM_PAGE_SIZE - 1))
            goto out_invalid;
        int prot = PROT_NONE;
        if (phdr[ph_i].p_flags & PF_R)
            prot |= PROT_READ;
        if (phdr[ph_i].p_flags & PF_W)
            prot |= PROT_WRITE;
        if (phdr[ph_i].p_flags & PF_X)
            prot |= PROT_EXEC;
        if (prot & PROT_WRITE && prot & PROT_EXEC) {
            warnx("%s: Error: phdr[%u] requests WRITE and EXEC permissions",
                  bin_name, ph_i);
            goto out_invalid;
        }
        assert(t_guest_mprotect != NULL);
        if (t_guest_mprotect(t_guest_mprotect_arg, p_vaddr_start, p_vaddr_end,
                    prot) == -1)
            goto out_error;
    }

    free(ehdr);
    free(phdr);
    *p_entry = e_entry;
    *p_end = e_end;
    return;

out_error:
    warn("%s", bin_name);
    free(ehdr);
    free(phdr);
    exit(1);

out_invalid:
    warnx("%s: Invalid or unsupported executable", bin_name);
    free(ehdr);
    free(phdr);
    exit(1);
}

int elf_load_note(int bin_fd, const char *bin_name, uint32_t note_type,
        size_t note_align, size_t max_note_size, void **out_note_data,
        size_t *out_note_size)
{
    ssize_t nbytes;
    Elf64_Phdr *phdr = NULL;
    Elf64_Ehdr *ehdr = NULL;
    uint8_t *note_data = NULL;
    size_t note_offset, note_size, note_pad;

    ehdr = malloc(sizeof(Elf64_Ehdr));
    if (ehdr == NULL)
        goto out_error;
    nbytes = pread_in_full(bin_fd, ehdr, sizeof(Elf64_Ehdr), 0);
    if (nbytes < 0)
        goto out_error;
    if (nbytes != sizeof(Elf64_Ehdr))
        goto out_invalid;
    if (!ehdr_is_valid(ehdr))
        goto out_invalid;

    size_t ph_size = ehdr->e_phnum * ehdr->e_phentsize;
    phdr = malloc(ph_size);
    if (!phdr)
        goto out_error;
    nbytes = pread_in_full(bin_fd, phdr, ph_size, ehdr->e_phoff);
    if (nbytes < 0)
        goto out_error;
    if (nbytes != ph_size)
        goto out_invalid;

    /*
     * Find the phdr containing the Solo5 NOTE of type note_type, and sanity
     * check its headers.
     */
    bool note_found = false;
    Elf64_Half ph_i;
    struct solo5_nhdr nhdr;
    for (ph_i = 0; ph_i < ehdr->e_phnum; ph_i++) {
        if (phdr[ph_i].p_type != PT_NOTE)
            continue;
        if (phdr[ph_i].p_filesz < sizeof (Elf64_Nhdr))
            /*
             * p_filesz is less than minimum possible size of a NOTE header,
             * reject the executable.
             */
            goto out_invalid;
        if (phdr[ph_i].p_filesz < sizeof nhdr)
            /*
             * p_filesz is less than minimum possible size of a Solo5 NOTE
             * header, ignore the note.
             */
            continue;
        nbytes = pread_in_full(bin_fd, &nhdr, sizeof nhdr,
                phdr[ph_i].p_offset);
        if (nbytes < 0)
            goto out_error;
        if (nbytes != sizeof nhdr)
            goto out_invalid;
        if (nhdr.h.n_namesz != sizeof(SOLO5_NOTE_NAME))
            /*
             * Not a Solo5-owned NOTE or invalid n_namesz, skip.
             */
            continue;
        if (strncmp(nhdr.n_name, SOLO5_NOTE_NAME, sizeof(SOLO5_NOTE_NAME)) != 0)
            /*
             * Not a Solo5-owned NOTE, skip.
             */
            continue;
        if (nhdr.h.n_type != note_type)
            /*
             * Not the Solo5 NOTE of note_type we are looking for, skip.
             */
            continue;
        /*
         * Check note descriptor (content) size is within limits, and
         * cross-check with p_filesz.
         */
        if (nhdr.h.n_descsz < 1 || nhdr.h.n_descsz > max_note_size)
            goto out_invalid;
        if (phdr[ph_i].p_filesz < sizeof nhdr + nhdr.h.n_descsz)
            goto out_invalid;

        note_found = true;
        break;
    }
    if (!note_found) {
        free(ehdr);
        free(phdr);
        return -1;
    }

    /*
     * At this point we have verified that the NOTE at phdr[ph_i] is the Solo5
     * NOTE with the requested note_type and its file sizes are sane.
     *
     * Adjust for alignment requested in (note_align) and read the note
     * descriptor (content) following the header into dynamically allocated
     * memory.
     */
    assert(note_align > 0 && (note_align & (note_align - 1)) == 0);
    note_offset = (sizeof nhdr + (note_align - 1)) & -note_align;
    assert(note_offset >= sizeof nhdr);
    note_pad = note_offset - sizeof nhdr;
    note_size = nhdr.h.n_descsz - note_pad;
    assert(note_size != 0 && note_size <= nhdr.h.n_descsz);
    note_data = malloc(note_size);
    if (note_data == NULL)
        goto out_error;
    nbytes = pread_in_full(bin_fd, note_data, note_size,
            phdr[ph_i].p_offset + note_offset);
    if (nbytes < 0)
        goto out_error;
    if (nbytes != note_size)
        goto out_invalid;

    *out_note_data = note_data;
    *out_note_size = note_size;
    free(ehdr);
    free(phdr);
    return 0;

out_error:
    warn("%s", bin_name);
    free(ehdr);
    free(phdr);
    free(note_data);
    exit(1);

out_invalid:
    warnx("%s: Invalid or unsupported executable", bin_name);
    free(ehdr);
    free(phdr);
    free(note_data);
    exit(1);
}
