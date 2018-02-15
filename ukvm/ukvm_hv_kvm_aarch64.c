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

/* Saved Program Status Register EL1 */
#define SPSR_EL1            ARM64_CORE_REG(regs.pstate)

/*
 * Default PSTATE flags:
 * Mask Debug, Abort, IRQ and FIQ. Switch to EL1h mode
 */
#define AARCH64_PSTATE_INIT \
                            (PSR_D_BIT | PSR_A_BIT | PSR_I_BIT | \
                             PSR_F_BIT | PSR_MODE_EL1h)

/* PC Register */
#define REG_PC              ARM64_CORE_REG(regs.pc)

/* Stack Pointer EL1 */
#define SP_EL1              ARM64_CORE_REG(sp_el1)

/* Generic Purpose register x0 */
#define REG_X0              ARM64_CORE_REG(regs.regs[0])

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

/*
 * Initialize registers: instruction pointer for our code, addends,
 * and PSTATE flags required by ARM64 architecture.
 * Arguments to the kernel main are passed using the ARM64 calling
 * convention: x0 ~ x7
 */
static void aarch64_setup_core_registers(struct ukvm_hv *hv,
                             ukvm_gpa_t gpa_ep, ukvm_gpa_t gpa_kend)
{
    int ret;
    struct ukvm_hvb *hvb = hv->b;
    struct ukvm_boot_info *bi;

    /* Set default PSTATE flags to SPSR_EL1 */
    ret = aarch64_set_one_register(hvb->vcpufd, SPSR_EL1,
                                   AARCH64_PSTATE_INIT);
    if (ret == -1)
         err(1, "Initialize spsr[EL1] failed!\n");

    /*
     * Set Stack Poniter for Guest. ARM64 require stack be 16-bytes
     * alignment by default.
     */
    ret = aarch64_set_one_register(hvb->vcpufd, SP_EL1,
                                   hv->mem_size - 16);
    if (ret == -1)
         err(1, "Initialize sp[EL1] failed!\n");

    bi = (struct ukvm_boot_info *)(hv->mem + AARCH64_BOOT_INFO);
    bi->mem_size = hv->mem_size;
    bi->kernel_end = gpa_kend;
    bi->cmdline = AARCH64_CMDLINE_BASE;

    /*
     * KVM on aarch64 doesn't support KVM_CAP_GET_TSC_KHZ. But we can use
     * the cntvct_el0 as RDTSC of x86. So we can read counter frequency
     * from cntfrq_el0 directly.
     */
    bi->cpu.tsc_freq = aarch64_get_counter_frequency();

    /* Passing ukvm_boot_info through x0 */
    ret = aarch64_set_one_register(hvb->vcpufd, REG_X0, AARCH64_BOOT_INFO);
    if (ret == -1)
         err(1, "Set boot info to x0 failed!\n");

    /* Set guest reset PC entry here */
    ret = aarch64_set_one_register(hvb->vcpufd, REG_PC, gpa_ep);
    if (ret == -1)
         err(1, "Set guest reset entry to PC failed!\n");
}

void ukvm_hv_vcpu_init(struct ukvm_hv *hv, ukvm_gpa_t gpa_ep,
        ukvm_gpa_t gpa_kend, char **cmdline)
{
    struct ukvm_hvb *hvb = hv->b;
    uint64_t phys_space_sz;

    /*
     * Setup aarch64 phys to virt mapping. Currently we only map 4GB for
     * RAM space and 1GB for MMIO space. Although the guest can use up
     * to 1TB address space which we configured in TCR_EL1.
     */
    phys_space_sz = AARCH64_MMIO_BASE + AARCH64_MMIO_SZ;
    aarch64_setup_memory_mapping(hv->mem, hv->mem_size, phys_space_sz);

    /* Select preferred target for guest */
    aarch64_setup_preferred_target(hvb->vmfd, hvb->vcpufd);
    /* Enable float for guest */
    aarch64_enable_guest_float(hvb->vcpufd);
    /* Enable MMU for guest*/
    aarch64_enable_guest_mmu(hvb->vcpufd);

    /* Initialize core registers for guest */
    aarch64_setup_core_registers(hv, gpa_ep, gpa_kend);

    *cmdline = (char *)(hv->mem + AARCH64_CMDLINE_BASE);
}

static inline uint32_t mmio_read32(void *data)
{
    return *(uint32_t *)data;
}

int ukvm_hv_vcpu_loop(struct ukvm_hv *hv)
{
    struct ukvm_hvb *hvb = hv->b;
    int ret;

    while (1) {
        ret = ioctl(hvb->vcpufd, KVM_RUN, NULL);
        if (ret == -1 && errno == EINTR)
            continue;
        if (ret == -1) {
            if (errno == EFAULT) {
                uint64_t pc;
                ret = aarch64_get_one_register(hvb->vcpufd, REG_PC, &pc);
                if (ret == -1)
                    err(1, "KVM: Dump PC failed after guest fault");
                errx(1, "KVM: host/guest translation fault: pc=0x%lx", pc);
            }
            else
                err(1, "KVM: ioctl (RUN) failed");
        }

        int handled = 0;
        for (ukvm_vmexit_fn_t *fn = ukvm_core_vmexits; *fn && !handled; fn++)
            handled = ((*fn)(hv) == 0);
        if (handled)
            continue;

        struct kvm_run *run = hvb->vcpurun;

        switch (run->exit_reason) {
        case KVM_EXIT_MMIO: {
            if (!run->mmio.is_write || run->mmio.len != 4)
                errx(1, "Invalid guest mmio access: mmio=0x%llx len=%d", run->mmio.phys_addr, run->mmio.len);

            if (run->mmio.phys_addr < UKVM_HYPERCALL_MMIO_BASE ||
                run->mmio.phys_addr >= UKVM_HYPERCALL_ADDRESS(UKVM_HYPERCALL_MAX))
                errx(1, "Invalid guest mmio access: mmio=0x%llx", run->mmio.phys_addr);

            int nr = UKVM_HYPERCALL_NR(run->mmio.phys_addr);

            /* Guest has halted the CPU. */
            if (nr == UKVM_HYPERCALL_HALT) {
                ukvm_gpa_t gpa = mmio_read32(run->mmio.data);
                struct ukvm_halt *p =
                    UKVM_CHECKED_GPA_P(hv, gpa, sizeof (struct ukvm_halt));
                return p->exit_status;
            }

            ukvm_hypercall_fn_t fn = ukvm_core_hypercalls[nr];
            if (fn == NULL)
                errx(1, "Invalid guest hypercall: num=%d", nr);

            ukvm_gpa_t gpa = mmio_read32(run->mmio.data);
            fn(hv, gpa);
            break;
        }

        case KVM_EXIT_FAIL_ENTRY:
            errx(1, "KVM: entry failure: hw_entry_failure_reason=0x%llx",
                 run->fail_entry.hardware_entry_failure_reason);

        case KVM_EXIT_INTERNAL_ERROR:
            errx(1, "KVM: internal error exit: suberror=0x%x",
                 run->internal.suberror);

        default: {
            uint64_t pc;
            ret = aarch64_get_one_register(hvb->vcpufd, REG_PC, &pc);
            if (ret == -1)
                err(1, "KVM: Dump PC failed after unhandled exit");
            errx(1, "KVM: unhandled exit: exit_reason=0x%x, pc=0x%lx",
                    run->exit_reason, pc);
        }
        } /* switch(run->exit_reason) */
    }
}

void ukvm_hv_mem_size(size_t *mem_size) {
    aarch64_mem_size(mem_size);
}
