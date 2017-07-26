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
