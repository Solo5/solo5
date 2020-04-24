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
 * hvt_openbsd_x86_64.c: x86_64 architecture-dependent part of OpenBSD
 * vmm(4) backend implementation.
 */

#define _GNU_SOURCE
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <inttypes.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <machine/cpu.h>
#include <machine/vmmvar.h>
#include <machine/specialreg.h>

#include "hvt.h"
#include "hvt_openbsd.h"
#include "hvt_cpu_x86_64.h"

static struct vcpu_segment_info sreg_to_vsi(const struct x86_sreg *);

static uint64_t get_tsc_freq(void)
{
    int invariant_tsc;
    static const int invar_mib[2] = { CTL_MACHDEP, CPU_INVARIANTTSC };
    static const int freq_mib[2] = { CTL_MACHDEP, CPU_TSCFREQ };
    uint64_t tsc_freq;
    size_t len;

    len = sizeof(invariant_tsc);
    if (sysctl(invar_mib, 2, &invariant_tsc, &len, NULL, 0) == -1)
        err(1, "sysctl invariant_tsc");

    if(invariant_tsc) {
        len = sizeof(tsc_freq);
        if (sysctl(freq_mib, 2, &tsc_freq, &len, NULL, 0) == -1)
            err(1, "sysctl tsc_freq");
    } else {
        errx(1, "Host TSC is not invariant, cannot continue");
    }

    return tsc_freq;
}

static struct vcpu_segment_info sreg_to_vsi(const struct x86_sreg *sreg)
{
    struct vcpu_segment_info vsi = {
        .vsi_sel = sreg->selector * 8,
        .vsi_limit = sreg->limit,
        .vsi_ar = (sreg->type
            | (sreg->s << 4)
            | (sreg->dpl << 5)
            | (sreg->p << 7)
            | (sreg->l << 13)
            | (sreg->db << 14)
            | (sreg->g << 15)
            | (sreg->unusable << X86_SREG_UNUSABLE_BIT)),
        .vsi_base = sreg->base
    };
    return vsi;
}

void hvt_mem_size(size_t *mem_size) {
    hvt_x86_mem_size(mem_size);
}

void hvt_vcpu_init(struct hvt *hvt, hvt_gpa_t gpa_ep)
{
    struct hvt_b *hvb = hvt->b;

    struct vm_resetcpu_params vrp = {
        .vrp_vm_id = hvb->vcp_id,
        .vrp_vcpu_id = hvb->vcpu_id,
        .vrp_init_state = {
            .vrs_gprs[VCPU_REGS_RFLAGS] = X86_RFLAGS_INIT,
            .vrs_gprs[VCPU_REGS_RIP] = gpa_ep,
            .vrs_gprs[VCPU_REGS_RSP] = hvt->mem_size - 8,
            .vrs_gprs[VCPU_REGS_RDI] = X86_BOOT_INFO_BASE,
            .vrs_crs[VCPU_REGS_CR0] = X86_CR0_INIT,
            .vrs_crs[VCPU_REGS_CR3] = X86_CR3_INIT,
            .vrs_crs[VCPU_REGS_CR4] = X86_CR4_INIT,
            .vrs_sregs[VCPU_REGS_CS] = sreg_to_vsi(&hvt_x86_sreg_code),
            .vrs_sregs[VCPU_REGS_DS] = sreg_to_vsi(&hvt_x86_sreg_data),
            .vrs_sregs[VCPU_REGS_ES] = sreg_to_vsi(&hvt_x86_sreg_data),
            .vrs_sregs[VCPU_REGS_FS] = sreg_to_vsi(&hvt_x86_sreg_data),
            .vrs_sregs[VCPU_REGS_GS] = sreg_to_vsi(&hvt_x86_sreg_data),
            .vrs_sregs[VCPU_REGS_SS] = sreg_to_vsi(&hvt_x86_sreg_data),
            .vrs_gdtr = { 0x0, X86_GDTR_LIMIT, 0x0, X86_GDT_BASE},
            .vrs_idtr = { 0x0, 0xFFFF, 0x0, 0x0},
            .vrs_sregs[VCPU_REGS_LDTR] = sreg_to_vsi(&hvt_x86_sreg_unusable),
            .vrs_sregs[VCPU_REGS_TR] = sreg_to_vsi(&hvt_x86_sreg_tr),
            .vrs_msrs[VCPU_REGS_EFER] = X86_EFER_INIT,
            .vrs_msrs[VCPU_REGS_STAR] = 0ULL,
            .vrs_msrs[VCPU_REGS_LSTAR] = 0ULL,
            .vrs_msrs[VCPU_REGS_CSTAR] = 0ULL,
            .vrs_msrs[VCPU_REGS_SFMASK] = 0ULL,
            .vrs_msrs[VCPU_REGS_KGSBASE] = 0ULL,
            .vrs_crs[VCPU_REGS_XCR0] = XCR0_X87
        }
    };

    hvt_x86_setup_gdt(hvt->mem);
    hvt_x86_setup_pagetables(hvt->mem, hvt->mem_size);

    hvt->cpu_cycle_freq = get_tsc_freq();

    if (ioctl(hvb->vmd_fd, VMM_IOC_RESETCPU, &vrp) < 0)
        err(1, "Cannot reset VCPU - exiting.");

    hvt->cpu_boot_info_base = X86_BOOT_INFO_BASE;
}

int hvt_vcpu_loop(struct hvt *hvt)
{
    struct hvt_b *hvb = hvt->b;
    struct vm_run_params *vrp;

    vrp = malloc(sizeof(struct vm_run_params));
    if (vrp == NULL)
        err(1, "malloc vrp");

    vrp->vrp_exit = malloc(sizeof(struct vm_exit));
    if (vrp == NULL)
        err(1, "malloc vrp_exit");

    vrp->vrp_vm_id = hvb->vcp_id;
    vrp->vrp_vcpu_id = hvb->vcpu_id;
    vrp->vrp_continue = 0;

    for (;;) {
        vrp->vrp_irq = 0xFFFF;

        if (ioctl(hvb->vmd_fd, VMM_IOC_RUN, vrp) < 0) {
            err(1, "hvt_vcpu_loop: vm / vcpu run ioctl failed");
        }

        struct vm_exit *vei = vrp->vrp_exit;
        if (vrp->vrp_exit_reason != VM_EXIT_NONE) {
            switch (vrp->vrp_exit_reason) {
                case VMX_EXIT_IO:
                case SVM_VMEXIT_IOIO:
                    if (vei->vei.vei_dir != VEI_DIR_OUT
                            || vei->vei.vei_size != 4)
                        errx(1, "Invalid guest port access: port=0x%x", vei->vei.vei_port);
                    if (vei->vei.vei_port < HVT_HYPERCALL_PIO_BASE ||
                            vei->vei.vei_port >= (HVT_HYPERCALL_PIO_BASE + HVT_HYPERCALL_MAX))
                        errx(1, "Invalid guest port access: port=0x%x", vei->vei.vei_port);

                    int nr = vei->vei.vei_port - HVT_HYPERCALL_PIO_BASE;

                    /* Guest has halted the CPU. */
                    if (nr == HVT_HYPERCALL_HALT) {
                        hvt_gpa_t gpa = vei->vei.vei_data;
                        return hvt_core_hypercall_halt(hvt, gpa);
                    }

                    hvt_hypercall_fn_t fn = hvt_core_hypercalls[nr];
                    if (fn == NULL)
                        errx(1, "Invalid guest hypercall: num=%d", nr);

                    hvt_gpa_t gpa = vei->vei.vei_data;
                    fn(hvt, gpa);
                    break;
#if defined(VMM_IOC_MPROTECT_EPT)
                case VMX_EXIT_EPT_VIOLATION:
                    if(vei->vee.vee_fault_type == VEE_FAULT_PROTECT) { 
                        errx(1, "VMM: host/guest translation fault: rip=0x%llx",
                            vei->vrs.vrs_gprs[VCPU_REGS_RIP]);
                    }
                    break;
#endif
                case VMX_EXIT_TRIPLE_FAULT:
                case SVM_VMEXIT_SHUTDOWN:
                    /* reset VM */
                    errx(1, "Triple Fault");
                default:
                    errx(1, "unhandled exit: unknown exit reason 0x%x",
                        vrp->vrp_exit_reason);
            }

            vrp->vrp_continue = 1;
        }
    }
}
