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

/* Memory Attribute Indirection Register EL1 */
#define MAIR_EL1            ARM64_SYS_REG(3, 0, 10, 2, 0)

#define MAIR(attr, mt)      (_AC(attr, UL) << ((mt) * 8))

#define MAIR_EL1_INIT       \
        MAIR(0x00, MT_DEVICE_nGnRnE) | MAIR(0x04, MT_DEVICE_nGnRE) | \
        MAIR(0x0C, MT_DEVICE_GRE) | MAIR(0x44, MT_NORMAL_NC) | \
        MAIR(0xFF, MT_NORMAL) | MAIR(0xBB, MT_NORMAL_WT)

/* Translation Control Register EL1 */
#define TCR_EL1             ARM64_SYS_REG(3, 0, 2, 0, 2)

/*
 * TCR flags.
 */
#define TCR_T0SZ_OFFSET     0
#define TCR_T1SZ_OFFSET     16
#define TCR_T0SZ(x)         ((_AC(64, UL) - (x)) << TCR_T0SZ_OFFSET)
#define TCR_T1SZ(x)         ((_AC(64, UL) - (x)) << TCR_T1SZ_OFFSET)
#define TCR_TxSZ(x)         (TCR_T0SZ(x) | TCR_T1SZ(x))

#define TCR_IRGN0_SHIFT     8
#define TCR_IRGN0_WBWA      (_AC(1, UL) << TCR_IRGN0_SHIFT)
#define TCR_IRGN1_SHIFT     24
#define TCR_IRGN1_WBWA      (_AC(1, UL) << TCR_IRGN1_SHIFT)
#define TCR_IRGN_WBWA       (TCR_IRGN0_WBWA | TCR_IRGN1_WBWA)

#define TCR_ORGN0_SHIFT     10
#define TCR_ORGN0_WBWA      (_AC(1, UL) << TCR_ORGN0_SHIFT)
#define TCR_ORGN1_SHIFT     26
#define TCR_ORGN1_WBWA      (_AC(1, UL) << TCR_ORGN1_SHIFT)
#define TCR_ORGN_WBWA       (TCR_ORGN0_WBWA | TCR_ORGN1_WBWA)

#define TCR_SH0_SHIFT       12
#define TCR_SH0_INNER       (_AC(3, UL) << TCR_SH0_SHIFT)
#define TCR_SH1_SHIFT       28
#define TCR_SH1_INNER       (_AC(3, UL) << TCR_SH1_SHIFT)
#define TCR_SHARED          (TCR_SH0_INNER | TCR_SH1_INNER)

#define TCR_TG0_SHIFT       14
#define TCR_TG0_4K          (_AC(0, UL) << TCR_TG0_SHIFT)
#define TCR_TG1_SHIFT       30
#define TCR_TG1_4K          (_AC(2, UL) << TCR_TG1_SHIFT)

#define TCR_ASID16          (_AC(1, UL) << 36)
#define TCR_TBI0            (_AC(1, UL) << 37)
#define TCR_IPS_1TB         (_AC(2, UL) << 32)

#define TCR_TG_FLAGS        TCR_TG0_4K | TCR_TG1_4K
#define TCR_CACHE_FLAGS     TCR_IRGN_WBWA | TCR_ORGN_WBWA

/*
 * We will do 1:1 VA to PA Mapping, so we define the same address size
 * for VA and PA. 1TB size for Virtual and Physical Address Space.
 */
#define VA_BITS     40
#define VA_SIZE     (_AC(1, UL) << VA_BITS)
#define PA_SIZE     (_AC(1, UL) << VA_BITS)

#define TCR_EL1_INIT        \
            TCR_TxSZ(VA_BITS) | TCR_CACHE_FLAGS | TCR_SHARED | \
            TCR_TG_FLAGS | TCR_ASID16 | TCR_TBI0 | TCR_IPS_1TB

/* Translation Table Base Register 0 EL1 */
#define TTBR0_EL1           ARM64_SYS_REG(3, 0, 2, 0, 0)

/* System Control Register EL1 */
#define SCTLR_EL1           ARM64_SYS_REG(3, 0, 1, 0, 0)
#define _SCTLR_M            _BITUL(0)
#define _SCTLR_C            _BITUL(2)
#define _SCTLR_I            _BITUL(12)

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

static uint64_t aarch64_get_counter_frequency(void)
{
    uint64_t frq;

    __asm__ __volatile__("mrs %0, cntfrq_el0" : "=r" (frq):: "memory");

    return frq;
}

static void aarch64_enable_guest_mmu(int vcpufd)
{
    int ret;
    uint64_t data;

    /*
     * Setup Memory Attribute Indirection Register EL1, this register
     * must be set before setup page tables.
     */
    data = MAIR_EL1_INIT;
    ret = aarch64_set_one_register(vcpufd, MAIR_EL1, data);
    if (ret == -1)
         err(1, "KVM: Setup Memory Attribute Indirection Register failed");

    /* Setup Translation Control Register EL1 */
    data = TCR_EL1_INIT;
    ret = aarch64_set_one_register(vcpufd, TCR_EL1, data);
    if (ret == -1)
         err(1, "KVM: Setup Translation Control Register EL1 failed");

    /*
     * Setup Translation Table Base Register 0 EL1. The translation range
     * doesn't exceed the 0 ~ 1^64. So the TTBR0_EL1 is enough.
     */
    data = AARCH64_PAGE_TABLE;
    ret = aarch64_set_one_register(vcpufd, TTBR0_EL1, data);
    if (ret == -1)
         err(1, "KVM: Translation Table Base Register 0 EL1 failed");

    /* Enable MMU and I/D Cache for EL1 */
    data |= (_SCTLR_M | _SCTLR_C | _SCTLR_I);
    ret = aarch64_set_one_register(vcpufd, SCTLR_EL1, data);
    if (ret == -1)
         err(1, "KVM: Setup System Control Register EL1 failed");
}
