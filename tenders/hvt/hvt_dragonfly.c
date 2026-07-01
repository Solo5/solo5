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
 * hvt_dragonfly.c: Architecture-independent part of DragonFly nvmm(4) backend
 * implementation.
 */

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define _WITH_DPRINTF
#include <stdio.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <nvmm.h>
#include <sys/param.h>

#include "hvt.h"
#include "hvt_dragonfly.h"

struct hvt *hvt_init(size_t mem_size)
{
    struct hvt *hvt = malloc(sizeof(struct hvt));
    if (hvt == NULL)
        err(EXIT_FAILURE, "malloc");
    memset(hvt, 0, sizeof(struct hvt));

    struct hvt_b *hvb = malloc(sizeof(struct hvt_b));
    if (hvb == NULL)
        err(EXIT_FAILURE, "malloc");
    memset(hvb, 0, sizeof(struct hvt_b));

    hvt->b = hvb;
    hvb->kick_net_pipe[0] = -1;
    hvb->kick_net_pipe[1] = -1;

    if (nvmm_init() == -1)
        err(EXIT_FAILURE, "unable to init nvmm");

    if (nvmm_machine_create(&hvb->mach) == -1)
        err(EXIT_FAILURE, "unable to create nvmm machine");

    if (nvmm_vcpu_create(&hvb->mach, 0, &hvb->vcpu) == -1)
        err(EXIT_FAILURE, "unable to create vcpu");

    hvt->mem = mmap(NULL, mem_size, PROT_READ | PROT_WRITE,
                    MAP_ANON | MAP_PRIVATE, -1, 0);
    if (hvt->mem == MAP_FAILED)
        err(EXIT_FAILURE, "unable to mmap");
    hvt->guest_mem_size = mem_size;
    hvt->mem_alloc_size = mem_size;

    if (nvmm_hva_map(&hvb->mach, (uintptr_t)hvt->mem, mem_size) == -1)
        err(EXIT_FAILURE, "unable to map HVA");

    if (nvmm_gpa_map(&hvb->mach, (uintptr_t)hvt->mem, 0, mem_size,
                     PROT_READ | PROT_WRITE | PROT_EXEC) == -1)
        err(EXIT_FAILURE, "unable to map GPA");

    return hvt;
}

#if HVT_DROP_PRIVILEGES
void hvt_drop_privileges()
{
}
#endif

int hvt_guest_mprotect(void *t_arg, uint64_t addr_start, uint64_t addr_end,
                       int prot)
{
    struct hvt *hvt = t_arg;

    assert(addr_start <= hvt->guest_mem_size);
    assert(addr_end <= hvt->guest_mem_size);
    assert(addr_start < addr_end);

    uint8_t *vaddr_start = hvt->mem + addr_start;
    assert(vaddr_start >= hvt->mem);
    size_t size = addr_end - addr_start;
    assert(size > 0 && size <= hvt->guest_mem_size);

    /*
     * Host-side page protections:
     *
     * Ensure that guest-executable pages are not also executable but are
     * readable in the host.
     *
     * Guest-side page protections:
     *
     * Manipulating guest-side (EPT) mappings is currently not supported by
     * DragonFly nvmm, so there is nothing more we can do.
     */
    if (prot & PROT_EXEC) {
        prot &= ~(PROT_EXEC);
        prot |= PROT_READ;
    }
    return mprotect(vaddr_start, size, prot);
}
