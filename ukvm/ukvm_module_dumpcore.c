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
 * Based on binutils-gdb/gdb/stubs/i386-stub.c, which is:
 * Not copyrighted.
 */

/*
 * ukvm_module_dumpcore.c: Dumps the unikernel memory as a core file
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
#include <stdbool.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include "ukvm.h"

#if defined(__linux__) && defined(__x86_64__)

#include "ukvm_dumpcore_kvm_x86_64.c"

#elif defined(__FreeBSD__) && defined(__x86_64__)

#include "ukvm_dumpcore_freebsd_x86_64.c"

#elif defined(__linux__) && defined(__aarch64__)

#include "ukvm_dumpcore_kvm_aarch64.c"

#else

#error Unsupported target

#endif

/*
 * the Unikernel's core format is:
 *   --------------
 *   |  elf header |
 *   --------------
 *   |  PT_NOTE    |
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
void ukvm_dumpcore(struct ukvm_hv *hv, struct ukvm_abort *info)
{
    int core_fd, num_notes = 0;
    size_t offset, note_size;
    Elf64_Ehdr hdr;
    Elf64_Phdr phdr;
    char filename[20] = {0};
    snprintf(filename, 20, "core.ukvm.%d", getpid());

    memset((void *)&hdr, 0, sizeof(Elf64_Ehdr));
    memset((void *)&phdr, 0, sizeof(Elf64_Phdr));

    core_fd = open(filename, O_RDWR|O_CREAT|O_TRUNC|O_APPEND, S_IRUSR|S_IWUSR);
    if (core_fd < 0) {
        goto failure;
    }

    memset(&hdr, 0, sizeof(Elf64_Ehdr));
    memcpy(&hdr, ELFMAG, SELFMAG);

    hdr.e_ident[EI_VERSION] = EV_CURRENT;
    hdr.e_type = ET_CORE;
    hdr.e_version = EV_CURRENT;
    hdr.e_machine = EM_X86_64;
    hdr.e_ehsize = sizeof(hdr);
    hdr.e_phoff = sizeof(hdr); 
    hdr.e_phentsize = sizeof(Elf64_Phdr);

    /* Fill in architecture specific contents for elf header */
    ukvm_dumpcore_fill_arch_header(&hdr);

    /* No section header in core file */
    hdr.e_shoff = 0;
    hdr.e_shentsize = 0;
    hdr.e_shnum = 0;
    hdr.e_shstrndx = 0;

    note_size = ukvm_dumpcore_get_note_size(&num_notes);
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
        ukvm_dumpcore_dump_notes(core_fd, hv, info) < 0) {
        goto failure;
    }

    /* Write memory */
    if (write(core_fd, hv->mem, hv->mem_size) < 0) {
        goto failure;
    }
    warnx("Dumped an unikernel core file: %s", filename);
    goto cleanup;

failure:
    warnx("%s: Failed to generate the core file", __FUNCTION__);

cleanup:
    close(core_fd);
}

static void hypercall_dumpcore(struct ukvm_hv *hv, ukvm_gpa_t gpa)
{
    struct ukvm_abort *t =
        UKVM_CHECKED_GPA_P(hv, gpa, sizeof (struct ukvm_abort));

    if (ukvm_dumpcore_get_regs(hv) == 0) {
        ukvm_dumpcore(hv, t);
    }
}

static int setup(struct ukvm_hv *hv)
{
    assert(ukvm_core_register_hypercall(UKVM_HYPERCALL_ABORT,
        hypercall_dumpcore) == 0);

    return 0;
}

struct ukvm_module ukvm_module_dumpcore = {
    .name = "dumpcore",
    .setup = setup,
    .handle_cmdarg = NULL,
    .usage = NULL 
};

