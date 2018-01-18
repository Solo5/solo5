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
 * ukvm_hv_kvm_x86_64.c: x86_64 architecture-dependent part of KVM backend
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
#include "ukvm_cpu_x86_64.h"

void ukvm_hv_mem_size(size_t *mem_size) {
    ukvm_x86_mem_size(mem_size);
}

static void setup_cpuid(struct ukvm_hvb *hvb)
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

void ukvm_hv_vcpu_init(struct ukvm_hv *hv, ukvm_gpa_t gpa_ep,
        ukvm_gpa_t gpa_kend, char **cmdline)
{
    struct ukvm_hvb *hvb = hv->b;
    int ret;

    ukvm_x86_setup_gdt(hv->mem);
    ukvm_x86_setup_pagetables(hv->mem, hv->mem_size);

    setup_cpuid(hvb);

    struct kvm_sregs sregs = {
        .cr0 = X86_CR0_INIT,
        .cr3 = X86_CR3_INIT,
        .cr4 = X86_CR4_INIT,
        .efer = X86_EFER_INIT,

        .cs = sreg_to_kvm(&ukvm_x86_sreg_code),
        .ss = sreg_to_kvm(&ukvm_x86_sreg_data),
        .ds = sreg_to_kvm(&ukvm_x86_sreg_data),
        .es = sreg_to_kvm(&ukvm_x86_sreg_data),
        .fs = sreg_to_kvm(&ukvm_x86_sreg_data),
        .gs = sreg_to_kvm(&ukvm_x86_sreg_data),

        .gdt = { .base = X86_GDT_BASE, .limit = X86_GDTR_LIMIT },
        .tr = sreg_to_kvm(&ukvm_x86_sreg_tr),
        .ldt = sreg_to_kvm(&ukvm_x86_sreg_unusable)
    };

    ret = ioctl(hvb->vcpufd, KVM_SET_SREGS, &sregs);
    if (ret == -1)
        err(1, "KVM: ioctl (SET_SREGS) failed");
   
    struct ukvm_boot_info *bi =
        (struct ukvm_boot_info *)(hv->mem + X86_BOOT_INFO_BASE);
    bi->mem_size = hv->mem_size;
    bi->kernel_end = gpa_kend;
    bi->cmdline = X86_CMDLINE_BASE;

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
     * KVM-based monitor.
     */
    bi->cpu.tsc_freq = tsc_khz * 1000ULL;

    /*
     * Initialize user registers using (Linux) x86_64 ABI convention.
     */
    struct kvm_regs regs = {
        .rip = gpa_ep,
        .rflags = X86_RFLAGS_INIT,
        .rsp = hv->mem_size - 8, /* x86_64 ABI requires ((rsp + 8) % 16) == 0 */
        .rdi = X86_BOOT_INFO_BASE,                  /* arg1 is ukvm_boot_info */
    };
    ret = ioctl(hvb->vcpufd, KVM_SET_REGS, &regs);
    if (ret == -1)
        err(1, "KVM: ioctl (SET_REGS) failed");

    *cmdline = (char *)(hv->mem + X86_CMDLINE_BASE);
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
        for (ukvm_vmexit_fn_t *fn = ukvm_core_vmexits; *fn && !handled; fn++)
            handled = ((*fn)(hv) == 0);
        if (handled)
            continue;

        struct kvm_run *run = hvb->vcpurun;

        switch (run->exit_reason) {
        case KVM_EXIT_IO: {
            if (run->io.direction != KVM_EXIT_IO_OUT
                    || run->io.size != 4)
                errx(1, "Invalid guest port access: port=0x%x", run->io.port);
            if (run->io.port < UKVM_HYPERCALL_PIO_BASE ||
                    run->io.port >= (UKVM_HYPERCALL_PIO_BASE + UKVM_HYPERCALL_MAX))
                errx(1, "Invalid guest port access: port=0x%x", run->io.port);

            int nr = run->io.port - UKVM_HYPERCALL_PIO_BASE;

            /* Guest has halted the CPU. */
            if (nr == UKVM_HYPERCALL_HALT) {
                ukvm_gpa_t gpa =
                    *(uint32_t *)((uint8_t *)run + run->io.data_offset);
                struct ukvm_halt *p =
                    UKVM_CHECKED_GPA_P(hv, gpa, sizeof (struct ukvm_halt));
                return p->exit_status;
            }

            ukvm_hypercall_fn_t fn = ukvm_core_hypercalls[nr];
            if (fn == NULL)
                errx(1, "Invalid guest hypercall: num=%d", nr);

            ukvm_gpa_t gpa =
                *(uint32_t *)((uint8_t *)run + run->io.data_offset);
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
