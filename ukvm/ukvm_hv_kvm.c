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
 * ukvm_hv_kvm.c: Architecture-independent part of KVM backend implementation.
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

#include "ukvm.h"
#include "ukvm_hv_kvm.h"

struct ukvm_hv *ukvm_hv_init(size_t mem_size)
{
    int ret;

    struct ukvm_hv *hv = malloc(sizeof (struct ukvm_hv));
    if (hv == NULL)
        err(1, "malloc");
    memset(hv, 0, sizeof (struct ukvm_hv));
    struct ukvm_hvb *hvb = malloc(sizeof (struct ukvm_hvb));
    if (hvb == NULL)
        err(1, "malloc");
    memset(hvb, 0, sizeof (struct ukvm_hvb));

    hvb->kvmfd = open("/dev/kvm", O_RDWR | O_CLOEXEC);
    if (hvb->kvmfd == -1)
        err(1, "Could not open: /dev/kvm");
    ret = ioctl(hvb->kvmfd, KVM_GET_API_VERSION, NULL);
    if (ret == -1)
        err(1, "KVM: ioctl (GET_API_VERSION) failed");
    if (ret != 12)
        errx(1, "KVM: API version is %d, ukvm requires version 12", ret);
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

    hv->mem = mmap(NULL, mem_size, PROT_READ | PROT_WRITE,
               MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (hv->mem == MAP_FAILED)
        err(1, "Error allocating guest memory");
    hv->mem_size = mem_size;

    struct kvm_userspace_memory_region region = {
        .slot = 0,
        .guest_phys_addr = 0,
        .memory_size = hv->mem_size,
        .userspace_addr = (uint64_t)hv->mem,
    };
    ret = ioctl(hvb->vmfd, KVM_SET_USER_MEMORY_REGION, &region);
    if (ret == -1)
        err(1, "KVM: ioctl (SET_USER_MEMORY_REGION) failed");

    hv->b = hvb;
    return hv;
}
