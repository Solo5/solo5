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
 * ukvm_hv_freebsd_x86_64.c: x86_64 architecture-dependent part of FreeBSD
 * vmm(4) backend implementation.
 */

#define _GNU_SOURCE
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <machine/vmm.h>
#include <sys/param.h>
#include <sys/cpuset.h>
#include <machine/vmm_dev.h>

#include "ukvm.h"
#include "ukvm_hv_freebsd.h"
#include "ukvm_cpu_x86_64.h"

void ukvm_hv_mem_size(size_t *mem_size) {
    ukvm_x86_mem_size(mem_size);
}

static void vmm_set_desc(int vmfd, int reg, uint64_t base, uint32_t limit,
        uint32_t access)
{
    int ret;
    struct vm_seg_desc vmsegdesc = {
        .cpuid = 0,
        .regnum = reg,
        .desc.base = base,
        .desc.limit = limit,
        .desc.access = access
    };

    ret = ioctl(vmfd, VM_SET_SEGMENT_DESCRIPTOR, &vmsegdesc);
    if (ret == -1)
        err(1, "VM_SET_SEGMENT_DESCRIPTOR (%d)", reg);
}

static void vmm_set_reg(int vmfd, int reg, uint64_t val)
{
    struct vm_register vmreg = {
        .cpuid = 0, .regnum = reg, .regval = val
    };

    if (ioctl(vmfd, VM_SET_REGISTER, &vmreg) == -1)
        err(1, "VM_SET_REGISTER (%d)", reg);
}

static void vmm_set_sreg(int vmfd, int reg, const struct x86_sreg *sreg)
{
    uint64_t base = sreg->base;
    uint32_t limit = sreg->limit;
    uint32_t access = (sreg->type
            | (sreg->s << 4)
            | (sreg->dpl << 5)
            | (sreg->p << 7)
            | (sreg->l << 13)
            | (sreg->db << 14)
            | (sreg->g << 15)
            | (sreg->unusable << X86_SREG_UNUSABLE_BIT));

    vmm_set_desc(vmfd, reg, base, limit, access);
    vmm_set_reg(vmfd, reg, sreg->selector * 8);
}

void ukvm_hv_vcpu_init(struct ukvm_hv *hv, ukvm_gpa_t gpa_ep,
        ukvm_gpa_t gpa_kend, char **cmdline)
{
    struct ukvm_hvb *hvb = hv->b;

    ukvm_x86_setup_gdt(hv->mem);
    ukvm_x86_setup_pagetables(hv->mem, hv->mem_size);

    vmm_set_reg(hvb->vmfd, VM_REG_GUEST_CR0, X86_CR0_INIT);
    vmm_set_reg(hvb->vmfd, VM_REG_GUEST_CR3, X86_CR3_INIT);
    vmm_set_reg(hvb->vmfd, VM_REG_GUEST_CR4, X86_CR4_INIT);
    vmm_set_reg(hvb->vmfd, VM_REG_GUEST_EFER, X86_EFER_INIT);

    vmm_set_sreg(hvb->vmfd, VM_REG_GUEST_CS, &ukvm_x86_sreg_code);
    vmm_set_sreg(hvb->vmfd, VM_REG_GUEST_SS, &ukvm_x86_sreg_data);
    vmm_set_sreg(hvb->vmfd, VM_REG_GUEST_DS, &ukvm_x86_sreg_data);
    vmm_set_sreg(hvb->vmfd, VM_REG_GUEST_ES, &ukvm_x86_sreg_data);
    vmm_set_sreg(hvb->vmfd, VM_REG_GUEST_FS, &ukvm_x86_sreg_data);
    vmm_set_sreg(hvb->vmfd, VM_REG_GUEST_GS, &ukvm_x86_sreg_data);

    vmm_set_desc(hvb->vmfd, VM_REG_GUEST_GDTR, X86_GDT_BASE, X86_GDTR_LIMIT, 0);
    vmm_set_sreg(hvb->vmfd, VM_REG_GUEST_TR, &ukvm_x86_sreg_tr);
    vmm_set_sreg(hvb->vmfd, VM_REG_GUEST_LDTR, &ukvm_x86_sreg_unusable);

    struct ukvm_boot_info *bi =
        (struct ukvm_boot_info *)(hv->mem + X86_BOOT_INFO_BASE);
    bi->mem_size = hv->mem_size;
    bi->kernel_end = gpa_kend;
    bi->cmdline = X86_CMDLINE_BASE;

    size_t outsz = sizeof bi->cpu.tsc_freq;
    int ret = sysctlbyname("machdep.tsc_freq", &bi->cpu.tsc_freq, &outsz, NULL,
            0);
    if (ret == -1)
        err(1, "sysctl(machdep.tsc_freq)");
    int invariant_tsc = 0;
    outsz = sizeof invariant_tsc;
    ret = sysctlbyname("kern.timecounter.invariant_tsc", &invariant_tsc, &outsz,
            NULL, 0);
    if (ret == -1)
        err(1, "sysctl(kern.timecounter.invariant_tsc");
    if (invariant_tsc != 1)
        errx(1, "Host TSC is not invariant, cannot continue");

    vmm_set_reg(hvb->vmfd, VM_REG_GUEST_RIP, gpa_ep);
    vmm_set_reg(hvb->vmfd, VM_REG_GUEST_RFLAGS, X86_RFLAGS_INIT);
    vmm_set_reg(hvb->vmfd, VM_REG_GUEST_RSP, hv->mem_size - 8);
    vmm_set_reg(hvb->vmfd, VM_REG_GUEST_RDI, X86_BOOT_INFO_BASE);

    struct vm_activate_cpu ac = {
        .vcpuid = 0
    };
    ret = ioctl(hvb->vmfd, VM_ACTIVATE_CPU, &ac);
    if (ret == -1)
        err(1, "VM_ACTIVATE_CPU");

    *cmdline = (char *)(hv->mem + X86_CMDLINE_BASE);
}

static void dump_vmx(struct vm_exit *vme)
{
    warnx("unhandled VMX exit:");
    warnx("\trip\t\t0x%016lx", vme->rip);
    warnx("\tinst_length\t%d", vme->inst_length);
    warnx("\tstatus\t\t%d", vme->u.vmx.status);
    warnx("\texit_reason\t%u", vme->u.vmx.exit_reason);
    warnx("\tqualification\t0x%016lx", vme->u.vmx.exit_qualification);
    warnx("\tinst_type\t%d", vme->u.vmx.inst_type);
    warnx("\tinst_error\t%d", vme->u.vmx.inst_error);
}

int ukvm_hv_vcpu_loop(struct ukvm_hv *hv)
{
    struct ukvm_hvb *hvb = hv->b;
    int ret;

    while (1) {
        ret = ioctl(hv->b->vmfd, VM_RUN, &hvb->vmrun);
        if (ret == -1 && errno == EINTR)
            continue;
        if (ret == -1) {
            err(1, "VM_RUN");
        }

        int handled = 0;
        for (ukvm_vmexit_fn_t *fn = ukvm_core_vmexits; *fn && !handled; fn++)
            handled = ((*fn)(hv) == 0);
        if (handled)
            continue;

        struct vm_exit *vme = &hvb->vmrun.vm_exit;

        switch (vme->exitcode) {
        case VM_EXITCODE_INOUT: {
            if (vme->u.inout.in || vme->u.inout.bytes != 4)
                errx(1, "Invalid guest port access: port=0x%x",
                        vme->u.inout.port);
            if (vme->u.inout.port < UKVM_HYPERCALL_PIO_BASE ||
                    vme->u.inout.port >= (UKVM_HYPERCALL_PIO_BASE + UKVM_HYPERCALL_MAX))
                errx(1, "Invalid guest port access: port=0x%x",
                        vme->u.inout.port);

            int nr = vme->u.inout.port - UKVM_HYPERCALL_PIO_BASE;

            /* Guest has halted the CPU. */
            if (nr == UKVM_HYPERCALL_HALT) {
                ukvm_gpa_t gpa = vme->u.inout.eax;
                struct ukvm_halt *p =
                    UKVM_CHECKED_GPA_P(hv, gpa, sizeof (struct ukvm_halt));
                return p->exit_status;
            }

            ukvm_hypercall_fn_t fn = ukvm_core_hypercalls[nr];
            if (fn == NULL)
                errx(1, "Invalid guest hypercall: num=%d", nr);

            ukvm_gpa_t gpa = vme->u.inout.eax;
            fn(hv, gpa);
            break;
        }

        case VM_EXITCODE_BOGUS: {
            /*
             * XXX: I've no idea what this actually means, so just do what
             * bhyve does.
             */
             assert(vme->inst_length == 0);
             break;
        }

        case VM_EXITCODE_VMX: {
            dump_vmx(vme);
            exit(1);
        }

        default: {
            errx(1, "unhandled exit: exitcode=%d, rip=0x%" PRIx64,
                    vme->exitcode, vme->rip);
        }
        } /* switch(vme->exitcode) */
    }
}
