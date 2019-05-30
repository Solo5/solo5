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
 * hvt_kvm_x86_64.c: x86_64 architecture-dependent part of KVM backend
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

#include "hvt.h"
#include "hvt_kvm.h"
#include "hvt_cpu_x86_64.h"

void hvt_mem_size(size_t *mem_size) {
    hvt_x86_mem_size(mem_size);
}

static void setup_cpuid(struct hvt_b *hvb)
{
    struct kvm_cpuid2 *kvm_cpuid;
    int max_entries = 100;

    kvm_cpuid = calloc(1, sizeof(*kvm_cpuid) +
                          max_entries * sizeof(*kvm_cpuid->entries));
    assert(kvm_cpuid);
    kvm_cpuid->nent = max_entries;

    if (ioctl(hvb->kvmfd, KVM_GET_SUPPORTED_CPUID, kvm_cpuid) < 0)
        err(1, "KVM: ioctl (GET_SUPPORTED_CPUID) failed");

    if (ioctl(hvb->vcpufd, KVM_SET_CPUID2, kvm_cpuid) < 0)
        err(1, "KVM: ioctl (SET_CPUID2) failed");
}

static struct kvm_segment sreg_to_kvm(const struct x86_sreg *sreg)
{
    /*
     * On x86, (struct kvm_segment) maps 1:1 to our shadow register
     * representation.
     */
    struct kvm_segment kvm = {
        .base = sreg->base,
        .limit = sreg->limit,
        .selector = sreg->selector * 8,
        .type = sreg->type, .present = sreg->p, .dpl = sreg->dpl,
        .db = sreg->db, .s = sreg->s, .l = sreg->l, .g = sreg->g,
        .avl = sreg->avl, .unusable = sreg->unusable
    };
    return kvm;
}

void hvt_vcpu_init(struct hvt *hvt, hvt_gpa_t gpa_ep)
{
    struct hvt_b *hvb = hvt->b;
    int ret;

    hvt_x86_setup_gdt(hvt->mem);
    hvt_x86_setup_pagetables(hvt->mem, hvt->mem_size);

    setup_cpuid(hvb);

    struct kvm_sregs sregs = {
        .cr0 = X86_CR0_INIT,
        .cr3 = X86_CR3_INIT,
        .cr4 = X86_CR4_INIT,
        .efer = X86_EFER_INIT,

        .cs = sreg_to_kvm(&hvt_x86_sreg_code),
        .ss = sreg_to_kvm(&hvt_x86_sreg_data),
        .ds = sreg_to_kvm(&hvt_x86_sreg_data),
        .es = sreg_to_kvm(&hvt_x86_sreg_data),
        .fs = sreg_to_kvm(&hvt_x86_sreg_data),
        .gs = sreg_to_kvm(&hvt_x86_sreg_data),

        .gdt = { .base = X86_GDT_BASE, .limit = X86_GDTR_LIMIT },
        .tr = sreg_to_kvm(&hvt_x86_sreg_tr),
        .ldt = sreg_to_kvm(&hvt_x86_sreg_unusable)
    };

    ret = ioctl(hvb->vcpufd, KVM_SET_SREGS, &sregs);
    if (ret == -1)
        err(1, "KVM: ioctl (SET_SREGS) failed");

    ret = ioctl(hvb->kvmfd, KVM_CHECK_EXTENSION, KVM_CAP_GET_TSC_KHZ);
    if (ret == -1)
        err(1, "KVM: ioctl (KVM_CHECK_EXTENSION) failed");
    if (ret != 1)
        errx(1, "KVM: host does not support KVM_CAP_GET_TSC_KHZ");
    int tsc_khz = ioctl(hvb->vcpufd, KVM_GET_TSC_KHZ);
    if (tsc_khz == -1) {
        if (errno == EIO)
            errx(1, "KVM: host TSC is unstable, cannot continue");
        else
            err(1, "KVM: ioctl (KVM_GET_TSC_KHZ) failed");
    }
    /*
     * KVM gives us the VCPU's TSC frequency in kHz; this is marginally less
     * accurate than what we want, but no less accurate than any other
     * KVM-based virtual machine monitor.
     */
    hvt->cpu_cycle_freq = tsc_khz * 1000ULL;

    /*
     * Initialize user registers using (Linux) x86_64 ABI convention.
     *
     * x86_64 ABI requires stack alignment of ((%rsp + 8) % 16) == 0.
     * %rdi is the only argument to _start, (struct hvt_boot_info *).
     */
    struct kvm_regs regs = {
        .rip = gpa_ep,
        .rflags = X86_RFLAGS_INIT,
        .rsp = hvt->mem_size - 8,
        .rdi = X86_BOOT_INFO_BASE,
    };
    ret = ioctl(hvb->vcpufd, KVM_SET_REGS, &regs);
    if (ret == -1)
        err(1, "KVM: ioctl (SET_REGS) failed");

    hvt->cpu_boot_info_base = X86_BOOT_INFO_BASE;
}

int hvt_vcpu_loop(struct hvt *hvt)
{
    struct hvt_b *hvb = hvt->b;
    int ret;

    while (1) {
        ret = ioctl(hvb->vcpufd, KVM_RUN, NULL);
        if (ret == -1 && errno == EINTR)
            continue;
        if (ret == -1) {
            if (errno == EFAULT) {
                struct kvm_regs regs;
                ret = ioctl(hvb->vcpufd, KVM_GET_REGS, &regs);
                if (ret == -1)
                    err(1, "KVM: ioctl (GET_REGS) failed after guest fault");
                errx(1, "KVM: host/guest translation fault: rip=0x%llx",
                        regs.rip);
            }
            else
                err(1, "KVM: ioctl (RUN) failed");
        }

        int handled = 0;
        for (hvt_vmexit_fn_t *fn = hvt_core_vmexits; *fn && !handled; fn++)
            handled = ((*fn)(hvt) == 0);
        if (handled)
            continue;

        struct kvm_run *run = hvb->vcpurun;

        switch (run->exit_reason) {
        case KVM_EXIT_IO: {
            if (run->io.direction != KVM_EXIT_IO_OUT
                    || run->io.size != 4)
                errx(1, "Invalid guest port access: port=0x%x", run->io.port);
            if (run->io.port < HVT_HYPERCALL_PIO_BASE ||
                    run->io.port >= (HVT_HYPERCALL_PIO_BASE + HVT_HYPERCALL_MAX))
                errx(1, "Invalid guest port access: port=0x%x", run->io.port);

            int nr = run->io.port - HVT_HYPERCALL_PIO_BASE;

            /* Guest has halted the CPU. */
            if (nr == HVT_HYPERCALL_HALT) {
                hvt_gpa_t gpa =
                    *(uint32_t *)((uint8_t *)run + run->io.data_offset);
                return hvt_core_hypercall_halt(hvt, gpa);
            }

            hvt_hypercall_fn_t fn = hvt_core_hypercalls[nr];
            if (fn == NULL)
                errx(1, "Invalid guest hypercall: num=%d", nr);

            hvt_gpa_t gpa =
                *(uint32_t *)((uint8_t *)run + run->io.data_offset);
            fn(hvt, gpa);
            break;
        }

        case KVM_EXIT_FAIL_ENTRY:
            errx(1, "KVM: entry failure: hw_entry_failure_reason=0x%llx",
                 run->fail_entry.hardware_entry_failure_reason);

        case KVM_EXIT_INTERNAL_ERROR:
            errx(1, "KVM: internal error exit: suberror=0x%x",
                 run->internal.suberror);

        default: {
            struct kvm_regs regs;
            ret = ioctl(hvb->vcpufd, KVM_GET_REGS, &regs);
            if (ret == -1)
                err(1, "KVM: ioctl (GET_REGS) failed after unhandled exit");
            errx(1, "KVM: unhandled exit: exit_reason=0x%x, rip=0x%llx",
                    run->exit_reason, regs.rip);
        }
        } /* switch(run->exit_reason) */
    }
}
