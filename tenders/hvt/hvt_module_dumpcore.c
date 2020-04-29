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
 * hvt_module_dumpcore.c: Dumps the unikernel memory as a core file
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

#include "hvt.h"

#if defined(__linux__) && defined(__x86_64__)

#include "hvt_dumpcore_kvm_x86_64.c"
#define EM_HOST EM_X86_64
typedef unsigned char *host_mvec_t;

#elif defined(__FreeBSD__) && defined(__x86_64__)

#if HVT_FREEBSD_ENABLE_CAPSICUM
#include <sys/capsicum.h>
#endif
#include "hvt_dumpcore_freebsd_x86_64.c"
#define EM_HOST EM_X86_64
typedef char *host_mvec_t;

#elif defined(__OpenBSD__) && defined(__x86_64__)

#include "hvt_dumpcore_openbsd_x86_64.c"
#define EM_HOST EM_X86_64
typedef unsigned char *host_mvec_t;

#elif defined(__linux__) && defined(__aarch64__)

#include "hvt_dumpcore_kvm_aarch64.c"
#define EM_HOST EM_AARCH64
typedef unsigned char *host_mvec_t;

#else

#error Unsupported target

#endif

static char *dumpcoredir;
static int dir;

void hvt_dumpcore_hook(struct hvt *hvt, int status, void *cookie)
{
    if (status != 255) /* SOLO5_EXIT_ABORT */
        return;

    char *filename;
    assert(asprintf(&filename, "core.solo5-hvt.%d", getpid()) != -1);
    /*
     * Note that O_APPEND must not be set as this modifies the behaviour of
     * pwrite() on Linux.
     */
    int fd = openat(dir, filename, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
    close(dir);
    if (fd < 0) {
        warn("dumpcore: open(%s)", filename);
        goto failure;
    }
    warnx("dumpcore: dumping guest core to: %s/%s", dumpcoredir, filename);

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
                        + hvt_dumpcore_prstatus_size();
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
        .p_memsz = hvt->mem_size,
        .p_filesz = hvt->mem_size,
        .p_flags = 0,
        .p_offset = offset
    };

    /*
     * (4) NT_PRSTATUS descriptor
     */
    Elf64_Nhdr nhdr = {
        .n_type = NT_PRSTATUS,
        .n_namesz = sizeof name,
        .n_descsz = hvt_dumpcore_prstatus_size(),
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
    if (hvt_dumpcore_write_prstatus(fd, hvt, cookie) < 0) {
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
    assert (hvt->mem_size % page_size == 0);
    size_t npages = hvt->mem_size / page_size;
    size_t ndumped = 0;
    host_mvec_t mvec = malloc(npages);
    assert (mvec);
    if (mincore(hvt->mem, hvt->mem_size, mvec) == -1) {
        warn("dumpcore: mincore() failed");
        goto failure;
    }
    off_t start = lseek(fd, 0, SEEK_CUR);
    for (size_t pg = 0; pg < npages; pg++) {
        if (mvec[pg] & 1) {
            off_t pgoff = (pg * page_size);
            ssize_t nbytes =
                pwrite(fd, hvt->mem + pgoff, page_size, start + pgoff);
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

static int handle_cmdarg(char *cmdarg, struct mft *mft)
{
    if (strncmp("--dumpcore=", cmdarg, 11))
	return -1;
    dumpcoredir = cmdarg + 11;

    return 0;
}

static char *usage(void)
{
    return "--dumpcore=DIR (enable guest core dump on abort/trap)";
}

static int setup(struct hvt *hvt, struct mft *mft)
{
    if (dumpcoredir == NULL)
        return 0; /* Not present */

    dir = open(dumpcoredir, O_RDONLY | O_DIRECTORY);
    if (dir == -1)
        errx(1, "dumpcore: cannot open dir");

    if (access(dumpcoredir, W_OK) == -1)
        errx(1, "dumpcore: dir not writable");

    if (hvt_core_register_halt_hook(hvt_dumpcore_hook) == -1)
        return -1;

    if (hvt_dumpcore_supported() == -1)
        errx(1, "dumpcore: not implemented for this backend/architecture");

#if HVT_FREEBSD_ENABLE_CAPSICUM
    cap_rights_t rights;
    cap_rights_init(&rights, CAP_CREATE, CAP_WRITE, CAP_LOOKUP, CAP_SEEK);
    if (cap_rights_limit(dir, &rights) == -1)
        err(1, "cap_rights_limit() failed");
#endif

    return 0;
}

DECLARE_MODULE(dumpcore,
    .setup = setup,
    .handle_cmdarg = handle_cmdarg,
    .usage = usage
)
