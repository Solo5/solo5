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
 * hvt_freebsd.c: Architecture-independent part of FreeBSD vmm(4) backend
 * implementation.
 */

#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define _WITH_DPRINTF
#include <stdio.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <machine/vmm.h>
#include <sys/param.h>
#include <sys/cpuset.h>
#include <machine/vmm_dev.h>

#if HVT_FREEBSD_ENABLE_CAPSICUM
#include <sys/capsicum.h>
#include <sys/nv.h>
#include <libcasper.h>
#include <casper/cap_sysctl.h>
#include <capsicum_helpers.h>
#endif

#include "hvt.h"
#include "hvt_freebsd.h"

/*
 * TODO: To ensure that the VM is correctly destroyed on shutdown (normal or
 * not) we currently install an atexit() handler. The top-level API will need
 * to be changed to accomodate this, e.g. by introducing a hvt_shutdown(),
 * however this is incompatible with the current "fail fast" approach to
 * internal error handling.
 */
static struct hvt *cleanup_hvt;

#if HVT_FREEBSD_ENABLE_CAPSICUM
static cap_channel_t *capsysctl;
#endif

static void cleanup_vm(void)
{
    if (cleanup_hvt != NULL) {
#if HVT_FREEBSD_ENABLE_CAPSICUM
       cap_sysctlbyname(capsysctl, "hw.vmm.destroy", NULL, NULL,
                cleanup_hvt->b->vmname, strlen(cleanup_hvt->b->vmname));
       cap_close(capsysctl);
#else
       sysctlbyname("hw.vmm.destroy", NULL, NULL, cleanup_hvt->b->vmname,
                strlen(cleanup_hvt->b->vmname));
#endif
    }
}

static void cleanup_vmfd(void)
{
    if (cleanup_hvt != NULL && cleanup_hvt->b->vmfd != -1)
        close(cleanup_hvt->b->vmfd);
}

struct hvt *hvt_init(size_t mem_size)
{
    int ret;

    struct hvt *hvt = malloc(sizeof (struct hvt));
    if (hvt == NULL)
        err(1, "malloc");
    memset(hvt, 0, sizeof (struct hvt));
    struct hvt_b *hvb = malloc(sizeof (struct hvt_b));
    if (hvb == NULL)
        err(1, "malloc");
    memset(hvb, 0, sizeof (struct hvt_b));
    hvt->b = hvb;
    hvb->vmfd = -1;

    int namelen = asprintf(&hvb->vmname, "solo5-%d", getpid());
    if (namelen == -1)
        err(1, "asprintf");
    ret = sysctlbyname("hw.vmm.create", NULL, NULL, hvb->vmname, namelen);
    if (ret == -1)
        err(1, "Cannot create VM '%s'", hvb->vmname);
    cleanup_hvt = hvt;
    atexit(cleanup_vm);

    char *vmmdevname;
    namelen = asprintf(&vmmdevname, "/dev/vmm/%s", hvb->vmname);
    if (namelen == -1)
        err(1, "asprintf");
    hvb->vmfd = open(vmmdevname, O_RDWR, 0);
    if (hvb->vmfd == -1)
        err(1, "open(%s)", vmmdevname);
    atexit(cleanup_vmfd);

    struct vm_capability vmcap = {
        .cpuid = 0, .captype = VM_CAP_HALT_EXIT, .capval = 1
    };
    ret = ioctl(hvb->vmfd, VM_SET_CAPABILITY, &vmcap);
    if (ret == -1)
        err(1, "set VM_CAP_HALT_EXIT");

    struct vm_memseg memseg = {
        .segid = 0, .len = mem_size
    };
    ret = ioctl(hvb->vmfd, VM_ALLOC_MEMSEG, &memseg);
    if (ret == -1)
        err(1, "VM_ALLOC_MEMSEG");

    struct vm_memmap memmap = {
        .gpa = 0, .len = mem_size, .segid = 0, .segoff = 0,
        .prot = PROT_READ | PROT_WRITE | PROT_EXEC, .flags = 0
    };
    ret = ioctl(hvb->vmfd, VM_MMAP_MEMSEG, &memmap);
    if (ret == -1)
        err(1, "VM_MMAP_MEMSEG");

    hvt->mem = mmap(NULL, mem_size, PROT_READ | PROT_WRITE, MAP_SHARED,
            hvb->vmfd, 0);
    if (hvt->mem == MAP_FAILED)
        err(1, "mmap");
    hvt->mem_size = mem_size;

#if HVT_FREEBSD_ENABLE_CAPSICUM
    cap_rights_t rights;
    const cap_ioctl_t cmds[] = { VM_RUN, VM_SET_CAPABILITY, VM_ALLOC_MEMSEG,
                                 VM_MMAP_MEMSEG, VM_SET_SEGMENT_DESCRIPTOR,
                                 VM_SET_REGISTER, VM_GET_REGISTER,
                                 VM_ACTIVATE_CPU };
    cap_rights_init(&rights, CAP_IOCTL, CAP_MMAP_RW);
    if (cap_rights_limit(hvb->vmfd, &rights) == -1)
        err(1, "cap_rights_limit() failed");
    if (cap_ioctls_limit(hvb->vmfd, cmds, nitems(cmds)) == -1)
        err(1, "cap_ioctls_limit() failed");

    cap_channel_t *capcas;
    capcas = cap_init();
    if (capcas == NULL)
        err(1, "Unable to contact Casper");

    capsysctl = cap_service_open(capcas, "system.sysctl");
    if (capsysctl == NULL)
        err(1, "Unable to open system.sysctl service");

    cap_close(capcas);

    nvlist_t *limits;
    limits = nvlist_create(0);

    nvlist_add_number(limits, "hw.vmm.destroy", CAP_SYSCTL_WRITE);
    if (cap_limit_set(capsysctl, limits) < 0)
        err(1, "Unable to set limits");

    if (caph_limit_stdout() == -1 || caph_limit_stderr() == -1)
        err(1, "Unable to apply rights for sandbox");
#endif

    return hvt;
}

#if HVT_DROP_PRIVILEGES
void hvt_drop_privileges()
{
#if HVT_FREEBSD_ENABLE_CAPSICUM
    if (caph_enter() == -1)
        err(1, "Unable to enter sandbox: cap_enter() failed");
#endif
}
#endif

int hvt_guest_mprotect(void *t_arg, uint64_t addr_start, uint64_t addr_end,
        int prot)
{
    struct hvt *hvt = t_arg;

    assert(addr_start <= hvt->mem_size);
    assert(addr_end <= hvt->mem_size);
    assert(addr_start < addr_end);

    uint8_t *vaddr_start = hvt->mem + addr_start;
    assert(vaddr_start >= hvt->mem);
    size_t size = addr_end - addr_start;
    assert(size > 0 && size <= hvt->mem_size);

    /*
     * Host-side page protections:
     *
     * Ensure that guest-executable pages are not also executable in the host.
     *
     * Guest-side page protections:
     *
     * Manipulating guest-side (EPT) mappings is currently not supported by
     * FreeBSD vmm, so there is nothing more we can do.
     */
    prot &= ~(PROT_EXEC);
    return mprotect(vaddr_start, size, prot);
}
