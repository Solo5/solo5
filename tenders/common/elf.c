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

#define INV_EXE "invalid executable"

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
    if (ehdr == NULL) {
        warnx("%s: malloc(sizeof(Elf64_Ehdr) returned NULL while loading elf",
              bin_name);
        goto mem_cleanup;
    }
    nbytes = pread_in_full(bin_fd, ehdr, sizeof(Elf64_Ehdr), 0);
    if (nbytes < 0) {
        warnx("%s: pread_in_full for elf header while loading elf returned"
              " %zu < 0", bin_name, nbytes);
        goto mem_cleanup;
    }
    if (nbytes != sizeof(Elf64_Ehdr)) {
        warnx("%s: %s: elf header does not match the expected size"
              " (%zu != %lu)", bin_name, INV_EXE, nbytes, sizeof(Elf64_Ehdr));
        goto mem_cleanup;
    }
    if (!ehdr_is_valid(ehdr)) {
        warnx("%s: invalid or unsupported executable (elf header invalid while"
              " loading elf)", bin_name);
        goto mem_cleanup;
    }
    /*
     * e_entry must be non-zero and within range of our memory allocation.
     */
    if (ehdr->e_entry < p_min_loadaddr || ehdr->e_entry >= mem_size) {
        warnx("%s: %s: e_entry does not fall within the memory allocation"
              " range (%llu < %llu || %llu >= %zu)", bin_name, INV_EXE,
              (unsigned long long)ehdr->e_entry,
              (unsigned long long)p_min_loadaddr,
              (unsigned long long)ehdr->e_entry, mem_size);
        goto mem_cleanup;
    }
    e_entry = ehdr->e_entry;

    size_t ph_size = ehdr->e_phnum * ehdr->e_phentsize;
    phdr = malloc(ph_size);
    if (!phdr) {
        warnx("%s: malloc(ph_size) returned NULL while loading elf", bin_name);
        goto mem_cleanup;
    }
    nbytes = pread_in_full(bin_fd, phdr, ph_size, ehdr->e_phoff);
    if (nbytes < 0) {
        warnx("%s: pread_in_full for ph_size returned %zu < 0 while loading"
              " elf", bin_name, nbytes);
        goto mem_cleanup;
    }
    if ((size_t)nbytes != ph_size) {
        warnx("%s: %s: program header does not match the expected size"
              " (%zu != %zu while loading elf)", bin_name, INV_EXE, nbytes,
              ph_size);
        goto mem_cleanup;
    }

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

        /*
         * consider only non empty PT_LOAD
         */
        if (phdr[ph_i].p_filesz == 0 || phdr[ph_i].p_type != PT_LOAD)
            continue;

        if (p_vaddr < p_min_loadaddr) {
            warnx("%s: %s: phdr[%u].p_vaddr outside of valid memory range"
                  " (%llu < %llu)", bin_name, INV_EXE, ph_i,
                  (unsigned long long)p_vaddr,
                  (unsigned long long)p_min_loadaddr);
            goto mem_cleanup;
        }
        /*
         * The ELF specification mandates that program headers are sorted on
         * p_vaddr in ascending order. Enforce this, at the same time avoiding
         * any surprises later.
         */
        if (p_vaddr < plast_vaddr) {
            warnx("%s: %s: phdr[%u].p_vaddr is not in ascending order"
                  " (%llu < %llu)", bin_name, INV_EXE, ph_i,
                  (unsigned long long)p_vaddr,
                  (unsigned long long)plast_vaddr);
            goto mem_cleanup;
        }
        else
            plast_vaddr = p_vaddr;
        /*
         * Compute p_vaddr_start = p_vaddr, aligned down to requested alignment
         * and verify result is within range.
         */
        if (align_down(p_vaddr, p_align, &p_vaddr_start)) {
            warnx("%s: %s: phdr[%u].p_vaddr is outside of memory range"
                  " (align_down with %llu (p_vaddr) and %llu (p_align) failed)",
                  bin_name, INV_EXE, ph_i, (unsigned long long)p_vaddr,
                  (unsigned long long)p_align);
            goto mem_cleanup;
        }
        if (p_vaddr_start < p_min_loadaddr) {
            warnx("%s: %s: phdr[%u].p_vaddr_start is outside of memory"
                  " range (%llu < %llu (p_min_loadaddr))", bin_name, INV_EXE,
                  ph_i, (unsigned long long)p_vaddr_start,
                  (unsigned long long)p_min_loadaddr);
            goto mem_cleanup;
        }
        /*
         * Disallow overlapping segments. This may be overkill, but in practice
         * the Solo5 toolchains do not produce such executables.
         */
        if (p_vaddr_start < e_end) {
            warnx("%s: %s: phdr[%u].p_vaddr_start is outside of memory"
                  " range (%llu < %llu (e_end))", bin_name, INV_EXE, ph_i,
                  (unsigned long long)p_vaddr_start, (unsigned long long)e_end);
            goto mem_cleanup;
        }
        /*
         * Verify p_vaddr + p_filesz is within range.
         */
        if (p_vaddr >= mem_size) {
            warnx("%s: %s: phdr[%u] program file segment falls outside of"
                  " valid range (%llu >= %zu (mem_size))", bin_name, INV_EXE,
                  ph_i, (unsigned long long)p_vaddr, mem_size);
            goto mem_cleanup;
        }
        if (add_overflow(p_vaddr, p_filesz, temp)) {
            warnx("%s: %s: phdr[%u] program file segment falls outside of"
                  " valid range (%llu (p_vaddr) + %llu (p_filesz) overflow)",
                  bin_name, INV_EXE, ph_i, (unsigned long long)p_vaddr,
                  (unsigned long long)p_filesz);
            goto mem_cleanup;
        }
        if (temp > mem_size) {
            warnx("%s: %s: phdr[%u] program file segment falls outside of"
                  " valid range (%llu (p_vaddr + p_filesz) > %zu (mem_size))",
                  bin_name, INV_EXE, ph_i, (unsigned long long)temp, mem_size);
            goto mem_cleanup;
        }
        /*
         * Compute p_vaddr_end = p_vaddr + p_memsz, aligned up to requested
         * alignment and verify result is within range.
         */
        if (p_memsz < p_filesz) {
            warnx("%s: %s: phdr[%u] program memory segment falls outside"
                  " of valid range (%llu (p_memsz) < %llu (p_filesz))",
                  bin_name, INV_EXE, ph_i, (unsigned long long)p_memsz,
                  (unsigned long long)p_filesz);
            goto mem_cleanup;
        }
        if (add_overflow(p_vaddr, p_memsz, p_vaddr_end)) {
            warnx("%s: %s: phdr[%u] program memory segment falls outside"
                  " of valid range (%llu (p_vaddr) + %llu (p_memsz) overflow)",
                  bin_name, INV_EXE, ph_i, (unsigned long long)p_vaddr,
                  (unsigned long long)p_memsz);
            goto mem_cleanup;
        }
        if (align_up(p_vaddr_end, p_align, &p_vaddr_end)) {
            warnx("%s: %s: phdr[%u] program memory segment falls outside"
                  " of valid range (align_up with %llu (p_vaddr_end) and %llu"
                  " (p_align) failed)", bin_name, INV_EXE, ph_i,
                  (unsigned long long)p_vaddr_end, (unsigned long long)p_align);
            goto mem_cleanup;
        }
        if (p_vaddr_end > mem_size) {
            warnx("%s: %s: phdr[%u] program memory segment falls outside of"
                  " valid range (%llu (p_vaddr + p_memsz) > %zu (mem_size))",
                  bin_name, INV_EXE, ph_i, (unsigned long long)p_vaddr_end,
                  mem_size);
            goto mem_cleanup;
        }
        /*
         * Keep track of the highest byte of memory occupied by the program.
         */
        if (p_vaddr_end > e_end) {
            e_end = p_vaddr_end;
            /*
             * Double check result for host (caller) address space overflow.
             */
            if ((mem + e_end) < (mem + p_min_loadaddr)) {
              warnx("%s: %s: phdr[%u] address space overflow"
                    " ((mem + e_end) < (mem + p_min_loaddr))",
                    bin_name, INV_EXE, ph_i);
              goto mem_cleanup;
            }
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
        if (host_vaddr < (mem + p_min_loadaddr)) {
            warnx("%s: %s: phdr[%u] address space overflow (host_vaddr <"
                  " (mem + p_min_loaddr))", bin_name, INV_EXE, ph_i);
            goto mem_cleanup;
        }
        nbytes = pread_in_full(bin_fd, host_vaddr, p_filesz,
                phdr[ph_i].p_offset);
        if (nbytes < 0) {
            warnx("%s: phdr[%u] pread_in_full returned %zu", bin_name, ph_i,
                  nbytes);
            goto mem_cleanup;
        }
        if ((size_t)nbytes != p_filesz) {
            warnx("%s: phdr[%u] host file segment mismatched (pread_in_full"
                  " returned %zu != %llu (p_filesz))", bin_name, ph_i, nbytes,
                  (unsigned long long)p_filesz);
            goto mem_cleanup;
        }
        memset(host_vaddr + p_filesz, 0, p_memsz - p_filesz);

        /*
         * Memory protection flags should be applied to the aligned address
         * range (p_vaddr_start .. p_vaddr_end). Before we apply them, also
         * verify that the address range is aligned to the architectural page
         * size.
         */
        if (p_vaddr_start & (EM_PAGE_SIZE - 1)) {
            warnx("%s: %s: phdr[%u] p_vaddr_start not aligned with"
                  " architectural page size (%llu & (%u - 1))", bin_name,
                  INV_EXE, ph_i, (unsigned long long)p_vaddr_start,
                  EM_PAGE_SIZE - 1);
            goto mem_cleanup;
        }
        if (p_vaddr_end & (EM_PAGE_SIZE - 1)) {
            warnx("%s: %s: phdr[%u] p_vaddr_end not aligned with architectural"
                  " page size (%llu & (%u - 1))", bin_name, INV_EXE, ph_i,
                  (unsigned long long)p_vaddr_end, EM_PAGE_SIZE - 1);
            goto mem_cleanup;
        }
        int prot = PROT_NONE;
        if (phdr[ph_i].p_flags & PF_R)
            prot |= PROT_READ;
        if (phdr[ph_i].p_flags & PF_W)
            prot |= PROT_WRITE;
        if (phdr[ph_i].p_flags & PF_X)
            prot |= PROT_EXEC;
        if (prot & PROT_WRITE && prot & PROT_EXEC) {
            warnx("%s: %s: phdr[%u] requests WRITE and EXEC permissions",
                  bin_name, INV_EXE, ph_i);
            goto mem_cleanup;
        }
        assert(t_guest_mprotect != NULL);
        if (t_guest_mprotect(t_guest_mprotect_arg, p_vaddr_start, p_vaddr_end,
                    prot) == -1) {
            warnx("%s: mprotect failed", bin_name);
            goto mem_cleanup;
        }
    }

    free(ehdr);
    free(phdr);
    *p_entry = e_entry;
    *p_end = e_end;
    return;

mem_cleanup:
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
    if (ehdr == NULL) {
        warnx("%s: malloc(Elf64_Ehdr) returned NULL while loading note",
              bin_name);
        goto mem_cleanup;
    }
    nbytes = pread_in_full(bin_fd, ehdr, sizeof(Elf64_Ehdr), 0);
    if (nbytes < 0) {
        warnx("%s: pread_in_full for elf header while loading note returned"
              " %zu < 0", bin_name, nbytes);
        goto mem_cleanup;
    }
    if (nbytes != sizeof(Elf64_Ehdr)) {
        warnx("%s: %s: elf header does not match the expected size"
              " (%zu != %lu)", bin_name, INV_EXE, nbytes, sizeof(Elf64_Ehdr));
        goto mem_cleanup;
    }
    if (!ehdr_is_valid(ehdr)) {
        warnx("%s: %s: invalid or unsupported executable (elf header invalid"
              " while loading note)", bin_name, INV_EXE);
        goto mem_cleanup;
    }

    size_t ph_size = ehdr->e_phnum * ehdr->e_phentsize;
    phdr = malloc(ph_size);
    if (!phdr) {
        warnx("%s: malloc(ph_size) returned NULL while loading note", bin_name);
        goto mem_cleanup;
    }
    nbytes = pread_in_full(bin_fd, phdr, ph_size, ehdr->e_phoff);
    if (nbytes < 0) {
        warnx("%s: pread_in_full for ph_size returned %zu < 0 while loading"
              " note", bin_name, nbytes);
        goto mem_cleanup;
    }
    if ((size_t)nbytes != ph_size) {
        warnx("%s: %s: program header does not match the expected size"
              " (%zu != %zu while loading note)", bin_name, INV_EXE, nbytes,
              ph_size);
        goto mem_cleanup;
    }

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
        if (phdr[ph_i].p_filesz < sizeof(Elf64_Nhdr)) {
            /*
             * p_filesz is less than minimum possible size of a NOTE header,
             * reject the executable.
             */
            warnx("%s: %s: phdr[%u].p_filesz note does not fall within"
                  " valid size (%llu < %lu)", bin_name, INV_EXE, ph_i,
                  (unsigned long long)phdr[ph_i].p_filesz, sizeof(Elf64_Nhdr));
            goto mem_cleanup;
        }
        if (phdr[ph_i].p_filesz < sizeof nhdr)
            /*
             * p_filesz is less than minimum possible size of a Solo5 NOTE
             * header, ignore the note.
             */
            continue;
        nbytes = pread_in_full(bin_fd, &nhdr, sizeof nhdr,
                phdr[ph_i].p_offset);
        if (nbytes < 0) {
            warnx("%s: phdr[%u] pread_in_full returned %zu", bin_name, ph_i,
                  nbytes);
            goto mem_cleanup;
        }
        if (nbytes != sizeof nhdr) {
            warnx("%s: phdr[%u] bytes read mismatches note size (%zu != %lu)",
                  bin_name, ph_i, nbytes, sizeof nhdr);
            goto mem_cleanup;
        }
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
        if (nhdr.h.n_descsz < 1 || nhdr.h.n_descsz > max_note_size) {
            warnx("%s: %s: phdr[%u] note does not fall within valid size"
                  " (%u < 1 || %u > %zu)", bin_name, INV_EXE, ph_i,
                  nhdr.h.n_descsz, nhdr.h.n_descsz, max_note_size);
            goto mem_cleanup;
        }
        if (phdr[ph_i].p_filesz < sizeof nhdr + nhdr.h.n_descsz) {
            warnx("%s: %s: phdr[%u] note does not fall within valid size"
                  " (%llu < %lu)", bin_name, INV_EXE, ph_i,
                  (unsigned long long)phdr[ph_i].p_filesz,
                  sizeof nhdr + nhdr.h.n_descsz);
            goto mem_cleanup;
        }

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
    if(note_size <= 0 || note_size > nhdr.h.n_descsz) {
        warnx("%s: INV_EXE: phdr[%u] note does not fall within valid size"
              " (%zu <= 0 || %zu > %u)", bin_name, ph_i, note_size, note_size,
              nhdr.h.n_descsz);
        goto mem_cleanup;
    }
    note_data = malloc(note_size);
    if (note_data == NULL) {
        warnx("%s: phdr[%u] malloc(note_size) returned NULL", bin_name, ph_i);
        goto mem_cleanup;
    }
    nbytes = pread_in_full(bin_fd, note_data, note_size,
            phdr[ph_i].p_offset + note_offset);
    if (nbytes < 0) {
        warnx("%s: phdr[%u] pread_in_full for note_data returned %zu < 0",
              bin_name, ph_i, nbytes);
        goto mem_cleanup;
    }
    if ((size_t)nbytes != note_size) {
        warnx("%s: phdr[%u] bytes read mismatches note size (%zu != %zu)",
              bin_name, ph_i, nbytes, note_size);
        goto mem_cleanup;
    }

    *out_note_data = note_data;
    *out_note_size = note_size;
    free(ehdr);
    free(phdr);
    return 0;

mem_cleanup:
    free(ehdr);
    free(phdr);
    free(note_data);
    exit(1);
}
