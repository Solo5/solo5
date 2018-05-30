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
#include <sys/uio.h>
#include "ukvm.h"

#if defined(__linux__) && defined(__x86_64__)

#include "ukvm_dumpcore_kvm_x86_64.c"
#define EM_HOST EM_X86_64
typedef unsigned char *host_mvec_t;

#elif defined(__FreeBSD__) && defined(__x86_64__)

#include "ukvm_dumpcore_freebsd_x86_64.c"
#define EM_HOST EM_X86_64
typedef char *host_mvec_t;

#elif defined(__linux__) && defined(__aarch64__)

#include "ukvm_dumpcore_kvm_aarch64.c"
#define EM_HOST EM_AARCH64
typedef unsigned char *host_mvec_t;

#else

#error Unsupported target

#endif

static bool use_dumpcore = false;

void ukvm_dumpcore(struct ukvm_hv *hv, int status, void *cookie)
{
    if (!use_dumpcore || status == 0) {
        return;
    }

    /* Don't proceed further if the architecture is not supported */
    if (!ukvm_is_arch_supported()) {
        warnx("Not dumping corefile since the architecture is not supported\n");
        return;
    }

    char filename[20] = { 0 };
    snprintf(filename, sizeof filename, "core.ukvm.%d", getpid());
    /*
     * Note that O_APPEND must not be set as this modifies the behaviour of
     * pwrite() on Linux.
     */
    int fd = open(filename, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        warn("dumpcore: open(%s)", filename);
        goto failure;
    }
    warnx("dumpcore: dumping guest core to: %s", filename);

    /*
     * Core file structure:
     * (1) ELF header with e_type=ET_CORE
     */
    size_t offset = sizeof (Elf64_Ehdr);
    Elf64_Ehdr ehdr = {
        .e_ident = {
            ELFMAG0, ELFMAG1, ELFMAG2, ELFMAG3,
            ELFCLASS64, ELFDATA2LSB,
            EV_CURRENT, ELFOSABI_STANDALONE, 0
        },
        .e_type = ET_CORE,
        .e_version = EV_CURRENT,
        .e_machine = EM_HOST,
        .e_ehsize = sizeof (Elf64_Ehdr),
        .e_phnum = 2, /* PT_NOTE, PT_LOAD */
        .e_phentsize = sizeof (Elf64_Phdr),
        .e_phoff = offset,
    };
    offset += ehdr.e_phnum * sizeof (Elf64_Phdr);

    /*
     * (2) PT_NOTE pointing to NT_PRSTATUS descriptor
     * name[] must be a multiple of the ELF word size, SVR4 uses "CORE".
     */
    const char name[8] = "CORE";
    size_t pnote_size = sizeof (Elf64_Nhdr) + sizeof name \
                        + ukvm_dumpcore_prstatus_size();
    Elf64_Phdr pnote = {
        .p_type = PT_NOTE,
        .p_filesz = pnote_size,
        .p_memsz = pnote_size,
        .p_offset = offset,
    };
    offset += pnote_size;

    /*
     * (3) PT_LOAD pointing to guest memory dump
     */
    Elf64_Phdr pload = {
        .p_type = PT_LOAD,
        .p_align = 0, .p_paddr = 0, .p_vaddr = 0,
        .p_memsz = hv->mem_size,
        .p_filesz = hv->mem_size,
        .p_flags = 0,
        .p_offset = offset
    };

    /*
     * (4) NT_PRSTATUS descriptor
     */
    Elf64_Nhdr nhdr = {
        .n_type = NT_PRSTATUS,
        .n_namesz = sizeof name,
        .n_descsz = ukvm_dumpcore_prstatus_size(),
    };

    struct iovec iov[] = {
        { .iov_base = &ehdr, .iov_len = sizeof ehdr },
        { .iov_base = &pnote, .iov_len = sizeof pnote },
        { .iov_base = &pload, .iov_len = sizeof pload },
        { .iov_base = &nhdr, .iov_len = sizeof nhdr },
        { .iov_base = (void *)name, .iov_len = nhdr.n_namesz }
    };
    size_t iovlen = sizeof ehdr + sizeof pnote + sizeof pload + sizeof nhdr \
                    + nhdr.n_namesz;
    if (writev(fd, iov, 5) != iovlen) {
        warn("dumpcore: Error writing ELF headers");
        goto failure;
    }

    /*
     * (5) NT_PRSTATUS content
     */
    if (ukvm_dumpcore_write_prstatus(fd, hv, cookie) < 0) {
        warnx("dumpcore: Could not retrieve guest state");
        goto failure;
    }

    /*
     * (6) guest memory dump
     *
     * We use mincore() to get the host kernel's view of which pages have
     * actually been touched by the guest. This speeds up the process of writing
     * out the core file significantly by reducing memory pressure on the host
     * and writing out a sparse file.
     *
     * Note that mincore() is definitely not portable, but the "mvec[pg] & 1"
     * construct should be portable across at least Linux and FreeBSD.
     */
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size == -1) {
        warn("dumpcore: Could not determine _SC_PAGESIZE");
        goto failure;
    }
    assert (hv->mem_size % page_size == 0);
    size_t npages = hv->mem_size / page_size;
    size_t ndumped = 0;
    host_mvec_t mvec = malloc(npages);
    assert (mvec);
    if (mincore(hv->mem, hv->mem_size, mvec) == -1) {
        warn("dumpcore: mincore() failed");
        goto failure;
    }
    off_t start = lseek(fd, 0, SEEK_CUR);
    for (size_t pg = 0; pg < npages; pg++) {
        if (mvec[pg] & 1) {
            off_t pgoff = (pg * page_size);
            ssize_t nbytes =
                pwrite(fd, hv->mem + pgoff, page_size, start + pgoff);
            if (nbytes == -1) {
                warn("dumpcore: Error dumping guest memory page %zd", pg);
                free(mvec);
                goto failure;
            }
            else if (nbytes != page_size) {
                warnx("dumpcore: Short write dumping guest memory page"
                        "%zd: %zd bytes", pg, nbytes);
                free(mvec);
                goto failure;
            }
            ndumped++;
        }
    }
    free(mvec);
    warnx("dumpcore: dumped %zd pages of total %zd pages", ndumped, npages);
    close(fd);
    return;

failure:
    warnx("dumpcore: error(s) dumping core, file may be incomplete");
    close(fd);
}

static int handle_cmdarg(char *cmdarg)
{
    if (!strcmp("--dumpcore", cmdarg)) {
        use_dumpcore = true;
        return 0;
    }
    return -1;
}

static char *usage(void)
{
    return "--dumpcore";
}

static int setup(struct ukvm_hv *hv)
{
    assert(ukvm_core_register_shutdown_hook(ukvm_dumpcore) == 0);

    return 0;
}

struct ukvm_module ukvm_module_dumpcore = {
    .name = "dumpcore",
    .setup = setup,
    .handle_cmdarg = handle_cmdarg,
    .usage = usage
};
