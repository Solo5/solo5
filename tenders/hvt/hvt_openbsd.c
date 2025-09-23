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
 * hvt_openbsd.c: Architecture-independent part of OpenBSD vmm(4) backend
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
#include <machine/vmmvar.h>
#include <sys/param.h>

#include "hvt_openbsd_config.h"
#ifdef HAVE_VMM_H
#include <dev/vmm/vmm.h>
#endif

#include "hvt.h"
#include "hvt_openbsd.h"

/*
 * TODO: To ensure that the VM is correctly destroyed on shutdown (normal or
 * not) we currently install an atexit() handler. The top-level API will need
 * to be changed to accomodate this, e.g. by introducing a hvt_shutdown(),
 * however this is incompatible with the current "fail fast" approach to
 * internal error handling.
 */
static struct hvt *cleanup_hvt;

static void cleanup_vmd_fd(void)
{
    if (cleanup_hvt != NULL && cleanup_hvt->b->vmd_fd != -1) {
        if(close(cleanup_hvt->b->vmd_fd) == -1)
            warn("close vmd_fd failed - still exiting");
    }
}

static void cleanup_vm(void)
{
    if (cleanup_hvt != NULL && cleanup_hvt->b->vcp_id != -1) {
        struct vm_terminate_params vtp = {.vtp_vm_id = cleanup_hvt->b->vcp_id};
        if (ioctl(cleanup_hvt->b->vmd_fd, VMM_IOC_TERM, &vtp) == -1)
            warn("terminate vmm ioctl failed - still exiting");
    }
}

struct hvt *hvt_init(size_t mem_size)
{
    struct hvt *hvt;
    struct hvt_b *hvb;
    struct vm_create_params *vcp;
    struct vm_mem_range *vmr;
#if OpenBSD <= 202504
    void *p;
#endif

    if(geteuid() != 0) {
        errno = EPERM;
        err(1, "need root privileges");
    }

    hvt = calloc(1, sizeof (struct hvt));
    if (hvt == NULL)
        err(1, "calloc hv");

    hvb = calloc(1, sizeof (struct hvt_b));
    if (hvb == NULL)
        err(1, "calloc");

    hvt->b = hvb;
    hvb->vmd_fd = -1;

    hvb->vmd_fd = open(VMM_NODE, O_RDWR);
    if (hvb->vmd_fd == -1)
        err(1, "VMM_NODE");

    cleanup_hvt = hvt;
    atexit(cleanup_vmd_fd);

    vcp = calloc(1, sizeof (struct vm_create_params));
    if (vcp == NULL)
        err(1, "calloc");

    vcp->vcp_ncpus = 1; // vmm only supports 1 cpu for now.
    int size = snprintf(vcp->vcp_name, VMM_MAX_NAME_LEN, "solo5-%d", getpid());
    if (size == -1)
        err(1, "snprintf");

    vcp->vcp_nmemranges = 1;
    vcp->vcp_memranges[0].vmr_gpa = 0x0;
    vcp->vcp_memranges[0].vmr_size = mem_size;

    vmr = &vcp->vcp_memranges[0];
#ifdef OpenBSD <= 202504
    p = mmap(NULL, vmr->vmr_size, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANON, -1, 0);
    if (p == MAP_FAILED)
        err(1, "mmap");

    vmr->vmr_va = (vaddr_t)p;
    hvt->mem = p;
    hvt->mem_size = mem_size;
#endif

    if (ioctl(hvb->vmd_fd, VMM_IOC_CREATE, vcp) < 0)
        err(1, "create vmm ioctl failed - exiting");

#ifdef OpenBSD > 202504
    hvt->mem = (uint8_t *)vmr->vmr_va;
    hvt->mem_size = mem_size;
#endif
    hvb->vcp_id = vcp->vcp_id;
    hvb->vcpu_id = 0; // the first and only cpu is at 0

    atexit(cleanup_vm);

    return hvt;
}

#if HVT_DROP_PRIVILEGES
void hvt_drop_privileges()
{
    struct passwd *pw = getpwnam(VMD_USER);
    if (pw == NULL)
        err(1, "can't get _vmd user");
    uid_t uid = pw->pw_uid;
    gid_t gid = pw->pw_gid;

    if (chroot(pw->pw_dir) == -1)
        err(1, "chroot(%s)", pw->pw_dir);

    if (chdir("/") == -1)
        err(1, "chdir(/)");

    if (setgroups(1, &gid) ||
            setresgid(gid, gid, gid) ||
            setresuid(uid, uid, uid))
        err(1, "unable to revoke privs");

    /*
     * pledge in the vm processes:
     * stdio - for malloc and basic I/O including events.
     * vmm - for the vmm ioctls and operations.
     */
    if (pledge("stdio vmm", NULL) == -1)
        err(errno, "pledge");
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

#if defined(VMM_IOC_MPROTECT_EPT)
    /*
     * Host-side page protections:
     *
     * Ensure that guest-executable pages are not also executable, but are
     * readable in the host.
     */
    int host_prot = prot;
    host_prot &= ~(PROT_EXEC);
    host_prot |= PROT_READ;
    if(mprotect(vaddr_start, size, host_prot) == -1)
        return -1;

    /*
     * Guest-side page protections:
     */
    struct hvt_b *hvb = hvt->b;
    struct vm_mprotect_ept_params *vmep;

    vmep = calloc(1, sizeof (struct vm_mprotect_ept_params));
    if (vmep == NULL) {
        warn("calloc");
        return -1;
    }

    vmep->vmep_vm_id = hvb->vcp_id;
    vmep->vmep_vcpu_id = hvb->vcpu_id;

    vmep->vmep_sgpa = addr_start;
    vmep->vmep_size = size;
    vmep->vmep_prot = prot;

    if (ioctl(hvb->vmd_fd, VMM_IOC_MPROTECT_EPT, vmep) < 0) {
       err(1, "mprotect ept vmm ioctl failed - exiting");
    }
#endif
    return 0;
}
