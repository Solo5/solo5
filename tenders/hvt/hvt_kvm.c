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
 * hvt_kvm.c: Architecture-independent part of KVM backend implementation.
 */

#define _GNU_SOURCE
#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>
#include <linux/kvm.h>
#include <sys/personality.h>

#include "hvt.h"
#include "hvt_kvm.h"

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

    hvb->kvmfd = open("/dev/kvm", O_RDWR | O_CLOEXEC);
    if (hvb->kvmfd == -1)
        err(1, "Could not open: /dev/kvm");
    ret = ioctl(hvb->kvmfd, KVM_GET_API_VERSION, NULL);
    if (ret == -1)
        err(1, "KVM: ioctl (GET_API_VERSION) failed");
    if (ret != 12)
        errx(1, "KVM: API version is %d, solo5-hvt requires version 12", ret);
    hvb->vmfd = ioctl(hvb->kvmfd, KVM_CREATE_VM, 0);
    if (hvb->vmfd == -1)
        err(1, "KVM: ioctl (CREATE_VM) failed");

    hvb->vcpufd = ioctl(hvb->vmfd, KVM_CREATE_VCPU, 0);
    if (hvb->vcpufd == -1)
        err(1, "KVM: ioctl (CREATE_VCPU) failed");
    size_t runsize = ioctl(hvb->kvmfd, KVM_GET_VCPU_MMAP_SIZE, NULL);
    if (runsize == (size_t)-1)
        err(1, "KVM: ioctl (GET_VCPU_MMAP_SIZE) failed");
    if (runsize < sizeof(*hvb->vcpurun))
        errx(1, "KVM: invalid VCPU_MMAP_SIZE: %zd", runsize);
    hvb->vcpurun =
        mmap(NULL, runsize, PROT_READ | PROT_WRITE, MAP_SHARED, hvb->vcpufd,
             0);
    if (hvb->vcpurun == MAP_FAILED)
        err(1, "KVM: VCPU mmap failed");

    hvt->mem = mmap(NULL, mem_size, PROT_READ | PROT_WRITE,
               MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (hvt->mem == MAP_FAILED)
        err(1, "Error allocating guest memory");
    hvt->mem_size = mem_size;

    struct kvm_userspace_memory_region region = {
        .slot = 0,
        .guest_phys_addr = 0,
        .memory_size = hvt->mem_size,
        .userspace_addr = (uint64_t)hvt->mem,
    };
    ret = ioctl(hvb->vmfd, KVM_SET_USER_MEMORY_REGION, &region);
    if (ret == -1)
        err(1, "KVM: ioctl (SET_USER_MEMORY_REGION) failed");

    hvt->b = hvb;
    return hvt;
}

#if HVT_DROP_PRIVILEGES
void hvt_drop_privileges()
{
    /*
     * This function intentionally left mostly blank for now (see #282).
     */

    /*
     * Sooo... it turns out that at least on some distributions, the Linux
     * "personality" flag READ_IMPLIES_EXEC is the default unless linked with
     * -z noexecstack. This is bad, as it results in mmap() with PROT_READ
     *  implying PROT_EXEC. Cowardly refuse to run on such systems.
     */
    int persona = -1;
    persona = personality(0xffffffff);
    assert(persona >= 0);
    if (persona & READ_IMPLIES_EXEC)
        errx(1, "Cowardly refusing to run with a sys_personality of "
                "READ_IMPLIES_EXEC. Please report a bug, with details of your "
                "Linux distribution and GCC version");
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
     * KVM will propagate guest-side R/W protections to its EPT mappings,
     * guest-side X/NX protection is currently not supported by the hypervisor.
     */
    prot &= ~(PROT_EXEC);
    return mprotect(vaddr_start, size, prot);
}
