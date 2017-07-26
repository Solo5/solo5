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
 * ukvm_hv_kvm_aarch64.c: aarch64 architecture-dependent part of KVM backend
 * implementation.
 */

#define _GNU_SOURCE
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>

#include <linux/kvm.h>

#include "ukvm.h"
#include "ukvm_hv_kvm.h"
#include "ukvm_cpu_aarch64.h"

/*
 * KVM/ARM64 provides an interface to userspace to modify the
 * VM registers. This interface describe the register by index.
 * We have to define the index here for those registers that we
 * will modify.
 */

/* Normal registers are mapped as coprocessor 16. */
#define KVM_REG_ARM_CORE    (0x0010 << KVM_REG_ARM_COPROC_SHIFT)

#define ARM64_CORE_REG(x)   \
            (KVM_REG_ARM64 | KVM_REG_SIZE_U64 | \
            KVM_REG_ARM_CORE | KVM_REG_ARM_CORE_REG(x))

/* Architectural Feature Access Control Register EL1 */
#define CPACR_EL1           ARM64_SYS_REG(3, 0, 1, 0, 2)
#define _FPEN_NOTRAP        0x3
#define _FPEN_SHIFT         20
#define _FPEN_MASK          GENMASK32(21, 20)

static int aarch64_set_one_register(int vcpufd, uint64_t id, uint64_t data)
{
    struct kvm_one_reg one_reg = {
        .id   = id,
        .addr = (uint64_t)&data,
    };

    return ioctl(vcpufd, KVM_SET_ONE_REG, &one_reg);
}

static int aarch64_get_one_register(int vcpufd, uint64_t id, uint64_t *pdata)
{
    struct kvm_one_reg one_reg = {
        .id   = id,
        .addr = (uint64_t)pdata,
    };

    return ioctl(vcpufd, KVM_GET_ONE_REG, &one_reg);
}

static void aarch64_enable_guest_float(int vcpufd)
{
    int ret;
    uint64_t data;

    /* Enable the floating-point and Advanced SIMD registers for Guest */
    ret = aarch64_get_one_register(vcpufd, CPACR_EL1, &data);
    if (ret == -1)
         err(1, "KVM: Get Architectural Feature Access Control Register failed");

    data &= ~(_FPEN_MASK);
    data |= (_FPEN_NOTRAP << _FPEN_SHIFT);
    ret = aarch64_set_one_register(vcpufd, CPACR_EL1, data);
    if (ret == -1)
         err(1, "KVM: Enable the floating-point and Advanced SIMD for Guest failed");
}

static void aarch64_setup_preferred_target(int vmfd, int vcpufd)
{
    int ret;
    struct kvm_vcpu_init init;

    ret = ioctl(vmfd, KVM_ARM_PREFERRED_TARGET, &init);
    if (ret == -1)
        err(1, "KVM: ioctl (KVM_ARM_PREFERRED_TARGET) failed");

    ret = ioctl(vcpufd, KVM_ARM_VCPU_INIT, &init);
    if (ret == -1)
        err(1, "KVM: ioctl (KVM_ARM_VCPU_INIT) failed");
}
