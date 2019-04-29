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
 * hvt_dumpcore_freebsd_x86_64.c: Glue between the dumpcore module
 * and FreeBSD vmm.
 */

#include <sys/procfs.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/cpuset.h>
#include <machine/vmm.h>
#include <machine/vmm_dev.h>

#include "hvt.h"
#include "hvt_freebsd.h"
#include "hvt_cpu_x86_64.h"

size_t hvt_dumpcore_prstatus_size(void)
{
    return sizeof (prstatus_t);
}

static uint64_t vmm_get_reg(int vmfd, int reg)
{
    struct vm_register vmreg = {
        .cpuid = 0, .regnum = reg
    };

    if (ioctl(vmfd, VM_GET_REGISTER, &vmreg) == -1)
        err(1, "VM_GET_REGISTER (%d)", reg);
    return vmreg.regval;
}

int hvt_dumpcore_write_prstatus(int fd, struct hvt *hvt, void *cookie)
{
    struct hvt_b *hvb = hvt->b;

    prstatus_t prstatus = { 0 };
    /*
     * Force version to (1), in case there's every a new version with more
     * required fields. We only fill out a minimum of the prstatus_t structure.
     *
     * XXX: The resulting core file is not understood by the gdb in the FreeBSD
     * base system, however gdb from ports works fine.
     */
    prstatus.pr_version = 1;
    prstatus.pr_statussz = sizeof (prstatus_t);
    prstatus.pr_gregsetsz = sizeof (gregset_t);

    /*
     * prstatus_t.pr_reg is actually a (struct reg) in disguise.
     */
    struct reg *uregs = (struct reg *)&prstatus.pr_reg;

    uregs->r_r8 = vmm_get_reg(hvb->vmfd, VM_REG_GUEST_R8);
    uregs->r_r9 = vmm_get_reg(hvb->vmfd, VM_REG_GUEST_R9);
    uregs->r_r10 = vmm_get_reg(hvb->vmfd, VM_REG_GUEST_R10);
    uregs->r_r11 = vmm_get_reg(hvb->vmfd, VM_REG_GUEST_R11);
    uregs->r_r12 = vmm_get_reg(hvb->vmfd, VM_REG_GUEST_R12);
    uregs->r_r13 = vmm_get_reg(hvb->vmfd, VM_REG_GUEST_R13);
    uregs->r_r14 = vmm_get_reg(hvb->vmfd, VM_REG_GUEST_R14);
    uregs->r_r15 = vmm_get_reg(hvb->vmfd, VM_REG_GUEST_R15);
    uregs->r_rbp = vmm_get_reg(hvb->vmfd, VM_REG_GUEST_RBP);
    uregs->r_rsp = vmm_get_reg(hvb->vmfd, VM_REG_GUEST_RSP);
    uregs->r_rdi = vmm_get_reg(hvb->vmfd, VM_REG_GUEST_RDI);
    uregs->r_rsi = vmm_get_reg(hvb->vmfd, VM_REG_GUEST_RSI);
    uregs->r_rdx = vmm_get_reg(hvb->vmfd, VM_REG_GUEST_RDX);
    uregs->r_rcx = vmm_get_reg(hvb->vmfd, VM_REG_GUEST_RCX);
    uregs->r_rbx = vmm_get_reg(hvb->vmfd, VM_REG_GUEST_RBX);
    uregs->r_rax = vmm_get_reg(hvb->vmfd, VM_REG_GUEST_RAX);
    uregs->r_rip = vmm_get_reg(hvb->vmfd, VM_REG_GUEST_RIP);
    uregs->r_rflags = vmm_get_reg(hvb->vmfd, VM_REG_GUEST_RFLAGS);

    uregs->r_cs = vmm_get_reg(hvb->vmfd, VM_REG_GUEST_CS);
    uregs->r_ss = vmm_get_reg(hvb->vmfd, VM_REG_GUEST_SS);
    uregs->r_ds = vmm_get_reg(hvb->vmfd, VM_REG_GUEST_DS);
    uregs->r_es = vmm_get_reg(hvb->vmfd, VM_REG_GUEST_ES);
    uregs->r_fs = vmm_get_reg(hvb->vmfd, VM_REG_GUEST_FS);
    uregs->r_gs = vmm_get_reg(hvb->vmfd, VM_REG_GUEST_GS);

    /*
     * If the guest provided us with register data from a trap handler, use
     * that.
     */
    if (cookie) {
        assert(sizeof(struct x86_trap_regs) < HVT_HALT_COOKIE_MAX);
        struct x86_trap_regs *regs = (struct x86_trap_regs *)cookie;
        uregs->r_rip = regs->rip;
        uregs->r_cs = regs->cs;
        uregs->r_rflags = regs->rflags;
        uregs->r_rsp = regs->rsp;
        uregs->r_ss = regs->ss;
    }

    ssize_t nbytes;
    nbytes = write(fd, &prstatus, sizeof prstatus);
    if (nbytes < 0) {
        warn("dumpcore: Error writing prstatus");
        return -1;
    }
    else if (nbytes != sizeof prstatus) {
        warnx("dumpcore: Short write() writing prstatus: %zd", nbytes);
        return -1;
    }

    return 0;
}

int hvt_dumpcore_supported()
{
    return 0;
}
